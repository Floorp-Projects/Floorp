/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorage.h"

#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/CacheStorageBinding.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheCommon.h"
#include "mozilla/dom/cache/CacheStorageChild.h"
#include "mozilla/dom/cache/CacheWorkerRef.h"
#include "mozilla/dom/cache/PCacheChild.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIGlobalObject.h"
#include "nsMixedContentBlocker.h"
#include "nsURLParsers.h"
#include "js/Object.h"              // JS::GetClass
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::dom::cache {

using mozilla::ErrorResult;
using mozilla::dom::quota::QuotaManager;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalToPrincipalInfo;

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::dom::cache::CacheStorage);
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::dom::cache::CacheStorage);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(mozilla::dom::cache::CacheStorage,
                                      mGlobal);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// We cannot reference IPC types in a webidl binding implementation header.  So
// define this in the .cpp.
struct CacheStorage::Entry final {
  RefPtr<Promise> mPromise;
  CacheOpArgs mArgs;
  // We cannot add the requests until after the actor is present.  So store
  // the request data separately for now.
  SafeRefPtr<InternalRequest> mRequest;
};

namespace {

bool IsTrusted(const PrincipalInfo& aPrincipalInfo, bool aTestingPrefEnabled) {
  // Can happen on main thread or worker thread

  if (aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return true;
  }

  // Require a ContentPrincipal to avoid null principal, etc.
  QM_TRY(OkIf(aPrincipalInfo.type() == PrincipalInfo::TContentPrincipalInfo),
         false);

  // If we're in testing mode, then don't do any more work to determine if
  // the origin is trusted.  We have to run some tests as http.
  if (aTestingPrefEnabled) {
    return true;
  }

  // Now parse the scheme of the principal's origin.  This is a short term
  // method for determining "trust".  In the long term we need to implement
  // the full algorithm here:
  //
  // https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-secure
  //
  // TODO: Implement full secure setting algorithm. (bug 1177856)

  const nsCString& flatURL = aPrincipalInfo.get_ContentPrincipalInfo().spec();
  const char* const url = flatURL.get();

  // off the main thread URL parsing using nsStdURLParser.
  const nsCOMPtr<nsIURLParser> urlParser = new nsStdURLParser();

  uint32_t schemePos;
  int32_t schemeLen;
  uint32_t authPos;
  int32_t authLen;
  QM_TRY(MOZ_TO_RESULT(urlParser->ParseURL(url, flatURL.Length(), &schemePos,
                                           &schemeLen, &authPos, &authLen,
                                           nullptr, nullptr)),  // ignore path
         false);

  const nsAutoCString scheme(Substring(flatURL, schemePos, schemeLen));
  if (scheme.LowerCaseEqualsLiteral("https") ||
      scheme.LowerCaseEqualsLiteral("file") ||
      scheme.LowerCaseEqualsLiteral("moz-extension")) {
    return true;
  }

  uint32_t hostPos;
  int32_t hostLen;
  QM_TRY(MOZ_TO_RESULT(
             urlParser->ParseAuthority(url + authPos, authLen, nullptr,
                                       nullptr,           // ignore username
                                       nullptr, nullptr,  // ignore password
                                       &hostPos, &hostLen,
                                       nullptr)),  // ignore port
         false);

  return nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackHost(
      nsDependentCSubstring(url + authPos + hostPos, hostLen));
}

}  // namespace

