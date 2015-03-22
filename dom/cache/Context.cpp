/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/Context.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/cache/Action.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/dom/quota/OriginOrPatternString.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace {

using mozilla::dom::Nullable;
using mozilla::dom::cache::QuotaInfo;
using mozilla::dom::quota::OriginOrPatternString;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

// Executed when the context is destroyed to release our lock on the
// QuotaManager.
class QuotaReleaseRunnable final : public nsRunnable
{
public:
  QuotaReleaseRunnable(const QuotaInfo& aQuotaInfo, const nsACString& aQuotaId)
    : mQuotaInfo(aQuotaInfo)
    , mQuotaId(aQuotaId)
  { }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    QuotaManager* qm = QuotaManager::Get();
    MOZ_ASSERT(qm);
    qm->AllowNextSynchronizedOp(OriginOrPatternString::FromOrigin(mQuotaInfo.mOrigin),
                                Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT),
                                mQuotaId);
    return NS_OK;
  }

private:
  ~QuotaReleaseRunnable() { }

  const QuotaInfo mQuotaInfo;
  const nsCString mQuotaId;
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

// Executed to perform the complicated dance of steps necessary to initialize
// the QuotaManager.  This must be performed for each origin before any disk
// IO occurrs.
class Context::QuotaInitRunnable final : public nsIRunnable
                                           , public Action::Resolver
{
public:
  QuotaInitRunnable(Context* aContext,
                    Manager* aManager,
                    const nsACString& aQuotaId,
                    Action* aQuotaIOThreadAction)
    : mContext(aContext)
    , mManager(aManager)
    , mQuotaId(aQuotaId)
    , mQuotaIOThreadAction(aQuotaIOThreadAction)
    , mInitiatingThread(NS_GetCurrentThread())
    , mState(STATE_INIT)
    , mResult(NS_OK)
  {
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mManager);
    MOZ_ASSERT(mInitiatingThread);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    MOZ_ASSERT(mState == STATE_INIT);

    mState = STATE_CALL_WAIT_FOR_OPEN_ALLOWED;
    nsresult rv = NS_DispatchToMainThread(this, nsIThread::DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mState = STATE_COMPLETE;
      Clear();
    }
    return rv;
  }

  virtual void Resolve(nsresult aRv) override
  {
    // Depending on the error or success path, this can run on either the
    // main thread or the QuotaManager IO thread.  The IO thread is an
    // idle thread which may be destroyed and recreated, so its hard to
    // assert on.
    MOZ_ASSERT(mState == STATE_RUNNING || NS_FAILED(aRv));

    mResult = aRv;
    mState = STATE_COMPLETING;

    nsresult rv = mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      // Shutdown must be delayed until all Contexts are destroyed.  Crash for
      // this invariant violation.
      MOZ_CRASH("Failed to dispatch QuotaInitRunnable to initiating thread.");
    }
  }

