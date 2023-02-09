/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesBookmarkKeyword_h
#define mozilla_dom_PlacesBookmarkKeyword_h

#include "mozilla/dom/PlacesBookmarkChanged.h"

namespace mozilla {
namespace dom {
class PlacesBookmarkKeyword final : public PlacesBookmarkChanged {
 public:
  explicit PlacesBookmarkKeyword()
      : PlacesBookmarkChanged(PlacesEventType::Bookmark_keyword_changed) {}

  static already_AddRefed<PlacesBookmarkKeyword> Constructor(
      const GlobalObject& aGlobal, const PlacesBookmarkKeywordInit& aInitDict) {
    RefPtr<PlacesBookmarkKeyword> event = new PlacesBookmarkKeyword();
    event->mId = aInitDict.mId;
    event->mItemType = aInitDict.mItemType;
    event->mUrl = aInitDict.mUrl;
    event->mGuid = aInitDict.mGuid;
    event->mParentGuid = aInitDict.mParentGuid;
    event->mKeyword = aInitDict.mKeyword;
    event->mLastModified = aInitDict.mLastModified;
    event->mSource = aInitDict.mSource;
    event->mIsTagging = aInitDict.mIsTagging;
    return event.forget();
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return PlacesBookmarkKeyword_Binding::Wrap(aCx, this, aGivenProto);
  }

  const PlacesBookmarkKeyword* AsPlacesBookmarkKeyword() const override {
    return this;
  }

  void GetKeyword(nsCString& aKeyword) const { aKeyword = mKeyword; }
  nsCString mKeyword;

 private:
  ~PlacesBookmarkKeyword() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif
