/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "nsDebug.h"
#include "nsTraceRefcnt.h"

using namespace mozilla;
using namespace std;

using mozilla::MonitorAutoLock;
using mozilla::MonitorAutoUnlock;

template<>
struct RunnableMethodTraits<mozilla::ipc::MessageChannel>
{
    static void RetainCallee(mozilla::ipc::MessageChannel* obj) { }
    static void ReleaseCallee(mozilla::ipc::MessageChannel* obj) { }
};

#define IPC_ASSERT(_cond, ...)                                      \
    do {                                                            \
        if (!(_cond))                                               \
            DebugAbort(__FILE__, __LINE__, #_cond,## __VA_ARGS__);  \
    } while (0)

namespace mozilla {
namespace ipc {

const int32_t MessageChannel::kNoTimeout = INT32_MIN;

// static
bool MessageChannel::sIsPumpingMessages = false;

MessageChannel::MessageChannel(MessageListener *aListener)
  : mListener(aListener->asWeakPtr()),
    mChannelState(ChannelClosed),
    mSide(UnknownSide),
    mLink(nullptr),
    mWorkerLoop(nullptr),
    mChannelErrorTask(nullptr),
    mWorkerLoopID(-1),
    mTimeoutMs(kNoTimeout),
    mInTimeoutSecondHalf(false),
    mNextSeqno(0),
    mPendingSyncReplies(0),
    mPendingUrgentReplies(0),
    mPendingRPCReplies(0),
    mCurrentRPCTransaction(0),
    mDispatchingSyncMessage(false),
    mRemoteStackDepthGuess(false),
    mSawInterruptOutMsg(false)
{
    MOZ_COUNT_CTOR(ipc::MessageChannel);

#ifdef OS_WIN
    mTopFrame = nullptr;
#endif

    mDequeueOneTask = new RefCountedTask(NewRunnableMethod(
                                                 this,
                                                 &MessageChannel::OnMaybeDequeueOne));

#ifdef OS_WIN
    mEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    NS_ASSERTION(mEvent, "CreateEvent failed! Nothing is going to work!");
#endif
}

MessageChannel::~MessageChannel()
{
    MOZ_COUNT_DTOR(ipc::MessageChannel);
    IPC_ASSERT(mCxxStackFrames.empty(), "mismatched CxxStackFrame ctor/dtors");
#ifdef OS_WIN
    DebugOnly<BOOL> ok = CloseHandle(mEvent);
    MOZ_ASSERT(ok);
#endif
}

bool
MessageChannel::Connected() const
{
    mMonitor->AssertCurrentThreadOwns();

    // The transport layer allows us to send messages before
    // receiving the "connected" ack from the remote side.
    return (ChannelOpening == mChannelState || ChannelConnected == mChannelState);
}

void
MessageChannel::Clear()
{
    // Don't clear mWorkerLoopID; we use it in AssertLinkThread() and
    // AssertWorkerThread().
    //
    // Also don't clear mListener.  If we clear it, then sending a message
    // through this channel after it's Clear()'ed can cause this process to
    // crash.
    //
    // In practice, mListener owns the channel, so the channel gets deleted
    // before mListener.  But just to be safe, mListener is a weak pointer.

    mDequeueOneTask->Cancel();

    mWorkerLoop = nullptr;
    delete mLink;
    mLink = nullptr;

    if (mChannelErrorTask) {
        mChannelErrorTask->Cancel();
        mChannelErrorTask = nullptr;
    }
}

bool
MessageChannel::Open(Transport* aTransport, MessageLoop* aIOLoop, Side aSide)
{
    NS_PRECONDITION(!mLink, "Open() called > once");

    mMonitor = new RefCountedMonitor();
    mWorkerLoop = MessageLoop::current();
    mWorkerLoopID = mWorkerLoop->id();

    ProcessLink *link = new ProcessLink(this);
    link->Open(aTransport, aIOLoop, aSide); // :TODO: n.b.: sets mChild
    mLink = link;
    return true;
}

bool
MessageChannel::Open(MessageChannel *aTargetChan, MessageLoop *aTargetLoop, Side aSide)
{
    // Opens a connection to another thread in the same process.

    //  This handshake proceeds as follows:
    //  - Let A be the thread initiating the process (either child or parent)
    //    and B be the other thread.
    //  - A spawns thread for B, obtaining B's message loop
    //  - A creates ProtocolChild and ProtocolParent instances.
    //    Let PA be the one appropriate to A and PB the side for B.
    //  - A invokes PA->Open(PB, ...):
    //    - set state to mChannelOpening
    //    - this will place a work item in B's worker loop (see next bullet)
    //      and then spins until PB->mChannelState becomes mChannelConnected
    //    - meanwhile, on PB's worker loop, the work item is removed and:
    //      - invokes PB->SlaveOpen(PA, ...):
    //        - sets its state and that of PA to Connected
    NS_PRECONDITION(aTargetChan, "Need a target channel");
    NS_PRECONDITION(ChannelClosed == mChannelState, "Not currently closed");

    CommonThreadOpenInit(aTargetChan, aSide);

    Side oppSide = UnknownSide;
    switch(aSide) {
      case ChildSide: oppSide = ParentSide; break;
      case ParentSide: oppSide = ChildSide; break;
      case UnknownSide: break;
    }

    mMonitor = new RefCountedMonitor();

    MonitorAutoLock lock(*mMonitor);
    mChannelState = ChannelOpening;
    aTargetLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(aTargetChan, &MessageChannel::OnOpenAsSlave, this, oppSide));

    while (ChannelOpening == mChannelState)
        mMonitor->Wait();
    NS_ASSERTION(ChannelConnected == mChannelState, "not connected when awoken");
    return (ChannelConnected == mChannelState);
}

void
MessageChannel::OnOpenAsSlave(MessageChannel *aTargetChan, Side aSide)
{
    // Invoked when the other side has begun the open.
    NS_PRECONDITION(ChannelClosed == mChannelState,
                    "Not currently closed");
    NS_PRECONDITION(ChannelOpening == aTargetChan->mChannelState,
                    "Target channel not in the process of opening");

    CommonThreadOpenInit(aTargetChan, aSide);
    mMonitor = aTargetChan->mMonitor;

    MonitorAutoLock lock(*mMonitor);
    NS_ASSERTION(ChannelOpening == aTargetChan->mChannelState,
                 "Target channel not in the process of opening");
    mChannelState = ChannelConnected;
    aTargetChan->mChannelState = ChannelConnected;
    aTargetChan->mMonitor->Notify();
}

void
MessageChannel::CommonThreadOpenInit(MessageChannel *aTargetChan, Side aSide)
{
    mWorkerLoop = MessageLoop::current();
    mWorkerLoopID = mWorkerLoop->id();
    mLink = new ThreadLink(this, aTargetChan);
    mSide = aSide;
}

bool
MessageChannel::Echo(Message* aMsg)
{
    nsAutoPtr<Message> msg(aMsg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    IPC_ASSERT(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    MonitorAutoLock lock(*mMonitor);

    if (!Connected()) {
        ReportConnectionError("MessageChannel");
        return false;
    }

    mLink->EchoMessage(msg.forget());
    return true;
}

bool
MessageChannel::Send(Message* aMsg)
{
    Message copy = *aMsg;
    CxxStackFrame frame(*this, OUT_MESSAGE, &copy);

    nsAutoPtr<Message> msg(aMsg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    IPC_ASSERT(MSG_ROUTING_NONE != msg->routing_id(), "need a route");

    MonitorAutoLock lock(*mMonitor);
    if (!Connected()) {
        ReportConnectionError("MessageChannel");
        return false;
    }
    mLink->SendMessage(msg.forget());
    return true;
}

bool
MessageChannel::MaybeInterceptSpecialIOMessage(const Message& aMsg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MSG_ROUTING_NONE == aMsg.routing_id() &&
        GOODBYE_MESSAGE_TYPE == aMsg.type())
    {
        // :TODO: Sort out Close() on this side racing with Close() on the
        // other side
        mChannelState = ChannelClosing;
        printf("NOTE: %s process received `Goodbye', closing down\n",
               (mSide == ChildSide) ? "child" : "parent");
        return true;
    }
    return false;
}

void
MessageChannel::OnMessageReceivedFromLink(const Message& aMsg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MaybeInterceptSpecialIOMessage(aMsg))
        return;

    // Regardless of the Interrupt stack, if we're awaiting a sync or urgent reply,
    // we know that it needs to be immediately handled to unblock us.
    if ((AwaitingSyncReply() && aMsg.is_sync()) ||
        (AwaitingUrgentReply() && aMsg.is_urgent()) ||
        (AwaitingRPCReply() && aMsg.is_rpc()))
    {
        mRecvd = new Message(aMsg);
        NotifyWorkerThread();
        return;
    }

    // Urgent messages cannot be compressed.
    MOZ_ASSERT(!aMsg.compress() || !aMsg.is_urgent());

    bool compress = (aMsg.compress() && !mPending.empty() &&
                     mPending.back().type() == aMsg.type() &&
                     mPending.back().routing_id() == aMsg.routing_id());
    if (compress) {
        // This message type has compression enabled, and the back of the
        // queue was the same message type and routed to the same destination.
        // Replace it with the newer message.
        MOZ_ASSERT(mPending.back().compress());
        mPending.pop_back();
    }

    bool shouldWakeUp = AwaitingInterruptReply() ||
                        // Allow incoming RPCs to be processed inside an urgent message.
                        (AwaitingUrgentReply() && aMsg.is_rpc()) ||
                        // Always process urgent messages while blocked.
                        ((AwaitingSyncReply() || AwaitingRPCReply()) && aMsg.is_urgent());

    // There are four cases we're concerned about, relating to the state of the
    // main thread:
    //
    // (1) We are waiting on a sync|rpc reply - main thread is blocked on the
    //     IPC monitor.
    //   - If the message is high priority, we wake up the main thread to
    //     deliver the message. Otherwise, we leave it in the mPending queue,
    //     posting a task to the main event loop, where it will be processed
    //     once the synchronous reply has been received.
    //
    // (2) We are waiting on an Interrupt reply - main thread is blocked on the
    //     IPC monitor.
    //   - Always notify and wake up the main thread.
    //
    // (3) We are not waiting on a reply.
    //   - We post a task to the main event loop.
    //
    // Note that, we may notify the main thread even though the monitor is not
    // blocked. This is okay, since we always check for pending events before
    // blocking again.

    if (shouldWakeUp && (AwaitingUrgentReply() && aMsg.is_rpc())) {
        // If we're receiving an RPC message while blocked on an urgent message,
        // we must defer any messages that were not sent as part of the child
        // answering the urgent message.
        //
        // We must also be sure that we will not accidentally defer any RPC
        // message that was sent while answering an urgent message. Otherwise,
        // we will deadlock.
        //
        // On the parent side, the current transaction can only transition from 0
        // to an ID, either by us issuing an urgent request while not blocked, or
        // by receiving an RPC request while not blocked. When we unblock, the
        // current transaction is reset to 0.
        //
        // When the child side receives an urgent message, any RPC messages sent
        // before issuing the urgent reply will carry the urgent message's
        // transaction ID.
        //
        // Since AwaitingUrgentReply() implies we are blocked, it also implies
        // that we are within a transaction that will not change until we are
        // completely unblocked (i.e, the transaction has completed).
        if (aMsg.transaction_id() != mCurrentRPCTransaction)
            shouldWakeUp = false;
    }

    if (aMsg.is_urgent()) {
        MOZ_ASSERT(!mPendingUrgentRequest);
        mPendingUrgentRequest = new Message(aMsg);
    } else if (aMsg.is_rpc() && shouldWakeUp) {
        // Only use this slot if we need to wake up for an RPC call. Otherwise
        // we treat it like a normal async or sync message.
        MOZ_ASSERT(!mPendingRPCCall);
        mPendingRPCCall = new Message(aMsg);
    } else {
        mPending.push_back(aMsg);
    }

    if (shouldWakeUp) {
        // Always wake up Interrupt waiters, sync waiters for urgent messages,
        // RPC waiters for urgent messages, and urgent waiters for RPCs in the
        // same transaction.
        NotifyWorkerThread();
    } else {
        // Worker thread is either not blocked on a reply, or this is an
        // incoming Interrupt that raced with outgoing sync, and needs to be
        // deferred to a later event-loop iteration.
        if (!compress) {
            // If we compressed away the previous message, we'll re-use
            // its pending task.
            mWorkerLoop->PostTask(FROM_HERE, new DequeueTask(mDequeueOneTask));
        }
    }
}

bool
MessageChannel::Send(Message* aMsg, Message* aReply)
{
    // Sanity checks.
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    Message copy = *aMsg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);

    MonitorAutoLock lock(*mMonitor);

    IPC_ASSERT(aMsg->is_sync(), "can only Send() sync messages here");
    IPC_ASSERT(!DispatchingSyncMessage(), "violation of sync handler invariant");
    IPC_ASSERT(!AwaitingSyncReply(), "nested sync messages are not supported");

    AutoEnterPendingReply replies(mPendingSyncReplies);
    if (!SendAndWait(aMsg, aReply))
        return false;

    NS_ABORT_IF_FALSE(aReply->is_sync(), "reply is not sync");
    return true;
}

bool
MessageChannel::UrgentCall(Message* aMsg, Message* aReply)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    IPC_ASSERT(mSide == ParentSide, "cannot send urgent requests from child");

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    Message copy = *aMsg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);

    MonitorAutoLock lock(*mMonitor);

    IPC_ASSERT(!AwaitingInterruptReply(), "urgent calls cannot be issued within Interrupt calls");
    IPC_ASSERT(!AwaitingSyncReply(), "urgent calls cannot be issued within sync sends");

    AutoEnterRPCTransaction transact(this);
    aMsg->set_transaction_id(mCurrentRPCTransaction);

    AutoEnterPendingReply replies(mPendingUrgentReplies);
    if (!SendAndWait(aMsg, aReply))
        return false;

    NS_ABORT_IF_FALSE(aReply->is_urgent(), "reply is not urgent");
    return true;
}

