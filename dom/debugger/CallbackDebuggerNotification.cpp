/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallbackDebuggerNotification.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(CallbackDebuggerNotification,
                                   DebuggerNotification)

NS_IMPL_ADDREF_INHERITED(CallbackDebuggerNotification, DebuggerNotification)
NS_IMPL_RELEASE_INHERITED(CallbackDebuggerNotification, DebuggerNotification)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CallbackDebuggerNotification)
NS_INTERFACE_MAP_END_INHERITING(DebuggerNotification)

JSObject* CallbackDebuggerNotification::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CallbackDebuggerNotification_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DebuggerNotification> CallbackDebuggerNotification::CloneInto(
    nsIGlobalObject* aNewOwner) const {
  RefPtr<CallbackDebuggerNotification> notification(
      new CallbackDebuggerNotification(mDebuggeeGlobal, mType, mPhase,
                                       aNewOwner));
  return notification.forget();
}

}  // namespace mozilla::dom
