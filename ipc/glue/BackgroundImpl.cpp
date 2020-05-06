/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChild.h"
#include "BackgroundParent.h"

#include "BackgroundChildImpl.h"
#include "BackgroundParentImpl.h"
#include "base/process_util.h"
#include "base/task.h"
#include "FileDescriptor.h"
#include "GeckoProfiler.h"
#include "InputStreamUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/ipc/ProtocolTypes.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessBridgeChild.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsISupportsImpl.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsXULAppAPI.h"
#include "nsXPCOMPrivate.h"
#include "prthread.h"

#include <functional>

#ifdef RELEASE_OR_BETA
#  define THREADSAFETY_ASSERT MOZ_ASSERT
#else
#  define THREADSAFETY_ASSERT MOZ_RELEASE_ASSERT
#endif

#define CRASH_IN_CHILD_PROCESS(_msg) \
  do {                               \
    if (XRE_IsParentProcess()) {     \
      MOZ_ASSERT(false, _msg);       \
    } else {                         \
      MOZ_CRASH(_msg);               \
    }                                \
  } while (0)

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::net;

namespace {

class ChildImpl;

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

void AssertIsInMainProcess() { MOZ_ASSERT(XRE_IsParentProcess()); }

void AssertIsInMainOrSocketProcess() {
  MOZ_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess());
}

void AssertIsOnMainThread() { THREADSAFETY_ASSERT(NS_IsMainThread()); }

void AssertIsNotOnMainThread() { THREADSAFETY_ASSERT(!NS_IsMainThread()); }

// -----------------------------------------------------------------------------
// ParentImpl Declaration
// -----------------------------------------------------------------------------

class ParentImpl final : public BackgroundParentImpl {
  friend class mozilla::ipc::BackgroundParent;

 public:
  class CreateCallback;

 private:
  class ShutdownObserver;
  class RequestMessageLoopRunnable;
  class ShutdownBackgroundThreadRunnable;
  class ForceCloseBackgroundActorsRunnable;
  class ConnectActorRunnable;
  class CreateActorHelper;

  struct MOZ_STACK_CLASS TimerCallbackClosure {
    nsIThread* mThread;
    nsTArray<ParentImpl*>* mLiveActors;

    TimerCallbackClosure(nsIThread* aThread, nsTArray<ParentImpl*>* aLiveActors)
        : mThread(aThread), mLiveActors(aLiveActors) {
      AssertIsInMainOrSocketProcess();
      AssertIsOnMainThread();
      MOZ_ASSERT(aThread);
      MOZ_ASSERT(aLiveActors);
    }
  };

  // The length of time we will wait at shutdown for all actors to clean
  // themselves up before forcing them to be destroyed.
  static const uint32_t kShutdownTimerDelayMS = 10000;

  // This is only modified on the main thread. It is null if the thread does not
  // exist or is shutting down.
  static StaticRefPtr<nsIThread> sBackgroundThread;

  // This is created and destroyed on the main thread but only modified on the
  // background thread. It is specific to each instance of sBackgroundThread.
  static nsTArray<ParentImpl*>* sLiveActorsForBackgroundThread;

  // This is only modified on the main thread.
  static StaticRefPtr<nsITimer> sShutdownTimer;

  // This exists so that that [Assert]IsOnBackgroundThread() can continue to
  // work during shutdown.
  static Atomic<PRThread*> sBackgroundPRThread;

  // This is only modified on the main thread. It is null if the thread does not
  // exist or is shutting down.
  static MessageLoop* sBackgroundThreadMessageLoop;

  // This is only modified on the main thread. It maintains a count of live
  // actors so that the background thread can be shut down when it is no longer
  // needed.
  static uint64_t sLiveActorCount;

  // This is only modified on the main thread. It is true after the shutdown
  // observer is registered and is never unset thereafter.
  static bool sShutdownObserverRegistered;

  // This is only modified on the main thread. It prevents us from trying to
  // create the background thread after application shutdown has started.
  static bool sShutdownHasStarted;

  // Only touched on the main thread, null if this is a same-process actor.
  RefPtr<ContentParent> mContent;

  // Set when the actor is opened successfully and used to handle shutdown
  // hangs. Only touched on the background thread.
  nsTArray<ParentImpl*>* mLiveActorArray;

  // Set at construction to indicate whether this parent actor corresponds to a
  // child actor in another process or to a child actor from a different thread
  // in the same process.
  const bool mIsOtherProcessActor;

  // Set after ActorDestroy has been called. Only touched on the background
  // thread.
  bool mActorDestroyed;

 public:
  static already_AddRefed<ChildImpl> CreateActorForSameProcess(
      nsIEventTarget* aMainEventTarget);

  static bool IsOnBackgroundThread() {
    return PR_GetCurrentThread() == sBackgroundPRThread;
  }

  static void AssertIsOnBackgroundThread() {
    THREADSAFETY_ASSERT(IsOnBackgroundThread());
  }

  NS_INLINE_DECL_REFCOUNTING(ParentImpl)

  void Destroy();

