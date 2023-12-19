/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProtocolUtils_h
#define mozilla_ipc_ProtocolUtils_h

#include <cstddef>
#include <cstdint>
#include <utility>
#include "IPCMessageStart.h"
#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/ipc_message.h"
#include "mojo/core/ports/port_ref.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/FunctionRef.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/MessageLink.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/ipc/Shmem.h"
#include "nsPrintfCString.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTHashSet.h"

// XXX Things that could be moved to ProtocolUtils.cpp
#include "base/process_util.h"  // for CloseProcessHandle
#include "prenv.h"              // for PR_GetEnv

#if defined(ANDROID) && defined(DEBUG)
#  include <android/log.h>
#endif

template <typename T>
class nsPtrHashKey;

// WARNING: this takes into account the private, special-message-type
// enum in ipc_channel.h.  They need to be kept in sync.
namespace {
// XXX the max message ID is actually kuint32max now ... when this
// changed, the assumptions of the special message IDs changed in that
// they're not carving out messages from likely-unallocated space, but
// rather carving out messages from the end of space allocated to
// protocol 0.  Oops!  We can get away with this until protocol 0
// starts approaching its 65,536th message.
enum {
  // Message types used by DataPipe
  DATA_PIPE_CLOSED_MESSAGE_TYPE = kuint16max - 18,
  DATA_PIPE_BYTES_CONSUMED_MESSAGE_TYPE = kuint16max - 17,

  // Message types used by NodeChannel
  ACCEPT_INVITE_MESSAGE_TYPE = kuint16max - 16,
  REQUEST_INTRODUCTION_MESSAGE_TYPE = kuint16max - 15,
  INTRODUCE_MESSAGE_TYPE = kuint16max - 14,
  BROADCAST_MESSAGE_TYPE = kuint16max - 13,
  EVENT_MESSAGE_TYPE = kuint16max - 12,

  // Message types used by MessageChannel
  MANAGED_ENDPOINT_DROPPED_MESSAGE_TYPE = kuint16max - 11,
  MANAGED_ENDPOINT_BOUND_MESSAGE_TYPE = kuint16max - 10,
  IMPENDING_SHUTDOWN_MESSAGE_TYPE = kuint16max - 9,
  BUILD_IDS_MATCH_MESSAGE_TYPE = kuint16max - 8,
  BUILD_ID_MESSAGE_TYPE = kuint16max - 7,  // unused
  CHANNEL_OPENED_MESSAGE_TYPE = kuint16max - 6,
  SHMEM_DESTROYED_MESSAGE_TYPE = kuint16max - 5,
  SHMEM_CREATED_MESSAGE_TYPE = kuint16max - 4,
  GOODBYE_MESSAGE_TYPE = kuint16max - 3,
  CANCEL_MESSAGE_TYPE = kuint16max - 2,

  // kuint16max - 1 is used by ipc_channel.h.
};

}  // namespace

class MessageLoop;
class PickleIterator;
class nsISerialEventTarget;

namespace mozilla {
class SchedulerGroup;

namespace dom {
class ContentParent;
}  // namespace dom

namespace net {
class NeckoParent;
}  // namespace net

namespace ipc {

// Scoped base::ProcessHandle to ensure base::CloseProcessHandle is called.
struct ScopedProcessHandleTraits {
  typedef base::ProcessHandle type;

  static type empty() { return base::kInvalidProcessHandle; }

  static void release(type aProcessHandle) {
    if (aProcessHandle && aProcessHandle != base::kInvalidProcessHandle) {
      base::CloseProcessHandle(aProcessHandle);
    }
  }
};
typedef mozilla::Scoped<ScopedProcessHandleTraits> ScopedProcessHandle;

class ProtocolFdMapping;
class ProtocolCloneContext;

// Used to pass references to protocol actors across the wire.
// Actors created on the parent-side have a positive ID, and actors
// allocated on the child side have a negative ID.
struct ActorHandle {
  int mId;
};

// What happens if Interrupt calls race?
enum RacyInterruptPolicy { RIPError, RIPChildWins, RIPParentWins };

enum class LinkStatus : uint8_t {
  // The actor has not established a link yet, or the actor is no longer in use
  // by IPC, and its 'Dealloc' method has been called or is being called.
  //
  // NOTE: This state is used instead of an explicit `Freed` state when IPC no
  // longer holds references to the current actor as we currently re-open
  // existing actors. Once we fix these poorly behaved actors, this loopback
  // state can be split to have the final state not be the same as the initial
  // state.
  Inactive,

