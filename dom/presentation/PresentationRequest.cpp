/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationRequest.h"

#include "AvailabilityCollection.h"
#include "ControllerConnectionCollection.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/PresentationRequestBinding.h"
#include "mozilla/dom/PresentationConnectionAvailableEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Move.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentSecurityManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIPresentationService.h"
#include "nsIURI.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"
#include "Presentation.h"
#include "PresentationAvailability.h"
#include "PresentationCallbacks.h"
#include "PresentationLog.h"
#include "PresentationTransportBuilderConstructor.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ADDREF_INHERITED(PresentationRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationRequest, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

static nsresult
GetAbsoluteURL(const nsAString& aUrl,
               nsIURI* aBaseUri,
               nsIDocument* aDocument,
               nsAString& aAbsoluteUrl)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          aUrl,
                          aDocument ? aDocument->GetDocumentCharacterSet().get()
                                    : nullptr,
                          aBaseUri);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString spec;
  uri->GetSpec(spec);

  aAbsoluteUrl = NS_ConvertUTF8toUTF16(spec);

  return NS_OK;
}

/* static */ already_AddRefed<PresentationRequest>
PresentationRequest::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aUrl,
                                 ErrorResult& aRv)
{
  Sequence<nsString> urls;
  urls.AppendElement(aUrl, fallible);
  return Constructor(aGlobal, urls, aRv);
}

/* static */ already_AddRefed<PresentationRequest>
PresentationRequest::Constructor(const GlobalObject& aGlobal,
                                 const Sequence<nsString>& aUrls,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (aUrls.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  // Resolve relative URL to absolute URL
  nsCOMPtr<nsIURI> baseUri = window->GetDocBaseURI();
  nsTArray<nsString> urls;
  for (const auto& url : aUrls) {
    nsAutoString absoluteUrl;
    nsresult rv =
      GetAbsoluteURL(url, baseUri, window->GetExtantDoc(), absoluteUrl);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }

    urls.AppendElement(absoluteUrl);
  }

  RefPtr<PresentationRequest> request =
    new PresentationRequest(window, Move(urls));
  return NS_WARN_IF(!request->Init()) ? nullptr : request.forget();
}

PresentationRequest::PresentationRequest(nsPIDOMWindowInner* aWindow,
                                         nsTArray<nsString>&& aUrls)
  : DOMEventTargetHelper(aWindow)
  , mUrls(Move(aUrls))
{
}

PresentationRequest::~PresentationRequest()
{
}

bool
PresentationRequest::Init()
{
  return true;
}

/* virtual */ JSObject*
PresentationRequest::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return PresentationRequestBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
PresentationRequest::Start(ErrorResult& aRv)
{
  return StartWithDevice(NullString(), aRv);
}

already_AddRefed<Promise>
PresentationRequest::StartWithDevice(const nsAString& aDeviceId,
                                     ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Get the origin.
  nsAutoString origin;
  nsresult rv = nsContentUtils::GetUTFOrigin(global->PrincipalOrNull(), origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (IsProhibitMixedSecurityContexts(doc) &&
      !IsAllURLAuthenticated()) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  RefPtr<Navigator> navigator =
    nsGlobalWindow::Cast(GetOwner())->GetNavigator(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Presentation> presentation = navigator->GetPresentation(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (presentation->IsStartSessionUnsettled()) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  // Generate a session ID.
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1");
  if(NS_WARN_IF(!uuidgen)) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  nsID uuid;
  uuidgen->GenerateUUIDInPlace(&uuid);
  char buffer[NSID_LENGTH];
  uuid.ToProvidedString(buffer);
  nsAutoString id;
  CopyASCIItoUTF16(buffer, id);

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return promise.forget();
  }

  presentation->SetStartSessionUnsettled(true);

  // Get xul:browser element in parent process or nsWindowRoot object in child
  // process. If it's in child process, the corresponding xul:browser element
  // will be obtained at PresentationRequestParent::DoRequest in its parent
  // process.
  nsCOMPtr<nsIDOMEventTarget> handler =
    do_QueryInterface(GetOwner()->GetChromeEventHandler());
  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsCOMPtr<nsIPresentationServiceCallback> callback =
    new PresentationRequesterCallback(this, id, promise);
  nsCOMPtr<nsIPresentationTransportBuilderConstructor> constructor =
    PresentationTransportBuilderConstructor::Create();
  rv = service->StartSession(mUrls,
                             id,
                             origin,
                             aDeviceId,
                             GetOwner()->WindowID(),
                             handler,
                             principal,
                             callback,
                             constructor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    NotifyPromiseSettled();
  }

  return promise.forget();
}

already_AddRefed<Promise>
PresentationRequest::Reconnect(const nsAString& aPresentationId,
                               ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (IsProhibitMixedSecurityContexts(doc) &&
      !IsAllURLAuthenticated()) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  nsString presentationId = nsString(aPresentationId);
  nsCOMPtr<nsIRunnable> r =
    NewRunnableMethod<nsString, RefPtr<Promise>>(
      this,
      &PresentationRequest::FindOrCreatePresentationConnection,
      presentationId,
      promise);

  if (NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(r)))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

void
PresentationRequest::FindOrCreatePresentationConnection(
  const nsAString& aPresentationId,
  Promise* aPromise)
{
  MOZ_ASSERT(aPromise);

  if (NS_WARN_IF(!GetOwner())) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  RefPtr<PresentationConnection> connection =
    ControllerConnectionCollection::GetSingleton()->FindConnection(
      GetOwner()->WindowID(),
      aPresentationId,
      nsIPresentationService::ROLE_CONTROLLER);

  if (connection) {
    nsAutoString url;
    connection->GetUrl(url);
    if (mUrls.Contains(url)) {
      switch (connection->State()) {
        case PresentationConnectionState::Closed:
          // We found the matched connection.
          break;
        case PresentationConnectionState::Connecting:
        case PresentationConnectionState::Connected:
          aPromise->MaybeResolve(connection);
          return;
        case PresentationConnectionState::Terminated:
          // A terminated connection cannot be reused.
          connection = nullptr;
          break;
        default:
          MOZ_CRASH("Unknown presentation session state.");
          return;
      }
    } else {
      connection = nullptr;
    }
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsCOMPtr<nsIPresentationServiceCallback> callback =
    new PresentationReconnectCallback(this,
                                      aPresentationId,
                                      aPromise,
                                      connection);

  nsresult rv =
    service->ReconnectSession(mUrls,
                              aPresentationId,
                              nsIPresentationService::ROLE_CONTROLLER,
                              callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }
}

already_AddRefed<Promise>
PresentationRequest::GetAvailability(ErrorResult& aRv)
{
  PRES_DEBUG("%s\n", __func__);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (IsProhibitMixedSecurityContexts(doc) &&
      !IsAllURLAuthenticated()) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  FindOrCreatePresentationAvailability(promise);

  return promise.forget();
}

void
PresentationRequest::FindOrCreatePresentationAvailability(RefPtr<Promise>& aPromise)
{
  MOZ_ASSERT(aPromise);

  if (NS_WARN_IF(!GetOwner())) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  AvailabilityCollection* collection = AvailabilityCollection::GetSingleton();
  if (NS_WARN_IF(!collection)) {
    aPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  RefPtr<PresentationAvailability> availability =
    collection->Find(GetOwner()->WindowID(), mUrls);

  if (!availability) {
    availability = PresentationAvailability::Create(GetOwner(), mUrls, aPromise);
  } else {
    PRES_DEBUG(">resolve with same object\n");

    // Fetching cached available devices is asynchronous in our implementation,
    // we need to ensure the promise is resolved in order.
    if (availability->IsCachedValueReady()) {
      aPromise->MaybeResolve(availability);
      return;
    }

    availability->EnqueuePromise(aPromise);
  }

  if (!availability) {
    aPromise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }
}

nsresult
PresentationRequest::DispatchConnectionAvailableEvent(PresentationConnection* aConnection)
{
  PresentationConnectionAvailableEventInit init;
  init.mConnection = aConnection;

  RefPtr<PresentationConnectionAvailableEvent> event =
    PresentationConnectionAvailableEvent::Constructor(this,
                                                      NS_LITERAL_STRING("connectionavailable"),
                                                      init);
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_FAILURE;
  }
  event->SetTrusted(true);

  RefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  return asyncDispatcher->PostDOMEvent();
}

void
PresentationRequest::NotifyPromiseSettled()
{
  PRES_DEBUG("%s\n", __func__);

  if (!GetOwner()) {
    return;
  }

  ErrorResult rv;
  RefPtr<Navigator> navigator =
    nsGlobalWindow::Cast(GetOwner())->GetNavigator(rv);
  if (!navigator) {
    return;
  }

  RefPtr<Presentation> presentation = navigator->GetPresentation(rv);

  if (presentation) {
    presentation->SetStartSessionUnsettled(false);
  }
}

bool
PresentationRequest::IsProhibitMixedSecurityContexts(nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  if (nsContentUtils::IsChromeDoc(aDocument)) {
    return true;
  }

  nsCOMPtr<nsIDocument> doc = aDocument;
  while (doc && !nsContentUtils::IsChromeDoc(doc)) {
    if (nsContentUtils::HttpsStateIsModern(doc)) {
      return true;
    }

    doc = doc->GetParentDocument();
  }

  return false;
}

bool
PresentationRequest::IsPrioriAuthenticatedURL(const nsAString& aUrl)
{
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), aUrl))) {
    return false;
  }

  nsAutoCString scheme;
  nsresult rv = uri->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (scheme.EqualsLiteral("data")) {
    return true;
  }

  nsAutoCString uriSpec;
  rv = uri->GetSpec(uriSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (uriSpec.EqualsLiteral("about:blank") ||
      uriSpec.EqualsLiteral("about:srcdoc")) {
    return true;
  }

  OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(uri, attrs);
  if (NS_WARN_IF(!principal)) {
    return false;
  }

  nsCOMPtr<nsIContentSecurityManager> csm =
    do_GetService(NS_CONTENTSECURITYMANAGER_CONTRACTID);
  if (NS_WARN_IF(!csm)) {
    return false;
  }

  bool isTrustworthyOrigin = false;
  csm->IsOriginPotentiallyTrustworthy(principal, &isTrustworthyOrigin);
  return isTrustworthyOrigin;
}

bool
PresentationRequest::IsAllURLAuthenticated()
{
  for (const auto& url : mUrls) {
    if (!IsPrioriAuthenticatedURL(url)) {
      return false;
    }
  }

  return true;
}
