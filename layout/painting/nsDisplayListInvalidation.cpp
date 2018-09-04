/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDisplayListInvalidation.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"
#include "nsTableFrame.h"

nsDisplayItemGeometry::nsDisplayItemGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
{
  MOZ_COUNT_CTOR(nsDisplayItemGeometry);
  bool snap;
  mBounds = aItem->GetBounds(aBuilder, &snap);
}

nsDisplayItemGeometry::~nsDisplayItemGeometry()
{
  MOZ_COUNT_DTOR(nsDisplayItemGeometry);
}

nsDisplayItemGenericGeometry::nsDisplayItemGenericGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , mBorderRect(aItem->GetBorderRect())
{}

bool
ShouldSyncDecodeImages(nsDisplayListBuilder* aBuilder)
{
  return aBuilder->ShouldSyncDecodeImages();
}

void
nsDisplayItemGenericGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mBorderRect.MoveBy(aOffset);
}

nsDisplayItemBoundsGeometry::nsDisplayItemBoundsGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
{
  nscoord radii[8];
  mHasRoundedCorners = aItem->Frame()->GetBorderRadii(radii);
}

nsDisplayBorderGeometry::nsDisplayBorderGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
{}

nsDisplayBackgroundGeometry::nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem,
                                                         nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
  , mPositioningArea(aItem->GetPositioningArea())
  , mDestRect(aItem->GetDestRect())
{}

void
nsDisplayBackgroundGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPositioningArea.MoveBy(aOffset);
  mDestRect.MoveBy(aOffset);
}

nsDisplayThemedBackgroundGeometry::nsDisplayThemedBackgroundGeometry(nsDisplayThemedBackground* aItem,
                                                                     nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , mPositioningArea(aItem->GetPositioningArea())
  , mWindowIsActive(aItem->IsWindowActive())
{}

void
nsDisplayThemedBackgroundGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPositioningArea.MoveBy(aOffset);
}

nsDisplayBoxShadowInnerGeometry::nsDisplayBoxShadowInnerGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , mPaddingRect(aItem->GetPaddingRect())
{}

void
nsDisplayBoxShadowInnerGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPaddingRect.MoveBy(aOffset);
}

nsDisplayBoxShadowOuterGeometry::nsDisplayBoxShadowOuterGeometry(nsDisplayItem* aItem,
    nsDisplayListBuilder* aBuilder, float aOpacity)
  : nsDisplayItemGenericGeometry(aItem, aBuilder)
  , mOpacity(aOpacity)
{}

void
nsDisplaySolidColorRegionGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mRegion.MoveBy(aOffset);
}

nsDisplaySVGEffectGeometry::nsDisplaySVGEffectGeometry(nsDisplaySVGEffects* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , mBBox(aItem->BBoxInUserSpace())
  , mUserSpaceOffset(aItem->UserSpaceOffset())
  , mFrameOffsetToReferenceFrame(aItem->ToReferenceFrame())
{}

void
nsDisplaySVGEffectGeometry::MoveBy(const nsPoint& aOffset)
{
  mBounds.MoveBy(aOffset);
  mFrameOffsetToReferenceFrame += aOffset;
}

nsDisplayMaskGeometry::nsDisplayMaskGeometry(nsDisplayMask* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplaySVGEffectGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
  , mDestRects(aItem->GetDestRects())
  , mOpacity(aItem->Frame()->StyleEffects()->mOpacity)
  , mHandleOpacity(aItem->ShouldHandleOpacity())
{}

nsDisplayFilterGeometry::nsDisplayFilterGeometry(nsDisplayFilter* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplaySVGEffectGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
{}

nsCharClipGeometry::nsCharClipGeometry(nsCharClipDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGenericGeometry(aItem, aBuilder)
  , mVisIStartEdge(aItem->mVisIStartEdge)
  , mVisIEndEdge(aItem->mVisIEndEdge)
{}

nsDisplayTableItemGeometry::nsDisplayTableItemGeometry(nsDisplayTableItem* aItem,
                                                       nsDisplayListBuilder* aBuilder,
                                                       const nsPoint& aFrameOffsetToViewport)
  : nsDisplayItemGenericGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
  , mFrameOffsetToViewport(aFrameOffsetToViewport)
{}
