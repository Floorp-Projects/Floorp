/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/WindowGlobalChild.h"

namespace mozilla {
namespace dom {

JSObject* JSWindowActorChild::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return JSWindowActorChild_Binding::Wrap(aCx, this, aGivenProto);
}

WindowGlobalChild* JSWindowActorChild::Manager() const { return mManager; }

void JSWindowActorChild::Init(WindowGlobalChild* aManager) {
  MOZ_ASSERT(!mManager, "Cannot Init() a JSWindowActorChild twice!");
  mManager = aManager;
}

nsISupports* JSWindowActorChild::GetParentObject() const {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(JSWindowActorChild, mManager)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(JSWindowActorChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(JSWindowActorChild, Release)

}  // namespace dom
}  // namespace mozilla
