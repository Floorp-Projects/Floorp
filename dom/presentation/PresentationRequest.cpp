/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationRequest.h"

#include "ControllerConnectionCollection.h"
#include "mozilla/dom/PresentationRequestBinding.h"
#include "mozilla/dom/PresentationConnectionAvailableEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozIThirdPartyUtil.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIPresentationService.h"
#include "nsIURI.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"
#include "nsSandboxFlags.h"
#include "nsServiceManagerUtils.h"
#include "PresentationAvailability.h"
#include "PresentationCallbacks.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(PresentationRequest, DOMEventTargetHelper,
                                   mAvailability)

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
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Ensure the URL is not empty.
  if (NS_WARN_IF(aUrl.IsEmpty())) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  // Resolve relative URL to absolute URL
  nsCOMPtr<nsIURI> baseUri = window->GetDocBaseURI();

  nsAutoString absoluteUrl;
  nsresult rv = GetAbsoluteURL(aUrl, baseUri, window->GetExtantDoc(), absoluteUrl);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<PresentationRequest> request =
    new PresentationRequest(window, absoluteUrl);
  return NS_WARN_IF(!request->Init()) ? nullptr : request.forget();
}

PresentationRequest::PresentationRequest(nsPIDOMWindowInner* aWindow,
                                         const nsAString& aUrl)
  : DOMEventTargetHelper(aWindow)
  , mUrl(aUrl)
{
}

PresentationRequest::~PresentationRequest()
{
}

bool
PresentationRequest::Init()
{
  mAvailability = PresentationAvailability::Create(GetOwner());
  if (NS_WARN_IF(!mAvailability)) {
    return false;
  }

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

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
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

  nsCOMPtr<nsIPresentationServiceCallback> callback =
    new PresentationRequesterCallback(this, mUrl, id, promise);
  rv = service->StartSession(mUrl, id, origin, aDeviceId, GetOwner()->WindowID(), callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
PresentationRequest::Reconnect(const nsAString& aPresentationId,
                               ErrorResult& aRv)
{
  // TODO: Before starting to reconnect, we have to run the prohibits
  // mixed security contexts algorithm first. This will be implemented
  // in bug 1254488.

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
    if (url.Equals(mUrl)) {
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
                                      mUrl,
                                      aPresentationId,
                                      aPromise,
                                      connection);

  nsresult rv =
    service->ReconnectSession(mUrl,
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

  if (doc->GetSandboxFlags() & SANDBOXED_PRESENTATION) {
    promise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
    return promise.forget();
  }

  promise->MaybeResolve(mAvailability);
  return promise.forget();
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
