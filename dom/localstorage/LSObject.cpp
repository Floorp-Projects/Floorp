/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSObject.h"

#include "ActorsChild.h"
#include "IPCBlobInputStreamThread.h"
#include "LocalStorageCommon.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsThread.h"

namespace mozilla {
namespace dom {

namespace {

class RequestHelper;

StaticMutex gRequestHelperMutex;
RequestHelper* gRequestHelper = nullptr;

class RequestHelper final
  : public Runnable
  , public LSRequestChildCallback
{
  enum class State
  {
    Initial,
    ResponsePending,
    Finishing,
    Complete
  };

  RefPtr<LSObject> mObject;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;
  LSRequestChild* mActor;
  const LSRequestParams mParams;
  LSRequestResponse mResponse;
  nsresult mResultCode;
  State mState;
  bool mWaiting;

public:
  RequestHelper(LSObject* aObject,
                const LSRequestParams& aParams)
    : Runnable("dom::RequestHelper")
    , mObject(aObject)
    , mOwningEventTarget(GetCurrentThreadEventTarget())
    , mActor(nullptr)
    , mParams(aParams)
    , mResultCode(NS_OK)
    , mState(State::Initial)
    , mWaiting(true)
  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    gRequestHelper = this;
  }

  bool
  IsOnOwningThread() const
  {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return
      NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) && current;
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  // Used for requests from the parent process to the parent process; in that
  // case we want ActorsParent to know our event-target and this is better than
  // trying to tunnel the pointer through IPC.
  const nsCOMPtr<nsIEventTarget>&
  GetSyncLoopEventTarget() const
  {
    MOZ_ASSERT(XRE_IsParentProcess());

    return mNestedEventTarget;
  }

  nsresult
  StartAndReturnResponse(LSRequestResponse& aResponse);

  nsresult
  CancelOnAnyThread();

private:
  ~RequestHelper()
  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    gRequestHelper = nullptr;
  }

  nsresult
  Start();

  void
  Finish();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // LSRequestChildCallback
  void
  OnResponse(const LSRequestResponse& aResponse) override;
};

} // namespace

LSObject::LSObject(nsPIDOMWindowInner* aWindow,
                   nsIPrincipal* aPrincipal)
  : Storage(aWindow, aPrincipal)
  , mActor(nullptr)
  , mPrivateBrowsingId(0)
  , mActorFailed(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NextGenLocalStorageEnabled());
}

LSObject::~LSObject()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mActorFailed, !mActor);

  DropObserver();

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
nsresult
LSObject::CreateForWindow(nsPIDOMWindowInner* aWindow,
                          Storage** aStorage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aStorage);
  MOZ_ASSERT(NextGenLocalStorageEnabled());
  MOZ_ASSERT(nsContentUtils::StorageAllowedForWindow(aWindow) >
               nsContentUtils::StorageAccess::eDeny);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_FAILURE;
  }

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  nsresult rv = PrincipalToPrincipalInfo(principal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo);

  nsCString origin;
  rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr, &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t privateBrowsingId;
  rv = principal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString documentURI;
  if (nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc()) {
    rv = doc->GetDocumentURI(documentURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  RefPtr<LSObject> object = new LSObject(aWindow, principal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mPrivateBrowsingId = privateBrowsingId;
  object->mOrigin = origin;
  object->mDocumentURI = documentURI;

  object.forget(aStorage);
  return NS_OK;
}

// static
nsresult
LSObject::CreateForPrincipal(nsPIDOMWindowInner* aWindow,
                             nsIPrincipal* aPrincipal,
                             const nsAString& aDocumentURI,
                             bool aPrivate,
                             LSObject** aObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aObject);

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
             principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  nsCString origin;

  if (principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &origin);
  } else {
    rv = QuotaManager::GetInfoFromPrincipal(aPrincipal,
                                            nullptr,
                                            nullptr,
                                            &origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  RefPtr<LSObject> object = new LSObject(aWindow, aPrincipal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mPrivateBrowsingId = aPrivate ? 1 : 0;
  object->mOrigin = origin;
  object->mDocumentURI = aDocumentURI;

  object.forget(aObject);
  return NS_OK;
}

// static
already_AddRefed<nsIEventTarget>
LSObject::GetSyncLoopEventTarget()
{
  RefPtr<RequestHelper> helper;

  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    helper = gRequestHelper;
  }

  nsCOMPtr<nsIEventTarget> target;
  if (helper) {
    target = helper->GetSyncLoopEventTarget();
  }

  return target.forget();
}

// static
void
LSObject::CancelSyncLoop()
{
  RefPtr<RequestHelper> helper;

  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    helper = gRequestHelper;
  }

  if (helper) {
    Unused << NS_WARN_IF(NS_FAILED(helper->CancelOnAnyThread()));
  }
}

LSRequestChild*
LSObject::StartRequest(nsIEventTarget* aMainEventTarget,
                       const LSRequestParams& aParams,
                       LSRequestChildCallback* aCallback)
{
  AssertIsOnDOMFileThread();

  PBackgroundChild* backgroundActor =
    BackgroundChild::GetOrCreateForCurrentThread(aMainEventTarget);
  if (NS_WARN_IF(!backgroundActor)) {
    return nullptr;
  }

  LSRequestChild* actor = new LSRequestChild(aCallback);

  backgroundActor->SendPBackgroundLSRequestConstructor(actor, aParams);

  return actor;
}

Storage::StorageType
LSObject::Type() const
{
  AssertIsOnOwningThread();

  return eLocalStorage;
}

bool
LSObject::IsForkOf(const Storage* aStorage) const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStorage);

  if (aStorage->Type() != eLocalStorage) {
    return false;
  }

  return static_cast<const LSObject*>(aStorage)->mOrigin == mOrigin;
}

