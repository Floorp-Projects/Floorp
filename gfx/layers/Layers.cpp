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
#include "mozilla/gfx/MatrixFwd.h"              // for Float
#include "mozilla/gfx/Polygon.h"                // for Polygon, PolygonTyped
#include "mozilla/layers/BSPTree.h"             // for LayerPolygon, BSPTree
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/Compositor.h"          // for Compositor
#include "mozilla/layers/LayersMessages.h"  // for SpecificLayerAttributes, CompositorAnimations (ptr only), ContainerLayerAt...
#include "mozilla/layers/LayersTypes.h"  // for EventRegions, operator<<, CompositionPayload, CSSTransformMatrix, MOZ_LAYE...
#include "nsBaseHashtable.h"  // for nsBaseHashtable<>::Iterator, nsBaseHashtable<>::LookupResult
#include "nsISupportsUtils.h"  // for NS_ADDREF, NS_RELEASE
#include "nsPrintfCString.h"   // for nsPrintfCString
#include "nsRegionFwd.h"       // for IntRegion
#include "nsString.h"          // for nsTSubstring

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
  return false;
}

// NB: eventually these methods will be defined unconditionally, and
// can be moved into Layers.h
const Maybe<ParentLayerIntRect>& Layer::GetLocalClipRect() {
  return GetClipRect();
}

const LayerIntRegion& Layer::GetLocalVisibleRegion() {
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

RenderTargetIntRect Layer::CalculateScissorRect(
    const RenderTargetIntRect& aCurrentScissorRect) {
  return RenderTargetIntRect();
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
  return transform;
}

const CSSTransformMatrix Layer::GetTransformTyped() const {
  return ViewAs<CSSTransformMatrix>(GetTransform());
}

Matrix4x4 Layer::GetLocalTransform() { return GetTransform(); }

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
  return std::min(std::max(opacity, 0.0f), 1.0f);
}

float Layer::GetEffectiveOpacity() {
  float opacity = GetLocalOpacity();
  return opacity;
}

CompositionOp Layer::GetEffectiveMixBlendMode() {
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

  if (mClipRect) {
    aStream << " [clip=" << *mClipRect << "]";
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

bool Layer::IsBackfaceHidden() {
  return false;
}

UniquePtr<LayerUserData> Layer::RemoveUserData(void* aKey) {
  UniquePtr<LayerUserData> d(static_cast<LayerUserData*>(
      mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
  return d;
}

//--------------------------------------------------
// LayerManager

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
