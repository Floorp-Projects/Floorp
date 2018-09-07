/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ColumnSetWrapperFrame.h"

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
  MOZ_ASSERT_UNREACHABLE("Unsupported operation!");
  nsBlockFrame::AppendFrames(aListID, aFrameList);
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
