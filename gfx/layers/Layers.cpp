/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Layers.h"

#include <inttypes.h>          // for PRIu64
#include <stdio.h>             // for stderr
#include <algorithm>           // for max, min
#include <list>                // for list
#include <set>                 // for set
#include <string>              // for char_traits, string, basic_string
#include <type_traits>         // for remove_reference<>::type
#include "CompositableHost.h"  // for CompositableHost
#include "GeckoProfiler.h"  // for profiler_can_accept_markers, PROFILER_MARKER_TEXT
#include "ImageLayers.h"    // for ImageLayer
#include "LayerUserData.h"  // for LayerUserData
#include "ReadbackLayer.h"  // for ReadbackLayer
#include "TreeTraversal.h"  // for ForwardIterator, ForEachNode, DepthFirstSearch, TraversalFlag, TraversalFl...
#include "UnitTransforms.h"  // for ViewAs, PixelCastJustification, PixelCastJustification::RenderTargetIsPare...
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "gfx2DGlue.h"              // for ThebesMatrix, ToPoint, ThebesRect
#include "gfxEnv.h"                 // for gfxEnv
#include "gfxMatrix.h"              // for gfxMatrix
#include "gfxUtils.h"               // for gfxUtils, gfxUtils::sDumpPaintFile
#include "mozilla/ArrayIterator.h"  // for ArrayIterator
#include "mozilla/BaseProfilerMarkersPrerequisites.h"  // for MarkerTiming
#include "mozilla/DebugOnly.h"                         // for DebugOnly
#include "mozilla/Logging.h"  // for LogLevel, LogLevel::Debug, MOZ_LOG_TEST
#include "mozilla/ScrollPositionUpdate.h"  // for ScrollPositionUpdate
#include "mozilla/Telemetry.h"             // for AccumulateTimeDelta
#include "mozilla/TelemetryHistogramEnums.h"  // for KEYPRESS_PRESENT_LATENCY, SCROLL_PRESENT_LATENCY
#include "mozilla/ToString.h"  // for ToString
#include "mozilla/gfx/2D.h"  // for SourceSurface, DrawTarget, DataSourceSurface
#include "mozilla/gfx/BasePoint3D.h"  // for BasePoint3D<>::(anonymous union)::(anonymous), BasePoint3D<>::(anonymous)
#include "mozilla/gfx/BaseRect.h"  // for operator<<, BaseRect (ptr only)
#include "mozilla/gfx/BaseSize.h"  // for operator<<, BaseSize<>::(anonymous union)::(anonymous), BaseSize<>::(anony...
#include "mozilla/gfx/Matrix.h"  // for Matrix4x4, Matrix, Matrix4x4Typed<>::(anonymous union)::(anonymous), Matri...
#include "mozilla/gfx/MatrixFwd.h"                 // for Float
#include "mozilla/gfx/Polygon.h"                   // for Polygon, PolygonTyped
#include "mozilla/layers/BSPTree.h"                // for LayerPolygon, BSPTree
#include "mozilla/layers/CompositableClient.h"     // for CompositableClient
#include "mozilla/layers/Compositor.h"             // for Compositor
#include "mozilla/layers/LayerManagerComposite.h"  // for HostLayer
#include "mozilla/layers/LayersMessages.h"  // for SpecificLayerAttributes, CompositorAnimations (ptr only), ContainerLayerAt...
#include "mozilla/layers/LayersTypes.h"  // for EventRegions, operator<<, CompositionPayload, CSSTransformMatrix, MOZ_LAYE...
#include "mozilla/layers/ShadowLayers.h"  // for ShadowableLayer
#include "nsBaseHashtable.h"  // for nsBaseHashtable<>::Iterator, nsBaseHashtable<>::LookupResult
#include "nsISupportsUtils.h"              // for NS_ADDREF, NS_RELEASE
#include "nsPrintfCString.h"               // for nsPrintfCString
#include "nsRegionFwd.h"                   // for IntRegion
#include "nsString.h"                      // for nsTSubstring
#include "protobuf/LayerScopePacket.pb.h"  // for LayersPacket::Layer, LayersPacket, LayersPacket_Layer::Matrix, LayersPacke...

// Undo the damage done by mozzconf.h
#undef compress
#include "mozilla/Compression.h"

