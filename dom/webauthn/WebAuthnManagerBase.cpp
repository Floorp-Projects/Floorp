/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnManagerBase.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/Event.h"
#include "nsGlobalWindowInner.h"
#include "nsPIWindowRoot.h"

namespace mozilla {
namespace dom {

NS_NAMED_LITERAL_STRING(kDeactivateEvent, "deactivate");
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

  nsCOMPtr<nsPIDOMWindowOuter> outer = mParent->GetOuterWindow();
  if (NS_WARN_IF(!outer)) {
    return;
  }

  nsCOMPtr<EventTarget> windowRoot = outer->GetTopWindowRoot();
  if (NS_WARN_IF(!windowRoot)) {
    return;
  }

  nsresult rv = windowRoot->AddEventListener(kDeactivateEvent, this,
                                             /* use capture */ true,
                                             /* wants untrusted */ false);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  rv = windowRoot->AddEventListener(kVisibilityChange, this,
                                    /* use capture */ true,
                                    /* wants untrusted */ false);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
WebAuthnManagerBase::StopListeningForVisibilityEvents()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindowOuter> outer = mParent->GetOuterWindow();
  if (NS_WARN_IF(!outer)) {
    return;
  }

  nsCOMPtr<EventTarget> windowRoot = outer->GetTopWindowRoot();
  if (NS_WARN_IF(!windowRoot)) {
    return;
  }

  windowRoot->RemoveEventListener(kDeactivateEvent, this,
                                  /* use capture */ true);
  windowRoot->RemoveEventListener(kVisibilityChange, this,
                                  /* use capture */ true);
}

NS_IMETHODIMP
WebAuthnManagerBase::HandleEvent(Event* aEvent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aEvent);

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.Equals(kDeactivateEvent) && !type.Equals(kVisibilityChange)) {
    return NS_ERROR_FAILURE;
  }

  // The "deactivate" event on the root window has no
  // "current inner window" and thus GetTarget() is always null.
  if (type.Equals(kVisibilityChange)) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aEvent->GetTarget());
    if (NS_WARN_IF(!doc) || !doc->Hidden()) {
      return NS_OK;
    }

    nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(doc->GetInnerWindow());
    if (NS_WARN_IF(!win) || !win->IsTopInnerWindow()) {
      return NS_OK;
    }
  }

  CancelTransaction(NS_ERROR_DOM_ABORT_ERR);
  return NS_OK;
}

}
}