 private:
  // Forwarded from BackgroundParent.
  static bool IsOtherProcessActor(PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static already_AddRefed<ContentParent> GetContentParent(
      PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static intptr_t GetRawContentParentForComparison(
      PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static uint64_t GetChildID(PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static bool GetLiveActorArray(PBackgroundParent* aBackgroundActor,
                                nsTArray<PBackgroundParent*>& aLiveActorArray);

  // Forwarded from BackgroundParent.
  static bool Alloc(ContentParent* aContent,
                    Endpoint<PBackgroundParent>&& aEndpoint);

  static bool CreateBackgroundThread();

  static void ShutdownBackgroundThread();

  static void ShutdownTimerCallback(nsITimer* aTimer, void* aClosure);

  // For same-process actors.
  ParentImpl()
      : mLiveActorArray(nullptr),
        mIsOtherProcessActor(false),
        mActorDestroyed(false) {
    AssertIsInMainProcess();
    AssertIsOnMainThread();
  }

  // For other-process actors.
  // NOTE: ParentImpl could be used in 3 cases below.
  // 1. Between parent process and content process.
  // 2. Between socket process and content process.
  // 3. Between parent process and socket process.
  // |mContent| should be not null for case 1. For case 2 and 3, it's null.
  explicit ParentImpl(ContentParent* aContent)
      : mContent(aContent),
        mLiveActorArray(nullptr),
        mIsOtherProcessActor(true),
        mActorDestroyed(false) {
    MOZ_ASSERT(XRE_IsParentProcess() || XRE_IsSocketProcess());
    AssertIsOnMainThread();
  }

  ~ParentImpl() {
    AssertIsInMainOrSocketProcess();
    AssertIsOnMainThread();
    MOZ_ASSERT(!mContent);
  }

  void MainThreadActorDestroy();

  void SetLiveActorArray(nsTArray<ParentImpl*>* aLiveActorArray) {
    AssertIsInMainOrSocketProcess();
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aLiveActorArray);
    MOZ_ASSERT(!aLiveActorArray->Contains(this));
    MOZ_ASSERT(!mLiveActorArray);
    MOZ_ASSERT(mIsOtherProcessActor);

    mLiveActorArray = aLiveActorArray;
    mLiveActorArray->AppendElement(this);
  }

  // These methods are only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

// -----------------------------------------------------------------------------
// ChildImpl Declaration
// -----------------------------------------------------------------------------

class ChildImpl final : public BackgroundChildImpl {
  friend class mozilla::ipc::BackgroundChild;
  friend class mozilla::ipc::BackgroundChildImpl;

  typedef base::ProcessId ProcessId;
  typedef mozilla::ipc::Transport Transport;

  class ShutdownObserver;

 public:
  class SendInitBackgroundRunnable;

  struct ThreadLocalInfo {
    ThreadLocalInfo()
#ifdef DEBUG
        : mClosed(false)
#endif
    {
    }

    RefPtr<ChildImpl> mActor;
    RefPtr<SendInitBackgroundRunnable> mSendInitBackgroundRunnable;
    UniquePtr<BackgroundChildImpl::ThreadLocal> mConsumerThreadLocal;
#ifdef DEBUG
    bool mClosed;
#endif
  };

 private:
  // A thread-local index that is not valid.
  static constexpr unsigned int kBadThreadLocalIndex =
      static_cast<unsigned int>(-1);

  // ThreadInfoWrapper encapsulates ThreadLocalInfo and ThreadLocalIndex and
  // also provides some common functions for creating PBackground IPC actor.
  class ThreadInfoWrapper final {
    friend class ChildImpl;

   public:
    using ActorCreateFunc = void (*)(ThreadLocalInfo*, unsigned int,
                                     nsIEventTarget*, ChildImpl**);

    constexpr explicit ThreadInfoWrapper(ActorCreateFunc aFunc)
        : mThreadLocalIndex(kBadThreadLocalIndex),
          mMainThreadInfo(nullptr),
          mCreateActorFunc(aFunc) {}

    void Startup() {
      MOZ_ASSERT(mThreadLocalIndex == kBadThreadLocalIndex,
                 "ThreadInfoWrapper::Startup() called more than once!");

      PRStatus status =
          PR_NewThreadPrivateIndex(&mThreadLocalIndex, ThreadLocalDestructor);
      MOZ_RELEASE_ASSERT(status == PR_SUCCESS,
                         "PR_NewThreadPrivateIndex failed!");

      MOZ_ASSERT(mThreadLocalIndex != kBadThreadLocalIndex);
    }

    void Shutdown() {
      if (sShutdownHasStarted) {
        MOZ_ASSERT_IF(mThreadLocalIndex != kBadThreadLocalIndex,
                      !PR_GetThreadPrivate(mThreadLocalIndex));
        return;
      }

      if (mThreadLocalIndex == kBadThreadLocalIndex) {
        return;
      }

      ThreadLocalInfo* threadLocalInfo;
#ifdef DEBUG
      threadLocalInfo =
          static_cast<ThreadLocalInfo*>(PR_GetThreadPrivate(mThreadLocalIndex));
      MOZ_ASSERT(!threadLocalInfo);
#endif

      threadLocalInfo = mMainThreadInfo;
      if (threadLocalInfo) {
#ifdef DEBUG
        MOZ_ASSERT(!threadLocalInfo->mClosed);
        threadLocalInfo->mClosed = true;
#endif

        ThreadLocalDestructor(threadLocalInfo);
        mMainThreadInfo = nullptr;
      }
    }

    void CloseForCurrentThread() {
      MOZ_ASSERT(!NS_IsMainThread());

      if (mThreadLocalIndex == kBadThreadLocalIndex) {
        return;
      }

      auto threadLocalInfo =
          static_cast<ThreadLocalInfo*>(PR_GetThreadPrivate(mThreadLocalIndex));

      if (!threadLocalInfo) {
        return;
      }

#ifdef DEBUG
      MOZ_ASSERT(!threadLocalInfo->mClosed);
      threadLocalInfo->mClosed = true;
#endif

      // Clearing the thread local will synchronously close the actor.
      DebugOnly<PRStatus> status =
          PR_SetThreadPrivate(mThreadLocalIndex, nullptr);
      MOZ_ASSERT(status == PR_SUCCESS);
    }

    PBackgroundChild* GetOrCreateForCurrentThread(
        nsIEventTarget* aMainEventTarget) {
      MOZ_ASSERT_IF(NS_IsMainThread(), !aMainEventTarget);

      MOZ_ASSERT(mThreadLocalIndex != kBadThreadLocalIndex,
                 "BackgroundChild::Startup() was never called!");

      if (NS_IsMainThread() && ChildImpl::sShutdownHasStarted) {
        return nullptr;
      }

      auto threadLocalInfo = NS_IsMainThread()
                                 ? mMainThreadInfo
                                 : static_cast<ThreadLocalInfo*>(
                                       PR_GetThreadPrivate(mThreadLocalIndex));

      if (!threadLocalInfo) {
        auto newInfo = MakeUnique<ThreadLocalInfo>();

        if (NS_IsMainThread()) {
          mMainThreadInfo = newInfo.get();
        } else {
          if (PR_SetThreadPrivate(mThreadLocalIndex, newInfo.get()) !=
              PR_SUCCESS) {
            CRASH_IN_CHILD_PROCESS("PR_SetThreadPrivate failed!");
            return nullptr;
          }
        }

        threadLocalInfo = newInfo.release();
      }

      PBackgroundChild* bgChild =
          GetFromThreadInfo(aMainEventTarget, threadLocalInfo);
      if (bgChild) {
        return bgChild;
      }

      RefPtr<ChildImpl> actor;
      mCreateActorFunc(threadLocalInfo, mThreadLocalIndex, aMainEventTarget,
                       getter_AddRefs(actor));
      return actor;
    }

   private:
    // This is only modified on the main thread. It is the thread-local index
    // that we use to store the BackgroundChild for each thread.
    unsigned int mThreadLocalIndex;

    // On the main thread, we store TLS in this global instead of in
    // mThreadLocalIndex. That way, cooperative main threads all share the same
    // thread info.
    ThreadLocalInfo* mMainThreadInfo;
    ActorCreateFunc mCreateActorFunc;
  };

  // For PBackground between parent and content process.
  static ThreadInfoWrapper sParentAndContentProcessThreadInfo;

  // For PBackground between socket and content process.
  static ThreadInfoWrapper sSocketAndContentProcessThreadInfo;

  // For PBackground between socket and parent process.
  static ThreadInfoWrapper sSocketAndParentProcessThreadInfo;

  // This is only modified on the main thread. It prevents us from trying to
  // create the background thread after application shutdown has started.
  static bool sShutdownHasStarted;

#if defined(DEBUG) || !defined(RELEASE_OR_BETA)
  nsISerialEventTarget* mOwningEventTarget;
#endif

#ifdef DEBUG
  bool mActorWasAlive;
  bool mActorDestroyed;
#endif

 public:
  static void Shutdown();

  void AssertIsOnOwningThread() {
    THREADSAFETY_ASSERT(mOwningEventTarget);

#ifdef RELEASE_OR_BETA
    DebugOnly<bool> current;
#else
    bool current;
#endif
    THREADSAFETY_ASSERT(
        NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)));
    THREADSAFETY_ASSERT(current);
  }

  void AssertActorDestroyed() {
    MOZ_ASSERT(mActorDestroyed, "ChildImpl::ActorDestroy not called in time");
  }

  explicit ChildImpl()
#if defined(DEBUG) || !defined(RELEASE_OR_BETA)
      : mOwningEventTarget(GetCurrentThreadSerialEventTarget())
#endif
#ifdef DEBUG
        ,
        mActorWasAlive(false),
        mActorDestroyed(false)
#endif
  {
    AssertIsOnOwningThread();
  }

  void SetActorAlive() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(!mActorWasAlive);
    MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
    mActorWasAlive = true;
#endif
  }

  NS_INLINE_DECL_REFCOUNTING(ChildImpl)

 private:
  // Forwarded from BackgroundChild.
  static void Startup();

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetForCurrentThread();

  // Helper function for getting PBackgroundChild from thread info.
  static PBackgroundChild* GetFromThreadInfo(nsIEventTarget* aMainEventTarget,
                                             ThreadLocalInfo* aThreadLocalInfo);

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetOrCreateForCurrentThread(
      nsIEventTarget* aMainEventTarget);

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetOrCreateSocketActorForCurrentThread(
      nsIEventTarget* aMainEventTarget);

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetOrCreateForSocketParentBridgeForCurrentThread(
      nsIEventTarget* aMainEventTarget);

  static void CloseForCurrentThread();

  // Forwarded from BackgroundChildImpl.
  static BackgroundChildImpl::ThreadLocal* GetThreadLocalForCurrentThread();

  static void ThreadLocalDestructor(void* aThreadLocal);

  // This class is reference counted.
  ~ChildImpl() { MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed); }

