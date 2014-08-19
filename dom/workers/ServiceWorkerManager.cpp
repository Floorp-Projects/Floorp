/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManager.h"

#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsPIDOMWindow.h"

#include "jsapi.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/InstallEventBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"

#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

#include "RuntimeService.h"
#include "ServiceWorker.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerEvents.h"
#include "WorkerInlines.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

using namespace mozilla;
using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

NS_IMPL_ISUPPORTS0(ServiceWorkerRegistration)

UpdatePromise::UpdatePromise()
  : mState(Pending)
{
  MOZ_COUNT_CTOR(UpdatePromise);
}

UpdatePromise::~UpdatePromise()
{
  MOZ_COUNT_DTOR(UpdatePromise);
}

void
UpdatePromise::AddPromise(Promise* aPromise)
{
  MOZ_ASSERT(mState == Pending);
  mPromises.AppendElement(aPromise);
}

void
UpdatePromise::ResolveAllPromises(const nsACString& aScriptSpec, const nsACString& aScope)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mState == Pending);
  mState = Resolved;
  RuntimeService* rs = RuntimeService::GetOrCreateService();
  MOZ_ASSERT(rs);

  nsTArray<WeakPtr<Promise>> array;
  array.SwapElements(mPromises);
  for (uint32_t i = 0; i < array.Length(); ++i) {
    WeakPtr<Promise>& pendingPromise = array.ElementAt(i);
    if (pendingPromise) {
      nsCOMPtr<nsIGlobalObject> go =
        do_QueryInterface(pendingPromise->GetParentObject());
      MOZ_ASSERT(go);

      AutoSafeJSContext cx;
      JS::Rooted<JSObject*> global(cx, go->GetGlobalJSObject());
      JSAutoCompartment ac(cx, global);

      GlobalObject domGlobal(cx, global);

      nsRefPtr<ServiceWorker> serviceWorker;
      nsresult rv = rs->CreateServiceWorker(domGlobal,
                                            NS_ConvertUTF8toUTF16(aScriptSpec),
                                            aScope,
                                            getter_AddRefs(serviceWorker));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        pendingPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
        continue;
      }

      pendingPromise->MaybeResolve(serviceWorker);
    }
  }
}

void
UpdatePromise::RejectAllPromises(nsresult aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mState == Pending);
  mState = Rejected;

  nsTArray<WeakPtr<Promise>> array;
  array.SwapElements(mPromises);
  for (uint32_t i = 0; i < array.Length(); ++i) {
    WeakPtr<Promise>& pendingPromise = array.ElementAt(i);
    if (pendingPromise) {
      // Since ServiceWorkerContainer is only exposed to windows we can be
      // certain about this cast.
      nsCOMPtr<nsPIDOMWindow> window =
        do_QueryInterface(pendingPromise->GetParentObject());
      MOZ_ASSERT(window);
      nsRefPtr<DOMError> domError = new DOMError(window, aRv);
      pendingPromise->MaybeRejectBrokenly(domError);
    }
  }
}

void
UpdatePromise::RejectAllPromises(const ErrorEventInit& aErrorDesc)
{
  MOZ_ASSERT(mState == Pending);
  mState = Rejected;

  nsTArray<WeakPtr<Promise>> array;
  array.SwapElements(mPromises);
  for (uint32_t i = 0; i < array.Length(); ++i) {
    WeakPtr<Promise>& pendingPromise = array.ElementAt(i);
    if (pendingPromise) {
      // Since ServiceWorkerContainer is only exposed to windows we can be
      // certain about this cast.
      nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(pendingPromise->GetParentObject());
      MOZ_ASSERT(go);

      AutoJSAPI jsapi;
      jsapi.Init(go);

      JSContext* cx = jsapi.cx();

      JS::Rooted<JSString*> stack(cx, JS_GetEmptyString(JS_GetRuntime(cx)));

      JS::Rooted<JS::Value> fnval(cx);
      ToJSValue(cx, aErrorDesc.mFilename, &fnval);
      JS::Rooted<JSString*> fn(cx, fnval.toString());

      JS::Rooted<JS::Value> msgval(cx);
      ToJSValue(cx, aErrorDesc.mMessage, &msgval);
      JS::Rooted<JSString*> msg(cx, msgval.toString());

      JS::Rooted<JS::Value> error(cx);
      if (!JS::CreateError(cx, JSEXN_ERR, stack, fn, aErrorDesc.mLineno,
                           aErrorDesc.mColno, nullptr, msg, &error)) {
        pendingPromise->MaybeReject(NS_ERROR_FAILURE);
        continue;
      }

      pendingPromise->MaybeReject(cx, error);
    }
  }
}

class FinishFetchOnMainThreadRunnable : public nsRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateInstance> mUpdateInstance;
public:
  FinishFetchOnMainThreadRunnable
    (const nsMainThreadPtrHandle<ServiceWorkerUpdateInstance>& aUpdateInstance)
    : mUpdateInstance(aUpdateInstance)
  { }

  NS_IMETHOD
  Run() MOZ_OVERRIDE;
};

class FinishSuccessfulFetchWorkerRunnable : public WorkerRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerUpdateInstance> mUpdateInstance;
public:
  FinishSuccessfulFetchWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                                      const nsMainThreadPtrHandle<ServiceWorkerUpdateInstance>& aUpdateInstance)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
      mUpdateInstance(aUpdateInstance)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    if (!aWorkerPrivate->WorkerScriptExecutedSuccessfully()) {
      return true;
    }

    nsRefPtr<FinishFetchOnMainThreadRunnable> r =
      new FinishFetchOnMainThreadRunnable(mUpdateInstance);
    NS_DispatchToMainThread(r);
    return true;
  }
};

// Allows newer calls to Update() to 'abort' older calls.
// Each call to Update() creates the instance which handles creating the
// worker and queues up a runnable to resolve the update promise once the
// worker has successfully been parsed.
class ServiceWorkerUpdateInstance MOZ_FINAL : public nsISupports
{
  // Owner of this instance.
  ServiceWorkerRegistration* mRegistration;
  nsCString mScriptSpec;
  nsCOMPtr<nsPIDOMWindow> mWindow;

