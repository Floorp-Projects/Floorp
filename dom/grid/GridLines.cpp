/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridLines.h"

#include "GridDimension.h"
#include "GridLine.h"
#include "mozilla/dom/GridBinding.h"
#include "mozilla/dom/GridArea.h"
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

GridLines::GridLines(GridDimension* aParent) : mParent(aParent) {
  MOZ_ASSERT(aParent, "Should never be instantiated with a null GridDimension");
}

GridLines::~GridLines() = default;

JSObject* GridLines::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return GridLines_Binding::Wrap(aCx, this, aGivenProto);
}

uint32_t GridLines::Length() const { return mLines.Length(); }

GridLine* GridLines::Item(uint32_t aIndex) {
  return mLines.SafeElementAt(aIndex);
}

GridLine* GridLines::IndexedGetter(uint32_t aIndex, bool& aFound) {
  aFound = aIndex < mLines.Length();
  if (!aFound) {
    return nullptr;
  }
  return mLines[aIndex];
}

static void AddLineNameIfNotPresent(nsTArray<RefPtr<nsAtom>>& aLineNames,
                                    nsAtom* aName) {
  if (!aLineNames.Contains(aName)) {
    aLineNames.AppendElement(aName);
  }
}

static void AddLineNamesIfNotPresent(nsTArray<RefPtr<nsAtom>>& aLineNames,
                                     const nsTArray<RefPtr<nsAtom>>& aNames) {
  for (const auto& name : aNames) {
    AddLineNameIfNotPresent(aLineNames, name);
  }
}