  // Only called by IPDL.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

// -----------------------------------------------------------------------------
// ParentImpl Helper Declarations
// -----------------------------------------------------------------------------

class ParentImpl::ShutdownObserver final : public nsIObserver {
 public:
  ShutdownObserver() { AssertIsOnMainThread(); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  ~ShutdownObserver() { AssertIsOnMainThread(); }
};

class ParentImpl::RequestMessageLoopRunnable final : public Runnable {
  nsCOMPtr<nsIThread> mTargetThread;
  MessageLoop* mMessageLoop;

 public:
  explicit RequestMessageLoopRunnable(nsIThread* aTargetThread)
      : Runnable("Background::ParentImpl::RequestMessageLoopRunnable"),
        mTargetThread(aTargetThread),
        mMessageLoop(nullptr) {
    AssertIsInMainOrSocketProcess();
    AssertIsOnMainThread();
    MOZ_ASSERT(aTargetThread);
  }

 private:
  ~RequestMessageLoopRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class ParentImpl::ShutdownBackgroundThreadRunnable final : public Runnable {
 public:
  ShutdownBackgroundThreadRunnable()
      : Runnable("Background::ParentImpl::ShutdownBackgroundThreadRunnable") {
    AssertIsInMainOrSocketProcess();
    AssertIsOnMainThread();
  }

 private:
  ~ShutdownBackgroundThreadRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class ParentImpl::ForceCloseBackgroundActorsRunnable final : public Runnable {
  nsTArray<ParentImpl*>* mActorArray;

 public:
  explicit ForceCloseBackgroundActorsRunnable(
      nsTArray<ParentImpl*>* aActorArray)
      : Runnable("Background::ParentImpl::ForceCloseBackgroundActorsRunnable"),
        mActorArray(aActorArray) {
    AssertIsInMainOrSocketProcess();
    AssertIsOnMainThread();
    MOZ_ASSERT(aActorArray);
  }

 private:
  ~ForceCloseBackgroundActorsRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class ParentImpl::ConnectActorRunnable final : public Runnable {
  RefPtr<ParentImpl> mActor;
  Endpoint<PBackgroundParent> mEndpoint;
  nsTArray<ParentImpl*>* mLiveActorArray;

 public:
  ConnectActorRunnable(ParentImpl* aActor,
                       Endpoint<PBackgroundParent>&& aEndpoint,
                       nsTArray<ParentImpl*>* aLiveActorArray)
      : Runnable("Background::ParentImpl::ConnectActorRunnable"),
        mActor(aActor),
        mEndpoint(std::move(aEndpoint)),
        mLiveActorArray(aLiveActorArray) {
    AssertIsInMainOrSocketProcess();
    AssertIsOnMainThread();
    MOZ_ASSERT(mEndpoint.IsValid());
    MOZ_ASSERT(aLiveActorArray);
  }

 private:
  ~ConnectActorRunnable() { AssertIsInMainOrSocketProcess(); }

  NS_DECL_NSIRUNNABLE
};

class ParentImpl::CreateActorHelper final : public Runnable {
  mozilla::Monitor mMonitor;
  RefPtr<ParentImpl> mParentActor;
  nsCOMPtr<nsIThread> mThread;
  nsresult mMainThreadResultCode;
  bool mWaiting;

 public:
  explicit CreateActorHelper()
      : Runnable("Background::ParentImpl::CreateActorHelper"),
        mMonitor("CreateActorHelper::mMonitor"),
        mMainThreadResultCode(NS_OK),
        mWaiting(true) {
    AssertIsInMainOrSocketProcess();
    AssertIsNotOnMainThread();
  }

  nsresult BlockAndGetResults(nsIEventTarget* aMainEventTarget,
                              RefPtr<ParentImpl>& aParentActor,
                              nsCOMPtr<nsIThread>& aThread);

 private:
  ~CreateActorHelper() { AssertIsInMainOrSocketProcess(); }

  nsresult RunOnMainThread();

  NS_DECL_NSIRUNNABLE
};

class NS_NO_VTABLE ParentImpl::CreateCallback {
 public:
  NS_INLINE_DECL_REFCOUNTING(CreateCallback)

  virtual void Success(already_AddRefed<ParentImpl> aActor,
                       MessageLoop* aMessageLoop) = 0;

  virtual void Failure() = 0;

 protected:
  virtual ~CreateCallback() = default;
};

// -----------------------------------------------------------------------------
// ChildImpl Helper Declarations
// -----------------------------------------------------------------------------

class ChildImpl::ShutdownObserver final : public nsIObserver {
 public:
  ShutdownObserver() { AssertIsOnMainThread(); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  ~ShutdownObserver() { AssertIsOnMainThread(); }
};

class ChildImpl::SendInitBackgroundRunnable final : public CancelableRunnable {
  nsCOMPtr<nsISerialEventTarget> mOwningEventTarget;
  RefPtr<StrongWorkerRef> mWorkerRef;
  Endpoint<PBackgroundParent> mParent;
  mozilla::Mutex mMutex;
  bool mSentInitBackground;
  std::function<void(Endpoint<PBackgroundParent>&& aParent)> mSendInitfunc;
  unsigned int mThreadLocalIndex;

 public:
  static already_AddRefed<SendInitBackgroundRunnable> Create(
      Endpoint<PBackgroundParent>&& aParent,
      std::function<void(Endpoint<PBackgroundParent>&& aParent)>&& aFunc,
      unsigned int aThreadLocalIndex);

  void ClearEventTarget() {
    mWorkerRef = nullptr;

    mozilla::MutexAutoLock lock(mMutex);
    mOwningEventTarget = nullptr;
  }

 private:
  explicit SendInitBackgroundRunnable(
      Endpoint<PBackgroundParent>&& aParent,
      std::function<void(Endpoint<PBackgroundParent>&& aParent)>&& aFunc,
      unsigned int aThreadLocalIndex)
      : CancelableRunnable("Background::ChildImpl::SendInitBackgroundRunnable"),
        mOwningEventTarget(GetCurrentThreadSerialEventTarget()),
        mParent(std::move(aParent)),
        mMutex("SendInitBackgroundRunnable::mMutex"),
        mSentInitBackground(false),
        mSendInitfunc(std::move(aFunc)),
        mThreadLocalIndex(aThreadLocalIndex) {}

  ~SendInitBackgroundRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

}  // namespace

namespace mozilla {
namespace ipc {

bool IsOnBackgroundThread() { return ParentImpl::IsOnBackgroundThread(); }

#ifdef DEBUG

void AssertIsOnBackgroundThread() { ParentImpl::AssertIsOnBackgroundThread(); }

#endif  // DEBUG

}  // namespace ipc
}  // namespace mozilla

// -----------------------------------------------------------------------------
// BackgroundParent Public Methods
// -----------------------------------------------------------------------------

// static
bool BackgroundParent::IsOtherProcessActor(
    PBackgroundParent* aBackgroundActor) {
  return ParentImpl::IsOtherProcessActor(aBackgroundActor);
}

// static
already_AddRefed<ContentParent> BackgroundParent::GetContentParent(
    PBackgroundParent* aBackgroundActor) {
  return ParentImpl::GetContentParent(aBackgroundActor);
}

// static
intptr_t BackgroundParent::GetRawContentParentForComparison(
    PBackgroundParent* aBackgroundActor) {
  return ParentImpl::GetRawContentParentForComparison(aBackgroundActor);
}

// static
uint64_t BackgroundParent::GetChildID(PBackgroundParent* aBackgroundActor) {
  return ParentImpl::GetChildID(aBackgroundActor);
}

// static
bool BackgroundParent::GetLiveActorArray(
    PBackgroundParent* aBackgroundActor,
    nsTArray<PBackgroundParent*>& aLiveActorArray) {
  return ParentImpl::GetLiveActorArray(aBackgroundActor, aLiveActorArray);
}

// static
bool BackgroundParent::Alloc(ContentParent* aContent,
                             Endpoint<PBackgroundParent>&& aEndpoint) {
  return ParentImpl::Alloc(aContent, std::move(aEndpoint));
}

// -----------------------------------------------------------------------------
// BackgroundChild Public Methods
// -----------------------------------------------------------------------------

// static
void BackgroundChild::Startup() { ChildImpl::Startup(); }

// static
PBackgroundChild* BackgroundChild::GetForCurrentThread() {
  return ChildImpl::GetForCurrentThread();
}

// static
PBackgroundChild* BackgroundChild::GetOrCreateForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return ChildImpl::GetOrCreateForCurrentThread(aMainEventTarget);
}

// static
PBackgroundChild* BackgroundChild::GetOrCreateSocketActorForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return ChildImpl::GetOrCreateSocketActorForCurrentThread(aMainEventTarget);
}

// static
PBackgroundChild*
BackgroundChild::GetOrCreateForSocketParentBridgeForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return ChildImpl::GetOrCreateForSocketParentBridgeForCurrentThread(
      aMainEventTarget);
}

// static
void BackgroundChild::CloseForCurrentThread() {
  ChildImpl::CloseForCurrentThread();
}

// -----------------------------------------------------------------------------
// BackgroundChildImpl Public Methods
// -----------------------------------------------------------------------------

// static
BackgroundChildImpl::ThreadLocal*
BackgroundChildImpl::GetThreadLocalForCurrentThread() {
  return ChildImpl::GetThreadLocalForCurrentThread();
}

// -----------------------------------------------------------------------------
// ParentImpl Static Members
// -----------------------------------------------------------------------------

StaticRefPtr<nsIThread> ParentImpl::sBackgroundThread;

nsTArray<ParentImpl*>* ParentImpl::sLiveActorsForBackgroundThread;

StaticRefPtr<nsITimer> ParentImpl::sShutdownTimer;

Atomic<PRThread*> ParentImpl::sBackgroundPRThread;

MessageLoop* ParentImpl::sBackgroundThreadMessageLoop = nullptr;

uint64_t ParentImpl::sLiveActorCount = 0;

bool ParentImpl::sShutdownObserverRegistered = false;

bool ParentImpl::sShutdownHasStarted = false;

// -----------------------------------------------------------------------------
// ChildImpl Static Members
// -----------------------------------------------------------------------------

static void ParentContentActorCreateFunc(
    ChildImpl::ThreadLocalInfo* aThreadLocalInfo,
    unsigned int aThreadLocalIndex, nsIEventTarget* aMainEventTarget,
    ChildImpl** aOutput) {
  if (XRE_IsParentProcess()) {
    RefPtr<ChildImpl> strongActor =
        ParentImpl::CreateActorForSameProcess(aMainEventTarget);
    if (NS_WARN_IF(!strongActor)) {
      return;
    }

    aThreadLocalInfo->mActor = strongActor;
    strongActor.forget(aOutput);
    return;
  }

  RefPtr<ContentChild> content = ContentChild::GetSingleton();
  MOZ_ASSERT(content);

  if (content->IsShuttingDown()) {
    // The transport for ContentChild is shut down and can't be used to open
    // PBackground.
    return;
  }

  Endpoint<PBackgroundParent> parent;
  Endpoint<PBackgroundChild> child;
  nsresult rv;
  rv = PBackground::CreateEndpoints(content->OtherPid(),
                                    base::GetCurrentProcId(), &parent, &child);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create top level actor!");
    return;
  }

