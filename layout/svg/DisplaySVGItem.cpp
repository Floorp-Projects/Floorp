/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "DisplaySVGItem.h"

#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/SVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsPoint.h"

using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {

void DisplaySVGItem::HitTest(nsDisplayListBuilder* aBuilder,
                             const nsRect& aRect, HitTestState* aState,
                             nsTArray<nsIFrame*>* aOutFrames) {
  ISVGDisplayableFrame* svgFrame = do_QueryFrame(mFrame);
  MOZ_ASSERT(svgFrame, "Unexpected frame type");

  nsPoint pointRelativeToReferenceFrame = aRect.Center();
  // ToReferenceFrame() includes mFrame->GetPosition(), our user
  // space position.
  nsPoint userSpacePtInAppUnits = pointRelativeToReferenceFrame -
                                  (ToReferenceFrame() - mFrame->GetPosition());
  gfxPoint userSpacePt =
      gfxPoint(userSpacePtInAppUnits.x, userSpacePtInAppUnits.y) /
      AppUnitsPerCSSPixel();
  if (auto* target = svgFrame->GetFrameForPoint(userSpacePt)) {
    aOutFrames->AppendElement(target);
  }
}

void DisplaySVGItem::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  ISVGDisplayableFrame* svgFrame = do_QueryFrame(mFrame);
  MOZ_ASSERT(svgFrame, "Unexpected frame type");
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  // ToReferenceFrame() includes our mRect offset, but painting takes
  // account of that too. To avoid double counting, we subtract that
  // here.
  nsPoint offset = ToReferenceFrame() - mFrame->GetPosition();

  gfxPoint devPixelOffset =
      nsLayoutUtils::PointToGfxPoint(offset, appUnitsPerDevPixel);

  gfxMatrix tm = SVGUtils::GetCSSPxToDevPxMatrix(mFrame) *
                 gfxMatrix::Translation(devPixelOffset);
  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  svgFrame->PaintSVG(*aCtx, tm, imgParams);
}

}  // namespace mozilla
