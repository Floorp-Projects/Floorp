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

#ifndef IPCD_H__
#define IPCD_H__

#include "ipcModule.h"
#include "ipcMessage.h"

//-----------------------------------------------------------------------------
// IPC daemon methods (see struct ipcDaemonMethods)
//
// these functions may only be called directly from within the daemon.  plug-in
// modules must access these through the ipcDaemonMethods structure.
//-----------------------------------------------------------------------------

PRStatus        IPC_DispatchMsg          (ipcClientHandle client, const nsID &target, const void *data, PRUint32 dataLen);
PRStatus        IPC_SendMsg              (ipcClientHandle client, const nsID &target, const void *data, PRUint32 dataLen);
ipcClientHandle IPC_GetClientByID        (PRUint32 id);
ipcClientHandle IPC_GetClientByName      (const char *name);
void            IPC_EnumClients          (ipcClientEnumFunc func, void *closure);
PRUint32        IPC_GetClientID          (ipcClientHandle client);
PRBool          IPC_ClientHasName        (ipcClientHandle client, const char *name);
PRBool          IPC_ClientHasTarget      (ipcClientHandle client, const nsID &target);
void            IPC_EnumClientNames      (ipcClientHandle client, ipcClientNameEnumFunc func, void *closure);
void            IPC_EnumClientTargets    (ipcClientHandle client, ipcClientTargetEnumFunc func, void *closure);

//-----------------------------------------------------------------------------
// other internal IPCD methods
//-----------------------------------------------------------------------------

//
// dispatch message
//
PRStatus IPC_DispatchMsg(ipcClientHandle client, const ipcMessage *msg);

//
// send message, takes ownership of |msg|.
//
PRStatus IPC_SendMsg(ipcClientHandle client, ipcMessage *msg);

//
// dispatch notifications about client connects and disconnects
//
void IPC_NotifyClientUp(ipcClientHandle client);
void IPC_NotifyClientDown(ipcClientHandle client);

#endif // !IPCD_H__
