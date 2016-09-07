/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridLines.h"

#include "GridDimension.h"
#include "GridLine.h"
#include "mozilla/dom/GridBinding.h"
#include "nsGridContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridLines, mParent, mLines)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridLines)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridLines)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridLines)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridLines::GridLines(GridDimension* aParent)
  : mParent(aParent)
{
  MOZ_ASSERT(aParent,
    "Should never be instantiated with a null GridDimension");
}

GridLines::~GridLines()
{
}

JSObject*
GridLines::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridLinesBinding::Wrap(aCx, this, aGivenProto);
}

uint32_t
GridLines::Length() const
{
  return mLines.Length();
}

GridLine*
GridLines::Item(uint32_t aIndex)
{
  return mLines.SafeElementAt(aIndex);
}

GridLine*
GridLines::IndexedGetter(uint32_t aIndex,
                         bool& aFound)
{
  aFound = aIndex < mLines.Length();
  if (!aFound) {
    return nullptr;
  }
  return mLines[aIndex];
}

void
GridLines::SetLineInfo(const ComputedGridTrackInfo* aTrackInfo,
                       const ComputedGridLineInfo* aLineInfo,
                       const nsTArray<RefPtr<GridArea>>& aAreas,
                       bool aIsRow)
{
  mLines.Clear();

  if (!aTrackInfo) {
    return;
  }

  uint32_t trackCount = aTrackInfo->mEndFragmentTrack -
                        aTrackInfo->mStartFragmentTrack;

  // If there is at least one track, line count is one more
  // than the number of tracks.
  if (trackCount > 0) {
    double endOfLastTrack = 0.0;
    double startOfNextTrack;

    for (uint32_t i = aTrackInfo->mStartFragmentTrack;
         i < aTrackInfo->mEndFragmentTrack + 1;
         i++) {
      uint32_t line1Index = i + 1;

      startOfNextTrack = (i < aTrackInfo->mEndFragmentTrack) ?
                         aTrackInfo->mPositions[i] :
                         endOfLastTrack;

      GridLine* line = new GridLine(this);
      mLines.AppendElement(line);

      nsTArray<nsString> lineNames;
      if (aLineInfo) {
        lineNames = aLineInfo->mNames.SafeElementAt(i, nsTArray<nsString>());
      }

      // Add in names from grid areas where this line is used as a boundary.
      for (auto area : aAreas) {
        bool haveNameToAdd = false;
        nsAutoString nameToAdd;
        area->GetName(nameToAdd);
        if (aIsRow) {
          if (area->RowStart() == line1Index) {
            haveNameToAdd = true;
            nameToAdd.AppendLiteral("-start");
          } else if (area->RowEnd() == line1Index) {
            haveNameToAdd = true;
            nameToAdd.AppendLiteral("-end");
          }
        } else {
          if (area->ColumnStart() == line1Index) {
            haveNameToAdd = true;
            nameToAdd.AppendLiteral("-start");
          } else if (area->ColumnEnd() == line1Index) {
            haveNameToAdd = true;
            nameToAdd.AppendLiteral("-end");
          }
        }

        if (haveNameToAdd && !lineNames.Contains(nameToAdd)) {
          lineNames.AppendElement(nameToAdd);
        }
      }

      line->SetLineValues(
        lineNames,
        nsPresContext::AppUnitsToDoubleCSSPixels(endOfLastTrack),
        nsPresContext::AppUnitsToDoubleCSSPixels(startOfNextTrack -
                                                 endOfLastTrack),
        line1Index,
        (
          // Implicit if there are no explicit tracks, or if the index
          // is before the first explicit track, or after
          // a track beyond the last explicit track.
          (aTrackInfo->mNumExplicitTracks == 0) ||
          (i < aTrackInfo->mNumLeadingImplicitTracks) ||
          (i > aTrackInfo->mNumLeadingImplicitTracks +
               aTrackInfo->mNumExplicitTracks) ?
            GridDeclaration::Implicit :
            GridDeclaration::Explicit
        )
      );

      if (i < aTrackInfo->mEndFragmentTrack) {
        endOfLastTrack = aTrackInfo->mPositions[i] + aTrackInfo->mSizes[i];
      }
    }
  }
}

} // namespace dom
} // namespace mozilla
