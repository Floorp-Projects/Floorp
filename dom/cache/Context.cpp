/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Context.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/cache/Action.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/cache/OfflineStorage.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozIStorageConnection.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace {

using mozilla::dom::Nullable;
using mozilla::dom::cache::Action;
using mozilla::dom::cache::QuotaInfo;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::OriginOrPatternString;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

// Release our lock on the QuotaManager directory asynchronously.
class QuotaReleaseRunnable final : public nsRunnable
{
public:
  explicit QuotaReleaseRunnable(const QuotaInfo& aQuotaInfo)
    : mQuotaInfo(aQuotaInfo)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    QuotaManager* qm = QuotaManager::Get();
    MOZ_ASSERT(qm);
    qm->AllowNextSynchronizedOp(OriginOrPatternString::FromOrigin(mQuotaInfo.mOrigin),
                                Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT),
                                mQuotaInfo.mStorageId);
    return NS_OK;
  }

private:
  ~QuotaReleaseRunnable() { }

  const QuotaInfo mQuotaInfo;
};

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
    MOZ_ASSERT(aResolver);
    aResolver->Resolve(NS_OK);
  }
};

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::DebugOnly;
using mozilla::dom::quota::OriginOrPatternString;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

class Context::Data final : public Action::Data
{
public:
  explicit Data(nsIThread* aTarget)
    : mTarget(aTarget)
  {
    MOZ_ASSERT(mTarget);
  }

  virtual mozIStorageConnection*
  GetConnection() const override
  {
    MOZ_ASSERT(mTarget == NS_GetCurrentThread());
    return mConnection;
  }

  virtual void
  SetConnection(mozIStorageConnection* aConn) override
  {
    MOZ_ASSERT(mTarget == NS_GetCurrentThread());
    MOZ_ASSERT(!mConnection);
    mConnection = aConn;
    MOZ_ASSERT(mConnection);
  }

private:
  ~Data()
  {
    if (mConnection) {
      NS_ProxyRelease(mTarget, mConnection);
    }
  }

  nsCOMPtr<nsIThread> mTarget;
  nsCOMPtr<mozIStorageConnection> mConnection;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Context::Data)
};

// Executed to perform the complicated dance of steps necessary to initialize
// the QuotaManager.  This must be performed for each origin before any disk
// IO occurrs.
class Context::QuotaInitRunnable final : public nsIRunnable
{
public:
  QuotaInitRunnable(Context* aContext,
                    Manager* aManager,
                    Action* aQuotaIOThreadAction)
    : mContext(aContext)
    , mThreadsafeHandle(aContext->CreateThreadsafeHandle())
    , mManager(aManager)
    , mQuotaIOThreadAction(aQuotaIOThreadAction)
    , mInitiatingThread(NS_GetCurrentThread())
    , mResult(NS_OK)
    , mState(STATE_INIT)
    , mCanceled(false)
    , mNeedsQuotaRelease(false)
  {
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mManager);
    MOZ_ASSERT(mInitiatingThread);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
    MOZ_ASSERT(mState == STATE_INIT);

    mState = STATE_CALL_WAIT_FOR_OPEN_ALLOWED;
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
    MOZ_ASSERT(!mCanceled);
    mCanceled = true;
    mQuotaIOThreadAction->CancelOnInitiatingThread();
  }

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
      MOZ_ASSERT(!mResolved);
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
    MOZ_ASSERT(mState == STATE_COMPLETE);
    MOZ_ASSERT(!mContext);
    MOZ_ASSERT(!mQuotaIOThreadAction);
  }

  enum State
  {
    STATE_INIT,
    STATE_CALL_WAIT_FOR_OPEN_ALLOWED,
    STATE_WAIT_FOR_OPEN_ALLOWED,
    STATE_ENSURE_ORIGIN_INITIALIZED,
    STATE_RUNNING,
    STATE_COMPLETING,
    STATE_COMPLETE
  };

  void Clear()
  {
    NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
    MOZ_ASSERT(mContext);
    mContext = nullptr;
    mManager = nullptr;
    mQuotaIOThreadAction = nullptr;
  }

  nsRefPtr<Context> mContext;
  nsRefPtr<ThreadsafeHandle> mThreadsafeHandle;
  nsRefPtr<Manager> mManager;
  nsRefPtr<Action> mQuotaIOThreadAction;
  nsCOMPtr<nsIThread> mInitiatingThread;
  nsresult mResult;
  QuotaInfo mQuotaInfo;
  nsMainThreadPtrHandle<OfflineStorage> mOfflineStorage;
  State mState;
  Atomic<bool> mCanceled;
  bool mNeedsQuotaRelease;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS(mozilla::dom::cache::Context::QuotaInitRunnable, nsIRunnable);

