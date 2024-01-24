/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundChild.h"
#include "BackgroundParent.h"

#include "BackgroundChildImpl.h"
#include "BackgroundParentImpl.h"
#include "MainThreadUtils.h"
#include "base/process_util.h"
#include "base/task.h"
#include "FileDescriptor.h"
#include "GeckoProfiler.h"
#include "InputStreamUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/ipc/BackgroundStarterChild.h"
#include "mozilla/ipc/BackgroundStarterParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundStarter.h"
#include "mozilla/ipc/ProtocolTypes.h"
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

void AssertIsOnMainThread() { THREADSAFETY_ASSERT(NS_IsMainThread()); }

// -----------------------------------------------------------------------------
// ParentImpl Declaration
// -----------------------------------------------------------------------------

class ParentImpl final : public BackgroundParentImpl {
  friend class ChildImpl;
  friend class mozilla::ipc::BackgroundParent;
  friend class mozilla::ipc::BackgroundStarterParent;

 private:
  class ShutdownObserver;

  struct MOZ_STACK_CLASS TimerCallbackClosure {
    nsIThread* mThread;
    nsTArray<IToplevelProtocol*>* mLiveActors;

    TimerCallbackClosure(nsIThread* aThread,
                         nsTArray<IToplevelProtocol*>* aLiveActors)
        : mThread(aThread), mLiveActors(aLiveActors) {
      AssertIsInMainProcess();
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
  static nsTArray<IToplevelProtocol*>* sLiveActorsForBackgroundThread;

  // This is only modified on the main thread.
  static StaticRefPtr<nsITimer> sShutdownTimer;

  // This exists so that that [Assert]IsOnBackgroundThread() can continue to
  // work during shutdown.
  static Atomic<PRThread*> sBackgroundPRThread;

  // Maintains a count of live actors so that the background thread can be shut
  // down when it is no longer needed.
  // May be incremented on either the background thread (by an existing actor)
  // or on the main thread, but must be decremented on the main thread.
  static Atomic<uint64_t> sLiveActorCount;

  // This is only modified on the main thread. It is true after the shutdown
  // observer is registered and is never unset thereafter.
  static bool sShutdownObserverRegistered;

  // This is only modified on the main thread. It prevents us from trying to
  // create the background thread after application shutdown has started.
  static bool sShutdownHasStarted;

  // null if this is a same-process actor.
  const RefPtr<ThreadsafeContentParentHandle> mContent;

  // Set when the actor is opened successfully and used to handle shutdown
  // hangs. Only touched on the background thread.
  nsTArray<IToplevelProtocol*>* mLiveActorArray;

  // Set at construction to indicate whether this parent actor corresponds to a
  // child actor in another process or to a child actor from a different thread
  // in the same process.
  const bool mIsOtherProcessActor;

  // Set after ActorDestroy has been called. Only touched on the background
  // thread.
  bool mActorDestroyed;

 public:
  static bool IsOnBackgroundThread() {
    return PR_GetCurrentThread() == sBackgroundPRThread;
  }

  static void AssertIsOnBackgroundThread() {
    THREADSAFETY_ASSERT(IsOnBackgroundThread());
  }

  // `ParentImpl` instances need to be deleted on the main thread, despite IPC
  // controlling them on a background thread. Use `_WITH_DELETE_ON_MAIN_THREAD`
  // to force destruction to occur on the desired thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_MAIN_THREAD(ParentImpl,
                                                                   override)

  void Destroy();

