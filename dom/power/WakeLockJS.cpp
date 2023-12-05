/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPIDOMWindow.h"
#include "WakeLockJS.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WakeLockJS, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WakeLockJS)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WakeLockJS)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WakeLockJS)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END

WakeLockJS::WakeLockJS(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

nsISupports* WakeLockJS::GetParentObject() const { return mWindow; }

JSObject* WakeLockJS::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return WakeLock_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> WakeLockJS::Request(WakeLockType aType) {
  return nullptr;
}

void WakeLockJS::NotifyOwnerDocumentActivityChanged() {}

NS_IMETHODIMP WakeLockJS::HandleEvent(Event* aEvent) { return NS_OK; }

}  // namespace mozilla::dom
