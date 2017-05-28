/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RubyUtils.h"
#include "nsRubyFrame.h"
#include "nsRubyBaseFrame.h"
#include "nsRubyTextFrame.h"
#include "nsRubyBaseContainerFrame.h"
#include "nsRubyTextContainerFrame.h"

using namespace mozilla;

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(ReservedISize, nscoord)

/* static */ void
RubyUtils::SetReservedISize(nsIFrame* aFrame, nscoord aISize)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  aFrame->SetProperty(ReservedISize(), aISize);
}

/* static */ void
RubyUtils::ClearReservedISize(nsIFrame* aFrame)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  aFrame->RemoveProperty(ReservedISize());
}

/* static */ nscoord
RubyUtils::GetReservedISize(nsIFrame* aFrame)
{
  MOZ_ASSERT(IsExpandableRubyBox(aFrame));
  return aFrame->GetProperty(ReservedISize());
}

AutoRubyTextContainerArray::AutoRubyTextContainerArray(
  nsRubyBaseContainerFrame* aBaseContainer)
{
  for (nsIFrame* frame = aBaseContainer->GetNextSibling();
       frame && frame->IsRubyTextContainerFrame();
       frame = frame->GetNextSibling()) {
    AppendElement(static_cast<nsRubyTextContainerFrame*>(frame));
  }
}

nsIFrame*
RubyColumn::Iterator::operator*() const
{
  nsIFrame* frame;
  if (mIndex == -1) {
    frame = mColumn.mBaseFrame;
  } else {
    frame = mColumn.mTextFrames[mIndex];
  }
  MOZ_ASSERT(frame, "Frame here cannot be null");
  return frame;
}

void
RubyColumn::Iterator::SkipUntilExistingFrame()
{
  if (mIndex == -1) {
    if (mColumn.mBaseFrame) {
      return;
    }
    ++mIndex;
  }
  int32_t numTextFrames = mColumn.mTextFrames.Length();
  for (; mIndex < numTextFrames; ++mIndex) {
    if (mColumn.mTextFrames[mIndex]) {
      break;
    }
  }
}

RubySegmentEnumerator::RubySegmentEnumerator(nsRubyFrame* aRubyFrame)
{
  nsIFrame* frame = aRubyFrame->PrincipalChildList().FirstChild();
  MOZ_ASSERT(!frame || frame->IsRubyBaseContainerFrame());
  mBaseContainer = static_cast<nsRubyBaseContainerFrame*>(frame);
}

void
RubySegmentEnumerator::Next()
{
  MOZ_ASSERT(mBaseContainer);
  nsIFrame* frame = mBaseContainer->GetNextSibling();
  while (frame && !frame->IsRubyBaseContainerFrame()) {
    frame = frame->GetNextSibling();
  }
  mBaseContainer = static_cast<nsRubyBaseContainerFrame*>(frame);
}

RubyColumnEnumerator::RubyColumnEnumerator(
  nsRubyBaseContainerFrame* aBaseContainer,
  const AutoRubyTextContainerArray& aTextContainers)
  : mAtIntraLevelWhitespace(false)
{
  const uint32_t rtcCount = aTextContainers.Length();
  mFrames.SetCapacity(rtcCount + 1);

  nsIFrame* rbFrame = aBaseContainer->PrincipalChildList().FirstChild();
  MOZ_ASSERT(!rbFrame || rbFrame->IsRubyBaseFrame());
  mFrames.AppendElement(static_cast<nsRubyContentFrame*>(rbFrame));
  for (uint32_t i = 0; i < rtcCount; i++) {
    nsRubyTextContainerFrame* container = aTextContainers[i];
    // If the container is for span, leave a nullptr here.
    // Spans do not take part in pairing.
    nsIFrame* rtFrame = !container->IsSpanContainer() ?
      container->PrincipalChildList().FirstChild() : nullptr;
    MOZ_ASSERT(!rtFrame || rtFrame->IsRubyTextFrame());
    mFrames.AppendElement(static_cast<nsRubyContentFrame*>(rtFrame));
  }

  // We have to init mAtIntraLevelWhitespace to be correct for the
  // first column. There are two ways we could end up with intra-level
  // whitespace in our first colum:
  // 1. The current segment itself is an inter-segment whitespace;
  // 2. If our ruby segment is split across multiple lines, and some
  //    intra-level whitespace happens to fall right after a line-break.
  //    Each line will get its own nsRubyBaseContainerFrame, and the
  //    container right after the line-break will end up with its first
  //    column containing that intra-level whitespace.
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* frame = mFrames[i];
    if (frame && frame->IsIntraLevelWhitespace()) {
      mAtIntraLevelWhitespace = true;
      break;
    }
  }
}

