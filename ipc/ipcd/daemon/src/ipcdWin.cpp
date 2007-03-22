/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>

#include "prthread.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcModuleReg.h"
#include "ipcdPrivate.h"
#include "ipcd.h"
#include "ipcm.h"

//
// declared in ipcdPrivate.h
//
ipcClient *ipcClients = NULL;
int        ipcClientCount = 0;

static ipcClient ipcClientArray[IPC_MAX_CLIENTS];

static HWND   ipcHwnd = NULL;
static PRBool ipcShutdown = PR_FALSE;

#define IPC_PURGE_TIMER_ID 1
#define IPC_WM_SENDMSG  (WM_USER + 1)
#define IPC_WM_SHUTDOWN (WM_USER + 2)

//-----------------------------------------------------------------------------
// client array manipulation
//-----------------------------------------------------------------------------

static void
RemoveClient(ipcClient *client)
{
    LOG(("RemoveClient\n"));

    int clientIndex = client - ipcClientArray;

    client->Finalize();

    //
    // move last ipcClient object down into the spot occupied by this client.
    //
    int fromIndex = ipcClientCount - 1;
    int toIndex = clientIndex;
    if (toIndex != fromIndex)
        memcpy(&ipcClientArray[toIndex], &ipcClientArray[fromIndex], sizeof(ipcClient));

    memset(&ipcClientArray[fromIndex], 0, sizeof(ipcClient));

    --ipcClientCount;
    LOG(("  num clients = %u\n", ipcClientCount));

    if (ipcClientCount == 0) {
        LOG(("  shutting down...\n"));
        KillTimer(ipcHwnd, IPC_PURGE_TIMER_ID);
        PostQuitMessage(0);
        ipcShutdown = PR_TRUE;
    }
}

static void
PurgeStaleClients()
{
    if (ipcClientCount == 0)
        return;

    LOG(("PurgeStaleClients [num-clients=%u]\n", ipcClientCount));
    //
    // walk the list of supposedly active clients, and verify the existance of
    // their respective message windows.
    //
    char wName[IPC_CLIENT_WINDOW_NAME_MAXLEN];
    for (int i=ipcClientCount-1; i>=0; --i) {
        ipcClient *client = &ipcClientArray[i];

        LOG(("  checking client at index %u [client-id=%u pid=%u]\n", 
            i, client->ID(), client->PID()));

        IPC_GetClientWindowName(client->PID(), wName);

        // XXX dougt has ideas about how to make this better

        HWND hwnd = FindWindow(IPC_CLIENT_WINDOW_CLASS, wName);
        if (!hwnd) {
            LOG(("  client window not found; removing client!\n"));
            RemoveClient(client);
        }
    }
}

static ipcClient *
AddClient(HWND hwnd, PRUint32 pid)
{
    LOG(("AddClient\n"));

    //
    // before adding a new client, verify that all existing clients are
    // still up and running.  remove any stale clients.
    //
    PurgeStaleClients();

    if (ipcClientCount == IPC_MAX_CLIENTS) {
        LOG(("  reached maximum client count!\n"));
        return NULL;
    }

    ipcClient *client = &ipcClientArray[ipcClientCount];
    client->Init();
    client->SetHwnd(hwnd);
    client->SetPID(pid);    // XXX one function instead of 3

    ++ipcClientCount;
    LOG(("  num clients = %u\n", ipcClientCount));

    if (ipcClientCount == 1)
        SetTimer(ipcHwnd, IPC_PURGE_TIMER_ID, 1000, NULL);

    return client;
}