  RefPtr<ChildImpl::SendInitBackgroundRunnable> runnable;
  if (!NS_IsMainThread()) {
    runnable = ChildImpl::SendInitBackgroundRunnable::Create(
        std::move(parent),
        [](Endpoint<PBackgroundParent>&& aParent) {
          RefPtr<ContentChild> content = ContentChild::GetSingleton();
          MOZ_ASSERT(content);

          if (!content->SendInitBackground(std::move(aParent))) {
            NS_WARNING("Failed to create top level actor!");
          }
        },
        aThreadLocalIndex);
    if (!runnable) {
      return;
    }
  }

  RefPtr<ChildImpl> strongActor = new ChildImpl();

  if (!child.Bind(strongActor)) {
    CRASH_IN_CHILD_PROCESS("Failed to bind ChildImpl!");

    return;
  }

  strongActor->SetActorAlive();

  if (NS_IsMainThread()) {
    if (!content->SendInitBackground(std::move(parent))) {
      NS_WARNING("Failed to create top level actor!");
      return;
    }
  } else {
    if (aMainEventTarget) {
      MOZ_ALWAYS_SUCCEEDS(
          aMainEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));
    } else {
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
    }

    aThreadLocalInfo->mSendInitBackgroundRunnable = runnable;
  }

  aThreadLocalInfo->mActor = strongActor;
  strongActor.forget(aOutput);
}

ChildImpl::ThreadInfoWrapper ChildImpl::sParentAndContentProcessThreadInfo(
    ParentContentActorCreateFunc);

static void SocketContentActorCreateFunc(
    ChildImpl::ThreadLocalInfo* aThreadLocalInfo,
    unsigned int aThreadLocalIndex, nsIEventTarget* aMainEventTarget,
    ChildImpl** aOutput) {
  RefPtr<SocketProcessBridgeChild> bridgeChild =
      SocketProcessBridgeChild::GetSingleton();

  if (!bridgeChild || bridgeChild->IsShuttingDown()) {
    // The transport for SocketProcessBridgeChild is shut down
    // and can't be used to open PBackground.
    return;
  }

  Endpoint<PBackgroundParent> parent;
  Endpoint<PBackgroundChild> child;
  nsresult rv;
  rv = PBackground::CreateEndpoints(bridgeChild->SocketProcessPid(),
                                    base::GetCurrentProcId(), &parent, &child);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create top level actor!");
    return;
  }

