/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageChannel.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsAppRunner.h"
#include "nsAutoPtr.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsContentUtils.h"
#include <math.h>

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

using mozilla::Move;

// Undo the damage done by mozzconf.h
#undef compress

// Logging seems to be somewhat broken on b2g.
#ifdef MOZ_B2G
#define IPC_LOG(...)
#else
static mozilla::LazyLogModule sLogModule("ipc");
#define IPC_LOG(...) MOZ_LOG(sLogModule, LogLevel::Debug, (__VA_ARGS__))
#endif

/*
 * IPC design:
 *
 * There are three kinds of messages: async, sync, and intr. Sync and intr
 * messages are blocking.
 *
 * Terminology: To dispatch a message Foo is to run the RecvFoo code for
 * it. This is also called "handling" the message.
 *
 * Sync and async messages can sometimes "nest" inside other sync messages
 * (i.e., while waiting for the sync reply, we can dispatch the inner
 * message). Intr messages cannot nest.  The three possible nesting levels are
 * NOT_NESTED, NESTED_INSIDE_SYNC, and NESTED_INSIDE_CPOW.  The intended uses
 * are:
 *   NOT_NESTED - most messages.
 *   NESTED_INSIDE_SYNC - CPOW-related messages, which are always sync
 *                        and can go in either direction.
 *   NESTED_INSIDE_CPOW - messages where we don't want to dispatch
 *                        incoming CPOWs while waiting for the response.
 * These nesting levels are ordered: NOT_NESTED, NESTED_INSIDE_SYNC,
 * NESTED_INSIDE_CPOW.  Async messages cannot be NESTED_INSIDE_SYNC but they can
 * be NESTED_INSIDE_CPOW.
 *
 * To avoid jank, the parent process is not allowed to send NOT_NESTED sync messages.
 * When a process is waiting for a response to a sync message
 * M0, it will dispatch an incoming message M if:
 *   1. M has a higher nesting level than M0, or
 *   2. if M has the same nesting level as M0 and we're in the child, or
 *   3. if M has the same nesting level as M0 and it was sent by the other side
 *      while dispatching M0.
 * The idea is that messages with higher nesting should take precendence. The
 * purpose of rule 2 is to handle a race where both processes send to each other
 * simultaneously. In this case, we resolve the race in favor of the parent (so
 * the child dispatches first).
 *
 * Messages satisfy the following properties:
 *   A. When waiting for a response to a sync message, we won't dispatch any
 *      messages of nesting level.
 *   B. Messages of the same nesting level will be dispatched roughly in the
 *      order they were sent. The exception is when the parent and child send
 *      sync messages to each other simulataneously. In this case, the parent's
 *      message is dispatched first. While it is dispatched, the child may send
 *      further nested messages, and these messages may be dispatched before the
 *      child's original message. We can consider ordering to be preserved here
 *      because we pretend that the child's original message wasn't sent until
 *      after the parent's message is finished being dispatched.
 *
 * When waiting for a sync message reply, we dispatch an async message only if
 * it is NESTED_INSIDE_CPOW. Normally NESTED_INSIDE_CPOW async
 * messages are sent only from the child. However, the parent can send
 * NESTED_INSIDE_CPOW async messages when it is creating a bridged protocol.
 *
 * Intr messages are blocking and can nest, but they don't participate in the
 * nesting levels. While waiting for an intr response, all incoming messages are
 * dispatched until a response is received. When two intr messages race with
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
using namespace mozilla::ipc;
using namespace std;

using mozilla::dom::AutoNoJSAPI;
using mozilla::dom::ScriptSettingsInitialized;
using mozilla::MonitorAutoLock;
using mozilla::MonitorAutoUnlock;

#define IPC_ASSERT(_cond, ...)                                      \
    do {                                                            \
        if (!(_cond))                                               \
            DebugAbort(__FILE__, __LINE__, #_cond,## __VA_ARGS__);  \
    } while (0)

static MessageChannel* gParentProcessBlocker;

namespace mozilla {
namespace ipc {

static const uint32_t kMinTelemetryMessageSize = 4096;

// Note: we round the time we spend to the nearest millisecond. So a min value
// of 1 ms actually captures from 500us and above.
static const uint32_t kMinTelemetryIPCWriteLatencyMs = 1;

// Note: we round the time we spend waiting for a response to the nearest
// millisecond. So a min value of 1 ms actually captures from 500us and above.
// This is used for both the sending and receiving side telemetry for sync IPC,
// (IPC_SYNC_MAIN_LATENCY_MS and IPC_SYNC_RECEIVE_MS).
static const uint32_t kMinTelemetrySyncIPCLatencyMs = 1;

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
      : mMessageName(msg->name()),
        mMessageRoutingId(msg->routing_id()),
        mMesageSemantics(msg->is_interrupt() ? INTR_SEMS :
                          msg->is_sync() ? SYNC_SEMS :
                          ASYNC_SEMS),
        mDirection(direction),
        mMoved(false)
    {
        MOZ_RELEASE_ASSERT(mMessageName);
    }

    InterruptFrame(InterruptFrame&& aOther)
    {
        MOZ_RELEASE_ASSERT(aOther.mMessageName);
        mMessageName = aOther.mMessageName;
        aOther.mMessageName = nullptr;
        mMoved = aOther.mMoved;
        aOther.mMoved = true;

        mMessageRoutingId = aOther.mMessageRoutingId;
        mMesageSemantics = aOther.mMesageSemantics;
        mDirection = aOther.mDirection;
    }

    ~InterruptFrame()
    {
        MOZ_RELEASE_ASSERT(mMessageName || mMoved);
    }

    InterruptFrame& operator=(InterruptFrame&& aOther)
    {
        MOZ_RELEASE_ASSERT(&aOther != this);
        this->~InterruptFrame();
        new (this) InterruptFrame(Move(aOther));
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

    bool IsOutgoingSync() const {
        return (mMesageSemantics == INTR_SEMS || mMesageSemantics == SYNC_SEMS) &&
               mDirection == OUT_MESSAGE;
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

    int32_t GetRoutingId() const
    {
        return mMessageRoutingId;
    }

private:
    const char* mMessageName;
    int32_t mMessageRoutingId;
    Semantics mMesageSemantics;
    Direction mDirection;
    bool mMoved;

    // Disable harmful methods.
    InterruptFrame(const InterruptFrame& aOther) = delete;
    InterruptFrame& operator=(const InterruptFrame&) = delete;
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

        if (!mThat.mCxxStackFrames.append(InterruptFrame(direction, msg)))
            MOZ_CRASH();

        const InterruptFrame& frame = mThat.mCxxStackFrames.back();

        if (frame.IsInterruptIncall())
            mThat.EnteredCall();

        if (frame.IsOutgoingSync())
            mThat.EnteredSyncSend();

        mThat.mSawInterruptOutMsg |= frame.IsInterruptOutcall();
    }

    ~CxxStackFrame() {
        mThat.AssertWorkerThread();

        MOZ_RELEASE_ASSERT(!mThat.mCxxStackFrames.empty());

        const InterruptFrame& frame = mThat.mCxxStackFrames.back();
        bool exitingSync = frame.IsOutgoingSync();
        bool exitingCall = frame.IsInterruptIncall();
        mThat.mCxxStackFrames.shrinkBy(1);

        bool exitingStack = mThat.mCxxStackFrames.empty();

        // According how lifetime is declared, mListener on MessageChannel
        // lives longer than MessageChannel itself.  Hence is expected to
        // be alive.  There is nothing to even assert here, there is no place
        // we would be nullifying mListener on MessageChannel.

        if (exitingCall)
            mThat.ExitedCall();

        if (exitingSync)
            mThat.ExitedSyncSend();

        if (exitingStack)
            mThat.ExitedCxxStack();
    }
private:
    MessageChannel& mThat;

    // Disable harmful methods.
    CxxStackFrame() = delete;
    CxxStackFrame(const CxxStackFrame&) = delete;
    CxxStackFrame& operator=(const CxxStackFrame&) = delete;
};

class AutoEnterTransaction
{
public:
    explicit AutoEnterTransaction(MessageChannel *aChan,
                                  int32_t aMsgSeqno,
                                  int32_t aTransactionID,
                                  int aNestedLevel)
      : mChan(aChan),
        mActive(true),
        mOutgoing(true),
        mNestedLevel(aNestedLevel),
        mSeqno(aMsgSeqno),
        mTransaction(aTransactionID),
        mNext(mChan->mTransactionStack)
    {
        mChan->mMonitor->AssertCurrentThreadOwns();
        mChan->mTransactionStack = this;
    }

    explicit AutoEnterTransaction(MessageChannel *aChan, const IPC::Message &aMessage)
      : mChan(aChan),
        mActive(true),
        mOutgoing(false),
        mNestedLevel(aMessage.nested_level()),
        mSeqno(aMessage.seqno()),
        mTransaction(aMessage.transaction_id()),
        mNext(mChan->mTransactionStack)
    {
        mChan->mMonitor->AssertCurrentThreadOwns();

        if (!aMessage.is_sync()) {
            mActive = false;
            return;
        }

        mChan->mTransactionStack = this;
    }

    ~AutoEnterTransaction() {
        mChan->mMonitor->AssertCurrentThreadOwns();
        if (mActive) {
            mChan->mTransactionStack = mNext;
        }
    }

    void Cancel() {
        AutoEnterTransaction *cur = mChan->mTransactionStack;
        MOZ_RELEASE_ASSERT(cur == this);
        while (cur && cur->mNestedLevel != IPC::Message::NOT_NESTED) {
            // Note that, in the following situation, we will cancel multiple
            // transactions:
            // 1. Parent sends NESTED_INSIDE_SYNC message P1 to child.
            // 2. Child sends NESTED_INSIDE_SYNC message C1 to child.
            // 3. Child dispatches P1, parent blocks.
            // 4. Child cancels.
            // In this case, both P1 and C1 are cancelled. The parent will
            // remove C1 from its queue when it gets the cancellation message.
            MOZ_RELEASE_ASSERT(cur->mActive);
            cur->mActive = false;
            cur = cur->mNext;
        }

        mChan->mTransactionStack = cur;

        MOZ_RELEASE_ASSERT(IsComplete());
    }

    bool AwaitingSyncReply() const {
        MOZ_RELEASE_ASSERT(mActive);
        if (mOutgoing) {
            return true;
        }
        return mNext ? mNext->AwaitingSyncReply() : false;
    }

    int AwaitingSyncReplyNestedLevel() const {
        MOZ_RELEASE_ASSERT(mActive);
        if (mOutgoing) {
            return mNestedLevel;
        }
        return mNext ? mNext->AwaitingSyncReplyNestedLevel() : 0;
    }

    bool DispatchingSyncMessage() const {
        MOZ_RELEASE_ASSERT(mActive);
        if (!mOutgoing) {
            return true;
        }
        return mNext ? mNext->DispatchingSyncMessage() : false;
    }

    int DispatchingSyncMessageNestedLevel() const {
        MOZ_RELEASE_ASSERT(mActive);
        if (!mOutgoing) {
            return mNestedLevel;
        }
        return mNext ? mNext->DispatchingSyncMessageNestedLevel() : 0;
    }

    int NestedLevel() const {
        MOZ_RELEASE_ASSERT(mActive);
        return mNestedLevel;
    }

    int32_t SequenceNumber() const {
        MOZ_RELEASE_ASSERT(mActive);
        return mSeqno;
    }

    int32_t TransactionID() const {
        MOZ_RELEASE_ASSERT(mActive);
        return mTransaction;
    }

    void ReceivedReply(IPC::Message&& aMessage) {
        MOZ_RELEASE_ASSERT(aMessage.seqno() == mSeqno);
        MOZ_RELEASE_ASSERT(aMessage.transaction_id() == mTransaction);
        MOZ_RELEASE_ASSERT(!mReply);
        IPC_LOG("Reply received on worker thread: seqno=%d", mSeqno);
        mReply = new IPC::Message(Move(aMessage));
        MOZ_RELEASE_ASSERT(IsComplete());
    }

    void HandleReply(IPC::Message&& aMessage) {
        AutoEnterTransaction *cur = mChan->mTransactionStack;
        MOZ_RELEASE_ASSERT(cur == this);
        while (cur) {
            MOZ_RELEASE_ASSERT(cur->mActive);
            if (aMessage.seqno() == cur->mSeqno) {
                cur->ReceivedReply(Move(aMessage));
                break;
            }
            cur = cur->mNext;
            MOZ_RELEASE_ASSERT(cur);
        }
    }

    bool IsComplete() {
        return !mActive || mReply;
    }

    bool IsOutgoing() {
        return mOutgoing;
    }

    bool IsCanceled() {
        return !mActive;
    }

    bool IsBottom() const {
        return !mNext;
    }

    bool IsError() {
        MOZ_RELEASE_ASSERT(mReply);
        return mReply->is_reply_error();
    }

    nsAutoPtr<IPC::Message> GetReply() {
        return Move(mReply);
    }

private:
    MessageChannel *mChan;

    // Active is true if this transaction is on the mChan->mTransactionStack
    // stack. Generally we're not on the stack if the transaction was canceled
    // or if it was for a message that doesn't require transactions (an async
    // message).
    bool mActive;

    // Is this stack frame for an outgoing message?
    bool mOutgoing;

    // Properties of the message being sent/received.
    int mNestedLevel;
    int32_t mSeqno;
    int32_t mTransaction;

    // Next item in mChan->mTransactionStack.
    AutoEnterTransaction *mNext;

    // Pointer the a reply received for this message, if one was received.
    nsAutoPtr<IPC::Message> mReply;
};

class PromiseReporter final : public nsIMemoryReporter
{
    ~PromiseReporter() {}
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD
    CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                   bool aAnonymize) override
    {
        MOZ_COLLECT_REPORT(
            "unresolved-ipc-promises", KIND_OTHER, UNITS_COUNT, MessageChannel::gUnresolvedPromises,
            "Outstanding IPC async message promises that is still not resolved.");
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(PromiseReporter, nsIMemoryReporter)

Atomic<size_t> MessageChannel::gUnresolvedPromises;

MessageChannel::MessageChannel(const char* aName,
                               IToplevelProtocol *aListener)
  : mName(aName),
    mListener(aListener),
    mChannelState(ChannelClosed),
    mSide(UnknownSide),
    mLink(nullptr),
    mWorkerLoop(nullptr),
    mChannelErrorTask(nullptr),
    mWorkerThread(nullptr),
    mTimeoutMs(kNoTimeout),
    mInTimeoutSecondHalf(false),
    mNextSeqno(0),
    mLastSendError(SyncSendError::SendSuccess),
    mDispatchingAsyncMessage(false),
    mDispatchingAsyncMessageNestedLevel(0),
    mTransactionStack(nullptr),
    mTimedOutMessageSeqno(0),
    mTimedOutMessageNestedLevel(0),
    mMaybeDeferredPendingCount(0),
    mRemoteStackDepthGuess(0),
    mSawInterruptOutMsg(false),
    mIsWaitingForIncoming(false),
    mAbortOnError(false),
    mNotifiedChannelDone(false),
    mFlags(REQUIRE_DEFAULT),
    mPeerPidSet(false),
    mPeerPid(-1),
    mIsPostponingSends(false)
{
    MOZ_COUNT_CTOR(ipc::MessageChannel);

#ifdef OS_WIN
    mTopFrame = nullptr;
    mIsSyncWaitingOnNonMainThread = false;
#endif

    mOnChannelConnectedTask = NewNonOwningCancelableRunnableMethod(
      "ipc::MessageChannel::DispatchOnChannelConnected",
      this,
      &MessageChannel::DispatchOnChannelConnected);

#ifdef OS_WIN
    mEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    MOZ_RELEASE_ASSERT(mEvent, "CreateEvent failed! Nothing is going to work!");
#endif

    static Atomic<bool> registered;
    if (registered.compareExchange(false, true)) {
        RegisterStrongMemoryReporter(new PromiseReporter());
    }
}

MessageChannel::~MessageChannel()
{
    MOZ_COUNT_DTOR(ipc::MessageChannel);
    IPC_ASSERT(mCxxStackFrames.empty(), "mismatched CxxStackFrame ctor/dtors");
#ifdef OS_WIN
    if (mEvent) {
        BOOL ok = CloseHandle(mEvent);
        mEvent = nullptr;

        if (!ok) {
            gfxDevCrash(mozilla::gfx::LogReason::MessageChannelCloseFailure) <<
                "MessageChannel failed to close. GetLastError: " <<
                GetLastError();
        }
        MOZ_RELEASE_ASSERT(ok);
    } else {
        gfxDevCrash(mozilla::gfx::LogReason::MessageChannelCloseFailure) <<
                "MessageChannel destructor ran without an mEvent Handle";
    }
#endif
    Clear();
}

#ifdef DEBUG
void
MessageChannel::AssertMaybeDeferredCountCorrect()
{
    size_t count = 0;
    for (MessageTask* task : mPending) {
        if (!IsAlwaysDeferred(task->Msg())) {
            count++;
        }
    }

    MOZ_ASSERT(count == mMaybeDeferredPendingCount);
}
#endif

// This function returns the current transaction ID. Since the notion of a
// "current transaction" can be hard to define when messages race with each
// other and one gets canceled and the other doesn't, we require that this
// function is only called when the current transaction is known to be for a
// NESTED_INSIDE_SYNC message. In that case, we know for sure what the caller is
// looking for.
int32_t
MessageChannel::CurrentNestedInsideSyncTransaction() const
{
    mMonitor->AssertCurrentThreadOwns();
    if (!mTransactionStack) {
        return 0;
    }
    MOZ_RELEASE_ASSERT(mTransactionStack->NestedLevel() == IPC::Message::NESTED_INSIDE_SYNC);
    return mTransactionStack->TransactionID();
}

bool
MessageChannel::AwaitingSyncReply() const
{
    mMonitor->AssertCurrentThreadOwns();
    return mTransactionStack ? mTransactionStack->AwaitingSyncReply() : false;
}

int
MessageChannel::AwaitingSyncReplyNestedLevel() const
{
    mMonitor->AssertCurrentThreadOwns();
    return mTransactionStack ? mTransactionStack->AwaitingSyncReplyNestedLevel() : 0;
}

bool
MessageChannel::DispatchingSyncMessage() const
{
    mMonitor->AssertCurrentThreadOwns();
    return mTransactionStack ? mTransactionStack->DispatchingSyncMessage() : false;
}

int
MessageChannel::DispatchingSyncMessageNestedLevel() const
{
    mMonitor->AssertCurrentThreadOwns();
    return mTransactionStack ? mTransactionStack->DispatchingSyncMessageNestedLevel() : 0;
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
    if (!mMonitor) {
        return false;
    }
    MonitorAutoLock lock(*mMonitor);
    return Connected();
}

void
MessageChannel::WillDestroyCurrentMessageLoop()
{
#if defined(DEBUG) && !defined(ANDROID)
#if defined(MOZ_CRASHREPORTER)
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProtocolName"),
                                       nsDependentCString(mName));
#endif
    MOZ_CRASH("MessageLoop destroyed before MessageChannel that's bound to it");
#endif
}

void
MessageChannel::Clear()
{
    // Don't clear mWorkerThread; we use it in AssertLinkThread() and
    // AssertWorkerThread().
    //
    // Also don't clear mListener.  If we clear it, then sending a message
    // through this channel after it's Clear()'ed can cause this process to
    // crash.
    //
    // In practice, mListener owns the channel, so the channel gets deleted
    // before mListener.  But just to be safe, mListener is a weak pointer.

#if !defined(ANDROID)
    if (!Unsound_IsClosed()) {
#if defined(MOZ_CRASHREPORTER)
        CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("ProtocolName"),
                                           nsDependentCString(mName));
#endif
        MOZ_CRASH("MessageChannel destroyed without being closed");
    }
#endif

    if (gParentProcessBlocker == this) {
        gParentProcessBlocker = nullptr;
    }

    if (mWorkerLoop) {
        mWorkerLoop->RemoveDestructionObserver(this);
    }

    gUnresolvedPromises -= mPendingPromises.size();
    for (auto& pair : mPendingPromises) {
        pair.second.mRejectFunction(pair.second.mPromise,
                                    PromiseRejectReason::ChannelClosed,
                                    __func__);
    }
    mPendingPromises.clear();

    mWorkerLoop = nullptr;
    delete mLink;
    mLink = nullptr;

    mOnChannelConnectedTask->Cancel();

    if (mChannelErrorTask) {
        mChannelErrorTask->Cancel();
        mChannelErrorTask = nullptr;
    }

    // Free up any memory used by pending messages.
    for (MessageTask* task : mPending) {
        task->Clear();
    }
    mPending.clear();

    mMaybeDeferredPendingCount = 0;

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
    mWorkerThread = GetCurrentVirtualThread();
    mWorkerLoop->AddDestructionObserver(this);
    mListener->SetIsMainThreadProtocol();

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
    aTargetLoop->PostTask(NewNonOwningRunnableMethod<MessageChannel*, Side>(
      "ipc::MessageChannel::OnOpenAsSlave",
      aTargetChan,
      &MessageChannel::OnOpenAsSlave,
      this,
      oppSide));

    while (ChannelOpening == mChannelState)
        mMonitor->Wait();
    MOZ_RELEASE_ASSERT(ChannelConnected == mChannelState, "not connected when awoken");
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
    MOZ_RELEASE_ASSERT(ChannelOpening == aTargetChan->mChannelState,
                          "Target channel not in the process of opening");
    mChannelState = ChannelConnected;
    aTargetChan->mChannelState = ChannelConnected;
    aTargetChan->mMonitor->Notify();
}

void
MessageChannel::CommonThreadOpenInit(MessageChannel *aTargetChan, Side aSide)
{
    mWorkerLoop = MessageLoop::current();
    mWorkerThread = GetCurrentVirtualThread();
    mWorkerLoop->AddDestructionObserver(this);
    mListener->SetIsMainThreadProtocol();

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
        ReportConnectionError("MessageChannel", msg);
        return false;
    }

    mLink->EchoMessage(msg.forget());
    return true;
}

bool
MessageChannel::Send(Message* aMsg)
{
    if (aMsg->size() >= kMinTelemetryMessageSize) {
        Telemetry::Accumulate(Telemetry::IPC_MESSAGE_SIZE2, aMsg->size());
    }

    // If the message was created by the IPC bindings, the create time will be
    // recorded. Use this information to report the IPC_WRITE_MAIN_THREAD_LATENCY_MS (time
    // from message creation to it being sent).
    if (NS_IsMainThread() && aMsg->create_time()) {
        uint32_t latencyMs = round((mozilla::TimeStamp::Now() - aMsg->create_time()).ToMilliseconds());
        if (latencyMs >= kMinTelemetryIPCWriteLatencyMs) {
            mozilla::Telemetry::Accumulate(mozilla::Telemetry::IPC_WRITE_MAIN_THREAD_LATENCY_MS,
                                           nsDependentCString(aMsg->name()),
                                           latencyMs);
        }
    }

    MOZ_RELEASE_ASSERT(!aMsg->is_sync());
    MOZ_RELEASE_ASSERT(aMsg->nested_level() != IPC::Message::NESTED_INSIDE_SYNC);

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
        ReportConnectionError("MessageChannel", msg);
        return false;
    }

    SendMessageToLink(msg.forget());
    return true;
}

void
MessageChannel::SendMessageToLink(Message* aMsg)
{
    if (mIsPostponingSends) {
        UniquePtr<Message> msg(aMsg);
        mPostponedSends.push_back(Move(msg));
        return;
    }
    mLink->SendMessage(aMsg);
}

void
MessageChannel::BeginPostponingSends()
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    MonitorAutoLock lock(*mMonitor);
    {
        MOZ_ASSERT(!mIsPostponingSends);
        mIsPostponingSends = true;
    }
}

void
MessageChannel::StopPostponingSends()
{
    // Note: this can be called from any thread.
    MonitorAutoLock lock(*mMonitor);

    MOZ_ASSERT(mIsPostponingSends);

    for (UniquePtr<Message>& iter : mPostponedSends) {
      mLink->SendMessage(iter.release());
    }

    // We unset this after SendMessage so we can make correct thread
    // assertions in MessageLink.
    mIsPostponingSends = false;
    mPostponedSends.clear();
}

already_AddRefed<MozPromiseRefcountable>
MessageChannel::PopPromise(const Message& aMsg)
{
    auto iter = mPendingPromises.find(aMsg.seqno());
    if (iter != mPendingPromises.end()) {
        PromiseHolder ret = iter->second;
        mPendingPromises.erase(iter);
        gUnresolvedPromises--;
        return ret.mPromise.forget();
    }
    return nullptr;
}

void
MessageChannel::RejectPendingPromisesForActor(ActorIdType aActorId)
{
  auto itr = mPendingPromises.begin();
  while (itr != mPendingPromises.end()) {
    if (itr->second.mActorId != aActorId) {
      ++itr;
      continue;
    }
    auto& promise = itr->second.mPromise;
    itr->second.mRejectFunction(promise,
                                PromiseRejectReason::ActorDestroyed,
                                __func__);
    // Take special care of advancing the iterator since we are
    // removing it while iterating.
    itr = mPendingPromises.erase(itr);
    gUnresolvedPromises--;
  }
}

class BuildIDMessage : public IPC::Message
{
public:
    BuildIDMessage()
        : IPC::Message(MSG_ROUTING_NONE, BUILD_ID_MESSAGE_TYPE)
    {
    }
    void Log(const std::string& aPrefix, FILE* aOutf) const
    {
        fputs("(special `Build ID' message)", aOutf);
    }
};

// Send the parent a special async message to allow it to detect if
// this process is running a different build. This is a minor
// variation on MessageChannel::Send(Message* aMsg).
void
MessageChannel::SendBuildID()
{
    MOZ_ASSERT(!XRE_IsParentProcess());
    nsAutoPtr<BuildIDMessage> msg(new BuildIDMessage());
    nsCString buildID(mozilla::PlatformBuildID());
    IPC::WriteParam(msg, buildID);

    MOZ_RELEASE_ASSERT(!msg->is_sync());
    MOZ_RELEASE_ASSERT(msg->nested_level() != IPC::Message::NESTED_INSIDE_SYNC);

    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();
    // Don't check for MSG_ROUTING_NONE.

    MonitorAutoLock lock(*mMonitor);
    if (!Connected()) {
        ReportConnectionError("MessageChannel", msg);
        MOZ_CRASH();
    }
    mLink->SendMessage(msg.forget());
}

class CancelMessage : public IPC::Message
{
public:
    explicit CancelMessage(int transaction) :
        IPC::Message(MSG_ROUTING_NONE, CANCEL_MESSAGE_TYPE)
    {
        set_transaction_id(transaction);
    }
    static bool Read(const Message* msg) {
        return true;
    }
    void Log(const std::string& aPrefix, FILE* aOutf) const {
        fputs("(special `Cancel' message)", aOutf);
    }
};

MOZ_NEVER_INLINE static void
CheckChildProcessBuildID(const IPC::Message& aMsg)
{
    MOZ_ASSERT(XRE_IsParentProcess());
    nsCString childBuildID;
    PickleIterator msgIter(aMsg);
    MOZ_ALWAYS_TRUE(IPC::ReadParam(&aMsg, &msgIter, &childBuildID));
    aMsg.EndRead(msgIter);

    nsCString parentBuildID(mozilla::PlatformBuildID());

    // This assert can fail if the child process has been updated
    // to a newer version while the parent process was running.
    MOZ_RELEASE_ASSERT(parentBuildID == childBuildID);
}

bool
MessageChannel::MaybeInterceptSpecialIOMessage(const Message& aMsg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MSG_ROUTING_NONE == aMsg.routing_id()) {
        if (GOODBYE_MESSAGE_TYPE == aMsg.type()) {
            // :TODO: Sort out Close() on this side racing with Close() on the
            // other side
            mChannelState = ChannelClosing;
            if (LoggingEnabled()) {
                printf("NOTE: %s process received `Goodbye', closing down\n",
                       (mSide == ChildSide) ? "child" : "parent");
            }
            return true;
        } else if (CANCEL_MESSAGE_TYPE == aMsg.type()) {
            IPC_LOG("Cancel from message");
            CancelTransaction(aMsg.transaction_id());
            NotifyWorkerThread();
            return true;
        } else if (BUILD_ID_MESSAGE_TYPE == aMsg.type()) {
            IPC_LOG("Build ID message");
            CheckChildProcessBuildID(aMsg);
            return true;
        }
    }
    return false;
}

/* static */ bool
MessageChannel::IsAlwaysDeferred(const Message& aMsg)
{
    // If a message is not NESTED_INSIDE_CPOW and not sync, then we always defer
    // it.
    return aMsg.nested_level() != IPC::Message::NESTED_INSIDE_CPOW &&
           !aMsg.is_sync();
}

