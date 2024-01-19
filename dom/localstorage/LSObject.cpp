/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSObject.h"

// Local includes
#include "ActorsChild.h"
#include "LSDatabase.h"
#include "LSObserver.h"

// Global includes
#include <utility>
#include "MainThreadUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Monitor.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RemoteLazyInputStreamThread.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/PBackgroundLSRequest.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/ProcessChild.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsISerialEventTarget.h"
#include "nsITimer.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsTStringRepr.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nscore.h"

/**
 * Automatically cancel and abort synchronous LocalStorage requests (for example
 * datastore preparation) if they take this long.  We've chosen a value that is
 * long enough that it is unlikely for the problem to be falsely triggered by
 * slow system I/O.  We've also chosen a value long enough so that automated
 * tests should time out and fail if LocalStorage hangs.  Also, this value is
 * long enough so that testers can notice the (content process) hang; we want to
 * know about the hangs, not hide them.  On the other hand this value is less
 * than 60 seconds which is used by nsTerminator to crash a hung main process.
 */
#define FAILSAFE_CANCEL_SYNC_OP_MS 50000

/**
 * Interval with which to wake up while waiting for the sync op to complete to
 * check ExpectingShutdown().
 */
#define SYNC_OP_WAKE_INTERVAL_MS 500

namespace mozilla::dom {

namespace {

class RequestHelper;

/**
 * Main-thread helper that implements the blocking logic required by
 * LocalStorage's synchronous semantics.  StartAndReturnResponse blocks on a
 * monitor until a result is received. See StartAndReturnResponse() for info on
 * this choice.
 *
 * The normal life-cycle of this method looks like:
 * - Main Thread: LSObject::DoRequestSynchronously creates a RequestHelper and
 *   invokes StartAndReturnResponse().  It Dispatches the RequestHelper to the
 *   RemoteLazyInputStream thread, and waits on mMonitor.
 * - RemoteLazyInputStream Thread: RequestHelper::Run is called, invoking
 *   Start() which invokes LSObject::StartRequest, which gets-or-creates the
 *   PBackground actor if necessary, sends LSRequest constructor which is
 *   provided with a callback reference to the RequestHelper. State advances to
 *   ResponsePending.
 * - RemoteLazyInputStreamThread: LSRequestChild::Recv__delete__ is received,
 *   which invokes RequestHelepr::OnResponse, advancing the state to Complete
 *   and notifying mMonitor.
 * - Main Thread: The main thread wakes up after waiting on the monitor,
 *   returning the received response.
 *
 * See LocalStorageCommon.h for high-level context and method comments for
 * low-level details.
 */
class RequestHelper final : public Runnable, public LSRequestChildCallback {
  enum class State {
    /**
     * The RequestHelper has been created and dispatched to the
     * RemoteLazyInputStream Thread.
     */
    Initial,
    /**
     * Start() has been invoked on the RemoteLazyInputStream Thread and
     * LSObject::StartRequest has been invoked from there, sending an IPC
     * message to PBackground to service the request.  We stay in this state
     * until a response is received or a timeout occurs.
     */
    ResponsePending,
    /**
     * The request timed out, or failed in some fashion, and needs to be
     * cancelled. A runnable has been dispatched to the DOM File thread to
     * notify the parent actor, and the main thread will continue to block until
     * we receive a reponse.
     */
    Canceling,
    /**
     * The request is complete, either successfully or due to being cancelled.
     * The main thread can stop waiting and immediately return to the caller of
     * StartAndReturnResponse.
     */
    Complete
  };

  // The object we are issuing a request on behalf of.  Present because of the
  // need to invoke LSObject::StartRequest off the main thread.  Dropped on
  // return to the main-thread in Finish().
  RefPtr<LSObject> mObject;
  // The thread the RequestHelper was created on.  This should be the main
  // thread.
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  // The IPC actor handling the request with standard IPC allocation rules.
  // Our reference is nulled in OnResponse which corresponds to the actor's
  // __destroy__ method.
  LSRequestChild* mActor;
  const LSRequestParams mParams;
  Monitor mMonitor;
  LSRequestResponse mResponse MOZ_GUARDED_BY(mMonitor);
  nsresult mResultCode MOZ_GUARDED_BY(mMonitor);
  State mState MOZ_GUARDED_BY(mMonitor);

