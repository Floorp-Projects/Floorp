/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ipc_glue_MessageChannel_h
#define ipc_glue_MessageChannel_h 1

#include "ipc/EnumSerializer.h"
#include "mozilla/Atomics.h"
#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/Vector.h"
#if defined(OS_WIN)
#  include "mozilla/ipc/Neutering.h"
#endif  // defined(OS_WIN)

#include <functional>
#include <map>
#include <stack>
#include <vector>

#include "MessageLink.h"  // for HasResultCodes
#include "mozilla/ipc/ScopedPort.h"

class MessageLoop;

namespace IPC {
template <typename T>
struct ParamTraits;
}

namespace mozilla {
namespace ipc {

class IToplevelProtocol;
class ActorLifecycleProxy;

class RefCountedMonitor : public Monitor {
 public:
  RefCountedMonitor() : Monitor("mozilla.ipc.MessageChannel.mMonitor") {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedMonitor)

 private:
  ~RefCountedMonitor() = default;
};

enum class MessageDirection {
  eSending,
  eReceiving,
};

enum class MessagePhase {
  Endpoint,
  TransferStart,
  TransferEnd,
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

enum class ResponseRejectReason {
  SendError,
  ChannelClosed,
  HandlerRejected,
  ActorDestroyed,
  ResolverDestroyed,
  EndGuard_,
};

template <typename T>
using ResolveCallback = std::function<void(T&&)>;

using RejectCallback = std::function<void(ResponseRejectReason)>;

enum ChannelState {
  ChannelClosed,
  ChannelConnected,
  ChannelTimeout,
  ChannelClosing,
  ChannelError
};

class AutoEnterTransaction;

class MessageChannel : HasResultCodes {
  friend class PortLink;
#ifdef FUZZING
  friend class ProtocolFuzzerHelper;
#endif

  typedef mozilla::Monitor Monitor;

  // We could templatize the actor type but it would unnecessarily
  // expand the code size. Using the actor address as the
  // identifier is already good enough.
  typedef void* ActorIdType;

 public:
  struct UntypedCallbackHolder {
    UntypedCallbackHolder(ActorIdType aActorId, RejectCallback&& aReject)
        : mActorId(aActorId), mReject(std::move(aReject)) {}

    virtual ~UntypedCallbackHolder() = default;

    void Reject(ResponseRejectReason&& aReason) { mReject(std::move(aReason)); }

    ActorIdType mActorId;
    RejectCallback mReject;
  };

  template <typename Value>
  struct CallbackHolder : public UntypedCallbackHolder {
    CallbackHolder(ActorIdType aActorId, ResolveCallback<Value>&& aResolve,
                   RejectCallback&& aReject)
        : UntypedCallbackHolder(aActorId, std::move(aReject)),
          mResolve(std::move(aResolve)) {}

    void Resolve(Value&& aReason) { mResolve(std::move(aReason)); }

    ResolveCallback<Value> mResolve;
  };

 private:
  static Atomic<size_t> gUnresolvedResponses;
  friend class PendingResponseReporter;

 public:
  static constexpr int32_t kNoTimeout = INT32_MIN;

  typedef IPC::Message Message;
  using ScopedPort = mozilla::ipc::ScopedPort;

  explicit MessageChannel(const char* aName, IToplevelProtocol* aListener);
  ~MessageChannel();

  IToplevelProtocol* Listener() const { return mListener; }

  // Returns the event target which the worker lives on and must be used for
  // operations on the current thread. Only safe to access after the
  // MessageChannel has been opened.
  nsISerialEventTarget* GetWorkerEventTarget() const { return mWorkerThread; }

  // "Open" a connection using an existing ScopedPort. The ScopedPort must be
  // valid and connected to a remote.
  //
  // The `aEventTarget` parameter must be on the current thread.
  bool Open(ScopedPort aPort, Side aSide,
            nsISerialEventTarget* aEventTarget = nullptr);

