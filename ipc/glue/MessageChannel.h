/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_MessageChannel_h
#define ipc_glue_MessageChannel_h 1

#include "base/basictypes.h"
#include "base/message_loop.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/Vector.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/ipc/Transport.h"
#include "MessageLink.h"
#include "nsAutoPtr.h"

#include <deque>
#include <stack>
#include <math.h>

namespace mozilla {
namespace ipc {

class MessageChannel;

class RefCountedMonitor : public Monitor
{
  public:
    RefCountedMonitor()
        : Monitor("mozilla.ipc.MessageChannel.mMonitor")
    {}

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMonitor)

  private:
    ~RefCountedMonitor() {}
};

class MessageChannel : HasResultCodes
{
    friend class ProcessLink;
    friend class ThreadLink;

    class CxxStackFrame;
    class InterruptFrame;

    typedef mozilla::Monitor Monitor;

  public:
    static const int32_t kNoTimeout;

    typedef IPC::Message Message;
    typedef mozilla::ipc::Transport Transport;

    explicit MessageChannel(MessageListener *aListener);
    ~MessageChannel();

    // "Open" from the perspective of the transport layer; the underlying
    // socketpair/pipe should already be created.
    //
    // Returns true if the transport layer was successfully connected,
    // i.e., mChannelState == ChannelConnected.
    bool Open(Transport* aTransport, MessageLoop* aIOLoop=0, Side aSide=UnknownSide);

    // "Open" a connection to another thread in the same process.
    //
    // Returns true if the transport layer was successfully connected,
    // i.e., mChannelState == ChannelConnected.
    //
    // For more details on the process of opening a channel between
    // threads, see the extended comment on this function
    // in MessageChannel.cpp.
    bool Open(MessageChannel *aTargetChan, MessageLoop *aTargetLoop, Side aSide);

    // Close the underlying transport channel.
    void Close();

    bool Connected() const;

    // Force the channel to behave as if a channel error occurred. Valid
    // for process links only, not thread links.
    void CloseWithError();

    void SetAbortOnError(bool abort)
    {
        mAbortOnError = true;
    }

    // Misc. behavioral traits consumers can request for this channel
    enum ChannelFlags {
      REQUIRE_DEFAULT                         = 0,
      // Windows: if this channel operates on the UI thread, indicates
      // WindowsMessageLoop code should enable deferred native message
      // handling to prevent deadlocks. Should only be used for protocols
      // that manage child processes which might create native UI, like
      // plugins.
      REQUIRE_DEFERRED_MESSAGE_PROTECTION     = 1 << 0
    };
    void SetChannelFlags(ChannelFlags aFlags) { mFlags = aFlags; }
    ChannelFlags GetChannelFlags() { return mFlags; }

    void BlockScripts();

    bool ShouldBlockScripts() const
    {
        return mBlockScripts;
    }

    // Asynchronously send a message to the other side of the channel
    bool Send(Message* aMsg);

    // Asynchronously deliver a message back to this side of the
    // channel
    bool Echo(Message* aMsg);

    // Synchronously send |msg| (i.e., wait for |reply|)
    bool Send(Message* aMsg, Message* aReply);

    // Make an Interrupt call to the other side of the channel
    bool Call(Message* aMsg, Message* aReply);

    bool CanSend() const;

    void SetReplyTimeoutMs(int32_t aTimeoutMs);

    bool IsOnCxxStack() const {
        return !mCxxStackFrames.empty();
    }

    void FlushPendingInterruptQueue();

    // Unsound_IsClosed and Unsound_NumQueuedMessages are safe to call from any
    // thread, but they make no guarantees about whether you'll get an
    // up-to-date value; the values are written on one thread and read without
    // locking, on potentially different threads.  Thus you should only use
    // them when you don't particularly care about getting a recent value (e.g.
    // in a memory report).
    bool Unsound_IsClosed() const {
        return mLink ? mLink->Unsound_IsClosed() : true;
    }
    uint32_t Unsound_NumQueuedMessages() const {
        return mLink ? mLink->Unsound_NumQueuedMessages() : 0;
    }

    static bool IsPumpingMessages() {
        return sIsPumpingMessages;
    }
    static void SetIsPumpingMessages(bool aIsPumping) {
        sIsPumpingMessages = aIsPumping;
    }

#ifdef OS_WIN
    struct MOZ_STACK_CLASS SyncStackFrame
    {
        SyncStackFrame(MessageChannel* channel, bool interrupt);
        ~SyncStackFrame();

