/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DebuggerNotificationObserver_h
#define mozilla_dom_DebuggerNotificationObserver_h

#include "DebuggerNotificationManager.h"
#include "mozilla/dom/DebuggerNotificationObserverBinding.h"
#include "mozilla/RefPtr.h"
#include "nsTObserverArray.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class DebuggerNotification;

class DebuggerNotificationObserver final : public nsISupports,
                                           public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DebuggerNotificationObserver)

  static already_AddRefed<DebuggerNotificationObserver> Constructor(
      GlobalObject& aGlobal, ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mOwnerGlobal; }

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool Connect(JSContext* aCx, JS::Handle<JSObject*> aDebuggeeGlobal,
               ErrorResult& aRv);
  bool Disconnect(JSContext* aCx, JS::Handle<JSObject*> aDebuggeeGlobal,
                  ErrorResult& aRv);

  bool AddListener(DebuggerNotificationCallback& aHandlerFn);
  bool RemoveListener(DebuggerNotificationCallback& aHandlerFn);

  bool HasListeners();

  MOZ_CAN_RUN_SCRIPT void NotifyListeners(DebuggerNotification* aNotification);

 private:
  explicit DebuggerNotificationObserver(nsIGlobalObject* aOwnerGlobal);
  ~DebuggerNotificationObserver() = default;

  nsTObserverArray<RefPtr<DebuggerNotificationCallback>>
      mEventListenerCallbacks;
  nsCOMPtr<nsIGlobalObject> mOwnerGlobal;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_DebuggerNotificationObserver_h
