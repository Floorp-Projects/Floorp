/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesFavicon_h
#define mozilla_dom_PlacesFavicon_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesFavicon final : public PlacesEvent {
 public:
  explicit PlacesFavicon() : PlacesEvent(PlacesEventType::Favicon_changed) {}

  static already_AddRefed<PlacesFavicon> Constructor(
      const GlobalObject& aGlobal, const PlacesFaviconInit& aInitDict) {
    RefPtr<PlacesFavicon> event = new PlacesFavicon();
    event->mUrl = aInitDict.mUrl;
    event->mPageGuid = aInitDict.mPageGuid;
    event->mFaviconUrl = aInitDict.mFaviconUrl;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesFavicon_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesFavicon* AsPlacesFavicon() const override { return this; }

  void GetUrl(nsString& aUrl) const { aUrl = mUrl; }
  void GetPageGuid(nsCString& aPageGuid) const { aPageGuid = mPageGuid; }
  void GetFaviconUrl(nsString& aFaviconUrl) const { aFaviconUrl = mFaviconUrl; }

  nsString mUrl;
  nsCString mPageGuid;
  nsString mFaviconUrl;

 private:
  ~PlacesFavicon() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesFavicon_h