  // "Open" a connection to another thread in the same process.
  //
  // Returns true if the transport layer was successfully connected,
  // i.e., mChannelState == ChannelConnected.
  //
  // For more details on the process of opening a channel between
  // threads, see the extended comment on this function
  // in MessageChannel.cpp.
  bool Open(MessageChannel* aTargetChan, nsISerialEventTarget* aEventTarget,
            Side aSide);

  // "Open" a connection to an actor on the current thread.
  //
  // Returns true if the transport layer was successfully connected,
  // i.e., mChannelState == ChannelConnected.
  //
  // Same-thread channels may not perform synchronous or blocking message
  // sends, to avoid deadlocks.
  bool OpenOnSameThread(MessageChannel* aTargetChan, Side aSide);

  /**
   * This sends a special message that is processed on the IO thread, so that
   * other actors can know that the process will soon shutdown.
   */
  void NotifyImpendingShutdown();

  // Close the underlying transport channel.
  void Close();

  // Force the channel to behave as if a channel error occurred. Valid
  // for process links only, not thread links.
  void CloseWithError();

  void CloseWithTimeout();

  void SetAbortOnError(bool abort) { mAbortOnError = abort; }

  // Call aInvoke for each pending message until it returns false.
  // XXX: You must get permission from an IPC peer to use this function
  //      since it requires custom deserialization and re-orders events.
  void PeekMessages(const std::function<bool(const Message& aMsg)>& aInvoke);

  // Misc. behavioral traits consumers can request for this channel
  enum ChannelFlags {
    REQUIRE_DEFAULT = 0,
    // Windows: if this channel operates on the UI thread, indicates
    // WindowsMessageLoop code should enable deferred native message
    // handling to prevent deadlocks. Should only be used for protocols
    // that manage child processes which might create native UI, like
    // plugins.
    REQUIRE_DEFERRED_MESSAGE_PROTECTION = 1 << 0,
    // Windows: When this flag is specified, any wait that occurs during
    // synchronous IPC will be alertable, thus allowing a11y code in the
    // chrome process to reenter content while content is waiting on a
    // synchronous call.
    REQUIRE_A11Y_REENTRY = 1 << 1,
  };
  void SetChannelFlags(ChannelFlags aFlags) { mFlags = aFlags; }
  ChannelFlags GetChannelFlags() { return mFlags; }

  // Asynchronously send a message to the other side of the channel
  bool Send(UniquePtr<Message> aMsg);

  // Asynchronously send a message to the other side of the channel
  // and wait for asynchronous reply.
  template <typename Value>
  void Send(UniquePtr<Message> aMsg, ActorIdType aActorId,
            ResolveCallback<Value>&& aResolve, RejectCallback&& aReject) {
    int32_t seqno = NextSeqno();
    aMsg->set_seqno(seqno);
    if (!Send(std::move(aMsg))) {
      aReject(ResponseRejectReason::SendError);
      return;
    }

    UniquePtr<UntypedCallbackHolder> callback =
        MakeUnique<CallbackHolder<Value>>(aActorId, std::move(aResolve),
                                          std::move(aReject));
    mPendingResponses.insert(std::make_pair(seqno, std::move(callback)));
    gUnresolvedResponses++;
  }

  bool SendBuildIDsMatchMessage(const char* aParentBuildI);
  bool DoBuildIDsMatch() { return mBuildIDsConfirmedMatch; }

  // Synchronously send |msg| (i.e., wait for |reply|)
  bool Send(UniquePtr<Message> aMsg, Message* aReply);

  bool CanSend() const;

  // Remove and return a callback that needs reply
  UniquePtr<UntypedCallbackHolder> PopCallback(const Message& aMsg);

  // Used to reject and remove pending responses owned by the given
  // actor when it's about to be destroyed.
  void RejectPendingResponsesForActor(ActorIdType aActorId);

