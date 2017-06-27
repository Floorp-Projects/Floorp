/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Context.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/dom/cache/Action.h"
#include "mozilla/dom/cache/FileUtils.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozIStorageConnection.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace {

using mozilla::dom::cache::Action;
using mozilla::dom::cache::QuotaInfo;

class NullAction final : public Action
{
public:
  NullAction()
  {
  }

  virtual void
  RunOnTarget(Resolver* aResolver, const QuotaInfo&, Data*) override
  {
    // Resolve success immediately.  This Action does no actual work.
    MOZ_DIAGNOSTIC_ASSERT(aResolver);
    aResolver->Resolve(NS_OK);
  }
};

} // namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::OpenDirectoryListener;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

class Context::Data final : public Action::Data
{
public:
  explicit Data(nsISerialEventTarget* aTarget)
    : mTarget(aTarget)
  {
    MOZ_DIAGNOSTIC_ASSERT(mTarget);
  }

  virtual mozIStorageConnection*
  GetConnection() const override
  {
    MOZ_ASSERT(mTarget->IsOnCurrentThread());
    return mConnection;
  }

  virtual void
  SetConnection(mozIStorageConnection* aConn) override
  {
    MOZ_ASSERT(mTarget->IsOnCurrentThread());
    MOZ_DIAGNOSTIC_ASSERT(!mConnection);
    mConnection = aConn;
    MOZ_DIAGNOSTIC_ASSERT(mConnection);
  }

private:
  ~Data()
  {
    // We could proxy release our data here, but instead just assert.  The
    // Context code should guarantee that we are destroyed on the target
    // thread once the connection is initialized.  If we're not, then
    // QuotaManager might race and try to clear the origin out from under us.
    MOZ_ASSERT_IF(mConnection, mTarget->IsOnCurrentThread());
  }

  nsCOMPtr<nsISerialEventTarget> mTarget;
  nsCOMPtr<mozIStorageConnection> mConnection;

  // Threadsafe counting because we're created on the PBackground thread
  // and destroyed on the target IO thread.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Context::Data)
};

// Executed to perform the complicated dance of steps necessary to initialize
// the QuotaManager.  This must be performed for each origin before any disk
// IO occurrs.
class Context::QuotaInitRunnable final : public nsIRunnable
                                       , public OpenDirectoryListener
{
public:
  QuotaInitRunnable(Context* aContext,
                    Manager* aManager,
                    Data* aData,
                    nsISerialEventTarget* aTarget,
                    Action* aInitAction)
    : mContext(aContext)
    , mThreadsafeHandle(aContext->CreateThreadsafeHandle())
    , mManager(aManager)
    , mData(aData)
    , mTarget(aTarget)
    , mInitAction(aInitAction)
    , mInitiatingEventTarget(GetCurrentThreadEventTarget())
    , mResult(NS_OK)
    , mState(STATE_INIT)
    , mCanceled(false)
  {
    MOZ_DIAGNOSTIC_ASSERT(mContext);
    MOZ_DIAGNOSTIC_ASSERT(mManager);
    MOZ_DIAGNOSTIC_ASSERT(mData);
    MOZ_DIAGNOSTIC_ASSERT(mTarget);
    MOZ_DIAGNOSTIC_ASSERT(mInitiatingEventTarget);
    MOZ_DIAGNOSTIC_ASSERT(mInitAction);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_INIT);

    mState = STATE_GET_INFO;
    nsresult rv = NS_DispatchToMainThread(this, nsIThread::DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mState = STATE_COMPLETE;
      Clear();
    }
    return rv;
  }

  void Cancel()
  {
    NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
    MOZ_DIAGNOSTIC_ASSERT(!mCanceled);
    mCanceled = true;
    mInitAction->CancelOnInitiatingThread();
  }

  void OpenDirectory();

  // OpenDirectoryListener methods
  virtual void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void
  DirectoryLockFailed() override;

