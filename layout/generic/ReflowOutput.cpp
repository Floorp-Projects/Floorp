/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the output from nsIFrame::Reflow */

#include "mozilla/ReflowOutput.h"
#include "mozilla/ReflowInput.h"

void
nsOverflowAreas::UnionWith(const nsOverflowAreas& aOther)
{
  // FIXME: We should probably change scrollable overflow to use
  // UnionRectIncludeEmpty (but leave visual overflow using UnionRect).
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    mRects[otype].UnionRect(mRects[otype], aOther.mRects[otype]);
  }
}

void
nsOverflowAreas::UnionAllWith(const nsRect& aRect)
{
  // FIXME: We should probably change scrollable overflow to use
  // UnionRectIncludeEmpty (but leave visual overflow using UnionRect).
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    mRects[otype].UnionRect(mRects[otype], aRect);
  }
}

void
nsOverflowAreas::SetAllTo(const nsRect& aRect)
{
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    mRects[otype] = aRect;
  }
}

namespace mozilla {

ReflowOutput::ReflowOutput(const ReflowInput& aReflowInput,
                                         uint32_t aFlags)
  : mISize(0)
  , mBSize(0)
  , mBlockStartAscent(ASK_FOR_BASELINE)
  , mFlags(aFlags)
  , mWritingMode(aReflowInput.GetWritingMode())
{
}

void
ReflowOutput::SetOverflowAreasToDesiredBounds()
{
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    mOverflowAreas.Overflow(otype).SetRect(0, 0, Width(), Height());
  }
}

void
ReflowOutput::UnionOverflowAreasWithDesiredBounds()
{
  // FIXME: We should probably change scrollable overflow to use
  // UnionRectIncludeEmpty (but leave visual overflow using UnionRect).
  nsRect rect(0, 0, Width(), Height());
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    nsRect& o = mOverflowAreas.Overflow(otype);
    o.UnionRect(o, rect);
  }
}

} // namespace mozilla
