/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"

#include "mozilla/dom/ScriptSettings.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsContentUtils.h"

#include "prprf.h"

// Undo the damage done by mozzconf.h
#undef compress

/*
 * IPC design:
 *
 * There are three kinds of messages: async, sync, and intr. Sync and intr
 * messages are blocking. Only intr and high-priority sync messages can nest.
 *
 * Terminology: To dispatch a message Foo is to run the RecvFoo code for
 * it. This is also called "handling" the message.
 *
 * Sync messages have priorities while async and intr messages always have
 * normal priority. The three possible priorities are normal, high, and urgent.
 * The intended uses of these priorities are:
 *   NORMAL - most messages.
 *   HIGH   - CPOW-related messages, which can go in either direction.
 *   URGENT - messages where we don't want to dispatch
 *            incoming CPOWs while waiting for the response.
 *
 * To avoid jank, the parent process is not allowed to send sync messages of
 * normal priority. The parent also is not allowed to send urgent messages at
 * all.  When a process is waiting for a response to a sync message M0, it will
 * dispatch an incoming message M if:
 *   1. M has a higher priority than M0, or
 *   2. if M has the same priority as M0 and we're in the child, or
 *   3. if M has the same priority as M0 and it was sent by the other side
        while dispatching M0 (nesting).
 * The idea is that higher priority messages should take precendence, and we
 * also want to allow nesting. The purpose of rule 2 is to handle a race where
 * both processes send to each other simultaneously. In this case, we resolve
 * the race in favor of the parent (so the child dispatches first).
 *
 * Sync messages satisfy the following properties:
 *   A. When waiting for a response to a sync message, we won't dispatch any
 *      messages of lower priority.
 *   B. Sync messages of the same priority will be dispatched roughly in the
 *      order they were sent. The exception is when the parent and child send
 *      sync messages to each other simulataneously. In this case, the parent's
 *      message is dispatched first. While it is dispatched, the child may send
 *      further nested messages, and these messages may be dispatched before the
 *      child's original message. We can consider ordering to be preserved here
 *      because we pretend that the child's original message wasn't sent until
 *      after the parent's message is finished being dispatched.
 *
 * Intr messages are blocking but not prioritized. While waiting for an intr
 * response, all incoming messages are dispatched until a response is
 * received. Intr messages also can be nested. When two intr messages race with
 * each other, a similar scheme is used to ensure that one side wins. The
 * winning side is chosen based on the message type.
 *
 * Intr messages differ from sync messages in that, while sending an intr
 * message, we may dispatch an async message. This causes some additional
 * complexity. One issue is that replies can be received out of order. It's also
 * more difficult to determine whether one message is nested inside
 * another. Consequently, intr handling uses mOutOfTurnReplies and
 * mRemoteStackDepthGuess, which are not needed for sync messages.
 */

using namespace mozilla;
using namespace std;

using mozilla::dom::AutoNoJSAPI;
using mozilla::dom::ScriptSettingsInitialized;
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

static bool gParentIsBlocked;

namespace mozilla {
namespace ipc {

const int32_t MessageChannel::kNoTimeout = INT32_MIN;

// static
bool MessageChannel::sIsPumpingMessages = false;

enum Direction
{
    IN_MESSAGE,
    OUT_MESSAGE
};


class MessageChannel::InterruptFrame
{
private:
    enum Semantics
    {
        INTR_SEMS,
        SYNC_SEMS,
        ASYNC_SEMS
    };

public:
    InterruptFrame(Direction direction, const Message* msg)
      : mMessageName(strdup(msg->name())),
        mMessageRoutingId(msg->routing_id()),
        mMesageSemantics(msg->is_interrupt() ? INTR_SEMS :
                          msg->is_sync() ? SYNC_SEMS :
                          ASYNC_SEMS),
        mDirection(direction),
        mMoved(false)
    {
        MOZ_ASSERT(mMessageName);
    }

    InterruptFrame(InterruptFrame&& aOther)
    {
        MOZ_ASSERT(aOther.mMessageName);
        mMessageName = aOther.mMessageName;
        aOther.mMessageName = nullptr;
        aOther.mMoved = true;

        mMessageRoutingId = aOther.mMessageRoutingId;
        mMesageSemantics = aOther.mMesageSemantics;
        mDirection = aOther.mDirection;
    }