private:
  class SyncResolver final : public Action::Resolver
  {
  public:
    SyncResolver()
      : mResolved(false)
      , mResult(NS_OK)
    { }

    virtual void
    Resolve(nsresult aRv) override
    {
      MOZ_DIAGNOSTIC_ASSERT(!mResolved);
      mResolved = true;
      mResult = aRv;
    };

    bool Resolved() const { return mResolved; }
    nsresult Result() const { return mResult; }

  private:
    ~SyncResolver() { }

    bool mResolved;
    nsresult mResult;

    NS_INLINE_DECL_REFCOUNTING(Context::QuotaInitRunnable::SyncResolver, override)
  };

  ~QuotaInitRunnable()
  {
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_COMPLETE);
    MOZ_DIAGNOSTIC_ASSERT(!mContext);
    MOZ_DIAGNOSTIC_ASSERT(!mInitAction);
  }

  enum State
  {
    STATE_INIT,
    STATE_GET_INFO,
    STATE_CREATE_QUOTA_MANAGER,
    STATE_OPEN_DIRECTORY,
    STATE_WAIT_FOR_DIRECTORY_LOCK,
    STATE_ENSURE_ORIGIN_INITIALIZED,
    STATE_RUN_ON_TARGET,
    STATE_RUNNING,
    STATE_COMPLETING,
    STATE_COMPLETE
  };

  void Complete(nsresult aResult)
  {
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_RUNNING || NS_FAILED(aResult));

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(mResult));
    mResult = aResult;

    mState = STATE_COMPLETING;
    MOZ_ALWAYS_SUCCEEDS(
      mInitiatingEventTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL));
  }

  void Clear()
  {
    NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
    MOZ_DIAGNOSTIC_ASSERT(mContext);
    mContext = nullptr;
    mManager = nullptr;
    mInitAction = nullptr;
  }

  RefPtr<Context> mContext;
  RefPtr<ThreadsafeHandle> mThreadsafeHandle;
  RefPtr<Manager> mManager;
  RefPtr<Data> mData;
  nsCOMPtr<nsISerialEventTarget> mTarget;
  RefPtr<Action> mInitAction;
  nsCOMPtr<nsIEventTarget> mInitiatingEventTarget;
  nsresult mResult;
  QuotaInfo mQuotaInfo;
  RefPtr<DirectoryLock> mDirectoryLock;
  State mState;
  Atomic<bool> mCanceled;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

void
Context::QuotaInitRunnable::OpenDirectory()
{
  NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_CREATE_QUOTA_MANAGER ||
                        mState == STATE_OPEN_DIRECTORY);
  MOZ_DIAGNOSTIC_ASSERT(QuotaManager::Get());

  // QuotaManager::OpenDirectory() will hold a reference to us as
  // a listener.  We will then get DirectoryLockAcquired() on the owning
  // thread when it is safe to access our storage directory.
  mState = STATE_WAIT_FOR_DIRECTORY_LOCK;
  QuotaManager::Get()->OpenDirectory(PERSISTENCE_TYPE_DEFAULT,
                                     mQuotaInfo.mGroup,
                                     mQuotaInfo.mOrigin,
                                     quota::Client::DOMCACHE,
                                     /* aExclusive */ false,
                                     this);
}

void
Context::QuotaInitRunnable::DirectoryLockAcquired(DirectoryLock* aLock)
{
  NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_WAIT_FOR_DIRECTORY_LOCK);
  MOZ_DIAGNOSTIC_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  if (mCanceled) {
    Complete(NS_ERROR_ABORT);
    return;
  }

  QuotaManager* qm = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(qm);

  mState = STATE_ENSURE_ORIGIN_INITIALIZED;
  nsresult rv = qm->IOThread()->Dispatch(this, nsIThread::DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Complete(rv);
    return;
  }
}

void
Context::QuotaInitRunnable::DirectoryLockFailed()
{
  NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_WAIT_FOR_DIRECTORY_LOCK);
  MOZ_DIAGNOSTIC_ASSERT(!mDirectoryLock);

  NS_WARNING("Failed to acquire a directory lock!");

  Complete(NS_ERROR_FAILURE);
}

NS_IMPL_ISUPPORTS(mozilla::dom::cache::Context::QuotaInitRunnable, nsIRunnable);