// The QuotaManager init state machine is represented in the following diagram:
//
//    +---------------+
//    |     Start     |      Resolve(error)
//    | (Orig Thread) +---------------------+
//    +-------+-------+                     |
//            |                             |
// +----------v-----------+                 |
// |CallWaitForOpenAllowed|  Resolve(error) |
// |    (Main Thread)     +-----------------+
// +----------+-----------+                 |
//            |                             |
//   +--------v---------+                   |
//   |WaitForOpenAllowed|    Resolve(error) |
//   |  (Main Thread)   +-------------------+
//   +--------+---------+                   |
//            |                             |
// +----------v------------+                |
// |EnsureOriginInitialized| Resolve(error) |
// |   (Quota IO Thread)   +----------------+
// +----------+------------+                |
//            |                             |
//  +---------v---------+            +------v------+
//  |      Running      |            |  Completing |
//  | (Quota IO Thread) +------------>(Orig Thread)|
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

  nsRefPtr<SyncResolver> resolver = new SyncResolver();

  switch(mState) {
    // -----------------------------------
    case STATE_CALL_WAIT_FOR_OPEN_ALLOWED:
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (mCanceled) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      QuotaManager* qm = QuotaManager::GetOrCreate();
      if (!qm) {
        resolver->Resolve(NS_ERROR_FAILURE);
        break;
      }

      nsRefPtr<ManagerId> managerId = mManager->GetManagerId();
      nsCOMPtr<nsIPrincipal> principal = managerId->Principal();
      nsresult rv = qm->GetInfoFromPrincipal(principal,
                                             &mQuotaInfo.mGroup,
                                             &mQuotaInfo.mOrigin,
                                             &mQuotaInfo.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        resolver->Resolve(rv);
        break;
      }

      QuotaManager::GetStorageId(PERSISTENCE_TYPE_DEFAULT,
                                 mQuotaInfo.mOrigin,
                                 Client::DOMCACHE,
                                 NS_LITERAL_STRING("cache"),
                                 mQuotaInfo.mStorageId);

      // QuotaManager::WaitForOpenAllowed() will hold a reference to us as
      // a callback.  We will then get executed again on the main thread when
      // it is safe to open the quota directory.
      mState = STATE_WAIT_FOR_OPEN_ALLOWED;
      rv = qm->WaitForOpenAllowed(OriginOrPatternString::FromOrigin(mQuotaInfo.mOrigin),
                                  Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT),
                                  mQuotaInfo.mStorageId, this);
      if (NS_FAILED(rv)) {
        resolver->Resolve(rv);
        break;
      }
      break;
    }
    // ------------------------------
    case STATE_WAIT_FOR_OPEN_ALLOWED:
    {
      MOZ_ASSERT(NS_IsMainThread());

      mNeedsQuotaRelease = true;

      if (mCanceled) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      QuotaManager* qm = QuotaManager::Get();
      MOZ_ASSERT(qm);

      nsRefPtr<OfflineStorage> offlineStorage =
        OfflineStorage::Register(mThreadsafeHandle, mQuotaInfo);
      mOfflineStorage = new nsMainThreadPtrHolder<OfflineStorage>(offlineStorage);

      mState = STATE_ENSURE_ORIGIN_INITIALIZED;
      nsresult rv = qm->IOThread()->Dispatch(this, nsIThread::DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        resolver->Resolve(rv);
        break;
      }
      break;
    }
    // ----------------------------------
    case STATE_ENSURE_ORIGIN_INITIALIZED:
    {
      // Can't assert quota IO thread because its an idle thread that can get
      // recreated.  At least assert we're not on main thread or owning thread.
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(_mOwningThread.GetThread() != PR_GetCurrentThread());

      if (mCanceled) {
        resolver->Resolve(NS_ERROR_ABORT);
        break;
      }

      QuotaManager* qm = QuotaManager::Get();
      MOZ_ASSERT(qm);
      nsresult rv = qm->EnsureOriginIsInitialized(PERSISTENCE_TYPE_DEFAULT,
                                                  mQuotaInfo.mGroup,
                                                  mQuotaInfo.mOrigin,
                                                  mQuotaInfo.mIsApp,
                                                  getter_AddRefs(mQuotaInfo.mDir));
      if (NS_FAILED(rv)) {
        resolver->Resolve(rv);
        break;
      }

      mState = STATE_RUNNING;

      if (!mQuotaIOThreadAction) {
        resolver->Resolve(NS_OK);
        break;
      }

      // Execute the provided initialization Action.  The Action must Resolve()
      // before returning.
      mQuotaIOThreadAction->RunOnTarget(resolver, mQuotaInfo, nullptr);
      MOZ_ASSERT(resolver->Resolved());

      break;
    }
    // -------------------
    case STATE_COMPLETING:
    {
      NS_ASSERT_OWNINGTHREAD(QuotaInitRunnable);
      if (mQuotaIOThreadAction) {
        mQuotaIOThreadAction->CompleteOnInitiatingThread(mResult);
      }
      mContext->OnQuotaInit(mResult, mQuotaInfo, mOfflineStorage);
      mState = STATE_COMPLETE;

      if (mNeedsQuotaRelease) {
        // Unlock the quota dir if we locked it previously
        nsCOMPtr<nsIRunnable> runnable = new QuotaReleaseRunnable(mQuotaInfo);
        MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
      }

      // Explicitly cleanup here as the destructor could fire on any of
      // the threads we have bounced through.
      Clear();
      break;
    }
    // -----
    default:
    {
      MOZ_CRASH("unexpected state in QuotaInitRunnable");
    }
  }

  if (resolver->Resolved()) {
    MOZ_ASSERT(mState == STATE_RUNNING || NS_FAILED(resolver->Result()));

    MOZ_ASSERT(NS_SUCCEEDED(mResult));
    mResult = resolver->Result();

    mState = STATE_COMPLETING;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL)));
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
  ActionRunnable(Context* aContext, Data* aData, nsIEventTarget* aTarget,
                 Action* aAction, const QuotaInfo& aQuotaInfo)
    : mContext(aContext)
    , mData(aData)
    , mTarget(aTarget)
    , mAction(aAction)
    , mQuotaInfo(aQuotaInfo)
    , mInitiatingThread(NS_GetCurrentThread())
    , mState(STATE_INIT)
    , mResult(NS_OK)
    , mExecutingRunOnTarget(false)
  {
    MOZ_ASSERT(mContext);
    // mData may be nullptr
    MOZ_ASSERT(mTarget);
    MOZ_ASSERT(mAction);
    MOZ_ASSERT(mQuotaInfo.mDir);
    MOZ_ASSERT(mInitiatingThread);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    MOZ_ASSERT(mState == STATE_INIT);

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
    MOZ_ASSERT(mTarget == NS_GetCurrentThread());
    MOZ_ASSERT(mState == STATE_RUNNING);

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
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      mTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL)));
  }

private:
  ~ActionRunnable()
  {
    MOZ_ASSERT(mState == STATE_COMPLETE);
    MOZ_ASSERT(!mContext);
    MOZ_ASSERT(!mAction);
  }

  void Clear()
  {
    NS_ASSERT_OWNINGTHREAD(ActionRunnable);
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mAction);
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

  nsRefPtr<Context> mContext;
  nsRefPtr<Data> mData;
  nsCOMPtr<nsIEventTarget> mTarget;
  nsRefPtr<Action> mAction;
  const QuotaInfo mQuotaInfo;
  nsCOMPtr<nsIThread> mInitiatingThread;
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
      MOZ_ASSERT(NS_GetCurrentThread() == mTarget);
      MOZ_ASSERT(!mExecutingRunOnTarget);

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
      MOZ_ASSERT(NS_GetCurrentThread() == mTarget);
      // The call to Action::RunOnTarget() must have returned now if we
      // are running on the target thread again.  We may now proceed
      // with completion.
      mState = STATE_COMPLETING;
      // Shutdown must be delayed until all Contexts are destroyed.  Crash
      // for this invariant violation.
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
        mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL)));
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
  if (mOwningThread == NS_GetCurrentThread()) {
    AllowToCloseOnOwningThread();
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &ThreadsafeHandle::AllowToCloseOnOwningThread);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
}

