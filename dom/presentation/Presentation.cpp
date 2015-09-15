/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDefaultRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSessions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingGetSessionPromises)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Presentation, DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDefaultRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSessions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingGetSessionPromises)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(Presentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Presentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Presentation)
  NS_INTERFACE_MAP_ENTRY(nsIPresentationRespondingListener)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */ already_AddRefed<Presentation>
Presentation::Create(nsPIDOMWindow* aWindow)
{
  nsRefPtr<Presentation> presentation = new Presentation(aWindow);
  return NS_WARN_IF(!presentation->Init()) ? nullptr : presentation.forget();
}

Presentation::Presentation(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
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

  if (NS_WARN_IF(!GetOwner())) {
    return false;
  }
  mWindowId = GetOwner()->WindowID();

  // Check if a session instance is required now. A session may already be
  // connecting before the web content gets loaded in a presenting browsing
  // context (receiver).
  nsAutoString sessionId;
  nsresult rv = service->GetExistentSessionIdAtLaunch(mWindowId, sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  if (!sessionId.IsEmpty()) {
    rv = NotifySessionConnect(mWindowId, sessionId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
  }

  // Register listener for incoming sessions.
  rv = service->RegisterRespondingListener(mWindowId, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

void Presentation::Shutdown()
{
  mDefaultRequest = nullptr;
  mSessions.Clear();
  mPendingGetSessionPromises.Clear();

  // Unregister listener for incoming sessions.
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterRespondingListener(mWindowId);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ void
Presentation::DisconnectFromOwner()
{
  Shutdown();
  DOMEventTargetHelper::DisconnectFromOwner();
}

/* virtual */ JSObject*
Presentation::WrapObject(JSContext* aCx,
                         JS::Handle<JSObject*> aGivenProto)
{
  return PresentationBinding::Wrap(aCx, this, aGivenProto);
}

void
Presentation::SetDefaultRequest(PresentationRequest* aRequest)
{
  mDefaultRequest = aRequest;
}

already_AddRefed<PresentationRequest>
Presentation::GetDefaultRequest() const
{
  nsRefPtr<PresentationRequest> request = mDefaultRequest;
  return request.forget();
}

already_AddRefed<Promise>
Presentation::GetSession(ErrorResult& aRv)
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

  // If there's no existing session, leave the promise pending until a
  // connecting request arrives from the controlling browsing context (sender).
  // http://w3c.github.io/presentation-api/#dom-presentation-getsession
  if (!mSessions.IsEmpty()) {
    promise->MaybeResolve(mSessions[0]);
  } else {
    mPendingGetSessionPromises.AppendElement(promise);
  }

  return promise.forget();
}

already_AddRefed<Promise>
Presentation::GetSessions(ErrorResult& aRv) const
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

  promise->MaybeResolve(mSessions);
  return promise.forget();
}

NS_IMETHODIMP
Presentation::NotifySessionConnect(uint64_t aWindowId,
                                   const nsAString& aSessionId)
{
  if (NS_WARN_IF(aWindowId != GetOwner()->WindowID())) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<PresentationSession> session =
    PresentationSession::Create(GetOwner(), aSessionId,
                                PresentationSessionState::Disconnected);
  if (NS_WARN_IF(!session)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mSessions.AppendElement(session);

  // Resolve pending |GetSession| promises if any.
  if (!mPendingGetSessionPromises.IsEmpty()) {
    for(uint32_t i = 0; i < mPendingGetSessionPromises.Length(); i++) {
      mPendingGetSessionPromises[i]->MaybeResolve(session);
    }
    mPendingGetSessionPromises.Clear();
  }

  return DispatchSessionAvailableEvent();
}

nsresult
Presentation::DispatchSessionAvailableEvent()
{
  nsRefPtr<AsyncEventDispatcher> asyncDispatcher =
    new AsyncEventDispatcher(this, NS_LITERAL_STRING("sessionavailable"), false);
  return asyncDispatcher->PostDOMEvent();
}