int64_t
LSObject::GetOriginQuotaUsage() const
{
  AssertIsOnOwningThread();

  return 0;
}

uint32_t
LSObject::GetLength(nsIPrincipal& aSubjectPrincipal,
                    ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return 0;
  }

  uint32_t result;
  rv = mDatabase->GetLength(&result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return 0;
  }

  return result;
}

void
LSObject::Key(uint32_t aIndex,
              nsAString& aResult,
              nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  nsString result;
  rv = mDatabase->GetKey(aIndex, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void
LSObject::GetItem(const nsAString& aKey,
                  nsAString& aResult,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  nsString result;
  rv = mDatabase->GetItem(aKey, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void
LSObject::GetSupportedNames(nsTArray<nsString>& aNames)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // Return just an empty array.
    aNames.Clear();
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = mDatabase->GetKeys(aNames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void
LSObject::SetItem(const nsAString& aKey,
                  const nsAString& aValue,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSWriteOpResponse response;
  rv = mDatabase->SetItem(aKey, aValue, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = GetInfoFromResponse(response, info);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), aValue);
  }
}

void
LSObject::RemoveItem(const nsAString& aKey,
                     nsIPrincipal& aSubjectPrincipal,
                     ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSWriteOpResponse response;
  rv = mDatabase->RemoveItem(aKey, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = GetInfoFromResponse(response, info);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), VoidString());
  }
}

void
LSObject::Clear(nsIPrincipal& aSubjectPrincipal,
                ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSWriteOpResponse response;
  rv = mDatabase->Clear(response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = GetInfoFromResponse(response, info);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(VoidString(), VoidString(), VoidString());
  }
}

void
LSObject::Open(nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }
}

void
LSObject::Close(nsIPrincipal& aSubjectPrincipal,
                ErrorResult& aError)
{
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  DropDatabase();
}

NS_IMPL_ADDREF_INHERITED(LSObject, Storage)
NS_IMPL_RELEASE_INHERITED(LSObject, Storage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LSObject)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_CYCLE_COLLECTION_CLASS(LSObject)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(LSObject, Storage)
  tmp->AssertIsOnOwningThread();
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(LSObject, Storage)
  tmp->AssertIsOnOwningThread();
  tmp->DropDatabase();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult
LSObject::DoRequestSynchronously(const LSRequestParams& aParams,
                                 LSRequestResponse& aResponse)
{
  // We don't need this yet, but once the request successfully finishes, it's
  // too late to initialize PBackground child on the owning thread, because
  // it can fail and parent would keep an extra strong ref to the datastore or
  // observer.
  PBackgroundChild* backgroundActor =
    BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RequestHelper> helper = new RequestHelper(this, aParams);

  // This will start and finish the request on the DOM File thread.
  // The owning thread is synchronously blocked while the request is
  // asynchronously processed on the DOM File thread.
  nsresult rv = helper->StartAndReturnResponse(aResponse);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aResponse.type() == LSRequestResponse::Tnsresult) {
    return aResponse.get_nsresult();
  }

  return NS_OK;
}

nsresult
LSObject::EnsureDatabase()
{
  AssertIsOnOwningThread();

  if (mDatabase && !mDatabase->IsAllowedToClose()) {
    return NS_OK;
  }

  mDatabase = nullptr;

  if (mActorFailed) {
    return NS_ERROR_FAILURE;
  }

  if (!mActor) {
    PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      return NS_ERROR_FAILURE;
    }

    LSObjectChild* actor = new LSObjectChild(this);

    mActor =
      static_cast<LSObjectChild*>(
        backgroundActor->SendPBackgroundLSObjectConstructor(
                                                           actor,
                                                           *mPrincipalInfo,
                                                           mDocumentURI,
                                                           mPrivateBrowsingId));

    if (NS_WARN_IF(!mActor)) {
      mActorFailed = true;
      return NS_ERROR_FAILURE;
    }
  }

  LSRequestPrepareDatastoreParams params;
  params.principalInfo() = *mPrincipalInfo;

  LSRequestResponse response;

  nsresult rv = DoRequestSynchronously(params, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(response.type() ==
               LSRequestResponse::TLSRequestPrepareDatastoreResponse);

  const LSRequestPrepareDatastoreResponse& prepareDatastoreResponse =
    response.get_LSRequestPrepareDatastoreResponse();

  uint64_t datastoreId = prepareDatastoreResponse.datastoreId();

  // The datastore is now ready on the parent side (prepared by the asynchronous
  // request on the DOM File thread).
  // Let's create a direct connection to the datastore (through a database
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the datastore.

  RefPtr<LSDatabase> database = new LSDatabase();

  LSDatabaseChild* actor = new LSDatabaseChild(database);

  MOZ_ALWAYS_TRUE(
    mActor->SendPBackgroundLSDatabaseConstructor(actor, datastoreId));

  database->SetActor(actor);

  mDatabase = std::move(database);

  return NS_OK;
}

void
LSObject::DropDatabase()
{
  AssertIsOnOwningThread();

  if (mDatabase) {
    if (!mDatabase->IsAllowedToClose()) {
      mDatabase->AllowToClose();
    }
    mDatabase = nullptr;
  }
}

nsresult
LSObject::EnsureObserver()
{
  AssertIsOnOwningThread();

  if (mObserver) {
    return NS_OK;
  }

  mObserver = LSObserver::Get(mOrigin);

  if (mObserver) {
    return NS_OK;
  }

  LSRequestPrepareObserverParams params;
  params.principalInfo() = *mPrincipalInfo;

  LSRequestResponse response;

  nsresult rv = DoRequestSynchronously(params, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(response.type() ==
               LSRequestResponse::TLSRequestPrepareObserverResponse);

  const LSRequestPrepareObserverResponse& prepareObserverResponse =
    response.get_LSRequestPrepareObserverResponse();

  uint64_t observerId = prepareObserverResponse.observerId();

  // The obsserver is now ready on the parent side (prepared by the asynchronous
  // request on the DOM File thread).
  // Let's create a direct connection to the observer (through an observer
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the observer.

  PBackgroundChild* backgroundActor = BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(backgroundActor);

  RefPtr<LSObserver> observer = new LSObserver(mOrigin);

  LSObserverChild* actor = new LSObserverChild(observer);

  MOZ_ALWAYS_TRUE(
    backgroundActor->SendPBackgroundLSObserverConstructor(actor, observerId));

  observer->SetActor(actor);

  mObserver = std::move(observer);

  return NS_OK;
}

void
LSObject::DropObserver()
{
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver = nullptr;
  }
}

