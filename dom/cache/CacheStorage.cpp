/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheStorage.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/CacheStorageBinding.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/cache/AutoUtils.h"
#include "mozilla/dom/cache/Cache.h"
#include "mozilla/dom/cache/CacheChild.h"
#include "mozilla/dom/cache/CacheStorageChild.h"
#include "mozilla/dom/cache/CacheWorkerHolder.h"
#include "mozilla/dom/cache/PCacheChild.h"
#include "mozilla/dom/cache/ReadStream.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsURLParsers.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::Unused;
using mozilla::ErrorResult;
using mozilla::dom::workers::WorkerPrivate;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::IProtocol;
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
struct CacheStorage::Entry final
{
  RefPtr<Promise> mPromise;
  CacheOpArgs mArgs;
  // We cannot add the requests until after the actor is present.  So store
  // the request data separately for now.
  RefPtr<InternalRequest> mRequest;
};

namespace {

bool
IsTrusted(const PrincipalInfo& aPrincipalInfo, bool aTestingPrefEnabled)
{
  // Can happen on main thread or worker thread

  if (aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return true;
  }

  // Require a ContentPrincipal to avoid null principal, etc.
  if (NS_WARN_IF(aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo)) {
    return false;
  }

  // If we're in testing mode, then don't do any more work to determing if
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
  const char* url = flatURL.get();

  // off the main thread URL parsing using nsStdURLParser.
  nsCOMPtr<nsIURLParser> urlParser = new nsStdURLParser();

  uint32_t schemePos;
  int32_t schemeLen;
  uint32_t authPos;
  int32_t authLen;
  nsresult rv = urlParser->ParseURL(url, flatURL.Length(),
                                    &schemePos, &schemeLen,
                                    &authPos, &authLen,
                                    nullptr, nullptr);      // ignore path
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  nsAutoCString scheme(Substring(flatURL, schemePos, schemeLen));
  if (scheme.LowerCaseEqualsLiteral("https") ||
      scheme.LowerCaseEqualsLiteral("file")) {
    return true;
  }

  uint32_t hostPos;
  int32_t hostLen;

  rv = urlParser->ParseAuthority(url + authPos, authLen,
                                 nullptr, nullptr,          // ignore username
                                 nullptr, nullptr,          // ignore password
                                 &hostPos, &hostLen,
                                 nullptr);                  // ignore port
  if (NS_WARN_IF(NS_FAILED(rv))) { return false; }

  nsDependentCSubstring hostname(url + authPos + hostPos, hostLen);

  return hostname.EqualsLiteral("localhost") ||
         hostname.EqualsLiteral("127.0.0.1") ||
         hostname.EqualsLiteral("::1");
}

} // namespace