 private:
  // Forwarded from BackgroundParent.
  static bool IsOtherProcessActor(PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static ThreadsafeContentParentHandle* GetContentParentHandle(
      PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static uint64_t GetChildID(PBackgroundParent* aBackgroundActor);

  // Forwarded from BackgroundParent.
  static void KillHardAsync(PBackgroundParent* aBackgroundActor,
                            const nsACString& aReason);

  // Forwarded from BackgroundParent.
  static bool AllocStarter(ContentParent* aContent,
                           Endpoint<PBackgroundStarterParent>&& aEndpoint,
                           bool aCrossProcess = true);

  static bool CreateBackgroundThread();

  static void ShutdownBackgroundThread();

  static void ShutdownTimerCallback(nsITimer* aTimer, void* aClosure);

  // NOTE: ParentImpl could be used in 2 cases below.
  // 1. Within the parent process.
  // 2. Between parent process and content process.
  // |aContent| should be not null for case 2. For cases 1, it's null.
  explicit ParentImpl(ThreadsafeContentParentHandle* aContent,
                      bool aIsOtherProcessActor)
      : mContent(aContent),
        mLiveActorArray(nullptr),
        mIsOtherProcessActor(aIsOtherProcessActor),
        mActorDestroyed(false) {
    AssertIsInMainProcess();
    MOZ_ASSERT_IF(!aIsOtherProcessActor, XRE_IsParentProcess());
  }

  ~ParentImpl() {
    AssertIsInMainProcess();
    AssertIsOnMainThread();
  }

  void MainThreadActorDestroy();

  void SetLiveActorArray(nsTArray<IToplevelProtocol*>* aLiveActorArray) {
    AssertIsInMainProcess();
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
  friend class mozilla::ipc::BackgroundStarterChild;

  typedef base::ProcessId ProcessId;

  class ShutdownObserver;

 public:
  struct ThreadLocalInfo {
    ThreadLocalInfo()
#ifdef DEBUG
        : mClosed(false)
#endif
    {
    }

    RefPtr<ChildImpl> mActor;
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

    ThreadInfoWrapper() = default;

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

      RefPtr<BackgroundStarterChild> starter;
      {
        auto lock = mStarter.Lock();
        starter = lock->forget();
      }
      if (starter) {
        CloseStarter(starter);
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

    template <typename Actor>
    void InitStarter(Actor* aActor) {
      AssertIsOnMainThread();

      // Create a pair of endpoints and send them to the other process.
      Endpoint<PBackgroundStarterParent> parent;
      Endpoint<PBackgroundStarterChild> child;
      MOZ_ALWAYS_SUCCEEDS(PBackgroundStarter::CreateEndpoints(
          aActor->OtherPid(), base::GetCurrentProcId(), &parent, &child));
      MOZ_ALWAYS_TRUE(aActor->SendInitBackground(std::move(parent)));

      InitStarter(std::move(child));
    }

    void InitStarter(Endpoint<PBackgroundStarterChild>&& aEndpoint) {
      AssertIsOnMainThread();

      base::ProcessId otherPid = aEndpoint.OtherPid();

      nsCOMPtr<nsISerialEventTarget> taskQueue;
      MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
          "PBackgroundStarter Queue", getter_AddRefs(taskQueue)));

      RefPtr<BackgroundStarterChild> starter =
          new BackgroundStarterChild(otherPid, taskQueue);

      taskQueue->Dispatch(NS_NewRunnableFunction(
          "PBackgroundStarterChild Init",
          [starter, endpoint = std::move(aEndpoint)]() mutable {
            MOZ_ALWAYS_TRUE(endpoint.Bind(starter));
          }));

      // Swap in the newly initialized `BackgroundStarterChild`, and close the
      // previous one if we're replacing an existing PBackgroundStarterChild
      // instance.
      RefPtr<BackgroundStarterChild> prevStarter;
      {
        auto lock = mStarter.Lock();
        prevStarter = lock->forget();
        *lock = starter.forget();
      }
      if (prevStarter) {
        CloseStarter(prevStarter);
      }
    }

    void CloseForCurrentThread() {
      MOZ_ASSERT(!NS_IsMainThread());

      if (mThreadLocalIndex == kBadThreadLocalIndex) {
        return;
      }

      auto* threadLocalInfo =
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

    PBackgroundChild* GetOrCreateForCurrentThread() {
      // Processes can be told to do final CC's during shutdown even though
      // they never finished starting (and thus call this), because they
      // hadn't gotten far enough to call Startup() before shutdown began.
      if (mThreadLocalIndex == kBadThreadLocalIndex) {
        NS_ERROR("BackgroundChild::Startup() was never called");
        return nullptr;
      }
      if (NS_IsMainThread() && ChildImpl::sShutdownHasStarted) {
        return nullptr;
      }

      auto* threadLocalInfo = NS_IsMainThread()
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

      if (threadLocalInfo->mActor) {
        return threadLocalInfo->mActor;
      }

      RefPtr<BackgroundStarterChild> starter;
      {
        auto lock = mStarter.Lock();
        starter = *lock;
      }
      if (!starter) {
        CRASH_IN_CHILD_PROCESS("No BackgroundStarterChild");
        return nullptr;
      }

      Endpoint<PBackgroundParent> parent;
      Endpoint<PBackgroundChild> child;
      nsresult rv;
      rv = PBackground::CreateEndpoints(
          starter->mOtherPid, base::GetCurrentProcId(), &parent, &child);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to create top level actor!");
        return nullptr;
      }

      RefPtr<ChildImpl> strongActor = new ChildImpl();
      if (!child.Bind(strongActor)) {
        CRASH_IN_CHILD_PROCESS("Failed to bind ChildImpl!");
        return nullptr;
      }
      strongActor->SetActorAlive();
      threadLocalInfo->mActor = strongActor;

      // Dispatch to the background task queue to create the relevant actor in
      // the remote process.
      starter->mTaskQueue->Dispatch(NS_NewRunnableFunction(
          "PBackground GetOrCreateForCurrentThread",
          [starter, endpoint = std::move(parent)]() mutable {
            if (!starter->SendInitBackground(std::move(endpoint))) {
              NS_WARNING("Failed to create toplevel actor");
            }
          }));
      return strongActor;
    }

   private:
    static void CloseStarter(BackgroundStarterChild* aStarter) {
      aStarter->mTaskQueue->Dispatch(NS_NewRunnableFunction(
          "PBackgroundStarterChild Close",
          [starter = RefPtr{aStarter}] { starter->Close(); }));
    }

    // This is only modified on the main thread. It is the thread-local index
    // that we use to store the BackgroundChild for each thread.
    unsigned int mThreadLocalIndex = kBadThreadLocalIndex;

    // On the main thread, we store TLS in this global instead of in
    // mThreadLocalIndex. That way, cooperative main threads all share the same
    // thread info.
    ThreadLocalInfo* mMainThreadInfo = nullptr;

    // The starter which will be used to launch PBackground instances of this
    // type. Only modified on the main thread, but may be read by any thread
    // wanting to start background actors.
    StaticDataMutex<StaticRefPtr<BackgroundStarterChild>> mStarter{"mStarter"};
  };

  // For PBackground between parent and content process.
  static ThreadInfoWrapper sParentAndContentProcessThreadInfo;

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
      : mOwningEventTarget(GetCurrentSerialEventTarget())
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

  // This type is threadsafe refcounted as actors managed by it may be destroyed
  // after the thread it is bound to dies, and hold a reference to this object.
  //
  // It is _not_ safe to use this type or any methods on it from off of the
  // thread it was created for.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChildImpl, override)

 private:
  // Forwarded from BackgroundChild.
  static void Startup();

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetForCurrentThread();

  // Forwarded from BackgroundChild.
  static PBackgroundChild* GetOrCreateForCurrentThread();

  static void CloseForCurrentThread();

  // Forwarded from BackgroundChildImpl.
  static BackgroundChildImpl::ThreadLocal* GetThreadLocalForCurrentThread();

  // Forwarded from BackgroundChild.
  static void InitContentStarter(mozilla::dom::ContentChild* aContent);

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
ThreadsafeContentParentHandle* BackgroundParent::GetContentParentHandle(
    PBackgroundParent* aBackgroundActor) {
  return ParentImpl::GetContentParentHandle(aBackgroundActor);
}

// static
uint64_t BackgroundParent::GetChildID(PBackgroundParent* aBackgroundActor) {
  return ParentImpl::GetChildID(aBackgroundActor);
}

// static
void BackgroundParent::KillHardAsync(PBackgroundParent* aBackgroundActor,
                                     const nsACString& aReason) {
  ParentImpl::KillHardAsync(aBackgroundActor, aReason);
}

// static
bool BackgroundParent::AllocStarter(
    ContentParent* aContent, Endpoint<PBackgroundStarterParent>&& aEndpoint) {
  return ParentImpl::AllocStarter(aContent, std::move(aEndpoint));
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
PBackgroundChild* BackgroundChild::GetOrCreateForCurrentThread() {
  return ChildImpl::GetOrCreateForCurrentThread();
}

// static
void BackgroundChild::CloseForCurrentThread() {
  ChildImpl::CloseForCurrentThread();
}

// static
void BackgroundChild::InitContentStarter(ContentChild* aContent) {
  ChildImpl::InitContentStarter(aContent);
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

nsTArray<IToplevelProtocol*>* ParentImpl::sLiveActorsForBackgroundThread;

StaticRefPtr<nsITimer> ParentImpl::sShutdownTimer;

Atomic<PRThread*> ParentImpl::sBackgroundPRThread;

Atomic<uint64_t> ParentImpl::sLiveActorCount;

bool ParentImpl::sShutdownObserverRegistered = false;

bool ParentImpl::sShutdownHasStarted = false;

// -----------------------------------------------------------------------------
// ChildImpl Static Members
// -----------------------------------------------------------------------------

ChildImpl::ThreadInfoWrapper ChildImpl::sParentAndContentProcessThreadInfo;

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
ThreadsafeContentParentHandle* ParentImpl::GetContentParentHandle(
    PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  return static_cast<ParentImpl*>(aBackgroundActor)->mContent.get();
}

// static
uint64_t ParentImpl::GetChildID(PBackgroundParent* aBackgroundActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);

  auto actor = static_cast<ParentImpl*>(aBackgroundActor);
  if (actor->mContent) {
    return actor->mContent->ChildID();
  }

  return 0;
}

// static
void ParentImpl::KillHardAsync(PBackgroundParent* aBackgroundActor,
                               const nsACString& aReason) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(BackgroundParent::IsOtherProcessActor(aBackgroundActor));

  RefPtr<ThreadsafeContentParentHandle> handle =
      GetContentParentHandle(aBackgroundActor);
  MOZ_ASSERT(handle);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NS_NewRunnableFunction(
          "ParentImpl::KillHardAsync",
          [handle = std::move(handle), reason = nsCString{aReason}]() {
            mozilla::AssertIsOnMainThread();

            if (RefPtr<ContentParent> contentParent =
                    handle->GetContentParent()) {
              contentParent->KillHard(reason.get());
            }
          }),
      NS_DISPATCH_NORMAL));

  // After we've scheduled killing of the remote process, also ensure we induce
  // a connection error in the IPC channel to immediately stop all IPC
  // communication on this channel.
  if (aBackgroundActor->CanSend()) {
    aBackgroundActor->GetIPCChannel()->InduceConnectionError();
  }
}

// static
bool ParentImpl::AllocStarter(ContentParent* aContent,
                              Endpoint<PBackgroundStarterParent>&& aEndpoint,
                              bool aCrossProcess) {
  AssertIsInMainProcess();
  AssertIsOnMainThread();

  MOZ_ASSERT(aEndpoint.IsValid());

  if (!sBackgroundThread && !CreateBackgroundThread()) {
    NS_WARNING("Failed to create background thread!");
    return false;
  }

  sLiveActorCount++;

  RefPtr<BackgroundStarterParent> actor = new BackgroundStarterParent(
      aContent ? aContent->ThreadsafeHandle() : nullptr, aCrossProcess);

  if (NS_FAILED(sBackgroundThread->Dispatch(NS_NewRunnableFunction(
          "BackgroundStarterParent::ConnectActorRunnable",
          [actor = std::move(actor), endpoint = std::move(aEndpoint),
           liveActorArray = sLiveActorsForBackgroundThread]() mutable {
            MOZ_ASSERT(endpoint.IsValid());
            MOZ_ALWAYS_TRUE(endpoint.Bind(actor));
            actor->SetLiveActorArray(liveActorArray);
          })))) {
    NS_WARNING("Failed to dispatch connect runnable!");

    MOZ_ASSERT(sLiveActorCount);
    sLiveActorCount--;
  }

  return true;
}

// static
bool ParentImpl::CreateBackgroundThread() {
  AssertIsInMainProcess();
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
  if (NS_FAILED(NS_NewNamedThread(
          "IPDL Background", getter_AddRefs(thread),
          NS_NewRunnableFunction(
              "Background::ParentImpl::CreateBackgroundThreadRunnable", []() {
                DebugOnly<PRThread*> oldBackgroundThread =
                    sBackgroundPRThread.exchange(PR_GetCurrentThread());

                MOZ_ASSERT_IF(oldBackgroundThread,
                              PR_GetCurrentThread() != oldBackgroundThread);
              })))) {
    NS_WARNING("NS_NewNamedThread failed!");
    return false;
  }

  sBackgroundThread = thread.forget();

  sLiveActorsForBackgroundThread = new nsTArray<IToplevelProtocol*>(1);

  if (!sShutdownTimer) {
    MOZ_ASSERT(newShutdownTimer);
    sShutdownTimer = newShutdownTimer;
  }

  return true;
}

// static
void ParentImpl::ShutdownBackgroundThread() {
  AssertIsInMainProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(sShutdownHasStarted);
  MOZ_ASSERT_IF(!sBackgroundThread, !sLiveActorCount);
  MOZ_ASSERT_IF(sBackgroundThread, sShutdownTimer);

  nsCOMPtr<nsITimer> shutdownTimer = sShutdownTimer.get();
  sShutdownTimer = nullptr;

  if (sBackgroundThread) {
    nsCOMPtr<nsIThread> thread = sBackgroundThread.get();
    sBackgroundThread = nullptr;

    UniquePtr<nsTArray<IToplevelProtocol*>> liveActors(
        sLiveActorsForBackgroundThread);
    sLiveActorsForBackgroundThread = nullptr;

    MOZ_ASSERT_IF(!sShutdownHasStarted, !sLiveActorCount);

    if (sLiveActorCount) {
      // We need to spin the event loop while we wait for all the actors to be
      // cleaned up. We also set a timeout to force-kill any hanging actors.
      TimerCallbackClosure closure(thread, liveActors.get());

      MOZ_ALWAYS_SUCCEEDS(shutdownTimer->InitWithNamedFuncCallback(
          &ShutdownTimerCallback, &closure, kShutdownTimerDelayMS,
          nsITimer::TYPE_ONE_SHOT, "ParentImpl::ShutdownTimerCallback"));

      SpinEventLoopUntil("ParentImpl::ShutdownBackgroundThread"_ns,
                         [&]() { return !sLiveActorCount; });

      MOZ_ASSERT(liveActors->IsEmpty());

      MOZ_ALWAYS_SUCCEEDS(shutdownTimer->Cancel());
    }

    // Dispatch this runnable to unregister the PR thread from the profiler.
    MOZ_ALWAYS_SUCCEEDS(thread->Dispatch(NS_NewRunnableFunction(
        "Background::ParentImpl::ShutdownBackgroundThreadRunnable", []() {
          // It is possible that another background thread was created while
          // this thread was shutting down. In that case we can't assert
          // anything about sBackgroundPRThread and we should not modify it
          // here.
          sBackgroundPRThread.compareExchange(PR_GetCurrentThread(), nullptr);
        })));

    MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
  }
}

// static
void ParentImpl::ShutdownTimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsInMainProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT(sShutdownHasStarted);
  MOZ_ASSERT(sLiveActorCount);

