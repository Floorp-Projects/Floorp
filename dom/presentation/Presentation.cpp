/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationService.h"
#include "nsServiceManagerUtils.h"
#include "Presentation.h"
#include "PresentationReceiver.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(Presentation, DOMEventTargetHelper,
                                   mDefaultRequest, mReceiver)

NS_IMPL_ADDREF_INHERITED(Presentation, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Presentation, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Presentation)
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

  // Check if a receiver instance is required now. A session may already be
  // connecting before the web content gets loaded in a receiving browsing
  // context.
  nsAutoString sessionId;
  nsresult rv = service->GetExistentSessionIdAtLaunch(GetOwner()->WindowID(), sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  if (!sessionId.IsEmpty()) {
    mReceiver = PresentationReceiver::Create(GetOwner(), sessionId);
    if (NS_WARN_IF(!mReceiver)) {
      return false;
    }
  }

  return true;
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

already_AddRefed<PresentationReceiver>
Presentation::GetReceiver() const
{
  nsRefPtr<PresentationReceiver> receiver = mReceiver;
  return receiver.forget();
}
