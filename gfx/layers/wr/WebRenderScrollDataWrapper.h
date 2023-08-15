/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERSCROLLDATAWRAPPER_H
#define GFX_WEBRENDERSCROLLDATAWRAPPER_H

#include "FrameMetrics.h"
#include "mozilla/layers/APZUpdater.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {
namespace layers {

/**
 * A wrapper class around a target WebRenderLayerScrollData (henceforth,
 * "layer") that allows user code to walk through the ScrollMetadata objects
 * on the layer the same way it would walk through a layer tree.
 * Consider the following layer tree:
 *
 *                    +---+
 *                    | A |
 *                    +---+
 *                   /  |  \
 *                  /   |   \
 *                 /    |    \
 *            +---+  +-----+  +---+
 *            | B |  |  C  |  | D |
 *            +---+  +-----+  +---+
 *                   | SMn |
 *                   |  .  |
 *                   |  .  |
 *                   |  .  |
 *                   | SM1 |
 *                   | SM0 |
 *                   +-----+
 *                   /     \
 *                  /       \
 *             +---+         +---+
 *             | E |         | F |
 *             +---+         +---+
 *
 * In this layer tree, there are six layers with A being the root and B,D,E,F
 * being leaf nodes. Layer C is in the middle and has n+1 ScrollMetadata,
 * labelled SM0...SMn. SM0 is the ScrollMetadata you get by calling
 * c->GetScrollMetadata(0) and SMn is the ScrollMetadata you can obtain by
 * calling c->GetScrollMetadata(c->GetScrollMetadataCount() - 1). This layer
 * tree is conceptually equivalent to this one below:
 *
 *                    +---+
 *                    | A |
 *                    +---+
 *                   /  |  \
 *                  /   |   \
 *                 /    |    \
 *            +---+  +-----+  +---+
 *            | B |  | Cn  |  | D |
 *            +---+  +-----+  +---+
 *                      |
 *                      .
 *                      .
 *                      .
 *                      |
 *                   +-----+
 *                   | C1  |
 *                   +-----+
 *                      |
 *                   +-----+
 *                   | C0  |
 *                   +-----+
 *                   /     \
 *                  /       \
 *             +---+         +---+
 *             | E |         | F |
 *             +---+         +---+
 *
 * In this layer tree, the layer C has been expanded into a stack of layers
 * C1...Cn, where C1 has ScrollMetadata SM1 and Cn has ScrollMetdata Fn.
 *
 * The WebRenderScrollDataWrapper class allows client code to treat the first
 * layer tree as though it were the second. That is, instead of client code
 * having to iterate through the ScrollMetadata objects directly, it can use a
 * WebRenderScrollDataWrapper to encapsulate that aspect of the layer tree and
 * just walk the tree as if it were a stack of layers.
 *
 * The functions on this class do different things depending on which
 * simulated layer is being wrapped. For example, if the
 * WebRenderScrollDataWrapper is pretending to be C0, the GetPrevSibling()
 * function will return null even though the underlying layer C does actually
 * have a prev sibling. The WebRenderScrollDataWrapper pretending to be Cn will
 * return B as the prev sibling.
 *
 * Implementation notes:
 *
 * The AtTopLayer() and AtBottomLayer() functions in this class refer to
 * Cn and C0 in the second layer tree above; that is, they are predicates
 * to test if the wrapper is simulating the topmost or bottommost layer, as
 * those can have special behaviour.
 *
 * It is possible to wrap a nullptr in a WebRenderScrollDataWrapper, in which
 * case the IsValid() function will return false. This is required to allow
 * WebRenderScrollDataWrapper to be a MOZ_STACK_CLASS (desirable because it is
 * used in loops and recursion).
 *
 * This class purposely does not expose the wrapped layer directly to avoid
 * user code from accidentally calling functions directly on it. Instead
 * any necessary functions should be wrapped in this class. It does expose
 * the wrapped layer as a void* for printf purposes.
 *
 * The implementation may look like it special-cases mIndex == 0 and/or
 * GetScrollMetadataCount() == 0. This is an artifact of the fact that both
 * mIndex and GetScrollMetadataCount() are uint32_t and GetScrollMetadataCount()
 * can return 0 but mIndex cannot store -1. This seems better than the
 * alternative of making mIndex a int32_t that can store -1, but then having
 * to cast to uint32_t all over the place.
 *
 * Note that WebRenderLayerScrollData objects are owned by WebRenderScrollData,
 * which stores them in a flattened representation. The field mData,
 * mLayerIndex, and mContainingSubtreeIndex are used to move around the "layers"
 * given the flattened representation. The mMetadataIndex is used to move around
 * the ScrollMetadata within a single layer.
 *
 * One important note here is that this class holds a pointer to the "owning"
 * WebRenderScrollData. The caller must ensure that this class does not outlive
 * the owning WebRenderScrollData, or this may result in use-after-free errors.
 * This class being declared a MOZ_STACK_CLASS should help with that.
 */
class MOZ_STACK_CLASS WebRenderScrollDataWrapper final {
 public:
  // Basic constructor for external callers. Starts the walker at the root of
  // the tree.
  explicit WebRenderScrollDataWrapper(
      const APZUpdater& aUpdater, const WebRenderScrollData* aData = nullptr)
      : mUpdater(&aUpdater),
        mData(aData),
        mLayerIndex(0),
        mContainingSubtreeLastIndex(0),
        mLayer(nullptr),
        mMetadataIndex(0) {
    if (!mData) {
      return;
    }
    mLayer = mData->GetLayerData(mLayerIndex);
    if (!mLayer) {
      return;
    }

    // sanity check on the data
    MOZ_ASSERT(mData->GetLayerCount() ==
               (size_t)(1 + mLayer->GetDescendantCount()));
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
      : mUpdater(aUpdater),
        mData(aData),
        mLayerIndex(aLayerIndex),
        mContainingSubtreeLastIndex(aContainingSubtreeLastIndex),
        mLayer(nullptr),
        mMetadataIndex(0) {
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
      : mUpdater(aUpdater),
        mData(aData),
        mLayerIndex(aLayerIndex),
        mContainingSubtreeLastIndex(aContainingSubtreeLastIndex),
        mLayer(aLayer),
        mMetadataIndex(aMetadataIndex) {
    MOZ_ASSERT(mData);
    MOZ_ASSERT(mLayer);
    MOZ_ASSERT(mLayer == mData->GetLayerData(mLayerIndex));
    MOZ_ASSERT(mMetadataIndex == 0 ||
               mMetadataIndex < mLayer->GetScrollMetadataCount());
  }

 public:
  bool IsValid() const { return mLayer != nullptr; }

  explicit operator bool() const { return IsValid(); }

  WebRenderScrollDataWrapper GetLastChild() const {
    MOZ_ASSERT(IsValid());

    if (!AtBottomLayer()) {
      // If we're still walking around in the virtual container layers created
      // by the ScrollMetadata array, we just need to update the metadata index
      // and that's it.
      return WebRenderScrollDataWrapper(mUpdater, mData, mLayerIndex,
                                        mContainingSubtreeLastIndex, mLayer,
                                        mMetadataIndex - 1);
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
      // TODO(botond): Replace the min() with just prevSiblingIndex (which
      // should be <= mContainingSubtreeLastIndex).
      MOZ_ASSERT(prevSiblingIndex <= mContainingSubtreeLastIndex);
      size_t subtreeLastIndex =
          std::min(mContainingSubtreeLastIndex, prevSiblingIndex);
      return WebRenderScrollDataWrapper(mUpdater, mData, mLayerIndex + 1,
                                        subtreeLastIndex);
    }

    // We've run out of descendants. But! If the original layer was a RefLayer,
    // then it connects to another layer tree and we need to traverse that too.
    // So return a WebRenderScrollDataWrapper for the root of the child layer
    // tree.
    if (mLayer->GetReferentId()) {
      return WebRenderScrollDataWrapper(
          *mUpdater, mUpdater->GetScrollData(*mLayer->GetReferentId()));
    }

    return WebRenderScrollDataWrapper(*mUpdater);
  }

  WebRenderScrollDataWrapper GetPrevSibling() const {
    MOZ_ASSERT(IsValid());

    if (!AtTopLayer()) {
      // The virtual container layers don't have siblings
      return WebRenderScrollDataWrapper(*mUpdater);
    }

    // Skip past the descendants to get to the previous sibling. However, we
    // might be at the last sibling already.
    size_t prevSiblingIndex = mLayerIndex + 1 + mLayer->GetDescendantCount();
    if (prevSiblingIndex < mContainingSubtreeLastIndex) {
      return WebRenderScrollDataWrapper(mUpdater, mData, prevSiblingIndex,
                                        mContainingSubtreeLastIndex);
    }
    return WebRenderScrollDataWrapper(*mUpdater);
  }

  const ScrollMetadata& Metadata() const {
    MOZ_ASSERT(IsValid());

    if (mMetadataIndex >= mLayer->GetScrollMetadataCount()) {
      return *ScrollMetadata::sNullMetadata;
    }
    return mLayer->GetScrollMetadata(*mData, mMetadataIndex);
  }

  const FrameMetrics& Metrics() const { return Metadata().GetMetrics(); }

  AsyncPanZoomController* GetApzc() const { return nullptr; }

  void SetApzc(AsyncPanZoomController* aApzc) const {}

  const char* Name() const { return "WebRenderScrollDataWrapper"; }

  gfx::Matrix4x4 GetTransform() const {
    MOZ_ASSERT(IsValid());

    // See WebRenderLayerScrollData::Initialize for more context.
    //  * The ancestor transform is associated with whichever layer has scroll
    //    id matching GetAncestorTransformId().
    //  * The transform is associated with the "bottommost" layer.
    // Multiple transforms may apply to the same layer (e.g. if there is only
    // one scrollmetadata on the layer, then it is both "topmost" and
    // "bottommost"), so we may need to combine the transforms.

    gfx::Matrix4x4 transform;
    // The ancestor transform is usually emitted at the layer with the
    // matching scroll id. However, sometimes the transform ends up on
    // a node with no scroll metadata at all. In such cases we generate
    // a single layer, and the ancestor transform needs to be on that layer,
    // otherwise it will be lost.
    bool emitAncestorTransform =
        !Metrics().IsScrollable() ||
        Metrics().GetScrollId() == mLayer->GetAncestorTransformId();
    if (emitAncestorTransform) {
      transform = mLayer->GetAncestorTransform();
    }
    if (AtBottomLayer()) {
      transform = mLayer->GetTransform() * transform;
    }
    return transform;
  }

  CSSTransformMatrix GetTransformTyped() const {
    return ViewAs<CSSTransformMatrix>(GetTransform());
  }

  bool TransformIsPerspective() const {
    MOZ_ASSERT(IsValid());

    // mLayer->GetTransformIsPerspective() tells us whether
    // mLayer->GetTransform() is a perspective transform. Since
    // mLayer->GetTransform() is only used at the bottom layer, we only
    // need to check GetTransformIsPerspective() at the bottom layer too.
    if (AtBottomLayer()) {
      return mLayer->GetTransformIsPerspective();
    }
    return false;
  }

  LayerIntRegion GetVisibleRegion() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetVisibleRegion();
    }

    return ViewAs<LayerPixel>(
        TransformBy(mLayer->GetTransformTyped(), mLayer->GetVisibleRegion()),
        PixelCastJustification::MovingDownToChildren);
  }