  RefPtr<ChildImpl::SendInitBackgroundRunnable> runnable;
  if (!NS_IsMainThread()) {
    runnable = ChildImpl::SendInitBackgroundRunnable::Create(
        std::move(parent),
        [](Endpoint<PBackgroundParent>&& aParent) {
          RefPtr<SocketProcessBridgeChild> bridgeChild =
              SocketProcessBridgeChild::GetSingleton();

          if (!bridgeChild->SendInitBackground(std::move(aParent))) {
            NS_WARNING("Failed to create top level actor!");
          }
        },
        aThreadLocalIndex);
    if (!runnable) {
      return;
    }
  }

  RefPtr<ChildImpl> strongActor = new ChildImpl();

  if (!child.Bind(strongActor)) {
    CRASH_IN_CHILD_PROCESS("Failed to bind ChildImpl!");

    return;
  }

  strongActor->SetActorAlive();

  if (NS_IsMainThread()) {
    if (!bridgeChild->SendInitBackground(std::move(parent))) {
      NS_WARNING("Failed to create top level actor!");
      // Need to close the IPC channel before ChildImpl getting deleted.
      strongActor->Close();
      strongActor->AssertActorDestroyed();
      return;
    }
  } else {
    if (aMainEventTarget) {
      MOZ_ALWAYS_SUCCEEDS(
          aMainEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));
    } else {
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
    }

    aThreadLocalInfo->mSendInitBackgroundRunnable = runnable;
  }

  aThreadLocalInfo->mActor = strongActor;
  strongActor.forget(aOutput);
}

ChildImpl::ThreadInfoWrapper ChildImpl::sSocketAndContentProcessThreadInfo(
    SocketContentActorCreateFunc);

static void SocketParentActorCreateFunc(
    ChildImpl::ThreadLocalInfo* aThreadLocalInfo,
    unsigned int aThreadLocalIndex, nsIEventTarget* aMainEventTarget,
    ChildImpl** aOutput) {
  SocketProcessChild* socketChild = SocketProcessChild::GetSingleton();

  if (!socketChild || socketChild->IsShuttingDown()) {
    return;
  }

  Endpoint<PBackgroundParent> parent;
  Endpoint<PBackgroundChild> child;
  nsresult rv;
  rv = PBackground::CreateEndpoints(socketChild->OtherPid(),
                                    base::GetCurrentProcId(), &parent, &child);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create top level actor!");
    return;
  }

  RefPtr<ChildImpl::SendInitBackgroundRunnable> runnable;
  if (!NS_IsMainThread()) {
    runnable = ChildImpl::SendInitBackgroundRunnable::Create(
        std::move(parent),
        [](Endpoint<PBackgroundParent>&& aParent) {
          SocketProcessChild* socketChild = SocketProcessChild::GetSingleton();
          MOZ_ASSERT(socketChild);

          if (!socketChild->SendInitBackground(std::move(aParent))) {
            MOZ_CRASH("Failed to create top level actor!");
          }
        },
        aThreadLocalIndex);
    if (!runnable) {
      return;
    }
  }

  RefPtr<ChildImpl> strongActor = new ChildImpl();

  if (!child.Bind(strongActor)) {
    CRASH_IN_CHILD_PROCESS("Failed to bind ChildImpl!");
    return;
  }

  strongActor->SetActorAlive();

  if (NS_IsMainThread()) {
    if (!socketChild->SendInitBackground(std::move(parent))) {
      NS_WARNING("Failed to create top level actor!");
      return;
    }
  } else {
    if (aMainEventTarget) {
      MOZ_ALWAYS_SUCCEEDS(
          aMainEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));
    } else {
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));
    }

    aThreadLocalInfo->mSendInitBackgroundRunnable = runnable;
  }

  aThreadLocalInfo->mActor = strongActor;
  strongActor.forget(aOutput);
}

ChildImpl::ThreadInfoWrapper ChildImpl::sSocketAndParentProcessThreadInfo(
    SocketParentActorCreateFunc);

bool ChildImpl::sShutdownHasStarted = false;

// -----------------------------------------------------------------------------
// ParentImpl Implementation
// -----------------------------------------------------------------------------

// static
bool ParentImpl::IsOtherProcessActor(PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  return static_cast<ParentImpl*>(aBackgroundActor)->mIsOtherProcessActor;
}

// static
already_AddRefed<ContentParent> ParentImpl::GetContentParent(
    PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  auto actor = static_cast<ParentImpl*>(aBackgroundActor);
  if (actor->mActorDestroyed) {
    MOZ_ASSERT(false, "GetContentParent called after ActorDestroy was called!");
    return nullptr;
  }

  if (actor->mContent) {
    // We need to hand out a reference to our ContentParent but we also need to
    // keep the one we have. We can't call AddRef here because ContentParent is
    // not threadsafe so instead we dispatch a runnable to the main thread to do
    // it for us. This is safe since we are guaranteed that our AddRef runnable
    // will run before the reference we hand out can be released, and the
    // ContentParent can't die as long as the existing reference is maintained.
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NewNonOwningRunnableMethod(
        "ContentParent::AddRef", actor->mContent, &ContentParent::AddRef)));
  }

  return already_AddRefed<ContentParent>(actor->mContent.get());
}

// static
intptr_t ParentImpl::GetRawContentParentForComparison(
    PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  auto actor = static_cast<ParentImpl*>(aBackgroundActor);
  if (actor->mActorDestroyed) {
    MOZ_ASSERT(false,
               "GetRawContentParentForComparison called after ActorDestroy was "
               "called!");
    return intptr_t(-1);
  }

  return intptr_t(static_cast<ContentParent*>(actor->mContent.get()));
}

// static
uint64_t ParentImpl::GetChildID(PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  auto actor = static_cast<ParentImpl*>(aBackgroundActor);
  if (actor->mActorDestroyed) {
    MOZ_ASSERT(false, "GetContentParent called after ActorDestroy was called!");
    return 0;
  }

  if (actor->mContent) {
    return actor->mContent->ChildID();
  }

  return 0;
}

