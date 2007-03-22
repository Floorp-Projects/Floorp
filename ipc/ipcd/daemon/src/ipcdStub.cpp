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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include "ipcd.h"
#include "ipcdPrivate.h"
#include "ipcLog.h"

#include "prerror.h"

//-----------------------------------------------------------------------------
// use this file as a template to add server-side IPC connectivity.
// 
// NOTE: if your platform supports local domain TCP sockets, then you should
//       be able to make use of ipcConnectionUnix.cpp.
//-----------------------------------------------------------------------------

// these variables are declared in ipcdPrivate.h and must be initialized by
// when the daemon starts up.
ipcClient *ipcClients = NULL;
int        ipcClientCount = 0;

PRStatus
IPC_PlatformSendMsg(ipcClient  *client, ipcMessage *msg)
{
    const char notimplemented[] = "IPC_PlatformSendMsg not implemented";
    PR_SetErrorText(sizeof(notimplemented), notimplemented);
    return PR_FAILURE;
}

int main(int argc, char **argv)
{
    IPC_InitLog("###");

    LOG(("daemon started...\n"));

    /*
    IPC_InitModuleReg(argv[0]);
    IPC_ShutdownModuleReg();
    */

    // let the parent process know that we are up-and-running
    IPC_NotifyParent();
    return 0;
}