  // If sending a sync message returns an error, this function gives a more
  // descriptive error message.
  SyncSendError LastSendError() const {
    AssertWorkerThread();
    return mLastSendError;
  }

  void SetReplyTimeoutMs(int32_t aTimeoutMs);

  bool IsOnCxxStack() const { return mOnCxxStack; }

  void CancelCurrentTransaction();

  // Force all calls to Send to defer actually sending messages. This will
  // cause sync messages to block until another thread calls
  // StopPostponingSends.
  //
  // This must be called from the worker thread.
  void BeginPostponingSends();

  // Stop postponing sent messages, and immediately flush all postponed
  // messages to the link. This may be called from any thread.
  //
  // Note that there are no ordering guarantees between two different
  // MessageChannels. If channel B sends a message, then stops postponing
  // channel A, messages from A may arrive before B. The easiest way to order
  // this, if needed, is to make B send a sync message.
  void StopPostponingSends();

  // IsClosed and NumQueuedMessages are safe to call from any thread, but
  // may provide an out-of-date value.
  bool IsClosed() {
    MonitorAutoLock lock(*mMonitor);
    return IsClosedLocked();
  }
  bool IsClosedLocked() const {
    mMonitor->AssertCurrentThreadOwns();
    return mLink ? mLink->IsClosed() : true;
  }

  static bool IsPumpingMessages() { return sIsPumpingMessages; }
  static void SetIsPumpingMessages(bool aIsPumping) {
    sIsPumpingMessages = aIsPumping;
  }

  /**
   * Does this MessageChannel currently cross process boundaries?
   */
  bool IsCrossProcess() const;
  void SetIsCrossProcess(bool aIsCrossProcess);

#ifdef OS_WIN
  struct MOZ_STACK_CLASS SyncStackFrame {
    explicit SyncStackFrame(MessageChannel* channel);
    ~SyncStackFrame();

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
      if (frame->mSpinNestedEvents) return true;
    }
    return false;
  }

 protected:
  // The deepest sync stack frame for this channel.
  SyncStackFrame* mTopFrame = nullptr;

  bool mIsSyncWaitingOnNonMainThread = false;

  // The deepest sync stack frame on any channel.
  static SyncStackFrame* sStaticTopFrame;

 public:
  void ProcessNativeEventsInInterruptCall();
  static void NotifyGeckoEventDispatch();

 private:
  void SpinInternalEventLoop();
#  if defined(ACCESSIBILITY)
  bool WaitForSyncNotifyWithA11yReentry();
#  endif  // defined(ACCESSIBILITY)
#endif    // defined(OS_WIN)

 private:
  void PostErrorNotifyTask();
  void OnNotifyMaybeChannelError();
  void ReportConnectionError(const char* aFunctionName,
                             const uint32_t aMsgTyp) const;
  void ReportMessageRouteError(const char* channelName) const;
  bool MaybeHandleError(Result code, const Message& aMsg,
                        const char* channelName);

  void Clear();

  bool HasPendingEvents();

  void ProcessPendingRequests(ActorLifecycleProxy* aProxy,
                              AutoEnterTransaction& aTransaction);
  bool ProcessPendingRequest(ActorLifecycleProxy* aProxy, Message&& aUrgent);

  void EnqueuePendingMessages();

  // Dispatches an incoming message to its appropriate handler.
  void DispatchMessage(ActorLifecycleProxy* aProxy, Message&& aMsg);

  // DispatchMessage will route to one of these functions depending on the
  // protocol type of the message.
  void DispatchSyncMessage(ActorLifecycleProxy* aProxy, const Message& aMsg,
                           Message*& aReply);
  void DispatchAsyncMessage(ActorLifecycleProxy* aProxy, const Message& aMsg);

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

  bool WaitResponse(bool aWaitTimedOut);

  bool ShouldContinueFromTimeout();

  void EndTimeout();
  void CancelTransaction(int transaction);

  void RepostAllMessages();

