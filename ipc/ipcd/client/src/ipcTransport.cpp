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

#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIFile.h"
#include "nsIProcess.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEventQueueUtils.h"
#include "nsAutoLock.h"
#include "prproces.h"
#include "prerror.h"
#include "plstr.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcConnection.h"
#include "ipcMessageUtils.h"
#include "ipcTransport.h"
#include "ipcm.h"

//-----------------------------------------------------------------------------

static ipcTransport *gTransport = nsnull;

void
IPC_OnConnectionEnd(nsresult error)
{
    LOG(("IPC_OnConnectionEnd [error=%x]\n", error));

    // if we hit a connection error during a sync message, then we need to
    // unblock the waiting thread.
    {
        nsAutoMonitor mon(gTransport->mMonitor);
        gTransport->mHaveConnection = PR_FALSE;
        mon.Notify();
    }

    gTransport->ProxyToMainThread(ipcTransport::ConnectionLost_EventHandler);
    NS_RELEASE(gTransport);
}

void
IPC_OnMessageAvailable(ipcMessage *msg)
{
    gTransport->OnMessageAvailable(msg);
}

//-----------------------------------------------------------------------------

nsresult
IPC_SpawnDaemon(const char *path)
{
    PRFileDesc *readable = NULL, *writable = NULL;
    PRProcessAttr *attr = NULL;
    nsresult rv = NS_ERROR_FAILURE;
    char *const argv[] = { (char *const) path, NULL };
    char c;

    // setup an anonymous pipe that we can use to determine when the daemon
    // process has started up.  the daemon will write a char to the pipe, and
    // when we read it, we'll know to proceed with trying to connect to the
    // daemon's socket port.

    if (PR_CreatePipe(&readable, &writable) != PR_SUCCESS)
        goto end;
    PR_SetFDInheritable(writable, PR_TRUE);

    attr = PR_NewProcessAttr();
    if (!attr)
        goto end;

    if (PR_ProcessAttrSetInheritableFD(attr, writable, IPC_STARTUP_PIPE_NAME) != PR_SUCCESS)
        goto end;

    if (PR_CreateProcessDetached(path, argv, NULL, attr) != PR_SUCCESS)
        goto end;

    if ((PR_Read(readable, &c, 1) != 1) && (c != IPC_STARTUP_PIPE_MAGIC))
        goto end;

    rv = NS_OK;
end:
    if (readable)
        PR_Close(readable);
    if (writable)
        PR_Close(writable);
    if (attr)
        PR_DestroyProcessAttr(attr);
    return rv;
}

//-----------------------------------------------------------------------------
// ipcTransport
//-----------------------------------------------------------------------------

nsresult
ipcTransport::Init(ipcTransportObserver *obs, PRUint32 *clientID)
{
    LOG(("ipcTransport::Init\n"));

    nsresult rv;
    nsCOMPtr<nsIFile> file;
    nsCAutoString path;

    rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(file));
    if (NS_FAILED(rv))
        return rv;

    rv = file->AppendNative(NS_LITERAL_CSTRING(IPC_DAEMON_APP_NAME));
    if (NS_FAILED(rv))
        return rv;

    rv = file->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

    // stash reference to self so we can handle the callbacks.
    NS_ADDREF(gTransport = this);

    rv = IPC_Connect(path.get());
    if (NS_SUCCEEDED(rv)) {
        // okay, send the CLIENT_HELLO message, and wait for the CLIENT_ID
        // response message from the IPC daemon.
        {
            nsAutoMonitor mon(mMonitor);

            mHaveConnection = PR_TRUE;

            ipcMessage *reply = nsnull;
            rv = SendMsg_Locked(new ipcmMessageClientHello(), PR_TRUE, &reply);
            if (NS_SUCCEEDED(rv) && reply &&
                reply->Target().Equals(IPCM_TARGET) &&
                IPCM_GetMsgType(reply) == IPCM_MSG_TYPE_CLIENT_ID) {
                // remember our client ID
                ipcMessageCast<ipcmMessageClientID> msg(reply);

                *clientID = msg->ClientID();
                mHaveConnection = PR_TRUE;
                mObserver = obs;

                LOG(("connected w/ client ID = %u\n", *clientID));
                return rv; // return with success!
            }

            mHaveConnection = PR_FALSE;
        }
        // otherwise, we need to tear down the connection, and cleanup.
        IPC_Disconnect();
    }

    // cleanup before exiting
    NS_RELEASE(gTransport);
    return rv;
}

nsresult
ipcTransport::Shutdown()
{
    LOG(("ipcTransport::Shutdown\n"));

    return IPC_Disconnect();
}

nsresult
ipcTransport::SendMsg(ipcMessage *msg, PRBool sync)
{
    NS_ENSURE_ARG_POINTER(msg);
    NS_ENSURE_TRUE(mObserver, NS_ERROR_NOT_INITIALIZED);

    LOG(("ipcTransport::SendMsg [msg=%p dataLen=%u]\n", msg, msg->DataLen()));

    nsresult rv;
    ipcMessage *syncReply = nsnull;
    {
        nsAutoMonitor mon(mMonitor);
        NS_ENSURE_TRUE(mHaveConnection, NS_ERROR_NOT_INITIALIZED);

        rv = SendMsg_Locked(msg, sync, &syncReply);
    }
    if (syncReply) {
        // NOTE: may re-enter SendMsg
        mObserver->OnMessageAvailable(syncReply);
        delete syncReply;
    }
    return rv;
}

