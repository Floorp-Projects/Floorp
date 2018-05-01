/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERSCROLLDATAWRAPPER_H
#define GFX_WEBRENDERSCROLLDATAWRAPPER_H

#include "FrameMetrics.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {
namespace layers {

/*
 * This class is a wrapper to walk through a WebRenderScrollData object, with
 * an exposed API that is template-compatible to LayerMetricsWrapper. This allows
 * APZ to walk through both layer trees and WebRender scroll metadata structures
 * without a lot of code duplication.
 * (Note that not all functions from LayerMetricsWrapper are implemented here,
 * only the ones we've needed in APZ code so far.)
 *
 * A WebRenderScrollData object is basically a flattened layer tree, with a
 * number of WebRenderLayerScrollData objects that have a 1:1 correspondence
 * to layers in a layer tree. Therefore the mLayer pointer in this class can
 * be considered equivalent to the mLayer pointer in the LayerMetricsWrapper.
 * There are some extra fields (mData, mLayerIndex, mContainingSubtreeLastIndex)
 * to move around between these "layers" given the flattened representation.
 * The mMetadataIndex field in this class corresponds to the mIndex field in
 * LayerMetricsWrapper, as both classes also need to manage walking through
 * "virtual" container layers implied by the list of ScrollMetadata objects.
 *
 * One important note here is that this class holds a pointer to the "owning"
 * WebRenderScrollData. The caller must ensure that this class does not outlive
 * the owning WebRenderScrollData, or this may result in use-after-free errors.
 * This class being declared a MOZ_STACK_CLASS should help with that.
 *
 * Refer to LayerMetricsWrapper.h for actual documentation on the exposed API.
 */
class MOZ_STACK_CLASS WebRenderScrollDataWrapper {
public:
  // Basic constructor for external callers. Starts the walker at the root of
  // the tree.
  explicit WebRenderScrollDataWrapper(const APZUpdater& aUpdater,
                                      const WebRenderScrollData* aData = nullptr)
    : mUpdater(&aUpdater)
    , mData(aData)
    , mLayerIndex(0)
    , mContainingSubtreeLastIndex(0)
    , mLayer(nullptr)
    , mMetadataIndex(0)
  {
    if (!mData) {
      return;
    }
    mLayer = mData->GetLayerData(mLayerIndex);
    if (!mLayer) {
      return;
    }

    // sanity check on the data
    MOZ_ASSERT(mData->GetLayerCount() == (size_t)(1 + mLayer->GetDescendantCount()));
    mContainingSubtreeLastIndex = mData->GetLayerCount();

    // See documentation in LayerMetricsWrapper.h about this. mMetadataIndex
    // in this class is equivalent to mIndex in that class.
    mMetadataIndex = mLayer->GetScrollMetadataCount();
    if (mMetadataIndex > 0) {
      mMetadataIndex--;
    }
  }

private:
  // Internal constructor for walking from one WebRenderLayerScrollData to
  // another. In this case we need to recompute the mMetadataIndex to be the
  // "topmost" scroll metadata on the new layer.
  WebRenderScrollDataWrapper(const APZUpdater* aUpdater,
                             const WebRenderScrollData* aData,
                             size_t aLayerIndex,
                             size_t aContainingSubtreeLastIndex)
    : mUpdater(aUpdater)
    , mData(aData)
    , mLayerIndex(aLayerIndex)
    , mContainingSubtreeLastIndex(aContainingSubtreeLastIndex)
    , mLayer(nullptr)
    , mMetadataIndex(0)
  {
    MOZ_ASSERT(mData);
    mLayer = mData->GetLayerData(mLayerIndex);
    MOZ_ASSERT(mLayer);

    // See documentation in LayerMetricsWrapper.h about this. mMetadataIndex
    // in this class is equivalent to mIndex in that class.
    mMetadataIndex = mLayer->GetScrollMetadataCount();
    if (mMetadataIndex > 0) {
      mMetadataIndex--;
    }
  }

