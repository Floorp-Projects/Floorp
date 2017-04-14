/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_layers_LayerAttributes_h
#define mozilla_gfx_layers_LayerAttributes_h

#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersTypes.h"

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

// Infrequently changing layer attributes that require no special
// serialization work.
class SimpleLayerAttributes final
{
  friend struct IPC::ParamTraits<mozilla::layers::SimpleLayerAttributes>;
public:
  SimpleLayerAttributes()
   : mTransformIsPerspective(false),
     mPostXScale(1.0f),
     mPostYScale(1.0f),
     mContentFlags(0),
     mOpacity(1.0f),
     mIsFixedPosition(false),
     mScrollbarTargetContainerId(FrameMetrics::NULL_SCROLL_ID),
     mScrollbarDirection(ScrollDirection::NONE),
     mScrollbarThumbRatio(0.0f),
     mIsScrollbarContainer(false),
     mMixBlendMode(gfx::CompositionOp::OP_OVER),
     mForceIsolatedGroup(false)
  {
  }

  //
  // Setters.
  // All set methods return true if values changed, false otherwise.
  //

  bool SetLayerBounds(const gfx::IntRect& aLayerBounds) {
    if (mLayerBounds.IsEqualEdges(aLayerBounds)) {
      return false;
    }
    mLayerBounds = aLayerBounds;
    return true;
  }
  bool SetPostScale(float aXScale, float aYScale) {
    if (mPostXScale == aXScale && mPostYScale == aYScale) {
      return false;
    }
    mPostXScale = aXScale;
    mPostYScale = aYScale;
    return true;
  }
  bool SetContentFlags(uint32_t aFlags) {
    if (aFlags == mContentFlags) {
      return false;
    }
    mContentFlags = aFlags;
    return true;
  }
  bool SetOpacity(float aOpacity) {
    if (aOpacity == mOpacity) {
      return false;
    }
    mOpacity = aOpacity;
    return true;
  }
  bool SetIsFixedPosition(bool aFixedPosition) {
    if (mIsFixedPosition == aFixedPosition) {
      return false;
    }
    mIsFixedPosition = aFixedPosition;
    return true;
  }
  bool SetScrollbarData(FrameMetrics::ViewID aScrollId, ScrollDirection aDir, float aThumbRatio) {
    if (mScrollbarTargetContainerId == aScrollId &&
        mScrollbarDirection == aDir &&
        mScrollbarThumbRatio == aThumbRatio)
    {
      return false;
    }
    mScrollbarTargetContainerId = aScrollId;
    mScrollbarDirection = aDir;
    mScrollbarThumbRatio = aThumbRatio;
    return true;
  }
  bool SetIsScrollbarContainer(FrameMetrics::ViewID aScrollId) {
    if (mIsScrollbarContainer && mScrollbarTargetContainerId == aScrollId) {
      return false;
    }
    mIsScrollbarContainer = true;
    mScrollbarTargetContainerId = aScrollId;
    return true;
  }
  bool SetMixBlendMode(gfx::CompositionOp aMixBlendMode) {
    if (mMixBlendMode == aMixBlendMode) {
      return false;
    }
    mMixBlendMode = aMixBlendMode;
    return true;
  }
  bool SetForceIsolatedGroup(bool aForceIsolatedGroup) {
    if (mForceIsolatedGroup == aForceIsolatedGroup) {
      return false;
    }
    mForceIsolatedGroup = aForceIsolatedGroup;
    return true;
  }
  bool SetTransform(const gfx::Matrix4x4& aMatrix) {
    if (mTransform == aMatrix) {
      return false;
    }
    mTransform = aMatrix;
    return true;
  }
  bool SetTransformIsPerspective(bool aIsPerspective) {
    if (mTransformIsPerspective == aIsPerspective) {
      return false;
    }
    mTransformIsPerspective = aIsPerspective;
    return true;
  }
  bool SetScrolledClip(const Maybe<LayerClip>& aScrolledClip) {
    if (mScrolledClip == aScrolledClip) {
      return false;
    }
    mScrolledClip = aScrolledClip;
    return true;
  }
  bool SetFixedPositionData(FrameMetrics::ViewID aScrollId,
                            const LayerPoint& aAnchor,
                            int32_t aSides)
  {
    if (mFixedPositionData &&
        mFixedPositionData->mScrollId == aScrollId &&
        mFixedPositionData->mAnchor == aAnchor &&
        mFixedPositionData->mSides == aSides) {
      return false;
    }
    if (!mFixedPositionData) {
      mFixedPositionData.emplace();
    }
    mFixedPositionData->mScrollId = aScrollId;
    mFixedPositionData->mAnchor = aAnchor;
    mFixedPositionData->mSides = aSides;
    return true;
  }
  bool SetStickyPositionData(FrameMetrics::ViewID aScrollId, LayerRect aOuter,
                             LayerRect aInner)
  {
    if (mStickyPositionData &&
        mStickyPositionData->mOuter.IsEqualEdges(aOuter) &&
        mStickyPositionData->mInner.IsEqualEdges(aInner)) {
      return false;
    }
    if (!mStickyPositionData) {
      mStickyPositionData.emplace();
    }
    mStickyPositionData->mScrollId = aScrollId;
    mStickyPositionData->mOuter = aOuter;
    mStickyPositionData->mInner = aInner;
    return true;
  }

