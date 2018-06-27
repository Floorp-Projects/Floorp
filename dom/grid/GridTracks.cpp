/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridTracks.h"

#include "GridDimension.h"
#include "GridTrack.h"
#include "mozilla/dom/GridBinding.h"
#include "nsGridContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridTracks, mParent, mTracks)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridTracks)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridTracks)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridTracks)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridTracks::GridTracks(GridDimension *aParent)
  : mParent(aParent)
{
  MOZ_ASSERT(aParent,
    "Should never be instantiated with a null GridDimension");
}

GridTracks::~GridTracks()
{
}

JSObject*
GridTracks::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridTracks_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t
GridTracks::Length() const
{
  return mTracks.Length();
}

GridTrack*
GridTracks::Item(uint32_t aIndex)
{
  return mTracks.SafeElementAt(aIndex);
}

GridTrack*
GridTracks::IndexedGetter(uint32_t aIndex,
                          bool& aFound)
{
  aFound = aIndex < mTracks.Length();
  if (!aFound) {
    return nullptr;
  }
  return mTracks[aIndex];
}

void
GridTracks::SetTrackInfo(const ComputedGridTrackInfo* aTrackInfo)
{
  // rebuild the tracks based on aTrackInfo
  mTracks.Clear();

  if (!aTrackInfo) {
    return;
  }

  nscoord lastTrackEdge = 0;
  uint32_t repeatIndex = 0;
  auto AppendRemovedAutoFits = [this, &aTrackInfo, &lastTrackEdge,
                                &repeatIndex]()
  {
    uint32_t numRepeatTracks = aTrackInfo->mRemovedRepeatTracks.Length();
    // Add in removed auto-fit tracks
    while (repeatIndex < numRepeatTracks &&
         aTrackInfo->mRemovedRepeatTracks[repeatIndex]) {

      RefPtr<GridTrack> track = new GridTrack(this);
      mTracks.AppendElement(track);
      track->SetTrackValues(
        nsPresContext::AppUnitsToDoubleCSSPixels(lastTrackEdge),
        nsPresContext::AppUnitsToDoubleCSSPixels(0),
        GridDeclaration::Explicit,
        GridTrackState::Removed
      );
      repeatIndex++;
    }
    repeatIndex++;
  };

  for (size_t i = aTrackInfo->mStartFragmentTrack;
       i < aTrackInfo->mEndFragmentTrack;
       i++) {
    if (i >= aTrackInfo->mRepeatFirstTrack) {
      // Append removed auto-fit tracks, if appropriate. The
      // AppendRemovedAutoFits function exits early once it has been called
      // aTrackInfo->mRemovedRepeatTracks.Length() times -- a check we don't
      // replicate here or at subsequent calling sites.
      AppendRemovedAutoFits();
    }

    RefPtr<GridTrack> track = new GridTrack(this);
    mTracks.AppendElement(track);
    track->SetTrackValues(
      nsPresContext::AppUnitsToDoubleCSSPixels(aTrackInfo->mPositions[i]),
      nsPresContext::AppUnitsToDoubleCSSPixels(aTrackInfo->mSizes[i]),
      (
        // Implicit if index is before the first explicit track, or after
        // the last explicit track.
        (i < aTrackInfo->mNumLeadingImplicitTracks) ||
        (i >= aTrackInfo->mNumLeadingImplicitTracks +
              aTrackInfo->mNumExplicitTracks) ?
          GridDeclaration::Implicit :
          GridDeclaration::Explicit
      ),
      GridTrackState(aTrackInfo->mStates[i])
    );

    lastTrackEdge = aTrackInfo->mPositions[i] + aTrackInfo->mSizes[i];
  }

  // Append any trailing removed auto-fit tracks.
  AppendRemovedAutoFits();
}

} // namespace dom
} // namespace mozilla
