/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesPurgeCaches_h
#define mozilla_dom_PlacesPurgeCaches_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesPurgeCaches final : public PlacesEvent {
 public:
  explicit PlacesPurgeCaches() : PlacesEvent(PlacesEventType::Purge_caches) {}

  static already_AddRefed<PlacesPurgeCaches> Constructor(
      const GlobalObject& aGlobal) {
    RefPtr<PlacesPurgeCaches> event = new PlacesPurgeCaches();
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesPurgeCaches_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesPurgeCaches* AsPlacesPurgeCaches() const override { return this; }

 private:
  ~PlacesPurgeCaches() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesPurgeCaches_h
