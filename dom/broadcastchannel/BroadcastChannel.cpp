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
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#include "nsIBFCacheEntry.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace workers;

class BroadcastChannelMessage final : public StructuredCloneHolder
{
public:
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelMessage)

  BroadcastChannelMessage()
    : StructuredCloneHolder(CloningSupported, TransferringNotSupported,
                            DifferentProcess)
  {}

private:
  ~BroadcastChannelMessage()
  {}
};

namespace {

nsIPrincipal*
GetPrincipalFromWorkerPrivate(WorkerPrivate* aWorkerPrivate)
{
  nsIPrincipal* principal = aWorkerPrivate->GetPrincipal();
  if (principal) {
    return principal;
  }

  // Walk up to our containing page
  WorkerPrivate* wp = aWorkerPrivate;
  while (wp->GetParent()) {
    wp = wp->GetParent();
  }

  return wp->GetPrincipal();
}

class InitializeRunnable final : public WorkerMainThreadRunnable
{
public:
  InitializeRunnable(WorkerPrivate* aWorkerPrivate, nsACString& aOrigin,
                     PrincipalInfo& aPrincipalInfo, ErrorResult& aRv)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("BroadcastChannel :: Initialize"))
    , mWorkerPrivate(GetCurrentThreadWorkerPrivate())
    , mOrigin(aOrigin)
    , mPrincipalInfo(aPrincipalInfo)
    , mRv(aRv)
  {
    MOZ_ASSERT(mWorkerPrivate);
  }

  bool MainThreadRun() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsIPrincipal* principal = GetPrincipalFromWorkerPrivate(mWorkerPrivate);
    if (!principal) {
      mRv.Throw(NS_ERROR_FAILURE);
      return true;
    }

    if (NS_WARN_IF(principal->GetIsNullPrincipal())) {
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
    WorkerPrivate* wp = mWorkerPrivate;
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
  WorkerPrivate* mWorkerPrivate;
  nsACString& mOrigin;
  PrincipalInfo& mPrincipalInfo;
  ErrorResult& mRv;
};

class BCPostMessageRunnable final : public nsIRunnable,
                                    public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS

  BCPostMessageRunnable(BroadcastChannelChild* aActor,
                        BroadcastChannelMessage* aData)
    : mActor(aActor)
    , mData(aData)
  {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mActor);
    if (mActor->IsActorDestroyed()) {
      return NS_OK;
    }

    ClonedMessageData message;

    SerializedStructuredCloneBuffer& buffer = message.data();
    buffer.data = mData->BufferData();
    buffer.dataLength = mData->BufferSize();

    PBackgroundChild* backgroundManager = mActor->Manager();
    MOZ_ASSERT(backgroundManager);

    const nsTArray<RefPtr<BlobImpl>>& blobImpls = mData->BlobImpls();

    if (!blobImpls.IsEmpty()) {
      message.blobsChild().SetCapacity(blobImpls.Length());

      for (uint32_t i = 0, len = blobImpls.Length(); i < len; ++i) {
        PBlobChild* blobChild =
          BackgroundChild::GetOrCreateActorForBlobImpl(backgroundManager,
                                                       blobImpls[i]);
        MOZ_ASSERT(blobChild);

        message.blobsChild().AppendElement(blobChild);
      }
    }

    mActor->SendPostMessage(message);
    return NS_OK;
  }

  nsresult Cancel() override
  {
    mActor = nullptr;
    return NS_OK;
  }

private:
  ~BCPostMessageRunnable() {}

  RefPtr<BroadcastChannelChild> mActor;
  RefPtr<BroadcastChannelMessage> mData;
};

NS_IMPL_ISUPPORTS(BCPostMessageRunnable, nsICancelableRunnable, nsIRunnable)

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
    mBC = nullptr;
    return NS_OK;
  }

private:
  ~CloseRunnable() {}

  RefPtr<BroadcastChannel> mBC;
};

NS_IMPL_ISUPPORTS(CloseRunnable, nsICancelableRunnable, nsIRunnable)

class TeardownRunnable final : public nsIRunnable,
                               public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS

  explicit TeardownRunnable(BroadcastChannelChild* aActor)
    : mActor(aActor)
  {
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mActor);
    if (!mActor->IsActorDestroyed()) {
      mActor->SendClose();
    }
    return NS_OK;
  }

  nsresult Cancel() override
  {
    mActor = nullptr;
    return NS_OK;
  }

