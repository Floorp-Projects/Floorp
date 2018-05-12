/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERSCROLLDATA_H
#define GFX_WEBRENDERSCROLLDATA_H

#include <map>

#include "chrome/common/ipc_message_utils.h"
#include "FrameMetrics.h"
#include "ipc/IPCMessageUtils.h"
#include "LayersTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersMessageUtils.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/Maybe.h"
#include "nsTArrayForwardDeclare.h"

class nsDisplayItem;

namespace mozilla {

struct ActiveScrolledRoot;

namespace layers {

class Layer;
class WebRenderLayerManager;
class WebRenderScrollData;

// Data needed by APZ, per layer. One instance of this class is created for
// each layer in the layer tree and sent over PWebRenderBridge to the APZ code.
// Each WebRenderLayerScrollData is conceptually associated with an "owning"
// WebRenderScrollData.
class WebRenderLayerScrollData
{
public:
  WebRenderLayerScrollData(); // needed for IPC purposes
  ~WebRenderLayerScrollData();

  void InitializeRoot(int32_t aDescendantCount);
  void Initialize(WebRenderScrollData& aOwner,
                  nsDisplayItem* aItem,
                  int32_t aDescendantCount,
                  const ActiveScrolledRoot* aStopAtAsr,
                  const Maybe<gfx::Matrix4x4>& aAncestorTransform);

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
  void SetTransform(const gfx::Matrix4x4& aTransform) { mTransform = aTransform; }
  gfx::Matrix4x4 GetTransform() const { return mTransform; }
  CSSTransformMatrix GetTransformTyped() const;
  void SetTransformIsPerspective(bool aTransformIsPerspective) { mTransformIsPerspective = aTransformIsPerspective; }
  bool GetTransformIsPerspective() const { return mTransformIsPerspective; }

  void AddEventRegions(const EventRegions& aRegions) { mEventRegions.OrWith(aRegions); }
  EventRegions GetEventRegions() const { return mEventRegions; }
  void SetEventRegionsOverride(const EventRegionsOverride& aOverride) { mEventRegionsOverride = aOverride; }
  EventRegionsOverride GetEventRegionsOverride() const { return mEventRegionsOverride; }

  const LayerIntRegion& GetVisibleRegion() const { return mVisibleRegion; }
  void SetReferentId(LayersId aReferentId) { mReferentId = Some(aReferentId); }
  Maybe<LayersId> GetReferentId() const { return mReferentId; }

  void SetScrollbarData(const ScrollbarData& aData) { mScrollbarData = aData; }
  const ScrollbarData& GetScrollbarData() const { return mScrollbarData; }
  void SetScrollbarAnimationId(const uint64_t& aId) { mScrollbarAnimationId = aId; }
  const uint64_t& GetScrollbarAnimationId() const { return mScrollbarAnimationId; }

  void SetFixedPositionScrollContainerId(FrameMetrics::ViewID aId) { mFixedPosScrollContainerId = aId; }
  FrameMetrics::ViewID GetFixedPositionScrollContainerId() const { return mFixedPosScrollContainerId; }

  void Dump(const WebRenderScrollData& aOwner) const;

  friend struct IPC::ParamTraits<WebRenderLayerScrollData>;

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
  nsTArray<size_t> mScrollIds;

  // Various data that we collect from the Layer in Initialize(), serialize
  // over IPC, and use on the parent side in APZ.

  gfx::Matrix4x4 mAncestorTransform;
  gfx::Matrix4x4 mTransform;
  bool mTransformIsPerspective;
  EventRegions mEventRegions;
  LayerIntRegion mVisibleRegion;
  Maybe<LayersId> mReferentId;
  EventRegionsOverride mEventRegionsOverride;
  ScrollbarData mScrollbarData;
  uint64_t mScrollbarAnimationId;
  FrameMetrics::ViewID mFixedPosScrollContainerId;
};

// Data needed by APZ, for the whole layer tree. One instance of this class
// is created for each transaction sent over PWebRenderBridge. It is populated
// with information from the WebRender layer tree on the client side and the
// information is used by APZ on the parent side.
class WebRenderScrollData
{
public:
  WebRenderScrollData();
  explicit WebRenderScrollData(WebRenderLayerManager* aManager);
  ~WebRenderScrollData();

  WebRenderLayerManager* GetManager() const;

  // Add the given ScrollMetadata if it doesn't already exist. Return an index
  // that can be used to look up the metadata later.
  size_t AddMetadata(const ScrollMetadata& aMetadata);
  // Add the provided WebRenderLayerScrollData and return the index that can
  // be used to look it up via GetLayerData.
  size_t AddLayerData(const WebRenderLayerScrollData& aData);

  size_t GetLayerCount() const;

  // Return a pointer to the scroll data at the given index. Use with caution,
  // as the pointer may be invalidated if this WebRenderScrollData is mutated.
  const WebRenderLayerScrollData* GetLayerData(size_t aIndex) const;

  const ScrollMetadata& GetScrollMetadata(size_t aIndex) const;
  Maybe<size_t> HasMetadataFor(const FrameMetrics::ViewID& aScrollId) const;

  const FocusTarget& GetFocusTarget() const { return mFocusTarget; }
  void SetFocusTarget(const FocusTarget& aFocusTarget);