// static
bool ParentImpl::GetLiveActorArray(
    PBackgroundParent* aBackgroundActor,
    nsTArray<PBackgroundParent*>& aLiveActorArray) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(aLiveActorArray.IsEmpty());

  auto actor = static_cast<ParentImpl*>(aBackgroundActor);
  if (actor->mActorDestroyed) {
    MOZ_ASSERT(false,
               "GetLiveActorArray called after ActorDestroy was called!");
    return false;
  }

  if (!actor->mLiveActorArray) {
    return true;
  }

  for (ParentImpl* liveActor : *actor->mLiveActorArray) {
    aLiveActorArray.AppendElement(liveActor);
  }

  return true;
}

// static
bool ParentImpl::Alloc(ContentParent* aContent,
                       Endpoint<PBackgroundParent>&& aEndpoint) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(aEndpoint.IsValid());

  if (!sBackgroundThread && !CreateBackgroundThread()) {
    NS_WARNING("Failed to create background thread!");
    return false;
  }

  MOZ_ASSERT(sLiveActorsForBackgroundThread);

  sLiveActorCount++;

  RefPtr<ParentImpl> actor = new ParentImpl(aContent);

  nsCOMPtr<nsIRunnable> connectRunnable = new ConnectActorRunnable(
      actor, std::move(aEndpoint), sLiveActorsForBackgroundThread);

  if (NS_FAILED(
          sBackgroundThread->Dispatch(connectRunnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch connect runnable!");

    MOZ_ASSERT(sLiveActorCount);
    sLiveActorCount--;

    return false;
  }

  return true;
}

// static
already_AddRefed<ChildImpl> ParentImpl::CreateActorForSameProcess(
    nsIEventTarget* aMainEventTarget) {
  AssertIsInMainProcess();

  RefPtr<ParentImpl> parentActor;
  nsCOMPtr<nsIThread> backgroundThread;

  if (NS_IsMainThread()) {
    if (!sBackgroundThread && !CreateBackgroundThread()) {
      NS_WARNING("Failed to create background thread!");
      return nullptr;
    }

    MOZ_ASSERT(!sShutdownHasStarted);

    sLiveActorCount++;

    parentActor = new ParentImpl();
    backgroundThread = sBackgroundThread.get();
  } else {
    RefPtr<CreateActorHelper> helper = new CreateActorHelper();

    nsresult rv = helper->BlockAndGetResults(aMainEventTarget, parentActor,
                                             backgroundThread);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }
  }

  RefPtr<ChildImpl> childActor = new ChildImpl();

  MessageChannel* parentChannel = parentActor->GetIPCChannel();
  MOZ_ASSERT(parentChannel);

  if (!childActor->Open(parentChannel, backgroundThread, ChildSide)) {
    NS_WARNING("Failed to open ChildImpl!");

    // Can't release it here, we will release this reference in Destroy.
    ParentImpl* actor;
    parentActor.forget(&actor);

    actor->Destroy();

    return nullptr;
  }

  childActor->SetActorAlive();

  // Make sure the parent knows it is same process.
  parentActor->SetOtherProcessId(base::GetCurrentProcId());

  // Now that Open() has succeeded transfer the ownership of the actors to IPDL.
  Unused << parentActor.forget();

  return childActor.forget();
}

// static
bool ParentImpl::CreateBackgroundThread() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(!sBackgroundThread);
  MOZ_ASSERT(!sLiveActorsForBackgroundThread);

  if (sShutdownHasStarted) {
    NS_WARNING(
        "Trying to create background thread after shutdown has "
        "already begun!");
    return false;
  }

  nsCOMPtr<nsITimer> newShutdownTimer;

  if (!sShutdownTimer) {
    newShutdownTimer = NS_NewTimer();
    if (!newShutdownTimer) {
      return false;
    }
  }

  if (!sShutdownObserverRegistered) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return false;
    }

    nsCOMPtr<nsIObserver> observer = new ShutdownObserver();

    nsresult rv = obs->AddObserver(
        observer, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    sShutdownObserverRegistered = true;
  }

  nsCOMPtr<nsIThread> thread;
  if (NS_FAILED(NS_NewNamedThread("IPDL Background", getter_AddRefs(thread)))) {
    NS_WARNING("NS_NewNamedThread failed!");
    return false;
  }

  nsCOMPtr<nsIRunnable> messageLoopRunnable =
      new RequestMessageLoopRunnable(thread);
  if (NS_FAILED(thread->Dispatch(messageLoopRunnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch RequestMessageLoopRunnable!");
    return false;
  }

  sBackgroundThread = thread;
  sLiveActorsForBackgroundThread = new nsTArray<ParentImpl*>(1);

  if (!sShutdownTimer) {
    MOZ_ASSERT(newShutdownTimer);
    sShutdownTimer = newShutdownTimer;
  }

  return true;
}

// static
void ParentImpl::ShutdownBackgroundThread() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(sShutdownHasStarted);
  MOZ_ASSERT_IF(!sBackgroundThread, !sLiveActorCount);
  MOZ_ASSERT_IF(sBackgroundThread, sShutdownTimer);

  nsCOMPtr<nsITimer> shutdownTimer = sShutdownTimer.get();
  sShutdownTimer = nullptr;

  if (sBackgroundThread) {
    nsCOMPtr<nsIThread> thread = sBackgroundThread.get();
    sBackgroundThread = nullptr;

    UniquePtr<nsTArray<ParentImpl*>> liveActors(sLiveActorsForBackgroundThread);
    sLiveActorsForBackgroundThread = nullptr;

    MOZ_ASSERT_IF(!sShutdownHasStarted, !sLiveActorCount);

    if (sLiveActorCount) {
      // We need to spin the event loop while we wait for all the actors to be
      // cleaned up. We also set a timeout to force-kill any hanging actors.
      TimerCallbackClosure closure(thread, liveActors.get());

      MOZ_ALWAYS_SUCCEEDS(shutdownTimer->InitWithNamedFuncCallback(
          &ShutdownTimerCallback, &closure, kShutdownTimerDelayMS,
          nsITimer::TYPE_ONE_SHOT, "ParentImpl::ShutdownTimerCallback"));

      SpinEventLoopUntil([&]() { return !sLiveActorCount; });

      MOZ_ASSERT(liveActors->IsEmpty());

      MOZ_ALWAYS_SUCCEEDS(shutdownTimer->Cancel());
    }

    // Dispatch this runnable to unregister the thread from the profiler.
    nsCOMPtr<nsIRunnable> shutdownRunnable =
        new ShutdownBackgroundThreadRunnable();
    MOZ_ALWAYS_SUCCEEDS(thread->Dispatch(shutdownRunnable, NS_DISPATCH_NORMAL));

    MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
  }
}

// static
void ParentImpl::ShutdownTimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(sShutdownHasStarted);
  MOZ_ASSERT(sLiveActorCount);

  auto closure = static_cast<TimerCallbackClosure*>(aClosure);
  MOZ_ASSERT(closure);

  // Don't let the stack unwind until the ForceCloseBackgroundActorsRunnable has
  // finished.
  sLiveActorCount++;

  nsCOMPtr<nsIRunnable> forceCloseRunnable =
      new ForceCloseBackgroundActorsRunnable(closure->mLiveActors);
  MOZ_ALWAYS_SUCCEEDS(
      closure->mThread->Dispatch(forceCloseRunnable, NS_DISPATCH_NORMAL));
}