        bool mInterrupt;
        bool mSpinNestedEvents;
        bool mListenerNotified;
        MessageChannel* mChannel;

        // The previous stack frame for this channel.
        SyncStackFrame* mPrev;

        // The previous stack frame on any channel.
        SyncStackFrame* mStaticPrev;
    };
    friend struct MessageChannel::SyncStackFrame;

    static bool IsSpinLoopActive() {
        for (SyncStackFrame* frame = sStaticTopFrame; frame; frame = frame->mPrev) {
            if (frame->mSpinNestedEvents)
                return true;
        }
        return false;
    }

  protected:
    // The deepest sync stack frame for this channel.
    SyncStackFrame* mTopFrame;

    bool mIsSyncWaitingOnNonMainThread;

    // The deepest sync stack frame on any channel.
    static SyncStackFrame* sStaticTopFrame;

  public:
    void ProcessNativeEventsInInterruptCall();
    static void NotifyGeckoEventDispatch();

  private:
    void SpinInternalEventLoop();
#endif

  private:
    void CommonThreadOpenInit(MessageChannel *aTargetChan, Side aSide);
    void OnOpenAsSlave(MessageChannel *aTargetChan, Side aSide);

    void PostErrorNotifyTask();
    void OnNotifyMaybeChannelError();
    void ReportConnectionError(const char* aChannelName) const;
    void ReportMessageRouteError(const char* channelName) const;
    bool MaybeHandleError(Result code, const Message& aMsg, const char* channelName);

    void Clear();

    // Send OnChannelConnected notification to listeners.
    void DispatchOnChannelConnected();

    // Any protocol that requires blocking until a reply arrives, will send its
    // outgoing message through this function. Currently, two protocols do this:
    //
    //  sync, which can only initiate messages from child to parent.
    //  urgent, which can only initiate messages from parent to child.
    //
    // SendAndWait() expects that the worker thread owns the monitor, and that
    // the message has been prepared to be sent over the link. It returns as
    // soon as a reply has been received, or an error has occurred.
    //
    // Note that while the child is blocked waiting for a sync reply, it can wake
    // up to process urgent calls from the parent.
    bool SendAndWait(Message* aMsg, Message* aReply);

    bool InterruptEventOccurred();

    bool ProcessPendingRequest(const Message &aUrgent);

    void MaybeUndeferIncall();
    void EnqueuePendingMessages();

    // Executed on the worker thread. Dequeues one pending message.
    bool OnMaybeDequeueOne();
    bool DequeueOne(Message *recvd);

    // Dispatches an incoming message to its appropriate handler.
    void DispatchMessage(const Message &aMsg);

    // DispatchMessage will route to one of these functions depending on the
    // protocol type of the message.
    void DispatchSyncMessage(const Message &aMsg);
    void DispatchUrgentMessage(const Message &aMsg);
    void DispatchAsyncMessage(const Message &aMsg);
    void DispatchRPCMessage(const Message &aMsg);
    void DispatchInterruptMessage(const Message &aMsg, size_t aStackDepth);

    // Return true if the wait ended because a notification was received.
    //
    // Return false if the time elapsed from when we started the process of
    // waiting until afterwards exceeded the currently allotted timeout.
    // That *DOES NOT* mean false => "no event" (== timeout); there are many
    // circumstances that could cause the measured elapsed time to exceed the
    // timeout EVEN WHEN we were notified.
    //
    // So in sum: true is a meaningful return value; false isn't,
    // necessarily.
    bool WaitForSyncNotify();
    bool WaitForInterruptNotify();

    bool WaitResponse(bool aWaitTimedOut);

    bool ShouldContinueFromTimeout();

    // The "remote view of stack depth" can be different than the
    // actual stack depth when there are out-of-turn replies.  When we
    // receive one, our actual Interrupt stack depth doesn't decrease, but
    // the other side (that sent the reply) thinks it has.  So, the
    // "view" returned here is |stackDepth| minus the number of
    // out-of-turn replies.
    //
    // Only called from the worker thread.
    size_t RemoteViewOfStackDepth(size_t stackDepth) const {
        AssertWorkerThread();
        return stackDepth - mOutOfTurnReplies.size();
    }