namespace mozilla {
namespace layers {

typedef ScrollableLayerGuid::ViewID ViewID;

using namespace mozilla::gfx;
using namespace mozilla::Compression;

//--------------------------------------------------
// Layer

Layer::Layer(LayerManager* aManager, void* aImplData)
    : mManager(aManager),
      mParent(nullptr),
      mNextSibling(nullptr),
      mPrevSibling(nullptr),
      mImplData(aImplData),
      mUseTileSourceRect(false)
#ifdef DEBUG
      ,
      mDebugColorIndex(0)
#endif
{
}

Layer::~Layer() = default;

void Layer::SetEventRegions(const EventRegions& aRegions) {
  if (mEventRegions != aRegions) {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(
        this, ("Layer::Mutated(%p) eventregions were %s, now %s", this,
               ToString(mEventRegions).c_str(), ToString(aRegions).c_str()));
    mEventRegions = aRegions;
    Mutated();
  }
}

void Layer::SetCompositorAnimations(
    const LayersId& aLayersId,
    const CompositorAnimations& aCompositorAnimations) {
  MOZ_LAYERS_LOG_IF_SHADOWABLE(
      this, ("Layer::Mutated(%p) SetCompositorAnimations with id=%" PRIu64,
             this, mAnimationInfo.GetCompositorAnimationsId()));

  mAnimationInfo.SetCompositorAnimations(aLayersId, aCompositorAnimations);

  Mutated();
}

void Layer::ClearCompositorAnimations() {
  MOZ_LAYERS_LOG_IF_SHADOWABLE(
      this, ("Layer::Mutated(%p) ClearCompositorAnimations with id=%" PRIu64,
             this, mAnimationInfo.GetCompositorAnimationsId()));

  mAnimationInfo.ClearAnimations();

  Mutated();
}

void Layer::StartPendingAnimations(const TimeStamp& aReadyTime) {
  ForEachNode<ForwardIterator>(this, [&aReadyTime](Layer* layer) {
    if (layer->mAnimationInfo.StartPendingAnimations(aReadyTime)) {
      layer->Mutated();
    }
  });
}

void Layer::SetAsyncPanZoomController(uint32_t aIndex,
                                      AsyncPanZoomController* controller) {
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
  // We should never be setting an APZC on a non-scrollable layer
  MOZ_ASSERT(!controller || GetFrameMetrics(aIndex).IsScrollable());
  mApzcs[aIndex] = controller;
}

AsyncPanZoomController* Layer::GetAsyncPanZoomController(
    uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
#ifdef DEBUG
  if (mApzcs[aIndex]) {
    MOZ_ASSERT(GetFrameMetrics(aIndex).IsScrollable());
  }
#endif
  return mApzcs[aIndex];
}

void Layer::ScrollMetadataChanged() {
  mApzcs.SetLength(GetScrollMetadataCount());
}

std::unordered_set<ScrollableLayerGuid::ViewID>
Layer::ApplyPendingUpdatesToSubtree() {
  ForEachNode<ForwardIterator>(this, [](Layer* layer) {
    layer->ApplyPendingUpdatesForThisTransaction();
  });
  // Once we're done recursing through the whole tree, clear the pending
  // updates from the manager.
  return Manager()->ClearPendingScrollInfoUpdate();
}

bool Layer::IsOpaqueForVisibility() {
  return GetEffectiveOpacity() == 1.0f &&
         GetEffectiveMixBlendMode() == CompositionOp::OP_OVER;
}

bool Layer::CanUseOpaqueSurface() {
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (GetContentFlags() & CONTENT_OPAQUE) return true;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  ContainerLayer* parent = GetParent();
  return parent && parent->GetFirstChild() == this &&
         parent->CanUseOpaqueSurface();
}

// NB: eventually these methods will be defined unconditionally, and
// can be moved into Layers.h
const Maybe<ParentLayerIntRect>& Layer::GetLocalClipRect() {
  if (HostLayer* shadow = AsHostLayer()) {
    return shadow->GetShadowClipRect();
  }
  return GetClipRect();
}

const LayerIntRegion& Layer::GetLocalVisibleRegion() {
  if (HostLayer* shadow = AsHostLayer()) {
    return shadow->GetShadowVisibleRegion();
  }
  return GetVisibleRegion();
}

Matrix4x4 Layer::SnapTransformTranslation(const Matrix4x4& aTransform,
                                          Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  if (!mManager->IsSnappingEffectiveTransforms()) {
    return aTransform;
  }

  return gfxUtils::SnapTransformTranslation(aTransform, aResidualTransform);
}

Matrix4x4 Layer::SnapTransformTranslation3D(const Matrix4x4& aTransform,
                                            Matrix* aResidualTransform) {
  return gfxUtils::SnapTransformTranslation3D(aTransform, aResidualTransform);
}

Matrix4x4 Layer::SnapTransform(const Matrix4x4& aTransform,
                               const gfxRect& aSnapRect,
                               Matrix* aResidualTransform) {
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  if (!mManager->IsSnappingEffectiveTransforms()) {
    return aTransform;
  }

  return gfxUtils::SnapTransform(aTransform, aSnapRect, aResidualTransform);
}

static bool AncestorLayerMayChangeTransform(Layer* aLayer) {
  for (Layer* l = aLayer; l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_MAY_CHANGE_TRANSFORM) {
      return true;
    }

    if (l->GetParent() && l->GetParent()->AsRefLayer()) {
      return false;
    }
  }
  return false;
}

bool Layer::MayResample() {
  Matrix transform2d;
  return !GetEffectiveTransform().Is2D(&transform2d) ||
         ThebesMatrix(transform2d).HasNonIntegerTranslation() ||
         AncestorLayerMayChangeTransform(this);
}

RenderTargetIntRect Layer::CalculateScissorRect(
    const RenderTargetIntRect& aCurrentScissorRect) {
  ContainerLayer* container = GetParent();
  ContainerLayer* containerChild = nullptr;
  NS_ASSERTION(GetParent(), "This can't be called on the root!");

  // Find the layer creating the 3D context.
  while (container->Extend3DContext() && !container->UseIntermediateSurface()) {
    containerChild = container;
    container = container->GetParent();
    MOZ_ASSERT(container);
  }

  // Find the nearest layer with a clip, or this layer.
  // ContainerState::SetupScrollingMetadata() may install a clip on
  // the layer.
  Layer* clipLayer = containerChild && containerChild->GetLocalClipRect()
                         ? containerChild
                         : this;

  // Establish initial clip rect: it's either the one passed in, or
  // if the parent has an intermediate surface, it's the extents of that
  // surface.
  RenderTargetIntRect currentClip;
  if (container->UseIntermediateSurface()) {
    currentClip.SizeTo(container->GetIntermediateSurfaceRect().Size());
  } else {
    currentClip = aCurrentScissorRect;
  }

  if (!clipLayer->GetLocalClipRect()) {
    return currentClip;
  }

  if (GetLocalVisibleRegion().IsEmpty()) {
    // When our visible region is empty, our parent may not have created the
    // intermediate surface that we would require for correct clipping; however,
    // this does not matter since we are invisible.
    // Make sure we still compute a clip rect if we want to draw checkboarding
    // for this layer, since we want to do this even if the layer is invisible.
    return RenderTargetIntRect(currentClip.TopLeft(),
                               RenderTargetIntSize(0, 0));
  }

  const RenderTargetIntRect clipRect = ViewAs<RenderTargetPixel>(
      *clipLayer->GetLocalClipRect(),
      PixelCastJustification::RenderTargetIsParentLayerForRoot);
  if (clipRect.IsEmpty()) {
    // We might have a non-translation transform in the container so we can't
    // use the code path below.
    return RenderTargetIntRect(currentClip.TopLeft(),
                               RenderTargetIntSize(0, 0));
  }

  RenderTargetIntRect scissor = clipRect;
  if (!container->UseIntermediateSurface()) {
    gfx::Matrix matrix;
    DebugOnly<bool> is2D = container->GetEffectiveTransform().Is2D(&matrix);
    // See DefaultComputeEffectiveTransforms below
    NS_ASSERTION(is2D && matrix.PreservesAxisAlignedRectangles(),
                 "Non preserves axis aligned transform with clipped child "
                 "should have forced intermediate surface");
    gfx::Rect r(scissor.X(), scissor.Y(), scissor.Width(), scissor.Height());
    gfxRect trScissor = gfx::ThebesRect(matrix.TransformBounds(r));
    trScissor.Round();
    IntRect tmp;
    if (!gfxUtils::GfxRectToIntRect(trScissor, &tmp)) {
      return RenderTargetIntRect(currentClip.TopLeft(),
                                 RenderTargetIntSize(0, 0));
    }
    scissor = ViewAs<RenderTargetPixel>(tmp);

    // Find the nearest ancestor with an intermediate surface
    do {
      container = container->GetParent();
    } while (container && !container->UseIntermediateSurface());
  }

  if (container) {
    scissor.MoveBy(-container->GetIntermediateSurfaceRect().TopLeft());
  }
  return currentClip.Intersect(scissor);
}

Maybe<ParentLayerIntRect> Layer::GetScrolledClipRect() const {
  const Maybe<LayerClip> clip = mSimpleAttrs.GetScrolledClip();
  return clip ? Some(clip->GetClipRect()) : Nothing();
}

const ScrollMetadata& Layer::GetScrollMetadata(uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
  return mScrollMetadata[aIndex];
}

const FrameMetrics& Layer::GetFrameMetrics(uint32_t aIndex) const {
  return GetScrollMetadata(aIndex).GetMetrics();
}

bool Layer::HasScrollableFrameMetrics() const {
  for (uint32_t i = 0; i < GetScrollMetadataCount(); i++) {
    if (GetFrameMetrics(i).IsScrollable()) {
      return true;
    }
  }
  return false;
}

bool Layer::IsScrollableWithoutContent() const {
  // A scrollable container layer with no children
  return AsContainerLayer() && HasScrollableFrameMetrics() && !GetFirstChild();
}

Matrix4x4 Layer::GetTransform() const {
  Matrix4x4 transform = mSimpleAttrs.GetTransform();
  transform.PostScale(GetPostXScale(), GetPostYScale(), 1.0f);
  if (const ContainerLayer* c = AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }
  return transform;
}

const CSSTransformMatrix Layer::GetTransformTyped() const {
  return ViewAs<CSSTransformMatrix>(GetTransform());
}

Matrix4x4 Layer::GetLocalTransform() {
  if (HostLayer* shadow = AsHostLayer()) {
    return shadow->GetShadowTransform();
  }
  return GetTransform();
}

const LayerToParentLayerMatrix4x4 Layer::GetLocalTransformTyped() {
  return ViewAs<LayerToParentLayerMatrix4x4>(GetLocalTransform());
}

bool Layer::IsScrollbarContainer() const {
  const ScrollbarData& data = GetScrollbarData();
  return (data.mScrollbarLayerType == ScrollbarLayerType::Container)
             ? data.mDirection.isSome()
             : false;
}

bool Layer::HasTransformAnimation() const {
  return mAnimationInfo.HasTransformAnimation();
}

void Layer::ApplyPendingUpdatesForThisTransaction() {
  if (mPendingTransform && *mPendingTransform != mSimpleAttrs.GetTransform()) {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(
        this, ("Layer::Mutated(%p) PendingUpdatesForThisTransaction", this));
    mSimpleAttrs.SetTransform(*mPendingTransform);
    MutatedSimple();
  }
  mPendingTransform = nullptr;

  if (mAnimationInfo.ApplyPendingUpdatesForThisTransaction()) {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(
        this, ("Layer::Mutated(%p) PendingUpdatesForThisTransaction", this));
    Mutated();
  }

  for (size_t i = 0; i < mScrollMetadata.Length(); i++) {
    FrameMetrics& fm = mScrollMetadata[i].GetMetrics();
    ScrollableLayerGuid::ViewID scrollId = fm.GetScrollId();
    Maybe<nsTArray<ScrollPositionUpdate>> update =
        Manager()->GetPendingScrollInfoUpdate(scrollId);
    if (update) {
      nsTArray<ScrollPositionUpdate> infos = update.extract();
      mScrollMetadata[i].UpdatePendingScrollInfo(std::move(infos));
      Mutated();
    }
  }
}

float Layer::GetLocalOpacity() {
  float opacity = mSimpleAttrs.GetOpacity();
  if (HostLayer* shadow = AsHostLayer()) opacity = shadow->GetShadowOpacity();
  return std::min(std::max(opacity, 0.0f), 1.0f);
}

float Layer::GetEffectiveOpacity() {
  float opacity = GetLocalOpacity();
  for (ContainerLayer* c = GetParent(); c && !c->UseIntermediateSurface();
       c = c->GetParent()) {
    opacity *= c->GetLocalOpacity();
  }
  return opacity;
}

CompositionOp Layer::GetEffectiveMixBlendMode() {
  if (mSimpleAttrs.GetMixBlendMode() != CompositionOp::OP_OVER)
    return mSimpleAttrs.GetMixBlendMode();
  for (ContainerLayer* c = GetParent(); c && !c->UseIntermediateSurface();
       c = c->GetParent()) {
    if (c->mSimpleAttrs.GetMixBlendMode() != CompositionOp::OP_OVER)
      return c->mSimpleAttrs.GetMixBlendMode();
  }

  return mSimpleAttrs.GetMixBlendMode();
}

Matrix4x4 Layer::ComputeTransformToPreserve3DRoot() {
  Matrix4x4 transform = GetLocalTransform();
  for (Layer* layer = GetParent(); layer && layer->Extend3DContext();
       layer = layer->GetParent()) {
    transform = transform * layer->GetLocalTransform();
  }
  return transform;
}

void Layer::ComputeEffectiveTransformForMaskLayers(
    const gfx::Matrix4x4& aTransformToSurface) {
  if (GetMaskLayer()) {
    ComputeEffectiveTransformForMaskLayer(GetMaskLayer(), aTransformToSurface);
  }
  for (size_t i = 0; i < GetAncestorMaskLayerCount(); i++) {
    Layer* maskLayer = GetAncestorMaskLayerAt(i);
    ComputeEffectiveTransformForMaskLayer(maskLayer, aTransformToSurface);
  }
}

/* static */
void Layer::ComputeEffectiveTransformForMaskLayer(
    Layer* aMaskLayer, const gfx::Matrix4x4& aTransformToSurface) {
#ifdef DEBUG
  bool maskIs2D = aMaskLayer->GetTransform().CanDraw2D();
  NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
#endif
  // The mask layer can have an async transform applied to it in some
  // situations, so be sure to use its GetLocalTransform() rather than
  // its GetTransform().
  aMaskLayer->mEffectiveTransform = aMaskLayer->SnapTransformTranslation(
      aMaskLayer->GetLocalTransform() * aTransformToSurface, nullptr);
}

RenderTargetRect Layer::TransformRectToRenderTarget(const LayerIntRect& aRect) {
  LayerRect rect(aRect);
  RenderTargetRect quad = RenderTargetRect::FromUnknownRect(
      GetEffectiveTransform().TransformBounds(rect.ToUnknownRect()));
  return quad;
}

bool Layer::GetVisibleRegionRelativeToRootLayer(nsIntRegion& aResult,
                                                IntPoint* aLayerOffset) {
  MOZ_ASSERT(aLayerOffset, "invalid offset pointer");

  if (!GetParent()) {
    return false;
  }

  IntPoint offset;
  aResult = GetLocalVisibleRegion().ToUnknownRegion();
  for (Layer* layer = this; layer; layer = layer->GetParent()) {
    gfx::Matrix matrix;
    if (!layer->GetLocalTransform().Is2D(&matrix) || !matrix.IsTranslation()) {
      return false;
    }

    // The offset of |layer| to its parent.
    auto currentLayerOffset = IntPoint::Round(matrix.GetTranslation());

    // Translate the accumulated visible region of |this| by the offset of
    // |layer|.
    aResult.MoveBy(currentLayerOffset.x, currentLayerOffset.y);

    // If the parent layer clips its lower layers, clip the visible region
    // we're accumulating.
    if (layer->GetLocalClipRect()) {
      aResult.AndWith(layer->GetLocalClipRect()->ToUnknownRect());
    }

    // Now we need to walk across the list of siblings for this parent layer,
    // checking to see if any of these layer trees obscure |this|. If so,
    // remove these areas from the visible region as well. This will pick up
    // chrome overlays like a tab modal prompt.
    Layer* sibling;
    for (sibling = layer->GetNextSibling(); sibling;
         sibling = sibling->GetNextSibling()) {
      gfx::Matrix siblingMatrix;
      if (!sibling->GetLocalTransform().Is2D(&siblingMatrix) ||
          !siblingMatrix.IsTranslation()) {
        continue;
      }

      // Retreive the translation from sibling to |layer|. The accumulated
      // visible region is currently oriented with |layer|.
      auto siblingOffset = IntPoint::Round(siblingMatrix.GetTranslation());
      nsIntRegion siblingVisibleRegion(
          sibling->GetLocalVisibleRegion().ToUnknownRegion());
      // Translate the siblings region to |layer|'s origin.
      siblingVisibleRegion.MoveBy(-siblingOffset.x, -siblingOffset.y);
      // Apply the sibling's clip.
      // Layer clip rects are not affected by the layer's transform.
      Maybe<ParentLayerIntRect> clipRect = sibling->GetLocalClipRect();
      if (clipRect) {
        siblingVisibleRegion.AndWith(clipRect->ToUnknownRect());
      }
      // Subtract the sibling visible region from the visible region of |this|.
      aResult.SubOut(siblingVisibleRegion);
    }

    // Keep track of the total offset for aLayerOffset.  We use this in plugin
    // positioning code.
    offset += currentLayerOffset;
  }

  *aLayerOffset = IntPoint(offset.x, offset.y);
  return true;
}

Maybe<ParentLayerIntRect> Layer::GetCombinedClipRect() const {
  Maybe<ParentLayerIntRect> clip = GetClipRect();

  clip = IntersectMaybeRects(clip, GetScrolledClipRect());

  for (size_t i = 0; i < mScrollMetadata.Length(); i++) {
    clip = IntersectMaybeRects(clip, mScrollMetadata[i].GetClipRect());
  }

  return clip;
}

ContainerLayer::ContainerLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData),
      mFirstChild(nullptr),
      mLastChild(nullptr),
      mPreXScale(1.0f),
      mPreYScale(1.0f),
      mInheritedXScale(1.0f),
      mInheritedYScale(1.0f),
      mPresShellResolution(1.0f),
      mUseIntermediateSurface(false),
      mSupportsComponentAlphaChildren(false),
      mMayHaveReadbackChild(false),
      mChildrenChanged(false) {}