void GridLines::SetLineInfo(const ComputedGridTrackInfo* aTrackInfo,
                            const ComputedGridLineInfo* aLineInfo,
                            const nsTArray<RefPtr<GridArea>>& aAreas,
                            bool aIsRow) {
  MOZ_ASSERT(aLineInfo);
  mLines.Clear();

  if (!aTrackInfo) {
    return;
  }

  uint32_t lineCount =
      aTrackInfo->mEndFragmentTrack - aTrackInfo->mStartFragmentTrack + 1;

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
    uint32_t leadingTrackCount =
        aTrackInfo->mNumLeadingImplicitTracks + aTrackInfo->mNumExplicitTracks;
    if (numRepeatTracks > 0) {
      for (auto& removedTrack : aTrackInfo->mRemovedRepeatTracks) {
        if (removedTrack) {
          ++leadingTrackCount;
        }
      }
    }

    for (uint32_t i = aTrackInfo->mStartFragmentTrack;
         i < aTrackInfo->mEndFragmentTrack + 1; i++) {
      // Since line indexes are 1-based, calculate a 1-based value
      // for this track to simplify some calculations.
      const uint32_t line1Index = i + 1;

      startOfNextTrack = (i < aTrackInfo->mEndFragmentTrack)
                             ? aTrackInfo->mPositions[i]
                             : lastTrackEdge;

      // Get the line names for the current line. aLineInfo->mNames
      // may contain duplicate names. This is intentional, since grid
      // layout works fine with duplicate names, and we don't want to
      // detect and remove duplicates in layout since it is an O(n^2)
      // problem. We do the work here since this is only run when
      // requested by devtools, and slowness here will not affect
      // normal browsing.
      const nsTArray<RefPtr<nsAtom>>& possiblyDuplicateLineNames(
          aLineInfo->mNames.SafeElementAt(i, nsTArray<RefPtr<nsAtom>>()));

      nsTArray<RefPtr<nsAtom>> lineNames;
      AddLineNamesIfNotPresent(lineNames, possiblyDuplicateLineNames);

      // Add in names from grid areas where this line is used as a boundary.
      for (auto area : aAreas) {
        // We specifically ignore line names from implicitly named areas,
        // because it can be confusing for designers who might naturally use
        // a named line of "-start" or "-end" and create an implicit named
        // area without meaning to.
        if (area->Type() == GridDeclaration::Implicit) {
          continue;
        }

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

        if (haveNameToAdd) {
          RefPtr<nsAtom> name = NS_Atomize(nameToAdd);
          AddLineNameIfNotPresent(lineNames, name);
        }
      }

      if (i >= (aTrackInfo->mRepeatFirstTrack +
                aTrackInfo->mNumLeadingImplicitTracks) &&
          repeatIndex < numRepeatTracks) {
        numAddedLines += AppendRemovedAutoFits(
            aTrackInfo, aLineInfo, lastTrackEdge, repeatIndex, numRepeatTracks,
            leadingTrackCount, lineNames);
      }

      // If this line is the one that ends a repeat, then add
      // in the mNamesFollowingRepeat names from aLineInfo.
      if (numRepeatTracks > 0 && i == (aTrackInfo->mRepeatFirstTrack +
                                       aTrackInfo->mNumLeadingImplicitTracks +
                                       numRepeatTracks - numAddedLines)) {
        AddLineNamesIfNotPresent(lineNames, aLineInfo->mNamesFollowingRepeat);
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
      uint32_t lineNumber = isBeforeFirstExplicit
                                ? 0
                                : (line1Index + numAddedLines -
                                   aTrackInfo->mNumLeadingImplicitTracks);

      // The negativeNumber is counted back from the leadingTrackCount.
      int32_t lineNegativeNumber =
          isAfterLastExplicit
              ? 0
              : (line1Index + numAddedLines - (leadingTrackCount + 2));
      GridDeclaration lineType = (isBeforeFirstExplicit || isAfterLastExplicit)
                                     ? GridDeclaration::Implicit
                                     : GridDeclaration::Explicit;
      line->SetLineValues(
          lineNames, nsPresContext::AppUnitsToDoubleCSSPixels(lastTrackEdge),
          nsPresContext::AppUnitsToDoubleCSSPixels(startOfNextTrack -
                                                   lastTrackEdge),
          lineNumber, lineNegativeNumber, lineType);

      if (i < aTrackInfo->mEndFragmentTrack) {
        lastTrackEdge = aTrackInfo->mPositions[i] + aTrackInfo->mSizes[i];
      }
    }

    // Define a function that gets the mLines index for a given line number.
    // This is necessary since it's possible for a line number to not be
    // represented in mLines. If this is the case, then return  -1.
    const int32_t lineCount = mLines.Length();
    const uint32_t lastLineNumber = mLines[lineCount - 1]->Number();
    auto IndexForLineNumber =
        [lineCount, lastLineNumber](uint32_t aLineNumber) -> int32_t {
      if (lastLineNumber == 0) {
        // None of the lines have addressable numbers, so none of them can have
        // aLineNumber
        return -1;
      }

      int32_t possibleIndex = (int32_t)aLineNumber - 1;
      if (possibleIndex < 0 || possibleIndex > lineCount - 1) {
        // aLineNumber is not represented in mLines.
        return -1;
      }

      return possibleIndex;
    };

    // Post-processing loop for implicit grid areas.
    for (const auto& area : aAreas) {
      if (area->Type() == GridDeclaration::Implicit) {
        // Get the appropriate indexes for the area's start and end lines as
        // they are represented in mLines.
        int32_t startIndex =
            IndexForLineNumber(aIsRow ? area->RowStart() : area->ColumnStart());
        int32_t endIndex =
            IndexForLineNumber(aIsRow ? area->RowEnd() : area->ColumnEnd());

        // If both start and end indexes are -1, then stop here since we cannot
        // reason about the naming for either lines.
        if (startIndex < 0 && endIndex < 0) {
          break;
        }

        // Get the "-start" and "-end" line names of the grid area.
        nsAutoString startLineName;
        area->GetName(startLineName);
        startLineName.AppendLiteral("-start");
        nsAutoString endLineName;
        area->GetName(endLineName);
        endLineName.AppendLiteral("-end");

        // Get the list of existing line names for the start and end of the grid
        // area. In the case where one of the start or end indexes are -1, use a
        // dummy line as a substitute for the start/end line.
        RefPtr<GridLine> dummyLine = new GridLine(this);
        RefPtr<GridLine> areaStartLine =
            startIndex > -1 ? mLines[startIndex] : dummyLine;
        nsTArray<RefPtr<nsAtom>> startLineNames(areaStartLine->Names().Clone());

        RefPtr<GridLine> areaEndLine =
            endIndex > -1 ? mLines[endIndex] : dummyLine;
        nsTArray<RefPtr<nsAtom>> endLineNames(areaEndLine->Names().Clone());

        RefPtr<nsAtom> start = NS_Atomize(startLineName);
        RefPtr<nsAtom> end = NS_Atomize(endLineName);
        if (startLineNames.Contains(end) || endLineNames.Contains(start)) {
          // Add the reversed line names.
          AddLineNameIfNotPresent(startLineNames, end);
          AddLineNameIfNotPresent(endLineNames, start);
        } else {
          // Add the normal line names.
          AddLineNameIfNotPresent(startLineNames, start);
          AddLineNameIfNotPresent(endLineNames, end);
        }

        areaStartLine->SetLineNames(startLineNames);
        areaEndLine->SetLineNames(endLineNames);
      }
    }
  }
}

