/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannel.h"
#include "BroadcastChannelChild.h"
#include "mozilla/dom/BroadcastChannelBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"

#include "nsIBFCacheEntry.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace ipc;

class BroadcastChannelMessage final : public StructuredCloneDataNoTransfers
{
public:
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelMessage)

  BroadcastChannelMessage()
    : StructuredCloneDataNoTransfers()
  {}

private:
  ~BroadcastChannelMessage()
  {}
};

namespace {

nsIPrincipal*
GetPrincipalFromThreadSafeWorkerRef(ThreadSafeWorkerRef* aWorkerRef)
{
  nsIPrincipal* principal = aWorkerRef->Private()->GetPrincipal();
  if (principal) {
    return principal;
  }

  // Walk up to our containing page
  WorkerPrivate* wp = aWorkerRef->Private();
  while (wp->GetParent()) {
    wp = wp->GetParent();
  }

  return wp->GetPrincipal();
}

class InitializeRunnable final : public WorkerMainThreadRunnable
{
public:
  InitializeRunnable(ThreadSafeWorkerRef* aWorkerRef, nsACString& aOrigin,
                     PrincipalInfo& aPrincipalInfo, ErrorResult& aRv)
    : WorkerMainThreadRunnable(aWorkerRef->Private(),
                               NS_LITERAL_CSTRING("BroadcastChannel :: Initialize"))
    , mWorkerRef(aWorkerRef)
    , mOrigin(aOrigin)
    , mPrincipalInfo(aPrincipalInfo)
    , mRv(aRv)
  {
    MOZ_ASSERT(mWorkerRef);
  }

  bool MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsIPrincipal* principal = GetPrincipalFromThreadSafeWorkerRef(mWorkerRef);
    if (!principal) {
      mRv.Throw(NS_ERROR_FAILURE);
      return true;
    }

    mRv = PrincipalToPrincipalInfo(principal, &mPrincipalInfo);
    if (NS_WARN_IF(mRv.Failed())) {
      return true;
    }

    mRv = principal->GetOrigin(mOrigin);
    if (NS_WARN_IF(mRv.Failed())) {
      return true;
    }

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerRef->Private();
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    // Window doesn't exist for some kind of workers (eg: SharedWorkers)
    nsPIDOMWindowInner* window = wp->GetWindow();
    if (!window) {
      return true;
    }

    return true;
  }

private:
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  nsACString& mOrigin;
  PrincipalInfo& mPrincipalInfo;
  ErrorResult& mRv;
};

class CloseRunnable final : public nsIRunnable,
                            public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS

  explicit CloseRunnable(BroadcastChannel* aBC)
    : mBC(aBC)
  {
    MOZ_ASSERT(mBC);
  }

  NS_IMETHOD Run() override
  {
    mBC->Shutdown();
    return NS_OK;
  }

  nsresult Cancel() override
  {
    return NS_OK;
  }

private:
  ~CloseRunnable() {}

  RefPtr<BroadcastChannel> mBC;
};

NS_IMPL_ISUPPORTS(CloseRunnable, nsICancelableRunnable, nsIRunnable)

class TeardownRunnable
{
protected:
  explicit TeardownRunnable(BroadcastChannelChild* aActor)
    : mActor(aActor)
  {
    MOZ_ASSERT(mActor);
  }

  void RunInternal()
  {
    MOZ_ASSERT(mActor);
    if (!mActor->IsActorDestroyed()) {
      mActor->SendClose();
    }
  }

protected:
  virtual ~TeardownRunnable() = default;

private:
  RefPtr<BroadcastChannelChild> mActor;
};

class TeardownRunnableOnMainThread final : public Runnable
                                         , public TeardownRunnable
{
public:
  explicit TeardownRunnableOnMainThread(BroadcastChannelChild* aActor)
    : Runnable("TeardownRunnableOnMainThread")
    , TeardownRunnable(aActor)
  {
  }

  NS_IMETHOD Run() override
  {
    RunInternal();
    return NS_OK;
  }
};

class TeardownRunnableOnWorker final : public WorkerControlRunnable
                                     , public TeardownRunnable
{
public:
  TeardownRunnableOnWorker(WorkerPrivate* aWorkerPrivate,
                           BroadcastChannelChild* aActor)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , TeardownRunnable(aActor)
  {
  }

  bool WorkerRun(JSContext*, WorkerPrivate*) override
  {
    RunInternal();
    return true;
  }

  bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {}

  bool
  PreRun(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
          bool aRunResult) override
  {}
};


} // namespace