private:
  ~TeardownRunnable() {}

  RefPtr<BroadcastChannelChild> mActor;
};

NS_IMPL_ISUPPORTS(TeardownRunnable, nsICancelableRunnable, nsIRunnable)

class BroadcastChannelWorkerHolder final : public workers::WorkerHolder
{
  BroadcastChannel* mChannel;

public:
  explicit BroadcastChannelWorkerHolder(BroadcastChannel* aChannel)
    : mChannel(aChannel)
  {
    MOZ_COUNT_CTOR(BroadcastChannelWorkerHolder);
  }

  virtual bool Notify(workers::Status aStatus) override
  {
    if (aStatus >= Closing) {
      mChannel->Shutdown();
    }

    return true;
  }

private:
  ~BroadcastChannelWorkerHolder()
  {
    MOZ_COUNT_DTOR(BroadcastChannelWorkerHolder);
  }
};

} // namespace

BroadcastChannel::BroadcastChannel(nsPIDOMWindowInner* aWindow,
                                   const PrincipalInfo& aPrincipalInfo,
                                   const nsACString& aOrigin,
                                   const nsAString& aChannel)
  : DOMEventTargetHelper(aWindow)
  , mWorkerHolder(nullptr)
  , mPrincipalInfo(new PrincipalInfo(aPrincipalInfo))
  , mOrigin(aOrigin)
  , mChannel(aChannel)
  , mIsKeptAlive(false)
  , mInnerID(0)
  , mState(StateActive)
{
  // Window can be null in workers
}

BroadcastChannel::~BroadcastChannel()
{
  Shutdown();
  MOZ_ASSERT(!mWorkerHolder);
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

  nsAutoCString origin;
  PrincipalInfo principalInfo;
  WorkerPrivate* workerPrivate = nullptr;

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

    if (NS_WARN_IF(principal->GetIsNullPrincipal())) {
      aRv.Throw(NS_ERROR_FAILURE);
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
    workerPrivate = GetWorkerPrivateFromContext(cx);
    MOZ_ASSERT(workerPrivate);

    RefPtr<InitializeRunnable> runnable =
      new InitializeRunnable(workerPrivate, origin, principalInfo, aRv);
    runnable->Dispatch(aRv);
  }

  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<BroadcastChannel> bc =
    new BroadcastChannel(window, principalInfo, origin, aChannel);

  // Register this component to PBackground.
  PBackgroundChild* actor = BackgroundChild::GetForCurrentThread();
  if (actor) {
    bc->ActorCreated(actor);
  } else {
    BackgroundChild::GetOrCreateForCurrentThread(bc);
  }

  if (!workerPrivate) {
    MOZ_ASSERT(window);
    MOZ_ASSERT(window->IsInnerWindow());
    bc->mInnerID = window->WindowID();

    // Register as observer for inner-window-destroyed.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(bc, "inner-window-destroyed", false);
    }
  } else {
    bc->mWorkerHolder = new BroadcastChannelWorkerHolder(bc);
    if (NS_WARN_IF(!bc->mWorkerHolder->HoldWorker(workerPrivate))) {
      bc->mWorkerHolder = nullptr;
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

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

  PostMessageInternal(aCx, aMessage, aRv);
}

void
BroadcastChannel::PostMessageInternal(JSContext* aCx,
                                      JS::Handle<JS::Value> aMessage,
                                      ErrorResult& aRv)
{
  RefPtr<BroadcastChannelMessage> data = new BroadcastChannelMessage();

  data->Write(aCx, aMessage, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  PostMessageData(data);
}

void
BroadcastChannel::PostMessageData(BroadcastChannelMessage* aData)
{
  RemoveDocFromBFCache();

  if (mActor) {
    RefPtr<BCPostMessageRunnable> runnable =
      new BCPostMessageRunnable(mActor, aData);

    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to dispatch to the current thread!");
    }

    return;
  }

  mPendingMessages.AppendElement(aData);
}

void
BroadcastChannel::Close()
{
  if (mState != StateActive) {
    return;
  }

  if (mPendingMessages.IsEmpty()) {
    // We cannot call Shutdown() immediatelly because we could have some
    // postMessage runnable already dispatched. Instead, we change the state to
    // StateClosed and we shutdown the actor asynchrounsly.

    mState = StateClosed;
    RefPtr<CloseRunnable> runnable = new CloseRunnable(this);

    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to dispatch to the current thread!");
    }
  } else {
    MOZ_ASSERT(!mActor);
    mState = StateClosing;
  }
}

void
BroadcastChannel::ActorFailed()
{
  MOZ_CRASH("Failed to create a PBackgroundChild actor!");
}

void
BroadcastChannel::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);

  if (mState == StateClosed) {
    return;
  }

  PBroadcastChannelChild* actor =
    aActor->SendPBroadcastChannelConstructor(*mPrincipalInfo, mOrigin, mChannel);

  mActor = static_cast<BroadcastChannelChild*>(actor);
  MOZ_ASSERT(mActor);

  mActor->SetParent(this);

  // Flush pending messages.
  for (uint32_t i = 0; i < mPendingMessages.Length(); ++i) {
    PostMessageData(mPendingMessages[i]);
  }

  mPendingMessages.Clear();

  if (mState == StateClosing) {
    Shutdown();
  }
}

void
BroadcastChannel::Shutdown()
{
  mState = StateClosed;

  // The DTOR of this WorkerHolder will release the worker for us.
  mWorkerHolder = nullptr;

  if (mActor) {
    mActor->SetParent(nullptr);

    RefPtr<TeardownRunnable> runnable = new TeardownRunnable(mActor);
    NS_DispatchToCurrentThread(runnable);

    mActor = nullptr;
  }

  // If shutdown() is called we have to release the reference if we still keep
  // it.
  if (mIsKeptAlive) {
    mIsKeptAlive = false;
    Release();
  }
}

EventHandlerNonNull*
BroadcastChannel::GetOnmessage()
{
  if (NS_IsMainThread()) {
    return GetEventHandler(nsGkAtoms::onmessage, EmptyString());
  }
  return GetEventHandler(nullptr, NS_LITERAL_STRING("message"));
}

void
BroadcastChannel::SetOnmessage(EventHandlerNonNull* aCallback)
{
  if (NS_IsMainThread()) {
    SetEventHandler(nsGkAtoms::onmessage, EmptyString(), aCallback);
  } else {
    SetEventHandler(nullptr, NS_LITERAL_STRING("message"), aCallback);
  }

  UpdateMustKeepAlive();
}

void
BroadcastChannel::AddEventListener(const nsAString& aType,
                                   EventListener* aCallback,
                                   const AddEventListenerOptionsOrBoolean& aOptions,
                                   const dom::Nullable<bool>& aWantsUntrusted,
                                   ErrorResult& aRv)
{
  DOMEventTargetHelper::AddEventListener(aType, aCallback, aOptions,
                                         aWantsUntrusted, aRv);

  if (aRv.Failed()) {
    return;
  }

  UpdateMustKeepAlive();
}

void
BroadcastChannel::RemoveEventListener(const nsAString& aType,
                                      EventListener* aCallback,
                                      const EventListenerOptionsOrBoolean& aOptions,
                                      ErrorResult& aRv)
{
  DOMEventTargetHelper::RemoveEventListener(aType, aCallback, aOptions, aRv);

  if (aRv.Failed()) {
    return;
  }

  UpdateMustKeepAlive();
}

void
BroadcastChannel::UpdateMustKeepAlive()
{
  bool toKeepAlive = HasListenersFor(NS_LITERAL_STRING("message"));
  if (toKeepAlive == mIsKeptAlive) {
    return;
  }

  mIsKeptAlive = toKeepAlive;

  if (toKeepAlive) {
    AddRef();
  } else {
    Release();
  }
}

NS_IMETHODIMP
BroadcastChannel::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aTopic, "inner-window-destroyed"));

  // If the window is destroyed we have to release the reference that we are
  // keeping.
  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (innerID == mInnerID) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
    }

    Shutdown();
  }

  return NS_OK;
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

NS_IMPL_CYCLE_COLLECTION_CLASS(BroadcastChannel)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BroadcastChannel,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BroadcastChannel,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BroadcastChannel)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BroadcastChannel, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BroadcastChannel, DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
