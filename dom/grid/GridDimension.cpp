/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridDimension.h"

#include "Grid.h"
#include "GridLines.h"
#include "GridTracks.h"
#include "mozilla/dom/GridBinding.h"
#include "nsGridContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridDimension, mParent, mLines, mTracks)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridDimension)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridDimension)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridDimension)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridDimension::GridDimension(Grid* aParent)
  : mParent(aParent)
  , mLines(new GridLines(this))
  , mTracks(new GridTracks(this))
{
  MOZ_ASSERT(aParent, "Should never be instantiated with a null Grid");
}

GridDimension::~GridDimension()
{
}

JSObject*
GridDimension::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridDimensionBinding::Wrap(aCx, this, aGivenProto);
}

GridLines*
GridDimension::Lines() const
{
  return mLines;
}

GridTracks*
GridDimension::Tracks() const
{
  return mTracks;
}

void
GridDimension::SetTrackInfo(const ComputedGridTrackInfo* aTrackInfo)
{
  mTracks->SetTrackInfo(aTrackInfo);
}

void
GridDimension::SetLineInfo(const ComputedGridTrackInfo* aTrackInfo,
                           const ComputedGridLineInfo* aLineInfo)
{
  mLines->SetLineInfo(aTrackInfo, aLineInfo);
}

} // namespace dom
} // namespace mozilla