  int32_t NextSeqno() {
    AssertWorkerThread();
    return (mSide == ChildSide) ? --mNextSeqno : ++mNextSeqno;
  }

  void DebugAbort(const char* file, int line, const char* cond, const char* why,
                  bool reply = false);

  void AddProfilerMarker(const IPC::Message& aMessage,
                         MessageDirection aDirection);

 private:
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

  // Tell the IO thread to close the channel and wait for it to ACK.
  void SynchronouslyClose();

  // Returns true if ShouldDeferMessage(aMsg) is guaranteed to return true.
  // Otherwise, the result of ShouldDeferMessage(aMsg) may be true or false,
  // depending on context.
  static bool IsAlwaysDeferred(const Message& aMsg);

  // Helper for sending a message via the link. This should only be used for
  // non-special messages that might have to be postponed.
  void SendMessageToLink(UniquePtr<Message> aMsg);

  bool WasTransactionCanceled(int transaction);
  bool ShouldDeferMessage(const Message& aMsg);
  void OnMessageReceivedFromLink(Message&& aMsg);
  void OnChannelErrorFromLink();

 private:
  // Clear this channel, and notify the listener that the channel has either
  // closed or errored.
  //
  // These methods must be called on the worker thread, passing in a
  // `Maybe<MonitorAutoLock>`. This lock guard will be reset before the listener
  // is called, allowing for the mutex to be unlocked before the MessageChannel
  // is potentially destroyed.
  void NotifyChannelClosed(Maybe<MonitorAutoLock>& aLock);
  void NotifyMaybeChannelError(Maybe<MonitorAutoLock>& aLock);

 private:
  void AssertWorkerThread() const {
    MOZ_ASSERT(mWorkerThread, "Channel hasn't been opened yet");
    MOZ_RELEASE_ASSERT(mWorkerThread && mWorkerThread->IsOnCurrentThread(),
                       "not on worker thread!");
  }

 private:
  class MessageTask : public CancelableRunnable,
                      public LinkedListElement<RefPtr<MessageTask>>,
                      public nsIRunnablePriority,
                      public nsIRunnableIPCMessageType {
   public:
    explicit MessageTask(MessageChannel* aChannel, Message&& aMessage);
    MessageTask() = delete;
    MessageTask(const MessageTask&) = delete;

    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD Run() override;
    nsresult Cancel() override;
    NS_IMETHOD GetPriority(uint32_t* aPriority) override;
    NS_DECL_NSIRUNNABLEIPCMESSAGETYPE
    void Post();

    bool IsScheduled() const {
      mMonitor->AssertCurrentThreadOwns();
      return mScheduled;
    }

    Message& Msg() { return mMessage; }
    const Message& Msg() const { return mMessage; }

   private:
    ~MessageTask() = default;

    MessageChannel* Channel() {
      mMonitor->AssertCurrentThreadOwns();
      MOZ_RELEASE_ASSERT(isInList());
      return mChannel;
    }

    // The connected MessageChannel's monitor. Guards `mChannel` and
    // `mScheduled`.
    RefPtr<RefCountedMonitor> const mMonitor;
    // The channel which this MessageTask is associated with. Only valid while
    // `mMonitor` is held, and this MessageTask `isInList()`.
    MessageChannel* const mChannel;
    Message mMessage;
    bool mScheduled : 1;
  };

  bool ShouldRunMessage(const Message& aMsg);
  void RunMessage(ActorLifecycleProxy* aProxy, MessageTask& aTask);

  typedef LinkedList<RefPtr<MessageTask>> MessageQueue;
  typedef std::map<size_t, UniquePtr<UntypedCallbackHolder>> CallbackMap;
  typedef IPC::Message::msgid_t msgid_t;

 private:
  // This will be a string literal, so lifetime is not an issue.
  const char* const mName;

  // Based on presumption the listener owns and overlives the channel,
  // this is never nullified.
  IToplevelProtocol* const mListener;

