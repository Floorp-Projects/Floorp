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

#ifndef ipcProto_h__
#define ipcProto_h__

#if defined(XP_WIN)
//
// use WM_COPYDATA messages
//
#include "prprf.h"

#define IPC_WINDOW_CLASS              "Mozilla:IPCWindowClass"
#define IPC_WINDOW_NAME               "Mozilla:IPCWindow"
#define IPC_CLIENT_WINDOW_CLASS       "Mozilla:IPCAppWindowClass"
#define IPC_CLIENT_WINDOW_NAME_PREFIX "Mozilla:IPCAppWindow:"
#define IPC_SYNC_EVENT_NAME           "Local\\MozillaIPCSyncEvent"
#define IPC_DAEMON_APP_NAME           "mozilla-ipcd.exe"
#define IPC_PATH_SEP_CHAR             '\\'
#define IPC_MODULES_DIR               "ipc\\modules"

#define IPC_CLIENT_WINDOW_NAME_MAXLEN (sizeof(IPC_CLIENT_WINDOW_NAME_PREFIX) + 20)

// writes client name into buf.  buf must be at least
// IPC_CLIENT_WINDOW_NAME_MAXLEN bytes in length.
inline void IPC_GetClientWindowName(PRUint32 pid, char *buf)
{
    PR_snprintf(buf, IPC_CLIENT_WINDOW_NAME_MAXLEN, "%s%u",
                IPC_CLIENT_WINDOW_NAME_PREFIX, pid);
}

#else
#include "prtypes.h"
//
// use UNIX domain socket
//
#define IPC_PORT                0
#define IPC_SOCKET_TYPE         "ipc"
#define IPC_DAEMON_APP_NAME     "mozilla-ipcd"
#ifdef XP_OS2
#define IPC_PATH_SEP_CHAR       '\\'
#define IPC_MODULES_DIR         "ipc\\modules"
#else
#define IPC_PATH_SEP_CHAR       '/'
#define IPC_MODULES_DIR         "ipc/modules"
#endif

void IPC_GetDefaultSocketPath(char *buf, PRUint32 bufLen);

#endif

// common shared configuration values

#define IPC_STARTUP_PIPE_NAME   "ipc:startup-pipe"
#define IPC_STARTUP_PIPE_MAGIC  0x1C

#endif // !ipcProto_h__