void ParentImpl::Destroy() {
  // May be called on any thread!

  AssertIsInMainOrSocketProcess();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NewNonOwningRunnableMethod("ParentImpl::MainThreadActorDestroy", this,
                                 &ParentImpl::MainThreadActorDestroy)));
}

void ParentImpl::MainThreadActorDestroy() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(!mIsOtherProcessActor, !mContent);

  mContent = nullptr;

  MOZ_ASSERT(sLiveActorCount);
  sLiveActorCount--;

  // This may be the last reference!
  Release();
}

void ParentImpl::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);
  MOZ_ASSERT_IF(mIsOtherProcessActor, mLiveActorArray);

  BackgroundParentImpl::ActorDestroy(aWhy);

  mActorDestroyed = true;

  if (mLiveActorArray) {
    MOZ_ALWAYS_TRUE(mLiveActorArray->RemoveElement(this));
    mLiveActorArray = nullptr;
  }

  // This is tricky. We should be able to call Destroy() here directly because
  // we're not going to touch 'this' or our MessageChannel any longer on this
  // thread. Destroy() dispatches the MainThreadActorDestroy runnable and when
  // it runs it will destroy 'this' and our associated MessageChannel. However,
  // IPDL is about to call MessageChannel::Clear() on this thread! To avoid
  // racing with the main thread we must ensure that the MessageChannel lives
  // long enough to be cleared in this call stack.

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NewNonOwningRunnableMethod(
      "ParentImpl::Destroy", this, &ParentImpl::Destroy)));
}

NS_IMPL_ISUPPORTS(ParentImpl::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
ParentImpl::ShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  AssertIsInMainOrSocketProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(!sShutdownHasStarted);
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID));

  sShutdownHasStarted = true;

  // Do this first before calling (and spinning the event loop in)
  // ShutdownBackgroundThread().
  ChildImpl::Shutdown();

  ShutdownBackgroundThread();

  return NS_OK;
}