bool
MessageChannel::RPCCall(Message* aMsg, Message* aReply)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    IPC_ASSERT(mSide == ChildSide, "cannot send rpc messages from parent");

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    Message copy = *aMsg;
    CxxStackFrame f(*this, OUT_MESSAGE, &copy);

    MonitorAutoLock lock(*mMonitor);

    // RPC calls must be the only thing on the stack.
    IPC_ASSERT(!AwaitingInterruptReply(), "rpc calls cannot be issued within interrupts");

    AutoEnterRPCTransaction transact(this);
    aMsg->set_transaction_id(mCurrentRPCTransaction);

    AutoEnterPendingReply replies(mPendingRPCReplies);
    if (!SendAndWait(aMsg, aReply))
        return false;

    NS_ABORT_IF_FALSE(aReply->is_rpc(), "expected rpc reply");
    return true;
}

bool
MessageChannel::SendAndWait(Message* aMsg, Message* aReply)
{
    mMonitor->AssertCurrentThreadOwns();

    nsAutoPtr<Message> msg(aMsg);

    if (!Connected()) {
        ReportConnectionError("MessageChannel::SendAndWait");
        return false;
    }

    msg->set_seqno(NextSeqno());

    DebugOnly<int32_t> replySeqno = msg->seqno();
    DebugOnly<msgid_t> replyType = msg->type() + 1;

    mLink->SendMessage(msg.forget());

    while (true) {
        // Wait for an event to occur.
        while (true) {
            if (mRecvd || mPendingUrgentRequest || mPendingRPCCall)
                break;

            bool maybeTimedOut = !WaitForSyncNotify();

            if (!Connected()) {
                ReportConnectionError("MessageChannel::SendAndWait");
                return false;
            }

            if (maybeTimedOut && !ShouldContinueFromTimeout())
                return false;
        }

        if (mPendingUrgentRequest && !ProcessPendingUrgentRequest())
            return false;

        if (mPendingRPCCall && !ProcessPendingRPCCall())
            return false;

        if (mRecvd) {
            NS_ABORT_IF_FALSE(mRecvd->is_reply(), "expected reply");

            if (mRecvd->is_reply_error()) {
                mRecvd = nullptr;
                return false;
            }

            NS_ABORT_IF_FALSE(mRecvd->type() == replyType, "wrong reply type");
            NS_ABORT_IF_FALSE(mRecvd->seqno() == replySeqno, "wrong sequence number");

            *aReply = *mRecvd;
            mRecvd = nullptr;
            return true;
        }
    }

    return true;
}

