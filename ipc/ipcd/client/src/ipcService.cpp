/* vim:set ts=4 sw=4 et cindent: */
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
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
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

#include "ipcService.h"

// The ipcService implementation is nothing more than a thin XPCOM wrapper
// around the ipcdclient.h API.

NS_IMPL_THREADSAFE_ISUPPORTS1(ipcService, ipcIService)

NS_IMETHODIMP
ipcService::GetID(PRUint32 *aID)
{
    return IPC_GetID(aID);
}

NS_IMETHODIMP
ipcService::AddName(const char *aName)
{
    return IPC_AddName(aName);
}

NS_IMETHODIMP
ipcService::RemoveName(const char *aName)
{
    return IPC_RemoveName(aName);
}

NS_IMETHODIMP
ipcService::AddClientObserver(ipcIClientObserver *aObserver)
{
    return IPC_AddClientObserver(aObserver);
}

NS_IMETHODIMP
ipcService::RemoveClientObserver(ipcIClientObserver *aObserver)
{
    return IPC_RemoveClientObserver(aObserver);
}

NS_IMETHODIMP
ipcService::ResolveClientName(const char *aName, PRUint32 *aID)
{
    return IPC_ResolveClientName(aName, aID);
}

NS_IMETHODIMP
ipcService::ClientExists(PRUint32 aClientID, PRBool *aResult)
{
    return IPC_ClientExists(aClientID, aResult);
}

NS_IMETHODIMP
ipcService::DefineTarget(const nsID &aTarget, ipcIMessageObserver *aObserver)
{
    return IPC_DefineTarget(aTarget, aObserver);
}

NS_IMETHODIMP
ipcService::SendMessage(PRUint32 aReceiverID, const nsID &aTarget,
                        const PRUint8 *aData, PRUint32 aDataLen)
{
    return IPC_SendMessage(aReceiverID, aTarget, aData, aDataLen);
}

NS_IMETHODIMP
ipcService::WaitMessage(PRUint32 aSenderID, const nsID &aTarget,
                        ipcIMessageObserver *aObserver,
                        PRUint32 aTimeout)
{
    return IPC_WaitMessage(aSenderID, aTarget, aObserver, PR_MillisecondsToInterval(aTimeout));
}
