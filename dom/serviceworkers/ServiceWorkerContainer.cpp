/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerContainer.h"

#include "nsContentPolicyUtils.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIServiceWorkerManager.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "mozilla/Services.h"

#include "nsCycleCollectionParticipant.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/LoadInfo.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerContainerBinding.h"

#include "ServiceWorker.h"
#include "ServiceWorkerContainerImpl.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ServiceWorkerContainer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ServiceWorkerContainer, DOMEventTargetHelper,
                                   mControllerWorker, mReadyPromise)

/* static */ bool
ServiceWorkerContainer::IsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());

  JS::Rooted<JSObject*> global(aCx, aGlobal);
  nsCOMPtr<nsPIDOMWindowInner> window = Navigator::GetWindowFromGlobal(global);
  if (!window) {
    return false;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc || nsContentUtils::IsInPrivateBrowsing(doc)) {
    return false;
  }

  return DOMPrefs::ServiceWorkersEnabled();
}

// static
already_AddRefed<ServiceWorkerContainer>
ServiceWorkerContainer::Create(nsIGlobalObject* aGlobal)
{
  RefPtr<Inner> inner = new ServiceWorkerContainerImpl();
  RefPtr<ServiceWorkerContainer> ref =
    new ServiceWorkerContainer(aGlobal, inner.forget());
  return ref.forget();
}

ServiceWorkerContainer::ServiceWorkerContainer(nsIGlobalObject* aGlobal,
                                               already_AddRefed<ServiceWorkerContainer::Inner> aInner)
  : DOMEventTargetHelper(aGlobal)
  , mInner(aInner)
{
  mInner->AddContainer(this);
  Maybe<ServiceWorkerDescriptor> controller = aGlobal->GetController();
  if (controller.isSome()) {
    mControllerWorker = aGlobal->GetOrCreateServiceWorker(controller.ref());
  }
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
  mInner->RemoveContainer(this);
}

void
ServiceWorkerContainer::DisconnectFromOwner()
{
  mControllerWorker = nullptr;
  mReadyPromise = nullptr;
  DOMEventTargetHelper::DisconnectFromOwner();
}

void
ServiceWorkerContainer::ControllerChanged(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = GetParentObject();
  if (!go) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mControllerWorker = go->GetOrCreateServiceWorker(go->GetController().ref());
  aRv = DispatchTrustedEvent(NS_LITERAL_STRING("controllerchange"));
}

JSObject*
ServiceWorkerContainer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ServiceWorkerContainer_Binding::Wrap(aCx, this, aGivenProto);
}

namespace {

already_AddRefed<nsIURI>
GetBaseURIFromGlobal(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  // It would be nice not to require a window here, but right
  // now we don't have a great way to get the base URL just
  // from the nsIGlobalObject.
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
  if (!window) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = doc->GetDocBaseURI();
  if (!baseURI) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return baseURI.forget();
}

// This function implements parts of the step 3 of the following algorithm:
// https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-secure
static bool
IsFromAuthenticatedOrigin(nsIDocument* aDoc)
{
  MOZ_ASSERT(aDoc);
  nsCOMPtr<nsIDocument> doc(aDoc);
  nsCOMPtr<nsIContentSecurityManager> csm = do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  if (NS_WARN_IF(!csm)) {
    return false;
  }

  while (doc && !nsContentUtils::IsChromeDoc(doc)) {
    bool trustworthyOrigin = false;

    // The origin of the document may be different from the document URI
    // itself.  Check the principal, not the document URI itself.
    nsCOMPtr<nsIPrincipal> documentPrincipal = doc->NodePrincipal();

    // The check for IsChromeDoc() above should mean we never see a system
    // principal inside the loop.
    MOZ_ASSERT(!nsContentUtils::IsSystemPrincipal(documentPrincipal));

    csm->IsOriginPotentiallyTrustworthy(documentPrincipal, &trustworthyOrigin);
    if (!trustworthyOrigin) {
      return false;
    }

    doc = doc->GetParentDocument();
  }
  return true;
}

} // anonymous namespace