bool
MessageChannel::Call(Message* aMsg, Message* aReply)
{
    if (aMsg->is_urgent())
        return UrgentCall(aMsg, aReply);
    if (aMsg->is_rpc())
        return RPCCall(aMsg, aReply);
    return InterruptCall(aMsg, aReply);
}

bool
MessageChannel::InterruptCall(Message* aMsg, Message* aReply)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif

    // This must come before MonitorAutoLock, as its destructor acquires the
    // monitor lock.
    Message copy = *aMsg;
    CxxStackFrame cxxframe(*this, OUT_MESSAGE, &copy);

    MonitorAutoLock lock(*mMonitor);
    if (!Connected()) {
        ReportConnectionError("MessageChannel::Call");
        return false;
    }

    // Sanity checks.
    IPC_ASSERT(!AwaitingSyncReply() && !AwaitingUrgentReply(),
               "cannot issue Interrupt call whiel blocked on sync or urgent");
    IPC_ASSERT(!DispatchingSyncMessage() || aMsg->priority() == IPC::Message::PRIORITY_HIGH,
               "violation of sync handler invariant");
    IPC_ASSERT(aMsg->is_interrupt(), "can only Call() Interrupt messages here");


    nsAutoPtr<Message> msg(aMsg);

    msg->set_seqno(NextSeqno());
    msg->set_interrupt_remote_stack_depth_guess(mRemoteStackDepthGuess);
    msg->set_interrupt_local_stack_depth(1 + InterruptStackDepth());
    mInterruptStack.push(*msg);
    mLink->SendMessage(msg.forget());

    while (true) {
        // if a handler invoked by *Dispatch*() spun a nested event
        // loop, and the connection was broken during that loop, we
        // might have already processed the OnError event. if so,
        // trying another loop iteration will be futile because
        // channel state will have been cleared
        if (!Connected()) {
            ReportConnectionError("MessageChannel::InterruptCall");
            return false;
        }

        // Now might be the time to process a message deferred because of race
        // resolution.
        MaybeUndeferIncall();

        // Wait for an event to occur.
        while (!InterruptEventOccurred()) {
            bool maybeTimedOut = !WaitForInterruptNotify();

            // We might have received a "subtly deferred" message in a nested
            // loop that it's now time to process.
            if (InterruptEventOccurred() ||
                (!maybeTimedOut && (!mDeferred.empty() || !mOutOfTurnReplies.empty())))
            {
                break;
            }

            if (maybeTimedOut && !ShouldContinueFromTimeout())
                return false;
        }

        Message recvd;
        MessageMap::iterator it;

        if (mPendingUrgentRequest) {
            recvd = *mPendingUrgentRequest;
            mPendingUrgentRequest = nullptr;
        } else if (mPendingRPCCall) {
            recvd = *mPendingRPCCall;
            mPendingRPCCall = nullptr;
        } else if ((it = mOutOfTurnReplies.find(mInterruptStack.top().seqno()))
                    != mOutOfTurnReplies.end())
        {
            recvd = it->second;
            mOutOfTurnReplies.erase(it);
        } else if (!mPending.empty()) {
            recvd = mPending.front();
            mPending.pop_front();
        } else {
            // because of subtleties with nested event loops, it's possible
            // that we got here and nothing happened.  or, we might have a
            // deferred in-call that needs to be processed.  either way, we
            // won't break the inner while loop again until something new
            // happens.
            continue;
        }

        // If the message is not Interrupt, we can dispatch it as normal.
        if (!recvd.is_interrupt()) {
            // Other side should be blocked.
            IPC_ASSERT(!recvd.is_sync() || mPending.empty(), "other side should be blocked");

            {
                AutoEnterRPCTransaction transaction(this, &recvd);
                MonitorAutoUnlock unlock(*mMonitor);
                CxxStackFrame frame(*this, IN_MESSAGE, &recvd);
                DispatchMessage(recvd);
            }
            if (!Connected()) {
                ReportConnectionError("MessageChannel::DispatchMessage");
                return false;
            }
            continue;
        }

        // If the message is an Interrupt reply, either process it as a reply to our
        // call, or add it to the list of out-of-turn replies we've received.
        if (recvd.is_reply()) {
            IPC_ASSERT(!mInterruptStack.empty(), "invalid Interrupt stack");

            // If this is not a reply the call we've initiated, add it to our
            // out-of-turn replies and keep polling for events.
            {
                const Message &outcall = mInterruptStack.top();

                // Note, In the parent, sequence numbers increase from 0, and
                // in the child, they decrease from 0.
                if ((mSide == ChildSide && recvd.seqno() > outcall.seqno()) ||
                    (mSide != ChildSide && recvd.seqno() < outcall.seqno()))
                {
                    mOutOfTurnReplies[recvd.seqno()] = recvd;
                    continue;
                }

                IPC_ASSERT(recvd.is_reply_error() ||
                           (recvd.type() == (outcall.type() + 1) &&
                            recvd.seqno() == outcall.seqno()),
                           "somebody's misbehavin'", true);
            }

            // We received a reply to our most recent outstanding call. Pop
            // this frame and return the reply.
            mInterruptStack.pop();

            if (!recvd.is_reply_error()) {
                *aReply = recvd;
            }

            // If we have no more pending out calls waiting on replies, then
            // the reply queue should be empty.
            IPC_ASSERT(!mInterruptStack.empty() || mOutOfTurnReplies.empty(),
                       "still have pending replies with no pending out-calls",
                       true);

            return !recvd.is_reply_error();
        }

        // Dispatch an Interrupt in-call. Snapshot the current stack depth while we
        // own the monitor.
        size_t stackDepth = InterruptStackDepth();
        {
            MonitorAutoUnlock unlock(*mMonitor);

            CxxStackFrame frame(*this, IN_MESSAGE, &recvd);
            DispatchInterruptMessage(recvd, stackDepth);
        }
        if (!Connected()) {
            ReportConnectionError("MessageChannel::DispatchInterruptMessage");
            return false;
        }
    }

    return true;
}

