/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERSCROLLDATA_H
#define GFX_WEBRENDERSCROLLDATA_H

#include <map>
#include <iosfwd>

#include "chrome/common/ipc_message_utils.h"
#include "FrameMetrics.h"
#include "ipc/IPCMessageUtils.h"
#include "LayersTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/ScrollbarData.h"
#include "mozilla/layers/WebRenderMessageUtils.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/HashTable.h"
#include "mozilla/Maybe.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {

class nsDisplayItem;
class nsDisplayListBuilder;
struct ActiveScrolledRoot;

namespace layers {

class APZTestAccess;
class Layer;
class WebRenderLayerManager;
class WebRenderScrollData;

// Data needed by APZ, per layer. One instance of this class is created for
// each layer in the layer tree and sent over PWebRenderBridge to the APZ code.
// Each WebRenderLayerScrollData is conceptually associated with an "owning"
// WebRenderScrollData.
class WebRenderLayerScrollData final {
 public:
  WebRenderLayerScrollData();  // needed for IPC purposes
  WebRenderLayerScrollData(WebRenderLayerScrollData&& aOther) = default;
  ~WebRenderLayerScrollData();

  using ViewID = ScrollableLayerGuid::ViewID;

  // Helper function for WebRenderScrollData::Validate().
  bool ValidateSubtree(const WebRenderScrollData& aParent,
                       std::vector<size_t>& aVisitCounts,
                       size_t aCurrentIndex) const;

  void InitializeRoot(int32_t aDescendantCount);
  void Initialize(WebRenderScrollData& aOwner, nsDisplayItem* aItem,
                  int32_t aDescendantCount,
                  const ActiveScrolledRoot* aStopAtAsr,
                  const Maybe<gfx::Matrix4x4>& aAncestorTransform,
                  const ViewID& aAncestorTransformId);

  int32_t GetDescendantCount() const;
  size_t GetScrollMetadataCount() const;

  void AppendScrollMetadata(WebRenderScrollData& aOwner,
                            const ScrollMetadata& aData);
  // Return the ScrollMetadata object that used to be on the original Layer
  // at the given index. Since we deduplicate the ScrollMetadata objects into
  // the array in the owning WebRenderScrollData object, we need to be passed
  // in a reference to that owner as well.
  const ScrollMetadata& GetScrollMetadata(const WebRenderScrollData& aOwner,
                                          size_t aIndex) const;

  gfx::Matrix4x4 GetAncestorTransform() const { return mAncestorTransform; }
  ViewID GetAncestorTransformId() const { return mAncestorTransformId; }
  void SetTransform(const gfx::Matrix4x4& aTransform) {
    mTransform = aTransform;
  }
  gfx::Matrix4x4 GetTransform() const { return mTransform; }
  CSSTransformMatrix GetTransformTyped() const;
  void SetTransformIsPerspective(bool aTransformIsPerspective) {
    mTransformIsPerspective = aTransformIsPerspective;
  }
  bool GetTransformIsPerspective() const { return mTransformIsPerspective; }

  void SetEventRegionsOverride(const EventRegionsOverride& aOverride) {
    mEventRegionsOverride = aOverride;
  }
  EventRegionsOverride GetEventRegionsOverride() const {
    return mEventRegionsOverride;
  }

  void SetVisibleRect(const LayerIntRect& aRect) { mVisibleRect = aRect; }
  const LayerIntRect& GetVisibleRect() const { return mVisibleRect; }
  void SetRemoteDocumentSize(const LayerIntSize& aRemoteDocumentSize) {
    mRemoteDocumentSize = aRemoteDocumentSize;
  }
  const LayerIntSize& GetRemoteDocumentSize() const {
    return mRemoteDocumentSize;
  }
  void SetReferentId(LayersId aReferentId) { mReferentId = Some(aReferentId); }
  Maybe<LayersId> GetReferentId() const { return mReferentId; }

  void SetScrollbarData(const ScrollbarData& aData) { mScrollbarData = aData; }
  const ScrollbarData& GetScrollbarData() const { return mScrollbarData; }
  void SetScrollbarAnimationId(const uint64_t& aId) {
    mScrollbarAnimationId = Some(aId);
  }
  Maybe<uint64_t> GetScrollbarAnimationId() const {
    return mScrollbarAnimationId;
  }

  void SetFixedPositionAnimationId(const uint64_t& aId) {
    mFixedPositionAnimationId = Some(aId);
  }
  Maybe<uint64_t> GetFixedPositionAnimationId() const {
    return mFixedPositionAnimationId;
  }