  bool mAborted;

  ~ServiceWorkerUpdateInstance() {}

public:
  NS_DECL_ISUPPORTS

  ServiceWorkerUpdateInstance(ServiceWorkerRegistration *aRegistration,
                              nsPIDOMWindow* aWindow)
    : mRegistration(aRegistration),
      // Capture the current script spec in case register() gets called.
      mScriptSpec(aRegistration->mScriptSpec),
      mWindow(aWindow),
      mAborted(false)
  {
    AssertIsOnMainThread();
  }

  const nsCString&
  GetScriptSpec() const
  {
    return mScriptSpec;
  }

  void
  Abort()
  {
    MOZ_ASSERT(!mAborted);
    mAborted = true;
  }

  void
  Update()
  {
    AssertIsOnMainThread();
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);

    nsRefPtr<ServiceWorker> serviceWorker;
    nsresult rv = swm->CreateServiceWorkerForWindow(mWindow,
                                                    mScriptSpec,
                                                    mRegistration->mScope,
                                                    getter_AddRefs(serviceWorker));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      swm->RejectUpdatePromiseObservers(mRegistration, rv);
      return;
    }

    nsMainThreadPtrHandle<ServiceWorkerUpdateInstance> handle(
      new nsMainThreadPtrHolder<ServiceWorkerUpdateInstance>(this));
    // FIXME(nsm): Deal with error case (worker failed to download, redirect,
    // parse) in error handler patch.
    nsRefPtr<FinishSuccessfulFetchWorkerRunnable> r =
      new FinishSuccessfulFetchWorkerRunnable(serviceWorker->GetWorkerPrivate(), handle);

    AutoSafeJSContext cx;
    if (!r->Dispatch(cx)) {
      swm->RejectUpdatePromiseObservers(mRegistration, NS_ERROR_FAILURE);
    }
  }

  void
  FetchDone()
  {
    AssertIsOnMainThread();
    if (mAborted) {
      return;
    }

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    MOZ_ASSERT(swm);
    swm->FinishFetch(mRegistration, mWindow);
  }
};

NS_IMPL_ISUPPORTS0(ServiceWorkerUpdateInstance)

NS_IMETHODIMP
FinishFetchOnMainThreadRunnable::Run()
{
  AssertIsOnMainThread();
  mUpdateInstance->FetchDone();
  return NS_OK;
}

ServiceWorkerRegistration::ServiceWorkerRegistration(const nsACString& aScope)
  : mControlledDocumentsCounter(0),
    mScope(aScope),
    mPendingUninstall(false)
{ }

ServiceWorkerRegistration::~ServiceWorkerRegistration()
{
  MOZ_ASSERT(!IsControllingDocuments());
}

//////////////////////////
// ServiceWorkerManager //
//////////////////////////

NS_IMPL_ADDREF(ServiceWorkerManager)
NS_IMPL_RELEASE(ServiceWorkerManager)

NS_INTERFACE_MAP_BEGIN(ServiceWorkerManager)
  NS_INTERFACE_MAP_ENTRY(nsIServiceWorkerManager)
  if (aIID.Equals(NS_GET_IID(ServiceWorkerManager)))
    foundInterface = static_cast<nsIServiceWorkerManager*>(this);
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIServiceWorkerManager)
NS_INTERFACE_MAP_END

ServiceWorkerManager::ServiceWorkerManager()
{
}

ServiceWorkerManager::~ServiceWorkerManager()
{
  // The map will assert if it is not empty when destroyed.
  mDomainMap.EnumerateRead(CleanupServiceWorkerInformation, nullptr);
  mDomainMap.Clear();
}

/* static */ PLDHashOperator
ServiceWorkerManager::CleanupServiceWorkerInformation(const nsACString& aDomain,
                                                      ServiceWorkerDomainInfo* aDomainInfo,
                                                      void *aUnused)
{
  aDomainInfo->mServiceWorkerRegistrations.Clear();
  return PL_DHASH_NEXT;
}

/*
 * Implements the async aspects of the register algorithm.
 */
class RegisterRunnable : public nsRunnable
{
  nsCOMPtr<nsPIDOMWindow> mWindow;
  const nsCString mScope;
  nsCOMPtr<nsIURI> mScriptURI;
  nsRefPtr<Promise> mPromise;
public:
  RegisterRunnable(nsPIDOMWindow* aWindow, const nsCString aScope,
                   nsIURI* aScriptURI, Promise* aPromise)
    : mWindow(aWindow), mScope(aScope), mScriptURI(aScriptURI),
      mPromise(aPromise)
  { }

  NS_IMETHODIMP
  Run()
  {
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    nsRefPtr<ServiceWorkerManager::ServiceWorkerDomainInfo> domainInfo = swm->GetDomainInfo(mScriptURI);
    if (!domainInfo) {
      nsCString domain;
      nsresult rv = mScriptURI->GetHost(domain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mPromise->MaybeReject(rv);
        return NS_OK;
      }

      domainInfo = new ServiceWorkerManager::ServiceWorkerDomainInfo;
      swm->mDomainMap.Put(domain, domainInfo);
    }

    nsRefPtr<ServiceWorkerRegistration> registration =
      domainInfo->GetRegistration(mScope);

    nsCString spec;
    nsresult rv = mScriptURI->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeReject(rv);
      return NS_OK;
    }

    if (registration) {
      registration->mPendingUninstall = false;
      if (spec.Equals(registration->mScriptSpec)) {
        // There is an existing update in progress. Resolve with whatever it
        // results in.
        if (registration->HasUpdatePromise()) {
          registration->AddUpdatePromiseObserver(mPromise);
          return NS_OK;
        }

        // There is no update in progress and since SW updating is upto the UA,
        // we will not update right now. Simply resolve with whatever worker we
        // have.
        nsRefPtr<ServiceWorkerInfo> info = registration->Newest();
        if (info) {
          nsRefPtr<ServiceWorker> serviceWorker;
          nsresult rv =
            swm->CreateServiceWorkerForWindow(mWindow,
                                              info->GetScriptSpec(),
                                              registration->mScope,
                                              getter_AddRefs(serviceWorker));

          if (NS_WARN_IF(NS_FAILED(rv))) {
            return NS_ERROR_FAILURE;
          }

          mPromise->MaybeResolve(serviceWorker);
          return NS_OK;
        }
      }
    } else {
      registration = domainInfo->CreateNewRegistration(mScope);
    }

    registration->mScriptSpec = spec;

    rv = swm->Update(registration, mWindow);
    MOZ_ASSERT(registration->HasUpdatePromise());

    // We append this register() call's promise after calling Update() because
    // we don't want this one to be aborted when the others (existing updates
    // for the same registration) are aborted. Update() sets a new
    // UpdatePromise on the registration.
    registration->mUpdatePromise->AddPromise(mPromise);

    return rv;
  }
};

// If we return an error code here, the ServiceWorkerContainer will
// automatically reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::Register(nsIDOMWindow* aWindow, const nsAString& aScope,
                               const nsAString& aScriptURL,
                               nsISupports** aPromise)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  // XXXnsm Don't allow chrome callers for now, we don't support chrome
  // ServiceWorkers.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(window);
  ErrorResult result;
  nsRefPtr<Promise> promise = Promise::Create(sgo, result);
  if (result.Failed()) {
    return result.ErrorCode();
  }

  nsCOMPtr<nsIURI> documentURI = window->GetDocumentURI();
  if (!documentURI) {
    return NS_ERROR_FAILURE;
  }

  // Although the spec says that the same-origin checks should also be done
  // asynchronously, we do them in sync because the Promise created by the
  // WebIDL infrastructure due to a returned error will be resolved
  // asynchronously. We aren't making any internal state changes in these
  // checks, so ordering of multiple calls is not affected.

  nsresult rv;
  // FIXME(nsm): Bug 1003991. Disable check when devtools are open.
  if (!Preferences::GetBool("dom.serviceWorkers.testing.enabled")) {
    bool isHttps;
    rv = documentURI->SchemeIs("https", &isHttps);
    if (NS_FAILED(rv) || !isHttps) {
      NS_WARNING("ServiceWorker registration from insecure websites is not allowed.");
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  nsCOMPtr<nsIPrincipal> documentPrincipal;
  if (window->GetExtantDoc()) {
    documentPrincipal = window->GetExtantDoc()->NodePrincipal();
  } else {
    documentPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1");
  }

  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr, documentURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Data URLs are not allowed.
  rv = documentPrincipal->CheckMayLoad(scriptURI, true /* report */,
                                       false /* allowIfInheritsPrincipal */);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsIURI> scopeURI;
  rv = NS_NewURI(getter_AddRefs(scopeURI), aScope, nullptr, documentURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  rv = documentPrincipal->CheckMayLoad(scopeURI, true /* report */,
                                       false /* allowIfInheritsPrinciple */);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCString cleanedScope;
  rv = scopeURI->GetSpecIgnoringRef(cleanedScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsIRunnable> registerRunnable =
    new RegisterRunnable(window, cleanedScope, scriptURI, promise);
  promise.forget(aPromise);
  return NS_DispatchToCurrentThread(registerRunnable);
}

void
ServiceWorkerManager::RejectUpdatePromiseObservers(ServiceWorkerRegistration* aRegistration,
                                                   nsresult aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRegistration->HasUpdatePromise());
  aRegistration->mUpdatePromise->RejectAllPromises(aRv);
  aRegistration->mUpdatePromise = nullptr;
}

void
ServiceWorkerManager::RejectUpdatePromiseObservers(ServiceWorkerRegistration* aRegistration,
                                                   const ErrorEventInit& aErrorDesc)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRegistration->HasUpdatePromise());
  aRegistration->mUpdatePromise->RejectAllPromises(aErrorDesc);
  aRegistration->mUpdatePromise = nullptr;
}

/*
 * Update() does not return the Promise that the spec says it should. Callers
 * may access the registration's (new) Promise after calling this method.
 */
NS_IMETHODIMP
ServiceWorkerManager::Update(ServiceWorkerRegistration* aRegistration,
                             nsPIDOMWindow* aWindow)
{
  if (aRegistration->HasUpdatePromise()) {
    NS_WARNING("Already had a UpdatePromise. Aborting that one!");
    RejectUpdatePromiseObservers(aRegistration, NS_ERROR_DOM_ABORT_ERR);
    MOZ_ASSERT(aRegistration->mUpdateInstance);
    aRegistration->mUpdateInstance->Abort();
    aRegistration->mUpdateInstance = nullptr;
  }

  if (aRegistration->mInstallingWorker) {
    // FIXME(nsm): Terminate the worker. We still haven't figured out worker
    // instance ownership when not associated with a window, so let's wait on
    // this.
    // FIXME(nsm): We should be setting the state on the actual worker
    // instance.
    // FIXME(nsm): Fire "statechange" on installing worker instance.
    aRegistration->mInstallingWorker = nullptr;
    InvalidateServiceWorkerContainerWorker(aRegistration,
                                           WhichServiceWorker::INSTALLING_WORKER);
  }

  aRegistration->mUpdatePromise = new UpdatePromise();
  // FIXME(nsm): Bug 931249. If we don't need to fetch & install, resolve
  // promise and skip this.
  // FIXME(nsm): Bug 931249. Force cache update if > 1 day.

  aRegistration->mUpdateInstance =
    new ServiceWorkerUpdateInstance(aRegistration, aWindow);
  aRegistration->mUpdateInstance->Update();

  return NS_OK;
}

// If we return an error, ServiceWorkerContainer will reject the Promise.
NS_IMETHODIMP
ServiceWorkerManager::Unregister(nsIDOMWindow* aWindow, const nsAString& aScope,
                                 nsISupports** aPromise)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  // XXXnsm Don't allow chrome callers for now.
  MOZ_ASSERT(!nsContentUtils::IsCallerChrome());

  // FIXME(nsm): Same bug, different patch.

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

/* static */
already_AddRefed<ServiceWorkerManager>
ServiceWorkerManager::GetInstance()
{
  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  nsRefPtr<ServiceWorkerManager> concrete = do_QueryObject(swm);
  return concrete.forget();
}

void
ServiceWorkerManager::ResolveRegisterPromises(ServiceWorkerRegistration* aRegistration,
                                              const nsACString& aWorkerScriptSpec)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aRegistration->HasUpdatePromise());
  if (aRegistration->mUpdatePromise->IsRejected()) {
    aRegistration->mUpdatePromise = nullptr;
    return;
  }

  aRegistration->mUpdatePromise->ResolveAllPromises(aWorkerScriptSpec,
                                                    aRegistration->mScope);
  aRegistration->mUpdatePromise = nullptr;
}

// Must NS_Free() aString
void
ServiceWorkerManager::FinishFetch(ServiceWorkerRegistration* aRegistration,
                                  nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(aRegistration->HasUpdatePromise());
  MOZ_ASSERT(aRegistration->mUpdateInstance);
  aRegistration->mUpdateInstance = nullptr;
  if (aRegistration->mUpdatePromise->IsRejected()) {
    aRegistration->mUpdatePromise = nullptr;
    return;
  }

  // We have skipped Steps 3-8.3 of the Update algorithm here!

  nsRefPtr<ServiceWorker> worker;
  nsresult rv = CreateServiceWorkerForWindow(aWindow,
                                             aRegistration->mScriptSpec,
                                             aRegistration->mScope,
                                             getter_AddRefs(worker));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    RejectUpdatePromiseObservers(aRegistration, rv);
    return;
  }

  ResolveRegisterPromises(aRegistration, aRegistration->mScriptSpec);

  nsRefPtr<ServiceWorkerInfo> info = new ServiceWorkerInfo(aRegistration->mScriptSpec);
  Install(aRegistration, info);
}

void
ServiceWorkerManager::HandleError(JSContext* aCx,
                                  const nsACString& aScope,
                                  const nsAString& aWorkerURL,
                                  nsString aMessage,
                                  nsString aFilename,
                                  nsString aLine,
                                  uint32_t aLineNumber,
                                  uint32_t aColumnNumber,
                                  uint32_t aFlags)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aScope, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(uri);
  if (!domainInfo) {
    return;
  }

  nsCString scope;
  scope.Assign(aScope);
  nsRefPtr<ServiceWorkerRegistration> registration = domainInfo->GetRegistration(scope);
  MOZ_ASSERT(registration);

  RootedDictionary<ErrorEventInit> init(aCx);
  init.mMessage = aMessage;
  init.mFilename = aFilename;
  init.mLineno = aLineNumber;
  init.mColno = aColumnNumber;

  // If the worker was the one undergoing registration, we reject the promises,
  // otherwise we fire events on the ServiceWorker instances.

  // If there is an update in progress and the worker that errored is the same one
  // that is being updated, it is a sufficient test for 'this worker is being
  // registered'.
  // FIXME(nsm): Except the case where an update is found for a worker, in
  // which case we'll need some other association than simply the URL.
  if (registration->mUpdateInstance &&
      registration->mUpdateInstance->GetScriptSpec().Equals(NS_ConvertUTF16toUTF8(aWorkerURL))) {
    RejectUpdatePromiseObservers(registration, init);
    // We don't need to abort here since the worker has already run.
    registration->mUpdateInstance = nullptr;
  } else {
    // FIXME(nsm): Bug 983497 Fire 'error' on ServiceWorkerContainers.
  }
}

class FinishInstallRunnable MOZ_FINAL : public nsRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

public:
  explicit FinishInstallRunnable(
    const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
    : mRegistration(aRegistration)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->FinishInstall(mRegistration.get());
    return NS_OK;
  }
};

class FinishActivationRunnable : public nsRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

public:
  FinishActivationRunnable(const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
    : mRegistration(aRegistration)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsOnMainThread();

    // FinishActivate takes ownership of the passed info.
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->FinishActivate(mRegistration.get());
    return NS_OK;
  }
};

class CancelServiceWorkerInstallationRunnable MOZ_FINAL : public nsRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

public:
  explicit CancelServiceWorkerInstallationRunnable(
    const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
    : mRegistration(aRegistration)
  {
  }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();
    // FIXME(nsm): Change installing worker state to redundant.
    // FIXME(nsm): Fire statechange.
    mRegistration->mInstallingWorker = nullptr;
    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->InvalidateServiceWorkerContainerWorker(mRegistration,
                                                WhichServiceWorker::INSTALLING_WORKER);
    return NS_OK;
  }
};

/*
 * Used to handle InstallEvent::waitUntil() and proceed with installation.
 */
class FinishInstallHandler MOZ_FINAL : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

  virtual
  ~FinishInstallHandler()
  { }

public:
  explicit FinishInstallHandler(
    const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
    : mRegistration(aRegistration)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<FinishInstallRunnable> r = new FinishInstallRunnable(mRegistration);
    NS_DispatchToMainThread(r);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE
  {
    nsRefPtr<CancelServiceWorkerInstallationRunnable> r =
      new CancelServiceWorkerInstallationRunnable(mRegistration);
    NS_DispatchToMainThread(r);
  }
};

class FinishActivateHandler : public PromiseNativeHandler
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

public:
  FinishActivateHandler(const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
    : mRegistration(aRegistration)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  virtual
  ~FinishActivateHandler()
  { }

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE
  {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<FinishActivationRunnable> r = new FinishActivationRunnable(mRegistration);
    NS_DispatchToMainThread(r);
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) MOZ_OVERRIDE
  {
    // FIXME(nsm). Spec is undefined.
  }
};

/*
 * Fires 'install' event on the ServiceWorkerGlobalScope. Modifies busy count
 * since it fires the event. This is ok since there can't be nested
 * ServiceWorkers, so the parent thread -> worker thread requirement for
 * runnables is satisfied.
 */
