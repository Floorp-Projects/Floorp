/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DebuggerNotificationManager_h
#define mozilla_dom_DebuggerNotificationManager_h

#include "DebuggerNotificationObserver.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupports.h"
#include "nsTObserverArray.h"

namespace mozilla::dom {

class DebuggerNotification;
class DebuggerNotificationObserver;

class DebuggerNotificationManager final : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DebuggerNotificationManager)

  static RefPtr<DebuggerNotificationManager> ForDispatch(
      nsIGlobalObject* aDebuggeeGlobal) {
    if (MOZ_UNLIKELY(!aDebuggeeGlobal)) {
      return nullptr;
    }
    auto managerPtr = aDebuggeeGlobal->GetExistingDebuggerNotificationManager();
    if (MOZ_LIKELY(!managerPtr) || !managerPtr->HasListeners()) {
      return nullptr;
    }

    return managerPtr;
  }

  explicit DebuggerNotificationManager(nsIGlobalObject* aDebuggeeGlobal)
      : mDebuggeeGlobal(aDebuggeeGlobal) {}

  bool Attach(DebuggerNotificationObserver* aObserver);
  bool Detach(DebuggerNotificationObserver* aObserver);

  bool HasListeners();

  template <typename T, typename... Args>
  MOZ_CAN_RUN_SCRIPT void Dispatch(Args... aArgs) {
    RefPtr<DebuggerNotification> notification(new T(mDebuggeeGlobal, aArgs...));
    NotifyListeners(notification);
  }

 private:
  ~DebuggerNotificationManager() = default;

  MOZ_CAN_RUN_SCRIPT void NotifyListeners(DebuggerNotification* aNotification);

  nsCOMPtr<nsIGlobalObject> mDebuggeeGlobal;
  nsTObserverArray<RefPtr<DebuggerNotificationObserver>> mNotificationObservers;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_DebuggerNotificationManager_h
