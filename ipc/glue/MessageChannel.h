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

#include "mozilla/Function.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Monitor.h"
#include "mozilla/Vector.h"
#if defined(OS_WIN)
#include "mozilla/ipc/Neutering.h"
#endif // defined(OS_WIN)
#include "mozilla/ipc/Transport.h"
#if defined(MOZ_CRASHREPORTER) && defined(OS_WIN)
#include "mozilla/mozalloc_oom.h"
#include "nsExceptionHandler.h"
#endif
#include "MessageLink.h"

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

enum class SyncSendError {
    SendSuccess,
    PreviousTimeout,
    SendingCPOWWhileDispatchingSync,
    SendingCPOWWhileDispatchingUrgent,
    NotConnectedBeforeSend,
    DisconnectedDuringSend,
    CancelledBeforeSend,
    CancelledAfterSend,
    TimedOut,
    ReplyError,
};

class AutoEnterTransaction;

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
    typedef IPC::MessageInfo MessageInfo;
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

    // Force the channel to behave as if a channel error occurred. Valid
    // for process links only, not thread links.
    void CloseWithError();

    void CloseWithTimeout();

    void SetAbortOnError(bool abort)
    {
        mAbortOnError = abort;
    }

    // Call aInvoke for each pending message until it returns false.
    // XXX: You must get permission from an IPC peer to use this function
    //      since it requires custom deserialization and re-orders events.
    void PeekMessages(mozilla::function<bool(const Message& aMsg)> aInvoke);

    // Misc. behavioral traits consumers can request for this channel
    enum ChannelFlags {
      REQUIRE_DEFAULT                         = 0,
      // Windows: if this channel operates on the UI thread, indicates
      // WindowsMessageLoop code should enable deferred native message
      // handling to prevent deadlocks. Should only be used for protocols
      // that manage child processes which might create native UI, like
      // plugins.
      REQUIRE_DEFERRED_MESSAGE_PROTECTION     = 1 << 0,
      // Windows: When this flag is specified, any wait that occurs during
      // synchronous IPC will be alertable, thus allowing a11y code in the
      // chrome process to reenter content while content is waiting on a
      // synchronous call.
      REQUIRE_A11Y_REENTRY                    = 1 << 1,
    };
    void SetChannelFlags(ChannelFlags aFlags) { mFlags = aFlags; }
    ChannelFlags GetChannelFlags() { return mFlags; }

    // Asynchronously send a message to the other side of the channel
    bool Send(Message* aMsg);

    // Asynchronously deliver a message back to this side of the
    // channel
    bool Echo(Message* aMsg);

    // Synchronously send |msg| (i.e., wait for |reply|)
    bool Send(Message* aMsg, Message* aReply);

    // Make an Interrupt call to the other side of the channel
    bool Call(Message* aMsg, Message* aReply);

    // Wait until a message is received
    bool WaitForIncomingMessage();

    bool CanSend() const;

    // If sending a sync message returns an error, this function gives a more
    // descriptive error message.
    SyncSendError LastSendError() const {
        AssertWorkerThread();
        return mLastSendError;
    }

    // Currently only for debugging purposes, doesn't aquire mMonitor.
    ChannelState GetChannelState__TotallyRacy() const {
        return mChannelState;
    }

    void SetReplyTimeoutMs(int32_t aTimeoutMs);

    bool IsOnCxxStack() const {
        return !mCxxStackFrames.empty();
    }

    bool IsInTransaction() const;
    void CancelCurrentTransaction();

    /**
     * This function is used by hang annotation code to determine which IPDL
     * actor is highest in the call stack at the time of the hang. It should
     * be called from the main thread when a sync or intr message is about to
     * be sent.
     */
    int32_t GetTopmostMessageRoutingId() const;

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
#if defined(ACCESSIBILITY)
    bool WaitForSyncNotifyWithA11yReentry();