uint32_t GridLines::AppendRemovedAutoFits(
    const ComputedGridTrackInfo* aTrackInfo,
    const ComputedGridLineInfo* aLineInfo, nscoord aLastTrackEdge,
    uint32_t& aRepeatIndex, uint32_t aNumRepeatTracks,
    uint32_t aNumLeadingTracks, nsTArray<RefPtr<nsAtom>>& aLineNames) {
  bool extractedExplicitLineNames = false;
  nsTArray<RefPtr<nsAtom>> explicitLineNames;
  uint32_t linesAdded = 0;
  while (aRepeatIndex < aNumRepeatTracks &&
         aTrackInfo->mRemovedRepeatTracks[aRepeatIndex]) {
    // If this is not the very first call to this function, and if we
    // haven't already added a line this call, pull all the explicit
    // names to pass along to the next line that will be added after
    // this function completes.
    if (aRepeatIndex > 0 && linesAdded == 0) {
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

    AddLineNamesIfNotPresent(aLineNames, aLineInfo->mNamesBefore);

    RefPtr<GridLine> line = new GridLine(this);
    mLines.AppendElement(line);

    // Time to calculate the line numbers. For the positive numbers
    // we count with a 1-based index from mRepeatFirstTrack. Although
    // this number is the index of the first repeat track AFTER all
    // the leading implicit tracks, that's still what we want since
    // all those leading implicit tracks have line number 0.
    uint32_t lineNumber = aTrackInfo->mRepeatFirstTrack + aRepeatIndex + 1;

    // The negative number does have to account for the leading
    // implicit tracks. We've been passed aNumLeadingTracks which is
    // the total of the leading implicit tracks plus the explicit
    // tracks. So all we have to do is subtract that number plus one
    // from the 0-based index of this track.
    int32_t lineNegativeNumber =
        (aTrackInfo->mNumLeadingImplicitTracks + aTrackInfo->mRepeatFirstTrack +
         aRepeatIndex) -
        (aNumLeadingTracks + 1);
    line->SetLineValues(
        aLineNames, nsPresContext::AppUnitsToDoubleCSSPixels(aLastTrackEdge),
        nsPresContext::AppUnitsToDoubleCSSPixels(0), lineNumber,
        lineNegativeNumber, GridDeclaration::Explicit);

    // No matter what, the next line should have the after names associated
    // with it. If we go through the loop again, the before names will also
    // be added.
    aLineNames = aLineInfo->mNamesAfter.Clone();
    aRepeatIndex++;

    linesAdded++;
  }

  aRepeatIndex++;

  if (extractedExplicitLineNames) {
    // Pass on the explicit names we saved to the next explicit line.
    AddLineNamesIfNotPresent(aLineNames, explicitLineNames);
  }

  // If we haven't finished adding auto-repeated tracks, then we need to put
  // back the before names, in case we cleared them above.
  if (aRepeatIndex < aNumRepeatTracks) {
    AddLineNamesIfNotPresent(aLineNames, aLineInfo->mNamesBefore);
  }

  return linesAdded;
}

}  // namespace dom
}  // namespace mozilla