 public:
  RequestHelper(LSObject* aObject, const LSRequestParams& aParams)
      : Runnable("dom::RequestHelper"),
        mObject(aObject),
        mOwningEventTarget(GetCurrentSerialEventTarget()),
        mActor(nullptr),
        mParams(aParams),
        mMonitor("dom::RequestHelper::mMonitor"),
        mResultCode(NS_OK),
        mState(State::Initial) {}

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult StartAndReturnResponse(LSRequestResponse& aResponse);

 private:
  ~RequestHelper() = default;

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // LSRequestChildCallback
  void OnResponse(const LSRequestResponse& aResponse) override;
};

void AssertExplicitSnapshotInvariants(const LSObject& aObject) {
  // Can be only called if the mInExplicitSnapshot flag is true.
  // An explicit snapshot must have been created.
  MOZ_ASSERT(aObject.InExplicitSnapshot());

  // If an explicit snapshot has been created then mDatabase must be not null.
  // DropDatabase could be called in the meatime, but that must be preceded by
  // Disconnect which sets mInExplicitSnapshot to false. EnsureDatabase could
  // be called in the meantime too, but that can't set mDatabase to null or to
  // a new value. See the comment below.
  MOZ_ASSERT(aObject.DatabaseStrongRef());

  // Existence of a snapshot prevents the database from allowing to close. See
  // LSDatabase::RequestAllowToClose and LSDatabase::NoteFinishedSnapshot.
  // If the database is not allowed to close then mDatabase could not have been
  // nulled out or set to a new value. See EnsureDatabase.
  MOZ_ASSERT(!aObject.DatabaseStrongRef()->IsAllowedToClose());
}

}  // namespace

LSObject::LSObject(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                   nsIPrincipal* aStoragePrincipal)
    : Storage(aWindow, aPrincipal, aStoragePrincipal),
      mPrivateBrowsingId(0),
      mInExplicitSnapshot(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NextGenLocalStorageEnabled());
}

LSObject::~LSObject() {
  AssertIsOnOwningThread();

  DropObserver();
}

// static
nsresult LSObject::CreateForWindow(nsPIDOMWindowInner* aWindow,
                                   Storage** aStorage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aStorage);
  MOZ_ASSERT(NextGenLocalStorageEnabled());
  MOZ_ASSERT(StorageAllowedForWindow(aWindow) != StorageAccess::eDeny);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> storagePrincipal = sop->GetEffectiveStoragePrincipal();
  if (NS_WARN_IF(!storagePrincipal)) {
    return NS_ERROR_FAILURE;
  }

  if (principal->IsSystemPrincipal()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // localStorage is not available on some pages on purpose, for example
  // about:home. Match the old implementation by using GenerateOriginKey
  // for the check.
  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = storagePrincipal->GetStorageOriginKey(originKey);
  storagePrincipal->OriginAttributesRef().CreateSuffix(originAttrSuffix);

  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto principalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(principal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo);

  auto storagePrincipalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(storagePrincipal, storagePrincipalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(storagePrincipalInfo->type() ==
             PrincipalInfo::TContentPrincipalInfo);

  if (NS_WARN_IF(
          !quota::QuotaManager::IsPrincipalInfoValid(*storagePrincipalInfo))) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  QM_TRY_INSPECT(
      const auto& principalMetadata,
      quota::QuotaManager::GetInfoFromPrincipal(storagePrincipal.get()));

  MOZ_ASSERT(originAttrSuffix == principalMetadata.mSuffix);

  const auto& origin = principalMetadata.mOrigin;
#else
  QM_TRY_INSPECT(
      const auto& origin,
      quota::QuotaManager::GetOriginFromPrincipal(storagePrincipal.get()));
#endif

  uint32_t privateBrowsingId;
  rv = storagePrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Maybe<ClientInfo> clientInfo = aWindow->GetClientInfo();
  if (clientInfo.isNothing()) {
    return NS_ERROR_FAILURE;
  }

  Maybe<nsID> clientId = Some(clientInfo.ref().Id());

  Maybe<PrincipalInfo> clientPrincipalInfo =
      Some(clientInfo.ref().PrincipalInfo());

  nsString documentURI;
  if (nsCOMPtr<Document> doc = aWindow->GetExtantDoc()) {
    rv = doc->GetDocumentURI(documentURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  RefPtr<LSObject> object = new LSObject(aWindow, principal, storagePrincipal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mStoragePrincipalInfo = std::move(storagePrincipalInfo);
  object->mPrivateBrowsingId = privateBrowsingId;
  object->mClientId = clientId;
  object->mClientPrincipalInfo = clientPrincipalInfo;
  object->mOrigin = origin;
  object->mOriginKey = originKey;
  object->mDocumentURI = documentURI;

  object.forget(aStorage);
  return NS_OK;
}

// static
nsresult LSObject::CreateForPrincipal(nsPIDOMWindowInner* aWindow,
                                      nsIPrincipal* aPrincipal,
                                      nsIPrincipal* aStoragePrincipal,
                                      const nsAString& aDocumentURI,
                                      bool aPrivate, LSObject** aObject) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStoragePrincipal);
  MOZ_ASSERT(aObject);

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = aStoragePrincipal->GetStorageOriginKey(originKey);
  aStoragePrincipal->OriginAttributesRef().CreateSuffix(originAttrSuffix);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  auto principalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(aPrincipal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
             principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  auto storagePrincipalInfo = MakeUnique<PrincipalInfo>();
  rv = PrincipalToPrincipalInfo(aStoragePrincipal, storagePrincipalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(
      storagePrincipalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
      storagePrincipalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  if (NS_WARN_IF(
          !quota::QuotaManager::IsPrincipalInfoValid(*storagePrincipalInfo))) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  QM_TRY_INSPECT(
      const auto& principalMetadata,
      ([&storagePrincipalInfo,
        &aPrincipal]() -> Result<quota::PrincipalMetadata, nsresult> {
        if (storagePrincipalInfo->type() ==
            PrincipalInfo::TSystemPrincipalInfo) {
          return quota::QuotaManager::GetInfoForChrome();
        }

        QM_TRY_RETURN(quota::QuotaManager::GetInfoFromPrincipal(aPrincipal));
      }()));

  MOZ_ASSERT(originAttrSuffix == principalMetadata.mSuffix);

  const auto& origin = principalMetadata.mOrigin;
#else
  QM_TRY_INSPECT(
      const auto& origin, ([&storagePrincipalInfo,
                            &aPrincipal]() -> Result<nsAutoCString, nsresult> {
        if (storagePrincipalInfo->type() ==
            PrincipalInfo::TSystemPrincipalInfo) {
          return nsAutoCString{quota::QuotaManager::GetOriginForChrome()};
        }

        QM_TRY_RETURN(quota::QuotaManager::GetOriginFromPrincipal(aPrincipal));
      }()));
#endif

  Maybe<nsID> clientId;
  if (aWindow) {
    Maybe<ClientInfo> clientInfo = aWindow->GetClientInfo();
    if (clientInfo.isNothing()) {
      return NS_ERROR_FAILURE;
    }

    clientId = Some(clientInfo.ref().Id());
  } else if (Preferences::GetBool("dom.storage.client_validation")) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<LSObject> object =
      new LSObject(aWindow, aPrincipal, aStoragePrincipal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mStoragePrincipalInfo = std::move(storagePrincipalInfo);
  object->mPrivateBrowsingId = aPrivate ? 1 : 0;
  object->mClientId = clientId;
  object->mOrigin = origin;
  object->mOriginKey = originKey;
  object->mDocumentURI = aDocumentURI;

  object.forget(aObject);
  return NS_OK;
}  // namespace dom

LSRequestChild* LSObject::StartRequest(const LSRequestParams& aParams,
                                       LSRequestChildCallback* aCallback) {
  AssertIsOnDOMFileThread();

  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return nullptr;
  }

  LSRequestChild* actor = new LSRequestChild();

  if (!backgroundActor->SendPBackgroundLSRequestConstructor(actor, aParams)) {
    return nullptr;
  }

  // Must set callback after calling SendPBackgroundLSRequestConstructor since
  // it can be called synchronously when SendPBackgroundLSRequestConstructor
  // fails.
  actor->SetCallback(aCallback);

  return actor;
}

Storage::StorageType LSObject::Type() const {
  AssertIsOnOwningThread();

  return eLocalStorage;
}

bool LSObject::IsForkOf(const Storage* aStorage) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStorage);

  if (aStorage->Type() != eLocalStorage) {
    return false;
  }

  return static_cast<const LSObject*>(aStorage)->mOrigin == mOrigin;
}

int64_t LSObject::GetOriginQuotaUsage() const {
  AssertIsOnOwningThread();

  // It's not necessary to return an actual value here.  This method is
  // implemented only because the SessionStore currently needs it to cap the
  // amount of data it persists to disk (via nsIDOMWindowUtils.getStorageUsage).
  // Any callers that want to know about storage usage should be asking
  // QuotaManager directly.
  //
  // Note: This may change as LocalStorage is repurposed to be the new
  // SessionStorage backend.
  return 0;
}

void LSObject::Disconnect() {
  // Explicit snapshots which were not ended in JS, must be ended here while
  // IPC is still available. We can't do that in DropDatabase because actors
  // may have been destroyed already at that point.
  if (mInExplicitSnapshot) {
    AssertExplicitSnapshotInvariants(*this);

    nsresult rv = mDatabase->EndExplicitSnapshot();
    Unused << NS_WARN_IF(NS_FAILED(rv));

    mInExplicitSnapshot = false;
  }
}

uint32_t LSObject::GetLength(nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aError) {
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
  rv = mDatabase->GetLength(this, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return 0;
  }

  return result;
}

void LSObject::Key(uint32_t aIndex, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
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
  rv = mDatabase->GetKey(this, aIndex, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void LSObject::GetItem(const nsAString& aKey, nsAString& aResult,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
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
  rv = mDatabase->GetItem(this, aKey, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void LSObject::GetSupportedNames(nsTArray<nsString>& aNames) {
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

  rv = mDatabase->GetKeys(this, aNames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void LSObject::SetItem(const nsAString& aKey, const nsAString& aValue,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
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

  LSNotifyInfo info;
  rv = mDatabase->SetItem(this, aKey, aValue, info);
  if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
    rv = NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), aValue);
  }
}

void LSObject::RemoveItem(const nsAString& aKey,
                          nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aError) {
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

  LSNotifyInfo info;
  rv = mDatabase->RemoveItem(this, aKey, info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), VoidString());
  }
}

void LSObject::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
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

  LSNotifyInfo info;
  rv = mDatabase->Clear(this, info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(VoidString(), VoidString(), VoidString());
  }
}

void LSObject::Open(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
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

void LSObject::Close(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  DropDatabase();
}

void LSObject::BeginExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (mInExplicitSnapshot) {
    aError.Throw(NS_ERROR_ALREADY_INITIALIZED);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  rv = mDatabase->BeginExplicitSnapshot(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  mInExplicitSnapshot = true;
}

void LSObject::CheckpointExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                          ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!mInExplicitSnapshot) {
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  AssertExplicitSnapshotInvariants(*this);

  nsresult rv = mDatabase->CheckpointExplicitSnapshot();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }
}

void LSObject::EndExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!mInExplicitSnapshot) {
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  AssertExplicitSnapshotInvariants(*this);

  nsresult rv = mDatabase->EndExplicitSnapshot();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  mInExplicitSnapshot = false;
}

bool LSObject::GetHasSnapshot(nsIPrincipal& aSubjectPrincipal,
                              ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return false;
  }

  // We can't call `HasSnapshot` on the database if it's being closed, but we
  // know that a database which is being closed can't have a snapshot, so we
  // return false in that case directly here.
  if (!mDatabase || mDatabase->IsAllowedToClose()) {
    return false;
  }

  return mDatabase->HasSnapshot();
}

int64_t LSObject::GetSnapshotUsage(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  if (!mDatabase || mDatabase->IsAllowedToClose()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return 0;
  }

  if (!mDatabase->HasSnapshot()) {
    aError.Throw(NS_ERROR_NOT_AVAILABLE);
    return 0;
  }

  return mDatabase->GetSnapshotUsage();
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

nsresult LSObject::DoRequestSynchronously(const LSRequestParams& aParams,
                                          LSRequestResponse& aResponse) {
  // We don't need this yet, but once the request successfully finishes, it's
  // too late to initialize PBackground child on the owning thread, because
  // it can fail and parent would keep an extra strong ref to the datastore or
  // observer.
  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RequestHelper> helper = new RequestHelper(this, aParams);

  // This will start and finish the request on the RemoteLazyInputStream thread.
  // The owning thread is synchronously blocked while the request is
  // asynchronously processed on the RemoteLazyInputStream thread.
  nsresult rv = helper->StartAndReturnResponse(aResponse);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aResponse.type() == LSRequestResponse::Tnsresult) {
    nsresult errorCode = aResponse.get_nsresult();

    if (errorCode == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      errorCode = NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    }

    return errorCode;
  }

  return NS_OK;
}

nsresult LSObject::EnsureDatabase() {
  AssertIsOnOwningThread();

  if (mDatabase && !mDatabase->IsAllowedToClose()) {
    return NS_OK;
  }

  mDatabase = LSDatabase::Get(mOrigin);

  if (mDatabase) {
    MOZ_ASSERT(!mDatabase->IsAllowedToClose());
    return NS_OK;
  }

  // We don't need this yet, but once the request successfully finishes, it's
  // too late to initialize PBackground child on the owning thread, because
  // it can fail and parent would keep an extra strong ref to the datastore.
  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  LSRequestCommonParams commonParams;
  commonParams.principalInfo() = *mPrincipalInfo;
  commonParams.storagePrincipalInfo() = *mStoragePrincipalInfo;
  commonParams.originKey() = mOriginKey;

  LSRequestPrepareDatastoreParams params;
  params.commonParams() = commonParams;
  params.clientId() = mClientId;
  params.clientPrincipalInfo() = mClientPrincipalInfo;

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
  // request on the RemoteLazyInputStream thread).
  // Let's create a direct connection to the datastore (through a database
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the datastore.

  RefPtr<LSDatabase> database = new LSDatabase(mOrigin);

  RefPtr<LSDatabaseChild> actor = new LSDatabaseChild(database);

  MOZ_ALWAYS_TRUE(backgroundActor->SendPBackgroundLSDatabaseConstructor(
      actor, *mStoragePrincipalInfo, mPrivateBrowsingId, datastoreId));

  database->SetActor(actor);

  mDatabase = std::move(database);

  return NS_OK;
}

void LSObject::DropDatabase() {
  AssertIsOnOwningThread();

  mDatabase = nullptr;
}

nsresult LSObject::EnsureObserver() {
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
  params.storagePrincipalInfo() = *mStoragePrincipalInfo;
  params.clientId() = mClientId;
  params.clientPrincipalInfo() = mClientPrincipalInfo;

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
  // request on the RemoteLazyInputStream thread).
  // Let's create a direct connection to the observer (through an observer
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the observer.

  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(backgroundActor);

  RefPtr<LSObserver> observer = new LSObserver(mOrigin);

  LSObserverChild* actor = new LSObserverChild(observer);

  MOZ_ALWAYS_TRUE(
      backgroundActor->SendPBackgroundLSObserverConstructor(actor, observerId));

  observer->SetActor(actor);

  mObserver = std::move(observer);

  return NS_OK;
}

void LSObject::DropObserver() {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver = nullptr;
  }
}

