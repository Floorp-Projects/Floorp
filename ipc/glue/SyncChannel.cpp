/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/ipc/SyncChannel.h"
#include "mozilla/ipc/RPCChannel.h"

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

const int32_t SyncChannel::kNoTimeout = INT32_MIN;

SyncChannel::SyncChannel(SyncListener* aListener)
  : AsyncChannel(aListener)
#ifdef OS_WIN
  , mTopFrame(nullptr)
#endif
  , mPendingReply(0)
  , mProcessingSyncMessage(false)
  , mNextSeqno(0)
  , mInTimeoutSecondHalf(false)
  , mTimeoutMs(kNoTimeout)
{
    MOZ_COUNT_CTOR(SyncChannel);
#ifdef OS_WIN
    mEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    NS_ASSERTION(mEvent, "CreateEvent failed! Nothing is going to work!");
#endif
}

SyncChannel::~SyncChannel()
{
    MOZ_COUNT_DTOR(SyncChannel);
#ifdef OS_WIN
    DebugOnly<BOOL> ok = CloseHandle(mEvent);
    MOZ_ASSERT(ok);
#endif
}

// static
bool SyncChannel::sIsPumpingMessages = false;

bool
SyncChannel::EventOccurred()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();
    NS_ABORT_IF_FALSE(AwaitingSyncReply(), "not in wait loop");

    return !Connected() ||
           mRecvd.type() != 0 ||
           mRecvd.is_reply_error() ||
           !mUrgent.empty();
}

bool
SyncChannel::ProcessUrgentMessages()
{
    while (!mUrgent.empty()) {
        Message msg = mUrgent.front();
        mUrgent.pop_front();

        MOZ_ASSERT(msg.priority() == IPC::Message::PRIORITY_HIGH);

        {
            MOZ_ASSERT(msg.is_sync() || msg.is_rpc());

            MonitorAutoUnlock unlock(*mMonitor);
            SyncChannel::OnDispatchMessage(msg);
        }

        // Check state that could have been changed during dispatch.
        if (!Connected()) {
            ReportConnectionError("SyncChannel");
            return false;
        }

        // We should not have received another synchronous reply,
        // because we cannot send synchronous messages in this state.
        MOZ_ASSERT(mRecvd.type() == 0);
    }

    return true;
}

bool
SyncChannel::Send(Message* _msg, Message* reply)
{
    if (mPendingReply) {
        // This is a temporary hack in place, for e10s CPOWs, until bug 901789
        // and the new followup RPC protocol land. Eventually this will become
        // an assert again. See bug 900062 for details.
        NS_ERROR("Nested sync messages are not supported");
        return false;
    }

    nsAutoPtr<Message> msg(_msg);

    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    NS_ABORT_IF_FALSE(!ProcessingSyncMessage(),
                      "violation of sync handler invariant");
    NS_ABORT_IF_FALSE(msg->is_sync(), "can only Send() sync messages here");

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    msg->set_seqno(NextSeqno());

    MonitorAutoLock lock(*mMonitor);

    if (!Connected()) {
        ReportConnectionError("SyncChannel");
        return false;
    }

    mPendingReply = msg->type() + 1;
    DebugOnly<int32_t> msgSeqno = msg->seqno();
    mLink->SendMessage(msg.forget());

    while (1) {
        // if a handler invoked by *Dispatch*() spun a nested event
        // loop, and the connection was broken during that loop, we
        // might have already processed the OnError event. if so,
        // trying another loop iteration will be futile because
        // channel state will have been cleared
        if (!Connected()) {
            ReportConnectionError("SyncChannel");
            return false;
        }

        while (!EventOccurred()) {
            bool maybeTimedOut = !SyncChannel::WaitForNotify();

            if (EventOccurred())
                break;

            // we might have received a "subtly deferred" message
            // in a nested loop that it's now time to process
            if (!mUrgent.empty())
                break;

            if (maybeTimedOut && !ShouldContinueFromTimeout())
                return false;
        }

        if (!Connected()) {
            ReportConnectionError("SyncChannel");

            return false;
        }

        // Process all urgent messages. We forbid nesting synchronous sends,
        // so mPendingReply etc will still be valid.
        if (!ProcessUrgentMessages())
            return false;

        if (mRecvd.is_reply_error() || mRecvd.type() != 0) {
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
            if (mRecvd.is_reply_error())
                return false;

            *reply = TakeReply();

            MOZ_ASSERT(mUrgent.empty());
            return true;
        }
    }

    return true;
}

void
SyncChannel::OnDispatchMessage(const Message& msg)
{
    AssertWorkerThread();
    NS_ABORT_IF_FALSE(msg.is_sync() || msg.is_rpc(), "only sync messages here");
    NS_ABORT_IF_FALSE(!msg.is_reply(), "wasn't awaiting reply");

    Message* reply = 0;

    mProcessingSyncMessage = true;
    Result rv;
    if (msg.is_sync())
        rv = static_cast<SyncListener*>(mListener.get())->OnMessageReceived(msg, reply);
    else
        rv = static_cast<RPCChannel::RPCListener*>(mListener.get())->OnCallReceived(msg, reply);
    mProcessingSyncMessage = false;

    if (!MaybeHandleError(rv, "SyncChannel")) {
        // FIXME/cjones: error handling; OnError()?
        delete reply;
        reply = new Message();
        if (msg.is_sync())
            reply->set_sync();
        else if (msg.is_rpc())
            reply->set_rpc();
        reply->set_reply();
        reply->set_reply_error();
    }

    reply->set_seqno(msg.seqno());

    {
        MonitorAutoLock lock(*mMonitor);
        if (ChannelConnected == mChannelState)
            mLink->SendMessage(reply);
    }
}

//
// The methods below run in the context of the link thread, and can proxy
// back to the methods above
//

void
SyncChannel::OnMessageReceivedFromLink(const Message& msg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MaybeInterceptSpecialIOMessage(msg))
        return;

    if (msg.priority() == IPC::Message::PRIORITY_HIGH) {
        // If the message is high priority, we skip the worker entirely, and
        // wake up the loop that's spinning for a reply.
        if (!AwaitingSyncReply()) {
            mWorkerLoop->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &SyncChannel::OnDispatchMessage, msg));
        } else {
            mUrgent.push_back(msg);
            NotifyWorkerThread();
        }
        return;
    }

    if (!msg.is_sync()) {
        AsyncChannel::OnMessageReceivedFromLink(msg);
        return;
    }

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
SyncChannel::OnChannelErrorFromLink()
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (AwaitingSyncReply())
        NotifyWorkerThread();

    AsyncChannel::OnChannelErrorFromLink();
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
    mMonitor->AssertCurrentThreadOwns();

    bool cont;
    {
        MonitorAutoUnlock unlock(*mMonitor);
        cont = static_cast<SyncListener*>(mListener.get())->OnReplyTimeout();
    }

    static enum { UNKNOWN, NOT_DEBUGGING, DEBUGGING } sDebuggingChildren = UNKNOWN;

    if (sDebuggingChildren == UNKNOWN) {
        sDebuggingChildren = getenv("MOZ_DEBUG_CHILD_PROCESS") ? DEBUGGING : NOT_DEBUGGING;
    }
    if (sDebuggingChildren == DEBUGGING) {
        return true;
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

bool
SyncChannel::WaitResponse(bool aWaitTimedOut)
{
  if (aWaitTimedOut) {
    if (mInTimeoutSecondHalf) {
      // We've really timed out this time
      return false;
    }
    // Try a second time
    mInTimeoutSecondHalf = true;
  } else {
    mInTimeoutSecondHalf = false;
  }
  return true;
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

    mMonitor->Wait(timeout);

    // if the timeout didn't expire, we know we received an event.
    // The converse is not true.
    return WaitResponse(IsTimeoutExpired(waitStart, timeout));
}

void
SyncChannel::NotifyWorkerThread()
{
    mMonitor->Notify();
}

#endif  // ifndef OS_WIN


} // namespace ipc
} // namespace mozilla
