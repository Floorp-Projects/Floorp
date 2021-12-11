/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkGuid_h
#define mozilla_dom_PlacesBookmarkGuid_h

#include "mozilla/dom/PlacesBookmarkChanged.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkGuid final : public PlacesBookmarkChanged {
 public:
  explicit PlacesBookmarkGuid()
      : PlacesBookmarkChanged(PlacesEventType::Bookmark_guid_changed) {}

  static already_AddRefed<PlacesBookmarkGuid> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkGuidInit& aInitDict) {
    RefPtr<PlacesBookmarkGuid> event = new PlacesBookmarkGuid();
    event->mId = aInitDict.mId;
    event->mItemType = aInitDict.mItemType;
    event->mUrl = aInitDict.mUrl;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mLastModified = aInitDict.mLastModified;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkGuid_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkGuid* AsPlacesBookmarkGuid() const override {
    return this;
  }

 private:
  ~PlacesBookmarkGuid() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
