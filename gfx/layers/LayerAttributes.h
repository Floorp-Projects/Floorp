/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_LayerAttributes_h
#define mozilla_gfx_layers_LayerAttributes_h

#include "mozilla/Maybe.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersTypes.h"

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {
namespace layers {

enum class ScrollbarLayerType : uint8_t { None, Thumb, Container };

/**
 *  It stores data for scroll thumb layer or container layers.
 */
struct ScrollbarData {
  ScrollbarData() = default;

  ScrollbarData(ScrollDirection aDirection,
                ScrollbarLayerType aScrollbarLayerType,
                float aThumbRatio,
                CSSCoord aThumbStart,
                CSSCoord aThumbLength,
                bool aThumbIsAsyncDraggable,
                CSSCoord aScrollTrackStart,
                CSSCoord aScrollTrackLength,
                uint64_t aTargetViewId)
    : mDirection(Some(aDirection))
    , mScrollbarLayerType(aScrollbarLayerType)
    , mThumbRatio(aThumbRatio)
    , mThumbStart(aThumbStart)
    , mThumbLength(aThumbLength)
    , mThumbIsAsyncDraggable(aThumbIsAsyncDraggable)
    , mScrollTrackStart(aScrollTrackStart)
    , mScrollTrackLength(aScrollTrackLength)
    , mTargetViewId(aTargetViewId)
  {}

  /**
   * The mDirection contains a direction if mScrollbarLayerType is Thumb
   * or Container, otherwise it's empty.
   */
  Maybe<ScrollDirection> mDirection;

  /**
   * Indicate what kind of layer this data is for. All possibilities are defined in
   * enum ScrollbarLayerType
   */
  ScrollbarLayerType mScrollbarLayerType = ScrollbarLayerType::None;

  /**
   * The scrollbar thumb ratio is the ratio of the thumb position (in the CSS
   * pixels of the scrollframe's parent's space) to the scroll position (in the
   * CSS pixels of the scrollframe's space).
   */
  float mThumbRatio = 0.0f;

  CSSCoord mThumbStart;
  CSSCoord mThumbLength;

  /**
   * Whether the scrollbar thumb can be dragged asynchronously.
   */
  bool mThumbIsAsyncDraggable = false;

  CSSCoord mScrollTrackStart;
  CSSCoord mScrollTrackLength;
  uint64_t mTargetViewId = FrameMetrics::NULL_SCROLL_ID;

  bool operator==(const ScrollbarData& aOther) const {
    return mDirection == aOther.mDirection &&
           mScrollbarLayerType == aOther.mScrollbarLayerType &&
           mThumbRatio == aOther.mThumbRatio &&
           mThumbStart == aOther.mThumbStart &&
           mThumbLength == aOther.mThumbLength &&
           mThumbIsAsyncDraggable == aOther.mThumbIsAsyncDraggable &&
           mScrollTrackStart == aOther.mScrollTrackStart &&
           mScrollTrackLength == aOther.mScrollTrackLength &&
           mTargetViewId == aOther.mTargetViewId;
  }
  bool operator!=(const ScrollbarData& aOther) const {
    return !(*this == aOther);
  }
};

/**
 * Infrequently changing layer attributes that require no special
 * serialization work.
 */
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
     mMixBlendMode(gfx::CompositionOp::OP_OVER),
     mForceIsolatedGroup(false)
  {
  }

  /**
   * Setters.
   * All set methods return true if values changed, false otherwise.
   */

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

  bool SetScrollbarData(const ScrollbarData& aScrollbarData) {
    if (mScrollbarData == aScrollbarData)
    {
      return false;
    }
    mScrollbarData = aScrollbarData;
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

  bool SetFixedPositionData(FrameMetrics::ViewID aTargetViewId,
                            const LayerPoint& aAnchor,
                            int32_t aSides)
  {
    if (mFixedPositionData &&
        mFixedPositionData->mScrollId == aTargetViewId &&
        mFixedPositionData->mAnchor == aAnchor &&
        mFixedPositionData->mSides == aSides) {
      return false;
    }
    if (!mFixedPositionData) {
      mFixedPositionData.emplace();
    }
    mFixedPositionData->mScrollId = aTargetViewId;
    mFixedPositionData->mAnchor = aAnchor;
    mFixedPositionData->mSides = aSides;
    return true;
  }

  bool SetStickyPositionData(FrameMetrics::ViewID aScrollId,
                             LayerRectAbsolute aOuter, LayerRectAbsolute aInner)
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

  /**
   * This returns true if scrolling info is equivalent for the purposes of
   * APZ hit testing.
   */
  bool HitTestingInfoIsEqual(const SimpleLayerAttributes& aOther) const {
    if (mScrollbarData != aOther.mScrollbarData) {
      return false;
    }
    if (GetFixedPositionScrollContainerId() != aOther.GetFixedPositionScrollContainerId()) {
      return false;
    }
    if (mTransform != aOther.mTransform) {
      return false;
    }
    return true;
  }

  /**
   * Getters.
   */

  float GetPostXScale() const {
    return mPostXScale;
  }

  float GetPostYScale() const {
    return mPostYScale;
  }

  uint32_t GetContentFlags() const {
    return mContentFlags;
  }

  float GetOpacity() const {
    return mOpacity;
  }

  bool IsFixedPosition() const {
    return mIsFixedPosition;
  }

  FrameMetrics::ViewID GetScrollbarTargetViewId() const {
    return mScrollbarData.mTargetViewId;
  }

  const ScrollbarData& GetScrollbarData() const {
    return mScrollbarData;
  }

  Maybe<ScrollDirection> GetScrollbarContainerDirection() const {
    return (mScrollbarData.mScrollbarLayerType == ScrollbarLayerType::Container)
      ? mScrollbarData.mDirection
      : Nothing();
  }

  gfx::CompositionOp GetMixBlendMode() const {
    return mMixBlendMode;
  }

  bool GetForceIsolatedGroup() const {
    return mForceIsolatedGroup;
  }

  const gfx::Matrix4x4& GetTransform() const {
    return mTransform;
  }

  bool GetTransformIsPerspective() const {
    return mTransformIsPerspective;
  }

  const Maybe<LayerClip>& GetScrolledClip() const {
    return mScrolledClip;
  }

  FrameMetrics::ViewID GetFixedPositionScrollContainerId() const {
    return mFixedPositionData
           ? mFixedPositionData->mScrollId
           : FrameMetrics::NULL_SCROLL_ID;
  }

  LayerPoint GetFixedPositionAnchor() const {
    return mFixedPositionData ? mFixedPositionData->mAnchor : LayerPoint();
  }

  int32_t GetFixedPositionSides() const {
    return mFixedPositionData ? mFixedPositionData->mSides : eSideBitsNone;
  }

  bool IsStickyPosition() const {
    return !!mStickyPositionData;
  }

  FrameMetrics::ViewID GetStickyScrollContainerId() const {
    return mStickyPositionData->mScrollId;
  }

  const LayerRectAbsolute& GetStickyScrollRangeOuter() const {
    return mStickyPositionData->mOuter;
  }

  const LayerRectAbsolute& GetStickyScrollRangeInner() const {
    return mStickyPositionData->mInner;
  }

  bool operator ==(const SimpleLayerAttributes& aOther) const {
    return mTransform == aOther.mTransform &&
           mTransformIsPerspective == aOther.mTransformIsPerspective &&
           mScrolledClip == aOther.mScrolledClip &&
           mPostXScale == aOther.mPostXScale &&
           mPostYScale == aOther.mPostYScale &&
           mContentFlags == aOther.mContentFlags &&
           mOpacity == aOther.mOpacity &&
           mIsFixedPosition == aOther.mIsFixedPosition &&
           mScrollbarData == aOther.mScrollbarData &&
           mMixBlendMode == aOther.mMixBlendMode &&
           mForceIsolatedGroup == aOther.mForceIsolatedGroup;
  }

private:
  gfx::Matrix4x4 mTransform;
  bool mTransformIsPerspective;
  Maybe<LayerClip> mScrolledClip;
  float mPostXScale;
  float mPostYScale;
  uint32_t mContentFlags;
  float mOpacity;
  bool mIsFixedPosition;
  ScrollbarData mScrollbarData;
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
    LayerRectAbsolute mOuter;
    LayerRectAbsolute mInner;
  };
  Maybe<StickyPositionData> mStickyPositionData;

  /**
   * This class may only contain plain-old-data members that can be safely
   * copied over IPC. Make sure to add new members to operator ==.
   */
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_LayerAttributes_h
