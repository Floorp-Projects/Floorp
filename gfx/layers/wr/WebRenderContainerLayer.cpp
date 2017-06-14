/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/ScrollingLayersHelper.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

void
WebRenderContainerLayer::ClearAnimations()
{

  if (!GetAnimations().IsEmpty()) {
    mManager->AsWebRenderLayerManager()->
      AddCompositorAnimationsIdForDiscard(GetCompositorAnimationsId());
  }

  Layer::ClearAnimations();
}

void
WebRenderContainerLayer::UpdateTransformDataForAnimation()
{
  for (Animation& animation : mAnimations) {
    if (animation.property() == eCSSProperty_transform) {
      TransformData& transformData = animation.data().get_TransformData();
      transformData.inheritedXScale() = GetInheritedXScale();
      transformData.inheritedYScale() = GetInheritedYScale();
      transformData.hasPerspectiveParent() =
        GetParent() && GetParent()->GetTransformIsPerspective();
    }
  }
}

void
WebRenderContainerLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                                     const StackingContextHelper& aSc)
{
  nsTArray<LayerPolygon> children = SortChildrenBy3DZOrder(SortMode::WITHOUT_GEOMETRY);

  gfx::Matrix4x4 transform = GetTransform();
  gfx::Matrix4x4* transformForSC = &transform;
  float opacity = GetLocalOpacity();
  float* opacityForSC = &opacity;
  uint64_t animationsId = 0;

  if (gfxPrefs::WebRenderOMTAEnabled() &&
      !GetAnimations().IsEmpty()) {
    MOZ_ASSERT(GetCompositorAnimationsId());

    OptionalOpacity opacityForCompositor = void_t();
    OptionalTransform transformForCompositor = void_t();

    // Update opacity as nullptr in stacking context if there exists
    // opacity animation, the opacity value will be resolved
    // after animation sampling on the compositor
    if (HasOpacityAnimation()) {
      opacityForSC = nullptr;
      // Pass default opacity to compositor in case gecko fails to
      // get animated value after animation sampling.
      opacityForCompositor = opacity;
    }

    // Update transfrom as nullptr in stacking context if there exists
    // transform animation, the transform value will be resolved
    // after animation sampling on the compositor
    if (HasTransformAnimation()) {
      transformForSC = nullptr;
      // Pass default transform to compositor in case gecko fails to
      // get animated value after animation sampling.
      transformForCompositor = transform;
      UpdateTransformDataForAnimation();
    }

    animationsId = GetCompositorAnimationsId();
    OpAddCompositorAnimations
      anim(CompositorAnimations(GetAnimations(), animationsId),
           transformForCompositor, opacityForCompositor);
    WrBridge()->AddWebRenderParentCommand(anim);
  }

  // If APZ is enabled and this layer is a scroll thumb, then it might need
  // to move in the compositor to represent the async scroll position. So we
  // ensure that there is an animations id set on it, we will use this to give
  // WebRender updated transforms for composition.
  if (WrManager()->AsyncPanZoomEnabled() &&
      GetScrollThumbData().mDirection != ScrollDirection::NONE) {
    // A scroll thumb better not have a transform animation already or we're
    // going to end up clobbering it with APZ animating it too.
    MOZ_ASSERT(transformForSC);

    EnsureAnimationsId();
    animationsId = GetCompositorAnimationsId();
    // We need to set the transform in the stacking context to null for it to
    // pick up and install the animation id.
    transformForSC = nullptr;
  }

  if (transformForSC && transform.IsIdentity()) {
    // If the transform is an identity transform, strip it out so that WR
    // doesn't turn this stacking context into a reference frame, as it
    // affects positioning. Bug 1345577 tracks a better fix.
    transformForSC = nullptr;
  }

  nsTArray<WrFilterOp> filters;
  for (const CSSFilter& filter : this->GetFilterChain()) {
    filters.AppendElement(wr::ToWrFilterOp(filter));
  }

  ScrollingLayersHelper scroller(this, aBuilder, aSc);
  StackingContextHelper sc(aSc, aBuilder, this, animationsId, opacityForSC, transformForSC, filters);

  LayerRect rect = Bounds();
  DumpLayerInfo("ContainerLayer", rect);

  aBuilder.PushClip(sc.ToRelativeWrRect(rect), nullptr);

  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer(aBuilder, sc);
  }
  aBuilder.PopClip();
}

void
WebRenderRefLayer::RenderLayer(wr::DisplayListBuilder& aBuilder,
                               const StackingContextHelper& aSc)
{
  ScrollingLayersHelper scroller(this, aBuilder, aSc);

  ParentLayerRect bounds = GetLocalTransformTyped().TransformBounds(Bounds());
  // As with WebRenderTextLayer, because we don't push a stacking context for
  // this layer, WR doesn't know about the transform on this layer. Therefore
  // we need to apply that transform to the bounds before we pass it on to WR.
  // The conversion from ParentLayerPixel to LayerPixel below is a result of
  // changing the reference layer from "this layer" to the "the layer that
  // created aSc".
  LayerRect rect = ViewAs<LayerPixel>(bounds,
      PixelCastJustification::MovingDownToChildren);
  DumpLayerInfo("RefLayer", rect);

  WrClipRegionToken clipRegion = aBuilder.PushClipRegion(aSc.ToRelativeWrRect(rect));
  aBuilder.PushIFrame(aSc.ToRelativeWrRect(rect), clipRegion, wr::AsPipelineId(mId));
}

} // namespace layers
} // namespace mozilla