void
Context::ThreadsafeHandle::InvalidateAndAllowToClose()
{
  if (mOwningThread == NS_GetCurrentThread()) {
    InvalidateAndAllowToCloseOnOwningThread();
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &ThreadsafeHandle::InvalidateAndAllowToCloseOnOwningThread);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
}

Context::ThreadsafeHandle::ThreadsafeHandle(Context* aContext)
  : mStrongRef(aContext)
  , mWeakRef(aContext)
  , mOwningThread(NS_GetCurrentThread())
{
}

Context::ThreadsafeHandle::~ThreadsafeHandle()
{
  // Normally we only touch mStrongRef on the owning thread.  This is safe,
  // however, because when we do use mStrongRef on the owning thread we are
  // always holding a strong ref to the ThreadsafeHandle via the owning
  // runnable.  So we cannot run the ThreadsafeHandle destructor simultaneously.
  if (!mStrongRef || mOwningThread == NS_GetCurrentThread()) {
    return;
  }

  // Dispatch is guaranteed to succeed here because we block shutdown until
  // all Contexts have been destroyed.
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewNonOwningRunnableMethod(mStrongRef.forget().take(), &Context::Release);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    mOwningThread->Dispatch(runnable, nsIThread::DISPATCH_NORMAL)));
}

void
Context::ThreadsafeHandle::AllowToCloseOnOwningThread()
{
  MOZ_ASSERT(mOwningThread == NS_GetCurrentThread());

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
  MOZ_ASSERT(mOwningThread == NS_GetCurrentThread());
  // Cancel the Context through the weak reference.  This means we can
  // allow the Context to close by dropping the strong ref, but then
  // still cancel ongoing IO if necessary.
  if (mWeakRef) {
    mWeakRef->Invalidate();
  }
  // We should synchronously have AllowToCloseOnOwningThread called when
  // the Context is canceled.
  MOZ_ASSERT(!mStrongRef);
}

void
Context::ThreadsafeHandle::ContextDestroyed(Context* aContext)
{
  MOZ_ASSERT(mOwningThread == NS_GetCurrentThread());
  MOZ_ASSERT(!mStrongRef);
  MOZ_ASSERT(mWeakRef);
  MOZ_ASSERT(mWeakRef == aContext);
  mWeakRef = nullptr;
}

// static
already_AddRefed<Context>
Context::Create(Manager* aManager, nsIThread* aTarget,
                Action* aQuotaIOThreadAction, Context* aOldContext)
{
  nsRefPtr<Context> context = new Context(aManager, aTarget);

  // Do this here to avoid doing an AddRef() in the constructor
  // TODO: pass context->mData to allow connetion sharing with init
  context->mInitRunnable = new QuotaInitRunnable(context, aManager,
                                                 aQuotaIOThreadAction);

  if (aOldContext) {
    aOldContext->SetNextContext(context);
  } else {
    context->Start();
  }

  return context.forget();
}

Context::Context(Manager* aManager, nsIThread* aTarget)
  : mManager(aManager)
  , mTarget(aTarget)
  , mData(new Data(aTarget))
  , mState(STATE_CONTEXT_PREINIT)
{
  MOZ_ASSERT(mManager);
}

