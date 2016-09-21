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
    nscoord lastTrackEdge = 0;
    nscoord startOfNextTrack;
    uint32_t repeatIndex = 0;
    uint32_t numRepeatTracks = aTrackInfo->mRemovedRepeatTracks.Length();
    uint32_t numAddedLines = 0;

    for (uint32_t i = aTrackInfo->mStartFragmentTrack;
         i < aTrackInfo->mEndFragmentTrack + 1;
         i++) {
      uint32_t line1Index = i + 1;

      startOfNextTrack = (i < aTrackInfo->mEndFragmentTrack) ?
                         aTrackInfo->mPositions[i] :
                         lastTrackEdge;

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

      if (i >= aTrackInfo->mRepeatFirstTrack &&
          repeatIndex < numRepeatTracks) {
        numAddedLines += AppendRemovedAutoFits(aTrackInfo,
                                               aLineInfo,
                                               lastTrackEdge,
                                               repeatIndex,
                                               numRepeatTracks,
                                               lineNames);
      }

      RefPtr<GridLine> line = new GridLine(this);
      mLines.AppendElement(line);
      line->SetLineValues(
        lineNames,
        nsPresContext::AppUnitsToDoubleCSSPixels(lastTrackEdge),
        nsPresContext::AppUnitsToDoubleCSSPixels(startOfNextTrack -
                                                 lastTrackEdge),
        line1Index + numAddedLines,
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
        lastTrackEdge = aTrackInfo->mPositions[i] + aTrackInfo->mSizes[i];
      }
    }
  }
}

uint32_t
GridLines::AppendRemovedAutoFits(const ComputedGridTrackInfo* aTrackInfo,
                                 const ComputedGridLineInfo* aLineInfo,
                                 nscoord aLastTrackEdge,
                                 uint32_t& aRepeatIndex,
                                 uint32_t aNumRepeatTracks,
                                 nsTArray<nsString>& aLineNames)
{
  // Check to see if lineNames contains ALL of the before line names.
  bool alreadyHasBeforeLineNames = true;
  for (const auto& beforeName : aLineInfo->mNamesBefore) {
    if (!aLineNames.Contains(beforeName)) {
      alreadyHasBeforeLineNames = false;
      break;
    }
  }

  bool extractedExplicitLineNames = false;
  nsTArray<nsString> explicitLineNames;
  uint32_t linesAdded = 0;
  while (aRepeatIndex < aNumRepeatTracks &&
         aTrackInfo->mRemovedRepeatTracks[aRepeatIndex]) {
    // If this is not the very first call to this function, and if we
    // haven't already added a line this call, pull all the explicit
    // names to pass along to the next line that will be added after
    // this function completes.
    if (aRepeatIndex > 0 &&
        linesAdded == 0) {
      // Find the names that didn't match the before or after names,
      // and extract them.
      for (const auto& name : aLineNames) {
        if (!aLineInfo->mNamesBefore.Contains(name) &&
            !aLineInfo->mNamesAfter.Contains(name)) {
          explicitLineNames.AppendElement(name);
        }
      }
      for (const auto& extractedName : explicitLineNames) {
        aLineNames.RemoveElement(extractedName);
      }
      extractedExplicitLineNames = true;
    }

    // If this is the second or later time through, or didn't already
    // have before names, add them.
    if (linesAdded > 0 || !alreadyHasBeforeLineNames) {
      aLineNames.AppendElements(aLineInfo->mNamesBefore);
    }

    RefPtr<GridLine> line = new GridLine(this);
    mLines.AppendElement(line);
    line->SetLineValues(
      aLineNames,
      nsPresContext::AppUnitsToDoubleCSSPixels(aLastTrackEdge),
      nsPresContext::AppUnitsToDoubleCSSPixels(0),
      aTrackInfo->mRepeatFirstTrack + aRepeatIndex + 1,
      GridDeclaration::Explicit
    );

    // No matter what, the next line should have the after names associated
    // with it. If we go through the loop again, the before names will also
    // be added.
    aLineNames = aLineInfo->mNamesAfter;
    aRepeatIndex++;

    linesAdded++;
  }
  aRepeatIndex++;

  if (extractedExplicitLineNames) {
    // Pass on the explicit names we saved to the next explicit line.
    aLineNames.AppendElements(explicitLineNames);
  }

  if (alreadyHasBeforeLineNames && linesAdded > 0) {
    // If we started with before names, pass them on to the next explicit
    // line.
    aLineNames.AppendElements(aLineInfo->mNamesBefore);
  }
  return linesAdded;
}

} // namespace dom
} // namespace mozilla
