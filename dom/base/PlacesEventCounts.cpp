/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlacesEventCounts.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/PlacesEventCounts.h"
#include "mozilla/dom/PlacesEventBinding.h"
#include "mozilla/dom/PlacesObserversBinding.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(PlacesEventCounts)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PlacesEventCounts)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PlacesEventCounts)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PlacesEventCounts)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PlacesEventCounts::PlacesEventCounts() {
  ErrorResult rv;
  for (auto eventType : MakeWebIDLEnumeratedRange<PlacesEventType>()) {
    PlacesEventCounts_Binding::MaplikeHelpers::Set(
        this, NS_ConvertUTF8toUTF16(GetEnumString(eventType)), 0, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return;
    }
  }
}

nsresult PlacesEventCounts::Increment(PlacesEventType aEventType) {
  ErrorResult rv;
  nsAutoCString eventName(GetEnumString(aEventType));
  uint64_t count = PlacesEventCounts_Binding::MaplikeHelpers::Get(
      this, NS_ConvertUTF8toUTF16(eventName), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }
  PlacesEventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(eventName), ++count, rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }
  return NS_OK;
}

JSObject* PlacesEventCounts::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return PlacesEventCounts_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