// The QuotaManager init state machine is represented in the following diagram:
//
//    +---------------+
//    |     Start     |      Resolve(error)
//    | (Orig Thread) +---------------------+
//    +-------+-------+                     |
//            |                             |
// +----------v-----------+                 |
// |       GetInfo        |  Resolve(error) |
// |    (Main Thread)     +-----------------+
// +----------+-----------+                 |
//            |                             |
// +----------v-----------+                 |
// |  CreateQuotaManager  |  Resolve(error) |
// |    (Orig Thread)     +-----------------+
// +----------+-----------+                 |
//            |                             |
// +----------v-----------+                 |
// |    OpenDirectory     |  Resolve(error) |
// |    (Orig Thread)     +-----------------+
// +----------+-----------+                 |
//            |                             |
// +----------v-----------+                 |
// | WaitForDirectoryLock |  Resolve(error) |
// |    (Orig Thread)     +-----------------+
// +----------+-----------+                 |
//            |                             |
// +----------v------------+                |
// |EnsureOriginInitialized| Resolve(error) |
// |   (Quota IO Thread)   +----------------+
// +----------+------------+                |
//            |                             |
// +----------v------------+                |
// |     RunOnTarget       | Resolve(error) |
// |   (Target Thread)     +----------------+
// +----------+------------+                |
//            |                             |
//  +---------v---------+            +------v------+
//  |      Running      |            |  Completing |
//  | (Target Thread)   +------------>(Orig Thread)|
//  +-------------------+            +------+------+
//                                          |
//                                    +-----v----+
//                                    | Complete |
//                                    +----------+
//
// The initialization process proceeds through the main states.  If an error
// occurs, then we transition to Completing state back on the original thread.
NS_IMETHODIMP
Context::QuotaInitRunnable::Run()
{
  // May run on different threads depending on the state.  See individual
  // state cases for thread assertions.

  RefPtr<SyncResolver> resolver = new SyncResolver();

  switch(mState) {
    // -----------------------------------
    case STATE_GET_INFO:
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (mCanceled) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      RefPtr<ManagerId> managerId = mManager->GetManagerId();
      nsCOMPtr<nsIPrincipal> principal = managerId->Principal();
      nsresult rv = QuotaManager::GetInfoFromPrincipal(principal,
                                                       &mQuotaInfo.mSuffix,
                                                       &mQuotaInfo.mGroup,
                                                       &mQuotaInfo.mOrigin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        resolver->Resolve(rv);
        break;
      }

      mState = STATE_CREATE_QUOTA_MANAGER;
      MOZ_ALWAYS_SUCCEEDS(
        mInitiatingEventTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL));
      break;
    }
    // ----------------------------------
    case STATE_CREATE_QUOTA_MANAGER:
    {
      NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);

      if (mCanceled || QuotaManager::IsShuttingDown()) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      if (QuotaManager::Get()) {
        OpenDirectory();
        return NS_OK;
      }

      mState = STATE_OPEN_DIRECTORY;
      QuotaManager::GetOrCreate(this);
      break;
    }
    // ----------------------------------
    case STATE_OPEN_DIRECTORY:
    {
      NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);

      if (NS_WARN_IF(!QuotaManager::Get())) {
        resolver->Resolve(NS_ERROR_FAILURE);
        break;
      }

      OpenDirectory();
      break;
    }
    // ----------------------------------
    case STATE_ENSURE_ORIGIN_INITIALIZED:
    {
      AssertIsOnIOThread();

      if (mCanceled) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      QuotaManager* qm = QuotaManager::Get();
      MOZ_DIAGNOSTIC_ASSERT(qm);
      nsresult rv = qm->EnsureOriginIsInitialized(PERSISTENCE_TYPE_DEFAULT,
                                                  mQuotaInfo.mSuffix,
                                                  mQuotaInfo.mGroup,
                                                  mQuotaInfo.mOrigin,
                                                  getter_AddRefs(mQuotaInfo.mDir));
      if (NS_FAILED(rv)) {
        resolver->Resolve(rv);
        break;
      }

      mState = STATE_RUN_ON_TARGET;

      MOZ_ALWAYS_SUCCEEDS(
        mTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL));
      break;
    }
    // -------------------
    case STATE_RUN_ON_TARGET:
    {
      MOZ_ASSERT(mTarget->IsOnCurrentThread());

      mState = STATE_RUNNING;

      // Execute the provided initialization Action.  The Action must Resolve()
      // before returning.
      mInitAction->RunOnTarget(resolver, mQuotaInfo, mData);
      MOZ_DIAGNOSTIC_ASSERT(resolver->Resolved());

      mData = nullptr;

      // If the database was opened, then we should always succeed when creating
      // the marker file.  If it wasn't opened successfully, then no need to
      // create a marker file anyway.
      if (NS_SUCCEEDED(resolver->Result())) {
        MOZ_ALWAYS_SUCCEEDS(CreateMarkerFile(mQuotaInfo));
      }

      break;
    }
    // -------------------
    case STATE_COMPLETING:
    {
      NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
      mInitAction->CompleteOnInitiatingThread(mResult);
      mContext->OnQuotaInit(mResult, mQuotaInfo, mDirectoryLock.forget());
      mState = STATE_COMPLETE;

      // Explicitly cleanup here as the destructor could fire on any of
      // the threads we have bounced through.
      Clear();
      break;
    }
    // -----
    case STATE_WAIT_FOR_DIRECTORY_LOCK:
    default:
    {
      MOZ_CRASH("unexpected state in QuotaInitRunnable");
    }
  }

  if (resolver->Resolved()) {
    Complete(resolver->Result());
  }

  return NS_OK;
}

