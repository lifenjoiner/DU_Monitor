
/*------------------------------------------------------------
  DU_Monitor.c
  lifenjoiner@163.com
  
  Ҫ˵���ǣ�CreateWindow �� CreateDialog �������в�ͬ��...
------------------------------------------------------------*/

// xp
#include "version.h"

// UI
#include "resource.h"
#define WM_TRAYNOTIFY WM_USER+1

// Data
#include <windows.h>
#include <stdlib.h>   // C
#include <stdio.h>
#include <iphlpapi.h> // GetIfTable
#include <process.h>  // MultiThread
#include <shellapi.h> // SysTrayIcon
#include <commctrl.h> // trackbar
#include <shlwapi.h>  // .ini extension
#include <time.h>     // ׼ȷ��ʱ
//
#include "DU_Monitor.h"

//
UINT  Update_ms = 1000;
MIB_IFTABLE *pIfTable[2];
char DownSpeed[16]; // 1000.00 KB/s
char UpSpeed[16];

// Global variable

HINSTANCE   hINSTANCE;
HWND        hWND_MAIN;

NOTIFYICONDATA nid;

HWND        hWND_MORE;
DWORD       MNC_idx;

int    PosL, PosT;
BOOL   TopMost;
int    TCheckBox;
long   TrackBarPos;
char   G_NIC[MAXLEN_IFDESCR];
char   B_NIC[MAXLEN_IFDESCR];
char   CfgFileName[FILENAME_MAX];

clock_t Clock_Tick[2];
float DurSeconds;

//////////////////////////////////////////////////////////////
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PSTR szCmdLine, int iCmdShow)
{
    hINSTANCE = hInstance;
    LPCTSTR lpTemplate = MAKEINTRESOURCE (IDD_DLG_MAIN);     // Dlg name in rc file
    
    MSG          msg;
    WNDCLASS     wndclass;

    hWND_MAIN = CreateDialog (hInstance,
                         lpTemplate,                         // from rc file
                         NULL,
                         (DLGPROC) MainDlgProc);             //
    //
    GetCfgFileName();
    LoadCfg();
    ApplyCfgToMain();
    //
    while (GetMessage (&msg, NULL, 0, 0))
    {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
    
    return msg.wParam;
}

/* ���Ի�����Ϣѭ�� */
LRESULT CALLBACK MainDlgProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UINT WM_TASKBARCREATED;
    switch (message)
    {
    case WM_INITDIALOG:
        //
        WM_TASKBARCREATED = RegisterWindowMessage("TaskbarCreated");
        AddNotifyIcon(hwnd);
        // ȡ��һ������
        pIfTable[1] = GetCurpIfTable();
        Clock_Tick[1] = clock();
        // ���߳�Timer���ֲ�SetTimer�Ĳ���
        if (_beginthread(UpdateDataTimer, 0, NULL) == 0 | _beginthread(MiscTimer, 0, NULL) == 0)
        {
            MessageBox (hWND_MAIN, "Timer failed!", "Error", MB_OK);
        }
        break;
    case WM_NCHITTEST:
    // �����ж�
        if (DefWindowProc (hwnd, message, wParam, lParam) == HTCLIENT)
        {
            SetWindowLong (hwnd, DWL_MSGRESULT, (LONG) HTCAPTION);
            SaveCfg();
            return HTCAPTION;
        }
        break;
    case WM_TRAYNOTIFY: // user defined
        TrayNotifyProc (hwnd, message, wParam, lParam);
        break;
    case WM_CONTEXTMENU:
    case WM_NCRBUTTONUP:
    // �������ϵ�ת�ƣ���Ϣ��λҪ��
        DisplayContextMenu (hwnd);
        break;
    
    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDM_MORE:
            if (IsWindow(hWND_MORE))
            {
                DestroyWindow (hWND_MORE);
            } else {
                hWND_MORE = CreateDialog (hINSTANCE, MAKEINTRESOURCE(IDD_DLG_MORE), hwnd, (DLGPROC) MoreDlgProc);
            }
            break;
        case IDM_TOP:
            //MessageBox(NULL, "TopMost", "Show",MB_OK);
            TopMost ^= TRUE;
            SetMainTopMost();
            SaveCfg();
            break;
        case IDM_ABOUT:
            DialogBox (hINSTANCE, MAKEINTRESOURCE(IDD_DLG_ABOUT), hwnd, (DLGPROC) AboutDlgProc);
            break;
        case IDM_EXIT:
            ExitApp (hwnd);
            break;
        }
        break;

    // ��������Ϣѭ������Ҫ
    case WM_CLOSE:
    // �ر�UI
        ExitApp (hwnd);
        break;
    case WM_DESTROY:
    // �˳�application
        PostQuitMessage (0);
        break;
    default:
        if (message == WM_TASKBARCREATED)
        {
            // explorer.exe ��[��]����
            AddNotifyIcon(hwnd);
        }
        // ������ return DefWindowProc(hwnd, message, wParam, lParam);
        break;
    }
    return 0;
}