  // Internal constructor for walking from one metadata to another metadata on
  // the same WebRenderLayerScrollData.
  WebRenderScrollDataWrapper(const APZUpdater* aUpdater,
                             const WebRenderScrollData* aData,
                             size_t aLayerIndex,
                             size_t aContainingSubtreeLastIndex,
                             const WebRenderLayerScrollData* aLayer,
                             uint32_t aMetadataIndex)
    : mUpdater(aUpdater)
    , mData(aData)
    , mLayerIndex(aLayerIndex)
    , mContainingSubtreeLastIndex(aContainingSubtreeLastIndex)
    , mLayer(aLayer)
    , mMetadataIndex(aMetadataIndex)
  {
    MOZ_ASSERT(mData);
    MOZ_ASSERT(mLayer);
    MOZ_ASSERT(mLayer == mData->GetLayerData(mLayerIndex));
    MOZ_ASSERT(mMetadataIndex == 0 || mMetadataIndex < mLayer->GetScrollMetadataCount());
  }

public:
  bool IsValid() const
  {
    return mLayer != nullptr;
  }

  explicit operator bool() const
  {
    return IsValid();
  }

  WebRenderScrollDataWrapper GetLastChild() const
  {
    MOZ_ASSERT(IsValid());

    if (!AtBottomLayer()) {
      // If we're still walking around in the virtual container layers created
      // by the ScrollMetadata array, we just need to update the metadata index
      // and that's it.
      return WebRenderScrollDataWrapper(mUpdater, mData, mLayerIndex,
          mContainingSubtreeLastIndex, mLayer, mMetadataIndex - 1);
    }

    // Otherwise, we need to walk to a different WebRenderLayerScrollData in
    // mData.

    // Since mData contains the layer in depth-first, last-to-first order,
    // the index after mLayerIndex must be mLayerIndex's last child, if it
    // has any children (indicated by GetDescendantCount() > 0). Furthermore
    // we compute the first index outside the subtree rooted at this node
    // (in |subtreeLastIndex|) and pass that in to the child wrapper to use as
    // its mContainingSubtreeLastIndex.
    if (mLayer->GetDescendantCount() > 0) {
      size_t prevSiblingIndex = mLayerIndex + 1 + mLayer->GetDescendantCount();
      size_t subtreeLastIndex = std::min(mContainingSubtreeLastIndex, prevSiblingIndex);
      return WebRenderScrollDataWrapper(mUpdater, mData, mLayerIndex + 1, subtreeLastIndex);
    }

    // We've run out of descendants. But! If the original layer was a RefLayer,
    // then it connects to another layer tree and we need to traverse that too.
    // So return a WebRenderScrollDataWrapper for the root of the child layer
    // tree.
    if (mLayer->GetReferentId()) {
      return WebRenderScrollDataWrapper(*mUpdater,
          mUpdater->GetScrollData(*mLayer->GetReferentId()));
    }

    return WebRenderScrollDataWrapper(*mUpdater);
  }

  WebRenderScrollDataWrapper GetPrevSibling() const
  {
    MOZ_ASSERT(IsValid());

    if (!AtTopLayer()) {
      // The virtual container layers don't have siblings
      return WebRenderScrollDataWrapper(*mUpdater);
    }

    // Skip past the descendants to get to the previous sibling. However, we
    // might be at the last sibling already.
    size_t prevSiblingIndex = mLayerIndex + 1 + mLayer->GetDescendantCount();
    if (prevSiblingIndex < mContainingSubtreeLastIndex) {
      return WebRenderScrollDataWrapper(mUpdater, mData, prevSiblingIndex, mContainingSubtreeLastIndex);
    }
    return WebRenderScrollDataWrapper(*mUpdater);
  }