bool
MessageChannel::ShouldDeferMessage(const Message& aMsg)
{
    // Never defer messages that have the highest nested level, even async
    // ones. This is safe because only the child can send these messages, so
    // they can never nest.
    if (aMsg.nested_level() == IPC::Message::NESTED_INSIDE_CPOW) {
        MOZ_ASSERT(!IsAlwaysDeferred(aMsg));
        return false;
    }

    // Unless they're NESTED_INSIDE_CPOW, we always defer async messages.
    // Note that we never send an async NESTED_INSIDE_SYNC message.
    if (!aMsg.is_sync()) {
        MOZ_RELEASE_ASSERT(aMsg.nested_level() == IPC::Message::NOT_NESTED);
        MOZ_ASSERT(IsAlwaysDeferred(aMsg));
        return true;
    }

    MOZ_ASSERT(!IsAlwaysDeferred(aMsg));

    int msgNestedLevel = aMsg.nested_level();
    int waitingNestedLevel = AwaitingSyncReplyNestedLevel();

    // Always defer if the nested level of the incoming message is less than the
    // nested level of the message we're awaiting.
    if (msgNestedLevel < waitingNestedLevel)
        return true;

    // Never defer if the message has strictly greater nested level.
    if (msgNestedLevel > waitingNestedLevel)
        return false;

    // When both sides send sync messages of the same nested level, we resolve the
    // race by dispatching in the child and deferring the incoming message in
    // the parent. However, the parent still needs to dispatch nested sync
    // messages.
    //
    // Deferring in the parent only sort of breaks message ordering. When the
    // child's message comes in, we can pretend the child hasn't quite
    // finished sending it yet. Since the message is sync, we know that the
    // child hasn't moved on yet.
    return mSide == ParentSide && aMsg.transaction_id() != CurrentNestedInsideSyncTransaction();
}

void
MessageChannel::OnMessageReceivedFromLink(Message&& aMsg)
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    if (MaybeInterceptSpecialIOMessage(aMsg))
        return;

    mListener->OnChannelReceivedMessage(aMsg);

    // Regardless of the Interrupt stack, if we're awaiting a sync reply,
    // we know that it needs to be immediately handled to unblock us.
    if (aMsg.is_sync() && aMsg.is_reply()) {
        IPC_LOG("Received reply seqno=%d xid=%d", aMsg.seqno(), aMsg.transaction_id());

        if (aMsg.seqno() == mTimedOutMessageSeqno) {
            // Drop the message, but allow future sync messages to be sent.
            IPC_LOG("Received reply to timedout message; igoring; xid=%d", mTimedOutMessageSeqno);
            EndTimeout();
            return;
        }

        MOZ_RELEASE_ASSERT(AwaitingSyncReply());
        MOZ_RELEASE_ASSERT(!mTimedOutMessageSeqno);

        mTransactionStack->HandleReply(Move(aMsg));
        NotifyWorkerThread();
        return;
    }

    // Nested messages cannot be compressed.
    MOZ_RELEASE_ASSERT(aMsg.compress_type() == IPC::Message::COMPRESSION_NONE ||
                       aMsg.nested_level() == IPC::Message::NOT_NESTED);

    bool reuseTask = false;
    if (aMsg.compress_type() == IPC::Message::COMPRESSION_ENABLED) {
        bool compress = (!mPending.isEmpty() &&
                         mPending.getLast()->Msg().type() == aMsg.type() &&
                         mPending.getLast()->Msg().routing_id() == aMsg.routing_id());
        if (compress) {
            // This message type has compression enabled, and the back of the
            // queue was the same message type and routed to the same destination.
            // Replace it with the newer message.
            MOZ_RELEASE_ASSERT(mPending.getLast()->Msg().compress_type() ==
                               IPC::Message::COMPRESSION_ENABLED);
            mPending.getLast()->Msg() = Move(aMsg);

            reuseTask = true;
        }
    } else if (aMsg.compress_type() == IPC::Message::COMPRESSION_ALL && !mPending.isEmpty()) {
        for (MessageTask* p = mPending.getLast(); p; p = p->getPrevious()) {
            if (p->Msg().type() == aMsg.type() &&
                p->Msg().routing_id() == aMsg.routing_id())
            {
                // This message type has compression enabled, and the queue
                // holds a message with the same message type and routed to the
                // same destination. Erase it. Note that, since we always
                // compress these redundancies, There Can Be Only One.
                MOZ_RELEASE_ASSERT(p->Msg().compress_type() == IPC::Message::COMPRESSION_ALL);
                MOZ_RELEASE_ASSERT(IsAlwaysDeferred(p->Msg()));
                p->remove();
                break;
            }
        }
    }

    bool alwaysDeferred = IsAlwaysDeferred(aMsg);

    bool wakeUpSyncSend = AwaitingSyncReply() && !ShouldDeferMessage(aMsg);

    bool shouldWakeUp = AwaitingInterruptReply() ||
                        wakeUpSyncSend ||
                        AwaitingIncomingMessage();

    // Although we usually don't need to post a message task if
    // shouldWakeUp is true, it's easier to post anyway than to have to
    // guarantee that every Send call processes everything it's supposed to
    // before returning.
    bool shouldPostTask = !shouldWakeUp || wakeUpSyncSend;

    IPC_LOG("Receive on link thread; seqno=%d, xid=%d, shouldWakeUp=%d",
            aMsg.seqno(), aMsg.transaction_id(), shouldWakeUp);

    if (reuseTask) {
        return;
    }

    // There are three cases we're concerned about, relating to the state of the
    // main thread:
    //
    // (1) We are waiting on a sync reply - main thread is blocked on the
    //     IPC monitor.
    //   - If the message is NESTED_INSIDE_SYNC, we wake up the main thread to
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

