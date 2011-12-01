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

using mozilla::MonitorAutoLock;

template<>
struct RunnableMethodTraits<mozilla::ipc::SyncChannel>
{
    static void RetainCallee(mozilla::ipc::SyncChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::SyncChannel* obj) { }
};

namespace mozilla {
namespace ipc {

const int32 SyncChannel::kNoTimeout = PR_INT32_MIN;

SyncChannel::SyncChannel(SyncListener* aListener)
  : AsyncChannel(aListener)
  , mPendingReply(0)
  , mProcessingSyncMessage(false)
  , mNextSeqno(0)
  , mTimeoutMs(kNoTimeout)
#ifdef OS_WIN
  , mTopFrame(NULL)
#endif
{
    MOZ_COUNT_CTOR(SyncChannel);
#ifdef OS_WIN
    mEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    NS_ASSERTION(mEvent, "CreateEvent failed! Nothing is going to work!");
#endif
}

SyncChannel::~SyncChannel()
{
    MOZ_COUNT_DTOR(SyncChannel);
#ifdef OS_WIN
    CloseHandle(mEvent);
#endif
}

// static
bool SyncChannel::sIsPumpingMessages = false;

bool
SyncChannel::EventOccurred()
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();
    NS_ABORT_IF_FALSE(AwaitingSyncReply(), "not in wait loop");

    return (!Connected() || 0 != mRecvd.type() || mRecvd.is_reply_error());
}

bool
SyncChannel::Send(Message* _msg, Message* reply)
{
    nsAutoPtr<Message> msg(_msg);

    AssertWorkerThread();
    mMonitor.AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ABORT_IF_FALSE(msg->is_sync(), "can only Send() sync messages here");

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    msg->set_seqno(NextSeqno());

    MonitorAutoLock lock(mMonitor);

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    mPendingReply = msg->type() + 1;
    int32 msgSeqno = msg->seqno();
    SendThroughTransport(msg.forget());

    while (1) {
        bool maybeTimedOut = !SyncChannel::WaitForNotify();

        if (EventOccurred())
            break;

        if (maybeTimedOut && !ShouldContinueFromTimeout())
            return false;
    }

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
    bool replyIsError = mRecvd.is_reply_error();
    NS_ABORT_IF_FALSE(mRecvd.is_sync() && mRecvd.is_reply() &&
                      (replyIsError ||
                       (mPendingReply == mRecvd.type() &&
                        msgSeqno == mRecvd.seqno())),
                      "unexpected sync message");

    mPendingReply = 0;
    if (!replyIsError) {
        *reply = mRecvd;
    }
    mRecvd = Message();

    return !replyIsError;
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

    {
        MonitorAutoLock lock(mMonitor);
        if (ChannelConnected == mChannelState)
            SendThroughTransport(reply);
    }
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

    MonitorAutoLock lock(mMonitor);

    if (MaybeInterceptSpecialIOMessage(msg))
        return;

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

    MonitorAutoLock lock(mMonitor);

    if (ChannelClosing != mChannelState)
        mChannelState = ChannelError;

    if (AwaitingSyncReply())
        NotifyWorkerThread();

    PostErrorNotifyTask();
}

//
// Synchronization between worker and IO threads
//

namespace {

bool
IsTimeoutExpired(PRIntervalTime aStart, PRIntervalTime aTimeout)
{
    return (aTimeout != PR_INTERVAL_NO_TIMEOUT) &&
        (aTimeout <= (PR_IntervalNow() - aStart));
}

} // namespace <anon>

bool
SyncChannel::ShouldContinueFromTimeout()
{
    AssertWorkerThread();
    mMonitor.AssertCurrentThreadOwns();

    bool cont;
    {
        MonitorAutoUnlock unlock(mMonitor);
        cont = static_cast<SyncListener*>(mListener)->OnReplyTimeout();
    }

    if (!cont) {
        // NB: there's a sublety here.  If parents were allowed to
        // send sync messages to children, then it would be possible
        // for this synchronous close-on-timeout to race with async
        // |OnMessageReceived| tasks arriving from the child, posted
        // to the worker thread's event loop.  This would complicate
        // cleanup of the *Channel.  But since IPDL forbids this (and
        // since it doesn't support children timing out on parents),
        // the parent can only block on RPC messages to the child, and
        // in that case arriving async messages are enqueued to the
        // RPC channel's special queue.  They're then ignored because
        // the channel state changes to ChannelTimeout
        // (i.e. !Connected).
        SynchronouslyClose();
        mChannelState = ChannelTimeout;
    }
        
    return cont;
}

// Windows versions of the following two functions live in
// WindowsMessageLoop.cpp.

#ifndef OS_WIN

bool
SyncChannel::WaitForNotify()
{
    PRIntervalTime timeout = (kNoTimeout == mTimeoutMs) ?
                             PR_INTERVAL_NO_TIMEOUT :
                             PR_MillisecondsToInterval(mTimeoutMs);
    // XXX could optimize away this syscall for "no timeout" case if desired
    PRIntervalTime waitStart = PR_IntervalNow();

    mMonitor.Wait(timeout);

    // if the timeout didn't expire, we know we received an event.
    // The converse is not true.
    return !IsTimeoutExpired(waitStart, timeout);
}

void
SyncChannel::NotifyWorkerThread()
{
    mMonitor.Notify();
}

#endif  // ifndef OS_WIN


} // namespace ipc
} // namespace mozilla