    int32_t NextSeqno() {
        AssertWorkerThread();
        return (mSide == ChildSide) ? --mNextSeqno : ++mNextSeqno;
    }

    // This helper class manages mCxxStackDepth on behalf of MessageChannel.
    // When the stack depth is incremented from zero to non-zero, it invokes
    // a callback, and similarly for when the depth goes from non-zero to zero.
    void EnteredCxxStack() {
       mListener->OnEnteredCxxStack();
    }

    void ExitedCxxStack();

    void EnteredCall() {
        mListener->OnEnteredCall();
    }

    void ExitedCall() {
        mListener->OnExitedCall();
    }

    MessageListener *Listener() const {
        return mListener.get();
    }

    void DebugAbort(const char* file, int line, const char* cond,
                    const char* why,
                    bool reply=false) const;

    // This method is only safe to call on the worker thread, or in a
    // debugger with all threads paused.
    void DumpInterruptStack(const char* const pfx="") const;

  private:
    // Called from both threads
    size_t InterruptStackDepth() const {
        mMonitor->AssertCurrentThreadOwns();
        return mInterruptStack.size();
    }

    // Returns true if we're blocking waiting for a reply.
    bool AwaitingSyncReply() const {
        mMonitor->AssertCurrentThreadOwns();
        return mAwaitingSyncReply;
    }
    int AwaitingSyncReplyPriority() const {
        mMonitor->AssertCurrentThreadOwns();
        return mAwaitingSyncReplyPriority;
    }
    bool AwaitingInterruptReply() const {
        mMonitor->AssertCurrentThreadOwns();
        return !mInterruptStack.empty();
    }

    // Returns true if we're dispatching a sync message's callback.
    bool DispatchingSyncMessage() const {
        AssertWorkerThread();
        return mDispatchingSyncMessage;
    }

    int DispatchingSyncMessagePriority() const {
        AssertWorkerThread();
        return mDispatchingSyncMessagePriority;
    }

  private:
    // Executed on the IO thread.
    void NotifyWorkerThread();

    // Return true if |aMsg| is a special message targeted at the IO
    // thread, in which case it shouldn't be delivered to the worker.
    bool MaybeInterceptSpecialIOMessage(const Message& aMsg);

    void OnChannelConnected(int32_t peer_id);

    // Tell the IO thread to close the channel and wait for it to ACK.
    void SynchronouslyClose();

    bool ShouldDeferMessage(const Message& aMsg);
    void OnMessageReceivedFromLink(const Message& aMsg);
    void OnChannelErrorFromLink();

  private:
    // Run on the not current thread.
    void NotifyChannelClosed();
    void NotifyMaybeChannelError();

