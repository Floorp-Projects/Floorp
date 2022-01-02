/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkTime_h
#define mozilla_dom_PlacesBookmarkTime_h

#include "mozilla/dom/PlacesBookmarkChanged.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkTime final : public PlacesBookmarkChanged {
 public:
  explicit PlacesBookmarkTime()
      : PlacesBookmarkChanged(PlacesEventType::Bookmark_time_changed) {}

  static already_AddRefed<PlacesBookmarkTime> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkTimeInit& aInitDict) {
    RefPtr<PlacesBookmarkTime> event = new PlacesBookmarkTime();
    event->mId = aInitDict.mId;
    event->mItemType = aInitDict.mItemType;
    event->mUrl = aInitDict.mUrl;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mDateAdded = aInitDict.mDateAdded;
    event->mLastModified = aInitDict.mLastModified;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkTime_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkTime* AsPlacesBookmarkTime() const override {
    return this;
  }

  uint64_t DateAdded() { return mDateAdded; }
  uint64_t mDateAdded;

 private:
  ~PlacesBookmarkTime() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