  // A live link is connected to the other side of this actor.
  Connected,

  // The link has begun being destroyed. Messages may still be received, but
  // cannot be sent. (exception: sync/intr replies may be sent while Doomed).
  Doomed,

  // The link has been destroyed, and messages will no longer be sent or
  // received.
  Destroyed,
};

typedef IPCMessageStart ProtocolId;

// Generated by IPDL compiler
const char* ProtocolIdToName(IPCMessageStart aId);

class IToplevelProtocol;
class ActorLifecycleProxy;
class WeakActorLifecycleProxy;
class IPDLResolverInner;
class UntypedManagedEndpoint;

class IProtocol : public HasResultCodes {
 public:
  enum ActorDestroyReason {
    FailedConstructor,
    Deletion,
    AncestorDeletion,
    NormalShutdown,
    AbnormalShutdown,
    ManagedEndpointDropped
  };

  typedef base::ProcessId ProcessId;
  typedef IPC::Message Message;

  IProtocol(ProtocolId aProtoId, Side aSide)
      : mId(0),
        mProtocolId(aProtoId),
        mSide(aSide),
        mLinkStatus(LinkStatus::Inactive),
        mLifecycleProxy(nullptr),
        mManager(nullptr),
        mToplevel(nullptr) {}

  IToplevelProtocol* ToplevelProtocol() { return mToplevel; }
  const IToplevelProtocol* ToplevelProtocol() const { return mToplevel; }

  // The following methods either directly forward to the toplevel protocol, or
  // almost directly do.
  int32_t Register(IProtocol* aRouted);
  int32_t RegisterID(IProtocol* aRouted, int32_t aId);
  IProtocol* Lookup(int32_t aId);
  void Unregister(int32_t aId);

  Shmem::SharedMemory* CreateSharedMemory(size_t aSize, bool aUnsafe,
                                          int32_t* aId);
  Shmem::SharedMemory* LookupSharedMemory(int32_t aId);
  bool IsTrackingSharedMemory(Shmem::SharedMemory* aSegment);
  bool DestroySharedMemory(Shmem& aShmem);

  MessageChannel* GetIPCChannel();
  const MessageChannel* GetIPCChannel() const;

  // Get the nsISerialEventTarget which all messages sent to this actor will be
  // processed on. Unless stated otherwise, all operations on IProtocol which
  // don't occur on this `nsISerialEventTarget` are unsafe.
  nsISerialEventTarget* GetActorEventTarget();

  // Actor lifecycle and other properties.
  ProtocolId GetProtocolId() const { return mProtocolId; }
  const char* GetProtocolName() const { return ProtocolIdToName(mProtocolId); }

  int32_t Id() const { return mId; }
  IProtocol* Manager() const { return mManager; }

  ActorLifecycleProxy* GetLifecycleProxy() { return mLifecycleProxy; }
  WeakActorLifecycleProxy* GetWeakLifecycleProxy();

  Side GetSide() const { return mSide; }
  bool CanSend() const { return mLinkStatus == LinkStatus::Connected; }
  bool CanRecv() const {
    return mLinkStatus == LinkStatus::Connected ||
           mLinkStatus == LinkStatus::Doomed;
  }

  // Remove or deallocate a managee given its type.
  virtual void RemoveManagee(int32_t, IProtocol*) = 0;
  virtual void DeallocManagee(int32_t, IProtocol*) = 0;

  Maybe<IProtocol*> ReadActor(IPC::MessageReader* aReader, bool aNullable,
                              const char* aActorDescription,
                              int32_t aProtocolTypeId);

  virtual Result OnMessageReceived(const Message& aMessage) = 0;
  virtual Result OnMessageReceived(const Message& aMessage,
                                   UniquePtr<Message>& aReply) = 0;
  virtual Result OnCallReceived(const Message& aMessage,
                                UniquePtr<Message>& aReply) = 0;
  bool AllocShmem(size_t aSize, Shmem* aOutMem);
  bool AllocUnsafeShmem(size_t aSize, Shmem* aOutMem);
  bool DeallocShmem(Shmem& aMem);

  void FatalError(const char* const aErrorMsg);
  virtual void HandleFatalError(const char* aErrorMsg);

 protected:
  virtual ~IProtocol();

  friend class IToplevelProtocol;
  friend class ActorLifecycleProxy;
  friend class IPDLResolverInner;
  friend class UntypedManagedEndpoint;

  void SetId(int32_t aId);

  // We have separate functions because the accessibility code manually
  // calls SetManager.
  void SetManager(IProtocol* aManager);

  // Sets the manager for the protocol and registers the protocol with
  // its manager, setting up channels for the protocol as well.  Not
  // for use outside of IPDL.
  void SetManagerAndRegister(IProtocol* aManager);
  void SetManagerAndRegister(IProtocol* aManager, int32_t aId);

  // Helpers for calling `Send` on our underlying IPC channel.
  bool ChannelSend(UniquePtr<IPC::Message> aMsg);
  bool ChannelSend(UniquePtr<IPC::Message> aMsg,
                   UniquePtr<IPC::Message>* aReply);
  template <typename Value>
  void ChannelSend(UniquePtr<IPC::Message> aMsg,
                   IPC::Message::msgid_t aReplyMsgId,
                   ResolveCallback<Value>&& aResolve,
                   RejectCallback&& aReject) {
    if (CanSend()) {
      GetIPCChannel()->Send(std::move(aMsg), Id(), aReplyMsgId,
                            std::move(aResolve), std::move(aReject));
    } else {
      WarnMessageDiscarded(aMsg.get());
      aReject(ResponseRejectReason::SendError);
    }
  }

  // Collect all actors managed by this object in an array. To make this safer
  // to iterate, `ActorLifecycleProxy` references are returned rather than raw
  // actor pointers.
  virtual void AllManagedActors(
      nsTArray<RefPtr<ActorLifecycleProxy>>& aActors) const = 0;

  virtual uint32_t AllManagedActorsCount() const = 0;

  // Internal method called when the actor becomes connected.
  void ActorConnected();

  // Called immediately before setting the actor state to doomed, and triggering
  // async actor destruction. Messages may be sent from this callback, but no
  // later.
  // FIXME(nika): This is currently unused!
  virtual void ActorDoom() {}
  void DoomSubtree();

  // Called when the actor has been destroyed due to an error, a __delete__
  // message, or a __doom__ reply.
  virtual void ActorDestroy(ActorDestroyReason aWhy) {}
  void DestroySubtree(ActorDestroyReason aWhy);

  // Called when IPC has acquired its first reference to the actor. This method
  // may take references which will later be freed by `ActorDealloc`.
  virtual void ActorAlloc() {}

  // Called when IPC has released its final reference to the actor. It will call
  // the dealloc method, causing the actor to be actually freed.
  //
  // The actor has been freed after this method returns.
  virtual void ActorDealloc() {
    if (Manager()) {
      Manager()->DeallocManagee(mProtocolId, this);
    }
  }

  static const int32_t kNullActorId = 0;
  static const int32_t kFreedActorId = 1;

 private:
#ifdef DEBUG
  void WarnMessageDiscarded(IPC::Message* aMsg);
#else
  void WarnMessageDiscarded(IPC::Message*) {}
#endif