    ~InterruptFrame()
    {
        MOZ_ASSERT_IF(!mMessageName, mMoved);

        if (mMessageName)
            free(const_cast<char*>(mMessageName));
    }

    InterruptFrame& operator=(InterruptFrame&& aOther)
    {
        MOZ_ASSERT(&aOther != this);
        this->~InterruptFrame();
        new (this) InterruptFrame(mozilla::Move(aOther));
        return *this;
    }

    bool IsInterruptIncall() const
    {
        return INTR_SEMS == mMesageSemantics && IN_MESSAGE == mDirection;
    }

    bool IsInterruptOutcall() const
    {
        return INTR_SEMS == mMesageSemantics && OUT_MESSAGE == mDirection;
    }

    void Describe(int32_t* id, const char** dir, const char** sems,
                  const char** name) const
    {
        *id = mMessageRoutingId;
        *dir = (IN_MESSAGE == mDirection) ? "in" : "out";
        *sems = (INTR_SEMS == mMesageSemantics) ? "intr" :
                (SYNC_SEMS == mMesageSemantics) ? "sync" :
                "async";
        *name = mMessageName;
    }

private:
    const char* mMessageName;
    int32_t mMessageRoutingId;
    Semantics mMesageSemantics;
    Direction mDirection;
    DebugOnly<bool> mMoved;

    // Disable harmful methods.
    InterruptFrame(const InterruptFrame& aOther) MOZ_DELETE;
    InterruptFrame& operator=(const InterruptFrame&) MOZ_DELETE;
};

class MOZ_STACK_CLASS MessageChannel::CxxStackFrame
{
public:
    CxxStackFrame(MessageChannel& that, Direction direction, const Message* msg)
      : mThat(that)
    {
        mThat.AssertWorkerThread();

        if (mThat.mCxxStackFrames.empty())
            mThat.EnteredCxxStack();

        mThat.mCxxStackFrames.append(InterruptFrame(direction, msg));

        const InterruptFrame& frame = mThat.mCxxStackFrames.back();

        if (frame.IsInterruptIncall())
            mThat.EnteredCall();

        mThat.mSawInterruptOutMsg |= frame.IsInterruptOutcall();
    }

    ~CxxStackFrame() {
        mThat.AssertWorkerThread();

        MOZ_ASSERT(!mThat.mCxxStackFrames.empty());

        bool exitingCall = mThat.mCxxStackFrames.back().IsInterruptIncall();
        mThat.mCxxStackFrames.shrinkBy(1);

        bool exitingStack = mThat.mCxxStackFrames.empty();

        // mListener could have gone away if Close() was called while
        // MessageChannel code was still on the stack
        if (!mThat.mListener)
            return;

        if (exitingCall)
            mThat.ExitedCall();

        if (exitingStack)
            mThat.ExitedCxxStack();
    }
private:
    MessageChannel& mThat;