bool
MessageChannel::InterruptEventOccurred()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();
    IPC_ASSERT(InterruptStackDepth() > 0, "not in wait loop");

    return (!Connected() ||
            !mPending.empty() ||
            mPendingUrgentRequest ||
            mPendingRPCCall ||
            (!mOutOfTurnReplies.empty() &&
             mOutOfTurnReplies.find(mInterruptStack.top().seqno()) !=
             mOutOfTurnReplies.end()));
}

bool
MessageChannel::ProcessPendingUrgentRequest()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    // Note that it is possible we could have sent a sync message at
    // the same time the parent process sent an urgent message, and
    // therefore mPendingUrgentRequest is set *and* mRecvd is set as
    // well, because the link thread received both before the worker
    // thread woke up.
    //
    // In this case, we process the urgent message first, but we need
    // to save the reply.
    nsAutoPtr<Message> savedReply(mRecvd.forget());

    // We're the child process. We should not be receiving RPC calls.
    IPC_ASSERT(!mPendingRPCCall, "unexpected RPC call");

    nsAutoPtr<Message> recvd(mPendingUrgentRequest.forget());
    {
        // In order to send the parent RPC messages and guarantee it will
        // wake up, we must re-use its transaction.
        AutoEnterRPCTransaction transaction(this, recvd);

        MonitorAutoUnlock unlock(*mMonitor);
        DispatchUrgentMessage(*recvd);
    }
    if (!Connected()) {
        ReportConnectionError("MessageChannel::DispatchUrgentMessage");
        return false;
    }

    // In between having dispatched our reply to the parent process, and
    // re-acquiring the monitor, the parent process could have already
    // processed that reply and sent the reply to our sync message. If so,
    // our saved reply should be empty.
    IPC_ASSERT(!mRecvd || !savedReply, "unknown reply");
    if (!mRecvd)
        mRecvd = savedReply.forget();
    return true;
}