// static
already_AddRefed<CacheStorage> CacheStorage::CreateOnMainThread(
    Namespace aNamespace, nsIGlobalObject* aGlobal, nsIPrincipal* aPrincipal,
    bool aForceTrustedOrigin, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(aPrincipal);
  MOZ_ASSERT(NS_IsMainThread());

  PrincipalInfo principalInfo;
  QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(aPrincipal, &principalInfo)),
         nullptr, [&aRv](const nsresult rv) { aRv.Throw(rv); });

  QM_TRY(OkIf(QuotaManager::IsPrincipalInfoValid(principalInfo)),
         RefPtr{new CacheStorage(NS_ERROR_DOM_SECURITY_ERR)}.forget(),
         [](const auto) {
           NS_WARNING("CacheStorage not supported on invalid origins.");
         });

  const bool testingEnabled =
      aForceTrustedOrigin ||
      Preferences::GetBool("dom.caches.testing.enabled", false) ||
      StaticPrefs::dom_serviceWorkers_testing_enabled();

  if (!IsTrusted(principalInfo, testingEnabled)) {
    NS_WARNING("CacheStorage not supported on untrusted origins.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  RefPtr<CacheStorage> ref =
      new CacheStorage(aNamespace, aGlobal, principalInfo, nullptr);
  return ref.forget();
}

// static
already_AddRefed<CacheStorage> CacheStorage::CreateOnWorker(
    Namespace aNamespace, nsIGlobalObject* aGlobal,
    WorkerPrivate* aWorkerPrivate, ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (aWorkerPrivate->GetOriginAttributes().mPrivateBrowsingId > 0 &&
      !StaticPrefs::dom_cache_privateBrowsing_enabled()) {
    NS_WARNING("CacheStorage not supported during private browsing.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  SafeRefPtr<CacheWorkerRef> workerRef =
      CacheWorkerRef::Create(aWorkerPrivate, CacheWorkerRef::eIPCWorkerRef);
  if (!workerRef) {
    NS_WARNING("Worker thread is shutting down.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  const PrincipalInfo& principalInfo =
      aWorkerPrivate->GetEffectiveStoragePrincipalInfo();

  QM_TRY(OkIf(QuotaManager::IsPrincipalInfoValid(principalInfo)), nullptr,
         [&aRv](const auto) { aRv.Throw(NS_ERROR_FAILURE); });

  // We have a number of cases where we want to skip the https scheme
  // validation:
  //
  // 1) Any worker when dom.caches.testing.enabled pref is true.
  // 2) Any worker when dom.serviceWorkers.testing.enabled pref is true.  This
  //    is mainly because most sites using SWs will expect Cache to work if
  //    SWs are enabled.
  // 3) If the window that created this worker has the devtools SW testing
  //    option enabled.  Same reasoning as (2).
  // 4) If the worker itself is a ServiceWorker, then we always skip the
  //    origin checks.  The ServiceWorker has its own trusted origin checks
  //    that are better than ours.  In addition, we don't have information
  //    about the window any more, so we can't do our own checks.
  bool testingEnabled = StaticPrefs::dom_caches_testing_enabled() ||
                        StaticPrefs::dom_serviceWorkers_testing_enabled() ||
                        aWorkerPrivate->ServiceWorkersTestingInWindow() ||
                        aWorkerPrivate->IsServiceWorker();

  if (!IsTrusted(principalInfo, testingEnabled)) {
    NS_WARNING("CacheStorage not supported on untrusted origins.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  RefPtr<CacheStorage> ref = new CacheStorage(
      aNamespace, aGlobal, principalInfo, std::move(workerRef));
  return ref.forget();
}

// static
bool CacheStorage::DefineCaches(JSContext* aCx, JS::Handle<JSObject*> aGlobal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(JS::GetClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
                        "Passed object is not a global object!");
  js::AssertSameCompartment(aCx, aGlobal);

  if (NS_WARN_IF(!CacheStorage_Binding::GetConstructorObject(aCx) ||
                 !Cache_Binding::GetConstructorObject(aCx))) {
    return false;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(principal);

  ErrorResult rv;
  RefPtr<CacheStorage> storage =
      CreateOnMainThread(DEFAULT_NAMESPACE, xpc::NativeGlobal(aGlobal),
                         principal, true, /* force trusted */
                         rv);
  if (NS_WARN_IF(rv.MaybeSetPendingException(aCx))) {
    return false;
  }

  JS::Rooted<JS::Value> caches(aCx);
  if (NS_WARN_IF(!ToJSValue(aCx, storage, &caches))) {
    return false;
  }

  return JS_DefineProperty(aCx, aGlobal, "caches", caches, JSPROP_ENUMERATE);
}

CacheStorage::CacheStorage(Namespace aNamespace, nsIGlobalObject* aGlobal,
                           const PrincipalInfo& aPrincipalInfo,
                           SafeRefPtr<CacheWorkerRef> aWorkerRef)
    : mNamespace(aNamespace),
      mGlobal(aGlobal),
      mPrincipalInfo(MakeUnique<PrincipalInfo>(aPrincipalInfo)),
      mActor(nullptr),
      mStatus(NS_OK) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);

  // If the PBackground actor is already initialized then we can
  // immediately use it
  PBackgroundChild* actor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actor)) {
    mStatus = NS_ERROR_UNEXPECTED;
    return;
  }

  // WorkerRef ownership is passed to the CacheStorageChild actor and any
  // actors it may create.  The WorkerRef will keep the worker thread alive
  // until the actors can gracefully shutdown.
  CacheStorageChild* newActor =
      new CacheStorageChild(this, std::move(aWorkerRef));
  PCacheStorageChild* constructedActor = actor->SendPCacheStorageConstructor(
      newActor, mNamespace, *mPrincipalInfo);

  if (NS_WARN_IF(!constructedActor)) {
    mStatus = NS_ERROR_UNEXPECTED;
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(constructedActor == newActor);
  mActor = newActor;
}

CacheStorage::CacheStorage(nsresult aFailureResult)
    : mNamespace(INVALID_NAMESPACE), mActor(nullptr), mStatus(aFailureResult) {
  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mStatus));
}

already_AddRefed<Promise> CacheStorage::Match(
    JSContext* aCx, const RequestOrUSVString& aRequest,
    const MultiCacheQueryOptions& aOptions, ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (!HasStorageAccess(eUseCounter_custom_PrivateBrowsingCachesMatch,
                        UseCounterWorker::Custom_PrivateBrowsingCachesMatch)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  SafeRefPtr<InternalRequest> request =
      ToInternalRequest(aCx, aRequest, IgnoreBody, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  CacheQueryParams params;
  ToCacheQueryParams(params, aOptions);

  auto entry = MakeUnique<Entry>();
  entry->mPromise = promise;
  entry->mArgs = StorageMatchArgs(CacheRequest(), params, GetOpenMode());
  entry->mRequest = std::move(request);

  RunRequest(std::move(entry));

  return promise.forget();
}

already_AddRefed<Promise> CacheStorage::Has(const nsAString& aKey,
                                            ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (!HasStorageAccess(eUseCounter_custom_PrivateBrowsingCachesHas,
                        UseCounterWorker::Custom_PrivateBrowsingCachesHas)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  auto entry = MakeUnique<Entry>();
  entry->mPromise = promise;
  entry->mArgs = StorageHasArgs(nsString(aKey));

  RunRequest(std::move(entry));

  return promise.forget();
}

already_AddRefed<Promise> CacheStorage::Open(const nsAString& aKey,
                                             ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (!HasStorageAccess(eUseCounter_custom_PrivateBrowsingCachesOpen,
                        UseCounterWorker::Custom_PrivateBrowsingCachesOpen)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  auto entry = MakeUnique<Entry>();
  entry->mPromise = promise;
  entry->mArgs = StorageOpenArgs(nsString(aKey));

  RunRequest(std::move(entry));

  return promise.forget();
}

already_AddRefed<Promise> CacheStorage::Delete(const nsAString& aKey,
                                               ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (!HasStorageAccess(eUseCounter_custom_PrivateBrowsingCachesDelete,
                        UseCounterWorker::Custom_PrivateBrowsingCachesDelete)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  auto entry = MakeUnique<Entry>();
  entry->mPromise = promise;
  entry->mArgs = StorageDeleteArgs(nsString(aKey));

  RunRequest(std::move(entry));

  return promise.forget();
}

already_AddRefed<Promise> CacheStorage::Keys(ErrorResult& aRv) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (!HasStorageAccess(eUseCounter_custom_PrivateBrowsingCachesKeys,
                        UseCounterWorker::Custom_PrivateBrowsingCachesKeys)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  auto entry = MakeUnique<Entry>();
  entry->mPromise = promise;
  entry->mArgs = StorageKeysArgs();

  RunRequest(std::move(entry));

  return promise.forget();
}

// static
already_AddRefed<CacheStorage> CacheStorage::Constructor(
    const GlobalObject& aGlobal, CacheStorageNamespace aNamespace,
    nsIPrincipal* aPrincipal, ErrorResult& aRv) {
  if (NS_WARN_IF(!NS_IsMainThread())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // TODO: remove Namespace in favor of CacheStorageNamespace
  static_assert(DEFAULT_NAMESPACE == (uint32_t)CacheStorageNamespace::Content,
                "Default namespace should match webidl Content enum");
  static_assert(
      CHROME_ONLY_NAMESPACE == (uint32_t)CacheStorageNamespace::Chrome,
      "Chrome namespace should match webidl Chrome enum");
  static_assert(NUMBER_OF_NAMESPACES == CacheStorageNamespaceValues::Count,
                "Number of namespace should match webidl count");

  Namespace ns = static_cast<Namespace>(aNamespace);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  bool privateBrowsing = false;
  if (nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global)) {
    RefPtr<Document> doc = window->GetExtantDoc();
    if (doc) {
      nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
      privateBrowsing = loadContext && loadContext->UsePrivateBrowsing();
    }
  }

  if (privateBrowsing && !StaticPrefs::dom_cache_privateBrowsing_enabled()) {
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  // Create a CacheStorage object bypassing the trusted origin checks
  // since this is a chrome-only constructor.
  return CreateOnMainThread(ns, global, aPrincipal,
                            true /* force trusted origin */, aRv);
}

nsISupports* CacheStorage::GetParentObject() const { return mGlobal; }

JSObject* CacheStorage::WrapObject(JSContext* aContext,
                                   JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::CacheStorage_Binding::Wrap(aContext, this, aGivenProto);
}

void CacheStorage::DestroyInternal(CacheStorageChild* aActor) {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  MOZ_DIAGNOSTIC_ASSERT(mActor);
  MOZ_DIAGNOSTIC_ASSERT(mActor == aActor);
  MOZ_DIAGNOSTIC_ASSERT(!NS_FAILED(mStatus));
  mActor->ClearListener();
  mActor = nullptr;
  mStatus = NS_ERROR_UNEXPECTED;

  // Note that we will never get an actor again in case another request is
  // made before this object is destructed.
}

nsIGlobalObject* CacheStorage::GetGlobalObject() const { return mGlobal; }

#ifdef DEBUG
void CacheStorage::AssertOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
}
#endif

PBackgroundChild* CacheStorage::GetIPCManager() {
  // This is true because CacheStorage always uses IgnoreBody for requests.
  // So we should never need to get the IPC manager during Request or
  // Response serialization.
  MOZ_CRASH("CacheStorage does not implement TypeUtils::GetIPCManager()");
}

CacheStorage::~CacheStorage() {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  if (mActor) {
    mActor->StartDestroyFromListener();
    // DestroyInternal() is called synchronously by StartDestroyFromListener().
    // So we should have already cleared the mActor.
    MOZ_DIAGNOSTIC_ASSERT(!mActor);
  }
}

void CacheStorage::RunRequest(UniquePtr<Entry> aEntry) {
  MOZ_ASSERT(mActor);

  AutoChildOpArgs args(this, aEntry->mArgs, 1);

  if (aEntry->mRequest) {
    ErrorResult rv;
    args.Add(*aEntry->mRequest, IgnoreBody, IgnoreInvalidScheme, rv);
    if (NS_WARN_IF(rv.Failed())) {
      aEntry->mPromise->MaybeReject(std::move(rv));
      return;
    }
  }

  mActor->ExecuteOp(mGlobal, aEntry->mPromise, this, args.SendAsOpArgs());
}

OpenMode CacheStorage::GetOpenMode() const {
  return mNamespace == CHROME_ONLY_NAMESPACE ? OpenMode::Eager : OpenMode::Lazy;
}

bool CacheStorage::HasStorageAccess(UseCounter aLabel,
                                    UseCounterWorker aLabelWorker) const {
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  if (NS_WARN_IF(!mGlobal)) {
    return false;
  }

  StorageAccess access = mGlobal->GetStorageAccess();
  if (access == StorageAccess::ePrivateBrowsing) {
    if (NS_IsMainThread()) {
      SetUseCounter(mGlobal->GetGlobalJSObject(), aLabel);
    } else {
      SetUseCounter(aLabelWorker);
    }
  }

  // Deny storage access for private browsing unless pref is toggled on.
  if (nsIPrincipal* principal = mGlobal->PrincipalOrNull()) {
    if (!principal->IsSystemPrincipal() &&
        principal->GetPrivateBrowsingId() !=
            nsIScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID &&
        !StaticPrefs::dom_cache_privateBrowsing_enabled()) {
      return false;
    }
  }

  return access > StorageAccess::eDeny ||
         (StaticPrefs::
              privacy_partition_always_partition_third_party_non_cookie_storage() &&
          ShouldPartitionStorage(access));
}

}  // namespace mozilla::dom::cache