#endif // defined(ACCESSIBILITY)
#endif // defined(OS_WIN)

  private:
    void CommonThreadOpenInit(MessageChannel *aTargetChan, Side aSide);
    void OnOpenAsSlave(MessageChannel *aTargetChan, Side aSide);

    void PostErrorNotifyTask();
    void OnNotifyMaybeChannelError();
    void ReportConnectionError(const char* aChannelName, Message* aMsg = nullptr) const;
    void ReportMessageRouteError(const char* channelName) const;
    bool MaybeHandleError(Result code, const Message& aMsg, const char* channelName);

    void Clear();

    // Send OnChannelConnected notification to listeners.
    void DispatchOnChannelConnected();

    bool InterruptEventOccurred();
    bool HasPendingEvents();

    void ProcessPendingRequests(AutoEnterTransaction& aTransaction);
    bool ProcessPendingRequest(Message &&aUrgent);

    void MaybeUndeferIncall();
    void EnqueuePendingMessages();

    // Executed on the worker thread. Dequeues one pending message.
    bool OnMaybeDequeueOne();
    bool DequeueOne(Message *recvd);

    // Dispatches an incoming message to its appropriate handler.
    void DispatchMessage(Message &&aMsg);

    // DispatchMessage will route to one of these functions depending on the
    // protocol type of the message.
    void DispatchSyncMessage(const Message &aMsg, Message*& aReply);
    void DispatchUrgentMessage(const Message &aMsg);
    void DispatchAsyncMessage(const Message &aMsg);
    void DispatchRPCMessage(const Message &aMsg);
    void DispatchInterruptMessage(Message &&aMsg, size_t aStackDepth);

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
    bool WaitForSyncNotify(bool aHandleWindowsMessages);
    bool WaitForInterruptNotify();

    bool WaitResponse(bool aWaitTimedOut);

    bool ShouldContinueFromTimeout();

    void EndTimeout();
    void CancelTransaction(int transaction);

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

    void EnteredSyncSend() {
        mListener->OnEnteredSyncSend();
    }

    void ExitedSyncSend() {
        mListener->OnExitedSyncSend();
    }

    MessageListener *Listener() const {
        return mListener;
    }

    void DebugAbort(const char* file, int line, const char* cond,
                    const char* why,
                    bool reply=false);

    // This method is only safe to call on the worker thread, or in a
    // debugger with all threads paused.
    void DumpInterruptStack(const char* const pfx="") const;

  private:
    // Called from both threads
    size_t InterruptStackDepth() const {
        mMonitor->AssertCurrentThreadOwns();
        return mInterruptStack.size();
    }

    bool AwaitingInterruptReply() const {
        mMonitor->AssertCurrentThreadOwns();
        return !mInterruptStack.empty();
    }
    bool AwaitingIncomingMessage() const {
        mMonitor->AssertCurrentThreadOwns();
        return mIsWaitingForIncoming;
    }

    class MOZ_STACK_CLASS AutoEnterWaitForIncoming
    {
    public:
        explicit AutoEnterWaitForIncoming(MessageChannel& aChannel)
            : mChannel(aChannel)
        {
            aChannel.mMonitor->AssertCurrentThreadOwns();
            aChannel.mIsWaitingForIncoming = true;
        }

        ~AutoEnterWaitForIncoming()
        {
            mChannel.mIsWaitingForIncoming = false;
        }

    private:
        MessageChannel& mChannel;
    };
    friend class AutoEnterWaitForIncoming;

    // Returns true if we're dispatching an async message's callback.
    bool DispatchingAsyncMessage() const {
        AssertWorkerThread();
        return mDispatchingAsyncMessage;
    }

    int DispatchingAsyncMessageNestedLevel() const {
        AssertWorkerThread();
        return mDispatchingAsyncMessageNestedLevel;
    }

    bool Connected() const;

  private:
    // Executed on the IO thread.
    void NotifyWorkerThread();

    // Return true if |aMsg| is a special message targeted at the IO
    // thread, in which case it shouldn't be delivered to the worker.
    bool MaybeInterceptSpecialIOMessage(const Message& aMsg);

    void OnChannelConnected(int32_t peer_id);

    // Tell the IO thread to close the channel and wait for it to ACK.
    void SynchronouslyClose();

    bool WasTransactionCanceled(int transaction);
    bool ShouldDeferMessage(const Message& aMsg);
    void OnMessageReceivedFromLink(Message&& aMsg);
    void OnChannelErrorFromLink();

  private:
    // Run on the not current thread.
    void NotifyChannelClosed();
    void NotifyMaybeChannelError();

  private:
    // Can be run on either thread
    void AssertWorkerThread() const
    {
        MOZ_RELEASE_ASSERT(mWorkerLoopID == MessageLoop::current()->id(),
                           "not on worker thread!");
    }

    // The "link" thread is either the I/O thread (ProcessLink) or the
    // other actor's work thread (ThreadLink).  In either case, it is
    // NOT our worker thread.
    void AssertLinkThread() const
    {
        MOZ_RELEASE_ASSERT(mWorkerLoopID != MessageLoop::current()->id(),
                           "on worker thread but should not be!");
    }

  private:
#if defined(MOZ_CRASHREPORTER) && defined(OS_WIN)
    // TODO: Remove the condition OS_WIN above once we move to GCC 5 or higher,
    // the code will be able to get compiled as std::deque will meet C++11
    // allocator requirements.
    template<class T>
    struct AnnotateAllocator
    {
      typedef T value_type;
      AnnotateAllocator(MessageChannel& channel) : mChannel(channel) {}
      template<class U> AnnotateAllocator(const AnnotateAllocator<U>& other) :
        mChannel(other.mChannel) {}
      template<class U> bool operator==(const AnnotateAllocator<U>&) { return true; }
      template<class U> bool operator!=(const AnnotateAllocator<U>&) { return false; }
      T* allocate(size_t n) {
        void* p = ::operator new(n * sizeof(T), std::nothrow);
        if (!p && n) {
          // Sort the pending messages by its type, note the sorting algorithm
          // has to be in-place to avoid memory allocation.
          MessageQueue& q = mChannel.mPending;
          std::sort(q.begin(), q.end(), [](const Message& a, const Message& b) {
            return a.type() < b.type();
          });

          // Iterate over the sorted queue to find the message that has the
          // highest number of count.
          const char* topName = nullptr;
          const char* curName = nullptr;
          msgid_t topType = 0, curType = 0;
          uint32_t topCount = 0, curCount = 0;
          for (MessageQueue::iterator it = q.begin(); it != q.end(); ++it) {
            Message &msg = *it;
            if (msg.type() == curType) {
              ++curCount;
            } else {
              if (curCount > topCount) {
                topName = curName;
                topType = curType;
                topCount = curCount;
              }
              curName = StringFromIPCMessageType(msg.type());
              curType = msg.type();
              curCount = 1;
            }
          }
          // In case the last type is the top one.
          if (curCount > topCount) {
            topName = curName;
            topType = curType;
            topCount = curCount;
          }

          CrashReporter::AnnotatePendingIPC(q.size(), topCount, topName, topType);

          mozalloc_handle_oom(n * sizeof(T));
        }
        return static_cast<T*>(p);
      }
      void deallocate(T* p, size_t n) {
        ::operator delete(p);
      }
      MessageChannel& mChannel;
    };
    typedef std::deque<Message, AnnotateAllocator<Message>> MessageQueue;
#else
    typedef std::deque<Message> MessageQueue;