/* More �Ի�����Ϣѭ�� */
LRESULT CALLBACK MoreDlgProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        // ��ʼ
        ShowCfgStatus(hwnd);
        break;
    case WM_COMMAND:
    // �ؼ���Ϣ
    case WM_NOTIFY:
        MoreCtrlProc (hwnd, message, wParam, lParam);
        break;
    case WM_CLOSE:
        DestroyWindow (hwnd);
        break;
    case WM_DESTROY:
        CloseHandle (hwnd);
        SaveCfg();
        break;
    }
    return 0;
}

/* More �Ի����¼���Ϣѭ�� */
LRESULT CALLBACK MoreCtrlProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int idx;
    switch(LOWORD(wParam))
    {
    case IDC_CBO_NETCARD:
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            if ((idx = SendDlgItemMessage(hwnd,IDC_CBO_NETCARD,CB_GETCURSEL,0,0)) != CB_ERR)
            {
                strcpy(G_NIC, pIfTable[1]->table[idx].bDescr);
            }
        }
        break;
    case IDC_CHK_TOP:
        if (HIWORD(wParam) != BN_CLICKED)   //��ʼ����ʱ��+
            ShowCfgStatus(hwnd);
        else
        {
            TCheckBox = SendMessage(GetDlgItem(hwnd, IDC_CHK_TOP), BM_GETCHECK, 0,0);
            SetMainTransparent();
        }
        break;
    case IDC_TRB_TRANSP:
        if (((LPNMHDR)lParam)->code == NM_RELEASEDCAPTURE) // break;
        {
            TrackBarPos = SendDlgItemMessage(hwnd, IDC_TRB_TRANSP, TBM_GETPOS, 0,0); // ȡ��ֵ
            SetMainTransparent();
        }
        break;
    }
    return 0;
}

/* About ������Ϣѭ�� */
LRESULT CALLBACK AboutDlgProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    case WM_CLOSE:
        EndDialog (hwnd, IDOK);
        break;
    }
    return 0;
}

/* ���Ի����Ҽ��˵���Ϣѭ�� */
VOID APIENTRY DisplayContextMenu (HWND hwnd)
{
    HMENU hmenu;            // top-level menu
    HMENU hmenuTrackPopup;  // shortcut menu
    UINT  state;
    POINT pt;

    // Load the menu resource.
    if ((hmenu = LoadMenu(hINSTANCE, MAKEINTRESOURCE(IDR_POPMENU1))) == NULL)
        return;

    // TrackPopupMenu cannot display the menu bar
    hmenuTrackPopup = GetSubMenu(hmenu, 0);
    
    // checkbox
    if (IsWindow(hWND_MORE))
    {
        state = MF_CHECKED;
    } else {
        state = MF_UNCHECKED;
    }
    CheckMenuItem (hmenu, IDM_MORE, state);
    if (TopMost == TRUE)
    {
        state = MF_CHECKED;
    } else {
        state = MF_UNCHECKED;
    }
    CheckMenuItem (hmenu, IDM_TOP, state);
    
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // 135788:Menus for Notification Icons Do Not Work Correctly, from MSDN
    TrackPopupMenu(hmenuTrackPopup, TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);

    DestroyMenu(hmenu);
}