class InstallEventRunnable MOZ_FINAL : public WorkerRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;
  nsCString mScope;

public:
  InstallEventRunnable(
    WorkerPrivate* aWorkerPrivate,
    const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
        mRegistration(aRegistration),
        mScope(aRegistration.get()->mScope) // copied for access on worker thread.
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchInstallEvent(aCx, aWorkerPrivate);
  }

private:
  bool
  DispatchInstallEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    InstallEventInit init;
    init.mBubbles = false;
    init.mCancelable = true;

    // FIXME(nsm): Bug 982787 pass previous active worker.

    nsRefPtr<EventTarget> target = aWorkerPrivate->GlobalScope();
    nsRefPtr<InstallEvent> event =
      InstallEvent::Constructor(target, NS_LITERAL_STRING("install"), init);

    event->SetTrusted(true);

    nsRefPtr<Promise> waitUntilPromise;

    nsresult rv = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);

    nsCOMPtr<nsIGlobalObject> sgo = aWorkerPrivate->GlobalScope();
    if (NS_SUCCEEDED(rv)) {
      waitUntilPromise = event->GetPromise();
      if (!waitUntilPromise) {
        ErrorResult rv;
        waitUntilPromise =
          Promise::Resolve(sgo,
                           aCx, JS::UndefinedHandleValue, rv);
      }
    } else {
      ErrorResult rv;
      // Continue with a canceled install.
      waitUntilPromise = Promise::Reject(sgo, aCx,
                                         JS::UndefinedHandleValue, rv);
    }

    nsRefPtr<FinishInstallHandler> handler =
      new FinishInstallHandler(mRegistration);
    waitUntilPromise->AppendNativeHandler(handler);
    return true;
  }
};

class ActivateEventRunnable : public WorkerRunnable
{
  nsMainThreadPtrHandle<ServiceWorkerRegistration> mRegistration;

public:
  ActivateEventRunnable(WorkerPrivate* aWorkerPrivate,
                        const nsMainThreadPtrHandle<ServiceWorkerRegistration>& aRegistration)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
        mRegistration(aRegistration)
  {
    MOZ_ASSERT(aWorkerPrivate);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    return DispatchActivateEvent(aCx, aWorkerPrivate);
  }

private:
  bool
  DispatchActivateEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate->IsServiceWorker());
    nsRefPtr<EventTarget> target = do_QueryObject(aWorkerPrivate->GlobalScope());

    // FIXME(nsm): Set activeWorker to the correct thing.
    EventInit init;
    init.mBubbles = false;
    init.mCancelable = true;
    nsRefPtr<InstallPhaseEvent> event =
      InstallPhaseEvent::Constructor(target, NS_LITERAL_STRING("activate"), init);

    event->SetTrusted(true);

    nsRefPtr<Promise> waitUntilPromise;

    nsresult rv = target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
    if (NS_SUCCEEDED(rv)) {
      waitUntilPromise = event->GetPromise();
      if (!waitUntilPromise) {
        ErrorResult rv;
        nsCOMPtr<nsIGlobalObject> global =
          do_QueryObject(aWorkerPrivate->GlobalScope());
        waitUntilPromise =
          Promise::Resolve(global,
                           aCx, JS::UndefinedHandleValue, rv);
      }
    } else {
      ErrorResult rv;
      nsCOMPtr<nsIGlobalObject> global =
        do_QueryObject(aWorkerPrivate->GlobalScope());
      // Continue with a canceled install.
      waitUntilPromise = Promise::Reject(global, aCx,
                                         JS::UndefinedHandleValue, rv);
    }

    nsRefPtr<FinishActivateHandler> handler = new FinishActivateHandler(mRegistration);
    waitUntilPromise->AppendNativeHandler(handler);
    return true;
  }
};

void
ServiceWorkerManager::Install(ServiceWorkerRegistration* aRegistration,
                              ServiceWorkerInfo* aServiceWorkerInfo)
{
  AssertIsOnMainThread();
  aRegistration->mInstallingWorker = aServiceWorkerInfo;
  MOZ_ASSERT(aRegistration->mInstallingWorker);
  InvalidateServiceWorkerContainerWorker(aRegistration,
                                         WhichServiceWorker::INSTALLING_WORKER);

  nsMainThreadPtrHandle<ServiceWorkerRegistration> handle(
    new nsMainThreadPtrHolder<ServiceWorkerRegistration>(aRegistration));

  nsRefPtr<ServiceWorker> serviceWorker;
  nsresult rv =
    CreateServiceWorker(aServiceWorkerInfo->GetScriptSpec(),
                        aRegistration->mScope,
                        getter_AddRefs(serviceWorker));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRegistration->mInstallingWorker = nullptr;
    // We don't need to invalidate here since the upper one will have done it.
    return;
  }

  nsRefPtr<InstallEventRunnable> r =
    new InstallEventRunnable(serviceWorker->GetWorkerPrivate(), handle);

  AutoSafeJSContext cx;
  r->Dispatch(cx);

  // When this function exits, although we've lost references to the ServiceWorker,
  // which means the underlying WorkerPrivate has no references, the worker
  // will stay alive due to the modified busy count until the install event has
  // been dispatched.
  // NOTE: The worker spec does not require Promises to keep a worker alive, so
  // the waitUntil() construct by itself will not keep a worker alive beyond
  // the event dispatch. On the other hand, networking, IDB and so on do keep
  // the worker alive, so the waitUntil() is only relevant if the Promise is
  // gated on those actions. I (nsm) am not sure if it is worth requiring
  // a special spec mention saying the install event should keep the worker
  // alive indefinitely purely on the basis of calling waitUntil(), since
  // a wait is likely to be required only when performing networking or storage
  // transactions in the first place.

  FireEventOnServiceWorkerContainers(aRegistration,
                                     NS_LITERAL_STRING("updatefound"));
}

class ActivationRunnable : public nsRunnable
{
  nsRefPtr<ServiceWorkerRegistration> mRegistration;
public:
  explicit ActivationRunnable(ServiceWorkerRegistration* aRegistration)
    : mRegistration(aRegistration)
  {
  }

  NS_IMETHODIMP
  Run() MOZ_OVERRIDE
  {
    if (mRegistration->mCurrentWorker) {
      // FIXME(nsm). Steps 3.1-3.4 of the algorithm.
    }

    mRegistration->mCurrentWorker = mRegistration->mWaitingWorker.forget();

    nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    swm->InvalidateServiceWorkerContainerWorker(mRegistration,
                                                WhichServiceWorker::ACTIVE_WORKER | WhichServiceWorker::WAITING_WORKER);

    // FIXME(nsm): Steps 7 of the algorithm.

    swm->FireEventOnServiceWorkerContainers(mRegistration,
                                            NS_LITERAL_STRING("controllerchange"));

    MOZ_ASSERT(mRegistration->mCurrentWorker);
    nsRefPtr<ServiceWorker> serviceWorker;
    nsresult rv =
      swm->CreateServiceWorker(mRegistration->mCurrentWorker->GetScriptSpec(),
                               mRegistration->mScope,
                               getter_AddRefs(serviceWorker));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsMainThreadPtrHandle<ServiceWorkerRegistration> handle(
      new nsMainThreadPtrHolder<ServiceWorkerRegistration>(mRegistration));

    nsRefPtr<ActivateEventRunnable> r =
      new ActivateEventRunnable(serviceWorker->GetWorkerPrivate(), handle);

    AutoSafeJSContext cx;
    if (!r->Dispatch(cx)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }
};

void
ServiceWorkerManager::FinishInstall(ServiceWorkerRegistration* aRegistration)
{
  AssertIsOnMainThread();

  if (aRegistration->mWaitingWorker) {
    // FIXME(nsm): Actually update the state of active ServiceWorker instances.
  }

  if (!aRegistration->mInstallingWorker) {
    // It is possible that while this run of [[Install]] was waiting for
    // the worker to handle the install event, some page called register() with
    // a different script leading to [[Update]] terminating the
    // installingWorker and setting it to null. The FinishInstallRunnable may
    // already have been dispatched, hence the check.
    return;
  }

  aRegistration->mWaitingWorker = aRegistration->mInstallingWorker.forget();
  MOZ_ASSERT(aRegistration->mWaitingWorker);
  InvalidateServiceWorkerContainerWorker(aRegistration,
                                         WhichServiceWorker::WAITING_WORKER | WhichServiceWorker::INSTALLING_WORKER);

  // FIXME(nsm): Actually update state of active ServiceWorker instances to
  // installed.
  // FIXME(nsm): Fire statechange on the instances.

  // FIXME(nsm): Handle replace().

  if (!aRegistration->IsControllingDocuments()) {
    nsRefPtr<ActivationRunnable> r =
      new ActivationRunnable(aRegistration);

    nsresult rv = NS_DispatchToMainThread(r);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // FIXME(nsm): Handle error.
      // How likely is this to happen and can we really do anything about it?
    }
  }
}

void
ServiceWorkerManager::FinishActivate(ServiceWorkerRegistration* aRegistration)
{
  // FIXME(nsm): Set aRegistration->mCurrentWorker state to activated.
  // Fire statechange.
}

NS_IMETHODIMP
ServiceWorkerManager::CreateServiceWorkerForWindow(nsPIDOMWindow* aWindow,
                                                   const nsACString& aScriptSpec,
                                                   const nsACString& aScope,
                                                   ServiceWorker** aServiceWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  RuntimeService* rs = RuntimeService::GetOrCreateService();
  nsRefPtr<ServiceWorker> serviceWorker;

  nsCOMPtr<nsIGlobalObject> sgo = do_QueryInterface(aWindow);

  AutoSafeJSContext cx;
  JS::Rooted<JSObject*> jsGlobal(cx, sgo->GetGlobalJSObject());
  JSAutoCompartment ac(cx, jsGlobal);

  GlobalObject global(cx, jsGlobal);
  nsresult rv = rs->CreateServiceWorker(global,
                                        NS_ConvertUTF8toUTF16(aScriptSpec),
                                        aScope,
                                        getter_AddRefs(serviceWorker));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker.forget(aServiceWorker);
  return rv;
}

already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerManager::GetServiceWorkerRegistration(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIDocument> document = aWindow->GetExtantDoc();
  return GetServiceWorkerRegistration(document);
}

already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerManager::GetServiceWorkerRegistration(nsIDocument* aDoc)
{
  nsCOMPtr<nsIURI> documentURI = aDoc->GetDocumentURI();
  return GetServiceWorkerRegistration(documentURI);
}

already_AddRefed<ServiceWorkerRegistration>
ServiceWorkerManager::GetServiceWorkerRegistration(nsIURI* aURI)
{
  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(aURI);
  if (!domainInfo) {
    return nullptr;
  }

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsCString scope = FindScopeForPath(domainInfo->mOrderedScopes, spec);
  if (scope.IsEmpty()) {
    return nullptr;
  }

  nsRefPtr<ServiceWorkerRegistration> registration;
  domainInfo->mServiceWorkerRegistrations.Get(scope, getter_AddRefs(registration));
  // ordered scopes and registrations better be in sync.
  MOZ_ASSERT(registration);

  return registration.forget();
}

namespace {
/*
 * Returns string without trailing '*'.
 */
void ScopeWithoutStar(const nsACString& aScope, nsACString& out)
{
  if (aScope.Last() == '*') {
    out.Assign(StringHead(aScope, aScope.Length() - 1));
    return;
  }

  out.Assign(aScope);
}
}; // anonymous namespace

