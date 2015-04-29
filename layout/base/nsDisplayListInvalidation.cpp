/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDisplayListInvalidation.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"

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
  , mContentRect(aItem->GetContentRect())
{}

void
nsDisplayBorderGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mContentRect.MoveBy(aOffset);
}

nsDisplayBackgroundGeometry::nsDisplayBackgroundGeometry(nsDisplayBackgroundImage* aItem,
                                                         nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , nsImageGeometryMixin(aItem, aBuilder)
  , mPositioningArea(aItem->GetPositioningArea())
{}

void
nsDisplayBackgroundGeometry::MoveBy(const nsPoint& aOffset)
{
  nsDisplayItemGeometry::MoveBy(aOffset);
  mPositioningArea.MoveBy(aOffset);
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

nsDisplaySVGEffectsGeometry::nsDisplaySVGEffectsGeometry(nsDisplaySVGEffects* aItem, nsDisplayListBuilder* aBuilder)
  : nsDisplayItemGeometry(aItem, aBuilder)
  , mBBox(aItem->BBoxInUserSpace())
  , mUserSpaceOffset(aItem->UserSpaceOffset())
  , mFrameOffsetToReferenceFrame(aItem->ToReferenceFrame())
{}

void
nsDisplaySVGEffectsGeometry::MoveBy(const nsPoint& aOffset)
{
  mBounds.MoveBy(aOffset);
  mFrameOffsetToReferenceFrame += aOffset;
}

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