#ifdef MOZ_TASK_TRACER
    aMsg.TaskTracerDispatch();
#endif
    RefPtr<MessageTask> task = new MessageTask(this, Move(aMsg));
    mPending.insertBack(task);

    if (!alwaysDeferred) {
        mMaybeDeferredPendingCount++;
    }

    if (shouldWakeUp) {
        NotifyWorkerThread();
    }

    if (shouldPostTask) {
        task->Post();
    }
}

void
MessageChannel::PeekMessages(const std::function<bool(const Message& aMsg)>& aInvoke)
{
    // FIXME: We shouldn't be holding the lock for aInvoke!
    MonitorAutoLock lock(*mMonitor);

    for (MessageTask* it : mPending) {
        const Message &msg = it->Msg();
        if (!aInvoke(msg)) {
            break;
        }
    }
}

void
MessageChannel::ProcessPendingRequests(AutoEnterTransaction& aTransaction)
{
    mMonitor->AssertCurrentThreadOwns();

    AssertMaybeDeferredCountCorrect();
    if (mMaybeDeferredPendingCount == 0) {
        return;
    }

    IPC_LOG("ProcessPendingRequests for seqno=%d, xid=%d",
            aTransaction.SequenceNumber(), aTransaction.TransactionID());

    // Loop until there aren't any more nested messages to process.
    for (;;) {
        // If we canceled during ProcessPendingRequest, then we need to leave
        // immediately because the results of ShouldDeferMessage will be
        // operating with weird state (as if no Send is in progress). That could
        // cause even NOT_NESTED sync messages to be processed (but not
        // NOT_NESTED async messages), which would break message ordering.
        if (aTransaction.IsCanceled()) {
            return;
        }

        mozilla::Vector<Message> toProcess;

        for (MessageTask* p = mPending.getFirst(); p; ) {
            Message &msg = p->Msg();

            MOZ_RELEASE_ASSERT(!aTransaction.IsCanceled(),
                               "Calling ShouldDeferMessage when cancelled");
            bool defer = ShouldDeferMessage(msg);

            // Only log the interesting messages.
            if (msg.is_sync() || msg.nested_level() == IPC::Message::NESTED_INSIDE_CPOW) {
                IPC_LOG("ShouldDeferMessage(seqno=%d) = %d", msg.seqno(), defer);
            }

            if (!defer) {
                MOZ_ASSERT(!IsAlwaysDeferred(msg));

                if (!toProcess.append(Move(msg)))
                    MOZ_CRASH();

                mMaybeDeferredPendingCount--;

                p = p->removeAndGetNext();
                continue;
            }
            p = p->getNext();
        }

        if (toProcess.empty()) {
            break;
        }

        // Processing these messages could result in more messages, so we
        // loop around to check for more afterwards.

        for (auto it = toProcess.begin(); it != toProcess.end(); it++) {
            ProcessPendingRequest(Move(*it));
        }
    }

    AssertMaybeDeferredCountCorrect();
}

bool
MessageChannel::Send(Message* aMsg, Message* aReply)
{
    mozilla::TimeStamp start = TimeStamp::Now();
    if (aMsg->size() >= kMinTelemetryMessageSize) {
        Telemetry::Accumulate(Telemetry::IPC_MESSAGE_SIZE2, aMsg->size());
    }

    nsAutoPtr<Message> msg(aMsg);

    // Sanity checks.
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, false);
    NeuteredWindowRegion neuteredRgn(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION);
