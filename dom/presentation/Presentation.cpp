/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include "mozilla/dom/PresentationBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocShell.h"
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
Presentation::Create(nsPIDOMWindowInner* aWindow)
{
  RefPtr<Presentation> presentation = new Presentation(aWindow);
  return presentation.forget();
}

Presentation::Presentation(nsPIDOMWindowInner* aWindow)
  : DOMEventTargetHelper(aWindow)
{
}

Presentation::~Presentation()
{
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
  if (IsInPresentedContent()) {
    return;
  }

  mDefaultRequest = aRequest;
}

already_AddRefed<PresentationRequest>
Presentation::GetDefaultRequest() const
{
  if (IsInPresentedContent()) {
    return nullptr;
  }

  RefPtr<PresentationRequest> request = mDefaultRequest;
  return request.forget();
}

already_AddRefed<PresentationReceiver>
Presentation::GetReceiver()
{
  // return the same receiver if already created
  if (mReceiver) {
    RefPtr<PresentationReceiver> receiver = mReceiver;
    return receiver.forget();
  }

  if (!IsInPresentedContent()) {
    return nullptr;
  }

  mReceiver = PresentationReceiver::Create(GetOwner());
  if (NS_WARN_IF(!mReceiver)) {
    MOZ_ASSERT(mReceiver);
    return nullptr;
  }

  RefPtr<PresentationReceiver> receiver = mReceiver;
  return receiver.forget();
}

bool
Presentation::IsInPresentedContent() const
{
  nsCOMPtr<nsIDocShell> docShell = GetOwner()->GetDocShell();
  MOZ_ASSERT(docShell);

  nsAutoString presentationURL;
  nsContentUtils::GetPresentationURL(docShell, presentationURL);

  return !presentationURL.IsEmpty();
}