ContainerLayer::~ContainerLayer() = default;

bool ContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter) {
  if (aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if (aChild->GetParent()) {
    NS_ERROR("aChild already in the tree");
    return false;
  }
  if (aChild->GetNextSibling() || aChild->GetPrevSibling()) {
    NS_ERROR("aChild already has siblings?");
    return false;
  }
  if (aAfter &&
      (aAfter->Manager() != Manager() || aAfter->GetParent() != this)) {
    NS_ERROR("aAfter is not our child");
    return false;
  }

  aChild->SetParent(this);
  if (aAfter == mLastChild) {
    mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    NS_ADDREF(aChild);
    DidInsertChild(aChild);
    return true;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  DidInsertChild(aChild);
  return true;
}

void ContainerLayer::RemoveAllChildren() {
  // Optimizes "while (mFirstChild) ContainerLayer::RemoveChild(mFirstChild);"
  Layer* current = mFirstChild;

  // This is inlining DidRemoveChild() on each layer; we can skip the calls
  // to NotifyPaintedLayerRemoved as it gets taken care of when as we call
  // NotifyRemoved prior to removing any layers.
  while (current) {
    Layer* next = current->GetNextSibling();
    if (current->GetType() == TYPE_READBACK) {
      static_cast<ReadbackLayer*>(current)->NotifyRemoved();
    }
    current = next;
  }

  current = mFirstChild;
  mFirstChild = nullptr;
  while (current) {
    MOZ_ASSERT(!current->GetPrevSibling());

    Layer* next = current->GetNextSibling();
    current->SetParent(nullptr);
    current->SetNextSibling(nullptr);
    if (next) {
      next->SetPrevSibling(nullptr);
    }
    NS_RELEASE(current);
    current = next;
  }
}

// Note that ContainerLayer::RemoveAllChildren is an optimized
// version of this code; if you make changes to ContainerLayer::RemoveChild
// consider whether the matching changes need to be made to
// ContainerLayer::RemoveAllChildren
bool ContainerLayer::RemoveChild(Layer* aChild) {
  if (aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if (aChild->GetParent() != this) {
    NS_ERROR("aChild not our child");
    return false;
  }

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    this->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    this->mLastChild = prev;
  }

  aChild->SetNextSibling(nullptr);
  aChild->SetPrevSibling(nullptr);
  aChild->SetParent(nullptr);

  this->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
  return true;
}

bool ContainerLayer::RepositionChild(Layer* aChild, Layer* aAfter) {
  if (aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if (aChild->GetParent() != this) {
    NS_ERROR("aChild not our child");
    return false;
  }
  if (aAfter &&
      (aAfter->Manager() != Manager() || aAfter->GetParent() != this)) {
    NS_ERROR("aAfter is not our child");
    return false;
  }
  if (aChild == aAfter) {
    NS_ERROR("aChild cannot be the same as aAfter");
    return false;
  }

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev == aAfter) {
    // aChild is already in the correct position, nothing to do.
    return true;
  }
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    mLastChild = prev;
  }
  if (!aAfter) {
    aChild->SetPrevSibling(nullptr);
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    return true;
  }

  Layer* afterNext = aAfter->GetNextSibling();
  if (afterNext) {
    afterNext->SetPrevSibling(aChild);
  } else {
    mLastChild = aChild;
  }
  aAfter->SetNextSibling(aChild);
  aChild->SetPrevSibling(aAfter);
  aChild->SetNextSibling(afterNext);
  return true;
}

void ContainerLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs) {
  aAttrs = ContainerLayerAttributes(mPreXScale, mPreYScale, mInheritedXScale,
                                    mInheritedYScale, mPresShellResolution);
}

bool ContainerLayer::Creates3DContextWithExtendingChildren() {
  if (Extend3DContext()) {
    return false;
  }
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    if (child->Extend3DContext()) {
      return true;
    }
  }
  return false;
}

RenderTargetIntRect ContainerLayer::GetIntermediateSurfaceRect() {
  NS_ASSERTION(mUseIntermediateSurface, "Must have intermediate surface");
  LayerIntRect bounds = GetLocalVisibleRegion().GetBounds();
  return RenderTargetIntRect::FromUnknownRect(bounds.ToUnknownRect());
}