void
RubyColumnEnumerator::Next()
{
  bool advancingToIntraLevelWhitespace = false;
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* frame = mFrames[i];
    // If we've got intra-level whitespace frames at some levels in the
    // current ruby column, we "faked" an anonymous box for all other
    // levels for this column. So when we advance off this column, we
    // don't advance any of the frames in those levels, because we're
    // just advancing across the "fake" frames.
    if (frame && (!mAtIntraLevelWhitespace ||
                  frame->IsIntraLevelWhitespace())) {
      nsIFrame* nextSibling = frame->GetNextSibling();
      MOZ_ASSERT(!nextSibling || nextSibling->Type() == frame->Type(),
                 "Frame type should be identical among a level");
      mFrames[i] = frame = static_cast<nsRubyContentFrame*>(nextSibling);
      if (!advancingToIntraLevelWhitespace &&
          frame && frame->IsIntraLevelWhitespace()) {
        advancingToIntraLevelWhitespace = true;
      }
    }
  }
  MOZ_ASSERT(!advancingToIntraLevelWhitespace || !mAtIntraLevelWhitespace,
             "Should never have adjacent intra-level whitespace columns");
  mAtIntraLevelWhitespace = advancingToIntraLevelWhitespace;
}

bool
RubyColumnEnumerator::AtEnd() const
{
  for (uint32_t i = 0, iend = mFrames.Length(); i < iend; i++) {
    if (mFrames[i]) {
      return false;
    }
  }
  return true;
}

nsRubyContentFrame*
RubyColumnEnumerator::GetFrameAtLevel(uint32_t aIndex) const
{
  // If the current ruby column is for intra-level whitespaces, we
  // return nullptr for any levels that do not have an actual intra-
  // level whitespace frame in this column.  This nullptr represents
  // an anonymous empty intra-level whitespace box.  (In this case,
  // it's important that we NOT return mFrames[aIndex], because it's
  // really part of the next column, not the current one.)
  nsRubyContentFrame* frame = mFrames[aIndex];
  return !mAtIntraLevelWhitespace ||
         (frame && frame->IsIntraLevelWhitespace()) ? frame : nullptr;
}

void
RubyColumnEnumerator::GetColumn(RubyColumn& aColumn) const
{
  nsRubyContentFrame* rbFrame = GetFrameAtLevel(0);
  MOZ_ASSERT(!rbFrame || rbFrame->IsRubyBaseFrame());
  aColumn.mBaseFrame = static_cast<nsRubyBaseFrame*>(rbFrame);
  aColumn.mTextFrames.ClearAndRetainStorage();
  for (uint32_t i = 1, iend = mFrames.Length(); i < iend; i++) {
    nsRubyContentFrame* rtFrame = GetFrameAtLevel(i);
    MOZ_ASSERT(!rtFrame || rtFrame->IsRubyTextFrame());
    aColumn.mTextFrames.AppendElement(static_cast<nsRubyTextFrame*>(rtFrame));
  }
  aColumn.mIsIntraLevelWhitespace = mAtIntraLevelWhitespace;
}