  private:
    // Can be run on either thread
    void AssertWorkerThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoopID == MessageLoop::current()->id(),
                          "not on worker thread!");
    }

    // The "link" thread is either the I/O thread (ProcessLink) or the
    // other actor's work thread (ThreadLink).  In either case, it is
    // NOT our worker thread.
    void AssertLinkThread() const
    {
        NS_ABORT_IF_FALSE(mWorkerLoopID != MessageLoop::current()->id(),
                          "on worker thread but should not be!");
    }

  private:
    typedef IPC::Message::msgid_t msgid_t;
    typedef std::deque<Message> MessageQueue;
    typedef std::map<size_t, Message> MessageMap;

    // All dequeuing tasks require a single point of cancellation,
    // which is handled via a reference-counted task.
    class RefCountedTask
    {
      public:
        explicit RefCountedTask(CancelableTask* aTask)
          : mTask(aTask)
        { }
      private:
        ~RefCountedTask() { delete mTask; }
      public:
        void Run() { mTask->Run(); }
        void Cancel() { mTask->Cancel(); }

        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedTask)

      private:
        CancelableTask* mTask;
    };

    // Wrap an existing task which can be cancelled at any time
    // without the wrapper's knowledge.
    class DequeueTask : public Task
    {
      public:
        explicit DequeueTask(RefCountedTask* aTask)
          : mTask(aTask)
        { }
        void Run() { mTask->Run(); }

      private:
        nsRefPtr<RefCountedTask> mTask;
    };

  private:
    mozilla::WeakPtr<MessageListener> mListener;
    ChannelState mChannelState;
    nsRefPtr<RefCountedMonitor> mMonitor;
    Side mSide;
    MessageLink* mLink;
    MessageLoop* mWorkerLoop;           // thread where work is done
    CancelableTask* mChannelErrorTask;  // NotifyMaybeChannelError runnable

    // id() of mWorkerLoop.  This persists even after mWorkerLoop is cleared
    // during channel shutdown.
    int mWorkerLoopID;

    // A task encapsulating dequeuing one pending message.
    nsRefPtr<RefCountedTask> mDequeueOneTask;

    // Timeout periods are broken up in two to prevent system suspension from
    // triggering an abort. This method (called by WaitForEvent with a 'did
    // timeout' flag) decides if we should wait again for half of mTimeoutMs
    // or give up.
    int32_t mTimeoutMs;
    bool mInTimeoutSecondHalf;

    // Worker-thread only; sequence numbers for messages that require
    // synchronous replies.
    int32_t mNextSeqno;

    static bool sIsPumpingMessages;

    template<class T>
    class AutoSetValue {
      public:
        explicit AutoSetValue(T &var, const T &newValue)
          : mVar(var), mPrev(var)
        {
            mVar = newValue;
        }
        ~AutoSetValue() {
            mVar = mPrev;
        }
      private:
        T& mVar;
        T mPrev;
    };

    // Worker thread only.
    bool mAwaitingSyncReply;
    int mAwaitingSyncReplyPriority;

    // Set while we are dispatching a synchronous message. Only for use on the
    // worker thread.
    bool mDispatchingSyncMessage;
    int mDispatchingSyncMessagePriority;

    // When we send an urgent request from the parent process, we could race
    // with an RPC message that was issued by the child beforehand. In this
    // case, if the parent were to wake up while waiting for the urgent reply,
    // and process the RPC, it could send an additional urgent message. The
    // child would wake up to process the urgent message (as it always will),
    // then send a reply, which could be received by the parent out-of-order
    // with respect to the first urgent reply.
    //
    // To address this problem, urgent or RPC requests are associated with a
    // "transaction". Whenever one side of the channel wishes to start a
    // chain of RPC/urgent messages, it allocates a new transaction ID. Any
    // messages the parent receives, not apart of this transaction, are
    // deferred. When issuing RPC/urgent requests on top of a started
    // transaction, the initiating transaction ID is used.
    //
    // To ensure IDs are unique, we use sequence numbers for transaction IDs,
    // which grow in opposite directions from child to parent.

    // The current transaction ID.
    int32_t mCurrentTransaction;

    class AutoEnterTransaction
    {
      public:
       explicit AutoEnterTransaction(MessageChannel *aChan)
        : mChan(aChan),
          mOldTransaction(mChan->mCurrentTransaction)
       {
           mChan->mMonitor->AssertCurrentThreadOwns();
           if (mChan->mCurrentTransaction == 0)
               mChan->mCurrentTransaction = mChan->NextSeqno();
       }
       explicit AutoEnterTransaction(MessageChannel *aChan, const Message &aMessage)
        : mChan(aChan),
          mOldTransaction(mChan->mCurrentTransaction)
       {
           mChan->mMonitor->AssertCurrentThreadOwns();

           if (!aMessage.is_sync())
               return;

           MOZ_ASSERT_IF(mChan->mSide == ParentSide && mOldTransaction != aMessage.transaction_id(),
                         !mOldTransaction || aMessage.priority() > mChan->AwaitingSyncReplyPriority());
           mChan->mCurrentTransaction = aMessage.transaction_id();
       }
       ~AutoEnterTransaction() {
           mChan->mMonitor->AssertCurrentThreadOwns();
           mChan->mCurrentTransaction = mOldTransaction;
       }

      private:
       MessageChannel *mChan;
       int32_t mOldTransaction;
    };

    // If waiting for the reply to a sync out-message, it will be saved here
    // on the I/O thread and then read and cleared by the worker thread.
    nsAutoPtr<Message> mRecvd;

    // Queue of all incoming messages, except for replies to sync and urgent
    // messages, which are delivered directly to mRecvd, and any pending urgent
    // incall, which is stored in mPendingUrgentRequest.
    //
    // If both this side and the other side are functioning correctly, the queue
    // can only be in certain configurations.  Let
    //
    //   |A<| be an async in-message,
    //   |S<| be a sync in-message,
    //   |C<| be an Interrupt in-call,
    //   |R<| be an Interrupt reply.
    //
    // The queue can only match this configuration
    //
    //  A<* (S< | C< | R< (?{mStack.size() == 1} A<* (S< | C<)))
    //
    // The other side can send as many async messages |A<*| as it wants before
    // sending us a blocking message.
    //
    // The first case is |S<|, a sync in-msg.  The other side must be blocked,
    // and thus can't send us any more messages until we process the sync
    // in-msg.
    //
    // The second case is |C<|, an Interrupt in-call; the other side must be blocked.
    // (There's a subtlety here: this in-call might have raced with an
    // out-call, but we detect that with the mechanism below,
    // |mRemoteStackDepth|, and races don't matter to the queue.)
    //
    // Final case, the other side replied to our most recent out-call |R<|.
    // If that was the *only* out-call on our stack, |?{mStack.size() == 1}|,
    // then other side "finished with us," and went back to its own business.
    // That business might have included sending any number of async message
    // |A<*| until sending a blocking message |(S< | C<)|.  If we had more than
    // one Interrupt call on our stack, the other side *better* not have sent us
    // another blocking message, because it's blocked on a reply from us.
    //
    MessageQueue mPending;

    // Stack of all the out-calls on which this channel is awaiting responses.
    // Each stack refers to a different protocol and the stacks are mutually
    // exclusive: multiple outcalls of the same kind cannot be initiated while
    // another is active.
    std::stack<Message> mInterruptStack;

    // This is what we think the Interrupt stack depth is on the "other side" of this
    // Interrupt channel.  We maintain this variable so that we can detect racy Interrupt
    // calls.  With each Interrupt out-call sent, we send along what *we* think the
    // stack depth of the remote side is *before* it will receive the Interrupt call.
    //
    // After sending the out-call, our stack depth is "incremented" by pushing
    // that pending message onto mPending.
    //
    // Then when processing an in-call |c|, it must be true that
    //
    //   mStack.size() == c.remoteDepth
    //
    // I.e., my depth is actually the same as what the other side thought it
    // was when it sent in-call |c|.  If this fails to hold, we have detected
    // racy Interrupt calls.
    //
    // We then increment mRemoteStackDepth *just before* processing the
    // in-call, since we know the other side is waiting on it, and decrement
    // it *just after* finishing processing that in-call, since our response
    // will pop the top of the other side's |mPending|.
    //
    // One nice aspect of this race detection is that it is symmetric; if one
    // side detects a race, then the other side must also detect the same race.
    size_t mRemoteStackDepthGuess;

    // Approximation of code frames on the C++ stack. It can only be
    // interpreted as the implication:
    //
    //  !mCxxStackFrames.empty() => MessageChannel code on C++ stack
    //
    // This member is only accessed on the worker thread, and so is not
    // protected by mMonitor.  It is managed exclusively by the helper
    // |class CxxStackFrame|.
    mozilla::Vector<InterruptFrame> mCxxStackFrames;

    // Did we process an Interrupt out-call during this stack?  Only meaningful in
    // ExitedCxxStack(), from which this variable is reset.
    bool mSawInterruptOutMsg;

    // Map of replies received "out of turn", because of Interrupt
    // in-calls racing with replies to outstanding in-calls.  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=521929.
    MessageMap mOutOfTurnReplies;

    // Stack of Interrupt in-calls that were deferred because of race
    // conditions.
    std::stack<Message> mDeferred;

#ifdef OS_WIN
    HANDLE mEvent;
#endif

    // Should the channel abort the process from the I/O thread when
    // a channel error occurs?
    bool mAbortOnError;

    // Should we prevent scripts from running while dispatching urgent messages?
    bool mBlockScripts;

    // See SetChannelFlags
    ChannelFlags mFlags;

    // Task and state used to asynchronously notify channel has been connected
    // safely.  This is necessary to be able to cancel notification if we are
    // closed at the same time.
    nsRefPtr<RefCountedTask> mOnChannelConnectedTask;
    DebugOnly<bool> mPeerPidSet;
    int32_t mPeerPid;
};

bool
ParentProcessIsBlocked();

} // namespace ipc
} // namespace mozilla

#endif  // ifndef ipc_glue_MessageChannel_h
