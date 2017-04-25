/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderContainerLayer.h"

#include <inttypes.h>
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderTypes.h"

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
}

void
WebRenderContainerLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
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

  StackingContextHelper sc(aBuilder, this, animationsId, opacityForSC, transformForSC);

  LayerRect rect = Bounds();
  DumpLayerInfo("ContainerLayer", rect);

  Maybe<WrImageMask> mask = BuildWrMaskLayer(true);
  aBuilder.PushClip(sc.ToRelativeWrRect(rect), mask.ptrOr(nullptr));

  for (LayerPolygon& child : children) {
    if (child.layer->IsBackfaceHidden()) {
      continue;
    }
    ToWebRenderLayer(child.layer)->RenderLayer(aBuilder);
  }
  aBuilder.PopClip();
}

void
WebRenderRefLayer::RenderLayer(wr::DisplayListBuilder& aBuilder)
{
  gfx::Matrix4x4 transform;// = GetTransform();
  gfx::Rect relBounds = TransformedVisibleBoundsRelativeToParent();

  WrClipRegion clipRegion = aBuilder.BuildClipRegion(wr::ToWrRect(relBounds));

  if (gfxPrefs::LayersDump()) {
    printf_stderr("RefLayer %p (%" PRIu64 ") using bounds/overflow=%s, transform=%s\n",
                  this->GetLayer(),
                  mId,
                  Stringify(relBounds).c_str(),
                  Stringify(transform).c_str());
  }

  aBuilder.PushIFrame(wr::ToWrRect(relBounds), clipRegion, wr::AsPipelineId(mId));
}

} // namespace layers
} // namespace mozilla