nsresult
LSObject::GetInfoFromResponse(const LSWriteOpResponse& aResponse,
                              LSNotifyInfo& aInfo)
{
  AssertIsOnOwningThread();

  if (aResponse.type() == LSWriteOpResponse::Tnsresult) {
    nsresult errorCode = aResponse.get_nsresult();

    if (errorCode == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      errorCode = NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    }

    return errorCode;
  }

  MOZ_ASSERT(aResponse.type() == LSWriteOpResponse::TLSNotifyInfo);

  aInfo = aResponse.get_LSNotifyInfo();
  return NS_OK;
}

void
LSObject::OnChange(const nsAString& aKey,
                   const nsAString& aOldValue,
                   const nsAString& aNewValue)
{
  AssertIsOnOwningThread();

  NotifyChange(/* aStorage */ this,
               Principal(),
               aKey,
               aOldValue,
               aNewValue,
               /* aStorageType */ kLocalStorageType,
               mDocumentURI,
               /* aIsPrivate */ !!mPrivateBrowsingId,
               /* aImmediateDispatch */ false);
}

void
LSObject::LastRelease()
{
  AssertIsOnOwningThread();

  DropDatabase();
}

nsresult
RequestHelper::StartAndReturnResponse(LSRequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  // Normally, we would use the standard way of blocking the thread using
  // a monitor.
  // The problem is that BackgroundChild::GetOrCreateForCurrentThread()
  // called on the DOM File thread may dispatch a runnable to the main
  // thread to finish initialization of PBackground. A monitor would block
  // the main thread and the runnable would never get executed causing the
  // helper to be stuck in a wait loop.
  // However, BackgroundChild::GetOrCreateForCurrentThread() supports passing
  // a custom main event target, so we can create a nested event target and
  // spin the event loop. Nothing can dispatch to the nested event target
  // except BackgroundChild::GetOrCreateForCurrentThread(), so spinning of the
  // event loop can't fire any other events.
  // This way the thread is synchronously blocked in a safe manner and the
  // runnable gets executed.
  {
    auto thread = static_cast<nsThread*>(NS_GetCurrentThread());

    auto queue =
      static_cast<ThreadEventQueue<EventQueue>*>(thread->EventQueue());

    mNestedEventTarget = queue->PushEventQueue();
    MOZ_ASSERT(mNestedEventTarget);

    auto autoPopEventQueue = mozilla::MakeScopeExit([&] {
      queue->PopEventQueue(mNestedEventTarget);
    });

    nsCOMPtr<nsIEventTarget> domFileThread =
      IPCBlobInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!domFileThread)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = domFileThread->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
      return !mWaiting;
    }));
  }

  if (NS_WARN_IF(NS_FAILED(mResultCode))) {
    return mResultCode;
  }

  aResponse = std::move(mResponse);
  return NS_OK;
}

nsresult
RequestHelper::CancelOnAnyThread()
{
  RefPtr<RequestHelper> self = this;

  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
    "RequestHelper::CancelOnAnyThread",
    [self] () {
      LSRequestChild* actor = self->mActor;
      if (actor && !actor->Finishing()) {
        actor->SendCancel();
      }
    });

  nsCOMPtr<nsIEventTarget> domFileThread =
    IPCBlobInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!domFileThread)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = domFileThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
RequestHelper::Start()
{
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::Initial);

  mState = State::ResponsePending;

  LSRequestChild* actor =
    mObject->StartRequest(mNestedEventTarget, mParams, this);
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_FAILURE;
  }

  mActor = actor;

  return NS_OK;
}

void
RequestHelper::Finish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Finishing);

  mObject = nullptr;

  mWaiting = false;

  mState = State::Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(RequestHelper, Runnable)

NS_IMETHODIMP
RequestHelper::Run()
{
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Start();
      break;

    case State::Finishing:
      Finish();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::Finishing) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::Finishing;

    if (IsOnOwningThread()) {
      Finish();
    } else {
      MOZ_ALWAYS_SUCCEEDS(mNestedEventTarget->Dispatch(this,
                                                       NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void
RequestHelper::OnResponse(const LSRequestResponse& aResponse)
{
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::ResponsePending);

  mActor = nullptr;

  mResponse = aResponse;

  mState = State::Finishing;
  MOZ_ALWAYS_SUCCEEDS(mNestedEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

} // namespace dom
} // namespace mozilla
