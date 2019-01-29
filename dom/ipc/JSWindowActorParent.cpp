/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/WindowGlobalParent.h"

namespace mozilla {
namespace dom {

JSObject* JSWindowActorParent::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return JSWindowActorParent_Binding::Wrap(aCx, this, aGivenProto);
}

WindowGlobalParent* JSWindowActorParent::Manager() const { return mManager; }

void JSWindowActorParent::Init(WindowGlobalParent* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorParent twice!");
  mManager = aManager;
}

nsISupports* JSWindowActorParent::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(JSWindowActorParent, mManager)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(JSWindowActorParent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(JSWindowActorParent, Release)

}  // namespace dom
}  // namespace mozilla