#endif
#ifdef MOZ_TASK_TRACER
    AutoScopedLabel autolabel("sync message %s", aMsg->name());
#endif

    CxxStackFrame f(*this, OUT_MESSAGE, msg);

    MonitorAutoLock lock(*mMonitor);

    if (mTimedOutMessageSeqno) {
        // Don't bother sending another sync message if a previous one timed out
        // and we haven't received a reply for it. Once the original timed-out
        // message receives a reply, we'll be able to send more sync messages
        // again.
        IPC_LOG("Send() failed due to previous timeout");
        mLastSendError = SyncSendError::PreviousTimeout;
        return false;
    }

    if (DispatchingSyncMessageNestedLevel() == IPC::Message::NOT_NESTED &&
        msg->nested_level() > IPC::Message::NOT_NESTED)
    {
        // Don't allow sending CPOWs while we're dispatching a sync message.
        // If you want to do that, use sendRpcMessage instead.
        IPC_LOG("Nested level forbids send");
        mLastSendError = SyncSendError::SendingCPOWWhileDispatchingSync;
        return false;
    }

    if (DispatchingSyncMessageNestedLevel() == IPC::Message::NESTED_INSIDE_CPOW ||
        DispatchingAsyncMessageNestedLevel() == IPC::Message::NESTED_INSIDE_CPOW)
    {
        // Generally only the parent dispatches urgent messages. And the only
        // sync messages it can send are NESTED_INSIDE_SYNC. Mainly we want to ensure
        // here that we don't return false for non-CPOW messages.
        MOZ_RELEASE_ASSERT(msg->nested_level() == IPC::Message::NESTED_INSIDE_SYNC);
        IPC_LOG("Sending while dispatching urgent message");
        mLastSendError = SyncSendError::SendingCPOWWhileDispatchingUrgent;
        return false;
    }

    if (msg->nested_level() < DispatchingSyncMessageNestedLevel() ||
        msg->nested_level() < AwaitingSyncReplyNestedLevel())
    {
        MOZ_RELEASE_ASSERT(DispatchingSyncMessage() || DispatchingAsyncMessage());
        MOZ_RELEASE_ASSERT(!mIsPostponingSends);
        IPC_LOG("Cancel from Send");
        CancelMessage *cancel = new CancelMessage(CurrentNestedInsideSyncTransaction());
        CancelTransaction(CurrentNestedInsideSyncTransaction());
        mLink->SendMessage(cancel);
    }

    IPC_ASSERT(msg->is_sync(), "can only Send() sync messages here");

    IPC_ASSERT(msg->nested_level() >= DispatchingSyncMessageNestedLevel(),
               "can't send sync message of a lesser nested level than what's being dispatched");
    IPC_ASSERT(AwaitingSyncReplyNestedLevel() <= msg->nested_level(),
               "nested sync message sends must be of increasing nested level");
    IPC_ASSERT(DispatchingSyncMessageNestedLevel() != IPC::Message::NESTED_INSIDE_CPOW,
               "not allowed to send messages while dispatching urgent messages");

    IPC_ASSERT(DispatchingAsyncMessageNestedLevel() != IPC::Message::NESTED_INSIDE_CPOW,
               "not allowed to send messages while dispatching urgent messages");

    if (!Connected()) {
        ReportConnectionError("MessageChannel::SendAndWait", msg);
        mLastSendError = SyncSendError::NotConnectedBeforeSend;
        return false;
    }

    msg->set_seqno(NextSeqno());

    int32_t seqno = msg->seqno();
    int nestedLevel = msg->nested_level();
    msgid_t replyType = msg->type() + 1;

    AutoEnterTransaction *stackTop = mTransactionStack;

    // If the most recent message on the stack is NESTED_INSIDE_SYNC, then our
    // message should nest inside that and we use the same transaction
    // ID. Otherwise we need a new transaction ID (so we use the seqno of the
    // message we're sending).
    bool nest = stackTop && stackTop->NestedLevel() == IPC::Message::NESTED_INSIDE_SYNC;
    int32_t transaction = nest ? stackTop->TransactionID() : seqno;
    msg->set_transaction_id(transaction);

    bool handleWindowsMessages = mListener->HandleWindowsMessages(*aMsg);
    AutoEnterTransaction transact(this, seqno, transaction, nestedLevel);

    IPC_LOG("Send seqno=%d, xid=%d", seqno, transaction);

    // msg will be destroyed soon, but name() is not owned by msg.
    const char* msgName = msg->name();

    SendMessageToLink(msg.forget());

    while (true) {
        MOZ_RELEASE_ASSERT(!transact.IsCanceled());
        ProcessPendingRequests(transact);
        if (transact.IsComplete()) {
            break;
        }
        if (!Connected()) {
            ReportConnectionError("MessageChannel::Send");
            mLastSendError = SyncSendError::DisconnectedDuringSend;
            return false;
        }

        MOZ_RELEASE_ASSERT(!mTimedOutMessageSeqno);
        MOZ_RELEASE_ASSERT(!transact.IsComplete());
        MOZ_RELEASE_ASSERT(mTransactionStack == &transact);

        bool maybeTimedOut = !WaitForSyncNotify(handleWindowsMessages);

        if (mListener->NeedArtificialSleep()) {
            MonitorAutoUnlock unlock(*mMonitor);
            mListener->ArtificialSleep();
        }

        if (!Connected()) {
            ReportConnectionError("MessageChannel::SendAndWait");
            mLastSendError = SyncSendError::DisconnectedDuringSend;
            return false;
        }

        if (transact.IsCanceled()) {
            break;
        }

        MOZ_RELEASE_ASSERT(mTransactionStack == &transact);

        // We only time out a message if it initiated a new transaction (i.e.,
        // if neither side has any other message Sends on the stack).
        bool canTimeOut = transact.IsBottom();
        if (maybeTimedOut && canTimeOut && !ShouldContinueFromTimeout()) {
            // Since ShouldContinueFromTimeout drops the lock, we need to
            // re-check all our conditions here. We shouldn't time out if any of
            // these things happen because there won't be a reply to the timed
            // out message in these cases.
            if (transact.IsComplete()) {
                break;
            }

            IPC_LOG("Timing out Send: xid=%d", transaction);

            mTimedOutMessageSeqno = seqno;
            mTimedOutMessageNestedLevel = nestedLevel;
            mLastSendError = SyncSendError::TimedOut;
            return false;
        }

        if (transact.IsCanceled()) {
            break;
        }
    }

    if (transact.IsCanceled()) {
        IPC_LOG("Other side canceled seqno=%d, xid=%d", seqno, transaction);
        mLastSendError = SyncSendError::CancelledAfterSend;
        return false;
    }

    if (transact.IsError()) {
        IPC_LOG("Error: seqno=%d, xid=%d", seqno, transaction);
        mLastSendError = SyncSendError::ReplyError;
        return false;
    }

    uint32_t latencyMs = round((TimeStamp::Now() - start).ToMilliseconds());
    IPC_LOG("Got reply: seqno=%d, xid=%d, msgName=%s, latency=%ums",
            seqno, transaction, msgName, latencyMs);

    nsAutoPtr<Message> reply = transact.GetReply();

    MOZ_RELEASE_ASSERT(reply);
    MOZ_RELEASE_ASSERT(reply->is_reply(), "expected reply");
    MOZ_RELEASE_ASSERT(!reply->is_reply_error());
    MOZ_RELEASE_ASSERT(reply->seqno() == seqno);
    MOZ_RELEASE_ASSERT(reply->type() == replyType, "wrong reply type");
    MOZ_RELEASE_ASSERT(reply->is_sync());

    *aReply = Move(*reply);
    if (aReply->size() >= kMinTelemetryMessageSize) {
        Telemetry::Accumulate(Telemetry::IPC_REPLY_SIZE,
                              nsDependentCString(msgName), aReply->size());
    }

    // NOTE: Only collect IPC_SYNC_MAIN_LATENCY_MS on the main thread (bug 1343729)
    if (NS_IsMainThread() && latencyMs >= kMinTelemetrySyncIPCLatencyMs) {
      Telemetry::Accumulate(Telemetry::IPC_SYNC_MAIN_LATENCY_MS,
                            nsDependentCString(msgName), latencyMs);
    }
    return true;
}

bool
MessageChannel::Call(Message* aMsg, Message* aReply)
{
    nsAutoPtr<Message> msg(aMsg);
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

#ifdef OS_WIN
    SyncStackFrame frame(this, true);
#endif
#ifdef MOZ_TASK_TRACER
    AutoScopedLabel autolabel("sync message %s", aMsg->name());
#endif

    // This must come before MonitorAutoLock, as its destructor acquires the
    // monitor lock.
    CxxStackFrame cxxframe(*this, OUT_MESSAGE, msg);

    MonitorAutoLock lock(*mMonitor);
    if (!Connected()) {
        ReportConnectionError("MessageChannel::Call", msg);
        return false;
    }

    // Sanity checks.
    IPC_ASSERT(!AwaitingSyncReply(),
               "cannot issue Interrupt call while blocked on sync request");
    IPC_ASSERT(!DispatchingSyncMessage(),
               "violation of sync handler invariant");
    IPC_ASSERT(msg->is_interrupt(), "can only Call() Interrupt messages here");
    IPC_ASSERT(!mIsPostponingSends, "not postponing sends");

    msg->set_seqno(NextSeqno());
    msg->set_interrupt_remote_stack_depth_guess(mRemoteStackDepthGuess);
    msg->set_interrupt_local_stack_depth(1 + InterruptStackDepth());
    mInterruptStack.push(MessageInfo(*msg));
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

#ifdef OS_WIN
        // We need to limit the scoped of neuteredRgn to this spot in the code.
        // Window neutering can't be enabled during some plugin calls because
        // we then risk the neutered window procedure being subclassed by a
        // plugin.
        {
            NeuteredWindowRegion neuteredRgn(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION);
            /* We should pump messages at this point to ensure that the IPC peer
               does not become deadlocked on a pending inter-thread SendMessage() */
            neuteredRgn.PumpOnce();
        }
#endif

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
        } else if (!mPending.isEmpty()) {
            RefPtr<MessageTask> task = mPending.popFirst();
            recvd = Move(task->Msg());
            if (!IsAlwaysDeferred(recvd)) {
                mMaybeDeferredPendingCount--;
            }
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
            DispatchMessage(Move(recvd));
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
                const MessageInfo &outcall = mInterruptStack.top();

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
#ifdef MOZ_TASK_TRACER
            Message::AutoTaskTracerRun tasktracerRun(recvd);
#endif
            MonitorAutoUnlock unlock(*mMonitor);

            CxxStackFrame frame(*this, IN_MESSAGE, &recvd);
            DispatchInterruptMessage(Move(recvd), stackDepth);
        }
        if (!Connected()) {
            ReportConnectionError("MessageChannel::DispatchInterruptMessage");
            return false;
        }
    }

    return true;
}

bool
MessageChannel::WaitForIncomingMessage()
{
#ifdef OS_WIN
    SyncStackFrame frame(this, true);
    NeuteredWindowRegion neuteredRgn(mFlags & REQUIRE_DEFERRED_MESSAGE_PROTECTION);
#endif

    MonitorAutoLock lock(*mMonitor);
    AutoEnterWaitForIncoming waitingForIncoming(*this);
    if (mChannelState != ChannelConnected) {
        return false;
    }
    if (!HasPendingEvents()) {
        return WaitForInterruptNotify();
    }

    MOZ_RELEASE_ASSERT(!mPending.isEmpty());
    RefPtr<MessageTask> task = mPending.getFirst();
    RunMessage(*task);
    return true;
}

bool
MessageChannel::HasPendingEvents()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();
    return Connected() && !mPending.isEmpty();
}

bool
MessageChannel::InterruptEventOccurred()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();
    IPC_ASSERT(InterruptStackDepth() > 0, "not in wait loop");

    return (!Connected() ||
            !mPending.isEmpty() ||
            (!mOutOfTurnReplies.empty() &&
             mOutOfTurnReplies.find(mInterruptStack.top().seqno()) !=
             mOutOfTurnReplies.end()));
}

bool
MessageChannel::ProcessPendingRequest(Message &&aUrgent)
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    IPC_LOG("Process pending: seqno=%d, xid=%d", aUrgent.seqno(), aUrgent.transaction_id());

    DispatchMessage(Move(aUrgent));
    if (!Connected()) {
        ReportConnectionError("MessageChannel::ProcessPendingRequest");
        return false;
    }

    return true;
}

bool
MessageChannel::ShouldRunMessage(const Message& aMsg)
{
    if (!mTimedOutMessageSeqno) {
        return true;
    }

    // If we've timed out a message and we're awaiting the reply to the timed
    // out message, we have to be careful what messages we process. Here's what
    // can go wrong:
    // 1. child sends a NOT_NESTED sync message S
    // 2. parent sends a NESTED_INSIDE_SYNC sync message H at the same time
    // 3. parent times out H
    // 4. child starts processing H and sends a NESTED_INSIDE_SYNC message H' nested
    //    within the same transaction
    // 5. parent dispatches S and sends reply
    // 6. child asserts because it instead expected a reply to H'.
    //
    // To solve this, we refuse to process S in the parent until we get a reply
    // to H. More generally, let the timed out message be M. We don't process a
    // message unless the child would need the response to that message in order
    // to process M. Those messages are the ones that have a higher nested level
    // than M or that are part of the same transaction as M.
    if (aMsg.nested_level() < mTimedOutMessageNestedLevel ||
        (aMsg.nested_level() == mTimedOutMessageNestedLevel
         && aMsg.transaction_id() != mTimedOutMessageSeqno))
    {
        return false;
    }

    return true;
}

void
MessageChannel::RunMessage(MessageTask& aTask)
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    Message& msg = aTask.Msg();

    if (!Connected()) {
        ReportConnectionError("RunMessage");
        return;
    }

    // Check that we're going to run the first message that's valid to run.
#ifdef DEBUG
    for (MessageTask* task : mPending) {
        if (task == &aTask) {
            break;
        }

        MOZ_ASSERT(!ShouldRunMessage(task->Msg()) ||
                   aTask.Msg().priority() != task->Msg().priority());

    }
#endif

    if (!mDeferred.empty()) {
        MaybeUndeferIncall();
    }

    if (!ShouldRunMessage(msg)) {
        return;
    }

    MOZ_RELEASE_ASSERT(aTask.isInList());
    aTask.remove();

    if (!IsAlwaysDeferred(msg)) {
        mMaybeDeferredPendingCount--;
    }

    if (IsOnCxxStack() && msg.is_interrupt() && msg.is_reply()) {
        // We probably just received a reply in a nested loop for an
        // Interrupt call sent before entering that loop.
        mOutOfTurnReplies[msg.seqno()] = Move(msg);
        return;
    }

    DispatchMessage(Move(msg));
}

NS_IMPL_ISUPPORTS_INHERITED(MessageChannel::MessageTask, CancelableRunnable, nsIRunnablePriority)

MessageChannel::MessageTask::MessageTask(MessageChannel* aChannel, Message&& aMessage)
  : CancelableRunnable(StringFromIPCMessageType(aMessage.type()))
  , mChannel(aChannel)
  , mMessage(Move(aMessage))
  , mScheduled(false)
{
}

nsresult
MessageChannel::MessageTask::Run()
{
    if (!mChannel) {
        return NS_OK;
    }

    mChannel->AssertWorkerThread();
    mChannel->mMonitor->AssertNotCurrentThreadOwns();

    MonitorAutoLock lock(*mChannel->mMonitor);

    // In case we choose not to run this message, we may need to be able to Post
    // it again.
    mScheduled = false;

    if (!isInList()) {
        return NS_OK;
    }

    mChannel->RunMessage(*this);
    return NS_OK;
}

// Warning: This method removes the receiver from whatever list it might be in.
nsresult
MessageChannel::MessageTask::Cancel()
{
    if (!mChannel) {
        return NS_OK;
    }

    mChannel->AssertWorkerThread();
    mChannel->mMonitor->AssertNotCurrentThreadOwns();

    MonitorAutoLock lock(*mChannel->mMonitor);

    if (!isInList()) {
        return NS_OK;
    }
    remove();

    if (!IsAlwaysDeferred(Msg())) {
        mChannel->mMaybeDeferredPendingCount--;
    }

    return NS_OK;
}

void
MessageChannel::MessageTask::Post()
{
    MOZ_RELEASE_ASSERT(!mScheduled);
    MOZ_RELEASE_ASSERT(isInList());

    mScheduled = true;

    RefPtr<MessageTask> self = this;
    nsCOMPtr<nsIEventTarget> eventTarget =
        mChannel->mListener->GetMessageEventTarget(mMessage);

    if (eventTarget) {
        eventTarget->Dispatch(self.forget(), NS_DISPATCH_NORMAL);
    } else {
        mChannel->mWorkerLoop->PostTask(self.forget());
    }
}

void
MessageChannel::MessageTask::Clear()
{
    mChannel->AssertWorkerThread();

    mChannel = nullptr;
}

NS_IMETHODIMP
MessageChannel::MessageTask::GetPriority(uint32_t* aPriority)
{
  *aPriority = mMessage.priority() == Message::HIGH_PRIORITY ?
               PRIORITY_HIGH : PRIORITY_NORMAL;
  return NS_OK;
}

void
MessageChannel::DispatchMessage(Message &&aMsg)
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    Maybe<AutoNoJSAPI> nojsapi;
    if (ScriptSettingsInitialized() && NS_IsMainThread())
        nojsapi.emplace();

    nsAutoPtr<Message> reply;

    IPC_LOG("DispatchMessage: seqno=%d, xid=%d", aMsg.seqno(), aMsg.transaction_id());

    {
        AutoEnterTransaction transaction(this, aMsg);

        int id = aMsg.transaction_id();
        MOZ_RELEASE_ASSERT(!aMsg.is_sync() || id == transaction.TransactionID());

        {
#ifdef MOZ_TASK_TRACER
            Message::AutoTaskTracerRun tasktracerRun(aMsg);
#endif
            MonitorAutoUnlock unlock(*mMonitor);
            CxxStackFrame frame(*this, IN_MESSAGE, &aMsg);

            mListener->ArtificialSleep();

            if (aMsg.is_sync())
                DispatchSyncMessage(aMsg, *getter_Transfers(reply));
            else if (aMsg.is_interrupt())
                DispatchInterruptMessage(Move(aMsg), 0);
            else
                DispatchAsyncMessage(aMsg);

            mListener->ArtificialSleep();
        }

        if (reply && transaction.IsCanceled()) {
            // The transaction has been canceled. Don't send a reply.
            IPC_LOG("Nulling out reply due to cancellation, seqno=%d, xid=%d", aMsg.seqno(), id);
            reply = nullptr;
        }
    }

    if (reply && ChannelConnected == mChannelState) {
        IPC_LOG("Sending reply seqno=%d, xid=%d", aMsg.seqno(), aMsg.transaction_id());
        mLink->SendMessage(reply.forget());
    }
}