/* static */ void
ServiceWorkerManager::AddScope(nsTArray<nsCString>& aList, const nsACString& aScope)
{
  for (uint32_t i = 0; i < aList.Length(); ++i) {
    const nsCString& current = aList[i];

    // Perfect match!
    if (aScope.Equals(current)) {
      return;
    }

    nsCString withoutStar;
    ScopeWithoutStar(current, withoutStar);
    // Edge case of match without '*'.
    // /foo should be sorted before /foo*.
    if (aScope.Equals(withoutStar)) {
      aList.InsertElementAt(i, aScope);
      return;
    }

    // /foo/bar* should be before /foo/*
    // Similarly /foo/b* is between the two.
    // But is /foo* categorically different?
    if (StringBeginsWith(aScope, withoutStar)) {
      // If the new scope is a pattern and the old one is a path, the new one
      // goes after.  This way Add(/foo) followed by Add(/foo*) ends up with
      // [/foo, /foo*].
      if (aScope.Last() == '*' &&
          withoutStar.Equals(current)) {
        aList.InsertElementAt(i+1, aScope);
      } else {
        aList.InsertElementAt(i, aScope);
      }
      return;
    }
  }

  aList.AppendElement(aScope);
}

// aPath can have a '*' at the end, but it is treated literally.
/* static */ nsCString
ServiceWorkerManager::FindScopeForPath(nsTArray<nsCString>& aList, const nsACString& aPath)
{
  MOZ_ASSERT(aPath.FindChar('*') == -1);

  nsCString match;

  for (uint32_t i = 0; i < aList.Length(); ++i) {
    const nsCString& current = aList[i];
    nsCString withoutStar;
    ScopeWithoutStar(current, withoutStar);
    if (StringBeginsWith(aPath, withoutStar)) {
      // If non-pattern match, then check equality.
      if (current.Last() == '*' ||
          aPath.Equals(current)) {
        match = current;
        break;
      }
    }
  }

  return match;
}

/* static */ void
ServiceWorkerManager::RemoveScope(nsTArray<nsCString>& aList, const nsACString& aScope)
{
  aList.RemoveElement(aScope);
}

already_AddRefed<ServiceWorkerManager::ServiceWorkerDomainInfo>
ServiceWorkerManager::GetDomainInfo(nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aDoc);
  nsCOMPtr<nsIURI> documentURI = aDoc->GetDocumentURI();
  return GetDomainInfo(documentURI);
}

already_AddRefed<ServiceWorkerManager::ServiceWorkerDomainInfo>
ServiceWorkerManager::GetDomainInfo(nsIURI* aURI)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aURI);

  nsCString domain;
  nsresult rv = aURI->GetHost(domain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsRefPtr<ServiceWorkerDomainInfo> domainInfo;
  mDomainMap.Get(domain, getter_AddRefs(domainInfo));
  return domainInfo.forget();
}

already_AddRefed<ServiceWorkerManager::ServiceWorkerDomainInfo>
ServiceWorkerManager::GetDomainInfo(const nsCString& aURL)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return GetDomainInfo(uri);
}

void
ServiceWorkerManager::MaybeStartControlling(nsIDocument* aDoc)
{
  AssertIsOnMainThread();
  if (!Preferences::GetBool("dom.serviceWorkers.enabled")) {
    return;
  }

  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(aDoc);
  if (!domainInfo) {
    return;
  }

  nsRefPtr<ServiceWorkerRegistration> registration =
    GetServiceWorkerRegistration(aDoc);
  if (registration && registration->mCurrentWorker) {
    MOZ_ASSERT(!domainInfo->mControlledDocuments.Contains(aDoc));
    registration->StartControllingADocument();
    // Use the already_AddRefed<> form of Put to avoid the addref-deref since
    // we don't need the registration pointer in this function anymore.
    domainInfo->mControlledDocuments.Put(aDoc, registration.forget());
  }
}

void
ServiceWorkerManager::MaybeStopControlling(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);
  if (!Preferences::GetBool("dom.serviceWorkers.enabled")) {
    return;
  }

  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(aDoc);
  if (!domainInfo) {
    return;
  }

  nsRefPtr<ServiceWorkerRegistration> registration;
  domainInfo->mControlledDocuments.Remove(aDoc, getter_AddRefs(registration));
  // A document which was uncontrolled does not maintain that state itself, so
  // it will always call MaybeStopControlling() even if there isn't an
  // associated registration. So this check is required.
  if (registration) {
    registration->StopControllingADocument();
  }
}