  void SetFixedPositionSides(const SideBits& aSideBits) {
    mFixedPositionSides = aSideBits;
  }
  SideBits GetFixedPositionSides() const { return mFixedPositionSides; }

  void SetFixedPositionScrollContainerId(ViewID aId) {
    mFixedPosScrollContainerId = aId;
  }
  ViewID GetFixedPositionScrollContainerId() const {
    return mFixedPosScrollContainerId;
  }

  void SetStickyPositionScrollContainerId(ViewID aId) {
    mStickyPosScrollContainerId = aId;
  }
  ViewID GetStickyPositionScrollContainerId() const {
    return mStickyPosScrollContainerId;
  }

  void SetStickyScrollRangeOuter(const LayerRectAbsolute& scrollRange) {
    mStickyScrollRangeOuter = scrollRange;
  }
  const LayerRectAbsolute& GetStickyScrollRangeOuter() const {
    return mStickyScrollRangeOuter;
  }

  void SetStickyScrollRangeInner(const LayerRectAbsolute& scrollRange) {
    mStickyScrollRangeInner = scrollRange;
  }
  const LayerRectAbsolute& GetStickyScrollRangeInner() const {
    return mStickyScrollRangeInner;
  }

  void SetStickyPositionAnimationId(const uint64_t& aId) {
    mStickyPositionAnimationId = Some(aId);
  }
  Maybe<uint64_t> GetStickyPositionAnimationId() const {
    return mStickyPositionAnimationId;
  }

  void SetZoomAnimationId(const uint64_t& aId) { mZoomAnimationId = Some(aId); }
  Maybe<uint64_t> GetZoomAnimationId() const { return mZoomAnimationId; }

  void SetAsyncZoomContainerId(const ViewID& aId) {
    mAsyncZoomContainerId = Some(aId);
  }
  Maybe<ViewID> GetAsyncZoomContainerId() const {
    return mAsyncZoomContainerId;
  }

  void Dump(std::ostream& aOut, const WebRenderScrollData& aOwner) const;

  friend struct IPC::ParamTraits<WebRenderLayerScrollData>;

 private:
  // For test use only
  friend class APZTestAccess;

  // For use by GTests in building WebRenderLayerScrollData trees.
  // GTests don't have a display list so they can't use Initialize().
  void InitializeForTest(int32_t aDescendantCount);

  ScrollMetadata& GetScrollMetadataMut(WebRenderScrollData& aOwner,
                                       size_t aIndex);

 private:
  // The number of descendants this layer has (not including the layer itself).
  // This is needed to reconstruct the depth-first layer tree traversal
  // efficiently. Leaf layers should always have 0 descendants.
  int32_t mDescendantCount;

  // Handles to the ScrollMetadata objects that were on this layer. The values
  // stored in this array are indices into the owning WebRenderScrollData's
  // mScrollMetadatas array. This indirection is used to deduplicate the
  // ScrollMetadata objects, since there is usually heavy duplication of them
  // within a layer tree.
  CopyableTArray<size_t> mScrollIds;

  // Various data that we collect from the Layer in Initialize(), serialize
  // over IPC, and use on the parent side in APZ.

  gfx::Matrix4x4 mAncestorTransform;
  ViewID mAncestorTransformId;
  gfx::Matrix4x4 mTransform;
  bool mTransformIsPerspective;
  LayerIntRect mVisibleRect;
  // The remote documents only need their size because their origin is always
  // (0, 0).
  LayerIntSize mRemoteDocumentSize;
  Maybe<LayersId> mReferentId;
  EventRegionsOverride mEventRegionsOverride;
  ScrollbarData mScrollbarData;
  Maybe<uint64_t> mScrollbarAnimationId;
  Maybe<uint64_t> mFixedPositionAnimationId;
  SideBits mFixedPositionSides;
  ViewID mFixedPosScrollContainerId;
  ViewID mStickyPosScrollContainerId;
  LayerRectAbsolute mStickyScrollRangeOuter;
  LayerRectAbsolute mStickyScrollRangeInner;
  Maybe<uint64_t> mStickyPositionAnimationId;
  Maybe<uint64_t> mZoomAnimationId;
  Maybe<ViewID> mAsyncZoomContainerId;

#if defined(DEBUG) || defined(MOZ_DUMP_PAINTING)
  // The display item for which this layer was built.
  // This is only set on the content side.
  nsDisplayItem* mInitializedFrom = nullptr;
#endif
};

// Data needed by APZ, for the whole layer tree. One instance of this class
// is created for each transaction sent over PWebRenderBridge. It is populated
// with information from the WebRender layer tree on the client side and the
// information is used by APZ on the parent side.
class WebRenderScrollData {
 public:
  WebRenderScrollData();
  explicit WebRenderScrollData(WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aBuilder);
  WebRenderScrollData(WebRenderScrollData&& aOther) = default;
  WebRenderScrollData& operator=(WebRenderScrollData&& aOther) = default;
  virtual ~WebRenderScrollData() = default;

