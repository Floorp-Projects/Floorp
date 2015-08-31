/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/PresentationAvailableEvent.h"
#include "mozilla/dom/PresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationService.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "Presentation.h"
#include "PresentationCallbacks.h"
#include "PresentationSession.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(Presentation)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Presentation, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSession)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Presentation, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSession)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(Presentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Presentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Presentation)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<Presentation>
Presentation::Create(nsPIDOMWindow* aWindow)
{
  nsRefPtr<Presentation> presentation = new Presentation(aWindow);
  return NS_WARN_IF(!presentation->Init()) ? nullptr : presentation.forget();
}

Presentation::Presentation(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mAvailable(false)
{
}

Presentation::~Presentation()
{
  Shutdown();
}

bool
Presentation::Init()
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->RegisterListener(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return false;
  }
  deviceManager->GetDeviceAvailable(&mAvailable);

  // Check if a session instance is required now. The receiver requires a
  // session instance is ready at beginning because the web content may access
  // it right away; whereas the sender doesn't until |startSession| succeeds.
  nsAutoString sessionId;
  rv = service->GetExistentSessionIdAtLaunch(GetOwner()->WindowID(), sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  if (!sessionId.IsEmpty()) {
    mSession = PresentationSession::Create(GetOwner(), sessionId,
                                           PresentationSessionState::Disconnected);
    if (NS_WARN_IF(!mSession)) {
      return false;
    }
  }

  return true;
}

void Presentation::Shutdown()
{
  mSession = nullptr;

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterListener(this);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ JSObject*
Presentation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PresentationBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
Presentation::StartSession(const nsAString& aUrl,
                           const Optional<nsAString>& aId,
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Ensure there's something to select.
  if (NS_WARN_IF(!mAvailable)) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  // Ensure the URL is not empty.
  if (NS_WARN_IF(aUrl.IsEmpty())) {
    promise->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    return promise.forget();
  }

  // Generate an ID if it's not assigned.
  nsAutoString id;
  if (aId.WasPassed()) {
    id = aId.Value();
  } else {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1");
    if(NS_WARN_IF(!uuidgen)) {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
      return promise.forget();
    }

    nsID uuid;
    uuidgen->GenerateUUIDInPlace(&uuid);
    char buffer[NSID_LENGTH];
    uuid.ToProvidedString(buffer);
    CopyASCIItoUTF16(buffer, id);
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if(NS_WARN_IF(!service)) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsIPresentationServiceCallback> callback =
    new PresentationRequesterCallback(GetOwner(), aUrl, id, promise);
  rv = service->StartSession(aUrl, id, origin, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }

  return promise.forget();
}

already_AddRefed<PresentationSession>
Presentation::GetSession() const
{
  nsRefPtr<PresentationSession> session = mSession;
  return session.forget();
}

bool
Presentation::CachedAvailable() const
{
  return mAvailable;
}

NS_IMETHODIMP
Presentation::NotifyAvailableChange(bool aAvailable)
{
  mAvailable = aAvailable;

  PresentationAvailableEventInit init;
  init.mAvailable = mAvailable;
  nsRefPtr<PresentationAvailableEvent> event =
    PresentationAvailableEvent::Constructor(this,
                                            NS_LITERAL_STRING("availablechange"),
                                            init);
  event->SetTrusted(true);

  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, event);
  return asyncDispatcher->PostDOMEvent();
}