bool ContainerLayer::HasMultipleChildren() {
  uint32_t count = 0;
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    const Maybe<ParentLayerIntRect>& clipRect = child->GetLocalClipRect();
    if (clipRect && clipRect->IsEmpty()) continue;
    if (!child->Extend3DContext() && child->GetLocalVisibleRegion().IsEmpty())
      continue;
    ++count;
    if (count > 1) return true;
  }

  return false;
}

/**
 * Collect all leaf descendants of the current 3D context.
 */
void ContainerLayer::Collect3DContextLeaves(nsTArray<Layer*>& aToSort) {
  ForEachNode<ForwardIterator>((Layer*)this, [this, &aToSort](Layer* layer) {
    ContainerLayer* container = layer->AsContainerLayer();
    if (layer == this || (container && container->Extend3DContext() &&
                          !container->UseIntermediateSurface())) {
      return TraversalFlag::Continue;
    }
    aToSort.AppendElement(layer);
    return TraversalFlag::Skip;
  });
}

static nsTArray<LayerPolygon> SortLayersWithBSPTree(nsTArray<Layer*>& aArray) {
  std::list<LayerPolygon> inputLayers;

  // Build a list of polygons to be sorted.
  for (Layer* layer : aArray) {
    // Ignore invisible layers.
    if (!layer->IsVisible()) {
      continue;
    }

    const gfx::IntRect& bounds =
        layer->GetLocalVisibleRegion().GetBounds().ToUnknownRect();

    const gfx::Matrix4x4& transform = layer->GetEffectiveTransform();

    if (transform.IsSingular()) {
      // Transform cannot be inverted.
      continue;
    }

    gfx::Polygon polygon = gfx::Polygon::FromRect(gfx::Rect(bounds));

    // Transform the polygon to screen space.
    polygon.TransformToScreenSpace(transform);

    if (polygon.GetPoints().Length() >= 3) {
      inputLayers.push_back(LayerPolygon(layer, std::move(polygon)));
    }
  }

  if (inputLayers.empty()) {
    return nsTArray<LayerPolygon>();
  }

  // Build a BSP tree from the list of polygons.
  BSPTree<Layer> tree(inputLayers);

  nsTArray<LayerPolygon> orderedLayers(tree.GetDrawOrder());

  // Transform the polygons back to layer space.
  for (LayerPolygon& layerPolygon : orderedLayers) {
    gfx::Matrix4x4 inverse =
        layerPolygon.data->GetEffectiveTransform().Inverse();

    MOZ_ASSERT(layerPolygon.geometry);
    layerPolygon.geometry->TransformToLayerSpace(inverse);
  }

  return orderedLayers;
}

static nsTArray<LayerPolygon> StripLayerGeometry(
    const nsTArray<LayerPolygon>& aLayers) {
  nsTArray<LayerPolygon> layers;
  std::set<Layer*> uniqueLayers;

  for (const LayerPolygon& layerPolygon : aLayers) {
    auto result = uniqueLayers.insert(layerPolygon.data);

    if (result.second) {
      // Layer was added to the set.
      layers.AppendElement(LayerPolygon(layerPolygon.data));
    }
  }

  return layers;
}

nsTArray<LayerPolygon> ContainerLayer::SortChildrenBy3DZOrder(
    SortMode aSortMode) {
  AutoTArray<Layer*, 10> toSort;
  nsTArray<LayerPolygon> drawOrder;

  for (Layer* layer = GetFirstChild(); layer; layer = layer->GetNextSibling()) {
    ContainerLayer* container = layer->AsContainerLayer();

    if (container && container->Extend3DContext() &&
        !container->UseIntermediateSurface()) {
      // Collect 3D layers in toSort array.
      container->Collect3DContextLeaves(toSort);

      // Sort the 3D layers.
      if (toSort.Length() > 0) {
        nsTArray<LayerPolygon> sorted = SortLayersWithBSPTree(toSort);
        drawOrder.AppendElements(std::move(sorted));

        toSort.ClearAndRetainStorage();
      }

      continue;
    }

    drawOrder.AppendElement(LayerPolygon(layer));
  }

  if (aSortMode == SortMode::WITHOUT_GEOMETRY) {
    // Compositor does not support arbitrary layers, strip the layer geometry
    // and duplicate layers.
    return StripLayerGeometry(drawOrder);
  }

  return drawOrder;
}

bool ContainerLayer::AnyAncestorOrThisIs3DContextLeaf() {
  Layer* parent = this;
  while (parent != nullptr) {
    if (parent->Is3DContextLeaf()) {
      return true;
    }

    parent = parent->GetParent();
  }

  return false;
}

void ContainerLayer::DefaultComputeEffectiveTransforms(
    const Matrix4x4& aTransformToSurface) {
  Matrix residual;
  Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;

  // Keep 3D transforms for leaves to keep z-order sorting correct.
  if (!Extend3DContext() && !Is3DContextLeaf()) {
    idealTransform.ProjectTo2D();
  }

  bool useIntermediateSurface;
  if (HasMaskLayers() || GetForceIsolatedGroup()) {
    useIntermediateSurface = true;
#ifdef MOZ_DUMP_PAINTING
  } else if (gfxEnv::DumpPaintIntermediate() && !Extend3DContext()) {
    useIntermediateSurface = true;
#endif
  } else {
    /* Don't use an intermediate surface for opacity when it's within a 3d
     * context, since we'd rather keep the 3d effects. This matches the
     * WebKit/blink behaviour, but is changing in the latest spec.
     */
    float opacity = GetEffectiveOpacity();
    CompositionOp blendMode = GetEffectiveMixBlendMode();
    if ((HasMultipleChildren() || Creates3DContextWithExtendingChildren()) &&
        ((opacity != 1.0f && !Extend3DContext()) ||
         (blendMode != CompositionOp::OP_OVER))) {
      useIntermediateSurface = true;
    } else if ((!idealTransform.Is2D() || AnyAncestorOrThisIs3DContextLeaf()) &&
               Creates3DContextWithExtendingChildren()) {
      useIntermediateSurface = true;
    } else if (blendMode != CompositionOp::OP_OVER &&
               Manager()->BlendingRequiresIntermediateSurface()) {
      useIntermediateSurface = true;
    } else {
      useIntermediateSurface = false;
      gfx::Matrix contTransform;
      bool checkClipRect = false;
      bool checkMaskLayers = false;

      if (!idealTransform.Is2D(&contTransform)) {
        // In 3D case, always check if we should use IntermediateSurface.
        checkClipRect = true;
        checkMaskLayers = true;
      } else {
        contTransform.NudgeToIntegers();
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        if (!contTransform.PreservesAxisAlignedRectangles()) {
#else
        if (gfx::ThebesMatrix(contTransform).HasNonIntegerTranslation()) {
#endif
          checkClipRect = true;
        }
        /* In 2D case, only translation and/or positive scaling can be done w/o
         * using IntermediateSurface. Otherwise, when rotation or flip happen,
         * we should check whether to use IntermediateSurface.
         */
        if (contTransform.HasNonAxisAlignedTransform() ||
            contTransform.HasNegativeScaling()) {
          checkMaskLayers = true;
        }
      }

      if (checkClipRect || checkMaskLayers) {
        for (Layer* child = GetFirstChild(); child;
             child = child->GetNextSibling()) {
          const Maybe<ParentLayerIntRect>& clipRect = child->GetLocalClipRect();
          /* We can't (easily) forward our transform to children with a
           * non-empty clip rect since it would need to be adjusted for the
           * transform. See the calculations performed by CalculateScissorRect
           * above. Nor for a child with a mask layer.
           */
          if (checkClipRect && (clipRect && !clipRect->IsEmpty() &&
                                (child->Extend3DContext() ||
                                 !child->GetLocalVisibleRegion().IsEmpty()))) {
            useIntermediateSurface = true;
            break;
          }
          if (checkMaskLayers && child->HasMaskLayers()) {
            useIntermediateSurface = true;
            break;
          }
        }
      }
    }
  }

  NS_ASSERTION(!Extend3DContext() || !useIntermediateSurface,
               "Can't have an intermediate surface with preserve-3d!");

  if (useIntermediateSurface) {
    mEffectiveTransform = SnapTransformTranslation(idealTransform, &residual);
  } else {
    mEffectiveTransform = idealTransform;
  }

  // For layers extending 3d context, its ideal transform should be
  // applied on children.
  if (!Extend3DContext()) {
    // Without this projection, non-container children would get a 3D
    // transform while 2D is expected.
    idealTransform.ProjectTo2D();
  }
  mUseIntermediateSurface = useIntermediateSurface;
  if (useIntermediateSurface) {
    ComputeEffectiveTransformsForChildren(Matrix4x4::From2D(residual));
  } else {
    ComputeEffectiveTransformsForChildren(idealTransform);
  }

  ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
}

void ContainerLayer::DefaultComputeSupportsComponentAlphaChildren(
    bool* aNeedsSurfaceCopy) {
  if (!(GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA_DESCENDANT) ||
      !Manager()->AreComponentAlphaLayersEnabled()) {
    mSupportsComponentAlphaChildren = false;
    if (aNeedsSurfaceCopy) {
      *aNeedsSurfaceCopy = false;
    }
    return;
  }

  mSupportsComponentAlphaChildren = false;
  bool needsSurfaceCopy = false;
  CompositionOp blendMode = GetEffectiveMixBlendMode();
  if (UseIntermediateSurface()) {
    if (GetLocalVisibleRegion().GetNumRects() == 1 &&
        (GetContentFlags() & Layer::CONTENT_OPAQUE)) {
      mSupportsComponentAlphaChildren = true;
    } else {
      gfx::Matrix transform;
      if (HasOpaqueAncestorLayer(this) &&
          GetEffectiveTransform().Is2D(&transform) &&
          !gfx::ThebesMatrix(transform).HasNonIntegerTranslation() &&
          blendMode == gfx::CompositionOp::OP_OVER) {
        mSupportsComponentAlphaChildren = true;
        needsSurfaceCopy = true;
      }
    }
  } else if (blendMode == gfx::CompositionOp::OP_OVER) {
    mSupportsComponentAlphaChildren =
        (GetContentFlags() & Layer::CONTENT_OPAQUE) ||
        (GetParent() && GetParent()->SupportsComponentAlphaChildren());
  }

  if (aNeedsSurfaceCopy) {
    *aNeedsSurfaceCopy = mSupportsComponentAlphaChildren && needsSurfaceCopy;
  }
}

void ContainerLayer::ComputeEffectiveTransformsForChildren(
    const Matrix4x4& aTransformToSurface) {
  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    l->ComputeEffectiveTransforms(aTransformToSurface);
  }
}

