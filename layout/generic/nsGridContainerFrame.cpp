/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: grid | inline-grid" */

#include "nsGridContainerFrame.h"

#include "nsCSSAnonBoxes.h"
#include "nsPresContext.h"
#include "nsReadableUtils.h"
#include "nsStyleContext.h"

using namespace mozilla;

// Return true if aString ends in aSuffix and has at least one character before
// the suffix. Assign aIndex to where the suffix starts.
static bool
IsNameWithSuffix(const nsString& aString, const nsString& aSuffix,
                 uint32_t* aIndex)
{
  if (StringEndsWith(aString, aSuffix)) {
    *aIndex = aString.Length() - aSuffix.Length();
    return *aIndex != 0;
  }
  return false;
}

static bool
IsNameWithEndSuffix(const nsString& aString, uint32_t* aIndex)
{
  return IsNameWithSuffix(aString, NS_LITERAL_STRING("-end"), aIndex);
}

static bool
IsNameWithStartSuffix(const nsString& aString, uint32_t* aIndex)
{
  return IsNameWithSuffix(aString, NS_LITERAL_STRING("-start"), aIndex);
}

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsGridContainerFrame)
  NS_QUERYFRAME_ENTRY(nsGridContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsGridContainerFrame)

nsContainerFrame*
NS_NewGridContainerFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext)
{
  return new (aPresShell) nsGridContainerFrame(aContext);
}


//----------------------------------------------------------------------

// nsGridContainerFrame Method Implementations
// ===========================================

void
nsGridContainerFrame::AddImplicitNamedAreas(
  const nsTArray<nsTArray<nsString>>& aLineNameLists)
{
  // http://dev.w3.org/csswg/css-grid/#implicit-named-areas
  // XXX this just checks x-start .. x-end in one dimension and there's
  // no other error checking.  A few wrong cases (maybe):
  // (x-start x-end)
  // (x-start) 0 (x-start) 0 (x-end)
  // (x-end) 0 (x-start) 0 (x-end)
  // (x-start) 0 (x-end) 0 (x-start) 0 (x-end)
  const uint32_t len = aLineNameLists.Length();
  nsTHashtable<nsStringHashKey> currentStarts;
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  for (uint32_t i = 0; i < len; ++i) {
    const nsTArray<nsString>& names(aLineNameLists[i]);
    const uint32_t jLen = names.Length();
    for (uint32_t j = 0; j < jLen; ++j) {
      const nsString& name = names[j];
      uint32_t index;
      if (::IsNameWithStartSuffix(name, &index)) {
        currentStarts.PutEntry(nsDependentSubstring(name, 0, index));
      } else if (::IsNameWithEndSuffix(name, &index)) {
        nsDependentSubstring area(name, 0, index);
        if (currentStarts.Contains(area)) {
          if (!areas) {
            areas = new ImplicitNamedAreas;
            Properties().Set(ImplicitNamedAreasProperty(), areas);
          }
          areas->PutEntry(area);
        }
      }
    }
  }
}

void
nsGridContainerFrame::InitImplicitNamedAreas(const nsStylePosition* aStyle)
{
  ImplicitNamedAreas* areas = GetImplicitNamedAreas();
  if (areas) {
    // Clear it, but reuse the hashtable itself for now.  We'll remove it
    // below if it isn't needed anymore.
    areas->Clear();
  }
  AddImplicitNamedAreas(aStyle->mGridTemplateColumns.mLineNameLists);
  AddImplicitNamedAreas(aStyle->mGridTemplateRows.mLineNameLists);
  if (areas && areas->Count() == 0) {
    Properties().Delete(ImplicitNamedAreasProperty());
  }
}

void
nsGridContainerFrame::Reflow(nsPresContext*           aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGridContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (IsFrameTreeTooDeep(aReflowState, aDesiredSize, aStatus)) {
    return;
  }

#ifdef DEBUG
  SanityCheckAnonymousGridItems();
#endif // DEBUG

  LogicalMargin bp = aReflowState.ComputedLogicalBorderPadding();
  bp.ApplySkipSides(GetLogicalSkipSides());
  nscoord contentBSize = GetEffectiveComputedBSize(aReflowState);
  if (contentBSize == NS_AUTOHEIGHT) {
    contentBSize = 0;
  }
  WritingMode wm = aReflowState.GetWritingMode();
  LogicalSize finalSize(wm,
                        aReflowState.ComputedISize() + bp.IStartEnd(wm),
                        contentBSize + bp.BStartEnd(wm));
  aDesiredSize.SetSize(wm, finalSize);
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  const nsStylePosition* stylePos = aReflowState.mStylePosition;
  InitImplicitNamedAreas(stylePos);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
}

nsIAtom*
nsGridContainerFrame::GetType() const
{
  return nsGkAtoms::gridContainerFrame;
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsGridContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("GridContainer"), aResult);
}
#endif

#ifdef DEBUG
static bool
FrameWantsToBeInAnonymousGridItem(nsIFrame* aFrame)
{
  // Note: This needs to match the logic in
  // nsCSSFrameConstructor::FrameConstructionItem::NeedsAnonFlexOrGridItem()
  return (aFrame->IsFrameOfType(nsIFrame::eLineParticipant) ||
          nsGkAtoms::placeholderFrame == aFrame->GetType());
}

// Debugging method, to let us assert that our anonymous grid items are
// set up correctly -- in particular, we assert:
//  (1) we don't have any inline non-replaced children
//  (2) we don't have any consecutive anonymous grid items
//  (3) we don't have any empty anonymous grid items
//  (4) all children are on the expected child lists
void
nsGridContainerFrame::SanityCheckAnonymousGridItems() const
{
  // XXX handle kOverflowContainersList / kExcessOverflowContainersList
  // when we implement fragmentation?
  ChildListIDs noCheckLists = kAbsoluteList | kFixedList;
  ChildListIDs checkLists = kPrincipalList | kOverflowList;
  for (nsIFrame::ChildListIterator childLists(this);
       !childLists.IsDone(); childLists.Next()) {
    if (!checkLists.Contains(childLists.CurrentID())) {
      MOZ_ASSERT(noCheckLists.Contains(childLists.CurrentID()),
                 "unexpected non-empty child list");
      continue;
    }

    bool prevChildWasAnonGridItem = false;
    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      MOZ_ASSERT(!FrameWantsToBeInAnonymousGridItem(child),
                 "frame wants to be inside an anonymous grid item, "
                 "but it isn't");
      if (child->StyleContext()->GetPseudo() ==
            nsCSSAnonBoxes::anonymousGridItem) {
/*
  XXX haven't decided yet whether to reorder children or not.
  XXX If we do, we want this assertion instead of the one below.
        MOZ_ASSERT(!prevChildWasAnonGridItem ||
                   HasAnyStateBits(NS_STATE_GRID_CHILDREN_REORDERED),
                   "two anon grid items in a row (shouldn't happen, unless our "
                   "children have been reordered with the 'order' property)");
*/
        MOZ_ASSERT(!prevChildWasAnonGridItem, "two anon grid items in a row");
        nsIFrame* firstWrappedChild = child->GetFirstPrincipalChild();
        MOZ_ASSERT(firstWrappedChild,
                   "anonymous grid item is empty (shouldn't happen)");
        prevChildWasAnonGridItem = true;
      } else {
        prevChildWasAnonGridItem = false;
      }
    }
  }
}
#endif // DEBUG