// Runnable wrapper around Action objects dispatched on the Context.  This
// runnable executes the Action on the appropriate threads while the Context
// is initialized.
class Context::ActionRunnable final : public nsIRunnable
                                    , public Action::Resolver
                                    , public Context::Activity
{
public:
  ActionRunnable(Context* aContext, Data* aData, nsISerialEventTarget* aTarget,
                 Action* aAction, const QuotaInfo& aQuotaInfo)
    : mContext(aContext)
    , mData(aData)
    , mTarget(aTarget)
    , mAction(aAction)
    , mQuotaInfo(aQuotaInfo)
    , mInitiatingThread(GetCurrentThreadEventTarget())
    , mState(STATE_INIT)
    , mResult(NS_OK)
    , mExecutingRunOnTarget(false)
  {
    MOZ_DIAGNOSTIC_ASSERT(mContext);
    // mData may be nullptr
    MOZ_DIAGNOSTIC_ASSERT(mTarget);
    MOZ_DIAGNOSTIC_ASSERT(mAction);
    // mQuotaInfo.mDir may be nullptr if QuotaInitRunnable failed
    MOZ_DIAGNOSTIC_ASSERT(mInitiatingThread);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_INIT);

    mState = STATE_RUN_ON_TARGET;
    nsresult rv = mTarget->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mState = STATE_COMPLETE;
      Clear();
    }
    return rv;
  }

  virtual bool
  MatchesCacheId(CacheId aCacheId) const override
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    return mAction->MatchesCacheId(aCacheId);
  }

  virtual void
  Cancel() override
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    mAction->CancelOnInitiatingThread();
  }

  virtual void Resolve(nsresult aRv) override
  {
    MOZ_ASSERT(mTarget->IsOnCurrentThread());
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_RUNNING);

    mResult = aRv;

    // We ultimately must complete on the initiating thread, but bounce through
    // the current thread again to ensure that we don't destroy objects and
    // state out from under the currently running action's stack.
    mState = STATE_RESOLVING;

    // If we were resolved synchronously within Action::RunOnTarget() then we
    // can avoid a thread bounce and just resolve once RunOnTarget() returns.
    // The Run() method will handle this by looking at mState after
    // RunOnTarget() returns.
    if (mExecutingRunOnTarget) {
      return;
    }

    // Otherwise we are in an asynchronous resolve.  And must perform a thread
    // bounce to run on the target thread again.
    MOZ_ALWAYS_SUCCEEDS(
      mTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL));
  }

