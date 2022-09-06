/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridTracks_h
#define mozilla_dom_GridTracks_h

#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {

struct ComputedGridTrackInfo;

namespace dom {

class GridDimension;
class GridTrack;

class GridTracks : public nsISupports, public nsWrapperCache {
 public:
  explicit GridTracks(GridDimension* aParent);

 protected:
  virtual ~GridTracks();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GridTracks)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  GridDimension* GetParentObject() { return mParent; }

  uint32_t Length() const;
  GridTrack* Item(uint32_t aIndex);
  GridTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  void SetTrackInfo(const mozilla::ComputedGridTrackInfo* aTrackInfo);

 protected:
  RefPtr<GridDimension> mParent;
  nsTArray<RefPtr<GridTrack>> mTracks;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_GridTracks_h */