bool
MessageChannel::ProcessPendingRPCCall()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    // See comment above re: mRecvd replies and incoming calls.
    nsAutoPtr<Message> savedReply(mRecvd.forget());

    IPC_ASSERT(!mPendingUrgentRequest, "unexpected urgent message");

    nsAutoPtr<Message> recvd(mPendingRPCCall.forget());
    {
        // If we are not currently in a transaction, this will begin one,
        // and the link thread will not wake us up for any RPC messages not
        // apart of this transaction. If we are already in a transaction,
        // then this will assert that we're still in the same transaction.
        AutoEnterRPCTransaction transaction(this, recvd);

        MonitorAutoUnlock unlock(*mMonitor);
        DispatchRPCMessage(*recvd);
    }
    if (!Connected()) {
        ReportConnectionError("MessageChannel::DispatchRPCMessage");
        return false;
    }

    // In between having dispatched our reply to the parent process, and
    // re-acquiring the monitor, the parent process could have already
    // processed that reply and sent the reply to our sync message. If so,
    // our saved reply should be empty.
    IPC_ASSERT(!mRecvd || !savedReply, "unknown reply");
    if (!mRecvd)
        mRecvd = savedReply.forget();
    return true;
}

bool
MessageChannel::DequeueOne(Message *recvd)
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    if (!Connected()) {
        ReportConnectionError("OnMaybeDequeueOne");
        return false;
    }

    if (mPendingUrgentRequest) {
        *recvd = *mPendingUrgentRequest;
        mPendingUrgentRequest = nullptr;
        return true;
    }

    if (mPendingRPCCall) {
        *recvd = *mPendingRPCCall;
        mPendingRPCCall = nullptr;
        return true;
    }

    if (!mDeferred.empty())
        MaybeUndeferIncall();

    if (mPending.empty())
        return false;

    *recvd = mPending.front();
    mPending.pop_front();
    return true;
}

bool
MessageChannel::OnMaybeDequeueOne()
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    Message recvd;

    MonitorAutoLock lock(*mMonitor);
    if (!DequeueOne(&recvd))
        return false;

    if (IsOnCxxStack() && recvd.is_interrupt() && recvd.is_reply()) {
        // We probably just received a reply in a nested loop for an
        // Interrupt call sent before entering that loop.
        mOutOfTurnReplies[recvd.seqno()] = recvd;
        return false;
    }

    {
        // We should not be in a transaction yet if we're not blocked.
        MOZ_ASSERT(mCurrentRPCTransaction == 0);
        AutoEnterRPCTransaction transaction(this, &recvd);

        MonitorAutoUnlock unlock(*mMonitor);

        CxxStackFrame frame(*this, IN_MESSAGE, &recvd);
        DispatchMessage(recvd);
    }
    return true;
}

void
MessageChannel::DispatchMessage(const Message &aMsg)
{
    if (aMsg.is_sync())
        DispatchSyncMessage(aMsg);
    else if (aMsg.is_urgent())
        DispatchUrgentMessage(aMsg);
    else if (aMsg.is_interrupt())
        DispatchInterruptMessage(aMsg, 0);
    else if (aMsg.is_rpc())
        DispatchRPCMessage(aMsg);
    else
        DispatchAsyncMessage(aMsg);
}

void
MessageChannel::DispatchSyncMessage(const Message& aMsg)
{
    AssertWorkerThread();

    Message *reply = nullptr;

    mDispatchingSyncMessage = true;
    Result rv = mListener->OnMessageReceived(aMsg, reply);
    mDispatchingSyncMessage = false;

    if (!MaybeHandleError(rv, "DispatchSyncMessage")) {
        delete reply;
        reply = new Message();
        reply->set_sync();
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState)
        mLink->SendMessage(reply);
}

void
MessageChannel::DispatchUrgentMessage(const Message& aMsg)
{
    AssertWorkerThread();
    MOZ_ASSERT(aMsg.is_urgent());

    Message *reply = nullptr;

    if (!MaybeHandleError(mListener->OnCallReceived(aMsg, reply), "DispatchUrgentMessage")) {
        delete reply;
        reply = new Message();
        reply->set_urgent();
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState)
        mLink->SendMessage(reply);
}