NS_IMETHODIMP
ParentImpl::RequestMessageLoopRunnable::Run() {
  AssertIsInMainOrSocketProcess();
  MOZ_ASSERT(mTargetThread);

  if (NS_IsMainThread()) {
    MOZ_ASSERT(mMessageLoop);

    if (!sBackgroundThread ||
        !SameCOMIdentity(mTargetThread.get(), sBackgroundThread.get())) {
      return NS_OK;
    }

    MOZ_ASSERT(!sBackgroundThreadMessageLoop);
    sBackgroundThreadMessageLoop = mMessageLoop;

    return NS_OK;
  }

#ifdef DEBUG
  {
    bool correctThread;
    MOZ_ASSERT(NS_SUCCEEDED(mTargetThread->IsOnCurrentThread(&correctThread)));
    MOZ_ASSERT(correctThread);
  }
#endif

  DebugOnly<PRThread*> oldBackgroundThread =
      sBackgroundPRThread.exchange(PR_GetCurrentThread());

  MOZ_ASSERT_IF(oldBackgroundThread,
                PR_GetCurrentThread() != oldBackgroundThread);

  MOZ_ASSERT(!mMessageLoop);

  mMessageLoop = MessageLoop::current();
  MOZ_ASSERT(mMessageLoop);

  if (NS_FAILED(NS_DispatchToMainThread(this))) {
    NS_WARNING("Failed to dispatch RequestMessageLoopRunnable to main thread!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
ParentImpl::ShutdownBackgroundThreadRunnable::Run() {
  AssertIsInMainOrSocketProcess();

  // It is possible that another background thread was created while this thread
  // was shutting down. In that case we can't assert anything about
  // sBackgroundPRThread and we should not modify it here.
  sBackgroundPRThread.compareExchange(PR_GetCurrentThread(), nullptr);

  return NS_OK;
}

NS_IMETHODIMP
ParentImpl::ForceCloseBackgroundActorsRunnable::Run() {
  AssertIsInMainOrSocketProcess();
  MOZ_ASSERT(mActorArray);

  if (NS_IsMainThread()) {
    MOZ_ASSERT(sLiveActorCount);
    sLiveActorCount--;
    return NS_OK;
  }

  AssertIsOnBackgroundThread();

  if (!mActorArray->IsEmpty()) {
    // Copy the array since calling Close() could mutate the actual array.
    nsTArray<ParentImpl*> actorsToClose(mActorArray->Clone());

    for (uint32_t index = 0; index < actorsToClose.Length(); index++) {
      actorsToClose[index]->Close();
    }
  }

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  return NS_OK;
}

NS_IMETHODIMP
ParentImpl::ConnectActorRunnable::Run() {
  AssertIsInMainOrSocketProcess();
  AssertIsOnBackgroundThread();

  // Transfer ownership to this thread. If Open() fails then we will release
  // this reference in Destroy.
  ParentImpl* actor;
  mActor.forget(&actor);

  Endpoint<PBackgroundParent> endpoint = std::move(mEndpoint);

  if (!endpoint.Bind(actor)) {
    actor->Destroy();
    return NS_ERROR_FAILURE;
  }

  actor->SetLiveActorArray(mLiveActorArray);

  return NS_OK;
}

nsresult ParentImpl::CreateActorHelper::BlockAndGetResults(
    nsIEventTarget* aMainEventTarget, RefPtr<ParentImpl>& aParentActor,
    nsCOMPtr<nsIThread>& aThread) {
  AssertIsNotOnMainThread();

  if (aMainEventTarget) {
    MOZ_ALWAYS_SUCCEEDS(aMainEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  }

  mozilla::MonitorAutoLock lock(mMonitor);
  while (mWaiting) {
    lock.Wait();
  }

  if (NS_WARN_IF(NS_FAILED(mMainThreadResultCode))) {
    return mMainThreadResultCode;
  }

  aParentActor = std::move(mParentActor);
  aThread = std::move(mThread);
  return NS_OK;
}

nsresult ParentImpl::CreateActorHelper::RunOnMainThread() {
  AssertIsOnMainThread();

  if (!sBackgroundThread && !CreateBackgroundThread()) {
    NS_WARNING("Failed to create background thread!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!sShutdownHasStarted);

  sLiveActorCount++;

  mParentActor = new ParentImpl();
  mThread = sBackgroundThread;

  return NS_OK;
}

NS_IMETHODIMP
ParentImpl::CreateActorHelper::Run() {
  AssertIsOnMainThread();

  nsresult rv = RunOnMainThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mMainThreadResultCode = rv;
  }

  mozilla::MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  lock.Notify();

  return NS_OK;
}

// -----------------------------------------------------------------------------
// ChildImpl Implementation
// -----------------------------------------------------------------------------

// static
void ChildImpl::Startup() {
  // This happens on the main thread but before XPCOM has started so we can't
  // assert that we're being called on the main thread here.

  sParentAndContentProcessThreadInfo.Startup();
  sSocketAndContentProcessThreadInfo.Startup();
  sSocketAndParentProcessThreadInfo.Startup();

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_RELEASE_ASSERT(observerService);

  nsCOMPtr<nsIObserver> observer = new ShutdownObserver();

  nsresult rv = observerService->AddObserver(
      observer, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
}

// static
void ChildImpl::Shutdown() {
  AssertIsOnMainThread();

  sParentAndContentProcessThreadInfo.Shutdown();
  sSocketAndContentProcessThreadInfo.Shutdown();
  sSocketAndParentProcessThreadInfo.Shutdown();

  sShutdownHasStarted = true;
}

// static
PBackgroundChild* ChildImpl::GetForCurrentThread() {
  MOZ_ASSERT(sParentAndContentProcessThreadInfo.mThreadLocalIndex !=
             kBadThreadLocalIndex);

  auto threadLocalInfo =
      NS_IsMainThread()
          ? sParentAndContentProcessThreadInfo.mMainThreadInfo
          : static_cast<ThreadLocalInfo*>(PR_GetThreadPrivate(
                sParentAndContentProcessThreadInfo.mThreadLocalIndex));

  if (!threadLocalInfo) {
    return nullptr;
  }

  return threadLocalInfo->mActor;
}

/* static */
PBackgroundChild* ChildImpl::GetFromThreadInfo(
    nsIEventTarget* aMainEventTarget, ThreadLocalInfo* aThreadLocalInfo) {
  MOZ_ASSERT(aThreadLocalInfo);

  if (aThreadLocalInfo->mActor) {
    RefPtr<SendInitBackgroundRunnable>& runnable =
        aThreadLocalInfo->mSendInitBackgroundRunnable;

    if (aMainEventTarget && runnable) {
      // The SendInitBackgroundRunnable was already dispatched to the main
      // thread to finish initialization of a new background child actor.
      // However, the caller passed a custom main event target which indicates
      // that synchronous blocking of the main thread is happening (done by
      // creating a nested event target and spinning the event loop).
      // It can happen that the SendInitBackgroundRunnable didn't have a chance
      // to run before the synchronous blocking has occured. Unblocking of the
      // main thread can depend on an IPC message received on this thread, so
      // we have to dispatch the SendInitBackgroundRunnable to the custom main
      // event target too, otherwise IPC will be only queueing messages on this
      // thread. The runnable will run twice in the end, but that's a harmless
      // race between the main and nested event queue of the main thread.
      // There's a guard in the runnable implementation for calling
      // SendInitBackground only once.

      MOZ_ALWAYS_SUCCEEDS(
          aMainEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));
    }

    return aThreadLocalInfo->mActor;
  }

  return nullptr;
}

/* static */
PBackgroundChild* ChildImpl::GetOrCreateForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return sParentAndContentProcessThreadInfo.GetOrCreateForCurrentThread(
      aMainEventTarget);
}

/* static */
PBackgroundChild* ChildImpl::GetOrCreateSocketActorForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return sSocketAndContentProcessThreadInfo.GetOrCreateForCurrentThread(
      aMainEventTarget);
}

/* static */
PBackgroundChild* ChildImpl::GetOrCreateForSocketParentBridgeForCurrentThread(
    nsIEventTarget* aMainEventTarget) {
  return sSocketAndParentProcessThreadInfo.GetOrCreateForCurrentThread(
      aMainEventTarget);
}

// static
void ChildImpl::CloseForCurrentThread() {
  MOZ_ASSERT(!NS_IsMainThread(),
             "PBackground for the main thread should be shut down via "
             "ChildImpl::Shutdown().");

  sParentAndContentProcessThreadInfo.CloseForCurrentThread();
  sSocketAndContentProcessThreadInfo.CloseForCurrentThread();
  sSocketAndParentProcessThreadInfo.CloseForCurrentThread();
}

// static
BackgroundChildImpl::ThreadLocal* ChildImpl::GetThreadLocalForCurrentThread() {
  MOZ_ASSERT(sParentAndContentProcessThreadInfo.mThreadLocalIndex !=
                 kBadThreadLocalIndex,
             "BackgroundChild::Startup() was never called!");

  auto threadLocalInfo =
      NS_IsMainThread()
          ? sParentAndContentProcessThreadInfo.mMainThreadInfo
          : static_cast<ThreadLocalInfo*>(PR_GetThreadPrivate(
                sParentAndContentProcessThreadInfo.mThreadLocalIndex));

  if (!threadLocalInfo) {
    return nullptr;
  }

  if (!threadLocalInfo->mConsumerThreadLocal) {
    threadLocalInfo->mConsumerThreadLocal =
        MakeUnique<BackgroundChildImpl::ThreadLocal>();
  }

  return threadLocalInfo->mConsumerThreadLocal.get();
}

// static
void ChildImpl::ThreadLocalDestructor(void* aThreadLocal) {
  auto threadLocalInfo = static_cast<ThreadLocalInfo*>(aThreadLocal);

  if (threadLocalInfo) {
    MOZ_ASSERT(threadLocalInfo->mClosed);

    if (threadLocalInfo->mActor) {
      threadLocalInfo->mActor->Close();
      threadLocalInfo->mActor->AssertActorDestroyed();
    }

    if (threadLocalInfo->mSendInitBackgroundRunnable) {
      threadLocalInfo->mSendInitBackgroundRunnable->ClearEventTarget();
    }

    delete threadLocalInfo;
  }
}

void ChildImpl::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif

  BackgroundChildImpl::ActorDestroy(aWhy);
}

NS_IMPL_ISUPPORTS(ChildImpl::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
ChildImpl::ShutdownObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID));

  ChildImpl::Shutdown();

  return NS_OK;
}

// static
already_AddRefed<ChildImpl::SendInitBackgroundRunnable>
ChildImpl::SendInitBackgroundRunnable::Create(
    Endpoint<PBackgroundParent>&& aParent,
    std::function<void(Endpoint<PBackgroundParent>&& aParent)>&& aFunc,
    unsigned int aThreadLocalIndex) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<SendInitBackgroundRunnable> runnable = new SendInitBackgroundRunnable(
      std::move(aParent), std::move(aFunc), aThreadLocalIndex);

  WorkerPrivate* workerPrivate = mozilla::dom::GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    return runnable.forget();
  }

  workerPrivate->AssertIsOnWorkerThread();

  runnable->mWorkerRef = StrongWorkerRef::Create(
      workerPrivate, "ChildImpl::SendInitBackgroundRunnable");
  if (NS_WARN_IF(!runnable->mWorkerRef)) {
    return nullptr;
  }

  return runnable.forget();
}

NS_IMETHODIMP
ChildImpl::SendInitBackgroundRunnable::Run() {
  if (NS_IsMainThread()) {
    if (mSentInitBackground) {
      return NS_OK;
    }

    mSentInitBackground = true;

    mSendInitfunc(std::move(mParent));

    nsCOMPtr<nsISerialEventTarget> owningEventTarget;
    {
      mozilla::MutexAutoLock lock(mMutex);
      owningEventTarget = mOwningEventTarget;
    }

    if (!owningEventTarget) {
      return NS_OK;
    }

    nsresult rv = owningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  ClearEventTarget();

  auto threadLocalInfo =
      static_cast<ThreadLocalInfo*>(PR_GetThreadPrivate(mThreadLocalIndex));

  if (!threadLocalInfo) {
    return NS_OK;
  }

  threadLocalInfo->mSendInitBackgroundRunnable = nullptr;

  return NS_OK;
}