already_AddRefed<Promise>
ServiceWorkerContainer::Register(const nsAString& aScriptURL,
                                 const RegistrationOptions& aOptions,
                                 ErrorResult& aRv)
{
  // Note, we can't use GetGlobalIfValid() from the start here.  If we
  // hit a storage failure we want to log a message with the final
  // scope string we put together below.
  nsIGlobalObject* global = GetParentObject();
  if (!global) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURIFromGlobal(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> scriptURI;
  rv = NS_NewURI(getter_AddRefs(scriptURI), aScriptURL, nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aScriptURL);
    return nullptr;
  }

  // In ServiceWorkerContainer.register() the scope argument is parsed against
  // different base URLs depending on whether it was passed or not.
  nsCOMPtr<nsIURI> scopeURI;

  // Step 4. If none passed, parse against script's URL
  if (!aOptions.mScope.WasPassed()) {
    NS_NAMED_LITERAL_STRING(defaultScope, "./");
    rv = NS_NewURI(getter_AddRefs(scopeURI), defaultScope,
                   nullptr, scriptURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsAutoCString spec;
      scriptURI->GetSpec(spec);
      NS_ConvertUTF8toUTF16 wSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(defaultScope, wSpec);
      return nullptr;
    }
  } else {
    // Step 5. Parse against entry settings object's base URL.
    rv = NS_NewURI(getter_AddRefs(scopeURI), aOptions.mScope.Value(),
                   nullptr, baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsIURI* uri = baseURI ? baseURI : scriptURI;
      nsAutoCString spec;
      uri->GetSpec(spec);
      NS_ConvertUTF8toUTF16 wSpec(spec);
      aRv.ThrowTypeError<MSG_INVALID_SCOPE>(aOptions.mScope.Value(), wSpec);
      return nullptr;
    }
  }

  // Strip the any ref from both the script and scope URLs.
  nsCOMPtr<nsIURI> cloneWithoutRef;
  aRv = scriptURI->CloneIgnoringRef(getter_AddRefs(cloneWithoutRef));
  if (aRv.Failed()) {
    return nullptr;
  }
  scriptURI = cloneWithoutRef.forget();

  aRv = scopeURI->CloneIgnoringRef(getter_AddRefs(cloneWithoutRef));
  if (aRv.Failed()) {
    return nullptr;
  }
  scopeURI = cloneWithoutRef.forget();

  aRv = ServiceWorkerScopeAndScriptAreValid(clientInfo.ref(),
                                            scopeURI,
                                            scriptURI);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  if (!window) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Next, implement a lame version of [SecureContext] with an
  // exception based on a pref or devtools option.
  // TODO: This logic should be moved to a webidl [Func]. See bug 1455078.
  nsCOMPtr<nsPIDOMWindowOuter> outerWindow = window->GetOuterWindow();
  bool serviceWorkersTestingEnabled =
    outerWindow->GetServiceWorkersTestingEnabled();

  bool authenticatedOrigin;
  if (DOMPrefs::ServiceWorkersTestingEnabled() ||
      serviceWorkersTestingEnabled) {
    authenticatedOrigin = true;
  } else {
    authenticatedOrigin = IsFromAuthenticatedOrigin(doc);
  }

  if (!authenticatedOrigin) {
    NS_WARNING("ServiceWorker registration from insecure websites is not allowed.");
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // The next section of code executes an NS_CheckContentLoadPolicy()
  // check.  This is necessary to enforce the CSP of the calling client.
  // Currently this requires an nsIDocument.  Once bug 965637 lands we
  // should try to move this into ServiceWorkerScopeAndScriptAreValid()
  // using the ClientInfo instead of doing a window-specific check here.
  // See bug 1455077 for further investigation.
  nsCOMPtr<nsILoadInfo> secCheckLoadInfo =
    new mozilla::net::LoadInfo(doc->NodePrincipal(), // loading principal
                               doc->NodePrincipal(), // triggering principal
                               doc,                  // loading node
                               nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
                               nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER);

  // Check content policy.
  int16_t decision = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(scriptURI,
                                 secCheckLoadInfo,
                                 NS_LITERAL_CSTRING("application/javascript"),
                                 &decision);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;

  }
  if (NS_WARN_IF(decision != nsIContentPolicy::ACCEPT)) {
    aRv.Throw(NS_ERROR_CONTENT_BLOCKED);
    return nullptr;
  }

  // Get the string representation for both the script and scope since
  // we sanitized them above.
  nsCString cleanedScopeURL;
  aRv = scopeURI->GetSpec(cleanedScopeURL);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCString cleanedScriptURL;
  aRv = scriptURI->GetSpec(cleanedScriptURL);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Verify that the global is valid and has permission to store
  // data.  We perform this late so that we can report the final
  // scope URL in any error message.
  Unused << GetGlobalIfValid(aRv, [&](nsIDocument* aDoc) {
    NS_ConvertUTF8toUTF16 reportScope(cleanedScopeURL);
    const char16_t* param[] = { reportScope.get() };
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Service Workers"),
                                    aDoc, nsContentUtils::eDOM_PROPERTIES,
                                    "ServiceWorkerRegisterStorageError",
                                    param, 1);
  });

  window->NoteCalledRegisterForServiceWorkerScope(cleanedScopeURL);

  RefPtr<Promise> outer = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  mInner->Register(
    clientInfo.ref(), cleanedScopeURL, cleanedScriptURL, aOptions.mUpdateViaCache,
    [self, outer] (const ServiceWorkerRegistrationDescriptor& aDesc) {
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      RefPtr<ServiceWorkerRegistration> reg =
        global->GetOrCreateServiceWorkerRegistration(aDesc);
      outer->MaybeResolve(reg);
    }, [outer] (ErrorResult& aRv) {
      outer->MaybeReject(aRv);
    });

  return outer.forget();
}

already_AddRefed<ServiceWorker>
ServiceWorkerContainer::GetController()
{
  RefPtr<ServiceWorker> ref = mControllerWorker;
  return ref.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetRegistrations(ErrorResult& aRv)
{
  nsIGlobalObject* global = GetGlobalIfValid(aRv, [](nsIDocument* aDoc) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Service Workers"), aDoc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "ServiceWorkerGetRegistrationStorageError");
  });
  if (aRv.Failed()) {
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<Promise> outer = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  mInner->GetRegistrations(clientInfo.ref(),
    [self, outer] (const nsTArray<ServiceWorkerRegistrationDescriptor>& aDescList) {
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      nsTArray<RefPtr<ServiceWorkerRegistration>> regList;
      for (auto& desc : aDescList) {
        RefPtr<ServiceWorkerRegistration> reg =
          global->GetOrCreateServiceWorkerRegistration(desc);
        if (reg) {
          regList.AppendElement(std::move(reg));
        }
      }
      outer->MaybeResolve(regList);
    }, [self, outer] (ErrorResult& aRv) {
      outer->MaybeReject(aRv);
    });

  return outer.forget();
}

already_AddRefed<Promise>
ServiceWorkerContainer::GetRegistration(const nsAString& aURL,
                                        ErrorResult& aRv)
{
  nsIGlobalObject* global = GetGlobalIfValid(aRv, [](nsIDocument* aDoc) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("Service Workers"), aDoc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "ServiceWorkerGetRegistrationStorageError");
  });
  if (aRv.Failed()) {
    return nullptr;
  }

  Maybe<ClientInfo> clientInfo = global->GetClientInfo();
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIURI> baseURI = GetBaseURIFromGlobal(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  aRv = NS_NewURI(getter_AddRefs(uri), aURL, nullptr, baseURI);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCString spec;
  aRv = uri->GetSpec(spec);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Promise> outer = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;

  mInner->GetRegistration(clientInfo.ref(), spec,
    [self, outer] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      RefPtr<ServiceWorkerRegistration> reg =
        global->GetOrCreateServiceWorkerRegistration(aDescriptor);
      outer->MaybeResolve(reg);
    }, [self, outer] (ErrorResult& aRv) {
      if (!aRv.Failed()) {
        Unused << self->GetGlobalIfValid(aRv);
        if (!aRv.Failed()) {
          outer->MaybeResolveWithUndefined();
          return;
        }
      }
      outer->MaybeReject(aRv);
    });

  return outer.forget();
}

Promise*
ServiceWorkerContainer::GetReady(ErrorResult& aRv)
{
  if (mReadyPromise) {
    return mReadyPromise;
  }

  nsIGlobalObject* global = GetGlobalIfValid(aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_DIAGNOSTIC_ASSERT(global);

  Maybe<ClientInfo> clientInfo(global->GetClientInfo());
  if (clientInfo.isNothing()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mReadyPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ServiceWorkerContainer> self = this;
  RefPtr<Promise> outer = mReadyPromise;

  mInner->GetReady(clientInfo.ref(),
    [self, outer] (const ServiceWorkerRegistrationDescriptor& aDescriptor) {
      ErrorResult rv;
      nsIGlobalObject* global = self->GetGlobalIfValid(rv);
      if (rv.Failed()) {
        outer->MaybeReject(rv);
        return;
      }
      RefPtr<ServiceWorkerRegistration> reg =
        global->GetOrCreateServiceWorkerRegistration(aDescriptor);
      NS_ENSURE_TRUE_VOID(reg);
      outer->MaybeResolve(reg);
    }, [self, outer] (ErrorResult& aRv) {
      outer->MaybeReject(aRv);
    });

  return mReadyPromise;
}

// Testing only.
void
ServiceWorkerContainer::GetScopeForUrl(const nsAString& aUrl,
                                       nsString& aScope,
                                       ErrorResult& aRv)
{
  nsCOMPtr<nsIServiceWorkerManager> swm = mozilla::services::GetServiceWorkerManager();
  if (!swm) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aRv = swm->GetScopeForUrl(doc->NodePrincipal(),
                            aUrl, aScope);
}

nsIGlobalObject*
ServiceWorkerContainer::GetGlobalIfValid(ErrorResult& aRv,
                                         const std::function<void(nsIDocument*)>&& aStorageFailureCB) const
{
  // For now we require a window since ServiceWorkerContainer is
  // not exposed on worker globals yet.  The main thing we need
  // to fix here to support that is the storage access check via
  // the nsIGlobalObject.
  nsPIDOMWindowInner* window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Don't allow a service worker to access service worker registrations
  // from a window with storage disabled.  If these windows can access
  // the registration it increases the chance they can bypass the storage
  // block via postMessage(), etc.
  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (NS_WARN_IF(storageAllowed != nsContentUtils::StorageAccess::eAllow)) {
    if (aStorageFailureCB) {
      aStorageFailureCB(doc);
    }
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Don't allow service workers when the document is chrome.
  if (NS_WARN_IF(nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  return window->AsGlobal();
}

} // namespace dom
} // namespace mozilla