  auto closure = static_cast<TimerCallbackClosure*>(aClosure);
  MOZ_ASSERT(closure);

  // Don't let the stack unwind until the ForceCloseBackgroundActorsRunnable has
  // finished.
  sLiveActorCount++;

  InvokeAsync(
      closure->mThread, __func__,
      [liveActors = closure->mLiveActors]() {
        MOZ_ASSERT(liveActors);

        if (!liveActors->IsEmpty()) {
          // Copy the array since calling Close() could mutate the
          // actual array.
          nsTArray<IToplevelProtocol*> actorsToClose(liveActors->Clone());
          for (IToplevelProtocol* actor : actorsToClose) {
            actor->Close();
          }
        }
        return GenericPromise::CreateAndResolve(true, __func__);
      })
      ->Then(GetCurrentSerialEventTarget(), __func__, []() {
        MOZ_ASSERT(sLiveActorCount);
        sLiveActorCount--;
      });
}

void ParentImpl::Destroy() {
  // May be called on any thread!

  AssertIsInMainProcess();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NewNonOwningRunnableMethod("ParentImpl::MainThreadActorDestroy", this,
                                 &ParentImpl::MainThreadActorDestroy)));
}

void ParentImpl::MainThreadActorDestroy() {
  AssertIsInMainProcess();
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(!mIsOtherProcessActor, !mContent);

  MOZ_ASSERT(sLiveActorCount);
  sLiveActorCount--;

  // This may be the last reference!
  Release();
}

void ParentImpl::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsInMainProcess();
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
  AssertIsInMainProcess();
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

BackgroundStarterParent::BackgroundStarterParent(
    ThreadsafeContentParentHandle* aContent, bool aCrossProcess)
    : mCrossProcess(aCrossProcess), mContent(aContent) {
  AssertIsOnMainThread();
  AssertIsInMainProcess();
  MOZ_ASSERT_IF(!mCrossProcess, !mContent);
  MOZ_ASSERT_IF(!mCrossProcess, XRE_IsParentProcess());
}

void BackgroundStarterParent::SetLiveActorArray(
    nsTArray<IToplevelProtocol*>* aLiveActorArray) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aLiveActorArray);
  MOZ_ASSERT(!aLiveActorArray->Contains(this));
  MOZ_ASSERT(!mLiveActorArray);
  MOZ_ASSERT_IF(!mCrossProcess, OtherPid() == base::GetCurrentProcId());

  mLiveActorArray = aLiveActorArray;
  mLiveActorArray->AppendElement(this);
}

IPCResult BackgroundStarterParent::RecvInitBackground(
    Endpoint<PBackgroundParent>&& aEndpoint) {
  AssertIsOnBackgroundThread();

  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this,
                    "Cannot initialize PBackground with invalid endpoint");
  }

  ParentImpl* actor = new ParentImpl(mContent, mCrossProcess);

  // Take a reference on this thread. If Open() fails then we will release this
  // reference in Destroy.
  NS_ADDREF(actor);

  ParentImpl::sLiveActorCount++;

  if (!aEndpoint.Bind(actor)) {
    actor->Destroy();
    return IPC_OK();
  }

  if (mCrossProcess) {
    actor->SetLiveActorArray(mLiveActorArray);
  }
  return IPC_OK();
}