/* static */
bool ContainerLayer::HasOpaqueAncestorLayer(Layer* aLayer) {
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE) return true;
  }
  return false;
}

// Note that ContainerLayer::RemoveAllChildren contains an optimized
// version of this code; if you make changes to ContainerLayer::DidRemoveChild
// consider whether the matching changes need to be made to
// ContainerLayer::RemoveAllChildren
void ContainerLayer::DidRemoveChild(Layer* aLayer) {
  PaintedLayer* tl = aLayer->AsPaintedLayer();
  if (tl && tl->UsedForReadback()) {
    for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
      if (l->GetType() == TYPE_READBACK) {
        static_cast<ReadbackLayer*>(l)->NotifyPaintedLayerRemoved(tl);
      }
    }
  }
  if (aLayer->GetType() == TYPE_READBACK) {
    static_cast<ReadbackLayer*>(aLayer)->NotifyRemoved();
  }
}

void ContainerLayer::DidInsertChild(Layer* aLayer) {
  if (aLayer->GetType() == TYPE_READBACK) {
    mMayHaveReadbackChild = true;
  }
}

void RefLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs) {
  aAttrs = RefLayerAttributes(GetReferentId(), mEventRegionsOverride,
                              mRemoteDocumentSize);
}

static void PrintInfo(std::stringstream& aStream, HostLayer* aLayerComposite);

#ifdef MOZ_DUMP_PAINTING
template <typename T>
void WriteSnapshotToDumpFile_internal(T* aObj, DataSourceSurface* aSurf) {
  nsCString string(aObj->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aObj);
  if (gfxUtils::sDumpPaintFile != stderr) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, R"(array["%s"]=")",
                   string.BeginReading());
  }
  gfxUtils::DumpAsDataURI(aSurf, gfxUtils::sDumpPaintFile);
  if (gfxUtils::sDumpPaintFile != stderr) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, R"(";)");
  }
}

void WriteSnapshotToDumpFile(Layer* aLayer, DataSourceSurface* aSurf) {
  WriteSnapshotToDumpFile_internal(aLayer, aSurf);
}

void WriteSnapshotToDumpFile(LayerManager* aManager, DataSourceSurface* aSurf) {
  WriteSnapshotToDumpFile_internal(aManager, aSurf);
}

void WriteSnapshotToDumpFile(Compositor* aCompositor, DrawTarget* aTarget) {
  RefPtr<SourceSurface> surf = aTarget->Snapshot();
  RefPtr<DataSourceSurface> dSurf = surf->GetDataSurface();
  WriteSnapshotToDumpFile_internal(aCompositor, dSurf);
}
#endif

void Layer::Dump(std::stringstream& aStream, const char* aPrefix,
                 bool aDumpHtml, bool aSorted,
                 const Maybe<gfx::Polygon>& aGeometry) {
#ifdef MOZ_DUMP_PAINTING
  bool dumpCompositorTexture = gfxEnv::DumpCompositorTextures() &&
                               AsHostLayer() &&
                               AsHostLayer()->GetCompositableHost();
  bool dumpClientTexture = gfxEnv::DumpPaint() && AsShadowableLayer() &&
                           AsShadowableLayer()->GetCompositableClient();
  nsCString layerId(Name());
  layerId.Append('-');
  layerId.AppendInt((uint64_t)this);
#endif
  if (aDumpHtml) {
    aStream << nsPrintfCString(R"(<li><a id="%p" )", this).get();
#ifdef MOZ_DUMP_PAINTING
    if (dumpCompositorTexture || dumpClientTexture) {
      aStream << nsPrintfCString(R"lit(href="javascript:ViewImage('%s')")lit",
                                 layerId.BeginReading())
                     .get();
    }
#endif
    aStream << ">";
  }
  DumpSelf(aStream, aPrefix, aGeometry);

#ifdef MOZ_DUMP_PAINTING
  if (dumpCompositorTexture) {
    AsHostLayer()->GetCompositableHost()->Dump(aStream, aPrefix, aDumpHtml);
  } else if (dumpClientTexture) {
    if (aDumpHtml) {
      aStream << nsPrintfCString(R"(<script>array["%s"]=")",
                                 layerId.BeginReading())
                     .get();
    }
    AsShadowableLayer()->GetCompositableClient()->Dump(
        aStream, aPrefix, aDumpHtml, TextureDumpMode::DoNotCompress);
    if (aDumpHtml) {
      aStream << R"(";</script>)";
    }
  }
#endif

  if (aDumpHtml) {
    aStream << "</a>";
#ifdef MOZ_DUMP_PAINTING
    if (dumpClientTexture) {
      aStream << nsPrintfCString("<br><img id=\"%s\">\n",
                                 layerId.BeginReading())
                     .get();
    }
#endif
  }

  if (Layer* mask = GetMaskLayer()) {
    aStream << nsPrintfCString("%s  Mask layer:\n", aPrefix).get();
    nsAutoCString pfx(aPrefix);
    pfx += "    ";
    mask->Dump(aStream, pfx.get(), aDumpHtml);
  }

  for (size_t i = 0; i < GetAncestorMaskLayerCount(); i++) {
    aStream << nsPrintfCString("%s  Ancestor mask layer %d:\n", aPrefix,
                               uint32_t(i))
                   .get();
    nsAutoCString pfx(aPrefix);
    pfx += "    ";
    GetAncestorMaskLayerAt(i)->Dump(aStream, pfx.get(), aDumpHtml);
  }

#ifdef MOZ_DUMP_PAINTING
  for (size_t i = 0; i < mExtraDumpInfo.Length(); i++) {
    const nsCString& str = mExtraDumpInfo[i];
    aStream << aPrefix << "  Info:\n" << str.get();
  }
#endif

  if (ContainerLayer* container = AsContainerLayer()) {
    nsTArray<LayerPolygon> children;
    if (aSorted) {
      children = container->SortChildrenBy3DZOrder(
          ContainerLayer::SortMode::WITH_GEOMETRY);
    } else {
      for (Layer* l = container->GetFirstChild(); l; l = l->GetNextSibling()) {
        children.AppendElement(LayerPolygon(l));
      }
    }
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    if (aDumpHtml) {
      aStream << "<ul>";
    }

    for (LayerPolygon& child : children) {
      child.data->Dump(aStream, pfx.get(), aDumpHtml, aSorted, child.geometry);
    }

    if (aDumpHtml) {
      aStream << "</ul>";
    }
  }

  if (aDumpHtml) {
    aStream << "</li>";
  }
}

static void DumpGeometry(std::stringstream& aStream,
                         const Maybe<gfx::Polygon>& aGeometry) {
  aStream << " [geometry=[";

  const nsTArray<gfx::Point4D>& points = aGeometry->GetPoints();
  for (size_t i = 0; i < points.Length(); ++i) {
    const gfx::IntPoint point = TruncatedToInt(points[i].As2DPoint());
    aStream << point;
    if (i != points.Length() - 1) {
      aStream << ",";
    }
  }

  aStream << "]]";
}

void Layer::DumpSelf(std::stringstream& aStream, const char* aPrefix,
                     const Maybe<gfx::Polygon>& aGeometry) {
  PrintInfo(aStream, aPrefix);

  if (aGeometry) {
    DumpGeometry(aStream, aGeometry);
  }

  aStream << "\n";
}

