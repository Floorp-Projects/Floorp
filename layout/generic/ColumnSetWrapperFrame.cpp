/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ColumnSetWrapperFrame.h"

#include "nsContentUtils.h"

using namespace mozilla;

nsBlockFrame*
NS_NewColumnSetWrapperFrame(nsIPresShell* aPresShell,
                            ComputedStyle* aStyle,
                            nsFrameState aStateFlags)
{
  ColumnSetWrapperFrame* frame = new (aPresShell) ColumnSetWrapperFrame(aStyle);

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

ColumnSetWrapperFrame::ColumnSetWrapperFrame(ComputedStyle* aStyle)
  : nsBlockFrame(aStyle, kClassID)
{
}

nsContainerFrame*
ColumnSetWrapperFrame::GetContentInsertionFrame()
{
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

void
ColumnSetWrapperFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult)
{
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
  nsIFrame* columnSet = PrincipalChildList().FirstChild();
  MOZ_ASSERT(columnSet && columnSet->IsColumnSetFrame(),
             "The first child should always be ColumnSet!");
  aResult.AppendElement(OwnedAnonBox(columnSet));
}

#ifdef DEBUG_FRAME_DUMP
nsresult
ColumnSetWrapperFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ColumnSetWrapper"), aResult);
}
#endif

// Disallow any append, insert, or remove operations after building the
// column hierarchy since any change to the column hierarchy in the column
// sub-tree need to be re-created.
void
ColumnSetWrapperFrame::AppendFrames(ChildListID aListID,
                                    nsFrameList& aFrameList)
{
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

void
ColumnSetWrapperFrame::InsertFrames(ChildListID aListID,
                                    nsIFrame* aPrevFrame,
                                    nsFrameList& aFrameList)
{
  MOZ_ASSERT_UNREACHABLE("Unsupported operation!");
  nsBlockFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

void
ColumnSetWrapperFrame::RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame)
{
  MOZ_ASSERT_UNREACHABLE("Unsupported operation!");
  nsBlockFrame::RemoveFrame(aListID, aOldFrame);
}

#ifdef DEBUG

/* static */ void
ColumnSetWrapperFrame::AssertColumnSpanWrapperSubtreeIsSane(
  const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->IsColumnSpan(), "aFrame is not column-span?");

  if (!aFrame->Style()->IsAnonBox()) {
    // aFrame is the primary frame of the element having "column-span: all".
    // Traverse no further.
    return;
  }

  MOZ_ASSERT(aFrame->Style()->GetPseudo() == nsCSSAnonBoxes::columnSpanWrapper(),
             "aFrame should be ::-moz-column-span-wrapper");

  MOZ_ASSERT(!aFrame->HasAnyStateBits(NS_FRAME_OWNS_ANON_BOXES),
             "::-moz-column-span-wrapper anonymous blocks cannot own "
             "other types of anonymous blocks!");

  nsTArray<nsIContent*> anonKids;
  nsContentUtils::AppendNativeAnonymousChildren(
    aFrame->GetContent(), anonKids, 0);
  MOZ_ASSERT(anonKids.IsEmpty(),
             "We support only column-span on block and inline frame. They "
             "should not create any native anonymous children.");

  for (const nsIFrame* child : aFrame->PrincipalChildList()) {
    AssertColumnSpanWrapperSubtreeIsSane(child);
  }
}

#endif