NS_IMETHODIMP
ServiceWorkerManager::GetScopeForUrl(const nsAString& aUrl, nsAString& aScope)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ServiceWorkerRegistration> r = GetServiceWorkerRegistration(uri);
  if (!r) {
      return NS_ERROR_FAILURE;
  }

  aScope = NS_ConvertUTF8toUTF16(r->mScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::AddContainerEventListener(nsIURI* aDocumentURI, nsIDOMEventTarget* aListener)
{
  MOZ_ASSERT(aDocumentURI);
  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(aDocumentURI);
  if (!domainInfo) {
    nsCString domain;
    nsresult rv = aDocumentURI->GetHost(domain);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    domainInfo = new ServiceWorkerDomainInfo;
    mDomainMap.Put(domain, domainInfo);
  }

  MOZ_ASSERT(domainInfo);

  ServiceWorkerContainer* container = static_cast<ServiceWorkerContainer*>(aListener);
  domainInfo->mServiceWorkerContainers.AppendElement(container);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::RemoveContainerEventListener(nsIURI* aDocumentURI, nsIDOMEventTarget* aListener)
{
  MOZ_ASSERT(aDocumentURI);
  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(aDocumentURI);
  if (!domainInfo) {
    return NS_OK;
  }

  ServiceWorkerContainer* container = static_cast<ServiceWorkerContainer*>(aListener);
  domainInfo->mServiceWorkerContainers.RemoveElement(container);
  return NS_OK;
}

void
ServiceWorkerManager::FireEventOnServiceWorkerContainers(
  ServiceWorkerRegistration* aRegistration,
  const nsAString& aName)
{
  AssertIsOnMainThread();
  nsRefPtr<ServiceWorkerDomainInfo> domainInfo =
    GetDomainInfo(aRegistration->mScriptSpec);

  if (domainInfo) {
    nsTObserverArray<ServiceWorkerContainer*>::ForwardIterator it(domainInfo->mServiceWorkerContainers);
    while (it.HasMore()) {
      nsRefPtr<ServiceWorkerContainer> target = it.GetNext();
      nsIURI* targetURI = target->GetDocumentURI();
      if (!targetURI) {
        NS_WARNING("Controlled domain cannot have page with null URI!");
        continue;
      }

      nsCString path;
      nsresult rv = targetURI->GetSpec(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      nsCString scope = FindScopeForPath(domainInfo->mOrderedScopes, path);
      if (scope.IsEmpty() ||
          !scope.Equals(aRegistration->mScope)) {
        continue;
      }

      target->DispatchTrustedEvent(aName);
    }
  }
}

/*
 * This is used for installing, waiting and active, and uses the registration
 * most specifically matching the current scope.
 */
NS_IMETHODIMP
ServiceWorkerManager::GetServiceWorkerForWindow(nsIDOMWindow* aWindow,
                                                WhichServiceWorker aWhichWorker,
                                                nsISupports** aServiceWorker)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  MOZ_ASSERT(window);

  nsRefPtr<ServiceWorkerRegistration> registration =
    GetServiceWorkerRegistration(window);

  if (!registration) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ServiceWorkerInfo> info;
  if (aWhichWorker == WhichServiceWorker::INSTALLING_WORKER) {
    info = registration->mInstallingWorker;
  } else if (aWhichWorker == WhichServiceWorker::WAITING_WORKER) {
    info = registration->mWaitingWorker;
  } else if (aWhichWorker == WhichServiceWorker::ACTIVE_WORKER) {
    info = registration->mCurrentWorker;
  } else {
    MOZ_CRASH("Invalid worker type");
  }

  if (!info) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsRefPtr<ServiceWorker> serviceWorker;
  nsresult rv = CreateServiceWorkerForWindow(window,
                                             info->GetScriptSpec(),
                                             registration->mScope,
                                             getter_AddRefs(serviceWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

/*
 * The .controller is for the registration associated with the document when
 * the document was loaded.
 */
NS_IMETHODIMP
ServiceWorkerManager::GetDocumentController(nsIDOMWindow* aWindow, nsISupports** aServiceWorker)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  MOZ_ASSERT(window);
  if (!window || !window->GetExtantDoc()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();

  nsRefPtr<ServiceWorkerDomainInfo> domainInfo = GetDomainInfo(doc);
  if (!domainInfo) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<ServiceWorkerRegistration> registration;
  if (!domainInfo->mControlledDocuments.Get(doc, getter_AddRefs(registration))) {
    return NS_ERROR_FAILURE;
  }

  // If the document is controlled, the current worker MUST be non-null.
  MOZ_ASSERT(registration->mCurrentWorker);

  nsRefPtr<ServiceWorker> serviceWorker;
  nsresult rv = CreateServiceWorkerForWindow(window,
                                             registration->mCurrentWorker->GetScriptSpec(),
                                             registration->mScope,
                                             getter_AddRefs(serviceWorker));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerManager::GetInstalling(nsIDOMWindow* aWindow,
                                    nsISupports** aServiceWorker)
{
  return GetServiceWorkerForWindow(aWindow, WhichServiceWorker::INSTALLING_WORKER,
                                   aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetWaiting(nsIDOMWindow* aWindow,
                                 nsISupports** aServiceWorker)
{
  return GetServiceWorkerForWindow(aWindow, WhichServiceWorker::WAITING_WORKER,
                                   aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::GetActive(nsIDOMWindow* aWindow, nsISupports** aServiceWorker)
{
  return GetServiceWorkerForWindow(aWindow, WhichServiceWorker::ACTIVE_WORKER,
                                   aServiceWorker);
}

NS_IMETHODIMP
ServiceWorkerManager::CreateServiceWorker(const nsACString& aScriptSpec,
                                          const nsACString& aScope,
                                          ServiceWorker** aServiceWorker)
{
  AssertIsOnMainThread();

  WorkerPrivate::LoadInfo info;
  nsresult rv = NS_NewURI(getter_AddRefs(info.mBaseURI), aScriptSpec, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  info.mResolvedScriptURI = info.mBaseURI;

  rv = info.mBaseURI->GetHost(info.mDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // FIXME(nsm): Create correct principal based on app-ness.
  // Would it make sense to store the nsIPrincipal of the first register() in
  // the ServiceWorkerRegistration and use that?
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  rv = ssm->GetNoAppCodebasePrincipal(info.mBaseURI, getter_AddRefs(info.mPrincipal));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoSafeJSContext cx;

  nsRefPtr<ServiceWorker> serviceWorker;
  RuntimeService* rs = RuntimeService::GetService();
  if (!rs) {
    return NS_ERROR_FAILURE;
  }

  rv = rs->CreateServiceWorkerFromLoadInfo(cx, info, NS_ConvertUTF8toUTF16(aScriptSpec), aScope,
                                           getter_AddRefs(serviceWorker));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  serviceWorker.forget(aServiceWorker);
  return NS_OK;
}

void
ServiceWorkerManager::InvalidateServiceWorkerContainerWorker(ServiceWorkerRegistration* aRegistration,
                                                             WhichServiceWorker aWhichOnes)
{
  AssertIsOnMainThread();
  nsRefPtr<ServiceWorkerDomainInfo> domainInfo =
    GetDomainInfo(aRegistration->mScriptSpec);

  if (domainInfo) {
    nsTObserverArray<ServiceWorkerContainer*>::ForwardIterator it(domainInfo->mServiceWorkerContainers);
    while (it.HasMore()) {
      nsRefPtr<ServiceWorkerContainer> target = it.GetNext();

      nsIURI* targetURI = target->GetDocumentURI();
      nsCString path;
      nsresult rv = targetURI->GetSpec(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      nsCString scope = FindScopeForPath(domainInfo->mOrderedScopes, path);
      if (scope.IsEmpty() ||
          !scope.Equals(aRegistration->mScope)) {
        continue;
      }

      target->InvalidateWorkerReference(aWhichOnes);
    }
  }
}

END_WORKERS_NAMESPACE
