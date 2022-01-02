/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesVisitTitle_h
#define mozilla_dom_PlacesVisitTitle_h

#include "mozilla/dom/PlacesEvent.h"

namespace mozilla {
namespace dom {

class PlacesVisitTitle final : public PlacesEvent {
 public:
  explicit PlacesVisitTitle()
      : PlacesEvent(PlacesEventType::Page_title_changed) {}

  static already_AddRefed<PlacesVisitTitle> Constructor(
      const GlobalObject& aGlobal, const PlacesVisitTitleInit& aInitDict) {
    RefPtr<PlacesVisitTitle> event = new PlacesVisitTitle();
    event->mUrl = aInitDict.mUrl;
    event->mPageGuid = aInitDict.mPageGuid;
    event->mTitle = aInitDict.mTitle;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesVisitTitle_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesVisitTitle* AsPlacesVisitTitle() const override { return this; }

  void GetUrl(nsString& aUrl) const { aUrl = mUrl; }
  void GetPageGuid(nsCString& aPageGuid) const { aPageGuid = mPageGuid; }
  void GetTitle(nsString& aTitle) const { aTitle = mTitle; }

  nsString mUrl;
  nsCString mPageGuid;
  nsString mTitle;

 private:
  ~PlacesVisitTitle() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesVisitTitle_h