  const ScrollMetadata& Metadata() const
  {
    MOZ_ASSERT(IsValid());

    if (mMetadataIndex >= mLayer->GetScrollMetadataCount()) {
      return *ScrollMetadata::sNullMetadata;
    }
    return mLayer->GetScrollMetadata(*mData, mMetadataIndex);
  }

  const FrameMetrics& Metrics() const
  {
    return Metadata().GetMetrics();
  }

  AsyncPanZoomController* GetApzc() const
  {
    return nullptr;
  }

  void SetApzc(AsyncPanZoomController* aApzc) const
  {
  }

  const char* Name() const
  {
    return "WebRenderScrollDataWrapper";
  }

  gfx::Matrix4x4 GetTransform() const
  {
    MOZ_ASSERT(IsValid());

    // See WebRenderLayerScrollData::Initialize for more context. The ancestor
    // transform is associated with the "topmost" layer, and the transform is
    // associated with the "bottommost" layer. If there is only one
    // scrollmetadata on the layer, then it is both "topmost" and "bottommost"
    // and we combine the two transforms.

    gfx::Matrix4x4 transform;
    if (AtTopLayer()) {
      transform = mLayer->GetAncestorTransform();
    }
    if (AtBottomLayer()) {
      transform = transform * mLayer->GetTransform();
    }
    return transform;
  }

  CSSTransformMatrix GetTransformTyped() const
  {
    return ViewAs<CSSTransformMatrix>(GetTransform());
  }

  bool TransformIsPerspective() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetTransformIsPerspective();
    }
    return false;
  }

  EventRegions GetEventRegions() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetEventRegions();
    }
    return EventRegions();
  }

  LayerIntRegion GetVisibleRegion() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetVisibleRegion();
    }

    return ViewAs<LayerPixel>(
        TransformBy(mLayer->GetTransformTyped(), mLayer->GetVisibleRegion()),
        PixelCastJustification::MovingDownToChildren);
  }

  Maybe<LayersId> GetReferentId() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetReferentId();
    }
    return Nothing();
  }

  Maybe<ParentLayerIntRect> GetClipRect() const
  {
    // TODO
    return Nothing();
  }

  EventRegionsOverride GetEventRegionsOverride() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer->GetEventRegionsOverride();
  }

  const ScrollbarData& GetScrollbarData() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer->GetScrollbarData();
  }

  uint64_t GetScrollbarAnimationId() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer->GetScrollbarAnimationId();
  }

  FrameMetrics::ViewID GetFixedPositionScrollContainerId() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer->GetFixedPositionScrollContainerId();
  }

  bool IsBackfaceHidden() const
  {
    // This is only used by APZCTM hit testing, and WR does its own
    // hit testing, so no need to implement this.
    return false;
  }

  const void* GetLayer() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer;
  }

private:
  bool AtBottomLayer() const
  {
    return mMetadataIndex == 0;
  }

  bool AtTopLayer() const
  {
    return mLayer->GetScrollMetadataCount() == 0 || mMetadataIndex == mLayer->GetScrollMetadataCount() - 1;
  }

private:
  const APZUpdater* mUpdater;
  const WebRenderScrollData* mData;
  // The index (in mData->mLayerScrollData) of the WebRenderLayerScrollData this
  // wrapper is pointing to.
  size_t mLayerIndex;
  // The upper bound on the set of valid indices inside the subtree rooted at
  // the parent of this "layer". That is, any layer index |i| in the range
  // mLayerIndex <= i < mContainingSubtreeLastIndex is guaranteed to point to
  // a layer that is a descendant of "parent", where "parent" is the parent
  // layer of the layer at mLayerIndex. This is needed in order to implement
  // GetPrevSibling() correctly.
  size_t mContainingSubtreeLastIndex;
  // The WebRenderLayerScrollData this wrapper is pointing to.
  const WebRenderLayerScrollData* mLayer;
  // The index of the scroll metadata within mLayer that this wrapper is
  // pointing to.
  uint32_t mMetadataIndex;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERSCROLLDATAWRAPPER_H */
