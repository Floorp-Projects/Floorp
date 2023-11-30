/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PlacesEvent_h
#define mozilla_dom_PlacesEvent_h

#include "mozilla/dom/PlacesEventBinding.h"
#include "nsWrapperCache.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class PlacesEvent : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PlacesEvent)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PlacesEvent)

  nsISupports* GetParentObject() const;

  PlacesEventType Type() const { return mType; }

  virtual const PlacesVisit* AsPlacesVisit() const { return nullptr; }
  virtual const PlacesBookmark* AsPlacesBookmark() const { return nullptr; }
  virtual const PlacesBookmarkAddition* AsPlacesBookmarkAddition() const {
    return nullptr;
  }
  virtual const PlacesBookmarkRemoved* AsPlacesBookmarkRemoved() const {
    return nullptr;
  }
  virtual const PlacesBookmarkMoved* AsPlacesBookmarkMoved() const {
    return nullptr;
  }
  virtual const PlacesBookmarkGuid* AsPlacesBookmarkGuid() const {
    return nullptr;
  }
  virtual const PlacesBookmarkKeyword* AsPlacesBookmarkKeyword() const {
    return nullptr;
  }
  virtual const PlacesBookmarkTags* AsPlacesBookmarkTags() const {
    return nullptr;
  }
  virtual const PlacesBookmarkTime* AsPlacesBookmarkTime() const {
    return nullptr;
  }
  virtual const PlacesBookmarkTitle* AsPlacesBookmarkTitle() const {
    return nullptr;
  }
  virtual const PlacesBookmarkUrl* AsPlacesBookmarkUrl() const {
    return nullptr;
  }
  virtual const PlacesFavicon* AsPlacesFavicon() const { return nullptr; }
  virtual const PlacesVisitTitle* AsPlacesVisitTitle() const { return nullptr; }
  virtual const PlacesHistoryCleared* AsPlacesHistoryCleared() const {
    return nullptr;
  }
  virtual const PlacesRanking* AsPlacesRanking() const { return nullptr; }
  virtual const PlacesVisitRemoved* AsPlacesVisitRemoved() const {
    return nullptr;
  }
  virtual const PlacesPurgeCaches* AsPlacesPurgeCaches() const {
    return nullptr;
  }

 protected:
  explicit PlacesEvent(PlacesEventType aType) : mType(aType) {}
  virtual ~PlacesEvent() = default;
  PlacesEventType mType;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PlacesEvent_h