void LSObject::OnChange(const nsAString& aKey, const nsAString& aOldValue,
                        const nsAString& aNewValue) {
  AssertIsOnOwningThread();

  NotifyChange(/* aStorage */ this, StoragePrincipal(), aKey, aOldValue,
               aNewValue, /* aStorageType */ kLocalStorageType, mDocumentURI,
               /* aIsPrivate */ !!mPrivateBrowsingId,
               /* aImmediateDispatch */ false);
}

void LSObject::LastRelease() {
  AssertIsOnOwningThread();

  DropDatabase();
}

nsresult RequestHelper::StartAndReturnResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  nsCOMPtr<nsIEventTarget> domFileThread =
      RemoteLazyInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!domFileThread)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  nsresult rv = domFileThread->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  TimeStamp deadline = TimeStamp::Now() + TimeDuration::FromMilliseconds(
                                              FAILSAFE_CANCEL_SYNC_OP_MS);

  MonitorAutoLock lock(mMonitor);
  while (mState != State::Complete) {
    TimeStamp now = TimeStamp::Now();
    // If we are expecting shutdown or have passed our deadline, immediately
    // dispatch ourselves to the DOM File thread to cancel the operation.  We
    // don't abort until the cancellation has gone through, as otherwise we
    // could race with the DOM File thread.
    if (mozilla::ipc::ProcessChild::ExpectingShutdown() || now >= deadline) {
      switch (mState) {
        case State::Initial:
          // The DOM File thread never even woke before ExpectingShutdown() or a
          // timeout - skip even creating the actor and just report an error.
          mResultCode = NS_ERROR_FAILURE;
          mState = State::Complete;
          continue;
        case State::ResponsePending:
          // The DOM File thread is currently waiting for a reply, switch to a
          // canceling state, and notify it to cancel by dispatching a runnable.
          mState = State::Canceling;
          MOZ_ALWAYS_SUCCEEDS(
              domFileThread->Dispatch(this, NS_DISPATCH_NORMAL));
          [[fallthrough]];
        case State::Canceling:
          // We've cancelled the request, so just need to wait indefinitely for
          // it to complete.
          lock.Wait();
          continue;
        default:
          MOZ_ASSERT_UNREACHABLE("unexpected state");
      }
    }

    // Wait until either we reach out deadline or for SYNC_OP_WAIT_INTERVAL_MS.
    lock.Wait(TimeDuration::Min(
        TimeDuration::FromMilliseconds(SYNC_OP_WAKE_INTERVAL_MS),
        deadline - now));
  }

  // The operation is complete, clear our reference to the LSObject.
  mObject = nullptr;

  if (NS_WARN_IF(NS_FAILED(mResultCode))) {
    return mResultCode;
  }

  aResponse = std::move(mResponse);
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(RequestHelper, Runnable)

