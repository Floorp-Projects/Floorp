/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridLines_h
#define mozilla_dom_GridLines_h

#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GridDimension;
class GridLine;

class GridLines : public nsISupports
                , public nsWrapperCache
{
public:
  explicit GridLines(GridDimension* aParent);

protected:
  virtual ~GridLines();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GridLines)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  GridDimension* GetParentObject()
  {
    return mParent;
  }

  uint32_t Length() const;
  GridLine* Item(uint32_t aIndex);
  GridLine* IndexedGetter(uint32_t aIndex, bool& aFound);

  void SetLineInfo(const ComputedGridTrackInfo* aTrackInfo,
                   const ComputedGridLineInfo* aLineInfo,
                   const nsTArray<RefPtr<GridArea>>& aAreas,
                   bool aIsRow);

protected:
  uint32_t AppendRemovedAutoFits(const ComputedGridTrackInfo* aTrackInfo,
                                 const ComputedGridLineInfo* aLineInfo,
                                 nscoord aLastTrackEdge,
                                 uint32_t& aRepeatIndex,
                                 uint32_t aNumRepeatTracks,
                                 nsTArray<nsString>& aLineNames);

  RefPtr<GridDimension> mParent;
  nsTArray<RefPtr<GridLine>> mLines;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_GridLines_h */