  int32_t mId;
  ProtocolId mProtocolId;
  Side mSide;
  LinkStatus mLinkStatus;
  ActorLifecycleProxy* mLifecycleProxy;
  IProtocol* mManager;
  IToplevelProtocol* mToplevel;
};

#define IPC_OK() mozilla::ipc::IPCResult::Ok()
#define IPC_FAIL(actor, why) \
  mozilla::ipc::IPCResult::Fail(WrapNotNull(actor), __func__, (why))
#define IPC_FAIL_NO_REASON(actor) \
  mozilla::ipc::IPCResult::Fail(WrapNotNull(actor), __func__)

/*
 * IPC_FAIL_UNSAFE_PRINTF(actor, format, ...)
 *
 * Create a failure IPCResult with a dynamic reason-string.
 *
 * @note This macro causes data collection because IPC failure reasons may be
 * sent to crash-stats, where they are publicly visible. Firefox data stewards
 * must do data review on usages of this macro.
 */
#define IPC_FAIL_UNSAFE_PRINTF(actor, format, ...) \
  mozilla::ipc::IPCResult::FailUnsafePrintfImpl(   \
      WrapNotNull(actor), __func__, nsPrintfCString(format, ##__VA_ARGS__))

/**
 * All message deserializers and message handlers should return this type via
 * the above macros. We use a less generic name here to avoid conflict with
 * `mozilla::Result` because we have quite a few `using namespace mozilla::ipc;`
 * in the code base.
 *
 * Note that merely constructing a failure-result, whether directly or via the
 * IPC_FAIL macros, causes the associated error message to be processed
 * immediately.
 */
class IPCResult {
 public:
  static IPCResult Ok() { return IPCResult(true); }

  // IPC failure messages can sometimes end up in telemetry. As such, to avoid
  // accidentally exfiltrating sensitive information without a data review, we
  // require that they be constant strings.
  template <size_t N, size_t M>
  static IPCResult Fail(NotNull<IProtocol*> aActor, const char (&aWhere)[N],
                        const char (&aWhy)[M]) {
    return FailImpl(aActor, aWhere, aWhy);
  }
  template <size_t N>
  static IPCResult Fail(NotNull<IProtocol*> aActor, const char (&aWhere)[N]) {
    return FailImpl(aActor, aWhere, "");
  }

  MOZ_IMPLICIT operator bool() const { return mSuccess; }

  // Only used by IPC_FAIL_UNSAFE_PRINTF (q.v.). Do not call this directly. (Or
  // at least get data-review's approval if you do.)
  template <size_t N>
  static IPCResult FailUnsafePrintfImpl(NotNull<IProtocol*> aActor,
                                        const char (&aWhere)[N],
                                        nsPrintfCString const& aWhy) {
    return FailImpl(aActor, aWhere, aWhy.get());
  }

 private:
  static IPCResult FailImpl(NotNull<IProtocol*> aActor, const char* aWhere,
                            const char* aWhy);

  explicit IPCResult(bool aResult) : mSuccess(aResult) {}
  bool mSuccess;
};

class UntypedEndpoint;

template <class PFooSide>
class Endpoint;

template <class PFooSide>
class ManagedEndpoint;

/**
 * All top-level protocols should inherit this class.
 *
 * IToplevelProtocol tracks all top-level protocol actors created from
 * this protocol actor.
 */
class IToplevelProtocol : public IProtocol {
  template <class PFooSide>
  friend class Endpoint;

 protected:
  explicit IToplevelProtocol(const char* aName, ProtocolId aProtoId,
                             Side aSide);
  ~IToplevelProtocol() = default;

 public:
  // All top-level protocols are refcounted.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Shadow methods on IProtocol which are implemented directly on toplevel
  // actors.
  int32_t Register(IProtocol* aRouted);
  int32_t RegisterID(IProtocol* aRouted, int32_t aId);
  IProtocol* Lookup(int32_t aId);
  void Unregister(int32_t aId);

  Shmem::SharedMemory* CreateSharedMemory(size_t aSize, bool aUnsafe,
                                          int32_t* aId);
  Shmem::SharedMemory* LookupSharedMemory(int32_t aId);
  bool IsTrackingSharedMemory(Shmem::SharedMemory* aSegment);
  bool DestroySharedMemory(Shmem& aShmem);

  MessageChannel* GetIPCChannel() { return &mChannel; }
  const MessageChannel* GetIPCChannel() const { return &mChannel; }

  void SetOtherProcessId(base::ProcessId aOtherPid);

  virtual void OnChannelClose() = 0;
  virtual void OnChannelError() = 0;
  virtual void ProcessingError(Result aError, const char* aMsgName) {}

  bool Open(ScopedPort aPort, const nsID& aMessageChannelId,
            base::ProcessId aOtherPid,
            nsISerialEventTarget* aEventTarget = nullptr);

  bool Open(IToplevelProtocol* aTarget, nsISerialEventTarget* aEventTarget,
            mozilla::ipc::Side aSide = mozilla::ipc::UnknownSide);

  // Open a toplevel actor such that both ends of the actor's channel are on
  // the same thread. This method should be called on the thread to perform
  // the link.
  //
  // WARNING: Attempting to send a sync or intr message on the same thread
  // will crash.
  bool OpenOnSameThread(IToplevelProtocol* aTarget,
                        mozilla::ipc::Side aSide = mozilla::ipc::UnknownSide);

  /**
   * This sends a special message that is processed on the IO thread, so that
   * other actors can know that the process will soon shutdown.
   */
  void NotifyImpendingShutdown();

  void Close();

  void SetReplyTimeoutMs(int32_t aTimeoutMs);

  void DeallocShmems();
  bool ShmemCreated(const Message& aMsg);
  bool ShmemDestroyed(const Message& aMsg);

  virtual bool ShouldContinueFromReplyTimeout() { return false; }

  // WARNING: This function is called with the MessageChannel monitor held.
  virtual void IntentionalCrash() { MOZ_CRASH("Intentional IPDL crash"); }

  // The code here is only useful for fuzzing. It should not be used for any
  // other purpose.
#ifdef DEBUG
  // Returns true if we should simulate a timeout.
  // WARNING: This is a testing-only function that is called with the
  // MessageChannel monitor held. Don't do anything fancy here or we could
  // deadlock.
  virtual bool ArtificialTimeout() { return false; }

  // Returns true if we want to cause the worker thread to sleep with the
  // monitor unlocked.
  virtual bool NeedArtificialSleep() { return false; }

  // This function should be implemented to sleep for some amount of time on
  // the worker thread. Will only be called if NeedArtificialSleep() returns
  // true.
  virtual void ArtificialSleep() {}
#else
  bool ArtificialTimeout() { return false; }
  bool NeedArtificialSleep() { return false; }
  void ArtificialSleep() {}
#endif

  bool IsOnCxxStack() const;

  virtual void ProcessRemoteNativeEventsInInterruptCall() {}

  virtual void OnChannelReceivedMessage(const Message& aMsg) {}

  void OnIPCChannelOpened() { ActorConnected(); }

  base::ProcessId OtherPidMaybeInvalid() const { return mOtherPid; }

 private:
  int32_t NextId();

  template <class T>
  using IDMap = nsTHashMap<nsUint32HashKey, T>;

  base::ProcessId mOtherPid;

  // NOTE NOTE NOTE
  // Used to be on mState
  int32_t mLastLocalId;
  IDMap<IProtocol*> mActorMap;
  IDMap<Shmem::SharedMemory*> mShmemMap;

  MessageChannel mChannel;
};

class IShmemAllocator {
 public:
  virtual bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) = 0;
  virtual bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) = 0;
  virtual bool DeallocShmem(mozilla::ipc::Shmem& aShmem) = 0;
};

#define FORWARD_SHMEM_ALLOCATOR_TO(aImplClass)                             \
  virtual bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem)       \
      override {                                                           \
    return aImplClass::AllocShmem(aSize, aShmem);                          \
  }                                                                        \
  virtual bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) \
      override {                                                           \
    return aImplClass::AllocUnsafeShmem(aSize, aShmem);                    \
  }                                                                        \
  virtual bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override {        \
    return aImplClass::DeallocShmem(aShmem);                               \
  }

