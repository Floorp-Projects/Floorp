/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DebuggerNotification_h
#define mozilla_dom_DebuggerNotification_h

#include "DebuggerNotificationManager.h"
#include "mozilla/dom/DebuggerNotificationBinding.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class DebuggerNotification : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DebuggerNotification)

  DebuggerNotification(nsIGlobalObject* aDebuggeeGlobal,
                       DebuggerNotificationType aType,
                       nsIGlobalObject* aOwnerGlobal = nullptr)
      : mType(aType),
        mDebuggeeGlobal(aDebuggeeGlobal),
        mOwnerGlobal(aOwnerGlobal) {}

  nsIGlobalObject* GetParentObject() const {
    MOZ_ASSERT(mOwnerGlobal,
               "Notification must be cloned into an observer global before "
               "being wrapped");
    return mOwnerGlobal;
  }

  DebuggerNotificationType Type() const { return mType; }

  void GetGlobal(JSContext* aCx, JS::MutableHandle<JSObject*> aResult) {
    aResult.set(mDebuggeeGlobal->GetGlobalJSObject());
  }

  virtual already_AddRefed<DebuggerNotification> CloneInto(
      nsIGlobalObject* aNewOwner) const;

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~DebuggerNotification() = default;

  DebuggerNotificationType mType;
  nsCOMPtr<nsIGlobalObject> mDebuggeeGlobal;

 private:
  nsCOMPtr<nsIGlobalObject> mOwnerGlobal;
};

MOZ_CAN_RUN_SCRIPT inline void DebuggerNotificationDispatch(
    nsIGlobalObject* aDebuggeeGlobal, DebuggerNotificationType aType) {
  auto manager = DebuggerNotificationManager::ForDispatch(aDebuggeeGlobal);
  if (MOZ_UNLIKELY(manager)) {
    manager->Dispatch<DebuggerNotification>(aType);
  }
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_DebuggerNotification_h
