/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkMoved_h
#define mozilla_dom_PlacesBookmarkMoved_h

#include "mozilla/dom/PlacesBookmark.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkMoved final : public PlacesBookmark {
 public:
  explicit PlacesBookmarkMoved()
      : PlacesBookmark(PlacesEventType::Bookmark_moved) {}

  static already_AddRefed<PlacesBookmarkMoved> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkMovedInit& aInitDict) {
    RefPtr<PlacesBookmarkMoved> event = new PlacesBookmarkMoved();
    event->mId = aInitDict.mId;
    event->mItemType = aInitDict.mItemType;
    event->mUrl = aInitDict.mUrl;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mIndex = aInitDict.mIndex;
    event->mOldParentGuid = aInitDict.mOldParentGuid;
    event->mOldIndex = aInitDict.mOldIndex;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    event->mTitle = aInitDict.mTitle;
    event->mTags = aInitDict.mTags;
    event->mFrecency = aInitDict.mFrecency;
    event->mHidden = aInitDict.mHidden;
    event->mVisitCount = aInitDict.mVisitCount;
    event->mDateAdded = aInitDict.mDateAdded;
    if (!aInitDict.mLastVisitDate.IsNull()) {
      event->mLastVisitDate.SetValue(aInitDict.mLastVisitDate.Value());
    } else {
      event->mLastVisitDate.SetNull();
    }
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkMoved_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkMoved* AsPlacesBookmarkMoved() const override {
    return this;
  }

  int32_t Index() { return mIndex; }
  int32_t OldIndex() { return mOldIndex; }
  void GetOldParentGuid(nsCString& aOldParentGuid) {
    aOldParentGuid = mOldParentGuid;
  }
  void GetTitle(nsString& aTitle) { aTitle = mTitle; }
  void GetTags(nsString& aTags) { aTags = mTags; }
  uint64_t Frecency() { return mFrecency; }
  bool Hidden() { return mHidden; }
  uint32_t VisitCount() { return mVisitCount; }
  uint64_t DateAdded() { return mDateAdded; }
  Nullable<uint64_t> GetLastVisitDate() { return mLastVisitDate; }

  int32_t mIndex;
  int32_t mOldIndex;
  nsCString mOldParentGuid;
  nsString mTitle;
  nsString mTags;
  int64_t mFrecency;
  bool mHidden;
  uint32_t mVisitCount;
  uint64_t mDateAdded;
  Nullable<uint64_t> mLastVisitDate;

 private:
  ~PlacesBookmarkMoved() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
