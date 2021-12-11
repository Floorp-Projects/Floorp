/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerNotification.h"

#include "DebuggerNotificationManager.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DebuggerNotification, mDebuggeeGlobal,
                                      mOwnerGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DebuggerNotification)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DebuggerNotification)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DebuggerNotification)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* DebuggerNotification::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return DebuggerNotification_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DebuggerNotification> DebuggerNotification::CloneInto(
    nsIGlobalObject* aNewOwner) const {
  RefPtr<DebuggerNotification> notification(
      new DebuggerNotification(mDebuggeeGlobal, mType, aNewOwner));
  return notification.forget();
}

}  // namespace mozilla::dom