#endif
    typedef std::map<size_t, Message> MessageMap;
    typedef IPC::Message::msgid_t msgid_t;

    // XXXkhuey this can almost certainly die.
    // All dequeuing tasks require a single point of cancellation,
    // which is handled via a reference-counted task.
    class RefCountedTask
    {
      public:
        explicit RefCountedTask(already_AddRefed<CancelableRunnable> aTask)
          : mTask(aTask)
        { }
      private:
        ~RefCountedTask() { }
      public:
        void Run() { mTask->Run(); }
        void Cancel() { mTask->Cancel(); }

        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedTask)

      private:
        RefPtr<CancelableRunnable> mTask;
    };

    // Wrap an existing task which can be cancelled at any time
    // without the wrapper's knowledge.
    class DequeueTask : public CancelableRunnable
    {
      public:
        explicit DequeueTask(RefCountedTask* aTask)
          : mTask(aTask)
        { }
        NS_IMETHOD Run() override {
          if (mTask) {
            mTask->Run();
          }
          return NS_OK;
        }
        nsresult Cancel() override {
          mTask = nullptr;
          return NS_OK;
        }

      private:
        RefPtr<RefCountedTask> mTask;
    };

  private:
    // Based on presumption the listener owns and overlives the channel,
    // this is never nullified.
    MessageListener* mListener;
    ChannelState mChannelState;
    RefPtr<RefCountedMonitor> mMonitor;
    Side mSide;
    MessageLink* mLink;
    MessageLoop* mWorkerLoop;           // thread where work is done
    RefPtr<CancelableRunnable> mChannelErrorTask;  // NotifyMaybeChannelError runnable

    // id() of mWorkerLoop.  This persists even after mWorkerLoop is cleared
    // during channel shutdown.
    int mWorkerLoopID;

    // A task encapsulating dequeuing one pending message.
    RefPtr<RefCountedTask> mDequeueOneTask;

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

    // If ::Send returns false, this gives a more descriptive error.
    SyncSendError mLastSendError;

    template<class T>
    class AutoSetValue {
      public:
        explicit AutoSetValue(T &var, const T &newValue)
          : mVar(var), mPrev(var), mNew(newValue)
        {
            mVar = newValue;
        }
        ~AutoSetValue() {
            // The value may have been zeroed if the transaction was
            // canceled. In that case we shouldn't return it to its previous
            // value.
            if (mVar == mNew) {
                mVar = mPrev;
            }
        }
      private:
        T& mVar;
        T mPrev;
        T mNew;
    };

    bool mDispatchingAsyncMessage;
    int mDispatchingAsyncMessageNestedLevel;

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

    friend class AutoEnterTransaction;
    AutoEnterTransaction *mTransactionStack;

    int32_t CurrentNestedInsideSyncTransaction() const;

    bool AwaitingSyncReply() const;
    int AwaitingSyncReplyNestedLevel() const;

    bool DispatchingSyncMessage() const;
    int DispatchingSyncMessageNestedLevel() const;

    // If a sync message times out, we store its sequence number here. Any
    // future sync messages will fail immediately. Once the reply for original
    // sync message is received, we allow sync messages again.
    //
    // When a message times out, nothing is done to inform the other side. The
    // other side will eventually dispatch the message and send a reply. Our
    // side is responsible for replying to all sync messages sent by the other
    // side when it dispatches the timed out message. The response is always an
    // error.
    //
    // A message is only timed out if it initiated a transaction. This avoids
    // hitting a lot of corner cases with message nesting that we don't really
    // care about.
    int32_t mTimedOutMessageSeqno;
    int mTimedOutMessageNestedLevel;

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
    std::stack<MessageInfo> mInterruptStack;

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

    // Are we waiting on this channel for an incoming message? This is used
    // to implement WaitForIncomingMessage(). Must only be accessed while owning
    // mMonitor.
    bool mIsWaitingForIncoming;

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

    // True if the listener has already been notified of a channel close or
    // error.
    bool mNotifiedChannelDone;

    // See SetChannelFlags
    ChannelFlags mFlags;

    // Task and state used to asynchronously notify channel has been connected
    // safely.  This is necessary to be able to cancel notification if we are
    // closed at the same time.
    RefPtr<RefCountedTask> mOnChannelConnectedTask;
    bool mPeerPidSet;
    int32_t mPeerPid;
};

void
CancelCPOWs();

} // namespace ipc
} // namespace mozilla

#endif  // ifndef ipc_glue_MessageChannel_h