// static
already_AddRefed<CacheStorage>
CacheStorage::CreateOnMainThread(Namespace aNamespace, nsIGlobalObject* aGlobal,
                                 nsIPrincipal* aPrincipal, bool aStorageDisabled,
                                 bool aForceTrustedOrigin, ErrorResult& aRv)
{
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(aPrincipal);
  MOZ_ASSERT(NS_IsMainThread());

  if (aStorageDisabled) {
    NS_WARNING("CacheStorage has been disabled.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  PrincipalInfo principalInfo;
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  bool testingEnabled = aForceTrustedOrigin ||
    Preferences::GetBool("dom.caches.testing.enabled", false) ||
    Preferences::GetBool("dom.serviceWorkers.testing.enabled", false);

  if (!IsTrusted(principalInfo, testingEnabled)) {
    NS_WARNING("CacheStorage not supported on untrusted origins.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  RefPtr<CacheStorage> ref = new CacheStorage(aNamespace, aGlobal,
                                                principalInfo, nullptr);
  return ref.forget();
}

// static
already_AddRefed<CacheStorage>
CacheStorage::CreateOnWorker(Namespace aNamespace, nsIGlobalObject* aGlobal,
                             WorkerPrivate* aWorkerPrivate, ErrorResult& aRv)
{
  MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (!aWorkerPrivate->IsStorageAllowed()) {
    NS_WARNING("CacheStorage is not allowed.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  if (aWorkerPrivate->GetOriginAttributes().mPrivateBrowsingId > 0) {
    NS_WARNING("CacheStorage not supported during private browsing.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  RefPtr<CacheWorkerHolder> workerHolder =
    CacheWorkerHolder::Create(aWorkerPrivate,
                              CacheWorkerHolder::AllowIdleShutdownStart);
  if (!workerHolder) {
    NS_WARNING("Worker thread is shutting down.");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  const PrincipalInfo& principalInfo = aWorkerPrivate->GetPrincipalInfo();

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
  bool testingEnabled = aWorkerPrivate->DOMCachesTestingEnabled() ||
                        aWorkerPrivate->ServiceWorkersTestingEnabled() ||
                        aWorkerPrivate->ServiceWorkersTestingInWindow() ||
                        aWorkerPrivate->IsServiceWorker();

  if (!IsTrusted(principalInfo, testingEnabled)) {
    NS_WARNING("CacheStorage not supported on untrusted origins.");
    RefPtr<CacheStorage> ref = new CacheStorage(NS_ERROR_DOM_SECURITY_ERR);
    return ref.forget();
  }

  RefPtr<CacheStorage> ref = new CacheStorage(aNamespace, aGlobal,
                                              principalInfo, workerHolder);
  return ref.forget();
}

// static
bool
CacheStorage::DefineCaches(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(js::GetObjectClass(aGlobal)->flags & JSCLASS_DOM_GLOBAL,
                                           "Passed object is not a global object!");
  js::AssertSameCompartment(aCx, aGlobal);

  if (NS_WARN_IF(!CacheStorageBinding::GetConstructorObject(aCx) ||
                 !CacheBinding::GetConstructorObject(aCx))) {
    return false;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal);
  MOZ_DIAGNOSTIC_ASSERT(principal);

  ErrorResult rv;
  RefPtr<CacheStorage> storage =
    CreateOnMainThread(DEFAULT_NAMESPACE, xpc::NativeGlobal(aGlobal), principal,
                       false, /* private browsing */
                       true,  /* force trusted */
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
                           CacheWorkerHolder* aWorkerHolder)
  : mNamespace(aNamespace)
  , mGlobal(aGlobal)
  , mPrincipalInfo(MakeUnique<PrincipalInfo>(aPrincipalInfo))
  , mActor(nullptr)
  , mStatus(NS_OK)
{
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);

  // If the PBackground actor is already initialized then we can
  // immediately use it
  PBackgroundChild* actor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actor)) {
    mStatus = NS_ERROR_UNEXPECTED;
    return;
  }

  // WorkerHolder ownership is passed to the CacheStorageChild actor and any
  // actors it may create.  The WorkerHolder will keep the worker thread alive
  // until the actors can gracefully shutdown.
  CacheStorageChild* newActor = new CacheStorageChild(this, aWorkerHolder);
  PCacheStorageChild* constructedActor =
    actor->SendPCacheStorageConstructor(newActor, mNamespace, *mPrincipalInfo);

  if (NS_WARN_IF(!constructedActor)) {
    mStatus = NS_ERROR_UNEXPECTED;
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(constructedActor == newActor);
  mActor = newActor;
}

CacheStorage::CacheStorage(nsresult aFailureResult)
  : mNamespace(INVALID_NAMESPACE)
  , mActor(nullptr)
  , mStatus(aFailureResult)
{
  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(mStatus));
}

already_AddRefed<Promise>
CacheStorage::Match(JSContext* aCx, const RequestOrUSVString& aRequest,
                    const CacheQueryOptions& aOptions, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<InternalRequest> request =
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

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageMatchArgs(CacheRequest(), params, GetOpenMode());
  entry->mRequest = request;

  RunRequest(Move(entry));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Has(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageHasArgs(nsString(aKey));

  RunRequest(Move(entry));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Open(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageOpenArgs(nsString(aKey));

  RunRequest(Move(entry));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Delete(const nsAString& aKey, ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageDeleteArgs(nsString(aKey));

  RunRequest(Move(entry));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Keys(ErrorResult& aRv)
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);

  if (NS_WARN_IF(NS_FAILED(mStatus))) {
    aRv.Throw(mStatus);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  nsAutoPtr<Entry> entry(new Entry());
  entry->mPromise = promise;
  entry->mArgs = StorageKeysArgs();

  RunRequest(Move(entry));

  return promise.forget();
}

// static
already_AddRefed<CacheStorage>
CacheStorage::Constructor(const GlobalObject& aGlobal,
                          CacheStorageNamespace aNamespace,
                          nsIPrincipal* aPrincipal, ErrorResult& aRv)
{
  if (NS_WARN_IF(!NS_IsMainThread())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // TODO: remove Namespace in favor of CacheStorageNamespace
  static_assert(DEFAULT_NAMESPACE == (uint32_t)CacheStorageNamespace::Content,
                "Default namespace should match webidl Content enum");
  static_assert(CHROME_ONLY_NAMESPACE == (uint32_t)CacheStorageNamespace::Chrome,
                "Chrome namespace should match webidl Chrome enum");
  static_assert(NUMBER_OF_NAMESPACES == (uint32_t)CacheStorageNamespace::EndGuard_,
                "Number of namespace should match webidl endguard enum");

  Namespace ns = static_cast<Namespace>(aNamespace);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  bool privateBrowsing = false;
  if (nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global)) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (doc) {
      nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
      privateBrowsing = loadContext && loadContext->UsePrivateBrowsing();
    }
  }

  // Create a CacheStorage object bypassing the trusted origin checks
  // since this is a chrome-only constructor.
  return CreateOnMainThread(ns, global, aPrincipal, privateBrowsing,
                            true /* force trusted origin */, aRv);
}

nsISupports*
CacheStorage::GetParentObject() const
{
  return mGlobal;
}

JSObject*
CacheStorage::WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::CacheStorageBinding::Wrap(aContext, this, aGivenProto);
}

void
CacheStorage::DestroyInternal(CacheStorageChild* aActor)
{
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

nsIGlobalObject*
CacheStorage::GetGlobalObject() const
{
  return mGlobal;
}

#ifdef DEBUG
void
CacheStorage::AssertOwningThread() const
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
}
#endif

PBackgroundChild*
CacheStorage::GetIPCManager()
{
  // This is true because CacheStorage always uses IgnoreBody for requests.
  // So we should never need to get the IPC manager during Request or
  // Response serialization.
  MOZ_CRASH("CacheStorage does not implement TypeUtils::GetIPCManager()");
}

CacheStorage::~CacheStorage()
{
  NS_ASSERT_OWNINGTHREAD(CacheStorage);
  if (mActor) {
    mActor->StartDestroyFromListener();
    // DestroyInternal() is called synchronously by StartDestroyFromListener().
    // So we should have already cleared the mActor.
    MOZ_DIAGNOSTIC_ASSERT(!mActor);
  }
}

void
CacheStorage::RunRequest(nsAutoPtr<Entry>&& aEntry)
{
  MOZ_ASSERT(mActor);

  nsAutoPtr<Entry> entry(Move(aEntry));

  AutoChildOpArgs args(this, entry->mArgs, 1);

  if (entry->mRequest) {
    ErrorResult rv;
    args.Add(entry->mRequest, IgnoreBody, IgnoreInvalidScheme, rv);
    if (NS_WARN_IF(rv.Failed())) {
      entry->mPromise->MaybeReject(rv);
      return;
    }
  }

  mActor->ExecuteOp(mGlobal, entry->mPromise, this, args.SendAsOpArgs());
}

OpenMode
CacheStorage::GetOpenMode() const
{
  return mNamespace == CHROME_ONLY_NAMESPACE ? OpenMode::Eager : OpenMode::Lazy;
}

} // namespace cache
} // namespace dom
} // namespace mozilla