  LayerIntSize GetRemoteDocumentSize() const {
    MOZ_ASSERT(IsValid());

    if (mLayer->GetReferentId().isNothing()) {
      return LayerIntSize();
    }

    if (AtBottomLayer()) {
      return mLayer->GetRemoteDocumentSize();
    }

    return ViewAs<LayerPixel>(mLayer->GetRemoteDocumentSize(),
                              PixelCastJustification::MovingDownToChildren);
  }

  Maybe<LayersId> GetReferentId() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetReferentId();
    }
    return Nothing();
  }

  EventRegionsOverride GetEventRegionsOverride() const {
    MOZ_ASSERT(IsValid());
    // Only ref layers can have an event regions override.
    if (GetReferentId()) {
      return mLayer->GetEventRegionsOverride();
    }
    return EventRegionsOverride::NoOverride;
  }

  const ScrollbarData& GetScrollbarData() const {
    MOZ_ASSERT(IsValid());
    return mLayer->GetScrollbarData();
  }

  Maybe<uint64_t> GetScrollbarAnimationId() const {
    MOZ_ASSERT(IsValid());
    return mLayer->GetScrollbarAnimationId();
  }

  Maybe<uint64_t> GetFixedPositionAnimationId() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetFixedPositionAnimationId();
    }
    return Nothing();
  }

  ScrollableLayerGuid::ViewID GetFixedPositionScrollContainerId() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetFixedPositionScrollContainerId();
    }
    return ScrollableLayerGuid::NULL_SCROLL_ID;
  }

  SideBits GetFixedPositionSides() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetFixedPositionSides();
    }
    return SideBits::eNone;
  }

  ScrollableLayerGuid::ViewID GetStickyScrollContainerId() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetStickyPositionScrollContainerId();
    }
    return ScrollableLayerGuid::NULL_SCROLL_ID;
  }

  const LayerRectAbsolute& GetStickyScrollRangeOuter() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetStickyScrollRangeOuter();
    }

    static const LayerRectAbsolute empty;
    return empty;
  }

  const LayerRectAbsolute& GetStickyScrollRangeInner() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetStickyScrollRangeInner();
    }

    static const LayerRectAbsolute empty;
    return empty;
  }

  Maybe<uint64_t> GetStickyPositionAnimationId() const {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetStickyPositionAnimationId();
    }
    return Nothing();
  }

  Maybe<uint64_t> GetZoomAnimationId() const {
    MOZ_ASSERT(IsValid());
    return mLayer->GetZoomAnimationId();
  }

  Maybe<ScrollableLayerGuid::ViewID> GetAsyncZoomContainerId() const {
    MOZ_ASSERT(IsValid());
    return mLayer->GetAsyncZoomContainerId();
  }

  // Expose an opaque pointer to the layer. Mostly used for printf
  // purposes. This is not intended to be a general-purpose accessor
  // for the underlying layer.
  const void* GetLayer() const {
    MOZ_ASSERT(IsValid());
    return mLayer;
  }

  template <int Level>
  size_t Dump(gfx::TreeLog<Level>& aOut) const {
    std::string result = "(invalid)";
    if (!IsValid()) {
      aOut << result;
      return result.length();
    }
    if (AtBottomLayer()) {
      if (mData != nullptr) {
        const WebRenderLayerScrollData* layerData =
            mData->GetLayerData(mLayerIndex);
        if (layerData != nullptr) {
          std::stringstream ss;
          layerData->Dump(ss, *mData);
          result = ss.str();
          aOut << result;
          return result.length();
        }
      }
    }
    return 0;
  }

 private:
  bool AtBottomLayer() const { return mMetadataIndex == 0; }

  bool AtTopLayer() const {
    return mLayer->GetScrollMetadataCount() == 0 ||
           mMetadataIndex == mLayer->GetScrollMetadataCount() - 1;
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

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_WEBRENDERSCROLLDATAWRAPPER_H */
