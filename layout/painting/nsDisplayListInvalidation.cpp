/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDisplayListInvalidation.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"
#include "nsTableFrame.h"

namespace mozilla {

nsDisplayItemGeometry::nsDisplayItemGeometry(nsDisplayItem* aItem,
                                             nsDisplayListBuilder* aBuilder) {
  MOZ_COUNT_CTOR(nsDisplayItemGeometry);
  bool snap;
  mBounds = aItem->GetBounds(aBuilder, &snap);
}

nsDisplayItemGeometry::~nsDisplayItemGeometry() {
  MOZ_COUNT_DTOR(nsDisplayItemGeometry);
}

nsDisplayItemGenericGeometry::nsDisplayItemGenericGeometry(
    nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      mBorderRect(aItem->GetBorderRect()) {}

bool ShouldSyncDecodeImages(nsDisplayListBuilder* aBuilder) {
  return aBuilder->ShouldSyncDecodeImages();
}

nsDisplayItemGeometry* GetPreviousGeometry(nsDisplayItem* aItem) {
  if (RefPtr<layers::WebRenderFallbackData> data =
          layers::GetWebRenderUserData<layers::WebRenderFallbackData>(
              aItem->Frame(), aItem->GetPerFrameKey())) {
    return data->GetGeometry();
  }
  return nullptr;
}

void nsDisplayItemGenericGeometry::MoveBy(const nsPoint& aOffset) {
  nsDisplayItemGeometry::MoveBy(aOffset);
  mBorderRect.MoveBy(aOffset);
}

nsDisplayItemBoundsGeometry::nsDisplayItemBoundsGeometry(
    nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder) {
  nscoord radii[8];
  mHasRoundedCorners = aItem->Frame()->GetBorderRadii(radii);
}

nsDisplayBorderGeometry::nsDisplayBorderGeometry(nsDisplayItem* aItem,
                                                 nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      nsImageGeometryMixin(aItem, aBuilder) {}

nsDisplayBackgroundGeometry::nsDisplayBackgroundGeometry(
    nsDisplayBackgroundImage* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      nsImageGeometryMixin(aItem, aBuilder),
      mPositioningArea(aItem->GetPositioningArea()),
      mDestRect(aItem->GetDestRect()) {}

void nsDisplayBackgroundGeometry::MoveBy(const nsPoint& aOffset) {
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPositioningArea.MoveBy(aOffset);
  mDestRect.MoveBy(aOffset);
}

nsDisplayThemedBackgroundGeometry::nsDisplayThemedBackgroundGeometry(
    nsDisplayThemedBackground* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      mPositioningArea(aItem->GetPositioningArea()),
      mWindowIsActive(aItem->IsWindowActive()) {}

void nsDisplayThemedBackgroundGeometry::MoveBy(const nsPoint& aOffset) {
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPositioningArea.MoveBy(aOffset);
}

nsDisplayBoxShadowInnerGeometry::nsDisplayBoxShadowInnerGeometry(
    nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      mPaddingRect(aItem->GetPaddingRect()) {}

void nsDisplayBoxShadowInnerGeometry::MoveBy(const nsPoint& aOffset) {
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPaddingRect.MoveBy(aOffset);
}

void nsDisplaySolidColorRegionGeometry::MoveBy(const nsPoint& aOffset) {
  nsDisplayItemGeometry::MoveBy(aOffset);
  mRegion.MoveBy(aOffset);
}

nsDisplaySVGEffectGeometry::nsDisplaySVGEffectGeometry(
    nsDisplayEffectsBase* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGeometry(aItem, aBuilder),
      mBBox(aItem->BBoxInUserSpace()),
      mUserSpaceOffset(aItem->UserSpaceOffset()),
      mFrameOffsetToReferenceFrame(aItem->ToReferenceFrame()) {}

void nsDisplaySVGEffectGeometry::MoveBy(const nsPoint& aOffset) {
  mBounds.MoveBy(aOffset);
  mFrameOffsetToReferenceFrame += aOffset;
}

nsDisplayMasksAndClipPathsGeometry::nsDisplayMasksAndClipPathsGeometry(
    nsDisplayMasksAndClipPaths* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplaySVGEffectGeometry(aItem, aBuilder),
      nsImageGeometryMixin(aItem, aBuilder),
      mDestRects(aItem->GetDestRects().Clone()) {}

nsDisplayFiltersGeometry::nsDisplayFiltersGeometry(
    nsDisplayFilters* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplaySVGEffectGeometry(aItem, aBuilder),
      nsImageGeometryMixin(aItem, aBuilder) {}

nsDisplayTableItemGeometry::nsDisplayTableItemGeometry(
    nsDisplayTableItem* aItem, nsDisplayListBuilder* aBuilder,
    const nsPoint& aFrameOffsetToViewport)
    : nsDisplayItemGenericGeometry(aItem, aBuilder),
      nsImageGeometryMixin(aItem, aBuilder),
      mFrameOffsetToViewport(aFrameOffsetToViewport) {}

}  // namespace mozilla
