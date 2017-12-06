/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnManagerBase.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

NS_NAMED_LITERAL_STRING(kVisibilityChange, "visibilitychange");

WebAuthnManagerBase::WebAuthnManagerBase(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aParent);
}

WebAuthnManagerBase::~WebAuthnManagerBase()
{
  MOZ_ASSERT(NS_IsMainThread());
}

/***********************************************************************
 * IPC Protocol Implementation
 **********************************************************************/

bool
WebAuthnManagerBase::MaybeCreateBackgroundActor()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mChild) {
    return true;
  }

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return false;
  }

  RefPtr<WebAuthnTransactionChild> mgr(new WebAuthnTransactionChild(this));
  PWebAuthnTransactionChild* constructedMgr =
    actorChild->SendPWebAuthnTransactionConstructor(mgr);

  if (NS_WARN_IF(!constructedMgr)) {
    return false;
  }

  MOZ_ASSERT(constructedMgr == mgr);
  mChild = mgr.forget();

  return true;
}

void
WebAuthnManagerBase::ActorDestroyed()
{
  MOZ_ASSERT(NS_IsMainThread());
  mChild = nullptr;
}

/***********************************************************************
 * Event Handling
 **********************************************************************/

void
WebAuthnManagerBase::ListenForVisibilityEvents()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIDocument> doc = mParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->AddSystemEventListener(kVisibilityChange, this,
                                            /* use capture */ true,
                                            /* wants untrusted */ false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
WebAuthnManagerBase::StopListeningForVisibilityEvents()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIDocument> doc = mParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }

  nsresult rv = doc->RemoveSystemEventListener(kVisibilityChange, this,
                                               /* use capture */ true);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
WebAuthnManagerBase::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aEvent);

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.Equals(kVisibilityChange)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_FAILURE;
  }

  if (doc->Hidden()) {
    CancelTransaction(NS_ERROR_ABORT);
  }

  return NS_OK;
}

}
}