static ipcClient *
GetClientByPID(PRUint32 pid)
{
    for (int i=0; i<ipcClientCount; ++i) {
        if (ipcClientArray[i].PID() == pid)
            return &ipcClientArray[i];
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// message processing
//-----------------------------------------------------------------------------

static void
ProcessMsg(HWND hwnd, PRUint32 pid, const ipcMessage *msg)
{
    LOG(("ProcessMsg [pid=%u len=%u]\n", pid, msg->MsgLen()));

    ipcClient *client = GetClientByPID(pid);

    if (client) {
        //
        // if this is an IPCM "client hello" message, then reset the client
        // instance object.
        //
        if (msg->Target().Equals(IPCM_TARGET) &&
            IPCM_GetType(msg) == IPCM_MSG_REQ_CLIENT_HELLO) {
            RemoveClient(client);
            client = NULL;
        }
    }

    if (client == NULL) {
        client = AddClient(hwnd, pid);
        if (!client)
            return;
    }

    IPC_DispatchMsg(client, msg);
}

//-----------------------------------------------------------------------------

PRStatus
IPC_PlatformSendMsg(ipcClient  *client, ipcMessage *msg)
{
    LOG(("IPC_PlatformSendMsg [clientID=%u clientPID=%u]\n",
        client->ID(), client->PID()));

    // use PostMessage to make this asynchronous; otherwise we might get
    // some wierd SendMessage recursion between processes.

    WPARAM wParam = (WPARAM) client->Hwnd();
    LPARAM lParam = (LPARAM) msg;
    if (!PostMessage(ipcHwnd, IPC_WM_SENDMSG, wParam, lParam)) {
        LOG(("PostMessage failed\n"));
        delete msg;
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// windows message loop
//-----------------------------------------------------------------------------

static LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LOG(("got message [msg=%x wparam=%x lparam=%x]\n", uMsg, wParam, lParam));

    if (uMsg == WM_COPYDATA) {
        if (ipcShutdown) {
            LOG(("ignoring message b/c daemon is shutting down\n"));
            return TRUE;
        }
        COPYDATASTRUCT *cd = (COPYDATASTRUCT *) lParam;
        if (cd && cd->lpData) {
            ipcMessage msg;
            PRUint32 bytesRead;
            PRBool complete;
            // XXX avoid extra malloc
            PRStatus rv = msg.ReadFrom((const char *) cd->lpData, cd->cbData,
                                       &bytesRead, &complete);
            if (rv == PR_SUCCESS && complete) {
                //
                // grab client PID and hwnd.
                //
                ProcessMsg((HWND) wParam, (PRUint32) cd->dwData, &msg);
            }
            else
                LOG(("ignoring malformed message\n"));
        }
        return TRUE;
    }

    if (uMsg == IPC_WM_SENDMSG) {
        HWND hWndDest = (HWND) wParam;
        ipcMessage *msg = (ipcMessage *) lParam;

        COPYDATASTRUCT cd;
        cd.dwData = GetCurrentProcessId();
        cd.cbData = (DWORD) msg->MsgLen();
        cd.lpData = (PVOID) msg->MsgBuf();

        LOG(("calling SendMessage...\n"));
        SendMessage(hWndDest, WM_COPYDATA, (WPARAM) hWnd, (LPARAM) &cd);
        LOG(("  done.\n"));

        delete msg;
        return 0;
    }

    if (uMsg == WM_TIMER) {
        PurgeStaleClients();
        return 0;
    }

#if 0
    if (uMsg == IPC_WM_SHUTDOWN) {
        //
        // since this message is handled asynchronously, it is possible
        // that other clients may have come online since this was issued.
        // in which case, we need to ignore this message.
        //
        if (ipcClientCount == 0) {
            ipcShutdown = PR_TRUE;
            PostQuitMessage(0);
        }
        return 0;
    }
#endif

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//-----------------------------------------------------------------------------
// daemon startup synchronization
//-----------------------------------------------------------------------------

static HANDLE ipcSyncEvent;

static PRBool
AcquireLock()
{
    ipcSyncEvent = CreateEvent(NULL, FALSE, FALSE,
                               IPC_SYNC_EVENT_NAME);
    if (!ipcSyncEvent) {
        LOG(("CreateEvent failed [%u]\n", GetLastError()));
        return PR_FALSE;
    }

    // check to see if event already existed prior to this call.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG(("  lock already set; exiting...\n"));
        return PR_FALSE;
    }
    
    LOG(("  acquired lock\n"));
    return PR_TRUE;
}

static void
ReleaseLock()
{
    if (ipcSyncEvent) {
        LOG(("releasing lock...\n"));
        CloseHandle(ipcSyncEvent);
        ipcSyncEvent = NULL;
    }
}

//-----------------------------------------------------------------------------
// main
//-----------------------------------------------------------------------------

#ifdef DEBUG
int
main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
    IPC_InitLog("###");

    LOG(("daemon started...\n"));

    if (!AcquireLock()) {
        // unblock the parent; it should be able to find the IPC window of the
        // other daemon process.
        IPC_NotifyParent();
        return 0;
    }

    // initialize global data
    memset(ipcClientArray, 0, sizeof(ipcClientArray));
    ipcClients = ipcClientArray;
    ipcClientCount = 0;

    // create message window up front...
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = IPC_WINDOW_CLASS;

    RegisterClass(&wc);

    ipcHwnd = CreateWindow(IPC_WINDOW_CLASS, IPC_WINDOW_NAME,
                           0, 0, 0, 10, 10, NULL, NULL, NULL, NULL);

    // unblock the parent process; it should now look for the IPC window.
    IPC_NotifyParent();

    if (!ipcHwnd)
        return -1;

    // load modules relative to the location of the executable...
    {
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, sizeof(path));
        IPC_InitModuleReg(path);
    }

    LOG(("entering message loop...\n"));
    MSG msg;
    while (GetMessage(&msg, ipcHwnd, 0, 0))
        DispatchMessage(&msg);

    // unload modules
    IPC_ShutdownModuleReg();

    //
    // we release the daemon lock before destroying the window because the
    // absence of our window is what will cause clients to try to spawn the
    // daemon.
    //
    ReleaseLock();

    //LOG(("sleeping 5 seconds...\n"));
    //PR_Sleep(PR_SecondsToInterval(5));

    LOG(("destroying message window...\n"));
    DestroyWindow(ipcHwnd);
    ipcHwnd = NULL;

    LOG(("exiting\n"));
    return 0;
}