inline bool LoggingEnabled() {
#if defined(DEBUG) || defined(FUZZING)
  return !!PR_GetEnv("MOZ_IPC_MESSAGE_LOG");
#else
  return false;
#endif
}

#if defined(DEBUG) || defined(FUZZING)
bool LoggingEnabledFor(const char* aTopLevelProtocol, mozilla::ipc::Side aSide,
                       const char* aFilter);
#endif

inline bool LoggingEnabledFor(const char* aTopLevelProtocol,
                              mozilla::ipc::Side aSide) {
#if defined(DEBUG) || defined(FUZZING)
  return LoggingEnabledFor(aTopLevelProtocol, aSide,
                           PR_GetEnv("MOZ_IPC_MESSAGE_LOG"));
#else
  return false;
#endif
}

MOZ_NEVER_INLINE void LogMessageForProtocol(const char* aTopLevelProtocol,
                                            base::ProcessId aOtherPid,
                                            const char* aContextDescription,
                                            uint32_t aMessageId,
                                            MessageDirection aDirection);

MOZ_NEVER_INLINE void ProtocolErrorBreakpoint(const char* aMsg);

// IPC::MessageReader and IPC::MessageWriter call this function for FatalError
// calls which come from serialization/deserialization.
MOZ_NEVER_INLINE void PickleFatalError(const char* aMsg, IProtocol* aActor);