private:
  ~ActionRunnable()
  {
    MOZ_DIAGNOSTIC_ASSERT(mState == STATE_COMPLETE);
    MOZ_DIAGNOSTIC_ASSERT(!mContext);
    MOZ_DIAGNOSTIC_ASSERT(!mAction);
  }

  void Clear()
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    MOZ_DIAGNOSTIC_ASSERT(mContext);
    MOZ_DIAGNOSTIC_ASSERT(mAction);
    mContext->RemoveActivity(this);
    mContext = nullptr;
    mAction = nullptr;
  }

  enum State
  {
    STATE_INIT,
    STATE_RUN_ON_TARGET,
    STATE_RUNNING,
    STATE_RESOLVING,
    STATE_COMPLETING,
    STATE_COMPLETE
  };

  RefPtr<Context> mContext;
  RefPtr<Data> mData;
  nsCOMPtr<nsISerialEventTarget> mTarget;
  RefPtr<Action> mAction;
  const QuotaInfo mQuotaInfo;
  nsCOMPtr<nsIEventTarget> mInitiatingThread;
  State mState;
  nsresult mResult;

  // Only accessible on target thread;
  bool mExecutingRunOnTarget;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS(mozilla::dom::cache::Context::ActionRunnable, nsIRunnable);

// The ActionRunnable has a simpler state machine.  It basically needs to run
// the action on the target thread and then complete on the original thread.
//
//   +-------------+
//   |    Start    |
//   |(Orig Thread)|
//   +-----+-------+
//         |
// +-------v---------+
// |  RunOnTarget    |
// |Target IO Thread)+---+ Resolve()
// +-------+---------+   |
//         |             |
// +-------v----------+  |
// |     Running      |  |
// |(Target IO Thread)|  |
// +------------------+  |
//         | Resolve()   |
// +-------v----------+  |
// |     Resolving    <--+                   +-------------+
// |                  |                      |  Completing |
// |(Target IO Thread)+---------------------->(Orig Thread)|
// +------------------+                      +-------+-----+
//                                                   |
//                                                   |
//                                              +----v---+
//                                              |Complete|
//                                              +--------+
//
// Its important to note that synchronous actions will effectively Resolve()
// out of the Running state immediately.  Asynchronous Actions may remain
// in the Running state for some time, but normally the ActionRunnable itself
// does not see any execution there.  Its all handled internal to the Action.
NS_IMETHODIMP
Context::ActionRunnable::Run()
{
  switch(mState) {
    // ----------------------
    case STATE_RUN_ON_TARGET:
    {
      MOZ_ASSERT(mTarget->IsOnCurrentThread());
      MOZ_DIAGNOSTIC_ASSERT(!mExecutingRunOnTarget);

      // Note that we are calling RunOnTarget().  This lets us detect
      // if Resolve() is called synchronously.
      AutoRestore<bool> executingRunOnTarget(mExecutingRunOnTarget);
      mExecutingRunOnTarget = true;

      mState = STATE_RUNNING;
      mAction->RunOnTarget(this, mQuotaInfo, mData);

      mData = nullptr;

      // Resolve was called synchronously from RunOnTarget().  We can
      // immediately move to completing now since we are sure RunOnTarget()
      // completed.
      if (mState == STATE_RESOLVING) {
        // Use recursion instead of switch case fall-through...  Seems slightly
        // easier to understand.
        Run();
      }

      break;
    }
    // -----------------
    case STATE_RESOLVING:
    {
      MOZ_ASSERT(mTarget->IsOnCurrentThread());
      // The call to Action::RunOnTarget() must have returned now if we
      // are running on the target thread again.  We may now proceed
      // with completion.
      mState = STATE_COMPLETING;
      // Shutdown must be delayed until all Contexts are destroyed.  Crash
      // for this invariant violation.
      MOZ_ALWAYS_SUCCEEDS(
        mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL));
      break;
    }
    // -------------------
    case STATE_COMPLETING:
    {
      NS_ASSERT_OWNINGTHREAD(ActionRunnable);
      mAction->CompleteOnInitiatingThread(mResult);
      mState = STATE_COMPLETE;
      // Explicitly cleanup here as the destructor could fire on any of
      // the threads we have bounced through.
      Clear();
      break;
    }
    // -----------------
    default:
    {
      MOZ_CRASH("unexpected state in ActionRunnable");
      break;
    }
  }
  return NS_OK;
}

