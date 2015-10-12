/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PresentationRequestBinding.h"
#include "mozilla/dom/PresentationConnectionAvailableEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozIThirdPartyUtil.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationService.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "PresentationAvailability.h"
#include "PresentationCallbacks.h"
#include "PresentationRequest.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(PresentationRequest, DOMEventTargetHelper,
                                   mAvailability)

NS_IMPL_ADDREF_INHERITED(PresentationRequest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationRequest, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PresentationRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<PresentationRequest>
PresentationRequest::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aUrl,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Ensure the URL is not empty.
  if (NS_WARN_IF(aUrl.IsEmpty())) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  nsRefPtr<PresentationRequest> request = new PresentationRequest(window, aUrl);
  return NS_WARN_IF(!request->Init()) ? nullptr : request.forget();
}

PresentationRequest::PresentationRequest(nsPIDOMWindow* aWindow,
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
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
  rv = service->StartSession(mUrl, id, origin, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
  }

  return promise.forget();
}

already_AddRefed<Promise>
PresentationRequest::GetAvailability(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  promise->MaybeResolve(mAvailability);
  return promise.forget();
}

nsresult
PresentationRequest::DispatchConnectionAvailableEvent(PresentationConnection* aConnection)
{
  PresentationConnectionAvailableEventInit init;
  init.mConnection = aConnection;

  nsRefPtr<PresentationConnectionAvailableEvent> event =
    PresentationConnectionAvailableEvent::Constructor(this,
                                                      NS_LITERAL_STRING("connectionavailable"),
                                                      init);
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_FAILURE;
  }
  event->SetTrusted(true);

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  return asyncDispatcher->PostDOMEvent();
}
