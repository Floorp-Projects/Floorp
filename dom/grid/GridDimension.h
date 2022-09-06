/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridDimension_h
#define mozilla_dom_GridDimension_h

#include "nsWrapperCache.h"
#include "nsTArray.h"

namespace mozilla {

struct ComputedGridLineInfo;
struct ComputedGridTrackInfo;

namespace dom {

class Grid;
class GridArea;
class GridLines;
class GridTracks;

class GridDimension : public nsISupports, public nsWrapperCache {
 public:
  explicit GridDimension(Grid* aParent);

 protected:
  virtual ~GridDimension();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GridDimension)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  Grid* GetParentObject() { return mParent; }

  GridLines* Lines() const;
  GridTracks* Tracks() const;

  void SetTrackInfo(const ComputedGridTrackInfo* aTrackInfo);
  void SetLineInfo(const ComputedGridTrackInfo* aTrackInfo,
                   const ComputedGridLineInfo* aLineInfo,
                   const nsTArray<RefPtr<GridArea>>& aAreas, bool aIsRow);

 protected:
  RefPtr<Grid> mParent;
  RefPtr<GridLines> mLines;
  RefPtr<GridTracks> mTracks;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_GridDimension_h */