void
Context::Dispatch(Action* aAction)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(aAction);

  MOZ_ASSERT(mState != STATE_CONTEXT_CANCELED);
  if (mState == STATE_CONTEXT_CANCELED) {
    return;
  } else if (mState == STATE_CONTEXT_INIT ||
             mState == STATE_CONTEXT_PREINIT) {
    PendingAction* pending = mPendingActions.AppendElement();
    pending->mAction = aAction;
    return;
  }

  MOZ_ASSERT(STATE_CONTEXT_READY);
  DispatchAction(aAction);
}

void
Context::CancelAll()
{
  NS_ASSERT_OWNINGTHREAD(Context);

  // In PREINIT state we have not dispatch the init runnable yet.  Just
  // forget it.
  if (mState == STATE_CONTEXT_PREINIT) {
    mInitRunnable = nullptr;

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
  MOZ_ASSERT(mManager);

  if (mThreadsafeHandle) {
    mThreadsafeHandle->ContextDestroyed(this);
  }

  mManager->RemoveContext(this);

  if (mNextContext) {
    mNextContext->Start();
  }
}

void
Context::Start()
{
  NS_ASSERT_OWNINGTHREAD(Context);

  // Previous context closing delayed our start, but then we were canceled.
  // In this case, just do nothing here.
  if (mState == STATE_CONTEXT_CANCELED) {
    MOZ_ASSERT(!mInitRunnable);
    return;
  }

  MOZ_ASSERT(mState == STATE_CONTEXT_PREINIT);
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

  nsRefPtr<ActionRunnable> runnable =
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
                     nsMainThreadPtrHandle<OfflineStorage>& aOfflineStorage)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  MOZ_ASSERT(mInitRunnable);
  mInitRunnable = nullptr;

  mQuotaInfo = aQuotaInfo;

  // Always save the offline storage to ensure QuotaManager does not shutdown
  // before the Context has gone away.
  MOZ_ASSERT(!mOfflineStorage);
  mOfflineStorage = aOfflineStorage;

  if (mState == STATE_CONTEXT_CANCELED || NS_FAILED(aRv)) {
    for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
      mPendingActions[i].mAction->CompleteOnInitiatingThread(aRv);
    }
    mPendingActions.Clear();
    mThreadsafeHandle->AllowToClose();
    // Context will destruct after return here and last ref is released.
    return;
  }

  MOZ_ASSERT(mState == STATE_CONTEXT_INIT);
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
  MOZ_ASSERT(aActivity);
  MOZ_ASSERT(!mActivityList.Contains(aActivity));
  mActivityList.AppendElement(aActivity);
}

void
Context::RemoveActivity(Activity* aActivity)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(aActivity);
  MOZ_ALWAYS_TRUE(mActivityList.RemoveElement(aActivity));
  MOZ_ASSERT(!mActivityList.Contains(aActivity));
}

already_AddRefed<Context::ThreadsafeHandle>
Context::CreateThreadsafeHandle()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  if (!mThreadsafeHandle) {
    mThreadsafeHandle = new ThreadsafeHandle(this);
  }
  nsRefPtr<ThreadsafeHandle> ref = mThreadsafeHandle;
  return ref.forget();
}

void
Context::SetNextContext(Context* aNextContext)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(aNextContext);
  MOZ_ASSERT(!mNextContext);
  mNextContext = aNextContext;
}

void
Context::DoomTargetData()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(mData);

  // We are about to drop our reference to the Data.  We need to ensure that
  // the ~Context() destructor does not run until contents of Data have been
  // released on the Target thread.

  // Dispatch a no-op Action.  This will hold the Context alive through a
  // roundtrip to the target thread and back to the owning thread.  The
  // ref to the Data object is cleared on the owning thread after creating
  // the ActionRunnable, but before dispatching it.
  nsRefPtr<Action> action = new NullAction();
  DispatchAction(action, true /* doomed data */);

  MOZ_ASSERT(!mData);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