  // Validate that the scroll data is well-formed, and particularly that
  // |mLayerScrollData| encodes a valid tree. This is necessary because
  // the data can be sent over IPC from a less-trusted content process.
  bool Validate() const;

  WebRenderLayerManager* GetManager() const;

  nsDisplayListBuilder* GetBuilder() const;

  // Add the given ScrollMetadata if it doesn't already exist. Return an index
  // that can be used to look up the metadata later.
  size_t AddMetadata(const ScrollMetadata& aMetadata);
  // Add the provided WebRenderLayerScrollData and return the index that can
  // be used to look it up via GetLayerData.
  size_t AddLayerData(WebRenderLayerScrollData&& aData);

  size_t GetLayerCount() const;

  // Return a pointer to the scroll data at the given index. Use with caution,
  // as the pointer may be invalidated if this WebRenderScrollData is mutated.
  const WebRenderLayerScrollData* GetLayerData(size_t aIndex) const;
  WebRenderLayerScrollData* GetLayerData(size_t aIndex);

  const ScrollMetadata& GetScrollMetadata(size_t aIndex) const;
  Maybe<size_t> HasMetadataFor(
      const ScrollableLayerGuid::ViewID& aScrollId) const;

  void SetIsFirstPaint();
  bool IsFirstPaint() const;
  void SetPaintSequenceNumber(uint32_t aPaintSequenceNumber);
  uint32_t GetPaintSequenceNumber() const;

  void ApplyUpdates(ScrollUpdatesMap&& aUpdates, uint32_t aPaintSequenceNumber);

  friend struct IPC::ParamTraits<WebRenderScrollData>;

  friend std::ostream& operator<<(std::ostream& aOut,
                                  const WebRenderScrollData& aData);

 private:
  // For test use only.
  friend class WebRenderLayerScrollData;
  ScrollMetadata& GetScrollMetadataMut(size_t aIndex);

 private:
  // This is called by the ParamTraits implementation to rebuild mScrollIdMap
  // based on mScrollMetadatas
  bool RepopulateMap();

  // This is a helper for the dumping code
  void DumpSubtree(std::ostream& aOut, size_t aIndex,
                   const std::string& aIndent) const;

 private:
  // Pointer back to the layer manager; if this is non-null, it will always be
  // valid, because the WebRenderLayerManager that created |this| will
  // outlive |this|.
  WebRenderLayerManager* MOZ_NON_OWNING_REF mManager;

  // Pointer to the display list builder; if this is non-null, it will always be
  // valid, because the nsDisplayListBuilder that created the layer manager will
  // outlive |this|.
  nsDisplayListBuilder* MOZ_NON_OWNING_REF mBuilder;

  // Internal data structure used to maintain uniqueness of mScrollMetadatas.
  // This is not serialized/deserialized over IPC, but it is rebuilt on the
  // parent side when mScrollMetadatas is deserialized. So it should always be
  // valid on both the child and parent.
  // The key into this map is the scrollId of a ScrollMetadata, and the value is
  // an index into the mScrollMetadatas array.
  HashMap<ScrollableLayerGuid::ViewID, size_t> mScrollIdMap;

  // A list of all the unique ScrollMetadata objects from the layer tree. Each
  // ScrollMetadata in this list must have a unique scroll id.
  nsTArray<ScrollMetadata> mScrollMetadatas;

  // A list of per-layer scroll data objects, generated via a depth-first,
  // pre-order, last-to-first traversal of the layer tree (i.e. a recursive
  // traversal where a node N first pushes itself, followed by its children in
  // last-to-first order). Each layer's scroll data object knows how many
  // descendants that layer had, which allows reconstructing the traversal on
  // the other side.
  nsTArray<WebRenderLayerScrollData> mLayerScrollData;

  bool mIsFirstPaint;
  uint32_t mPaintSequenceNumber;
};

}  // namespace layers
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::WebRenderLayerScrollData> {
  typedef mozilla::layers::WebRenderLayerScrollData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam);

  static bool Read(MessageReader* aReader, paramType* aResult);
};

template <>
struct ParamTraits<mozilla::layers::WebRenderScrollData> {
  typedef mozilla::layers::WebRenderScrollData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam);

  static bool Read(MessageReader* aReader, paramType* aResult);
};

}  // namespace IPC

#endif /* GFX_WEBRENDERSCROLLDATA_H */