BroadcastChannel::BroadcastChannel(nsPIDOMWindowInner* aWindow,
                                   const nsAString& aChannel)
  : DOMEventTargetHelper(aWindow)
  , mChannel(aChannel)
  , mState(StateActive)
{
  // Window can be null in workers

  KeepAliveIfHasListenersFor(NS_LITERAL_STRING("message"));
}

BroadcastChannel::~BroadcastChannel()
{
  Shutdown();
  MOZ_ASSERT(!mWorkerRef);
}

JSObject*
BroadcastChannel::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return BroadcastChannelBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<BroadcastChannel>
BroadcastChannel::Constructor(const GlobalObject& aGlobal,
                              const nsAString& aChannel,
                              ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window =
    do_QueryInterface(aGlobal.GetAsSupports());
  // Window is null in workers.

  RefPtr<BroadcastChannel> bc = new BroadcastChannel(window, aChannel);

  nsAutoCString origin;
  PrincipalInfo principalInfo;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIGlobalObject> incumbent = mozilla::dom::GetIncumbentGlobal();

    if (!incumbent) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsIPrincipal* principal = incumbent->PrincipalOrNull();
    if (!principal) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    aRv = principal->GetOrigin(origin);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = PrincipalToPrincipalInfo(principal, &principalInfo);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    JSContext* cx = aGlobal.Context();

    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
    MOZ_ASSERT(workerPrivate);

    RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(workerPrivate, "BroadcastChannel",
                              [bc] () { bc->Shutdown(); });
    // We are already shutting down the worker. Let's return a non-active
    // object.
    if (NS_WARN_IF(!workerRef)) {
      bc->mState = StateClosed;
      return bc.forget();
    }

    RefPtr<ThreadSafeWorkerRef> tsr = new ThreadSafeWorkerRef(workerRef);

    RefPtr<InitializeRunnable> runnable =
      new InitializeRunnable(tsr, origin, principalInfo, aRv);
    runnable->Dispatch(Closing, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    bc->mWorkerRef = Move(workerRef);
  }

  // Register this component to PBackground.
  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    // Firefox is probably shutting down. Let's return a 'generic' error.
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  PBroadcastChannelChild* actor =
    actorChild->SendPBroadcastChannelConstructor(principalInfo, origin,
                                                 nsString(aChannel));

  bc->mActor = static_cast<BroadcastChannelChild*>(actor);
  MOZ_ASSERT(bc->mActor);

  bc->mActor->SetParent(bc);

  return bc.forget();
}

void
BroadcastChannel::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                              ErrorResult& aRv)
{
  if (mState != StateActive) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  RefPtr<BroadcastChannelMessage> data = new BroadcastChannelMessage();

  data->Write(aCx, aMessage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RemoveDocFromBFCache();

  ClonedMessageData message;
  data->BuildClonedMessageDataForBackgroundChild(mActor->Manager(), message);
  mActor->SendPostMessage(message);
}

void
BroadcastChannel::Close()
{
  if (mState != StateActive) {
    return;
  }

  // We cannot call Shutdown() immediatelly because we could have some
  // postMessage runnable already dispatched. Instead, we change the state to
  // StateClosed and we shutdown the actor asynchrounsly.

  mState = StateClosed;
  RefPtr<CloseRunnable> runnable = new CloseRunnable(this);

  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    NS_WARNING("Failed to dispatch to the current thread!");
  }
}

void
BroadcastChannel::Shutdown()
{
  mState = StateClosed;

  // The DTOR of this WorkerRef will release the worker for us.
  mWorkerRef = nullptr;

  if (mActor) {
    mActor->SetParent(nullptr);

    if (NS_IsMainThread()) {
      RefPtr<TeardownRunnableOnMainThread> runnable =
        new TeardownRunnableOnMainThread(mActor);
      NS_DispatchToCurrentThread(runnable);
    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      RefPtr<TeardownRunnableOnWorker> runnable =
        new TeardownRunnableOnWorker(workerPrivate, mActor);
      runnable->Dispatch();
    }

    mActor = nullptr;
  }

  IgnoreKeepAliveIfHasListenersFor(NS_LITERAL_STRING("message"));
}

void
BroadcastChannel::RemoveDocFromBFCache()
{
  if (!NS_IsMainThread()) {
    return;
  }

  nsPIDOMWindowInner* window = GetOwner();
  if (!window) {
    return;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    return;
  }

  nsCOMPtr<nsIBFCacheEntry> bfCacheEntry = doc->GetBFCacheEntry();
  if (!bfCacheEntry) {
    return;
  }

  bfCacheEntry->RemoveFromBFCacheSync();
}

void
BroadcastChannel::DisconnectFromOwner()
{
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(BroadcastChannel)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BroadcastChannel,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BroadcastChannel,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BroadcastChannel)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BroadcastChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BroadcastChannel, DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