private:
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
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    MOZ_ASSERT(mContext);
    mContext = nullptr;
    mManager = nullptr;
    mQuotaIOThreadAction = nullptr;
  }

  nsRefPtr<Context> mContext;
  nsRefPtr<Manager> mManager;
  const nsCString mQuotaId;
  nsRefPtr<Action> mQuotaIOThreadAction;
  nsCOMPtr<nsIThread> mInitiatingThread;
  State mState;
  nsresult mResult;
  QuotaInfo mQuotaInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS_INHERITED(mozilla::dom::cache::Context::QuotaInitRunnable,
                            Action::Resolver, nsIRunnable);

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
//  |      Running      |  Resolve() |  Completing |
//  | (Quota IO Thread) +------------>(Orig Thread)|
//  +-------------------+            +------+------+
//                                          |
//                                    +-----v----+
//                                    | Complete |
//                                    +----------+
//
// The initialization process proceeds through the main states.  If an error
// occurs, then we transition back to Completing state back on the original
// thread.
NS_IMETHODIMP
Context::QuotaInitRunnable::Run()
{
  // May run on different threads depending on the state.  See individual
  // state cases for thread assertions.

  switch(mState) {
    // -----------------------------------
    case STATE_CALL_WAIT_FOR_OPEN_ALLOWED:
    {
      MOZ_ASSERT(NS_IsMainThread());
      QuotaManager* qm = QuotaManager::GetOrCreate();
      if (!qm) {
        Resolve(NS_ERROR_FAILURE);
        return NS_OK;
      }

      nsRefPtr<ManagerId> managerId = mManager->GetManagerId();
      nsCOMPtr<nsIPrincipal> principal = managerId->Principal();
      nsresult rv = qm->GetInfoFromPrincipal(principal,
                                             &mQuotaInfo.mGroup,
                                             &mQuotaInfo.mOrigin,
                                             &mQuotaInfo.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Resolve(rv);
        return NS_OK;
      }

      // QuotaManager::WaitForOpenAllowed() will hold a reference to us as
      // a callback.  We will then get executed again on the main thread when
      // it is safe to open the quota directory.
      mState = STATE_WAIT_FOR_OPEN_ALLOWED;
      rv = qm->WaitForOpenAllowed(OriginOrPatternString::FromOrigin(mQuotaInfo.mOrigin),
                                  Nullable<PersistenceType>(PERSISTENCE_TYPE_DEFAULT),
                                  mQuotaId, this);
      if (NS_FAILED(rv)) {
        Resolve(rv);
        return NS_OK;
      }
      break;
    }
    // ------------------------------
    case STATE_WAIT_FOR_OPEN_ALLOWED:
    {
      MOZ_ASSERT(NS_IsMainThread());
      QuotaManager* qm = QuotaManager::Get();
      MOZ_ASSERT(qm);
      mState = STATE_ENSURE_ORIGIN_INITIALIZED;
      nsresult rv = qm->IOThread()->Dispatch(this, nsIThread::DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        Resolve(rv);
        return NS_OK;
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

      QuotaManager* qm = QuotaManager::Get();
      MOZ_ASSERT(qm);
      nsresult rv = qm->EnsureOriginIsInitialized(PERSISTENCE_TYPE_DEFAULT,
                                                  mQuotaInfo.mGroup,
                                                  mQuotaInfo.mOrigin,
                                                  mQuotaInfo.mIsApp,
                                                  getter_AddRefs(mQuotaInfo.mDir));
      if (NS_FAILED(rv)) {
        Resolve(rv);
        return NS_OK;
      }

      mState = STATE_RUNNING;

      if (!mQuotaIOThreadAction) {
        Resolve(NS_OK);
        return NS_OK;
      }

      // Execute the provided initialization Action.  We pass ourselves as the
      // Resolver.  The Action must either call Resolve() immediately or hold
      // a ref to us and call Resolve() later.
      mQuotaIOThreadAction->RunOnTarget(this, mQuotaInfo);

      break;
    }
    // -------------------
    case STATE_COMPLETING:
    {
      NS_ASSERT_OWNINGTHREAD(Action::Resolver);
      if (mQuotaIOThreadAction) {
        mQuotaIOThreadAction->CompleteOnInitiatingThread(mResult);
      }
      mContext->OnQuotaInit(mResult, mQuotaInfo);
      mState = STATE_COMPLETE;
      // Explicitly cleanup here as the destructor could fire on any of
      // the threads we have bounced through.
      Clear();
      break;
    }
    // -----
    default:
    {
      MOZ_CRASH("unexpected state in QuotaInitRunnable");
      break;
    }
  }

  return NS_OK;
}

// Runnable wrapper around Action objects dispatched on the Context.  This
// runnable executes the Action on the appropriate threads while the Context
// is initialized.
class Context::ActionRunnable final : public nsIRunnable
                                    , public Action::Resolver
{
public:
  ActionRunnable(Context* aContext, nsIEventTarget* aTarget, Action* aAction,
                 const QuotaInfo& aQuotaInfo)
    : mContext(aContext)
    , mTarget(aTarget)
    , mAction(aAction)
    , mQuotaInfo(aQuotaInfo)
    , mInitiatingThread(NS_GetCurrentThread())
    , mState(STATE_INIT)
    , mResult(NS_OK)
  {
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mTarget);
    MOZ_ASSERT(mAction);
    MOZ_ASSERT(mQuotaInfo.mDir);
    MOZ_ASSERT(mInitiatingThread);
  }

  nsresult Dispatch()
  {
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    MOZ_ASSERT(mState == STATE_INIT);

    mState = STATE_RUN_ON_TARGET;
    nsresult rv = mTarget->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mState = STATE_COMPLETE;
      Clear();
    }
    return rv;
  }

  bool MatchesCacheId(CacheId aCacheId) {
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    return mAction->MatchesCacheId(aCacheId);
  }

  void Cancel()
  {
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    mAction->CancelOnInitiatingThread();
  }

  virtual void Resolve(nsresult aRv) override
  {
    MOZ_ASSERT(mTarget == NS_GetCurrentThread());
    MOZ_ASSERT(mState == STATE_RUNNING);
    mResult = aRv;
    mState = STATE_COMPLETING;
    nsresult rv = mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      // Shutdown must be delayed until all Contexts are destroyed.  Crash
      // for this invariant violation.
      MOZ_CRASH("Failed to dispatch ActionRunnable to initiating thread.");
    }
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
    NS_ASSERT_OWNINGTHREAD(Action::Resolver);
    MOZ_ASSERT(mContext);
    MOZ_ASSERT(mAction);
    mContext->OnActionRunnableComplete(this);
    mContext = nullptr;
    mAction = nullptr;
  }

  enum State
  {
    STATE_INIT,
    STATE_RUN_ON_TARGET,
    STATE_RUNNING,
    STATE_COMPLETING,
    STATE_COMPLETE
  };

  nsRefPtr<Context> mContext;
  nsCOMPtr<nsIEventTarget> mTarget;
  nsRefPtr<Action> mAction;
  const QuotaInfo mQuotaInfo;
  nsCOMPtr<nsIThread> mInitiatingThread;
  State mState;
  nsresult mResult;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS_INHERITED(mozilla::dom::cache::Context::ActionRunnable,
                            Action::Resolver, nsIRunnable);

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
// |Target IO Thread)+-------------------------------+
// +-------+---------+                               |
//         |                                         |
// +-------v----------+ Resolve()            +-------v-----+
// |     Running      |                      |  Completing |
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
      mState = STATE_RUNNING;
      mAction->RunOnTarget(this, mQuotaInfo);
      break;
    }
    // -------------------
    case STATE_COMPLETING:
    {
      NS_ASSERT_OWNINGTHREAD(Action::Resolver);
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

// static
already_AddRefed<Context>
Context::Create(Manager* aManager, Action* aQuotaIOThreadAction)
{
  nsRefPtr<Context> context = new Context(aManager);

  nsRefPtr<QuotaInitRunnable> runnable =
    new QuotaInitRunnable(context, aManager, NS_LITERAL_CSTRING("Cache"),
                          aQuotaIOThreadAction);
  nsresult rv = runnable->Dispatch();
  if (NS_FAILED(rv)) {
    // Shutdown must be delayed until all Contexts are destroyed.  Shutdown
    // must also prevent any new Contexts from being constructed.  Crash
    // for this invariant violation.
    MOZ_CRASH("Failed to dispatch QuotaInitRunnable.");
  }

  return context.forget();
}

Context::Context(Manager* aManager)
  : mManager(aManager)
  , mState(STATE_CONTEXT_INIT)
{
  MOZ_ASSERT(mManager);
}

void
Context::Dispatch(nsIEventTarget* aTarget, Action* aAction)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(aTarget);
  MOZ_ASSERT(aAction);

  if (mState == STATE_CONTEXT_CANCELED) {
    return;
  } else if (mState == STATE_CONTEXT_INIT) {
    PendingAction* pending = mPendingActions.AppendElement();
    pending->mTarget = aTarget;
    pending->mAction = aAction;
    return;
  }

  MOZ_ASSERT(STATE_CONTEXT_READY);
  DispatchAction(aTarget, aAction);
}