void
MessageChannel::DispatchRPCMessage(const Message& aMsg)
{
    AssertWorkerThread();
    MOZ_ASSERT(aMsg.is_rpc());

    Message *reply = nullptr;

    if (!MaybeHandleError(mListener->OnCallReceived(aMsg, reply), "DispatchRPCMessage")) {
        delete reply;
        reply = new Message();
        reply->set_rpc();
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());
    
    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState)
        mLink->SendMessage(reply);
}

void
MessageChannel::DispatchAsyncMessage(const Message& aMsg)
{
    AssertWorkerThread();
    MOZ_ASSERT(!aMsg.is_interrupt() && !aMsg.is_sync() && !aMsg.is_urgent());

    if (aMsg.routing_id() == MSG_ROUTING_NONE) {
        NS_RUNTIMEABORT("unhandled special message!");
    }

    MaybeHandleError(mListener->OnMessageReceived(aMsg), "DispatchAsyncMessage");
}

void
MessageChannel::DispatchInterruptMessage(const Message& aMsg, size_t stackDepth)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    IPC_ASSERT(aMsg.is_interrupt() && !aMsg.is_reply(), "wrong message type");

    // Race detection: see the long comment near mRemoteStackDepthGuess in
    // MessageChannel.h. "Remote" stack depth means our side, and "local" means
    // the other side.
    if (aMsg.interrupt_remote_stack_depth_guess() != RemoteViewOfStackDepth(stackDepth)) {
        // Interrupt in-calls have raced. The winner, if there is one, gets to defer
        // processing of the other side's in-call.
        bool defer;
        const char* winner;
        switch (mListener->MediateInterruptRace((mSide == ChildSide) ? aMsg : mInterruptStack.top(),
                                          (mSide != ChildSide) ? mInterruptStack.top() : aMsg))
        {
          case RIPChildWins:
            winner = "child";
            defer = (mSide == ChildSide);
            break;
          case RIPParentWins:
            winner = "parent";
            defer = (mSide != ChildSide);
            break;
          case RIPError:
            NS_RUNTIMEABORT("NYI: 'Error' Interrupt race policy");
            return;
          default:
            NS_RUNTIMEABORT("not reached");
            return;
        }

        if (LoggingEnabled()) {
            printf_stderr("  (%s: %s won, so we're%sdeferring)\n",
                          (mSide == ChildSide) ? "child" : "parent",
                          winner,
                          defer ? " " : " not ");
        }

        if (defer) {
            // We now know the other side's stack has one more frame
            // than we thought.
            ++mRemoteStackDepthGuess; // decremented in MaybeProcessDeferred()
            mDeferred.push(aMsg);
            return;
        }

        // We "lost" and need to process the other side's in-call. Don't need
        // to fix up the mRemoteStackDepthGuess here, because we're just about
        // to increment it in DispatchCall(), which will make it correct again.
    }

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif

    Message* reply = nullptr;

    ++mRemoteStackDepthGuess;
    Result rv = mListener->OnCallReceived(aMsg, reply);
    --mRemoteStackDepthGuess;

    if (!MaybeHandleError(rv, "DispatchInterruptMessage")) {
        delete reply;
        reply = new Message();
        reply->set_interrupt();
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState)
        mLink->SendMessage(reply);
}

void
MessageChannel::MaybeUndeferIncall()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    if (mDeferred.empty())
        return;

    size_t stackDepth = InterruptStackDepth();

    // the other side can only *under*-estimate our actual stack depth
    IPC_ASSERT(mDeferred.top().interrupt_remote_stack_depth_guess() <= stackDepth,
               "fatal logic error");

    if (mDeferred.top().interrupt_remote_stack_depth_guess() < RemoteViewOfStackDepth(stackDepth))
        return;

    // maybe time to process this message
    Message call = mDeferred.top();
    mDeferred.pop();

    // fix up fudge factor we added to account for race
    IPC_ASSERT(0 < mRemoteStackDepthGuess, "fatal logic error");
    --mRemoteStackDepthGuess;

    mPending.push_back(call);
}

void
MessageChannel::FlushPendingInterruptQueue()
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    {
        MonitorAutoLock lock(*mMonitor);

        if (mDeferred.empty()) {
            if (mPending.empty())
                return;

            const Message& last = mPending.back();
            if (!last.is_interrupt() || last.is_reply())
                return;
        }
    }

    while (OnMaybeDequeueOne());
}

void
MessageChannel::ExitedCxxStack()
{
    mListener->OnExitedCxxStack();
    if (mSawInterruptOutMsg) {
        MonitorAutoLock lock(*mMonitor);
        // see long comment in OnMaybeDequeueOne()
        EnqueuePendingMessages();
        mSawInterruptOutMsg = false;
    }
}

void
MessageChannel::EnqueuePendingMessages()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    MaybeUndeferIncall();

    for (size_t i = 0; i < mDeferred.size(); ++i) {
        mWorkerLoop->PostTask(FROM_HERE, new DequeueTask(mDequeueOneTask));
    }

    // XXX performance tuning knob: could process all or k pending
    // messages here, rather than enqueuing for later processing

    for (size_t i = 0; i < mPending.size(); ++i) {
        mWorkerLoop->PostTask(FROM_HERE, new DequeueTask(mDequeueOneTask));
    }
}

static inline bool
IsTimeoutExpired(PRIntervalTime aStart, PRIntervalTime aTimeout)
{
    return (aTimeout != PR_INTERVAL_NO_TIMEOUT) &&
           (aTimeout <= (PR_IntervalNow() - aStart));
}

bool
MessageChannel::WaitResponse(bool aWaitTimedOut)
{
    if (aWaitTimedOut) {
        if (mInTimeoutSecondHalf) {
            // We've really timed out this time.
            return false;
        }
        // Try a second time.
        mInTimeoutSecondHalf = true;
    } else {
        mInTimeoutSecondHalf = false;
    }
    return true;
}

