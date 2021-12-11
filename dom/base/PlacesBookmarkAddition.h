/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkAddition_h
#define mozilla_dom_PlacesBookmarkAddition_h

#include "mozilla/dom/PlacesBookmark.h"

namespace mozilla {
namespace dom {

class PlacesBookmarkAddition final : public PlacesBookmark {
 public:
  explicit PlacesBookmarkAddition()
      : PlacesBookmark(PlacesEventType::Bookmark_added) {}

  static already_AddRefed<PlacesBookmarkAddition> Constructor(
      const GlobalObject& aGlobal,
      const PlacesBookmarkAdditionInit& aInitDict) {
    RefPtr<PlacesBookmarkAddition> event = new PlacesBookmarkAddition();
    event->mItemType = aInitDict.mItemType;
    event->mId = aInitDict.mId;
    event->mParentId = aInitDict.mParentId;
    event->mIndex = aInitDict.mIndex;
    event->mUrl = aInitDict.mUrl;
    event->mTitle = aInitDict.mTitle;
    event->mDateAdded = aInitDict.mDateAdded;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkAddition_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkAddition* AsPlacesBookmarkAddition() const override {
    return this;
  }

  int32_t Index() { return mIndex; }
  void GetTitle(nsString& aTitle) { aTitle = mTitle; }
  uint64_t DateAdded() { return mDateAdded; }

  int32_t mIndex;
  nsString mTitle;
  uint64_t mDateAdded;

 private:
  ~PlacesBookmarkAddition() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesBookmarkAddition_h
