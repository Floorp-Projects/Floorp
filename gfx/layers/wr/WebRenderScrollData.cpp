/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderScrollData.h"

#include <ostream>

#include "Layers.h"
#include "mozilla/layers/LayersMessageUtils.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/ToString.h"
#include "mozilla/Unused.h"
#include "nsDisplayList.h"
#include "nsTArray.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

WebRenderLayerScrollData::WebRenderLayerScrollData()
    : mDescendantCount(-1),
      mTransformIsPerspective(false),
      mResolution(1.f),
      mEventRegionsOverride(EventRegionsOverride::NoOverride),
      mFixedPositionSides(mozilla::SideBits::eNone),
      mFixedPosScrollContainerId(ScrollableLayerGuid::NULL_SCROLL_ID),
      mStickyPosScrollContainerId(ScrollableLayerGuid::NULL_SCROLL_ID) {}

WebRenderLayerScrollData::~WebRenderLayerScrollData() = default;

void WebRenderLayerScrollData::InitializeRoot(int32_t aDescendantCount) {
  mDescendantCount = aDescendantCount;
}

void WebRenderLayerScrollData::Initialize(
    WebRenderScrollData& aOwner, nsDisplayItem* aItem, int32_t aDescendantCount,
    const ActiveScrolledRoot* aStopAtAsr,
    const Maybe<gfx::Matrix4x4>& aAncestorTransform,
    const ViewID& aAncestorTransformId) {
  MOZ_ASSERT(aDescendantCount >= 0);  // Ensure value is valid
  MOZ_ASSERT(mDescendantCount ==
             -1);  // Don't allow re-setting an already set value
  mDescendantCount = aDescendantCount;

  MOZ_ASSERT(aItem);
  aItem->UpdateScrollData(&aOwner, this);

  const ActiveScrolledRoot* asr = aItem->GetActiveScrolledRoot();
  if (ActiveScrolledRoot::IsAncestor(asr, aStopAtAsr)) {
    // If the item's ASR is an ancestor of the stop-at ASR, then we don't need
    // any more metrics information because we'll end up duplicating what the
    // ancestor WebRenderLayerScrollData already has.
    asr = nullptr;
  }

  while (asr && asr != aStopAtAsr) {
    MOZ_ASSERT(aOwner.GetManager());
    ScrollableLayerGuid::ViewID scrollId = asr->GetViewId();
    if (Maybe<size_t> index = aOwner.HasMetadataFor(scrollId)) {
      mScrollIds.AppendElement(index.ref());
    } else {
      Maybe<ScrollMetadata> metadata =
          asr->mScrollableFrame->ComputeScrollMetadata(
              aOwner.GetManager(), aItem->ReferenceFrame(), nullptr);
      aOwner.GetBuilder()->AddScrollFrameToNotify(asr->mScrollableFrame);
      if (metadata) {
        MOZ_ASSERT(metadata->GetMetrics().GetScrollId() == scrollId);
        mScrollIds.AppendElement(aOwner.AddMetadata(metadata.ref()));
      } else {
        MOZ_ASSERT_UNREACHABLE("Expected scroll metadata to be available!");
      }
    }
    asr = asr->mParent;
  }

#ifdef DEBUG
  // Sanity check: if we have an ancestor transform, its scroll id should
  // match one of the scroll metadatas on this node (WebRenderScrollDataWrapper
  // will then use the ancestor transform at the level of that scroll metadata).
  // One exception to this is if we have no scroll metadatas, which can happen
  // if the scroll id of the transform is on an enclosing node.
  if (aAncestorTransformId != ScrollableLayerGuid::NULL_SCROLL_ID &&
      !mScrollIds.IsEmpty()) {
    bool seenAncestorTransformId = false;
    for (size_t scrollIdIndex : mScrollIds) {
      if (aAncestorTransformId ==
          aOwner.GetScrollMetadata(scrollIdIndex).GetMetrics().GetScrollId()) {
        seenAncestorTransformId = true;
      }
    }
    MOZ_ASSERT(
        seenAncestorTransformId,
        "The ancestor transform's view ID should match one of the metrics "
        "on this node");
  }
#endif

  // See the comments on StackingContextHelper::mDeferredTransformItem for an
  // overview of what deferred transforms are.
  // aAncestorTransform, if present, is the transform from a deferred transform
  // item that is an ancestor of |aItem|. We store this transform value
  // separately from mTransform because in the case where we have multiple
  // scroll metadata on this layer item, the mAncestorTransform is associated
  // with the "topmost" scroll metadata, and the mTransform is associated with
  // the "bottommost" scroll metadata. The code in
  // WebRenderScrollDataWrapper::GetTransform() is responsible for combining
  // these transforms and exposing them appropriately. Also, we don't save the
  // ancestor transform for thumb layers, because those are a special case in
  // APZ; we need to keep the ancestor transform for the scrollable content that
  // the thumb scrolls, but not for the thumb itself, as it will result in
  // incorrect visual positioning of the thumb.
  if (aAncestorTransform &&
      mScrollbarData.mScrollbarLayerType != ScrollbarLayerType::Thumb) {
    mAncestorTransform = *aAncestorTransform;
    mAncestorTransformId = aAncestorTransformId;
  }
}