    // Disable harmful methods.
    CxxStackFrame() MOZ_DELETE;
    CxxStackFrame(const CxxStackFrame&) MOZ_DELETE;
    CxxStackFrame& operator=(const CxxStackFrame&) MOZ_DELETE;
};

namespace {

class MOZ_STACK_CLASS MaybeScriptBlocker {
public:
    explicit MaybeScriptBlocker(MessageChannel *aChannel, bool aBlock
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mBlocked(aChannel->ShouldBlockScripts() && aBlock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (mBlocked) {
            nsContentUtils::AddScriptBlocker();
        }
    }
    ~MaybeScriptBlocker() {
        if (mBlocked) {
            nsContentUtils::RemoveScriptBlocker();
        }
    }
private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    bool mBlocked;
};

} /* namespace {} */

MessageChannel::MessageChannel(MessageListener *aListener)
  : mListener(aListener),
    mChannelState(ChannelClosed),
    mSide(UnknownSide),
    mLink(nullptr),
    mWorkerLoop(nullptr),
    mChannelErrorTask(nullptr),
    mWorkerLoopID(-1),
    mTimeoutMs(kNoTimeout),
    mInTimeoutSecondHalf(false),
    mNextSeqno(0),
    mAwaitingSyncReply(false),
    mAwaitingSyncReplyPriority(0),
    mDispatchingSyncMessage(false),
    mDispatchingSyncMessagePriority(0),
    mCurrentTransaction(0),
    mTimedOutMessageSeqno(0),
    mRecvdErrors(0),
    mRemoteStackDepthGuess(false),
    mSawInterruptOutMsg(false),
    mAbortOnError(false),
    mBlockScripts(false),
    mFlags(REQUIRE_DEFAULT),
    mPeerPidSet(false),
    mPeerPid(-1)
{
    MOZ_COUNT_CTOR(ipc::MessageChannel);

#ifdef OS_WIN
    mTopFrame = nullptr;
    mIsSyncWaitingOnNonMainThread = false;
#endif

    mDequeueOneTask = new RefCountedTask(NewRunnableMethod(
                                                 this,
                                                 &MessageChannel::OnMaybeDequeueOne));

    mOnChannelConnectedTask = new RefCountedTask(NewRunnableMethod(
        this,
        &MessageChannel::DispatchOnChannelConnected));

#ifdef OS_WIN
    mEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
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
    Clear();
}

static void
PrintErrorMessage(Side side, const char* channelName, const char* msg)
{
    const char *from = (side == ChildSide)
                       ? "Child"
                       : ((side == ParentSide) ? "Parent" : "Unknown");
    printf_stderr("\n###!!! [%s][%s] Error: %s\n\n", from, channelName, msg);
}

bool
MessageChannel::Connected() const
{
    mMonitor->AssertCurrentThreadOwns();

    // The transport layer allows us to send messages before
    // receiving the "connected" ack from the remote side.
    return (ChannelOpening == mChannelState || ChannelConnected == mChannelState);
}

bool
MessageChannel::CanSend() const
{
    MonitorAutoLock lock(*mMonitor);
    return Connected();
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

    mOnChannelConnectedTask->Cancel();

    if (mChannelErrorTask) {
        mChannelErrorTask->Cancel();
        mChannelErrorTask = nullptr;
    }

    // Free up any memory used by pending messages.
    mPending.clear();
    mRecvd = nullptr;
    mOutOfTurnReplies.clear();
    while (!mDeferred.empty()) {
        mDeferred.pop();
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
    if (MSG_ROUTING_NONE == msg->routing_id()) {
        ReportMessageRouteError("MessageChannel::Echo");
        return false;
    }

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
    CxxStackFrame frame(*this, OUT_MESSAGE, aMsg);

    nsAutoPtr<Message> msg(aMsg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    if (MSG_ROUTING_NONE == msg->routing_id()) {
        ReportMessageRouteError("MessageChannel::Send");
        return false;
    }

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
        if (LoggingEnabled()) {
            printf("NOTE: %s process received `Goodbye', closing down\n",
                   (mSide == ChildSide) ? "child" : "parent");
        }
        return true;
    }
    return false;
}

bool
MessageChannel::ShouldDeferMessage(const Message& aMsg)
{
    // Never defer messages that have the highest priority, even async
    // ones. This is safe because only the child can send these messages, so
    // they can never nest.
    if (aMsg.priority() == IPC::Message::PRIORITY_URGENT) {
        MOZ_ASSERT(mSide == ParentSide);
        return false;
    }

    // Unless they're urgent, we always defer async messages.
    if (!aMsg.is_sync()) {
        MOZ_ASSERT(aMsg.priority() == IPC::Message::PRIORITY_NORMAL);
        return true;
    }

    int msgPrio = aMsg.priority();
    int waitingPrio = AwaitingSyncReplyPriority();

    // Always defer if the priority of the incoming message is less than the
    // priority of the message we're awaiting.
    if (msgPrio < waitingPrio)
        return true;

    // Never defer if the message has strictly greater priority.
    if (msgPrio > waitingPrio)
        return false;

    // When both sides send sync messages of the same priority, we resolve the
    // race by dispatching in the child and deferring the incoming message in
    // the parent. However, the parent still needs to dispatch nested sync
    // messages.
    //
    // Deferring in the parent only sort of breaks message ordering. When the
    // child's message comes in, we can pretend the child hasn't quite
    // finished sending it yet. Since the message is sync, we know that the
    // child hasn't moved on yet.
    return mSide == ParentSide && aMsg.transaction_id() != mCurrentTransaction;
}

// Predicate that is true for messages that should be consolidated if 'compress' is set.
class MatchingKinds {
    typedef IPC::Message Message;
    Message::msgid_t mType;
    int32_t mRoutingId;
public:
    MatchingKinds(Message::msgid_t aType, int32_t aRoutingId) :
        mType(aType), mRoutingId(aRoutingId) {}
    bool operator()(const Message &msg) {
        return msg.type() == mType && msg.routing_id() == mRoutingId;
    }
};

void
MessageChannel::OnMessageReceivedFromLink(const Message& aMsg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MaybeInterceptSpecialIOMessage(aMsg))
        return;

    // Regardless of the Interrupt stack, if we're awaiting a sync reply,
    // we know that it needs to be immediately handled to unblock us.
    if (aMsg.is_sync() && aMsg.is_reply()) {
        if (aMsg.seqno() == mTimedOutMessageSeqno) {
            // Drop the message, but allow future sync messages to be sent.
            mTimedOutMessageSeqno = 0;
            return;
        }

        MOZ_ASSERT(AwaitingSyncReply());
        MOZ_ASSERT(!mRecvd);

        // Rather than storing errors in mRecvd, we mark them in
        // mRecvdErrors. We need a counter because multiple replies can arrive
        // when a timeout happens, as in the following example. Imagine the
        // child is running slowly. The parent sends a sync message P1. It times
        // out. The child eventually sends a sync message C1. While waiting for
        // the C1 response, the child dispatches P1. In doing so, it sends sync
        // message C2. At that point, it's valid for the parent to send error
        // responses for both C1 and C2.
        if (aMsg.is_reply_error()) {
            mRecvdErrors++;
            NotifyWorkerThread();
            return;
        }

        mRecvd = new Message(aMsg);
        NotifyWorkerThread();
        return;
    }

    // Prioritized messages cannot be compressed.
    MOZ_ASSERT(!aMsg.compress() || aMsg.priority() == IPC::Message::PRIORITY_NORMAL);

    bool compress = (aMsg.compress() && !mPending.empty());
    if (compress) {
        // Check the message queue for another message with this type/destination.
        auto it = std::find_if(mPending.rbegin(), mPending.rend(),
                               MatchingKinds(aMsg.type(), aMsg.routing_id()));
        if (it != mPending.rend()) {
            // This message type has compression enabled, and the queue holds
            // a message with the same message type and routed to the same destination.
            // Erase it.  Note that, since we always compress these redundancies, There Can
            // Be Only One.
            MOZ_ASSERT((*it).compress());
            mPending.erase((++it).base());
        } else {
            // No other messages with the same type/destination exist.
            compress = false;
        }
    }

    bool shouldWakeUp = AwaitingInterruptReply() ||
                        (AwaitingSyncReply() && !ShouldDeferMessage(aMsg));

    // There are three cases we're concerned about, relating to the state of the
    // main thread:
    //
    // (1) We are waiting on a sync reply - main thread is blocked on the
    //     IPC monitor.
    //   - If the message is high priority, we wake up the main thread to
    //     deliver the message depending on ShouldDeferMessage. Otherwise, we
    //     leave it in the mPending queue, posting a task to the main event
    //     loop, where it will be processed once the synchronous reply has been
    //     received.
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

    mPending.push_back(aMsg);

    if (shouldWakeUp) {
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
    // See comment in DispatchSyncMessage.
    MaybeScriptBlocker scriptBlocker(this, true);

    // Sanity checks.
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
#endif

    CxxStackFrame f(*this, OUT_MESSAGE, aMsg);

    MonitorAutoLock lock(*mMonitor);

    if (mTimedOutMessageSeqno) {
        // Don't bother sending another sync message if a previous one timed out
        // and we haven't received a reply for it. Once the original timed-out
        // message receives a reply, we'll be able to send more sync messages
        // again.
        return false;
    }

    IPC_ASSERT(aMsg->is_sync(), "can only Send() sync messages here");
    IPC_ASSERT(aMsg->priority() >= DispatchingSyncMessagePriority(),
               "can't send sync message of a lesser priority than what's being dispatched");
    IPC_ASSERT(mAwaitingSyncReplyPriority <= aMsg->priority(),
               "nested sync message sends must be of increasing priority");

    nsAutoPtr<Message> msg(aMsg);

    if (!Connected()) {
        ReportConnectionError("MessageChannel::SendAndWait");
        return false;
    }

    msg->set_seqno(NextSeqno());

    int32_t seqno = msg->seqno();
    DebugOnly<msgid_t> replyType = msg->type() + 1;

    AutoSetValue<bool> replies(mAwaitingSyncReply, true);
    AutoSetValue<int> prio(mAwaitingSyncReplyPriority, msg->priority());
    AutoEnterTransaction transact(this, seqno);

    int32_t transaction = mCurrentTransaction;
    msg->set_transaction_id(transaction);

    mLink->SendMessage(msg.forget());

    while (true) {
        // Loop until there aren't any more priority messages to process.
        for (;;) {
            mozilla::Vector<Message> toProcess;

            for (MessageQueue::iterator it = mPending.begin(); it != mPending.end(); ) {
                Message &msg = *it;
                if (!ShouldDeferMessage(msg)) {
                    toProcess.append(Move(msg));
                    it = mPending.erase(it);
                    continue;
                }
                it++;
            }

            if (toProcess.empty())
                break;

            // Processing these messages could result in more messages, so we
            // loop around to check for more afterwards.
            for (auto it = toProcess.begin(); it != toProcess.end(); it++)
                ProcessPendingRequest(*it);
        }

        // See if we've received a reply.
        if (mRecvdErrors) {
            mRecvdErrors--;
            return false;
        }

        if (mRecvd) {
            MOZ_ASSERT(mRecvd->is_reply(), "expected reply");
            MOZ_ASSERT(!mRecvd->is_reply_error());
            MOZ_ASSERT(mRecvd->type() == replyType, "wrong reply type");
            MOZ_ASSERT(mRecvd->seqno() == seqno);
            MOZ_ASSERT(mRecvd->is_sync());

            *aReply = Move(*mRecvd);
            mRecvd = nullptr;
            return true;
        }

        MOZ_ASSERT(!mTimedOutMessageSeqno);

        bool maybeTimedOut = !WaitForSyncNotify();

        if (!Connected()) {
            ReportConnectionError("MessageChannel::SendAndWait");
            return false;
        }

        // We only time out a message if it initiated a new transaction (i.e.,
        // if neither side has any other message Sends on the stack).
        bool canTimeOut = transaction == seqno;
        if (maybeTimedOut && canTimeOut && !ShouldContinueFromTimeout()) {
            mTimedOutMessageSeqno = seqno;
            return false;
        }
    }

    return true;
}

bool
MessageChannel::Call(Message* aMsg, Message* aReply)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif

    // This must come before MonitorAutoLock, as its destructor acquires the
    // monitor lock.
    CxxStackFrame cxxframe(*this, OUT_MESSAGE, aMsg);

    MonitorAutoLock lock(*mMonitor);
    if (!Connected()) {
        ReportConnectionError("MessageChannel::Call");
        return false;
    }

    // Sanity checks.
    IPC_ASSERT(!AwaitingSyncReply(),
               "cannot issue Interrupt call while blocked on sync request");
    IPC_ASSERT(!DispatchingSyncMessage(),
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
            ReportConnectionError("MessageChannel::Call");
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

        if ((it = mOutOfTurnReplies.find(mInterruptStack.top().seqno()))
            != mOutOfTurnReplies.end())
        {
            recvd = Move(it->second);
            mOutOfTurnReplies.erase(it);
        } else if (!mPending.empty()) {
            recvd = Move(mPending.front());
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
            {
                AutoEnterTransaction transaction(this, recvd);
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
                    mOutOfTurnReplies[recvd.seqno()] = Move(recvd);
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

            bool is_reply_error = recvd.is_reply_error();
            if (!is_reply_error) {
                *aReply = Move(recvd);
            }

            // If we have no more pending out calls waiting on replies, then
            // the reply queue should be empty.
            IPC_ASSERT(!mInterruptStack.empty() || mOutOfTurnReplies.empty(),
                       "still have pending replies with no pending out-calls",
                       true);

            return !is_reply_error;
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
            (!mOutOfTurnReplies.empty() &&
             mOutOfTurnReplies.find(mInterruptStack.top().seqno()) !=
             mOutOfTurnReplies.end()));
}

bool
MessageChannel::ProcessPendingRequest(const Message &aUrgent)
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

    {
        // In order to send the parent RPC messages and guarantee it will
        // wake up, we must re-use its transaction.
        AutoEnterTransaction transaction(this, aUrgent);

        MonitorAutoUnlock unlock(*mMonitor);
        DispatchMessage(aUrgent);
    }
    if (!Connected()) {
        ReportConnectionError("MessageChannel::ProcessPendingRequest");
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

    if (!mDeferred.empty())
        MaybeUndeferIncall();

    if (mPending.empty())
        return false;

    *recvd = Move(mPending.front());
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
        mOutOfTurnReplies[recvd.seqno()] = Move(recvd);
        return false;
    }

    {
        // We should not be in a transaction yet if we're not blocked.
        MOZ_ASSERT(mCurrentTransaction == 0);
        AutoEnterTransaction transaction(this, recvd);

        MonitorAutoUnlock unlock(*mMonitor);

        CxxStackFrame frame(*this, IN_MESSAGE, &recvd);
        DispatchMessage(recvd);
    }
    return true;
}

void
MessageChannel::DispatchMessage(const Message &aMsg)
{
    Maybe<AutoNoJSAPI> nojsapi;
    if (ScriptSettingsInitialized() && NS_IsMainThread())
        nojsapi.emplace();
    if (aMsg.is_sync())
        DispatchSyncMessage(aMsg);
    else if (aMsg.is_interrupt())
        DispatchInterruptMessage(aMsg, 0);
    else
        DispatchAsyncMessage(aMsg);
}

void
MessageChannel::DispatchSyncMessage(const Message& aMsg)
{
    AssertWorkerThread();

    nsAutoPtr<Message> reply;

    int prio = aMsg.priority();

    // We don't want to run any code that might run a nested event loop here, so
    // we avoid running event handlers. Once we've sent the response to the
    // urgent message, it's okay to run event handlers again since the parent is
    // no longer blocked.
    MOZ_ASSERT_IF(prio > IPC::Message::PRIORITY_NORMAL, NS_IsMainThread());
    MaybeScriptBlocker scriptBlocker(this, prio > IPC::Message::PRIORITY_NORMAL);

    IPC_ASSERT(prio >= mDispatchingSyncMessagePriority,
               "priority inversion while dispatching sync message");
    IPC_ASSERT(prio >= mAwaitingSyncReplyPriority,
               "dispatching a message of lower priority while waiting for a response");

    bool dummy;
    bool& blockingVar = ShouldBlockScripts() ? gParentIsBlocked : dummy;

    Result rv;
    if (mTimedOutMessageSeqno) {
        // If the other side sends a message in response to one of our messages
        // that we've timed out, then we reply with an error.
        //
        // We even reject messages that were sent before the other side even got
        // to our timed out message.
        rv = MsgNotAllowed;
    } else {
        AutoSetValue<bool> blocked(blockingVar, true);
        AutoSetValue<bool> sync(mDispatchingSyncMessage, true);
        AutoSetValue<int> prioSet(mDispatchingSyncMessagePriority, prio);
        rv = mListener->OnMessageReceived(aMsg, *getter_Transfers(reply));
    }

    if (!MaybeHandleError(rv, aMsg, "DispatchSyncMessage")) {
        reply = new Message();
        reply->set_sync();
        reply->set_priority(aMsg.priority());
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());
    reply->set_transaction_id(aMsg.transaction_id());

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState) {
        mLink->SendMessage(reply.forget());
    }
}

void
MessageChannel::DispatchAsyncMessage(const Message& aMsg)
{
    AssertWorkerThread();
    MOZ_ASSERT(!aMsg.is_interrupt() && !aMsg.is_sync());

    if (aMsg.routing_id() == MSG_ROUTING_NONE) {
        NS_RUNTIMEABORT("unhandled special message!");
    }

    MaybeHandleError(mListener->OnMessageReceived(aMsg), aMsg, "DispatchAsyncMessage");
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

    nsAutoPtr<Message> reply;

    ++mRemoteStackDepthGuess;
    Result rv = mListener->OnCallReceived(aMsg, *getter_Transfers(reply));
    --mRemoteStackDepthGuess;

    if (!MaybeHandleError(rv, aMsg, "DispatchInterruptMessage")) {
        reply = new Message();
        reply->set_interrupt();
        reply->set_reply();
        reply->set_reply_error();
    }
    reply->set_seqno(aMsg.seqno());

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected == mChannelState) {
        mLink->SendMessage(reply.forget());
    }
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