#ifndef OS_WIN
bool
MessageChannel::WaitForSyncNotify()
{
    PRIntervalTime timeout = (kNoTimeout == mTimeoutMs) ?
                             PR_INTERVAL_NO_TIMEOUT :
                             PR_MillisecondsToInterval(mTimeoutMs);
    // XXX could optimize away this syscall for "no timeout" case if desired
    PRIntervalTime waitStart = PR_IntervalNow();

    mMonitor->Wait(timeout);

    // If the timeout didn't expire, we know we received an event. The
    // converse is not true.
    return WaitResponse(IsTimeoutExpired(waitStart, timeout));
}

bool
MessageChannel::WaitForInterruptNotify()
{
    return WaitForSyncNotify();
}

void
MessageChannel::NotifyWorkerThread()
{
    mMonitor->Notify();
}
#endif

bool
MessageChannel::ShouldContinueFromTimeout()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    bool cont;
    {
        MonitorAutoUnlock unlock(*mMonitor);
        cont = mListener->OnReplyTimeout();
    }

    static enum { UNKNOWN, NOT_DEBUGGING, DEBUGGING } sDebuggingChildren = UNKNOWN;

    if (sDebuggingChildren == UNKNOWN) {
        sDebuggingChildren = getenv("MOZ_DEBUG_CHILD_PROCESS") ? DEBUGGING : NOT_DEBUGGING;
    }
    if (sDebuggingChildren == DEBUGGING) {
        return true;
    }

    if (!cont) {
        // NB: there's a sublety here.  If parents were allowed to send sync
        // messages to children, then it would be possible for this
        // synchronous close-on-timeout to race with async |OnMessageReceived|
        // tasks arriving from the child, posted to the worker thread's event
        // loop.  This would complicate cleanup of the *Channel.  But since
        // IPDL forbids this (and since it doesn't support children timing out
        // on parents), the parent can only block on interrupt messages to the child,
        // and in that case arriving async messages are enqueued to the interrupt 
        // channel's special queue.  They're then ignored because the channel
        // state changes to ChannelTimeout (i.e. !Connected).
        SynchronouslyClose();
        mChannelState = ChannelTimeout;
    }

    return cont;
}

void
MessageChannel::SetReplyTimeoutMs(int32_t aTimeoutMs)
{
    // Set channel timeout value. Since this is broken up into
    // two period, the minimum timeout value is 2ms.
    AssertWorkerThread();
    mTimeoutMs = (aTimeoutMs <= 0)
                 ? kNoTimeout
                 : (int32_t)ceil((double)aTimeoutMs / 2.0);
}

void
MessageChannel::OnChannelConnected(int32_t peer_id)
{
    mWorkerLoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &MessageChannel::DispatchOnChannelConnected,
                          peer_id));
}

void
MessageChannel::DispatchOnChannelConnected(int32_t peer_pid)
{
    AssertWorkerThread();
    if (mListener)
        mListener->OnChannelConnected(peer_pid);
}


static void
PrintErrorMessage(Side side, const char* channelName, const char* msg)
{
    const char *from = (side == ChildSide)
                       ? "Child"
                       : ((side == ParentSide) ? "Parent" : "Unknown");
    printf_stderr("\n###!!! [%s][%s] Error: %s\n\n", from, channelName, msg);
}

void
MessageChannel::ReportConnectionError(const char* aChannelName) const
{
    const char* errorMsg = nullptr;
    switch (mChannelState) {
      case ChannelClosed:
        errorMsg = "Closed channel: cannot send/recv";
        break;
      case ChannelOpening:
        errorMsg = "Opening channel: not yet ready for send/recv";
        break;
      case ChannelTimeout:
        errorMsg = "Channel timeout: cannot send/recv";
        break;
      case ChannelClosing:
        errorMsg = "Channel closing: too late to send/recv, messages will be lost";
        break;
      case ChannelError:
        errorMsg = "Channel error: cannot send/recv";
        break;

      default:
        NS_RUNTIMEABORT("unreached");
    }

    PrintErrorMessage(mSide, aChannelName, errorMsg);
    mListener->OnProcessingError(MsgDropped);
}

bool
MessageChannel::MaybeHandleError(Result code, const char* channelName)
{
    if (MsgProcessed == code)
        return true;

    const char* errorMsg = nullptr;
    switch (code) {
      case MsgNotKnown:
        errorMsg = "Unknown message: not processed";
        break;
      case MsgNotAllowed:
        errorMsg = "Message not allowed: cannot be sent/recvd in this state";
        break;
      case MsgPayloadError:
        errorMsg = "Payload error: message could not be deserialized";
        break;
      case MsgProcessingError:
        errorMsg = "Processing error: message was deserialized, but the handler returned false (indicating failure)";
        break;
      case MsgRouteError:
        errorMsg = "Route error: message sent to unknown actor ID";
        break;
      case MsgValueError:
        errorMsg = "Value error: message was deserialized, but contained an illegal value";
        break;

    default:
        NS_RUNTIMEABORT("unknown Result code");
        return false;
    }

    PrintErrorMessage(mSide, channelName, errorMsg);

    mListener->OnProcessingError(code);

    return false;
}

void
MessageChannel::OnChannelErrorFromLink()
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (InterruptStackDepth() > 0)
        NotifyWorkerThread();

    if (AwaitingSyncReply() || AwaitingRPCReply() || AwaitingUrgentReply())
        NotifyWorkerThread();

    if (ChannelClosing != mChannelState) {
        mChannelState = ChannelError;
        mMonitor->Notify();
    }

    PostErrorNotifyTask();
}