  // This monitor guards all state in this MessageChannel, except where
  // otherwise noted. It is refcounted so a reference to it can be shared with
  // IPC listener objects which need to access weak references to this
  // `MessageChannel`.
  RefPtr<RefCountedMonitor> const mMonitor;

  ChannelState mChannelState = ChannelClosed;
  Side mSide = UnknownSide;
  bool mIsCrossProcess = false;
  UniquePtr<MessageLink> mLink;

  // NotifyMaybeChannelError runnable
  RefPtr<CancelableRunnable> mChannelErrorTask;

  // Thread we are allowed to send and receive on.  Set in Open(); never
  // changed, and we can only call Open() once.  We shouldn't be accessing
  // from multiple threads before Open().
  nsCOMPtr<nsISerialEventTarget> mWorkerThread;

  // Timeout periods are broken up in two to prevent system suspension from
  // triggering an abort. This method (called by WaitForEvent with a 'did
  // timeout' flag) decides if we should wait again for half of mTimeoutMs
  // or give up.
  // only accessed on WorkerThread
  int32_t mTimeoutMs = kNoTimeout;
  bool mInTimeoutSecondHalf = false;

  // Worker-thread only; sequence numbers for messages that require
  // replies.
  int32_t mNextSeqno = 0;

  static bool sIsPumpingMessages;

  // If ::Send returns false, this gives a more descriptive error.
  SyncSendError mLastSendError = SyncSendError::SendSuccess;