NS_IMETHODIMP
RequestHelper::Run() {
  AssertIsOnDOMFileThread();

  MonitorAutoLock lock(mMonitor);

  switch (mState) {
    case State::Initial: {
      mState = State::ResponsePending;
      {
        MonitorAutoUnlock unlock(mMonitor);
        mActor = mObject->StartRequest(mParams, this);
      }
      if (NS_WARN_IF(!mActor) && mState != State::Complete) {
        // If we fail to even create the actor, instantly fail and notify our
        // caller of the error. Otherwise we'll notify from OnResponse as called
        // by the actor.
        mResultCode = NS_ERROR_FAILURE;
        mState = State::Complete;
        lock.Notify();
      }
      return NS_OK;
    }

    case State::Canceling: {
      // StartRequest() could fail or OnResponse was already called, so we need
      // to check if actor is not null. The actor can also be in the final
      // (finishing) state, in that case we are not allowed to send the cancel
      // message and it wouldn't make any sense because the request is about to
      // be destroyed anyway.
      if (mActor && !mActor->Finishing()) {
        mActor->SendCancel();
      }
      return NS_OK;
    }

    case State::Complete: {
      // The operation was cancelled before we ran, do nothing.
      return NS_OK;
    }

    default:
      MOZ_CRASH("Bad state!");
  }
}

void RequestHelper::OnResponse(const LSRequestResponse& aResponse) {
  AssertIsOnDOMFileThread();

  MonitorAutoLock lock(mMonitor);

  MOZ_ASSERT(mState == State::ResponsePending || mState == State::Canceling);

  mActor = nullptr;

  mResponse = aResponse;

  mState = State::Complete;

  lock.Notify();
}

}  // namespace mozilla::dom
