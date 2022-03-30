/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkRemoved_h
#define mozilla_dom_PlacesBookmarkRemoved_h

#include "mozilla/dom/PlacesBookmark.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkRemoved final : public PlacesBookmark {
 public:
  explicit PlacesBookmarkRemoved()
      : PlacesBookmark(PlacesEventType::Bookmark_removed) {}

  static already_AddRefed<PlacesBookmarkRemoved> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkRemovedInit& aInitDict) {
    RefPtr<PlacesBookmarkRemoved> event = new PlacesBookmarkRemoved();
    event->mItemType = aInitDict.mItemType;
    event->mId = aInitDict.mId;
    event->mParentId = aInitDict.mParentId;
    event->mIndex = aInitDict.mIndex;
    event->mUrl = aInitDict.mUrl;
    event->mTitle = aInitDict.mTitle;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    event->mIsDescendantRemoval = aInitDict.mIsDescendantRemoval;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkRemoved_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkRemoved* AsPlacesBookmarkRemoved() const override {
    return this;
  }

  int32_t Index() { return mIndex; }
  void GetTitle(nsString& aTitle) { aTitle = mTitle; }
  bool IsDescendantRemoval() { return mIsDescendantRemoval; }

  int32_t mIndex;
  nsString mTitle;
  bool mIsDescendantRemoval;

 private:
  ~PlacesBookmarkRemoved() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
