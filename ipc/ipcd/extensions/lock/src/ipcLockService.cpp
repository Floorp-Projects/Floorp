/* vim:set ts=4 sw=4 sts=4 et cindent: */
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

#include <stdlib.h>
#include "nsDependentString.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"
#include "ipcILockNotify.h"
#include "ipcLockService.h"
#include "ipcLockProtocol.h"
#include "ipcLog.h"

static const nsID kLockTargetID = IPC_LOCK_TARGETID;

//-----------------------------------------------------------------------------

nsresult
ipcLockService::Init()
{
    if (!mResultMap.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    // Configure OnMessageAvailable to be called on the IPC thread.  This is
    // done to allow us to proxy OnAcquireLockComplete events to the right
    // thread immediately even if the main thread is blocked waiting to acquire
    // some other lock synchronously.

    return IPC_DefineTarget(kLockTargetID, this, PR_FALSE);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(ipcLockService, ipcILockService, ipcIMessageObserver)

NS_IMETHODIMP
ipcLockService::AcquireLock(const char *lockName, PRBool waitIfBusy)
{
    LOG(("ipcLockService::AcquireLock [lock=%s wait=%u]\n", lockName, waitIfBusy));

    ipcLockMsg msg;
    msg.opcode = IPC_LOCK_OP_ACQUIRE;
    msg.flags = (waitIfBusy ? 0 : IPC_LOCK_FL_NONBLOCKING);
    msg.key = lockName;

    PRUint32 bufLen;
    nsAutoPtr<PRUint8> buf( IPC_FlattenLockMsg(&msg, &bufLen) );
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult lockStatus = NS_ERROR_UNEXPECTED;
    nsDependentCString lockNameStr(lockName);
    nsCStringHashKey hashKey(&lockNameStr);

    if (!mResultMap.Put(hashKey.GetKey(), &lockStatus))
        return NS_ERROR_OUT_OF_MEMORY;

    // prevent our OnMessageAvailable from being called until we explicitly ask
    // for it to be called via IPC_WaitMessage.
    IPC_DISABLE_MESSAGE_OBSERVER_FOR_SCOPE(kLockTargetID);

    nsresult rv = IPC_SendMessage(0, kLockTargetID, buf, bufLen);
    if (NS_SUCCEEDED(rv)) {
        // block the calling thread until we get a response from the daemon
        rv = IPC_WaitMessage(0, kLockTargetID, this, PR_INTERVAL_NO_TIMEOUT);
        if (NS_SUCCEEDED(rv))
            rv = lockStatus;
    }

    mResultMap.Remove(hashKey.GetKey());
    return rv;
}

NS_IMETHODIMP
ipcLockService::ReleaseLock(const char *lockName)
{
    LOG(("ipcLockService::ReleaseLock [lock=%s]\n", lockName));

    ipcLockMsg msg;
    msg.opcode = IPC_LOCK_OP_RELEASE;
    msg.flags = 0;
    msg.key = lockName;

    PRUint32 bufLen;
    PRUint8 *buf = IPC_FlattenLockMsg(&msg, &bufLen);
    if (!buf)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = IPC_SendMessage(0, kLockTargetID, buf, bufLen);
    delete buf;

    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
ipcLockService::OnMessageAvailable(PRUint32 unused, const nsID &target,
                                   const PRUint8 *data, PRUint32 dataLen)
{
    ipcLockMsg msg;
    IPC_UnflattenLockMsg(data, dataLen, &msg);

    LOG(("ipcLockService::OnMessageAvailable [lock=%s opcode=%u]\n", msg.key, msg.opcode));

    nsDependentCString lockNameStr(msg.key);
    nsCStringHashKey hashKey(&lockNameStr);

    nsresult *status;
    mResultMap.Get(hashKey.GetKey(), &status);

    if (msg.opcode == IPC_LOCK_OP_STATUS_ACQUIRED)
        *status = NS_OK;
    else
        *status = NS_ERROR_FAILURE;

    return NS_OK;
}
