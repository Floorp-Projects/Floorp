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

#ifndef ipcModuleUtil_h__
#define ipcModuleUtil_h__

#include "prlog.h"
#include "ipcModule.h"

extern const ipcDaemonMethods *gIPCDaemonMethods;

//-----------------------------------------------------------------------------
// inline wrapper functions
// 
// these functions may only be called by a module that uses the
// IPC_IMPL_GETMODULES macro.
//-----------------------------------------------------------------------------

inline PRStatus
IPC_DispatchMsg(ipcClientHandle client, const nsID &target, const void *data, PRUint32 dataLen)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->dispatchMsg(client, target, data, dataLen);
}

inline PRStatus
IPC_SendMsg(ipcClientHandle client, const nsID &target, const void *data, PRUint32 dataLen)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->sendMsg(client, target, data, dataLen);
}

inline ipcClientHandle
IPC_GetClientByID(PRUint32 id)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientByID(id);
}

inline ipcClientHandle
IPC_GetClientByName(const char *name)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientByName(name);
}

inline void
IPC_EnumClients(ipcClientEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClients(func, closure);
}

inline PRUint32
IPC_GetClientID(ipcClientHandle client)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientID(client);
}

inline PRBool
IPC_ClientHasName(ipcClientHandle client, const char *name)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->clientHasName(client, name);
}

inline PRBool
IPC_ClientHasTarget(ipcClientHandle client, const nsID &target)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->clientHasTarget(client, target);
}

inline void
IPC_EnumClientNames(ipcClientHandle client, ipcClientNameEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClientNames(client, func, closure);
}

inline void
IPC_EnumClientTargets(ipcClientHandle client, ipcClientTargetEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClientTargets(client, func, closure);
}

//-----------------------------------------------------------------------------
// inline composite functions
//-----------------------------------------------------------------------------

inline PRStatus
IPC_SendMsg(PRUint32 clientID, const nsID &target, const void *data, PRUint32 dataLen)
{
    ipcClient *client = IPC_GetClientByID(clientID);
    if (!client)
        return PR_FAILURE;
    return IPC_SendMsg(client, target, data, dataLen);
}

//-----------------------------------------------------------------------------
// module factory macros
//-----------------------------------------------------------------------------

#define IPC_IMPL_GETMODULES(_modName, _modEntries)                  \
    const ipcDaemonMethods *gIPCDaemonMethods;                      \
    IPC_EXPORT int                                                  \
    IPC_GetModules(const ipcDaemonMethods *dmeths,                  \
                   const ipcModuleEntry **ents) {                   \
        /* XXX do version checking */                               \
        gIPCDaemonMethods = dmeths;                                 \
        *ents = _modEntries;                                        \
        return sizeof(_modEntries) / sizeof(ipcModuleEntry);        \
    }

#endif // !ipcModuleUtil_h__