void
Context::ThreadsafeHandle::AllowToClose()
{
  if (mOwningEventTarget->IsOnCurrentThread()) {
    AllowToCloseOnOwningThread();
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod(
    "dom::cache::Context::ThreadsafeHandle::AllowToCloseOnOwningThread",
    this,
    &ThreadsafeHandle::AllowToCloseOnOwningThread);
  MOZ_ALWAYS_SUCCEEDS(
    mOwningEventTarget->Dispatch(runnable.forget(), nsIThread::DISPATCH_NORMAL));
}

void
Context::ThreadsafeHandle::InvalidateAndAllowToClose()
{
  if (mOwningEventTarget->IsOnCurrentThread()) {
    InvalidateAndAllowToCloseOnOwningThread();
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod(
    "dom::cache::Context::ThreadsafeHandle::"
    "InvalidateAndAllowToCloseOnOwningThread",
    this,
    &ThreadsafeHandle::InvalidateAndAllowToCloseOnOwningThread);
  MOZ_ALWAYS_SUCCEEDS(
    mOwningEventTarget->Dispatch(runnable.forget(), nsIThread::DISPATCH_NORMAL));
}

Context::ThreadsafeHandle::ThreadsafeHandle(Context* aContext)
  : mStrongRef(aContext)
  , mWeakRef(aContext)
  , mOwningEventTarget(GetCurrentThreadSerialEventTarget())
{
}

Context::ThreadsafeHandle::~ThreadsafeHandle()
{
  // Normally we only touch mStrongRef on the owning thread.  This is safe,
  // however, because when we do use mStrongRef on the owning thread we are
  // always holding a strong ref to the ThreadsafeHandle via the owning
  // runnable.  So we cannot run the ThreadsafeHandle destructor simultaneously.
  if (!mStrongRef || mOwningEventTarget->IsOnCurrentThread()) {
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  NS_ProxyRelease(
    "Context::ThreadsafeHandle::mStrongRef", mOwningEventTarget, mStrongRef.forget());
}

void
Context::ThreadsafeHandle::AllowToCloseOnOwningThread()
{
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());

  // A Context "closes" when its ref count drops to zero.  Dropping this
  // strong ref is necessary, but not sufficient for the close to occur.
  // Any outstanding IO will continue and keep the Context alive.  Once
  // the Context is idle, it will be destroyed.

  // First, tell the context to flush any target thread shared data.  This
  // data must be released on the target thread prior to running the Context
  // destructor.  This will schedule an Action which ensures that the
  // ~Context() is not immediately executed when we drop the strong ref.
  if (mStrongRef) {
    mStrongRef->DoomTargetData();
  }

  // Now drop our strong ref and let Context finish running any outstanding
  // Actions.
  mStrongRef = nullptr;
}

void
Context::ThreadsafeHandle::InvalidateAndAllowToCloseOnOwningThread()
{
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  // Cancel the Context through the weak reference.  This means we can
  // allow the Context to close by dropping the strong ref, but then
  // still cancel ongoing IO if necessary.
  if (mWeakRef) {
    mWeakRef->Invalidate();
  }
  // We should synchronously have AllowToCloseOnOwningThread called when
  // the Context is canceled.
  MOZ_DIAGNOSTIC_ASSERT(!mStrongRef);
}

void
Context::ThreadsafeHandle::ContextDestroyed(Context* aContext)
{
  MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread());
  MOZ_DIAGNOSTIC_ASSERT(!mStrongRef);
  MOZ_DIAGNOSTIC_ASSERT(mWeakRef);
  MOZ_DIAGNOSTIC_ASSERT(mWeakRef == aContext);
  mWeakRef = nullptr;
}

// static
already_AddRefed<Context>
Context::Create(Manager* aManager, nsISerialEventTarget* aTarget,
                Action* aInitAction, Context* aOldContext)
{
  RefPtr<Context> context = new Context(aManager, aTarget, aInitAction);
  context->Init(aOldContext);
  return context.forget();
}

Context::Context(Manager* aManager, nsISerialEventTarget* aTarget, Action* aInitAction)
  : mManager(aManager)
  , mTarget(aTarget)
  , mData(new Data(aTarget))
  , mState(STATE_CONTEXT_PREINIT)
  , mOrphanedData(false)
  , mInitAction(aInitAction)
{
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(mTarget);
}

void
Context::Dispatch(Action* aAction)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(aAction);

  MOZ_DIAGNOSTIC_ASSERT(mState != STATE_CONTEXT_CANCELED);
  if (mState == STATE_CONTEXT_CANCELED) {
    return;
  } else if (mState == STATE_CONTEXT_INIT ||
             mState == STATE_CONTEXT_PREINIT) {
    PendingAction* pending = mPendingActions.AppendElement();
    pending->mAction = aAction;
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_CONTEXT_READY);
  DispatchAction(aAction);
}