void BackgroundStarterParent::ActorDestroy(ActorDestroyReason aReason) {
  AssertIsOnBackgroundThread();

  if (mLiveActorArray) {
    MOZ_ALWAYS_TRUE(mLiveActorArray->RemoveElement(this));
    mLiveActorArray = nullptr;
  }

  // Make sure to decrement `sLiveActorCount` on the main thread.
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NS_NewRunnableFunction("BackgroundStarterParent::MainThreadDestroy",
                             [] { ParentImpl::sLiveActorCount--; })));
}

// -----------------------------------------------------------------------------
// ChildImpl Implementation
// -----------------------------------------------------------------------------

// static
void ChildImpl::Startup() {
  // This happens on the main thread but before XPCOM has started so we can't
  // assert that we're being called on the main thread here.

  sParentAndContentProcessThreadInfo.Startup();

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_RELEASE_ASSERT(observerService);

  nsCOMPtr<nsIObserver> observer = new ShutdownObserver();

  nsresult rv = observerService->AddObserver(
      observer, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // Initialize a starter actor to allow starting PBackground within the parent
  // process.
  if (XRE_IsParentProcess()) {
    Endpoint<PBackgroundStarterParent> parent;
    Endpoint<PBackgroundStarterChild> child;
    MOZ_ALWAYS_SUCCEEDS(PBackgroundStarter::CreateEndpoints(
        base::GetCurrentProcId(), base::GetCurrentProcId(), &parent, &child));

    MOZ_ALWAYS_TRUE(ParentImpl::AllocStarter(nullptr, std::move(parent),
                                             /* aCrossProcess */ false));
    sParentAndContentProcessThreadInfo.InitStarter(std::move(child));
  }
}

// static
void ChildImpl::Shutdown() {
  AssertIsOnMainThread();

  sParentAndContentProcessThreadInfo.Shutdown();

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
PBackgroundChild* ChildImpl::GetOrCreateForCurrentThread() {
  return sParentAndContentProcessThreadInfo.GetOrCreateForCurrentThread();
}

// static
void ChildImpl::CloseForCurrentThread() {
  MOZ_ASSERT(!NS_IsMainThread(),
             "PBackground for the main thread should be shut down via "
             "ChildImpl::Shutdown().");

  sParentAndContentProcessThreadInfo.CloseForCurrentThread();
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
void ChildImpl::InitContentStarter(mozilla::dom::ContentChild* aContent) {
  sParentAndContentProcessThreadInfo.InitStarter(aContent);
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
