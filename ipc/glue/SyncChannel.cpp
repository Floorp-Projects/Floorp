/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "mozilla/ipc/SyncChannel.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"

using mozilla::MutexAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::SyncChannel>
{
    static void RetainCallee(mozilla::ipc::SyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::SyncChannel* obj) { }
};

namespace mozilla {
namespace ipc {

SyncChannel::SyncChannel(SyncListener* aListener)
  : AsyncChannel(aListener),
    mPendingReply(0),
    mProcessingSyncMessage(false),
    mNextSeqno(0)
{
  MOZ_COUNT_CTOR(SyncChannel);
}

SyncChannel::~SyncChannel()
{
    MOZ_COUNT_DTOR(SyncChannel);
    // FIXME/cjones: impl
}

// static
bool SyncChannel::sIsPumpingMessages = false;

bool
SyncChannel::Send(Message* msg, Message* reply)
{
    AssertWorkerThread();
    mMutex.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ABORT_IF_FALSE(msg->is_sync(), "can only Send() sync messages here");

    msg->set_seqno(NextSeqno());

    MutexAutoLock lock(mMutex);

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    mPendingReply = msg->type() + 1;
    int32 msgSeqno = msg->seqno();
    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &SyncChannel::OnSend, msg));

    // NB: this is a do-while loop instead of a single wait because if
    // there's a pending RPC out- or in-call below us, and the sync
    // message handler on the other side sends us an async message,
    // the IO thread will Notify() this thread of the async message.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=538239.
    do {
        // wait for the next sync message to arrive
        SyncChannel::WaitForNotify();
    } while(Connected() &&
            mPendingReply != mRecvd.type() && !mRecvd.is_reply_error());

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    // we just received a synchronous message from the other side.
    // If it's not the reply we were awaiting, there's a serious
    // error: either a mistimed/malformed message or a sync in-message
    // that raced with our sync out-message.
    // (NB: IPDL prevents the latter from occuring in actor code)

    // FIXME/cjones: real error handling
    NS_ABORT_IF_FALSE(mRecvd.is_sync() && mRecvd.is_reply() &&
                      (mRecvd.is_reply_error() ||
                       (mPendingReply == mRecvd.type() &&
                        msgSeqno == mRecvd.seqno())),
                      "unexpected sync message");

    mPendingReply = 0;
    *reply = mRecvd;
    mRecvd = Message();

    return true;
}

void
SyncChannel::OnDispatchMessage(const Message& msg)
{
    AssertWorkerThread();
    NS_ABORT_IF_FALSE(msg.is_sync(), "only sync messages here");
    NS_ABORT_IF_FALSE(!msg.is_reply(), "wasn't awaiting reply");

    Message* reply = 0;

    mProcessingSyncMessage = true;
    Result rv =
        static_cast<SyncListener*>(mListener)->OnMessageReceived(msg, reply);
    mProcessingSyncMessage = false;

    if (!MaybeHandleError(rv, "SyncChannel")) {
        // FIXME/cjones: error handling; OnError()?
        delete reply;
        reply = new Message();
        reply->set_sync();
        reply->set_reply();
        reply->set_reply_error();
    }

    reply->set_seqno(msg.seqno());

    mIOLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &SyncChannel::OnSend, reply));
}

//
// The methods below run in the context of the IO thread, and can proxy
// back to the methods above
//

void
SyncChannel::OnMessageReceived(const Message& msg)
{
    AssertIOThread();
    if (!msg.is_sync()) {
        return AsyncChannel::OnMessageReceived(msg);
    }

    MutexAutoLock lock(mMutex);

    if (!AwaitingSyncReply()) {
        // wake up the worker, there's work to do
        mWorkerLoop->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, msg));
    }
    else {
        // let the worker know a new sync message has arrived
        mRecvd = msg;
        NotifyWorkerThread();
    }
}

void
SyncChannel::OnChannelError()
{
    AssertIOThread();

    AsyncChannel::OnChannelError();

    MutexAutoLock lock(mMutex);
    if (AwaitingSyncReply())
        NotifyWorkerThread();
}

//
// Synchronization between worker and IO threads
//

// Windows versions of the following two functions live in
// WindowsMessageLoop.cpp.

#ifndef OS_WIN

void
SyncChannel::WaitForNotify()
{
    mCvar.Wait();
}

void
SyncChannel::NotifyWorkerThread()
{
    mCvar.Notify();
}

#endif  // ifndef OS_WIN


} // namespace ipc
} // namespace mozilla