void
Context::CancelAll()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  mState = STATE_CONTEXT_CANCELED;
  mPendingActions.Clear();
  for (uint32_t i = 0; i < mActionRunnables.Length(); ++i) {
    nsRefPtr<ActionRunnable> runnable = mActionRunnables[i];
    runnable->Cancel();
  }
}

void
Context::CancelForCacheId(CacheId aCacheId)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
    if (mPendingActions[i].mAction->MatchesCacheId(aCacheId)) {
      mPendingActions.RemoveElementAt(i);
    }
  }
  for (uint32_t i = 0; i < mActionRunnables.Length(); ++i) {
    nsRefPtr<ActionRunnable> runnable = mActionRunnables[i];
    if (runnable->MatchesCacheId(aCacheId)) {
      runnable->Cancel();
    }
  }
}

Context::~Context()
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(mManager);

  // Unlock the quota dir as we go out of scope.
  nsCOMPtr<nsIRunnable> runnable =
    new QuotaReleaseRunnable(mQuotaInfo, NS_LITERAL_CSTRING("Cache"));
  nsresult rv = NS_DispatchToMainThread(runnable, nsIThread::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    // Shutdown must be delayed until all Contexts are destroyed.  Crash
    // for this invariant violation.
    MOZ_CRASH("Failed to dispatch QuotaReleaseRunnable to main thread.");
  }

  mManager->RemoveContext(this);
}

void
Context::DispatchAction(nsIEventTarget* aTarget, Action* aAction)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  nsRefPtr<ActionRunnable> runnable =
    new ActionRunnable(this, aTarget, aAction, mQuotaInfo);
  nsresult rv = runnable->Dispatch();
  if (NS_FAILED(rv)) {
    // Shutdown must be delayed until all Contexts are destroyed.  Crash
    // for this invariant violation.
    MOZ_CRASH("Failed to dispatch ActionRunnable to target thread.");
  }
  mActionRunnables.AppendElement(runnable);
}

void
Context::OnQuotaInit(nsresult aRv, const QuotaInfo& aQuotaInfo)
{
  NS_ASSERT_OWNINGTHREAD(Context);

  mQuotaInfo = aQuotaInfo;

  if (mState == STATE_CONTEXT_CANCELED || NS_FAILED(aRv)) {
    for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
      mPendingActions[i].mAction->CompleteOnInitiatingThread(aRv);
    }
    mPendingActions.Clear();
    // Context will destruct after return here and last ref is released.
    return;
  }

  MOZ_ASSERT(mState == STATE_CONTEXT_INIT);
  mState = STATE_CONTEXT_READY;

  for (uint32_t i = 0; i < mPendingActions.Length(); ++i) {
    DispatchAction(mPendingActions[i].mTarget, mPendingActions[i].mAction);
  }
  mPendingActions.Clear();
}

void
Context::OnActionRunnableComplete(ActionRunnable* aActionRunnable)
{
  NS_ASSERT_OWNINGTHREAD(Context);
  MOZ_ASSERT(aActionRunnable);
  MOZ_ALWAYS_TRUE(mActionRunnables.RemoveElement(aActionRunnable));
}

} // namespace cache
} // namespace dom
} // namespace mozilla
