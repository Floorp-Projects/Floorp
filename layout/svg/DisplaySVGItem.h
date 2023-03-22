/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_DISPLAYSVGITEM_H_
#define LAYOUT_SVG_DISPLAYSVGITEM_H_

#include "nsDisplayList.h"

namespace mozilla {

//----------------------------------------------------------------------
// Display list item:

class DisplaySVGItem : public nsPaintedDisplayItem {
 public:
  DisplaySVGItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_ASSERT(aFrame, "Must have a frame!");
  }

  /**
   * Hit testing for display lists.
   * @param aRect the point or rect being tested, relative to aFrame.
   * If the width and height are both 1 app unit, it indicates we're
   * hit testing a point, not a rect.
   * @param aOutFrames each item appends the frame(s) in this display item that
   * the rect is considered over (if any) to aOutFrames.
   */
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  /**
   * Paint the frame to some rendering context.
   */
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_DISPLAYSVGITEM_H_
