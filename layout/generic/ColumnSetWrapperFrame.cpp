/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ColumnSetWrapperFrame.h"

#include "mozilla/ColumnUtils.h"
#include "mozilla/PresShell.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"

using namespace mozilla;

nsBlockFrame* NS_NewColumnSetWrapperFrame(PresShell* aPresShell,
                                          ComputedStyle* aStyle,
                                          nsFrameState aStateFlags) {
  ColumnSetWrapperFrame* frame = new (aPresShell)
      ColumnSetWrapperFrame(aStyle, aPresShell->GetPresContext());

  // CSS Multi-column level 1 section 2: A multi-column container
  // establishes a new block formatting context, as per CSS 2.1 section
  // 9.4.1.
  frame->AddStateBits(aStateFlags | NS_BLOCK_FORMATTING_CONTEXT_STATE_BITS);
  return frame;
}

NS_IMPL_FRAMEARENA_HELPERS(ColumnSetWrapperFrame)

NS_QUERYFRAME_HEAD(ColumnSetWrapperFrame)
  NS_QUERYFRAME_ENTRY(ColumnSetWrapperFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

ColumnSetWrapperFrame::ColumnSetWrapperFrame(ComputedStyle* aStyle,
                                             nsPresContext* aPresContext)
    : nsBlockFrame(aStyle, aPresContext, kClassID) {}

void ColumnSetWrapperFrame::Init(nsIContent* aContent,
                                 nsContainerFrame* aParent,
                                 nsIFrame* aPrevInFlow) {
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  // ColumnSetWrapperFrame doesn't need to call ResolveBidi().
  RemoveStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
}

nsContainerFrame* ColumnSetWrapperFrame::GetContentInsertionFrame() {
  nsIFrame* columnSet = PrincipalChildList().OnlyChild();
  if (columnSet) {
    // We have only one child, which means we don't have any column-span
    // descendants. Thus we can safely return our only ColumnSet child's
    // insertion frame as ours.
    MOZ_ASSERT(columnSet->IsColumnSetFrame());
    return columnSet->GetContentInsertionFrame();
  }

  // We have column-span descendants. Return ourselves as the insertion
  // frame to let nsCSSFrameConstructor::WipeContainingBlock() figure out
  // what to do.
  return this;
}

void ColumnSetWrapperFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(!GetPrevContinuation(),
             "Who set NS_FRAME_OWNS_ANON_BOXES on our continuations?");

  // It's sufficient to append the first ColumnSet child, which is the first
  // continuation of all the other ColumnSets.
  //
  // We don't need to append -moz-column-span-wrapper children because
  // they're non-inheriting anon boxes, and they cannot have any directly
  // owned anon boxes nor generate any native anonymous content themselves.
  // Thus, no need to restyle them. AssertColumnSpanWrapperSubtreeIsSane()
  // asserts all the conditions above which allow us to skip appending
  // -moz-column-span-wrappers.
  auto FindFirstChildInChildLists = [this]() -> nsIFrame* {
    const ChildListID listIDs[] = {kPrincipalList, kOverflowList};
    for (nsIFrame* frag = this; frag; frag = frag->GetNextInFlow()) {
      for (ChildListID id : listIDs) {
        const nsFrameList& list = frag->GetChildList(id);
        if (nsIFrame* firstChild = list.FirstChild()) {
          return firstChild;
        }
      }
    }
    return nullptr;
  };

  nsIFrame* columnSet = FindFirstChildInChildLists();
  MOZ_ASSERT(columnSet && columnSet->IsColumnSetFrame(),
             "The first child should always be ColumnSet!");
  aResult.AppendElement(OwnedAnonBox(columnSet));
}

#ifdef DEBUG_FRAME_DUMP
nsresult ColumnSetWrapperFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"ColumnSetWrapper"_ns, aResult);
}
#endif

// Disallow any append, insert, or remove operations after building the
// column hierarchy since any change to the column hierarchy in the column
// sub-tree need to be re-created.
void ColumnSetWrapperFrame::AppendFrames(ChildListID aListID,
                                         nsFrameList& aFrameList) {
#ifdef DEBUG
  MOZ_ASSERT(!mFinishedBuildingColumns, "Should only call once!");
  mFinishedBuildingColumns = true;
#endif

  nsBlockFrame::AppendFrames(aListID, aFrameList);

#ifdef DEBUG
  nsIFrame* firstColumnSet = PrincipalChildList().FirstChild();
  for (nsIFrame* child : PrincipalChildList()) {
    if (child->IsColumnSpan()) {
      AssertColumnSpanWrapperSubtreeIsSane(child);
    } else if (child != firstColumnSet) {
      // All the other ColumnSets are the continuation of the first ColumnSet.
      MOZ_ASSERT(child->IsColumnSetFrame() && child->GetPrevContinuation(),
                 "ColumnSet's prev-continuation is not set properly?");
    }
  }
#endif
}

void ColumnSetWrapperFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList& aFrameList) {
  MOZ_ASSERT_UNREACHABLE("Unsupported operation!");
  nsBlockFrame::InsertFrames(aListID, aPrevFrame, aPrevFrameLine, aFrameList);
}

void ColumnSetWrapperFrame::RemoveFrame(ChildListID aListID,
                                        nsIFrame* aOldFrame) {
  MOZ_ASSERT_UNREACHABLE("Unsupported operation!");
  nsBlockFrame::RemoveFrame(aListID, aOldFrame);
}

void ColumnSetWrapperFrame::MarkIntrinsicISizesDirty() {
  nsBlockFrame::MarkIntrinsicISizesDirty();

  // The parent's method adds NS_BLOCK_NEEDS_BIDI_RESOLUTION to all our
  // continuations. Clear the bit because we don't want to call ResolveBidi().
  for (nsIFrame* f = FirstContinuation(); f; f = f->GetNextContinuation()) {
    f->RemoveStateBits(NS_BLOCK_NEEDS_BIDI_RESOLUTION);
  }
}

nscoord ColumnSetWrapperFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord iSize = 0;
  DISPLAY_MIN_INLINE_SIZE(this, iSize);

  if (StyleDisplay()->IsContainSize()) {
    // If we're size-contained, we determine our minimum intrinsic size purely
    // from our column styling, as if we had no descendants. This should match
    // what happens in nsColumnSetFrame::GetMinISize in an actual no-descendants
    // scenario.
    const nsStyleColumn* colStyle = StyleColumn();
    if (colStyle->mColumnWidth.IsLength()) {
      // As available inline size reduces to zero, our number of columns reduces
      // to one, so no column gaps contribute to our minimum intrinsic size.
      // Also, column-width doesn't set a lower bound on our minimum intrinsic
      // size, either. Just use 0 because we're size-contained.
      iSize = 0;
    } else {
      MOZ_ASSERT(colStyle->mColumnCount != nsStyleColumn::kColumnCountAuto,
                 "column-count and column-width can't both be auto!");
      // As available inline size reduces to zero, we still have mColumnCount
      // columns, so compute our minimum intrinsic size based on N zero-width
      // columns, with specified gap size between them.
      const nscoord colGap =
          ColumnUtils::GetColumnGap(this, NS_UNCONSTRAINEDSIZE);
      iSize = ColumnUtils::IntrinsicISize(colStyle->mColumnCount, colGap, 0);
    }
  } else {
    for (nsIFrame* f : PrincipalChildList()) {
      iSize = std::max(iSize, f->GetMinISize(aRenderingContext));
    }
  }

  return iSize;
}

nscoord ColumnSetWrapperFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord iSize = 0;
  DISPLAY_PREF_INLINE_SIZE(this, iSize);

  if (StyleDisplay()->IsContainSize()) {
    const nsStyleColumn* colStyle = StyleColumn();
    nscoord colISize;
    if (colStyle->mColumnWidth.IsLength()) {
      colISize =
          ColumnUtils::ClampUsedColumnWidth(colStyle->mColumnWidth.AsLength());
    } else {
      MOZ_ASSERT(colStyle->mColumnCount != nsStyleColumn::kColumnCountAuto,
                 "column-count and column-width can't both be auto!");
      colISize = 0;
    }

    // If column-count is auto, assume one column.
    const uint32_t numColumns =
        colStyle->mColumnCount == nsStyleColumn::kColumnCountAuto
            ? 1
            : colStyle->mColumnCount;
    const nscoord colGap =
        ColumnUtils::GetColumnGap(this, NS_UNCONSTRAINEDSIZE);
    iSize = ColumnUtils::IntrinsicISize(numColumns, colGap, colISize);
  } else {
    for (nsIFrame* f : PrincipalChildList()) {
      iSize = std::max(iSize, f->GetPrefISize(aRenderingContext));
    }
  }

  return iSize;
}

#ifdef DEBUG

/* static */
void ColumnSetWrapperFrame::AssertColumnSpanWrapperSubtreeIsSane(
    const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->IsColumnSpan(), "aFrame is not column-span?");

  if (!nsLayoutUtils::GetStyleFrame(const_cast<nsIFrame*>(aFrame))
           ->Style()
           ->IsAnonBox()) {
    // aFrame's style frame has "column-span: all". Traverse no further.
    return;
  }

  MOZ_ASSERT(
      aFrame->Style()->GetPseudoType() == PseudoStyleType::columnSpanWrapper,
      "aFrame should be ::-moz-column-span-wrapper");

  MOZ_ASSERT(!aFrame->HasAnyStateBits(NS_FRAME_OWNS_ANON_BOXES),
             "::-moz-column-span-wrapper anonymous blocks cannot own "
             "other types of anonymous blocks!");

  for (const nsIFrame* child : aFrame->PrincipalChildList()) {
    AssertColumnSpanWrapperSubtreeIsSane(child);
  }
}

#endif