void
MessageChannel::DispatchSyncMessage(const Message& aMsg, Message*& aReply)
{
    AssertWorkerThread();

    mozilla::TimeStamp start = TimeStamp::Now();

    int nestedLevel = aMsg.nested_level();

    MOZ_RELEASE_ASSERT(nestedLevel == IPC::Message::NOT_NESTED || NS_IsMainThread());
#ifdef MOZ_TASK_TRACER
    AutoScopedLabel autolabel("sync message %s", aMsg.name());
#endif

    MessageChannel* dummy;
    MessageChannel*& blockingVar = mSide == ChildSide && NS_IsMainThread() ? gParentProcessBlocker : dummy;

    Result rv;
    {
        AutoSetValue<MessageChannel*> blocked(blockingVar, this);
        rv = mListener->OnMessageReceived(aMsg, aReply);
    }

    uint32_t latencyMs = round((TimeStamp::Now() - start).ToMilliseconds());
    if (latencyMs >= kMinTelemetrySyncIPCLatencyMs) {
        Telemetry::Accumulate(Telemetry::IPC_SYNC_RECEIVE_MS,
                              nsDependentCString(aMsg.name()),
                              latencyMs);
    }

    if (!MaybeHandleError(rv, aMsg, "DispatchSyncMessage")) {
        aReply = new Message();
        aReply->set_sync();
        aReply->set_nested_level(aMsg.nested_level());
        aReply->set_reply();
        aReply->set_reply_error();
    }
    aReply->set_seqno(aMsg.seqno());
    aReply->set_transaction_id(aMsg.transaction_id());
}

void
MessageChannel::DispatchAsyncMessage(const Message& aMsg)
{
    AssertWorkerThread();
    MOZ_RELEASE_ASSERT(!aMsg.is_interrupt() && !aMsg.is_sync());

    if (aMsg.routing_id() == MSG_ROUTING_NONE) {
        MOZ_CRASH("unhandled special message!");
    }

    Result rv;
    {
        int nestedLevel = aMsg.nested_level();
        AutoSetValue<bool> async(mDispatchingAsyncMessage, true);
        AutoSetValue<int> nestedLevelSet(mDispatchingAsyncMessageNestedLevel, nestedLevel);
        rv = mListener->OnMessageReceived(aMsg);
    }
    MaybeHandleError(rv, aMsg, "DispatchAsyncMessage");
}

void
MessageChannel::DispatchInterruptMessage(Message&& aMsg, size_t stackDepth)
{
    AssertWorkerThread();
    mMonitor->AssertNotCurrentThreadOwns();

    IPC_ASSERT(aMsg.is_interrupt() && !aMsg.is_reply(), "wrong message type");

    if (ShouldDeferInterruptMessage(aMsg, stackDepth)) {
        // We now know the other side's stack has one more frame
        // than we thought.
        ++mRemoteStackDepthGuess; // decremented in MaybeProcessDeferred()
        mDeferred.push(Move(aMsg));
        return;
    }

    // If we "lost" a race and need to process the other side's in-call, we
    // don't need to fix up the mRemoteStackDepthGuess here, because we're just
    // about to increment it, which will make it correct again.

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

bool
MessageChannel::ShouldDeferInterruptMessage(const Message& aMsg, size_t aStackDepth)
{
    AssertWorkerThread();

    // We may or may not own the lock in this function, so don't access any
    // channel state.

    IPC_ASSERT(aMsg.is_interrupt() && !aMsg.is_reply(), "wrong message type");

    // Race detection: see the long comment near mRemoteStackDepthGuess in
    // MessageChannel.h. "Remote" stack depth means our side, and "local" means
    // the other side.
    if (aMsg.interrupt_remote_stack_depth_guess() == RemoteViewOfStackDepth(aStackDepth)) {
        return false;
    }

    // Interrupt in-calls have raced. The winner, if there is one, gets to defer
    // processing of the other side's in-call.
    bool defer;
    const char* winner;
    const MessageInfo parentMsgInfo =
        (mSide == ChildSide) ? MessageInfo(aMsg) : mInterruptStack.top();
    const MessageInfo childMsgInfo =
        (mSide == ChildSide) ? mInterruptStack.top() : MessageInfo(aMsg);
    switch (mListener->MediateInterruptRace(parentMsgInfo, childMsgInfo))
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
            MOZ_CRASH("NYI: 'Error' Interrupt race policy");
        default:
            MOZ_CRASH("not reached");
    }

    IPC_LOG("race in %s: %s won",
            (mSide == ChildSide) ? "child" : "parent",
            winner);

    return defer;
}

void
MessageChannel::MaybeUndeferIncall()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    if (mDeferred.empty())
        return;

    size_t stackDepth = InterruptStackDepth();

    Message& deferred = mDeferred.top();

    // the other side can only *under*-estimate our actual stack depth
    IPC_ASSERT(deferred.interrupt_remote_stack_depth_guess() <= stackDepth,
               "fatal logic error");

    if (ShouldDeferInterruptMessage(deferred, stackDepth)) {
        return;
    }

    // maybe time to process this message
    Message call(Move(deferred));
    mDeferred.pop();

    // fix up fudge factor we added to account for race
    IPC_ASSERT(0 < mRemoteStackDepthGuess, "fatal logic error");
    --mRemoteStackDepthGuess;

    MOZ_RELEASE_ASSERT(call.nested_level() == IPC::Message::NOT_NESTED);
    RefPtr<MessageTask> task = new MessageTask(this, Move(call));
    mPending.insertBack(task);
    MOZ_ASSERT(IsAlwaysDeferred(task->Msg()));
    task->Post();
}

void
MessageChannel::EnteredCxxStack()
{
    mListener->EnteredCxxStack();
}

void
MessageChannel::ExitedCxxStack()
{
    mListener->ExitedCxxStack();
    if (mSawInterruptOutMsg) {
        MonitorAutoLock lock(*mMonitor);
        // see long comment in OnMaybeDequeueOne()
        EnqueuePendingMessages();
        mSawInterruptOutMsg = false;
    }
}

void
MessageChannel::EnteredCall()
{
    mListener->EnteredCall();
}

void
MessageChannel::ExitedCall()
{
    mListener->ExitedCall();
}

void
MessageChannel::EnteredSyncSend()
{
    mListener->OnEnteredSyncSend();
}

void
MessageChannel::ExitedSyncSend()
{
    mListener->OnExitedSyncSend();
}

void
MessageChannel::EnqueuePendingMessages()
{
    AssertWorkerThread();
    mMonitor->AssertCurrentThreadOwns();

    MaybeUndeferIncall();

    // XXX performance tuning knob: could process all or k pending
    // messages here, rather than enqueuing for later processing

    RepostAllMessages();
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
MessageChannel::WaitForSyncNotify(bool /* aHandleWindowsMessages */)
{
#ifdef DEBUG
    // WARNING: We don't release the lock here. We can't because the link thread
    // could signal at this time and we would miss it. Instead we require
    // ArtificialTimeout() to be extremely simple.
    if (mListener->ArtificialTimeout()) {
        return false;
    }
#endif

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
    return WaitForSyncNotify(true);
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
        cont = mListener->ShouldContinueFromReplyTimeout();
        mListener->ArtificialSleep();
    }

    static enum { UNKNOWN, NOT_DEBUGGING, DEBUGGING } sDebuggingChildren = UNKNOWN;

    if (sDebuggingChildren == UNKNOWN) {
        sDebuggingChildren = getenv("MOZ_DEBUG_CHILD_PROCESS") ||
                             getenv("MOZ_DEBUG_CHILD_PAUSE")
                               ? DEBUGGING
                               : NOT_DEBUGGING;
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
    MOZ_RELEASE_ASSERT(!mPeerPidSet);
    mPeerPidSet = true;
    mPeerPid = peer_id;
    RefPtr<CancelableRunnable> task = mOnChannelConnectedTask;
    mWorkerLoop->PostTask(task.forget());
}

void
MessageChannel::DispatchOnChannelConnected()
{
    AssertWorkerThread();
    MOZ_RELEASE_ASSERT(mPeerPidSet);
    mListener->OnChannelConnected(mPeerPid);
}

void
MessageChannel::ReportMessageRouteError(const char* channelName) const
{
    PrintErrorMessage(mSide, channelName, "Need a route");
    mListener->ProcessingError(MsgRouteError, "MsgRouteError");
}

void
MessageChannel::ReportConnectionError(const char* aChannelName, Message* aMsg) const
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
        MOZ_CRASH("unreached");
    }

    if (aMsg) {
        char reason[512];
        SprintfLiteral(reason,"(msgtype=0x%X,name=%s) %s",
                       aMsg->type(), aMsg->name(), errorMsg);

        PrintErrorMessage(mSide, aChannelName, reason);
    } else {
        PrintErrorMessage(mSide, aChannelName, errorMsg);
    }

    MonitorAutoUnlock unlock(*mMonitor);
    mListener->ProcessingError(MsgDropped, errorMsg);
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
        MOZ_CRASH("unknown Result code");
        return false;
    }

    char reason[512];
    const char* msgname = StringFromIPCMessageType(aMsg.type());
    if (msgname[0] == '?') {
        SprintfLiteral(reason,"(msgtype=0x%X) %s", aMsg.type(), errorMsg);
    } else {
        SprintfLiteral(reason,"%s %s", msgname, errorMsg);
    }

    PrintErrorMessage(mSide, channelName, reason);

    // Error handled in mozilla::ipc::IPCResult.
    if (code == MsgProcessingError) {
        return false;
    }

    mListener->ProcessingError(code, reason);

    return false;
}