void Layer::Dump(layerscope::LayersPacket* aPacket, const void* aParent) {
  DumpPacket(aPacket, aParent);

  if (Layer* kid = GetFirstChild()) {
    kid->Dump(aPacket, this);
  }

  if (Layer* next = GetNextSibling()) {
    next->Dump(aPacket, aParent);
  }
}

void Layer::SetDisplayListLog(const char* log) {
  if (gfxUtils::DumpDisplayList()) {
    mDisplayListLog = log;
  }
}

void Layer::GetDisplayListLog(nsCString& log) {
  log.SetLength(0);

  if (gfxUtils::DumpDisplayList()) {
    // This function returns a plain text string which consists of two things
    //   1. DisplayList log.
    //   2. Memory address of this layer.
    // We know the target layer of each display item by information in #1.
    // Here is an example of a Text display item line log in #1
    //   Text p=0xa9850c00 f=0x0xaa405b00(.....
    // f keeps the address of the target client layer of a display item.
    // For LayerScope, display-item-to-client-layer mapping is not enough since
    // LayerScope, which lives in the chrome process, knows only composite
    // layers. As so, we need display-item-to-client-layer-to-layer-composite
    // mapping. That's the reason we insert #2 into the log
    log.AppendPrintf("0x%p\n%s", (void*)this, mDisplayListLog.get());
  }
}

void Layer::Log(const char* aPrefix) {
  if (!IsLogEnabled()) return;

  LogSelf(aPrefix);

  if (Layer* kid = GetFirstChild()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    kid->Log(pfx.get());
  }

  if (Layer* next = GetNextSibling()) next->Log(aPrefix);
}

void Layer::LogSelf(const char* aPrefix) {
  if (!IsLogEnabled()) return;

  std::stringstream ss;
  PrintInfo(ss, aPrefix);
  MOZ_LAYERS_LOG(("%s", ss.str().c_str()));

  if (mMaskLayer) {
    nsAutoCString pfx(aPrefix);
    pfx += R"(   \ MaskLayer )";
    mMaskLayer->LogSelf(pfx.get());
  }
}

void Layer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  aStream << aPrefix;
  aStream
      << nsPrintfCString("%s%s (0x%p)", mManager->Name(), Name(), this).get();

  layers::PrintInfo(aStream, AsHostLayer());

  if (mClipRect) {
    aStream << " [clip=" << *mClipRect << "]";
  }
  if (mSimpleAttrs.GetScrolledClip()) {
    aStream << " [scrolled-clip="
            << mSimpleAttrs.GetScrolledClip()->GetClipRect() << "]";
    if (const Maybe<size_t>& ix =
            mSimpleAttrs.GetScrolledClip()->GetMaskLayerIndex()) {
      aStream << " [scrolled-mask=" << ix.value() << "]";
    }
  }
  if (1.0 != mSimpleAttrs.GetPostXScale() ||
      1.0 != mSimpleAttrs.GetPostYScale()) {
    aStream << nsPrintfCString(" [postScale=%g, %g]",
                               mSimpleAttrs.GetPostXScale(),
                               mSimpleAttrs.GetPostYScale())
                   .get();
  }
  if (!GetBaseTransform().IsIdentity()) {
    aStream << " [transform=" << GetBaseTransform() << "]";
  }
  if (!GetEffectiveTransform().IsIdentity()) {
    aStream << " [effective-transform=" << GetEffectiveTransform() << "]";
  }
  if (GetTransformIsPerspective()) {
    aStream << " [perspective]";
  }
  if (!mVisibleRegion.IsEmpty()) {
    aStream << " [visible=" << mVisibleRegion << "]";
  } else {
    aStream << " [not visible]";
  }
  if (!mEventRegions.IsEmpty()) {
    aStream << " " << mEventRegions;
  }
  if (1.0 != GetOpacity()) {
    aStream << nsPrintfCString(" [opacity=%g]", GetOpacity()).get();
  }
  if (IsOpaque()) {
    aStream << " [opaqueContent]";
  }
  if (GetContentFlags() & CONTENT_COMPONENT_ALPHA) {
    aStream << " [componentAlpha]";
  }
  if (GetContentFlags() & CONTENT_BACKFACE_HIDDEN) {
    aStream << " [backfaceHidden]";
  }
  if (Extend3DContext()) {
    aStream << " [extend3DContext]";
  }
  if (Combines3DTransformWithAncestors()) {
    aStream << " [combines3DTransformWithAncestors]";
  }
  if (Is3DContextLeaf()) {
    aStream << " [is3DContextLeaf]";
  }
  if (Maybe<FrameMetrics::ViewID> viewId = GetAsyncZoomContainerId()) {
    aStream << nsPrintfCString(" [asyncZoomContainer scrollId=%" PRIu64 "]",
                               *viewId)
                   .get();
  }
  if (IsScrollbarContainer()) {
    aStream << " [scrollbar]";
  }
  if (GetScrollbarData().IsThumb()) {
    if (Maybe<ScrollDirection> thumbDirection = GetScrollbarData().mDirection) {
      if (*thumbDirection == ScrollDirection::eVertical) {
        aStream << nsPrintfCString(" [vscrollbar=%" PRIu64 "]",
                                   GetScrollbarData().mTargetViewId)
                       .get();
      }
      if (*thumbDirection == ScrollDirection::eHorizontal) {
        aStream << nsPrintfCString(" [hscrollbar=%" PRIu64 "]",
                                   GetScrollbarData().mTargetViewId)
                       .get();
      }
    }
  }
  if (GetIsFixedPosition()) {
    LayerPoint anchor = GetFixedPositionAnchor();
    aStream << nsPrintfCString(
                   " [isFixedPosition scrollId=%" PRIu64
                   " sides=0x%x anchor=%s]",
                   GetFixedPositionScrollContainerId(),
                   static_cast<unsigned int>(GetFixedPositionSides()),
                   ToString(anchor).c_str())
                   .get();
  }
  if (GetIsStickyPosition()) {
    aStream << nsPrintfCString(" [isStickyPosition scrollId=%" PRIu64
                               " outer=(%.3f,%.3f)-(%.3f,%.3f) "
                               "inner=(%.3f,%.3f)-(%.3f,%.3f)]",
                               GetStickyScrollContainerId(),
                               GetStickyScrollRangeOuter().X(),
                               GetStickyScrollRangeOuter().Y(),
                               GetStickyScrollRangeOuter().XMost(),
                               GetStickyScrollRangeOuter().YMost(),
                               GetStickyScrollRangeInner().X(),
                               GetStickyScrollRangeInner().Y(),
                               GetStickyScrollRangeInner().XMost(),
                               GetStickyScrollRangeInner().YMost())
                   .get();
  }
  if (mMaskLayer) {
    aStream << nsPrintfCString(" [mMaskLayer=%p]", mMaskLayer.get()).get();
  }
  for (uint32_t i = 0; i < mScrollMetadata.Length(); i++) {
    if (!mScrollMetadata[i].IsDefault()) {
      aStream << " [metrics" << i << "=" << mScrollMetadata[i] << "]";
    }
  }
  // FIXME: On the compositor thread, we don't set mAnimationInfo::mAnimations,
  // All animations are transformed by AnimationHelper::ExtractAnimations() into
  // mAnimationInfo.mPropertyAnimationGroups, instead. So if we want to check
  // if layer trees are properly synced up across processes, we should dump
  // mAnimationInfo.mPropertyAnimationGroups for the compositor thread.
  // (See AnimationInfo.h for more details.)
  if (!mAnimationInfo.GetAnimations().IsEmpty()) {
    aStream << nsPrintfCString(" [%d animations with id=%" PRIu64 " ]",
                               (int)mAnimationInfo.GetAnimations().Length(),
                               mAnimationInfo.GetCompositorAnimationsId())
                   .get();
  }
}

// The static helper function sets the transform matrix into the packet
static void DumpTransform(layerscope::LayersPacket::Layer::Matrix* aLayerMatrix,
                          const Matrix4x4& aMatrix) {
  aLayerMatrix->set_is2d(aMatrix.Is2D());
  if (aMatrix.Is2D()) {
    Matrix m = aMatrix.As2D();
    aLayerMatrix->set_isid(m.IsIdentity());
    if (!m.IsIdentity()) {
      aLayerMatrix->add_m(m._11);
      aLayerMatrix->add_m(m._12);
      aLayerMatrix->add_m(m._21);
      aLayerMatrix->add_m(m._22);
      aLayerMatrix->add_m(m._31);
      aLayerMatrix->add_m(m._32);
    }
  } else {
    aLayerMatrix->add_m(aMatrix._11);
    aLayerMatrix->add_m(aMatrix._12);
    aLayerMatrix->add_m(aMatrix._13);
    aLayerMatrix->add_m(aMatrix._14);
    aLayerMatrix->add_m(aMatrix._21);
    aLayerMatrix->add_m(aMatrix._22);
    aLayerMatrix->add_m(aMatrix._23);
    aLayerMatrix->add_m(aMatrix._24);
    aLayerMatrix->add_m(aMatrix._31);
    aLayerMatrix->add_m(aMatrix._32);
    aLayerMatrix->add_m(aMatrix._33);
    aLayerMatrix->add_m(aMatrix._34);
    aLayerMatrix->add_m(aMatrix._41);
    aLayerMatrix->add_m(aMatrix._42);
    aLayerMatrix->add_m(aMatrix._43);
    aLayerMatrix->add_m(aMatrix._44);
  }
}