    MOZ_ASSERT(call.priority() == IPC::Message::PRIORITY_NORMAL);
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
    MOZ_ASSERT(!mPeerPidSet);
    mPeerPidSet = true;
    mPeerPid = peer_id;
    mWorkerLoop->PostTask(FROM_HERE, new DequeueTask(mOnChannelConnectedTask));
}

void
MessageChannel::DispatchOnChannelConnected()
{
    AssertWorkerThread();
    MOZ_ASSERT(mPeerPidSet);
    if (mListener)
        mListener->OnChannelConnected(mPeerPid);
}

void
MessageChannel::ReportMessageRouteError(const char* channelName) const
{
    PrintErrorMessage(mSide, channelName, "Need a route");
    mListener->OnProcessingError(MsgRouteError);
}

void
MessageChannel::ReportConnectionError(const char* aChannelName) const
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

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

    MonitorAutoUnlock unlock(*mMonitor);
    mListener->OnProcessingError(MsgDropped);
}

bool
MessageChannel::MaybeHandleError(Result code, const Message& aMsg, const char* channelName)
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

    char printedMsg[512];
    PR_snprintf(printedMsg, sizeof(printedMsg),
                "(msgtype=0x%lX,name=%s) %s",
                aMsg.type(), aMsg.name(), errorMsg);

