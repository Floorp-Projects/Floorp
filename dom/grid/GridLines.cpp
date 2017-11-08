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
  MOZ_ASSERT(aLineInfo);
  mLines.Clear();

  if (!aTrackInfo) {
    return;
  }

  uint32_t lineCount = aTrackInfo->mEndFragmentTrack -
                       aTrackInfo->mStartFragmentTrack +
                       1;

  // If there is at least one track, line count is one more
  // than the number of tracks.
  if (lineCount > 0) {
    nscoord lastTrackEdge = 0;
    nscoord startOfNextTrack;
    uint32_t repeatIndex = 0;
    uint32_t numRepeatTracks = aTrackInfo->mRemovedRepeatTracks.Length();
    uint32_t numAddedLines = 0;

    // For the calculation of negative line numbers, we need to know
    // the total number of leading implicit and explicit tracks.
    // This might be different from the number of tracks sizes in
    // aTrackInfo, because some of those tracks may be auto-fits that
    // have been removed.
    uint32_t leadingTrackCount = aTrackInfo->mNumLeadingImplicitTracks +
                                 aTrackInfo->mNumExplicitTracks;
    if (numRepeatTracks > 0) {
      for (auto& removedTrack : aTrackInfo->mRemovedRepeatTracks) {
        if (removedTrack) {
          ++leadingTrackCount;
        }
      }
    }

    for (uint32_t i = aTrackInfo->mStartFragmentTrack;
         i < aTrackInfo->mEndFragmentTrack + 1;
         i++) {
      // Since line indexes are 1-based, calculate a 1-based value
      // for this track to simplify some calculations.
      const uint32_t line1Index = i + 1;

      startOfNextTrack = (i < aTrackInfo->mEndFragmentTrack) ?
                         aTrackInfo->mPositions[i] :
                         lastTrackEdge;

      nsTArray<nsString> lineNames;
      lineNames = aLineInfo->mNames.SafeElementAt(i, nsTArray<nsString>());

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

      if (i >= (aTrackInfo->mRepeatFirstTrack +
                aTrackInfo->mNumLeadingImplicitTracks) &&
          repeatIndex < numRepeatTracks) {
        numAddedLines += AppendRemovedAutoFits(aTrackInfo,
                                               aLineInfo,
                                               lastTrackEdge,
                                               repeatIndex,
                                               numRepeatTracks,
                                               leadingTrackCount,
                                               lineNames);
      }

      RefPtr<GridLine> line = new GridLine(this);
      mLines.AppendElement(line);
      MOZ_ASSERT(line1Index > 0, "line1Index must be positive.");
      bool isBeforeFirstExplicit =
        (line1Index <= aTrackInfo->mNumLeadingImplicitTracks);
      bool isAfterLastExplicit = line1Index > (leadingTrackCount + 1);
      // Calculate an actionable line number for this line, that could be used
      // in a css grid property to align a grid item or area at that line.
      // For implicit lines that appear before line 1, report a number of 0.
      // We can't report negative indexes, because those have a different
      // meaning in the css grid spec (negative indexes are negative-1-based
      // from the end of the grid decreasing towards the front).
      uint32_t lineNumber = isBeforeFirstExplicit ? 0 :
        (line1Index + numAddedLines - aTrackInfo->mNumLeadingImplicitTracks);

      // The negativeNumber is counted back from the leadingTrackCount.
      int32_t lineNegativeNumber = isAfterLastExplicit ? 0 :
        (line1Index + numAddedLines - (leadingTrackCount + 2));
      GridDeclaration lineType =
        (isBeforeFirstExplicit || isAfterLastExplicit)
         ? GridDeclaration::Implicit
         : GridDeclaration::Explicit;
      line->SetLineValues(
        lineNames,
        nsPresContext::AppUnitsToDoubleCSSPixels(lastTrackEdge),
        nsPresContext::AppUnitsToDoubleCSSPixels(startOfNextTrack -
                                                 lastTrackEdge),
        lineNumber,
        lineNegativeNumber,
        lineType
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
                                 uint32_t aNumLeadingTracks,
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

    // Time to calculate the line numbers. For the positive numbers
    // we count with a 1-based index from mRepeatFirstTrack. Although
    // this number is the index of the first repeat track AFTER all
    // the leading implicit tracks, that's still what we want since
    // all those leading implicit tracks have line number 0.
    uint32_t lineNumber = aTrackInfo->mRepeatFirstTrack +
                          aRepeatIndex + 1;

    // The negative number does have to account for the leading
    // implicit tracks. We've been passed aNumLeadingTracks which is
    // the total of the leading implicit tracks plus the explicit
    // tracks. So all we have to do is subtract that number plus one
    // from the 0-based index of this track.
    int32_t lineNegativeNumber = (aTrackInfo->mNumLeadingImplicitTracks +
                                  aTrackInfo->mRepeatFirstTrack +
                                  aRepeatIndex) - (aNumLeadingTracks + 1);
    line->SetLineValues(
      aLineNames,
      nsPresContext::AppUnitsToDoubleCSSPixels(aLastTrackEdge),
      nsPresContext::AppUnitsToDoubleCSSPixels(0),
      lineNumber,
      lineNegativeNumber,
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