void
Context::CancelAll()
{
  NS_ASSERT_OWNINGTHREAD(Context);

  // In PREINIT state we have not dispatch the init action yet.  Just
  // forget it.
  if (mState == STATE_CONTEXT_PREINIT) {
    MOZ_DIAGNOSTIC_ASSERT(!mInitRunnable);
    mInitAction = nullptr;

  // In INIT state we have dispatched the runnable, but not received the
  // async completion yet.  Cancel the runnable, but don't forget about it
  // until we get OnQuotaInit() callback.
  } else if (mState == STATE_CONTEXT_INIT) {
    mInitRunnable->Cancel();
  }

  mState = STATE_CONTEXT_CANCELED;
  mPendingActions.Clear();
  {
    ActivityList::ForwardIterator iter(mActivityList);
    while (iter.HasMore()) {
      iter.GetNext()->Cancel();
    }
  }
  AllowToClose();
}

bool
Context::IsCanceled() const
{
  NS_ASSERT_OWNINGTHREAD(Context);
  return mState == STATE_CONTEXT_CANCELED;
}

void
Context::Invalidate()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  mManager->NoteClosing();
  CancelAll();
}

void
Context::AllowToClose()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  if (mThreadsafeHandle) {
    mThreadsafeHandle->AllowToClose();
  }
}

void
Context::CancelForCacheId(CacheId aCacheId)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  // Remove matching pending actions
  for (int32_t i = mPendingActions.Length() - 1; i >= 0; --i) {
    if (mPendingActions[i].mAction->MatchesCacheId(aCacheId)) {
      mPendingActions.RemoveElementAt(i);
    }
  }

  // Cancel activities and let them remove themselves
  ActivityList::ForwardIterator iter(mActivityList);
  while (iter.HasMore()) {
    Activity* activity = iter.GetNext();
    if (activity->MatchesCacheId(aCacheId)) {
      activity->Cancel();
    }
  }
}

Context::~Context()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  MOZ_DIAGNOSTIC_ASSERT(!mData);

  if (mThreadsafeHandle) {
    mThreadsafeHandle->ContextDestroyed(this);
  }

  // Note, this may set the mOrphanedData flag.
  mManager->RemoveContext(this);

  if (mQuotaInfo.mDir && !mOrphanedData) {
    MOZ_ALWAYS_SUCCEEDS(DeleteMarkerFile(mQuotaInfo));
  }

  if (mNextContext) {
    mNextContext->Start();
  }
}

void
Context::Init(Context* aOldContext)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  if (aOldContext) {
    aOldContext->SetNextContext(this);
    return;
  }

  Start();
}

void
Context::Start()
{
  NS_ASSERT_OWNINGTHREAD(Context);

  // Previous context closing delayed our start, but then we were canceled.
  // In this case, just do nothing here.
  if (mState == STATE_CONTEXT_CANCELED) {
    MOZ_DIAGNOSTIC_ASSERT(!mInitRunnable);
    MOZ_DIAGNOSTIC_ASSERT(!mInitAction);
    // If we can't initialize the quota subsystem we will never be able to
    // clear our shared data object via the target IO thread.  Instead just
    // clear it here to maintain the invariant that the shared data is
    // cleared before Context destruction.
    mData = nullptr;
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_CONTEXT_PREINIT);
  MOZ_DIAGNOSTIC_ASSERT(!mInitRunnable);

  mInitRunnable = new QuotaInitRunnable(this, mManager, mData, mTarget,
                                        mInitAction);
  mInitAction = nullptr;

  mState = STATE_CONTEXT_INIT;

  nsresult rv = mInitRunnable->Dispatch();
  if (NS_FAILED(rv)) {
    // Shutdown must be delayed until all Contexts are destroyed.  Shutdown
    // must also prevent any new Contexts from being constructed.  Crash
    // for this invariant violation.
    MOZ_CRASH("Failed to dispatch QuotaInitRunnable.");
  }
}