void
MessageChannel::OnChannelErrorFromLink()
{
    AssertLinkThread();
    mMonitor->AssertCurrentThreadOwns();

    IPC_LOG("OnChannelErrorFromLink");

    if (InterruptStackDepth() > 0)
        NotifyWorkerThread();

    if (AwaitingSyncReply() || AwaitingIncomingMessage())
        NotifyWorkerThread();

    if (ChannelClosing != mChannelState) {
        if (mAbortOnError) {
            MOZ_CRASH("Aborting on channel error.");
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

    Clear();

    // Oops, error!  Let the listener know about it.
    mChannelState = ChannelError;

    // IPDL assumes these notifications do not fire twice, so we do not let
    // that happen.
    if (mNotifiedChannelDone) {
      return;
    }
    mNotifiedChannelDone = true;

    // After this, the channel may be deleted.  Based on the premise that
    // mListener owns this channel, any calls back to this class that may
    // work with mListener should still work on living objects.
    mListener->OnChannelError();
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
      mChannelErrorTask = NewNonOwningCancelableRunnableMethod(
        "ipc::MessageChannel::OnNotifyMaybeChannelError",
        this,
        &MessageChannel::OnNotifyMaybeChannelError);
      RefPtr<Runnable> task = mChannelErrorTask;
      // 10 ms delay is completely arbitrary
      mWorkerLoop->PostDelayedTask(task.forget(), 10);
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
    mChannelErrorTask = NewNonOwningCancelableRunnableMethod(
      "ipc::MessageChannel::OnNotifyMaybeChannelError",
      this,
      &MessageChannel::OnNotifyMaybeChannelError);
    RefPtr<Runnable> task = mChannelErrorTask;
    mWorkerLoop->PostTask(task.forget());
}

// Special async message.
class GoodbyeMessage : public IPC::Message
{
public:
    GoodbyeMessage() :
        IPC::Message(MSG_ROUTING_NONE, GOODBYE_MESSAGE_TYPE)
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

        if (ChannelClosed == mChannelState) {
            // XXX be strict about this until there's a compelling reason
            // to relax
            MOZ_CRASH("Close() called on closed channel!");
        }

        // Notify the other side that we're about to close our socket. If we've
        // already received a Goodbye from the other side (and our state is
        // ChannelClosing), there's no reason to send one.
        if (ChannelConnected == mChannelState) {
          mLink->SendMessage(new GoodbyeMessage());
        }
        SynchronouslyClose();
    }

    NotifyChannelClosed();
}

void
MessageChannel::NotifyChannelClosed()
{
    mMonitor->AssertNotCurrentThreadOwns();

    if (ChannelClosed != mChannelState)
        MOZ_CRASH("channel should have been closed!");

    Clear();

    // IPDL assumes these notifications do not fire twice, so we do not let
    // that happen.
    if (mNotifiedChannelDone) {
      return;
    }
    mNotifiedChannelDone = true;

    // OK, the IO thread just closed the channel normally.  Let the
    // listener know about it. After this point the channel may be
    // deleted.
    mListener->OnChannelClose();
}

void
MessageChannel::DebugAbort(const char* file, int line, const char* cond,
                           const char* why,
                           bool reply)
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

    MessageQueue pending = Move(mPending);
    while (!pending.isEmpty()) {
        printf_stderr("    [ %s%s ]\n",
                      pending.getFirst()->Msg().is_interrupt() ? "intr" :
                      (pending.getFirst()->Msg().is_sync() ? "sync" : "async"),
                      pending.getFirst()->Msg().is_reply() ? "reply" : "");
        pending.popFirst();
    }

    NS_RUNTIMEABORT(why);
}

void
MessageChannel::DumpInterruptStack(const char* const pfx) const
{
    NS_WARNING_ASSERTION(
      MessageLoop::current() != mWorkerLoop,
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

int32_t
MessageChannel::GetTopmostMessageRoutingId() const
{
    MOZ_RELEASE_ASSERT(MessageLoop::current() == mWorkerLoop);
    if (mCxxStackFrames.empty()) {
        return MSG_ROUTING_NONE;
    }
    const InterruptFrame& frame = mCxxStackFrames.back();
    return frame.GetRoutingId();
}

void
MessageChannel::EndTimeout()
{
    mMonitor->AssertCurrentThreadOwns();

    IPC_LOG("Ending timeout of seqno=%d", mTimedOutMessageSeqno);
    mTimedOutMessageSeqno = 0;
    mTimedOutMessageNestedLevel = 0;

    RepostAllMessages();
}

void
MessageChannel::RepostAllMessages()
{
    bool needRepost = false;
    for (MessageTask* task : mPending) {
        if (!task->IsScheduled()) {
            needRepost = true;
        }
    }
    if (!needRepost) {
        // If everything is already scheduled to run, do nothing.
        return;
    }

    // In some cases we may have deferred dispatch of some messages in the
    // queue. Now we want to run them again. However, we can't just re-post
    // those messages since the messages after them in mPending would then be
    // before them in the event queue. So instead we cancel everything and
    // re-post all messages in the correct order.
    MessageQueue queue = Move(mPending);
    while (RefPtr<MessageTask> task = queue.popFirst()) {
        RefPtr<MessageTask> newTask = new MessageTask(this, Move(task->Msg()));
        mPending.insertBack(newTask);
        newTask->Post();
    }

    AssertMaybeDeferredCountCorrect();
}

void
MessageChannel::CancelTransaction(int transaction)
{
    mMonitor->AssertCurrentThreadOwns();

    // When we cancel a transaction, we need to behave as if there's no longer
    // any IPC on the stack. Anything we were dispatching or sending will get
    // canceled. Consequently, we have to update the state variables below.
    //
    // We also need to ensure that when any IPC functions on the stack return,
    // they don't reset these values using an RAII class like AutoSetValue. To
    // avoid that, these RAII classes check if the variable they set has been
    // tampered with (by us). If so, they don't reset the variable to the old
    // value.

    IPC_LOG("CancelTransaction: xid=%d", transaction);

    // An unusual case: We timed out a transaction which the other side then
    // cancelled. In this case we just leave the timedout state and try to
    // forget this ever happened.
    if (transaction == mTimedOutMessageSeqno) {
        IPC_LOG("Cancelled timed out message %d", mTimedOutMessageSeqno);
        EndTimeout();

        // Normally mCurrentTransaction == 0 here. But it can be non-zero if:
        // 1. Parent sends NESTED_INSIDE_SYNC message H.
        // 2. Parent times out H.
        // 3. Child dispatches H and sends nested message H' (same transaction).
        // 4. Parent dispatches H' and cancels.
        MOZ_RELEASE_ASSERT(!mTransactionStack || mTransactionStack->TransactionID() == transaction);
        if (mTransactionStack) {
            mTransactionStack->Cancel();
        }
    } else {
        MOZ_RELEASE_ASSERT(mTransactionStack->TransactionID() == transaction);
        mTransactionStack->Cancel();
    }

    bool foundSync = false;
    for (MessageTask* p = mPending.getFirst(); p; ) {
        Message &msg = p->Msg();

        // If there was a race between the parent and the child, then we may
        // have a queued sync message. We want to drop this message from the
        // queue since if will get cancelled along with the transaction being
        // cancelled. This happens if the message in the queue is NESTED_INSIDE_SYNC.
        if (msg.is_sync() && msg.nested_level() != IPC::Message::NOT_NESTED) {
            MOZ_RELEASE_ASSERT(!foundSync);
            MOZ_RELEASE_ASSERT(msg.transaction_id() != transaction);
            IPC_LOG("Removing msg from queue seqno=%d xid=%d", msg.seqno(), msg.transaction_id());
            foundSync = true;
            if (!IsAlwaysDeferred(msg)) {
                mMaybeDeferredPendingCount--;
            }
            p = p->removeAndGetNext();
            continue;
        }

        p = p->getNext();
    }

    AssertMaybeDeferredCountCorrect();
}

bool
MessageChannel::IsInTransaction() const
{
    MonitorAutoLock lock(*mMonitor);
    return !!mTransactionStack;
}

void
MessageChannel::CancelCurrentTransaction()
{
    MonitorAutoLock lock(*mMonitor);
    if (DispatchingSyncMessageNestedLevel() >= IPC::Message::NESTED_INSIDE_SYNC) {
        if (DispatchingSyncMessageNestedLevel() == IPC::Message::NESTED_INSIDE_CPOW ||
            DispatchingAsyncMessageNestedLevel() == IPC::Message::NESTED_INSIDE_CPOW)
        {
            mListener->IntentionalCrash();
        }

        IPC_LOG("Cancel requested: current xid=%d", CurrentNestedInsideSyncTransaction());
        MOZ_RELEASE_ASSERT(DispatchingSyncMessage());
        CancelMessage *cancel = new CancelMessage(CurrentNestedInsideSyncTransaction());
        CancelTransaction(CurrentNestedInsideSyncTransaction());
        mLink->SendMessage(cancel);
    }
}

void
CancelCPOWs()
{
    if (gParentProcessBlocker) {
        mozilla::Telemetry::Accumulate(mozilla::Telemetry::IPC_TRANSACTION_CANCEL, true);
        gParentProcessBlocker->CancelCurrentTransaction();
    }
}

} // namespace ipc
} // namespace mozilla
