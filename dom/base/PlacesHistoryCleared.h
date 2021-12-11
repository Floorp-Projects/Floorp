/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesHistoryCleared_h
#define mozilla_dom_PlacesHistoryCleared_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesHistoryCleared final : public PlacesEvent {
 public:
  explicit PlacesHistoryCleared()
      : PlacesEvent(PlacesEventType::History_cleared) {}

  static already_AddRefed<PlacesHistoryCleared> Constructor(
      const GlobalObject& aGlobal) {
    RefPtr<PlacesHistoryCleared> event = new PlacesHistoryCleared();
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesHistoryCleared_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesHistoryCleared* AsPlacesHistoryCleared() const override {
    return this;
  }

 private:
  ~PlacesHistoryCleared() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesHistoryCleared_h