// The code generator calls this function for errors which come from the
// methods of protocols.  Doing this saves codesize by making the error
// cases significantly smaller.
MOZ_NEVER_INLINE void FatalError(const char* aMsg, bool aIsParent);

// The code generator calls this function for errors which are not
// protocol-specific: errors in generated struct methods or errors in
// transition functions, for instance.  Doing this saves codesize by
// by making the error cases significantly smaller.
MOZ_NEVER_INLINE void LogicError(const char* aMsg);

MOZ_NEVER_INLINE void ActorIdReadError(const char* aActorDescription);

MOZ_NEVER_INLINE void BadActorIdError(const char* aActorDescription);

MOZ_NEVER_INLINE void ActorLookupError(const char* aActorDescription);

MOZ_NEVER_INLINE void MismatchedActorTypeError(const char* aActorDescription);

MOZ_NEVER_INLINE void UnionTypeReadError(const char* aUnionName);

MOZ_NEVER_INLINE void ArrayLengthReadError(const char* aElementName);

MOZ_NEVER_INLINE void SentinelReadError(const char* aElementName);

/**
 * Annotate the crash reporter with the error code from the most recent system
 * call. Returns the system error.
 */
void AnnotateSystemError();

// The ActorLifecycleProxy is a helper type used internally by IPC to maintain a
// maybe-owning reference to an IProtocol object. For well-behaved actors
// which are not freed until after their `Dealloc` method is called, a
// reference to an actor's `ActorLifecycleProxy` object is an owning one, as the
// `Dealloc` method will only be called when all references to the
// `ActorLifecycleProxy` are released.
//
// Unfortunately, some actors may be destroyed before their `Dealloc` method
// is called. For these actors, `ActorLifecycleProxy` acts as a weak pointer,
// and will begin to return `nullptr` from its `Get()` method once the
// corresponding actor object has been destroyed.
//
// When calling a `Recv` method, IPC will hold a `ActorLifecycleProxy` reference
// to the target actor, meaning that well-behaved actors can behave as though a
// strong reference is being held.
//
// Generic IPC code MUST treat ActorLifecycleProxy references as weak
// references!
class ActorLifecycleProxy {
 public:
  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(ActorLifecycleProxy)

  IProtocol* Get() { return mActor; }

  WeakActorLifecycleProxy* GetWeakProxy();

 private:
  friend class IProtocol;

  explicit ActorLifecycleProxy(IProtocol* aActor);
  ~ActorLifecycleProxy();

  ActorLifecycleProxy(const ActorLifecycleProxy&) = delete;
  ActorLifecycleProxy& operator=(const ActorLifecycleProxy&) = delete;

  IProtocol* MOZ_NON_OWNING_REF mActor;

  // Hold a reference to the actor's manager's ActorLifecycleProxy to help
  // prevent it from dying while we're still alive!
  RefPtr<ActorLifecycleProxy> mManager;