  // This returns true if scrolling info is equivalent for the purposes of
  // APZ hit testing.
  bool HitTestingInfoIsEqual(const SimpleLayerAttributes& aOther) const {
    if (mIsScrollbarContainer != aOther.mIsScrollbarContainer) {
      return false;
    }
    if (mScrollbarTargetContainerId != aOther.mScrollbarTargetContainerId) {
      return false;
    }
    if (mScrollbarDirection != aOther.mScrollbarDirection) {
      return false;
    }
    if (FixedPositionScrollContainerId() != aOther.FixedPositionScrollContainerId()) {
      return false;
    }
    if (mTransform != aOther.mTransform) {
      return false;
    }
    return true;
  }

  //
  // Getters.
  //

  const gfx::IntRect& LayerBounds() const {
    return mLayerBounds;
  }
  float PostXScale() const {
    return mPostXScale;
  }
  float PostYScale() const {
    return mPostYScale;
  }
  uint32_t ContentFlags() const {
    return mContentFlags;
  }
  float Opacity() const {
    return mOpacity;
  }
  bool IsFixedPosition() const {
    return mIsFixedPosition;
  }
  FrameMetrics::ViewID ScrollbarTargetContainerId() const {
    return mScrollbarTargetContainerId;
  }
  ScrollDirection ScrollbarDirection() const {
    return mScrollbarDirection;
  }
  float ScrollbarThumbRatio() const {
    return mScrollbarThumbRatio;
  }
  float IsScrollbarContainer() const {
    return mIsScrollbarContainer;
  }
  gfx::CompositionOp MixBlendMode() const {
    return mMixBlendMode;
  }
  bool ForceIsolatedGroup() const {
    return mForceIsolatedGroup;
  }
  const gfx::Matrix4x4& Transform() const {
    return mTransform;
  }
  bool TransformIsPerspective() const {
    return mTransformIsPerspective;
  }
  const Maybe<LayerClip>& ScrolledClip() const {
    return mScrolledClip;
  }
  FrameMetrics::ViewID FixedPositionScrollContainerId() const {
    return mFixedPositionData
           ? mFixedPositionData->mScrollId
           : FrameMetrics::NULL_SCROLL_ID;
  }
  LayerPoint FixedPositionAnchor() const {
    return mFixedPositionData ? mFixedPositionData->mAnchor : LayerPoint();
  }
  int32_t FixedPositionSides() const {
    return mFixedPositionData ? mFixedPositionData->mSides : eSideBitsNone;
  }
  bool IsStickyPosition() const {
    return !!mStickyPositionData;
  }
  FrameMetrics::ViewID StickyScrollContainerId() const {
    return mStickyPositionData->mScrollId;
  }
  const LayerRect& StickyScrollRangeOuter() const {
    return mStickyPositionData->mOuter;
  }
  const LayerRect& StickyScrollRangeInner() const {
    return mStickyPositionData->mInner;
  }

  bool operator ==(const SimpleLayerAttributes& aOther) const {
    return mLayerBounds == aOther.mLayerBounds &&
           mTransform == aOther.mTransform &&
           mTransformIsPerspective == aOther.mTransformIsPerspective &&
           mScrolledClip == aOther.mScrolledClip &&
           mPostXScale == aOther.mPostXScale &&
           mPostYScale == aOther.mPostYScale &&
           mContentFlags == aOther.mContentFlags &&
           mOpacity == aOther.mOpacity &&
           mIsFixedPosition == aOther.mIsFixedPosition &&
           mScrollbarTargetContainerId == aOther.mScrollbarTargetContainerId &&
           mScrollbarDirection == aOther.mScrollbarDirection &&
           mScrollbarThumbRatio == aOther.mScrollbarThumbRatio &&
           mIsScrollbarContainer == aOther.mIsScrollbarContainer &&
           mMixBlendMode == aOther.mMixBlendMode &&
           mForceIsolatedGroup == aOther.mForceIsolatedGroup;
  }

private:
  gfx::IntRect mLayerBounds;
  gfx::Matrix4x4 mTransform;
  bool mTransformIsPerspective;
  Maybe<LayerClip> mScrolledClip;
  float mPostXScale;
  float mPostYScale;
  uint32_t mContentFlags;
  float mOpacity;
  bool mIsFixedPosition;
  uint64_t mScrollbarTargetContainerId;
  ScrollDirection mScrollbarDirection;
  // The scrollbar thumb ratio is the ratio of the thumb position (in the CSS
  // pixels of the scrollframe's parent's space) to the scroll position (in the
  // CSS pixels of the scrollframe's space).
  float mScrollbarThumbRatio;
  bool mIsScrollbarContainer;
  gfx::CompositionOp mMixBlendMode;
  bool mForceIsolatedGroup;

  struct FixedPositionData {
    FrameMetrics::ViewID mScrollId;
    LayerPoint mAnchor;
    int32_t mSides;
  };
  Maybe<FixedPositionData> mFixedPositionData;

  struct StickyPositionData {
    FrameMetrics::ViewID mScrollId;
    LayerRect mOuter;
    LayerRect mInner;
  };
  Maybe<StickyPositionData> mStickyPositionData;

  // This class may only contain plain-old-data members that can be safely
  // copied over IPC. Make sure to add new members to operator ==.
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_LayerAttributes_h