  void SetIsFirstPaint();
  bool IsFirstPaint() const;
  void SetPaintSequenceNumber(uint32_t aPaintSequenceNumber);
  uint32_t GetPaintSequenceNumber() const;

  void ApplyUpdates(const ScrollUpdatesMap& aUpdates,
                    uint32_t aPaintSequenceNumber);

  friend struct IPC::ParamTraits<WebRenderScrollData>;

  void Dump() const;

private:
  // This is called by the ParamTraits implementation to rebuild mScrollIdMap
  // based on mScrollMetadatas
  bool RepopulateMap();

private:
  // Pointer back to the layer manager; if this is non-null, it will always be
  // valid, because the WebRenderLayerManager that created |this| will
  // outlive |this|.
  WebRenderLayerManager* MOZ_NON_OWNING_REF mManager;

  // Internal data structure used to maintain uniqueness of mScrollMetadatas.
  // This is not serialized/deserialized over IPC, but it is rebuilt on the
  // parent side when mScrollMetadatas is deserialized. So it should always be
  // valid on both the child and parent.
  // The key into this map is the scrollId of a ScrollMetadata, and the value is
  // an index into the mScrollMetadatas array.
  std::map<FrameMetrics::ViewID, size_t> mScrollIdMap;

  // A list of all the unique ScrollMetadata objects from the layer tree. Each
  // ScrollMetadata in this list must have a unique scroll id.
  nsTArray<ScrollMetadata> mScrollMetadatas;

  // A list of per-layer scroll data objects, generated via a depth-first,
  // pre-order, last-to-first traversal of the layer tree (i.e. a recursive
  // traversal where a node N first pushes itself, followed by its children in
  // last-to-first order). Each layer's scroll data object knows how many
  // descendants that layer had, which allows reconstructing the traversal on the
  // other side.
  nsTArray<WebRenderLayerScrollData> mLayerScrollData;

  // The focus information for this layer tree
  FocusTarget mFocusTarget;

  bool mIsFirstPaint;
  uint32_t mPaintSequenceNumber;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

// When ScrollbarData is stored on the layer tree, it's part of
// SimpleAttributes which itself uses PlainOldDataSerializer, so
// we don't need a ParamTraits specialization for ScrollbarData
// separately. Here, however, ScrollbarData is stored as part
// of WebRenderLayerScrollData whose fields are serialized
// individually, so we do.
template<>
struct ParamTraits<mozilla::layers::ScrollbarData>
  : public PlainOldDataSerializer<mozilla::layers::ScrollbarData>
{ };

template<>
struct ParamTraits<mozilla::layers::WebRenderLayerScrollData>
{
  typedef mozilla::layers::WebRenderLayerScrollData paramType;

  static void
  Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDescendantCount);
    WriteParam(aMsg, aParam.mScrollIds);
    WriteParam(aMsg, aParam.mAncestorTransform);
    WriteParam(aMsg, aParam.mTransform);
    WriteParam(aMsg, aParam.mTransformIsPerspective);
    WriteParam(aMsg, aParam.mEventRegions);
    WriteParam(aMsg, aParam.mVisibleRegion);
    WriteParam(aMsg, aParam.mReferentId);
    WriteParam(aMsg, aParam.mEventRegionsOverride);
    WriteParam(aMsg, aParam.mScrollbarData);
    WriteParam(aMsg, aParam.mScrollbarAnimationId);
    WriteParam(aMsg, aParam.mFixedPosScrollContainerId);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mDescendantCount)
        && ReadParam(aMsg, aIter, &aResult->mScrollIds)
        && ReadParam(aMsg, aIter, &aResult->mAncestorTransform)
        && ReadParam(aMsg, aIter, &aResult->mTransform)
        && ReadParam(aMsg, aIter, &aResult->mTransformIsPerspective)
        && ReadParam(aMsg, aIter, &aResult->mEventRegions)
        && ReadParam(aMsg, aIter, &aResult->mVisibleRegion)
        && ReadParam(aMsg, aIter, &aResult->mReferentId)
        && ReadParam(aMsg, aIter, &aResult->mEventRegionsOverride)
        && ReadParam(aMsg, aIter, &aResult->mScrollbarData)
        && ReadParam(aMsg, aIter, &aResult->mScrollbarAnimationId)
        && ReadParam(aMsg, aIter, &aResult->mFixedPosScrollContainerId);
  }
};

template<>
struct ParamTraits<mozilla::layers::WebRenderScrollData>
{
  typedef mozilla::layers::WebRenderScrollData paramType;

  static void
  Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mScrollMetadatas);
    WriteParam(aMsg, aParam.mLayerScrollData);
    WriteParam(aMsg, aParam.mFocusTarget);
    WriteParam(aMsg, aParam.mIsFirstPaint);
    WriteParam(aMsg, aParam.mPaintSequenceNumber);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mScrollMetadatas)
        && ReadParam(aMsg, aIter, &aResult->mLayerScrollData)
        && ReadParam(aMsg, aIter, &aResult->mFocusTarget)
        && ReadParam(aMsg, aIter, &aResult->mIsFirstPaint)
        && ReadParam(aMsg, aIter, &aResult->mPaintSequenceNumber)
        && aResult->RepopulateMap();
  }
};

} // namespace IPC

#endif /* GFX_WEBRENDERSCROLLDATA_H */