  // When requested, the current self-referencing weak reference for this
  // ActorLifecycleProxy.
  RefPtr<WeakActorLifecycleProxy> mWeakProxy;
};

// Unlike ActorLifecycleProxy, WeakActorLifecycleProxy only holds a weak
// reference to both the proxy and the actual actor, meaning that holding this
// type will not attempt to keep the actor object alive.
//
// This type is safe to hold on threads other than the actor's thread, but is
// _NOT_ safe to access on other threads, as actors and ActorLifecycleProxy
// objects are not threadsafe.
class WeakActorLifecycleProxy final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WeakActorLifecycleProxy)

  // May only be called on the actor's event target.
  // Will return `nullptr` if the actor has already been destroyed from IPC's
  // point of view.
  IProtocol* Get() const;

  // Safe to call on any thread.
  nsISerialEventTarget* ActorEventTarget() const { return mActorEventTarget; }

 private:
  friend class ActorLifecycleProxy;

  explicit WeakActorLifecycleProxy(ActorLifecycleProxy* aProxy);
  ~WeakActorLifecycleProxy();

  WeakActorLifecycleProxy(const WeakActorLifecycleProxy&) = delete;
  WeakActorLifecycleProxy& operator=(const WeakActorLifecycleProxy&) = delete;

  // This field may only be accessed on the actor's thread, and will be
  // automatically cleared when the ActorLifecycleProxy is destroyed.
  ActorLifecycleProxy* MOZ_NON_OWNING_REF mProxy;

  // The serial event target which owns the actor, and is the only thread where
  // it is OK to access the ActorLifecycleProxy.
  const nsCOMPtr<nsISerialEventTarget> mActorEventTarget;
};

class IPDLResolverInner final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(IPDLResolverInner,
                                                     Destroy())

  explicit IPDLResolverInner(UniquePtr<IPC::Message> aReply, IProtocol* aActor);

  template <typename F>
  void Resolve(F&& aWrite) {
    ResolveOrReject(true, aWrite);
  }

 private:
  void ResolveOrReject(bool aResolve,
                       FunctionRef<void(IPC::Message*, IProtocol*)> aWrite);

  void Destroy();
  ~IPDLResolverInner();

  UniquePtr<IPC::Message> mReply;
  RefPtr<WeakActorLifecycleProxy> mWeakProxy;
};

}  // namespace ipc

template <typename Protocol>
class ManagedContainer {
 public:
  using iterator = typename nsTArray<Protocol*>::const_iterator;

  iterator begin() const { return mArray.begin(); }
  iterator end() const { return mArray.end(); }
  iterator cbegin() const { return begin(); }
  iterator cend() const { return end(); }

  bool IsEmpty() const { return mArray.IsEmpty(); }
  uint32_t Count() const { return mArray.Length(); }

  void ToArray(nsTArray<Protocol*>& aArray) const {
    aArray.AppendElements(mArray);
  }

  bool EnsureRemoved(Protocol* aElement) {
    return mArray.RemoveElementSorted(aElement);
  }

  void Insert(Protocol* aElement) {
    // Equivalent to `InsertElementSorted`, avoiding inserting a duplicate
    // element.
    size_t index = mArray.IndexOfFirstElementGt(aElement);
    if (index == 0 || mArray[index - 1] != aElement) {
      mArray.InsertElementAt(index, aElement);
    }
  }

  void Clear() { mArray.Clear(); }

 private:
  nsTArray<Protocol*> mArray;
};

template <typename Protocol>
Protocol* LoneManagedOrNullAsserts(
    const ManagedContainer<Protocol>& aManagees) {
  if (aManagees.IsEmpty()) {
    return nullptr;
  }
  MOZ_ASSERT(aManagees.Count() == 1);
  return *aManagees.cbegin();
}

template <typename Protocol>
Protocol* SingleManagedOrNull(const ManagedContainer<Protocol>& aManagees) {
  if (aManagees.Count() != 1) {
    return nullptr;
  }
  return *aManagees.cbegin();
}

}  // namespace mozilla

#endif  // mozilla_ipc_ProtocolUtils_h
