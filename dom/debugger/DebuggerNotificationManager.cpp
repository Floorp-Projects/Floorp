/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerNotificationManager.h"

#include "nsIGlobalObject.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(DebuggerNotificationManager, mDebuggeeGlobal,
                         mNotificationObservers)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DebuggerNotificationManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DebuggerNotificationManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DebuggerNotificationManager)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DebuggerNotificationManager)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

bool DebuggerNotificationManager::Attach(
    DebuggerNotificationObserver* aObserver) {
  RefPtr<DebuggerNotificationObserver> ptr(aObserver);

  if (mNotificationObservers.Contains(ptr)) {
    return false;
  }

  mNotificationObservers.AppendElement(ptr);
  return true;
}
bool DebuggerNotificationManager::Detach(
    DebuggerNotificationObserver* aObserver) {
  RefPtr<DebuggerNotificationObserver> ptr(aObserver);

  return mNotificationObservers.RemoveElement(ptr);
}

bool DebuggerNotificationManager::HasListeners() {
  const auto [begin, end] = mNotificationObservers.NonObservingRange();
  return std::any_of(begin, end, [](const auto& observer) {
    return observer->HasListeners();
  });
}

void DebuggerNotificationManager::NotifyListeners(
    DebuggerNotification* aNotification) {
  for (RefPtr<DebuggerNotificationObserver> observer :
       mNotificationObservers.ForwardRange()) {
    observer->NotifyListeners(aNotification);
  }
}

}  // namespace mozilla::dom