void
MessageChannel::NotifyMaybeChannelError()
{
    mMonitor->AssertNotCurrentThreadOwns();

    // TODO sort out Close() on this side racing with Close() on the other side
    if (ChannelClosing == mChannelState) {
        // the channel closed, but we received a "Goodbye" message warning us
        // about it. no worries
        mChannelState = ChannelClosed;
        NotifyChannelClosed();
        return;
    }

    // Oops, error!  Let the listener know about it.
    mChannelState = ChannelError;
    mListener->OnChannelError();
    Clear();
}

void
MessageChannel::OnNotifyMaybeChannelError()
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    mChannelErrorTask = nullptr;

    // OnChannelError holds mMonitor when it posts this task and this
    // task cannot be allowed to run until OnChannelError has
    // exited. We enforce that order by grabbing the mutex here which
    // should only continue once OnChannelError has completed.
    {
        MonitorAutoLock lock(*mMonitor);
        // nothing to do here
    }

    if (IsOnCxxStack()) {
        mChannelErrorTask =
            NewRunnableMethod(this, &MessageChannel::OnNotifyMaybeChannelError);
        // 10 ms delay is completely arbitrary
        mWorkerLoop->PostDelayedTask(FROM_HERE, mChannelErrorTask, 10);
        return;
    }

    NotifyMaybeChannelError();
}

void
MessageChannel::PostErrorNotifyTask()
{
    mMonitor->AssertCurrentThreadOwns();

    if (mChannelErrorTask)
        return;

    // This must be the last code that runs on this thread!
    mChannelErrorTask =
        NewRunnableMethod(this, &MessageChannel::OnNotifyMaybeChannelError);
    mWorkerLoop->PostTask(FROM_HERE, mChannelErrorTask);
}

// Special async message.
class GoodbyeMessage : public IPC::Message
{
public:
    GoodbyeMessage() :
        IPC::Message(MSG_ROUTING_NONE, GOODBYE_MESSAGE_TYPE, PRIORITY_NORMAL)
    {
    }
    static bool Read(const Message* msg) {
        return true;
    }
    void Log(const std::string& aPrefix, FILE* aOutf) const {
        fputs("(special `Goodbye' message)", aOutf);
    }
};

void
MessageChannel::SynchronouslyClose()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();
    mLink->SendClose();
    while (ChannelClosed != mChannelState)
        mMonitor->Wait();
}

void
MessageChannel::CloseWithError()
{
    AssertWorkerThread();

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected != mChannelState) {
        return;
    }
    SynchronouslyClose();
    mChannelState = ChannelError;
    PostErrorNotifyTask();
}

void
MessageChannel::Close()
{
    AssertWorkerThread();

    {
        MonitorAutoLock lock(*mMonitor);

        if (ChannelError == mChannelState || ChannelTimeout == mChannelState) {
            // See bug 538586: if the listener gets deleted while the
            // IO thread's NotifyChannelError event is still enqueued
            // and subsequently deletes us, then the error event will
            // also be deleted and the listener will never be notified
            // of the channel error.
            if (mListener) {
                MonitorAutoUnlock unlock(*mMonitor);
                NotifyMaybeChannelError();
            }
            return;
        }

        if (ChannelConnected != mChannelState) {
            // XXX be strict about this until there's a compelling reason
            // to relax
            NS_RUNTIMEABORT("Close() called on closed channel!");
        }

        AssertWorkerThread();

        // notify the other side that we're about to close our socket
        mLink->SendMessage(new GoodbyeMessage());
        SynchronouslyClose();
    }

    NotifyChannelClosed();
}

void
MessageChannel::NotifyChannelClosed()
{
    mMonitor->AssertNotCurrentThreadOwns();

    if (ChannelClosed != mChannelState)
        NS_RUNTIMEABORT("channel should have been closed!");

    // OK, the IO thread just closed the channel normally.  Let the
    // listener know about it.
    mListener->OnChannelClose();

    Clear();
}

void
MessageChannel::DebugAbort(const char* file, int line, const char* cond,
                           const char* why,
                           bool reply) const
{
    printf_stderr("###!!! [MessageChannel][%s][%s:%d] "
                  "Assertion (%s) failed.  %s %s\n",
                  mSide == ChildSide ? "Child" : "Parent",
                  file, line, cond,
                  why,
                  reply ? "(reply)" : "");
    // technically we need the mutex for this, but we're dying anyway
    DumpInterruptStack("  ");
    printf_stderr("  remote Interrupt stack guess: %lu\n",
                  mRemoteStackDepthGuess);
    printf_stderr("  deferred stack size: %lu\n",
                  mDeferred.size());
    printf_stderr("  out-of-turn Interrupt replies stack size: %lu\n",
                  mOutOfTurnReplies.size());
    printf_stderr("  Pending queue size: %lu, front to back:\n",
                  mPending.size());

    MessageQueue pending = mPending;
    while (!pending.empty()) {
        printf_stderr("    [ %s%s ]\n",
                      pending.front().is_interrupt() ? "intr" :
                      (pending.front().is_sync() ? "sync" : "async"),
                      pending.front().is_reply() ? "reply" : "");
        pending.pop_front();
    }

    NS_RUNTIMEABORT(why);
}

void
MessageChannel::DumpInterruptStack(const char* const pfx) const
{
    NS_WARN_IF_FALSE(MessageLoop::current() != mWorkerLoop,
                     "The worker thread had better be paused in a debugger!");

    printf_stderr("%sMessageChannel 'backtrace':\n", pfx);

    // print a python-style backtrace, first frame to last
    for (uint32_t i = 0; i < mCxxStackFrames.size(); ++i) {
        int32_t id;
        const char* dir, *sems, *name;
        mCxxStackFrames[i].Describe(&id, &dir, &sems, &name);

        printf_stderr("%s[(%u) %s %s %s(actor=%d) ]\n", pfx,
                      i, dir, sems, name, id);
    }
}

} // ipc
} // mozilla