int32_t WebRenderLayerScrollData::GetDescendantCount() const {
  MOZ_ASSERT(mDescendantCount >= 0);  // check that it was set
  return mDescendantCount;
}

size_t WebRenderLayerScrollData::GetScrollMetadataCount() const {
  return mScrollIds.Length();
}

void WebRenderLayerScrollData::AppendScrollMetadata(
    WebRenderScrollData& aOwner, const ScrollMetadata& aData) {
  mScrollIds.AppendElement(aOwner.AddMetadata(aData));
}

const ScrollMetadata& WebRenderLayerScrollData::GetScrollMetadata(
    const WebRenderScrollData& aOwner, size_t aIndex) const {
  MOZ_ASSERT(aIndex < mScrollIds.Length());
  return aOwner.GetScrollMetadata(mScrollIds[aIndex]);
}

CSSTransformMatrix WebRenderLayerScrollData::GetTransformTyped() const {
  return ViewAs<CSSTransformMatrix>(GetTransform());
}

void WebRenderLayerScrollData::Dump(std::ostream& aOut,
                                    const WebRenderScrollData& aOwner) const {
  aOut << "WebRenderLayerScrollData(" << this
       << "), descendantCount=" << mDescendantCount;
  for (size_t i = 0; i < mScrollIds.Length(); i++) {
    aOut << ", metadata" << i << "=" << aOwner.GetScrollMetadata(mScrollIds[i]);
  }
  if (!mAncestorTransform.IsIdentity()) {
    aOut << ", ancestorTransform=" << mAncestorTransform;
  }
  if (!mTransform.IsIdentity()) {
    aOut << ", transform=" << mTransform;
    if (mTransformIsPerspective) {
      aOut << ", transformIsPerspective";
    }
  }
  if (mResolution != 1.f) {
    aOut << ", resolution=" << mResolution;
  }
  aOut << ", visible=" << mVisibleRegion;
  if (mReferentId) {
    aOut << ", refLayersId=" << *mReferentId;
  }
  if (mEventRegionsOverride) {
    aOut << std::hex << ", eventRegionsOverride=0x"
         << (int)mEventRegionsOverride << std::dec;
  }
  if (mScrollbarData.mScrollbarLayerType != ScrollbarLayerType::None) {
    aOut << ", scrollbarType=" << (int)mScrollbarData.mScrollbarLayerType
         << std::hex << ", scrollbarAnimationId=0x"
         << mScrollbarAnimationId.valueOr(0) << std::dec;
  }
  if (mFixedPosScrollContainerId != ScrollableLayerGuid::NULL_SCROLL_ID) {
    aOut << ", fixedContainer=" << mFixedPosScrollContainerId << std::hex
         << ", fixedAnimation=0x" << mFixedPositionAnimationId.valueOr(0)
         << ", sideBits=0x" << (int)mFixedPositionSides << std::dec;
  }
  if (mStickyPosScrollContainerId != ScrollableLayerGuid::NULL_SCROLL_ID) {
    aOut << ", stickyContainer=" << mStickyPosScrollContainerId << std::hex
         << ", stickyAnimation=" << mStickyPositionAnimationId.valueOr(0)
         << std::dec << ", stickyInner=" << mStickyScrollRangeInner
         << ", stickyOuter=" << mStickyScrollRangeOuter;
  }
}

WebRenderScrollData::WebRenderScrollData()
    : mManager(nullptr), mIsFirstPaint(false), mPaintSequenceNumber(0) {}

WebRenderScrollData::WebRenderScrollData(WebRenderLayerManager* aManager,
                                         nsDisplayListBuilder* aBuilder)
    : mManager(aManager),
      mBuilder(aBuilder),
      mIsFirstPaint(false),
      mPaintSequenceNumber(0) {}

WebRenderLayerManager* WebRenderScrollData::GetManager() const {
  return mManager;
}

nsDisplayListBuilder* WebRenderScrollData::GetBuilder() const {
  return mBuilder;
}

size_t WebRenderScrollData::AddMetadata(const ScrollMetadata& aMetadata) {
  ScrollableLayerGuid::ViewID scrollId = aMetadata.GetMetrics().GetScrollId();
  auto p = mScrollIdMap.lookupForAdd(scrollId);
  if (!p) {
    // It's a scrollId we hadn't seen before
    bool ok = mScrollIdMap.add(p, scrollId, mScrollMetadatas.Length());
    MOZ_RELEASE_ASSERT(ok);
    mScrollMetadatas.AppendElement(aMetadata);
  }  // else we didn't insert, because it already existed
  return p->value();
}

size_t WebRenderScrollData::AddLayerData(
    const WebRenderLayerScrollData& aData) {
  mLayerScrollData.AppendElement(aData);
  return mLayerScrollData.Length() - 1;
}

size_t WebRenderScrollData::GetLayerCount() const {
  return mLayerScrollData.Length();
}

const WebRenderLayerScrollData* WebRenderScrollData::GetLayerData(
    size_t aIndex) const {
  if (aIndex >= mLayerScrollData.Length()) {
    return nullptr;
  }
  return &(mLayerScrollData.ElementAt(aIndex));
}

const ScrollMetadata& WebRenderScrollData::GetScrollMetadata(
    size_t aIndex) const {
  MOZ_ASSERT(aIndex < mScrollMetadatas.Length());
  return mScrollMetadatas[aIndex];
}

Maybe<size_t> WebRenderScrollData::HasMetadataFor(
    const ScrollableLayerGuid::ViewID& aScrollId) const {
  auto ptr = mScrollIdMap.lookup(aScrollId);
  return (ptr ? Some(ptr->value()) : Nothing());
}

void WebRenderScrollData::SetIsFirstPaint() { mIsFirstPaint = true; }

bool WebRenderScrollData::IsFirstPaint() const { return mIsFirstPaint; }

void WebRenderScrollData::SetPaintSequenceNumber(
    uint32_t aPaintSequenceNumber) {
  mPaintSequenceNumber = aPaintSequenceNumber;
}

uint32_t WebRenderScrollData::GetPaintSequenceNumber() const {
  return mPaintSequenceNumber;
}

void WebRenderScrollData::ApplyUpdates(ScrollUpdatesMap&& aUpdates,
                                       uint32_t aPaintSequenceNumber) {
  for (auto it = aUpdates.Iter(); !it.Done(); it.Next()) {
    if (Maybe<size_t> index = HasMetadataFor(it.Key())) {
      mScrollMetadatas[*index].UpdatePendingScrollInfo(std::move(it.Data()));
    }
  }
  mPaintSequenceNumber = aPaintSequenceNumber;
}

void WebRenderScrollData::DumpSubtree(std::ostream& aOut, size_t aIndex,
                                      const std::string& aIndent) const {
  aOut << aIndent;
  mLayerScrollData.ElementAt(aIndex).Dump(aOut, *this);
  aOut << std::endl;

  int32_t descendants = mLayerScrollData.ElementAt(aIndex).GetDescendantCount();
  if (descendants == 0) {
    return;
  }

  // Build a stack of indices at which this aIndex's children live. We do
  // this because we want to dump them first-to-last but they are stored
  // last-to-first.
  std::stack<size_t> childIndices;
  size_t childIndex = aIndex + 1;
  while (descendants > 0) {
    childIndices.push(childIndex);
    // "1" for the child itelf, plus whatever descendants it has
    int32_t subtreeSize =
        1 + mLayerScrollData.ElementAt(childIndex).GetDescendantCount();
    childIndex += subtreeSize;
    descendants -= subtreeSize;
    MOZ_ASSERT(descendants >= 0);
  }

  std::string indent = aIndent + "    ";
  while (!childIndices.empty()) {
    size_t child = childIndices.top();
    childIndices.pop();
    DumpSubtree(aOut, child, indent);
  }
}

std::ostream& operator<<(std::ostream& aOut, const WebRenderScrollData& aData) {
  aOut << "--- WebRenderScrollData (firstPaint=" << aData.mIsFirstPaint
       << ") ---" << std::endl;

  if (aData.mLayerScrollData.Length() > 0) {
    aData.DumpSubtree(aOut, 0, std::string());
  }
  return aOut;
}

bool WebRenderScrollData::RepopulateMap() {
  MOZ_ASSERT(mScrollIdMap.empty());
  for (size_t i = 0; i < mScrollMetadatas.Length(); i++) {
    ScrollableLayerGuid::ViewID scrollId =
        mScrollMetadatas[i].GetMetrics().GetScrollId();
    bool ok = mScrollIdMap.putNew(scrollId, i);
    MOZ_RELEASE_ASSERT(ok);
  }
  return true;
}

}  // namespace layers
}  // namespace mozilla

namespace IPC {

void ParamTraits<mozilla::layers::WebRenderLayerScrollData>::Write(
    Message* aMsg, const paramType& aParam) {
  WriteParam(aMsg, aParam.mDescendantCount);
  WriteParam(aMsg, aParam.mScrollIds);
  WriteParam(aMsg, aParam.mAncestorTransform);
  WriteParam(aMsg, aParam.mAncestorTransformId);
  WriteParam(aMsg, aParam.mTransform);
  WriteParam(aMsg, aParam.mTransformIsPerspective);
  WriteParam(aMsg, aParam.mResolution);
  WriteParam(aMsg, aParam.mVisibleRegion);
  WriteParam(aMsg, aParam.mRemoteDocumentSize);
  WriteParam(aMsg, aParam.mReferentId);
  WriteParam(aMsg, aParam.mEventRegionsOverride);
  WriteParam(aMsg, aParam.mScrollbarData);
  WriteParam(aMsg, aParam.mScrollbarAnimationId);
  WriteParam(aMsg, aParam.mFixedPositionAnimationId);
  WriteParam(aMsg, aParam.mFixedPositionSides);
  WriteParam(aMsg, aParam.mFixedPosScrollContainerId);
  WriteParam(aMsg, aParam.mStickyPosScrollContainerId);
  WriteParam(aMsg, aParam.mStickyScrollRangeOuter);
  WriteParam(aMsg, aParam.mStickyScrollRangeInner);
  WriteParam(aMsg, aParam.mStickyPositionAnimationId);
  WriteParam(aMsg, aParam.mZoomAnimationId);
  WriteParam(aMsg, aParam.mAsyncZoomContainerId);
}

bool ParamTraits<mozilla::layers::WebRenderLayerScrollData>::Read(
    const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
  return ReadParam(aMsg, aIter, &aResult->mDescendantCount) &&
         ReadParam(aMsg, aIter, &aResult->mScrollIds) &&
         ReadParam(aMsg, aIter, &aResult->mAncestorTransform) &&
         ReadParam(aMsg, aIter, &aResult->mAncestorTransformId) &&
         ReadParam(aMsg, aIter, &aResult->mTransform) &&
         ReadParam(aMsg, aIter, &aResult->mTransformIsPerspective) &&
         ReadParam(aMsg, aIter, &aResult->mResolution) &&
         ReadParam(aMsg, aIter, &aResult->mVisibleRegion) &&
         ReadParam(aMsg, aIter, &aResult->mRemoteDocumentSize) &&
         ReadParam(aMsg, aIter, &aResult->mReferentId) &&
         ReadParam(aMsg, aIter, &aResult->mEventRegionsOverride) &&
         ReadParam(aMsg, aIter, &aResult->mScrollbarData) &&
         ReadParam(aMsg, aIter, &aResult->mScrollbarAnimationId) &&
         ReadParam(aMsg, aIter, &aResult->mFixedPositionAnimationId) &&
         ReadParam(aMsg, aIter, &aResult->mFixedPositionSides) &&
         ReadParam(aMsg, aIter, &aResult->mFixedPosScrollContainerId) &&
         ReadParam(aMsg, aIter, &aResult->mStickyPosScrollContainerId) &&
         ReadParam(aMsg, aIter, &aResult->mStickyScrollRangeOuter) &&
         ReadParam(aMsg, aIter, &aResult->mStickyScrollRangeInner) &&
         ReadParam(aMsg, aIter, &aResult->mStickyPositionAnimationId) &&
         ReadParam(aMsg, aIter, &aResult->mZoomAnimationId) &&
         ReadParam(aMsg, aIter, &aResult->mAsyncZoomContainerId);
}

void ParamTraits<mozilla::layers::WebRenderScrollData>::Write(
    Message* aMsg, const paramType& aParam) {
  WriteParam(aMsg, aParam.mScrollMetadatas);
  WriteParam(aMsg, aParam.mLayerScrollData);
  WriteParam(aMsg, aParam.mIsFirstPaint);
  WriteParam(aMsg, aParam.mPaintSequenceNumber);
}

bool ParamTraits<mozilla::layers::WebRenderScrollData>::Read(
    const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
  return ReadParam(aMsg, aIter, &aResult->mScrollMetadatas) &&
         ReadParam(aMsg, aIter, &aResult->mLayerScrollData) &&
         ReadParam(aMsg, aIter, &aResult->mIsFirstPaint) &&
         ReadParam(aMsg, aIter, &aResult->mPaintSequenceNumber) &&
         aResult->RepopulateMap();
}

}  // namespace IPC