    PrintErrorMessage(mSide, channelName, printedMsg);

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

    if (AwaitingSyncReply())
        NotifyWorkerThread();

    if (ChannelClosing != mChannelState) {
        if (mAbortOnError) {
            NS_RUNTIMEABORT("Aborting on channel error.");
        }
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
MessageChannel::CloseWithTimeout()
{
    AssertWorkerThread();

    MonitorAutoLock lock(*mMonitor);
    if (ChannelConnected != mChannelState) {
        return;
    }
    SynchronouslyClose();
    mChannelState = ChannelTimeout;
}

void
MessageChannel::BlockScripts()
{
    MOZ_ASSERT(NS_IsMainThread());
    mBlockScripts = true;
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

        if (ChannelOpening == mChannelState) {
            // SynchronouslyClose() waits for an ack from the other side, so
            // the opening sequence should complete before this returns.
            SynchronouslyClose();
            mChannelState = ChannelError;
            NotifyMaybeChannelError();
            return;
        }

        if (ChannelConnected != mChannelState) {
            // XXX be strict about this until there's a compelling reason
            // to relax
            NS_RUNTIMEABORT("Close() called on closed channel!");
        }

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
    printf_stderr("  remote Interrupt stack guess: %" PRIuSIZE "\n",
                  mRemoteStackDepthGuess);
    printf_stderr("  deferred stack size: %" PRIuSIZE "\n",
                  mDeferred.size());
    printf_stderr("  out-of-turn Interrupt replies stack size: %" PRIuSIZE "\n",
                  mOutOfTurnReplies.size());
    printf_stderr("  Pending queue size: %" PRIuSIZE ", front to back:\n",
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
    for (uint32_t i = 0; i < mCxxStackFrames.length(); ++i) {
        int32_t id;
        const char* dir;
        const char* sems;
        const char* name;
        mCxxStackFrames[i].Describe(&id, &dir, &sems, &name);

        printf_stderr("%s[(%u) %s %s %s(actor=%d) ]\n", pfx,
                      i, dir, sems, name, id);
    }
}

bool
ParentProcessIsBlocked()
{
    return gParentIsBlocked;
}

} // ipc
} // mozilla
