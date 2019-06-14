/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerNotificationManager.h"

#include "mozilla/ScopeExit.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

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
  nsTObserverArray<RefPtr<DebuggerNotificationObserver>>::ForwardIterator iter(
      mNotificationObservers);

  while (iter.HasMore()) {
    RefPtr<DebuggerNotificationObserver> observer(iter.GetNext());
    if (observer->HasListeners()) {
      return true;
    }
  }

  return false;
}

void DebuggerNotificationManager::NotifyListeners(
    DebuggerNotification* aNotification) {
  nsTObserverArray<RefPtr<DebuggerNotificationObserver>>::ForwardIterator iter(
      mNotificationObservers);

  while (iter.HasMore()) {
    RefPtr<DebuggerNotificationObserver> observer(iter.GetNext());
    observer->NotifyListeners(aNotification);
  }
}

}  // namespace dom
}  // namespace mozilla
