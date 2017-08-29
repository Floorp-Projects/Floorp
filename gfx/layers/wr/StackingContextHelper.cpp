/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "mozilla/layers/WebRenderLayer.h"
#include "UnitTransforms.h"
#include "nsDisplayList.h"

namespace mozilla {
namespace layers {

StackingContextHelper::StackingContextHelper()
  : mBuilder(nullptr)
  , mXScale(1.0f)
  , mYScale(1.0f)
{
  // mOrigin remains at 0,0
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             const Maybe<gfx::Matrix4x4>& aTransform,
                                             const nsTArray<wr::WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
  , mXScale(1.0f)
  , mYScale(1.0f)
{
  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(aLayer->BoundsForStackingContext());
  Layer* layer = aLayer->GetLayer();
  mTransform = aTransform.valueOr(layer->GetTransform());

  float opacity = 1.0f;
  mBuilder->PushStackingContext(scBounds, 0, &opacity,
                                mTransform.IsIdentity() ? nullptr : &mTransform,
                                wr::TransformStyle::Flat,
                                nullptr,
                                wr::ToMixBlendMode(layer->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             WebRenderLayer* aLayer,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             const nsTArray<wr::WrFilterOp>& aFilters)
  : mBuilder(&aBuilder)
  , mXScale(1.0f)
  , mYScale(1.0f)
{
  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(aLayer->BoundsForStackingContext());
  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  mBuilder->PushStackingContext(scBounds,
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr,
                                wr::TransformStyle::Flat,
                                nullptr,
                                wr::ToMixBlendMode(aLayer->GetLayer()->GetMixBlendMode()),
                                aFilters);
  mOrigin = aLayer->Bounds().TopLeft();
}

StackingContextHelper::StackingContextHelper(const StackingContextHelper& aParentSC,
                                             wr::DisplayListBuilder& aBuilder,
                                             nsDisplayListBuilder* aDisplayListBuilder,
                                             nsDisplayItem* aItem,
                                             nsDisplayList* aDisplayList,
                                             gfx::Matrix4x4Typed<LayerPixel, LayerPixel>* aBoundTransform,
                                             uint64_t aAnimationsId,
                                             float* aOpacityPtr,
                                             gfx::Matrix4x4* aTransformPtr,
                                             gfx::Matrix4x4* aPerspectivePtr,
                                             const nsTArray<wr::WrFilterOp>& aFilters,
                                             const gfx::CompositionOp& aMixBlendMode)
  : mBuilder(&aBuilder)
  , mXScale(1.0f)
  , mYScale(1.0f)
{
  nsRect visibleRect;

  if (aTransformPtr) {
    mTransform = *aTransformPtr;
  }

  bool is2d = !aTransformPtr || (aTransformPtr->Is2D() && !aPerspectivePtr);
  if (is2d) {
    nsRect itemBounds = aDisplayList->GetClippedBoundsWithRespectToASR(aDisplayListBuilder, aItem->GetActiveScrolledRoot());
    nsRect childrenVisible = aItem->GetVisibleRectForChildren();
    visibleRect = itemBounds.Intersect(childrenVisible);

    // Apply the inherited scale from parent
    mTransform.PostScale(aParentSC.mXScale, aParentSC.mYScale, 1.0);
    mTransform.NudgeToIntegersFixedEpsilon();

    gfx::Size scale = mTransform.As2D().ScaleFactors(true);

    // Restore the scale to default if the scale is too small
    if (FuzzyEqualsAdditive(scale.width, 0.0f) ||
        FuzzyEqualsAdditive(scale.height, 0.0f)) {
      scale = gfx::Size(1.0f, 1.0f);
    }

    mTransform.PreScale(1.0f/scale.width, 1.0f/scale.height, 1.0);

    // Store the inherited scale if has
    this->mXScale = scale.width;
    this->mYScale = scale.height;
  } else {
    visibleRect = aDisplayList->GetBounds(aDisplayListBuilder);
    // The position of bounds are calculated by transform and perspective matrix in 3d case. reset it to (0, 0)
    visibleRect.MoveTo(0, 0);
  }
  float appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  LayerRect bounds = ViewAs<LayerPixel>(LayoutDeviceRect::FromAppUnits(visibleRect, appUnitsPerDevPixel),
                                        PixelCastJustification::WebRenderHasUnitResolution);

  // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
  if (aBoundTransform && !aBoundTransform->IsIdentity() && is2d) {
    bounds.MoveTo(aBoundTransform->TransformPoint(bounds.TopLeft()));
  }

  wr::LayoutRect scBounds = aParentSC.ToRelativeLayoutRect(bounds);

  mBuilder->PushStackingContext(scBounds,
                                aAnimationsId,
                                aOpacityPtr,
                                aTransformPtr ? &mTransform : aTransformPtr,
                                is2d ? wr::TransformStyle::Flat : wr::TransformStyle::Preserve3D,
                                aPerspectivePtr,
                                wr::ToMixBlendMode(aMixBlendMode),
                                aFilters);

  mOrigin = bounds.TopLeft();
}

StackingContextHelper::~StackingContextHelper()
{
  if (mBuilder) {
    mBuilder->PopStackingContext();
  }
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayerRect& aRect) const
{
  // Multiply by the scale inherited from ancestors if exits
  LayerRect aMaybeScaledRect = aRect;
  if (mXScale != 1.0f || mYScale != 1.0f) {
    aMaybeScaledRect.Scale(mXScale, mYScale);
  }

  return wr::ToLayoutRect(aMaybeScaledRect - mOrigin);
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRect(const LayoutDeviceRect& aRect) const
{
  // Multiply by the scale inherited from ancestors if exits
  LayoutDeviceRect aMaybeScaledRect = aRect;
  if (mXScale != 1.0f || mYScale != 1.0f) {
    aMaybeScaledRect.Scale(mXScale, mYScale);
  }

  return wr::ToLayoutRect(ViewAs<LayerPixel>(aMaybeScaledRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin);
}

wr::LayoutPoint
StackingContextHelper::ToRelativeLayoutPoint(const LayerPoint& aPoint) const
{
  return wr::ToLayoutPoint(aPoint - mOrigin);
}

wr::LayoutRect
StackingContextHelper::ToRelativeLayoutRectRounded(const LayoutDeviceRect& aRect) const
{
  // Multiply by the scale inherited from ancestors if exits
  LayoutDeviceRect aMaybeScaledRect = aRect;
  if (mXScale != 1.0f || mYScale != 1.0f) {
    aMaybeScaledRect.Scale(mXScale, mYScale);
  }

  return wr::ToLayoutRect(RoundedToInt(ViewAs<LayerPixel>(aMaybeScaledRect, PixelCastJustification::WebRenderHasUnitResolution) - mOrigin));
}

} // namespace layers
} // namespace mozilla