  template <class T>
  class AutoSetValue {
   public:
    explicit AutoSetValue(T& var, const T& newValue)
        : mVar(var), mPrev(var), mNew(newValue) {
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

  bool mDispatchingAsyncMessage = false;
  int mDispatchingAsyncMessageNestedLevel = 0;

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
  AutoEnterTransaction* mTransactionStack = nullptr;

  int32_t CurrentNestedInsideSyncTransaction() const;

  bool AwaitingSyncReply() const;
  int AwaitingSyncReplyNestedLevel() const;

  bool DispatchingSyncMessage() const;
  int DispatchingSyncMessageNestedLevel() const;

#ifdef DEBUG
  void AssertMaybeDeferredCountCorrect();
#else
  void AssertMaybeDeferredCountCorrect() {}
#endif

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
  int32_t mTimedOutMessageSeqno = 0;
  int mTimedOutMessageNestedLevel = 0;

  // Queue of all incoming messages.
  //
  // If both this side and the other side are functioning correctly, the other
  // side can send as many async messages as it wants before sending us a
  // blocking message.  After sending a blocking message, the other side must be
  // blocked, and thus can't send us any more messages until we process the sync
  // in-msg.
  //
  MessageQueue mPending;

  // The number of messages in mPending for which IsAlwaysDeferred is false
  // (i.e., the number of messages that might not be deferred, depending on
  // context).
  size_t mMaybeDeferredPendingCount = 0;

  // Is there currently MessageChannel logic for this channel on the C++ stack?
  // This member is only accessed on the worker thread, and so is not protected
  // by mMonitor.
  bool mOnCxxStack = false;

  // Map of async Callbacks that are still waiting replies.
  CallbackMap mPendingResponses;

#ifdef OS_WIN
  HANDLE mEvent;
#endif

  // Should the channel abort the process from the I/O thread when
  // a channel error occurs?
  bool mAbortOnError = false;

  // True if the listener has already been notified of a channel close or
  // error.
  bool mNotifiedChannelDone = false;

  // See SetChannelFlags
  ChannelFlags mFlags = REQUIRE_DEFAULT;

  // Channels can enter messages are not sent immediately; instead, they are
  // held in a queue until another thread deems it is safe to send them.
  bool mIsPostponingSends = false;
  std::vector<UniquePtr<Message>> mPostponedSends;

  bool mBuildIDsConfirmedMatch = false;

  // If this is true, both ends of this message channel have event targets
  // on the same thread.
  bool mIsSameThreadChannel = false;
};

void CancelCPOWs();

}  // namespace ipc
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::ipc::ResponseRejectReason>
    : public ContiguousEnumSerializer<
          mozilla::ipc::ResponseRejectReason,
          mozilla::ipc::ResponseRejectReason::SendError,
          mozilla::ipc::ResponseRejectReason::EndGuard_> {};
}  // namespace IPC

namespace geckoprofiler::markers {

struct IPCMarker {
  static constexpr mozilla::Span<const char> MarkerTypeName() {
    return mozilla::MakeStringSpan("IPC");
  }
  static void StreamJSONMarkerData(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      mozilla::TimeStamp aStart, mozilla::TimeStamp aEnd, int32_t aOtherPid,
      int32_t aMessageSeqno, IPC::Message::msgid_t aMessageType,
      mozilla::ipc::Side aSide, mozilla::ipc::MessageDirection aDirection,
      mozilla::ipc::MessagePhase aPhase, bool aSync,
      mozilla::MarkerThreadId aOriginThreadId) {
    using namespace mozilla::ipc;
    // This payload still streams a startTime and endTime property because it
    // made the migration to MarkerTiming on the front-end easier.
    aWriter.TimeProperty("startTime", aStart);
    aWriter.TimeProperty("endTime", aEnd);

    aWriter.IntProperty("otherPid", aOtherPid);
    aWriter.IntProperty("messageSeqno", aMessageSeqno);
    aWriter.StringProperty(
        "messageType",
        mozilla::MakeStringSpan(IPC::StringFromIPCMessageType(aMessageType)));
    aWriter.StringProperty("side", IPCSideToString(aSide));
    aWriter.StringProperty("direction",
                           aDirection == MessageDirection::eSending
                               ? mozilla::MakeStringSpan("sending")
                               : mozilla::MakeStringSpan("receiving"));
    aWriter.StringProperty("phase", IPCPhaseToString(aPhase));
    aWriter.BoolProperty("sync", aSync);
    if (!aOriginThreadId.IsUnspecified()) {
      // Tech note: If `ToNumber()` returns a uint64_t, the conversion to
      // int64_t is "implementation-defined" before C++20. This is acceptable
      // here, because this is a one-way conversion to a unique identifier
      // that's used to visually separate data by thread on the front-end.
      aWriter.IntProperty(
          "threadId",
          static_cast<int64_t>(aOriginThreadId.ThreadId().ToNumber()));
    }
  }
  static mozilla::MarkerSchema MarkerTypeDisplay() {
    return mozilla::MarkerSchema::SpecialFrontendLocation{};
  }

 private:
  static mozilla::Span<const char> IPCSideToString(mozilla::ipc::Side aSide) {
    switch (aSide) {
      case mozilla::ipc::ParentSide:
        return mozilla::MakeStringSpan("parent");
      case mozilla::ipc::ChildSide:
        return mozilla::MakeStringSpan("child");
      case mozilla::ipc::UnknownSide:
        return mozilla::MakeStringSpan("unknown");
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC side");
        return mozilla::MakeStringSpan("<invalid IPC side>");
    }
  }

  static mozilla::Span<const char> IPCPhaseToString(
      mozilla::ipc::MessagePhase aPhase) {
    switch (aPhase) {
      case mozilla::ipc::MessagePhase::Endpoint:
        return mozilla::MakeStringSpan("endpoint");
      case mozilla::ipc::MessagePhase::TransferStart:
        return mozilla::MakeStringSpan("transferStart");
      case mozilla::ipc::MessagePhase::TransferEnd:
        return mozilla::MakeStringSpan("transferEnd");
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid IPC phase");
        return mozilla::MakeStringSpan("<invalid IPC phase>");
    }
  }
};

}  // namespace geckoprofiler::markers

#endif  // ifndef ipc_glue_MessageChannel_h