BOOL AddNotifyIcon(HWND hwnd)
{
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 100;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.hIcon = LoadIcon(hINSTANCE, MAKEINTRESOURCE(TrayIcon));
    nid.uCallbackMessage = WM_TRAYNOTIFY;  // �Զ�����Ϣ��δ����resource.h����ǰ
    // tip
    strcpy(nid.szTip, "I See You ~");
    //
    return Shell_NotifyIcon(NIM_ADD, &nid);
}

BOOL DeleteNotifyIcon(HWND hwnd)
{
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ExitApp(HWND hwnd)
{
    if (CfgFileName != NULL) SaveCfg();
    DeleteNotifyIcon(hwnd);
    DestroyWindow (hwnd);
}

/* ���ݲ˵���Ϣѭ�� */
LRESULT CALLBACK TrayNotifyProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (lParam)
    {
    case WM_LBUTTONDOWN:
        if (IsWindowVisible(hwnd)){
            ShowWindow(hwnd, SW_HIDE);
        }else{
            ShowWindow(hwnd, SW_SHOW);
            ShowMonitorInfoBallon(hwnd);
        }
        break;
    case WM_RBUTTONDOWN:
        DisplayContextMenu (hwnd);
        break;
    }
    return 0;
}

BOOL ShowMonitorInfoBallon(HWND hwnd)
{
    ShowNotifyBallon(hwnd, "��ǰ�������", G_NIC);
}

BOOL ShowNotifyBallon(HWND hwnd, char title[], char info[])
{
    nid.hWnd = hwnd;
    // ballon
    nid.uFlags |= NIF_INFO;
    nid.dwInfoFlags = NIIF_NONE;
    strcpy(nid.szInfoTitle, title);
    strcpy(nid.szInfo, info);
    // uTimeout could be deprecated
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void UpdateNCDListBox(HWND hComboBox)
{
    DWORD i;
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < pIfTable[1]->dwNumEntries; i++)
    {
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)pIfTable[1]->table[i].bDescr);
    }
    // combox, ѡ�е�ǰ����
    SendMessage(hComboBox,CB_SETCURSEL,MNC_idx,0);
}

void UpdateTransparent(float Transp)
{
    TrackBarPos = Transp;
    TCheckBox = SendMessage(GetDlgItem(hWND_MORE, IDC_CHK_TOP), BM_GETCHECK, 0,0);    
}

void ShowCfgStatus(HWND hwnd)
{
    //MessageBox(NULL, "GetCheckBoxStatus", "Show",MB_OK);
    SendDlgItemMessage(hwnd, IDC_CHK_TOP, BM_SETCHECK, (WPARAM)TCheckBox, 0);
    SendDlgItemMessage(hwnd, IDC_TRB_TRANSP, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)TrackBarPos);
}

void GetCfgFileName(void)
{
    if (GetModuleFileName(NULL, CfgFileName, MAX_PATH) != 0)
    PathRenameExtension(CfgFileName, ".ini");
    //MessageBox(NULL, CfgFileName, "Show",MB_OK);
}

void LoadCfg(void)
{
    PosL = GetPrivateProfileInt("Config", "X", -1, CfgFileName);
    PosT = GetPrivateProfileInt("Config", "Y", -1, CfgFileName);
    TopMost = GetPrivateProfileInt("Config", "TopMost", TRUE, CfgFileName);
    TCheckBox = GetPrivateProfileInt("Config", "Transparent", BST_CHECKED, CfgFileName);
    TrackBarPos = GetPrivateProfileInt("Config", "Transparency", 50, CfgFileName);
    GetPrivateProfileString("Config", "NIC", NULL, G_NIC, MAX_PATH, CfgFileName);
}

void ApplyCfgToMain(void)
{
    SetMainPos();
    SetMainTransparent();
    SetMainTopMost();
}

void SetMainPos(void)
{
    if (PosL >= 0 && PosT >= 0)
    {
        SetWindowPos(hWND_MAIN, NULL, PosL, PosT, 0, 0, SWP_NOZORDER | SWP_NOSIZE); // ignore Z order
    }
}