// The static helper function sets the IntRect into the packet
template <typename T, typename Sub, typename Point, typename SizeT,
          typename MarginT>
static void DumpRect(layerscope::LayersPacket::Layer::Rect* aLayerRect,
                     const BaseRect<T, Sub, Point, SizeT, MarginT>& aRect) {
  aLayerRect->set_x(aRect.X());
  aLayerRect->set_y(aRect.Y());
  aLayerRect->set_w(aRect.Width());
  aLayerRect->set_h(aRect.Height());
}

// The static helper function sets the nsIntRegion into the packet
static void DumpRegion(layerscope::LayersPacket::Layer::Region* aLayerRegion,
                       const nsIntRegion& aRegion) {
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    DumpRect(aLayerRegion->add_r(), iter.Get());
  }
}

void Layer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent) {
  // Add a new layer (UnknownLayer)
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->add_layer();
  // Basic information
  layer->set_type(LayersPacket::Layer::UnknownLayer);
  layer->set_ptr(reinterpret_cast<uint64_t>(this));
  layer->set_parentptr(reinterpret_cast<uint64_t>(aParent));
  // Shadow
  if (HostLayer* lc = AsHostLayer()) {
    LayersPacket::Layer::Shadow* s = layer->mutable_shadow();
    if (const Maybe<ParentLayerIntRect>& clipRect = lc->GetShadowClipRect()) {
      DumpRect(s->mutable_clip(), *clipRect);
    }
    if (!lc->GetShadowBaseTransform().IsIdentity()) {
      DumpTransform(s->mutable_transform(), lc->GetShadowBaseTransform());
    }
    if (!lc->GetShadowVisibleRegion().IsEmpty()) {
      DumpRegion(s->mutable_vregion(),
                 lc->GetShadowVisibleRegion().ToUnknownRegion());
    }
  }
  // Clip
  if (mClipRect) {
    DumpRect(layer->mutable_clip(), *mClipRect);
  }
  // Transform
  if (!GetBaseTransform().IsIdentity()) {
    DumpTransform(layer->mutable_transform(), GetBaseTransform());
  }
  // Visible region
  if (!mVisibleRegion.ToUnknownRegion().IsEmpty()) {
    DumpRegion(layer->mutable_vregion(), mVisibleRegion.ToUnknownRegion());
  }
  // EventRegions
  if (!mEventRegions.IsEmpty()) {
    const EventRegions& e = mEventRegions;
    if (!e.mHitRegion.IsEmpty()) {
      DumpRegion(layer->mutable_hitregion(), e.mHitRegion);
    }
    if (!e.mDispatchToContentHitRegion.IsEmpty()) {
      DumpRegion(layer->mutable_dispatchregion(),
                 e.mDispatchToContentHitRegion);
    }
    if (!e.mNoActionRegion.IsEmpty()) {
      DumpRegion(layer->mutable_noactionregion(), e.mNoActionRegion);
    }
    if (!e.mHorizontalPanRegion.IsEmpty()) {
      DumpRegion(layer->mutable_hpanregion(), e.mHorizontalPanRegion);
    }
    if (!e.mVerticalPanRegion.IsEmpty()) {
      DumpRegion(layer->mutable_vpanregion(), e.mVerticalPanRegion);
    }
  }
  // Opacity
  layer->set_opacity(GetOpacity());
  // Content opaque
  layer->set_copaque(static_cast<bool>(GetContentFlags() & CONTENT_OPAQUE));
  // Component alpha
  layer->set_calpha(
      static_cast<bool>(GetContentFlags() & CONTENT_COMPONENT_ALPHA));
  // Vertical or horizontal bar
  if (GetScrollbarData().mScrollbarLayerType ==
      layers::ScrollbarLayerType::Thumb) {
    layer->set_direct(*GetScrollbarData().mDirection ==
                              ScrollDirection::eVertical
                          ? LayersPacket::Layer::VERTICAL
                          : LayersPacket::Layer::HORIZONTAL);
    layer->set_barid(GetScrollbarData().mTargetViewId);
  }

  // Mask layer
  if (mMaskLayer) {
    layer->set_mask(reinterpret_cast<uint64_t>(mMaskLayer.get()));
  }

  // DisplayList log.
  if (mDisplayListLog.Length() > 0) {
    layer->set_displaylistloglength(mDisplayListLog.Length());
    auto compressedData =
        MakeUnique<char[]>(LZ4::maxCompressedSize(mDisplayListLog.Length()));
    int compressedSize =
        LZ4::compress((char*)mDisplayListLog.get(), mDisplayListLog.Length(),
                      compressedData.get());
    layer->set_displaylistlog(compressedData.get(), compressedSize);
  }
}

bool Layer::IsBackfaceHidden() {
  if (GetContentFlags() & CONTENT_BACKFACE_HIDDEN) {
    Layer* container = AsContainerLayer() ? this : GetParent();
    if (container) {
      // The effective transform can include non-preserve-3d parent
      // transforms, since we don't always require an intermediate.
      if (container->Extend3DContext() || container->Is3DContextLeaf()) {
        return container->GetEffectiveTransform().IsBackfaceVisible();
      }
      return container->GetBaseTransform().IsBackfaceVisible();
    }
  }
  return false;
}

UniquePtr<LayerUserData> Layer::RemoveUserData(void* aKey) {
  UniquePtr<LayerUserData> d(static_cast<LayerUserData*>(
      mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
  return d;
}

void Layer::SetManager(LayerManager* aManager, HostLayer* aSelf) {
  // No one should be calling this for weird reasons.
  MOZ_ASSERT(aSelf);
  MOZ_ASSERT(aSelf->GetLayer() == this);
  mManager = aManager;
}

void PaintedLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  nsIntRegion validRegion = GetValidRegion();
  if (!validRegion.IsEmpty()) {
    aStream << " [valid=" << validRegion << "]";
  }
}

void PaintedLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                              const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::PaintedLayer);
  nsIntRegion validRegion = GetValidRegion();
  if (!validRegion.IsEmpty()) {
    DumpRegion(layer->mutable_valid(), validRegion);
  }
}

void ContainerLayer::PrintInfo(std::stringstream& aStream,
                               const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  if (UseIntermediateSurface()) {
    aStream << " [usesTmpSurf]";
  }
  if (1.0 != mPreXScale || 1.0 != mPreYScale) {
    aStream
        << nsPrintfCString(" [preScale=%g, %g]", mPreXScale, mPreYScale).get();
  }
  aStream << nsPrintfCString(" [presShellResolution=%g]", mPresShellResolution)
                 .get();
}

void ContainerLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                                const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::ContainerLayer);
}

void ColorLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  aStream << " [color=" << mColor << "] [bounds=" << mBounds << "]";
}

void ColorLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                            const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::ColorLayer);
  layer->set_color(mColor.ToABGR());
}

CanvasLayer::CanvasLayer(LayerManager* aManager, void* aImplData)
    : Layer(aManager, aImplData), mSamplingFilter(SamplingFilter::GOOD) {}

CanvasLayer::~CanvasLayer() = default;

void CanvasLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  if (mSamplingFilter != SamplingFilter::GOOD) {
    aStream << " [filter=" << mSamplingFilter << "]";
  }
}

// This help function is used to assign the correct enum value
// to the packet
static void DumpFilter(layerscope::LayersPacket::Layer* aLayer,
                       const SamplingFilter& aSamplingFilter) {
  using namespace layerscope;
  switch (aSamplingFilter) {
    case SamplingFilter::GOOD:
      aLayer->set_filter(LayersPacket::Layer::FILTER_GOOD);
      break;
    case SamplingFilter::LINEAR:
      aLayer->set_filter(LayersPacket::Layer::FILTER_LINEAR);
      break;
    case SamplingFilter::POINT:
      aLayer->set_filter(LayersPacket::Layer::FILTER_POINT);
      break;
    default:
      // ignore it
      break;
  }
}

void CanvasLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                             const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::CanvasLayer);
  DumpFilter(layer, mSamplingFilter);
}

RefPtr<CanvasRenderer> CanvasLayer::CreateOrGetCanvasRenderer() {
  if (!mCanvasRenderer) {
    mCanvasRenderer = CreateCanvasRendererInternal();
  }

  return mCanvasRenderer;
}

void ImageLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  if (mSamplingFilter != SamplingFilter::GOOD) {
    aStream << " [filter=" << mSamplingFilter << "]";
  }
}

void ImageLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                            const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::ImageLayer);
  DumpFilter(layer, mSamplingFilter);
}

void RefLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  ContainerLayer::PrintInfo(aStream, aPrefix);
  if (mId.IsValid()) {
    aStream << " [id=" << mId << "]";
  }
  if (mEventRegionsOverride & EventRegionsOverride::ForceDispatchToContent) {
    aStream << " [force-dtc]";
  }
  if (mEventRegionsOverride & EventRegionsOverride::ForceEmptyHitRegion) {
    aStream << " [force-ehr]";
  }
}

void RefLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                          const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::RefLayer);
  layer->set_refid(uint64_t(mId));
}

void ReadbackLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  Layer::PrintInfo(aStream, aPrefix);
  aStream << " [size=" << mSize << "]";
  if (mBackgroundLayer) {
    aStream << " [backgroundLayer="
            << nsPrintfCString("%p", mBackgroundLayer).get() << "]";
    aStream << " [backgroundOffset=" << mBackgroundLayerOffset << "]";
  } else if (mBackgroundColor.a == 1.f) {
    aStream << " [backgroundColor=" << mBackgroundColor << "]";
  } else {
    aStream << " [nobackground]";
  }
}

void ReadbackLayer::DumpPacket(layerscope::LayersPacket* aPacket,
                               const void* aParent) {
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer =
      aPacket->mutable_layer(aPacket->layer_size() - 1);
  layer->set_type(LayersPacket::Layer::ReadbackLayer);
  LayersPacket::Layer::Size* size = layer->mutable_size();
  size->set_w(mSize.width);
  size->set_h(mSize.height);
}

//--------------------------------------------------
// LayerManager

void LayerManager::Dump(std::stringstream& aStream, const char* aPrefix,
                        bool aDumpHtml, bool aSorted) {
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    aStream << "<ul><li>";
  }
#endif
  DumpSelf(aStream, aPrefix, aSorted);

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    aStream << nsPrintfCString("%s(null)\n", pfx.get()).get();
    if (aDumpHtml) {
      aStream << "</li></ul>";
    }
    return;
  }

  if (aDumpHtml) {
    aStream << "<ul>";
  }
  GetRoot()->Dump(aStream, pfx.get(), aDumpHtml, aSorted);
  if (aDumpHtml) {
    aStream << "</ul></li></ul>";
  }
  aStream << "\n";
}

void LayerManager::DumpSelf(std::stringstream& aStream, const char* aPrefix,
                            bool aSorted) {
  PrintInfo(aStream, aPrefix);
  aStream << " --- in "
          << (aSorted ? "3D-sorted rendering order" : "content order");
  aStream << "\n";
}

void LayerManager::Dump(bool aSorted) {
  std::stringstream ss;
  Dump(ss, "", false, aSorted);
  print_stderr(ss);
}

void LayerManager::Dump(layerscope::LayersPacket* aPacket) {
  DumpPacket(aPacket);

  if (GetRoot()) {
    GetRoot()->Dump(aPacket, this);
  }
}

void LayerManager::Log(const char* aPrefix) {
  if (!IsLogEnabled()) return;

  LogSelf(aPrefix);

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    MOZ_LAYERS_LOG(("%s(null)", pfx.get()));
    return;
  }

  GetRoot()->Log(pfx.get());
}

void LayerManager::LogSelf(const char* aPrefix) {
  nsAutoCString str;
  std::stringstream ss;
  PrintInfo(ss, aPrefix);
  MOZ_LAYERS_LOG(("%s", ss.str().c_str()));
}

void LayerManager::PrintInfo(std::stringstream& aStream, const char* aPrefix) {
  aStream << aPrefix
          << nsPrintfCString("%sLayerManager (0x%p)", Name(), this).get();
}

void LayerManager::DumpPacket(layerscope::LayersPacket* aPacket) {
  using namespace layerscope;
  // Add a new layer data (LayerManager)
  LayersPacket::Layer* layer = aPacket->add_layer();
  layer->set_type(LayersPacket::Layer::LayerManager);
  layer->set_ptr(reinterpret_cast<uint64_t>(this));
  // Layer Tree Root
  layer->set_parentptr(0);
}

/*static*/
bool LayerManager::IsLogEnabled() {
  return MOZ_LOG_TEST(GetLog(), LogLevel::Debug);
}

bool LayerManager::AddPendingScrollUpdateForNextTransaction(
    ScrollableLayerGuid::ViewID aScrollId,
    const ScrollPositionUpdate& aUpdateInfo) {
  Layer* withPendingTransform = DepthFirstSearch<ForwardIterator>(
      GetRoot(), [](Layer* aLayer) { return aLayer->HasPendingTransform(); });
  if (withPendingTransform) {
    return false;
  }

  mPendingScrollUpdates.LookupOrInsert(aScrollId).AppendElement(aUpdateInfo);
  return true;
}

Maybe<nsTArray<ScrollPositionUpdate>> LayerManager::GetPendingScrollInfoUpdate(
    ScrollableLayerGuid::ViewID aScrollId) {
  auto p = mPendingScrollUpdates.Lookup(aScrollId);
  if (!p) {
    return Nothing();
  }
  // We could have this function return a CopyableTArray or something, but it
  // seems better to avoid implicit copies and just do the one explicit copy
  // where we need it, here.
  nsTArray<ScrollPositionUpdate> copy;
  copy.AppendElements(p.Data());
  return Some(std::move(copy));
}

std::unordered_set<ScrollableLayerGuid::ViewID>
LayerManager::ClearPendingScrollInfoUpdate() {
  std::unordered_set<ScrollableLayerGuid::ViewID> scrollIds(
      mPendingScrollUpdates.Keys().cbegin(),
      mPendingScrollUpdates.Keys().cend());
  mPendingScrollUpdates.Clear();
  return scrollIds;
}

void PrintInfo(std::stringstream& aStream, HostLayer* aLayerComposite) {
  if (!aLayerComposite) {
    return;
  }
  if (const Maybe<ParentLayerIntRect>& clipRect =
          aLayerComposite->GetShadowClipRect()) {
    aStream << " [shadow-clip=" << *clipRect << "]";
  }
  if (!aLayerComposite->GetShadowBaseTransform().IsIdentity()) {
    aStream << " [shadow-transform="
            << aLayerComposite->GetShadowBaseTransform() << "]";
  }
  if (!aLayerComposite->GetLayer()->Extend3DContext() &&
      !aLayerComposite->GetShadowVisibleRegion().IsEmpty()) {
    aStream << " [shadow-visible=" << aLayerComposite->GetShadowVisibleRegion()
            << "]";
  }
}

void SetAntialiasingFlags(Layer* aLayer, DrawTarget* aTarget) {
  bool permitSubpixelAA =
      !(aLayer->GetContentFlags() & Layer::CONTENT_DISABLE_SUBPIXEL_AA);
  if (aTarget->IsCurrentGroupOpaque()) {
    aTarget->SetPermitSubpixelAA(permitSubpixelAA);
    return;
  }

  const IntRect& bounds =
      aLayer->GetVisibleRegion().GetBounds().ToUnknownRect();
  gfx::Rect transformedBounds = aTarget->GetTransform().TransformBounds(
      gfx::Rect(Float(bounds.X()), Float(bounds.Y()), Float(bounds.Width()),
                Float(bounds.Height())));
  transformedBounds.RoundOut();
  IntRect intTransformedBounds;
  transformedBounds.ToIntRect(&intTransformedBounds);
  permitSubpixelAA &=
      !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
      aTarget->GetOpaqueRect().Contains(intTransformedBounds);
  aTarget->SetPermitSubpixelAA(permitSubpixelAA);
}

IntRect ToOutsideIntRect(const gfxRect& aRect) {
  return IntRect::RoundOut(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
}

void RecordCompositionPayloadsPresented(
    const TimeStamp& aCompositionEndTime,
    const nsTArray<CompositionPayload>& aPayloads) {
  if (aPayloads.Length()) {
    TimeStamp presented = aCompositionEndTime;
    for (const CompositionPayload& payload : aPayloads) {
      if (profiler_can_accept_markers()) {
        MOZ_RELEASE_ASSERT(payload.mType <= kHighestCompositionPayloadType);
        nsAutoCString name(
            kCompositionPayloadTypeNames[uint8_t(payload.mType)]);
        name.AppendLiteral(" Payload Presented");
        // This doesn't really need to be a text marker. Once we have a version
        // of profiler_add_marker that accepts both a start time and an end
        // time, we could use that here.
        nsPrintfCString text(
            "Latency: %dms",
            int32_t((presented - payload.mTimeStamp).ToMilliseconds()));
        PROFILER_MARKER_TEXT(
            name, GRAPHICS,
            MarkerTiming::Interval(payload.mTimeStamp, presented), text);
      }

      if (payload.mType == CompositionPayloadType::eKeyPress) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::KEYPRESS_PRESENT_LATENCY, payload.mTimeStamp,
            presented);
      } else if (payload.mType == CompositionPayloadType::eAPZScroll) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::SCROLL_PRESENT_LATENCY, payload.mTimeStamp,
            presented);
      } else if (payload.mType ==
                 CompositionPayloadType::eMouseUpFollowedByClick) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::MOUSEUP_FOLLOWED_BY_CLICK_PRESENT_LATENCY,
            payload.mTimeStamp, presented);
      }
    }
  }
}

}  // namespace layers
}  // namespace mozilla