nsresult
ipcTransport::SendMsg_Locked(ipcMessage *msg, PRBool sync, ipcMessage **syncReply)
{
    nsresult rv;

    if (sync) {
        msg->SetFlag(IPC_MSG_FLAG_SYNC_QUERY);
        // flag before sending to avoid race with background thread.
        mSyncWaiting = PR_TRUE;
    }

    rv = IPC_SendMsg(msg);

    if (sync && NS_SUCCEEDED(rv)) {
        LOG(("  waiting for synchronous response...\n"));

        // break out of this loop if we receive a the response to the sync msg
        // or if the connection is lost...
        while (mSyncWaiting && mHaveConnection)
            PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT);

        // if the connection is lost, then we will not have a sync reply.
        if (!mHaveConnection) {
            LOG(("  connection lost while waiting for sync response\n"));
            rv = NS_ERROR_UNEXPECTED;
        }

        *syncReply = mSyncReplyMsg;
        mSyncReplyMsg = nsnull;
    }
    return rv;
}

void
ipcTransport::ProcessIncomingMsgQ()
{
    LOG(("ipcTransport::ProcessIncomingMsgQ\n"));

    // we can't hold mMonitor while calling into the observer, so we grab
    // mIncomingMsgQ and NULL it out inside the monitor to prevent others
    // from modifying it while we iterate over it.
    ipcMessageQ *inQ;
    {
        nsAutoMonitor mon(mMonitor);
        inQ = mIncomingMsgQ;
        mIncomingMsgQ = nsnull;
    }
    if (inQ) {
        while (!inQ->IsEmpty()) {
            ipcMessage *msg = inQ->First();
            if (mObserver)
                mObserver->OnMessageAvailable(msg);
            inQ->DeleteFirst();
        }
        delete inQ;
    }
}

void * PR_CALLBACK
ipcTransport::ProcessIncomingMsgQ_EventHandler(PLEvent *ev)
{
    ipcTransport *self = (ipcTransport *) PL_GetEventOwner(ev);
    self->ProcessIncomingMsgQ();
    return nsnull;
}

void * PR_CALLBACK
ipcTransport::ConnectionLost_EventHandler(PLEvent *ev)
{
    ipcTransport *self = (ipcTransport *) PL_GetEventOwner(ev);
    if (self->mObserver) {
        self->mObserver->OnConnectionLost();
        self->mObserver = nsnull;
    }
    return nsnull;
}

void PR_CALLBACK
ipcTransport::Generic_EventCleanup(PLEvent *ev)
{
    ipcTransport *self = (ipcTransport *) PL_GetEventOwner(ev);
    NS_RELEASE(self);
    delete ev;
}

// called on a background thread
void
ipcTransport::OnMessageAvailable(ipcMessage *rawMsg)
{
    LOG(("ipcTransport::OnMessageAvailable [msg=%p dataLen=%u]\n",
        rawMsg, rawMsg->DataLen()));

    //
    // XXX FIX COMMENTS XXX
    //
    // 1- append to incoming message queue
    //
    // 2- post event to main thread to handle incoming message queue
    //    or if sync waiting, unblock waiter so it can scan incoming
    //    message queue.
    //

    PRBool dispatchEvent = PR_FALSE;
    {
        nsAutoMonitor mon(mMonitor);

        LOG(("  mSyncWaiting=%u MSG_FLAG_SYNC_REPLY=%u\n",
             mSyncWaiting, rawMsg->TestFlag(IPC_MSG_FLAG_SYNC_REPLY) != 0));

        if (mSyncWaiting && rawMsg->TestFlag(IPC_MSG_FLAG_SYNC_REPLY)) {
            mSyncReplyMsg = rawMsg;
            mSyncWaiting = PR_FALSE;
            mon.Notify();
        }
        else {
            // we batch up all incoming event processing.  this is done to
            // avoid crushing the main thread's event queue with a bunch of
            // IPC events.  of course, this could also cause us to process
            // too many events synchronously on the main thread.  that could
            // be a different kind of performance issue ;-)
            if (!mIncomingMsgQ) {
                mIncomingMsgQ = new ipcMessageQ();
                if (!mIncomingMsgQ)
                    return;
                dispatchEvent = PR_TRUE;
            }
            mIncomingMsgQ->Append(rawMsg);
        }

        LOG(("  dispatchEvent=%u mSyncReplyMsg=%p mIncomingMsgQ=%p\n",
            dispatchEvent, mSyncReplyMsg, mIncomingMsgQ));
    }

    if (dispatchEvent)
        ProxyToMainThread(ProcessIncomingMsgQ_EventHandler);
}

void
ipcTransport::ProxyToMainThread(PLHandleEventProc proc)
{
    LOG(("ipcTransport::ProxyToMainThread\n"));

    nsCOMPtr<nsIEventQueue> eq;
    NS_GetMainEventQ(getter_AddRefs(eq));
    if (eq) {
        PLEvent *ev = new PLEvent();
        PL_InitEvent(ev, this, proc, Generic_EventCleanup);
        NS_ADDREF_THIS();
        if (NS_FAILED(eq->PostEvent(ev))) {
            LOG(("  PostEvent failed"));
            NS_RELEASE_THIS();
            delete ev;
        }
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS0(ipcTransport)
