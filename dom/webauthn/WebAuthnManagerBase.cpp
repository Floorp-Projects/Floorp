/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnManagerBase.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

WebAuthnManagerBase::WebAuthnManagerBase(nsPIDOMWindowInner* aParent)
    : mParent(aParent) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aParent);
}

WebAuthnManagerBase::~WebAuthnManagerBase() { MOZ_ASSERT(NS_IsMainThread()); }

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthnManagerBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(WebAuthnManagerBase, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthnManagerBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthnManagerBase)

/***********************************************************************
 * IPC Protocol Implementation
 **********************************************************************/

bool WebAuthnManagerBase::MaybeCreateBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mChild) {
    return true;
  }

  ::mozilla::ipc::PBackgroundChild* actorChild =
      ::mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
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
  mChild = std::move(mgr);

  return true;
}

void WebAuthnManagerBase::ActorDestroyed() {
  MOZ_ASSERT(NS_IsMainThread());
  mChild = nullptr;
}

}  // namespace mozilla::dom
