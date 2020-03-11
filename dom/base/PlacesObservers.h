/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesObservers__
#define mozilla_dom_PlacesObservers__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PlacesObserversBinding.h"
#include "mozilla/dom/PlacesEvent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Pair.h"
#include "mozilla/places/INativePlacesEventCallback.h"
#include "nsIWeakReferenceUtils.h"

namespace mozilla {

namespace dom {

class PlacesObservers {
 public:
  static void AddListener(GlobalObject& aGlobal,
                          const nsTArray<PlacesEventType>& aEventTypes,
                          PlacesEventCallback& aCallback, ErrorResult& rv);
  static void AddListener(GlobalObject& aGlobal,
                          const nsTArray<PlacesEventType>& aEventTypes,
                          PlacesWeakCallbackWrapper& aCallback,
                          ErrorResult& rv);
  static void AddListener(const nsTArray<PlacesEventType>& aEventTypes,
                          places::INativePlacesEventCallback* aCallback);
  static void RemoveListener(GlobalObject& aGlobal,
                             const nsTArray<PlacesEventType>& aEventTypes,
                             PlacesEventCallback& aCallback, ErrorResult& rv);
  static void RemoveListener(GlobalObject& aGlobal,
                             const nsTArray<PlacesEventType>& aEventTypes,
                             PlacesWeakCallbackWrapper& aCallback,
                             ErrorResult& rv);
  static void RemoveListener(const nsTArray<PlacesEventType>& aEventTypes,
                             places::INativePlacesEventCallback* aCallback);

  MOZ_CAN_RUN_SCRIPT
  static void NotifyListeners(
      GlobalObject& aGlobal,
      const Sequence<OwningNonNull<PlacesEvent>>& aEvents, ErrorResult& rv);

  MOZ_CAN_RUN_SCRIPT
  static void NotifyListeners(
      const Sequence<OwningNonNull<PlacesEvent>>& aEvents);

 private:
  static void RemoveListener(uint32_t aFlags, PlacesEventCallback& aCallback);
  static void RemoveListener(uint32_t aFlags,
                             PlacesWeakCallbackWrapper& aCallback);
  static void RemoveListener(uint32_t aFlags,
                             places::INativePlacesEventCallback* aCallback);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesObservers__
