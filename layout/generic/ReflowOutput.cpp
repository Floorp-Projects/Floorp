/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the output from nsIFrame::Reflow */

#include "mozilla/ReflowOutput.h"
#include "mozilla/ReflowInput.h"

namespace mozilla {

/* static */
nsRect OverflowAreas::GetOverflowClipRect(const nsRect& aRectToClip,
                                          const nsRect& aBounds,
                                          PhysicalAxes aClipAxes,
                                          const nsSize& aOverflowMargin) {
  auto inflatedBounds = aBounds;
  inflatedBounds.Inflate(aOverflowMargin);
  auto clip = aRectToClip;
  if (aClipAxes & PhysicalAxes::Vertical) {
    clip.y = inflatedBounds.y;
    clip.height = inflatedBounds.height;
  }
  if (aClipAxes & PhysicalAxes::Horizontal) {
    clip.x = inflatedBounds.x;
    clip.width = inflatedBounds.width;
  }
  return clip;
}

/* static */
void OverflowAreas::ApplyOverflowClippingOnRect(nsRect& aOverflowRect,
                                                const nsRect& aBounds,
                                                PhysicalAxes aClipAxes,
                                                const nsSize& aOverflowMargin) {
  aOverflowRect = aOverflowRect.Intersect(
      GetOverflowClipRect(aOverflowRect, aBounds, aClipAxes, aOverflowMargin));
}

void OverflowAreas::UnionWith(const OverflowAreas& aOther) {
  InkOverflow().UnionRect(InkOverflow(), aOther.InkOverflow());
  ScrollableOverflow().UnionRect(ScrollableOverflow(),
                                 aOther.ScrollableOverflow());
}

void OverflowAreas::UnionAllWith(const nsRect& aRect) {
  InkOverflow().UnionRect(InkOverflow(), aRect);
  ScrollableOverflow().UnionRect(ScrollableOverflow(), aRect);
}

void OverflowAreas::SetAllTo(const nsRect& aRect) {
  InkOverflow() = aRect;
  ScrollableOverflow() = aRect;
}

ReflowOutput::ReflowOutput(const ReflowInput& aReflowInput)
    : ReflowOutput(aReflowInput.GetWritingMode()) {}

void ReflowOutput::SetOverflowAreasToDesiredBounds() {
  mOverflowAreas.SetAllTo(nsRect(0, 0, Width(), Height()));
}

void ReflowOutput::UnionOverflowAreasWithDesiredBounds() {
  mOverflowAreas.UnionAllWith(nsRect(0, 0, Width(), Height()));
}

}  // namespace mozilla