void SetMainTransparent(void)
{
    long lWinNewStyle = GetWindowLong(hWND_MAIN, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT;
    long LWA_Flag = LWA_COLORKEY | LWA_ALPHA;
    if (TCheckBox != BST_CHECKED){
        lWinNewStyle ^= WS_EX_TRANSPARENT;
        LWA_Flag ^= LWA_COLORKEY;
    }
    SetWindowLong(hWND_MAIN, GWL_EXSTYLE, lWinNewStyle);
    SetLayeredWindowAttributes(hWND_MAIN, GetSysColor(COLOR_3DFACE), (1-(float)TrackBarPos/100)*255, LWA_Flag);
}

void SetMainTopMost(void)
{
    HWND hwndTopMost;
    if (TopMost == TRUE)
    {
        hwndTopMost = HWND_TOPMOST;
    } else hwndTopMost = HWND_NOTOPMOST;
    SetWindowPos(hWND_MAIN, hwndTopMost, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

void SaveCfg(void)
{
    char buffer[MAX_PATH];
    RECT Rect;
    if (GetWindowRect(hWND_MAIN, &Rect))
    {
        sprintf(buffer, "%d", Rect.left);
        WritePrivateProfileString("Config", "X", buffer, CfgFileName);
        sprintf(buffer, "%d", Rect.top);
        WritePrivateProfileString("Config", "Y", buffer, CfgFileName);
    }
    sprintf(buffer, "%d", TopMost);
    WritePrivateProfileString("Config", "TopMost", buffer, CfgFileName);
    sprintf(buffer, "%d", TCheckBox);
    WritePrivateProfileString("Config", "Transparent", buffer, CfgFileName);
    sprintf(buffer, "%d", TrackBarPos);
    WritePrivateProfileString("Config", "Transparency", buffer, CfgFileName);
    WritePrivateProfileString("Config", "NIC", G_NIC, CfgFileName);
    //MessageBox(NULL, CfgFileName, "Show",MB_OK);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// �����̣߳�������������������ȷ����ʱ׼ȷȡ������
void UpdateDataTimer (void *Param)
{
    while (TRUE)
    {
        sleep (Update_ms);  // ��Щʱ�� InitDialog
        UpdateDataProc();
    }
}

void UpdateDataProc (void)
{
    char buffer[16];
    BOOL dataOk, MNC_Exist;
    DWORD i;

    // ������������(��ȡ������)
    free(pIfTable[0]);
    pIfTable[0] = pIfTable[1];
    pIfTable[1] = GetCurpIfTable();
    
    // �������洦�����ݺ�ʱ��ȡʵ��ʱ��
    Clock_Tick[0] = Clock_Tick[1];
    Clock_Tick[1] = clock();
    DurSeconds = Clock_Tick[1] - Clock_Tick[0];
    //char buffer[MAX_PATH]; sprintf(buffer, "%f", DurSeconds); ShowNotifyBallon(hWND_MAIN, "���κ�ʱ", buffer);
    DurSeconds /= CLOCKS_PER_SEC; // ���� 0.001s
    //char buffer[MAX_PATH]; sprintf(buffer, "%f", DurSeconds); ShowNotifyBallon(hWND_MAIN, "���κ�ʱ", buffer);
    
    // ��ȡ�����ݣ����ܸ�ʽ��ȷ
    if (pIfTable[0] != NULL && pIfTable[1] != NULL)
    {
    // ȡ���ݳɹ������Ŵ���
        // ȷ����������һ��
        if (pIfTable[1]->dwNumEntries == pIfTable[0]->dwNumEntries) dataOk = TRUE;
        // ȷ����ͬһ����
        for (i = 0; i < pIfTable[1]->dwNumEntries; i++) {
            if (pIfTable[1]->table[i].dwIndex != pIfTable[0]->table[i].dwIndex) {
                dataOk = FALSE;
                break;
            }
        }
        // �����ݴ���
        if (dataOk == TRUE)
        {
            // ��������
            // ���ָ��
            for (i = 0; i < pIfTable[1]->dwNumEntries; i++)
            {
                if (strcmp(pIfTable[1]->table[i].bDescr, G_NIC) == 0)
                {
                    MNC_idx = i;
                    MNC_Exist = TRUE;
                    //MessageBox(NULL, G_NIC, "Show",MB_OK);
                    break;
                }
                else
                {
                    MNC_Exist = FALSE;
                }
            }
            // �Զ�ѡ��
            if (MNC_Exist == FALSE)
            {
                // ���ٻ���һ��
                for (i = pIfTable[1]->dwNumEntries - 1; i >= 0; i--)
                {
                    if (pIfTable[1]->table[i].dwOperStatus == IF_OPER_STATUS_CONNECTED
                       || pIfTable[1]->table[i].dwOperStatus == IF_OPER_STATUS_OPERATIONAL)
                    {
                        MNC_idx = i;
                        break;
                    }
                }
            }
            // .ini ����
            if (strcmp(pIfTable[1]->table[MNC_idx].bDescr, G_NIC) != 0)
            {
                strcpy(G_NIC, pIfTable[1]->table[MNC_idx].bDescr);
            }
            // ֪ͨ
            if (strcmp(G_NIC, B_NIC) != 0)
            {
                strcpy(B_NIC, G_NIC);
                ShowMonitorInfoBallon(hWND_MAIN);
            }
            // ��ʾ���
            //sprintf(DownSpeed, "��%.2f KB/s", (float)(pIfTable[1]->table[MNC_idx].dwInOctets - pIfTable[0]->table[MNC_idx].dwInOctets)/1024/DurSeconds);
            FriendlyOctets((float)(pIfTable[1]->table[MNC_idx].dwInOctets - pIfTable[0]->table[MNC_idx].dwInOctets)/DurSeconds, buffer);
            sprintf(DownSpeed, "��%s/s", buffer);
            SetDlgItemText (hWND_MAIN, IDC_STC_DOWN, DownSpeed);
            //sprintf(UpSpeed, "��%.2f KB/s", (float)(pIfTable[1]->table[MNC_idx].dwOutOctets - pIfTable[0]->table[MNC_idx].dwOutOctets)/1024/DurSeconds);
            FriendlyOctets((float)(pIfTable[1]->table[MNC_idx].dwOutOctets - pIfTable[0]->table[MNC_idx].dwOutOctets)/DurSeconds, buffer);
            sprintf(UpSpeed, "��%s/s", buffer);
            SetDlgItemText (hWND_MAIN, IDC_STC_UP, UpSpeed);
            //
            if (IsWindow(hWND_MORE)) ShowDetails();
        }
    }
}

void ShowDetails(void)
{
    char buffer[128];
    HWND hComboBoxNCD;
    char NCDDes[128];
    BOOL updateLB;
    
    DWORD i;

    // �������� update Combobox
    updateLB = TRUE;
    hComboBoxNCD = GetDlgItem(hWND_MORE, IDC_CBO_NETCARD);
    if (pIfTable[1]->dwNumEntries == SendMessage(hComboBoxNCD,CB_GETCOUNT,0,0)) // δ�仯
    {
        updateLB = FALSE;
        for (i = 0; i < pIfTable[1]->dwNumEntries; i++)
        {
            SendMessage(hComboBoxNCD,CB_GETLBTEXT,i,(LPARAM)NCDDes);
            if (strcmp(pIfTable[1]->table[i].bDescr,NCDDes) != 0)
            {
                updateLB = TRUE;
                break;
            }
        }
    }
    if (updateLB) UpdateNCDListBox(hComboBoxNCD);
    // update data
    switch (pIfTable[1]->table[MNC_idx].dwOperStatus)
    {
    case IF_OPER_STATUS_NON_OPERATIONAL:
        sprintf(buffer, "%s", "�ѽ���");
        break;
    case IF_OPER_STATUS_UNREACHABLE:
        sprintf(buffer, "%s", "δ����");
        break;
    case IF_OPER_STATUS_DISCONNECTED:
        sprintf(buffer, "%s", "δ����");
        break;
    case IF_OPER_STATUS_CONNECTING:
        sprintf(buffer, "%s", "������");
        break;
    case IF_OPER_STATUS_CONNECTED:
        sprintf(buffer, "%s", "������");
        break;
    case IF_OPER_STATUS_OPERATIONAL:
        sprintf(buffer, "%s", "��ʹ��");
        break;
    default:
        sprintf(buffer, "%s", "δ֪");
    }
    SetDlgItemText (hWND_MORE, IDC_STC_STATUS_T, buffer);
    sprintf(buffer, "%s", MacS(pIfTable[1]->table[MNC_idx].bPhysAddr,pIfTable[1]->table[MNC_idx].dwPhysAddrLen));
    SetDlgItemText (hWND_MORE, IDC_STC_MAC_T, buffer);
    sprintf(buffer, "%.2f Mb", (float)pIfTable[1]->table[MNC_idx].dwSpeed/1000000);
    SetDlgItemText (hWND_MORE, IDC_STC_LIMT_T, buffer);
    // in
    SetDlgItemText (hWND_MORE, IDC_STC_INSPEED_T, DownSpeed);
    //sprintf(buffer, "%u B", pIfTable[1]->table[MNC_idx].dwInOctets);
    FriendlyOctets((float)pIfTable[1]->table[MNC_idx].dwInOctets, buffer);
    SetDlgItemText (hWND_MORE, IDC_STC_INOCT_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwInUcastPkts);
    SetDlgItemText (hWND_MORE, IDC_STC_INUCAST_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwInNUcastPkts);
    SetDlgItemText (hWND_MORE, IDC_STC_INNUCAST_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwInDiscards);
    SetDlgItemText (hWND_MORE, IDC_STC_INDISCARD_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwInErrors);
    SetDlgItemText (hWND_MORE, IDC_STC_INERROR_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwInUnknownProtos);
    SetDlgItemText (hWND_MORE, IDC_STC_INUNKNOWN_T, buffer);
    // out
    SetDlgItemText (hWND_MORE, IDC_STC_OUTSPEED_T, UpSpeed);
    //sprintf(buffer, "%u B", pIfTable[1]->table[MNC_idx].dwOutOctets);
    FriendlyOctets((float)pIfTable[1]->table[MNC_idx].dwOutOctets, buffer);
    SetDlgItemText (hWND_MORE, IDC_STC_OUTOCT_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwOutUcastPkts);
    SetDlgItemText (hWND_MORE, IDC_STC_OUTUCAST_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwOutNUcastPkts);
    SetDlgItemText (hWND_MORE, IDC_STC_OUTNUCAST_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwOutDiscards);
    SetDlgItemText (hWND_MORE, IDC_STC_OUTDISCARD_T, buffer);
    sprintf(buffer, "%u", pIfTable[1]->table[MNC_idx].dwOutErrors);
    SetDlgItemText (hWND_MORE, IDC_STC_OUTERROR_T, buffer);
}

char *MacS(BYTE bPhysAddr[], DWORD Len)
{
    DWORD i = 0;
    //printf("%d\n", MAXLEN_PHYSADDR);
    static char pMacStr[MAXLEN_PHYSADDR * 3 + 1];
    if (Len > 0)
    {
        for (i = 0; i < Len; i++) {
            sprintf(pMacStr + i * 3, "%0.2X-", bPhysAddr[i]);
        }
        memset(pMacStr + strlen(pMacStr) - 1, 0, 1);
    }
    else
    {
        memset(pMacStr, 0, strlen(pMacStr));
    }
    return pMacStr;
}

void FriendlyOctets(float Octets, char *buffer)
{
    float num;
    num = Octets/1073741824;
    if (num >= 1)
    {
        sprintf(buffer, "%.2f GB", num);
        return;
    }
    num = Octets/1048576;
    if (num >= 1)
    {
        sprintf(buffer, "%.2f MB", num);
        return;
    }
    num = Octets/1024;
    if (num >= 1)
    {
        sprintf(buffer, "%.2f KB", num);
        return;
    }
    sprintf(buffer, "%.2f B", Octets);
    return;
}

void *GetCurpIfTable(void)
{
    static MIB_IFTABLE *pIfTable;
    DWORD dwSize;
    DWORD dwRetVal;

    dwSize = 0;
    pIfTable = malloc(dwSize);
    dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        free(pIfTable);
        pIfTable = malloc(dwSize);
        dwRetVal = GetIfTable(pIfTable, &dwSize, TRUE);
    }
    // ������ʧ��
    if (dwRetVal != NO_ERROR) {
        // MessageBox(NULL, "Errro", "Show",MB_OK);
        // �ͷ�
        free(pIfTable);
        // �쳣�˳�
        return;
    }
    return pIfTable;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// �����̣߳���������������
void MiscTimer (void *Param)
{
    while (TRUE)
    {
        sleep (Update_ms * 10);
        if (!IsWindow(hWND_MORE)) SetMainTopMost();   // ȷ���ö�
    }
}
