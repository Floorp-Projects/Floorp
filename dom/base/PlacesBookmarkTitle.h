/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkTitle_h
#define mozilla_dom_PlacesBookmarkTitle_h

#include "mozilla/dom/PlacesBookmarkChanged.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkTitle final : public PlacesBookmarkChanged {
 public:
  explicit PlacesBookmarkTitle()
      : PlacesBookmarkChanged(PlacesEventType::Bookmark_title_changed) {}

  static already_AddRefed<PlacesBookmarkTitle> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkTitleInit& aInitDict) {
    RefPtr<PlacesBookmarkTitle> event = new PlacesBookmarkTitle();
    event->mId = aInitDict.mId;
    event->mItemType = aInitDict.mItemType;
    event->mUrl = aInitDict.mUrl;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mTitle = aInitDict.mTitle;
    event->mLastModified = aInitDict.mLastModified;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkTitle_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkTitle* AsPlacesBookmarkTitle() const override {
    return this;
  }

  void GetTitle(nsString& aTitle) const { aTitle = mTitle; }

  nsString mTitle;

 private:
  ~PlacesBookmarkTitle() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