void
Context::DispatchAction(Action* aAction, bool aDoomData)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  RefPtr<ActionRunnable> runnable =
    new ActionRunnable(this, mData, mTarget, aAction, mQuotaInfo);

  if (aDoomData) {
    mData = nullptr;
  }

  nsresult rv = runnable->Dispatch();
  if (NS_FAILED(rv)) {
    // Shutdown must be delayed until all Contexts are destroyed.  Crash
    // for this invariant violation.
    MOZ_CRASH("Failed to dispatch ActionRunnable to target thread.");
  }
  AddActivity(runnable);
}

void
Context::OnQuotaInit(nsresult aRv, const QuotaInfo& aQuotaInfo,
                     already_AddRefed<DirectoryLock> aDirectoryLock)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  MOZ_DIAGNOSTIC_ASSERT(mInitRunnable);
  mInitRunnable = nullptr;

  mQuotaInfo = aQuotaInfo;

  // Always save the directory lock to ensure QuotaManager does not shutdown
  // before the Context has gone away.
  MOZ_DIAGNOSTIC_ASSERT(!mDirectoryLock);
  mDirectoryLock = aDirectoryLock;

  // If we opening the context failed, but we were not explicitly canceled,
  // still treat the entire context as canceled.  We don't want to allow
  // new actions to be dispatched.  We also cannot leave the context in
  // the INIT state after failing to open.
  if (NS_FAILED(aRv)) {
    mState = STATE_CONTEXT_CANCELED;
  }

  if (mState == STATE_CONTEXT_CANCELED) {
    for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
      mPendingActions[i].mAction->CompleteOnInitiatingThread(aRv);
    }
    mPendingActions.Clear();
    mThreadsafeHandle->AllowToClose();
    // Context will destruct after return here and last ref is released.
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mState == STATE_CONTEXT_INIT);
  mState = STATE_CONTEXT_READY;

  for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
    DispatchAction(mPendingActions[i].mAction);
  }
  mPendingActions.Clear();
}

void
Context::AddActivity(Activity* aActivity)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(aActivity);
  MOZ_ASSERT(!mActivityList.Contains(aActivity));
  mActivityList.AppendElement(aActivity);
}

void
Context::RemoveActivity(Activity* aActivity)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(aActivity);
  MOZ_ALWAYS_TRUE(mActivityList.RemoveElement(aActivity));
  MOZ_ASSERT(!mActivityList.Contains(aActivity));
}

void
Context::NoteOrphanedData()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  // This may be called more than once
  mOrphanedData = true;
}

already_AddRefed<Context::ThreadsafeHandle>
Context::CreateThreadsafeHandle()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  if (!mThreadsafeHandle) {
    mThreadsafeHandle = new ThreadsafeHandle(this);
  }
  RefPtr<ThreadsafeHandle> ref = mThreadsafeHandle;
  return ref.forget();
}

void
Context::SetNextContext(Context* aNextContext)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(aNextContext);
  MOZ_DIAGNOSTIC_ASSERT(!mNextContext);
  mNextContext = aNextContext;
}

void
Context::DoomTargetData()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_DIAGNOSTIC_ASSERT(mData);

  // We are about to drop our reference to the Data.  We need to ensure that
  // the ~Context() destructor does not run until contents of Data have been
  // released on the Target thread.

  // Dispatch a no-op Action.  This will hold the Context alive through a
  // roundtrip to the target thread and back to the owning thread.  The
  // ref to the Data object is cleared on the owning thread after creating
  // the ActionRunnable, but before dispatching it.
  RefPtr<Action> action = new NullAction();
  DispatchAction(action, true /* doomed data */);

  MOZ_DIAGNOSTIC_ASSERT(!mData);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
