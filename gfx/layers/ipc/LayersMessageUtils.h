/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_LayersMessageUtils
#define mozilla_layers_LayersMessageUtils

#include <stdint.h>

#include <utility>

#include "FrameMetrics.h"
#include "VsyncSource.h"
#include "chrome/common/ipc_message_utils.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/MotionPathUtils.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ipc/ByteBuf.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/GeckoContentControllerTypes.h"
#include "mozilla/layers/KeyboardMap.h"
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/MatrixMessage.h"
#include "mozilla/layers/RepaintRequest.h"
#include "nsSize.h"
#include "mozilla/layers/DoubleTapToZoom.h"

// For ParamTraits, could be moved to cpp file
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/ipc/ByteBufUtils.h"

#ifdef _MSC_VER
#  pragma warning(disable : 4800)
#endif

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::LayersId>
    : public PlainOldDataSerializer<mozilla::layers::LayersId> {};

template <typename T>
struct ParamTraits<mozilla::layers::BaseTransactionId<T>>
    : public PlainOldDataSerializer<mozilla::layers::BaseTransactionId<T>> {};

template <>
struct ParamTraits<mozilla::VsyncId>
    : public PlainOldDataSerializer<mozilla::VsyncId> {};

template <>
struct ParamTraits<mozilla::VsyncEvent> {
  typedef mozilla::VsyncEvent paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mId);
    WriteParam(msg, param.mTime);
    WriteParam(msg, param.mOutputTime);
  }
  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return ReadParam(msg, iter, &result->mId) &&
           ReadParam(msg, iter, &result->mTime) &&
           ReadParam(msg, iter, &result->mOutputTime);
  }
};

template <>
struct ParamTraits<mozilla::layers::MatrixMessage> {
  typedef mozilla::layers::MatrixMessage paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mMatrix);
    WriteParam(aMsg, aParam.mTopLevelViewportVisibleRectInBrowserCoords);
    WriteParam(aMsg, aParam.mLayersId);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mMatrix) &&
           ReadParam(aMsg, aIter,
                     &aResult->mTopLevelViewportVisibleRectInBrowserCoords) &&
           ReadParam(aMsg, aIter, &aResult->mLayersId);
  }
};

template <>
struct ParamTraits<mozilla::layers::LayersObserverEpoch>
    : public PlainOldDataSerializer<mozilla::layers::LayersObserverEpoch> {};

template <>
struct ParamTraits<mozilla::layers::WindowKind>
    : public ContiguousEnumSerializer<mozilla::layers::WindowKind,
                                      mozilla::layers::WindowKind::MAIN,
                                      mozilla::layers::WindowKind::LAST> {};

template <>
struct ParamTraits<mozilla::layers::LayersBackend>
    : public ContiguousEnumSerializer<
          mozilla::layers::LayersBackend,
          mozilla::layers::LayersBackend::LAYERS_NONE,
          mozilla::layers::LayersBackend::LAYERS_LAST> {};

template <>
struct ParamTraits<mozilla::layers::WebRenderBackend>
    : public ContiguousEnumSerializer<
          mozilla::layers::WebRenderBackend,
          mozilla::layers::WebRenderBackend::HARDWARE,
          mozilla::layers::WebRenderBackend::LAST> {};

template <>
struct ParamTraits<mozilla::layers::WebRenderCompositor>
    : public ContiguousEnumSerializer<
          mozilla::layers::WebRenderCompositor,
          mozilla::layers::WebRenderCompositor::DRAW,
          mozilla::layers::WebRenderCompositor::LAST> {};

template <>
struct ParamTraits<mozilla::layers::TextureType>
    : public ContiguousEnumSerializer<mozilla::layers::TextureType,
                                      mozilla::layers::TextureType::Unknown,
                                      mozilla::layers::TextureType::Last> {};

template <>
struct ParamTraits<mozilla::layers::ScaleMode>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::ScaleMode, mozilla::layers::ScaleMode::SCALE_NONE,
          mozilla::layers::kHighestScaleMode> {};

template <>
struct ParamTraits<mozilla::StyleScrollSnapStrictness>
    : public ContiguousEnumSerializerInclusive<
          mozilla::StyleScrollSnapStrictness,
          mozilla::StyleScrollSnapStrictness::None,
          mozilla::StyleScrollSnapStrictness::Proximity> {};

template <>
struct ParamTraits<mozilla::layers::TextureFlags>
    : public BitFlagsEnumSerializer<mozilla::layers::TextureFlags,
                                    mozilla::layers::TextureFlags::ALL_BITS> {};

template <>
struct ParamTraits<mozilla::layers::DiagnosticTypes>
    : public BitFlagsEnumSerializer<
          mozilla::layers::DiagnosticTypes,
          mozilla::layers::DiagnosticTypes::ALL_BITS> {};

template <>
struct ParamTraits<mozilla::layers::ScrollDirection>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::ScrollDirection,
          mozilla::layers::ScrollDirection::eVertical,
          mozilla::layers::kHighestScrollDirection> {};

template <>
struct ParamTraits<mozilla::layers::FrameMetrics::ScrollOffsetUpdateType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::FrameMetrics::ScrollOffsetUpdateType,
          mozilla::layers::FrameMetrics::ScrollOffsetUpdateType::eNone,
          mozilla::layers::FrameMetrics::sHighestScrollOffsetUpdateType> {};

template <>
struct ParamTraits<mozilla::layers::RepaintRequest::ScrollOffsetUpdateType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::RepaintRequest::ScrollOffsetUpdateType,
          mozilla::layers::RepaintRequest::ScrollOffsetUpdateType::eNone,
          mozilla::layers::RepaintRequest::sHighestScrollOffsetUpdateType> {};

template <>
struct ParamTraits<mozilla::layers::OverscrollBehavior>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::OverscrollBehavior,
          mozilla::layers::OverscrollBehavior::Auto,
          mozilla::layers::kHighestOverscrollBehavior> {};

template <>
struct ParamTraits<mozilla::layers::LayerHandle> {
  typedef mozilla::layers::LayerHandle paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mHandle);
  }
  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return ReadParam(msg, iter, &result->mHandle);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositableHandle> {
  typedef mozilla::layers::CompositableHandle paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mHandle);
  }
  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return ReadParam(msg, iter, &result->mHandle);
  }
};

template <>
struct ParamTraits<mozilla::layers::FrameMetrics>
    : BitfieldHelper<mozilla::layers::FrameMetrics> {
  typedef mozilla::layers::FrameMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mPresShellResolution);
    WriteParam(aMsg, aParam.mCompositionBounds);
    WriteParam(aMsg, aParam.mCompositionBoundsWidthIgnoringScrollbars);
    WriteParam(aMsg, aParam.mDisplayPort);
    WriteParam(aMsg, aParam.mCriticalDisplayPort);
    WriteParam(aMsg, aParam.mScrollableRect);
    WriteParam(aMsg, aParam.mCumulativeResolution);
    WriteParam(aMsg, aParam.mDevPixelsPerCSSPixel);
    WriteParam(aMsg, aParam.mScrollOffset);
    WriteParam(aMsg, aParam.mZoom);
    WriteParam(aMsg, aParam.mScrollGeneration);
    WriteParam(aMsg, aParam.mBoundingCompositionSize);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mLayoutViewport);
    WriteParam(aMsg, aParam.mExtraResolution);
    WriteParam(aMsg, aParam.mPaintRequestTime);
    WriteParam(aMsg, aParam.mVisualDestination);
    WriteParam(aMsg, aParam.mVisualScrollUpdateType);
    WriteParam(aMsg, aParam.mFixedLayerMargins);
    WriteParam(aMsg, aParam.mCompositionSizeWithoutDynamicToolbar);
    WriteParam(aMsg, aParam.mIsRootContent);
    WriteParam(aMsg, aParam.mIsScrollInfoLayer);
    WriteParam(aMsg, aParam.mHasNonZeroDisplayPortMargins);
    WriteParam(aMsg, aParam.mMinimalDisplayPort);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellResolution) &&
            ReadParam(aMsg, aIter, &aResult->mCompositionBounds) &&
            ReadParam(aMsg, aIter,
                      &aResult->mCompositionBoundsWidthIgnoringScrollbars) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCriticalDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mScrollableRect) &&
            ReadParam(aMsg, aIter, &aResult->mCumulativeResolution) &&
            ReadParam(aMsg, aIter, &aResult->mDevPixelsPerCSSPixel) &&
            ReadParam(aMsg, aIter, &aResult->mScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mZoom) &&
            ReadParam(aMsg, aIter, &aResult->mScrollGeneration) &&
            ReadParam(aMsg, aIter, &aResult->mBoundingCompositionSize) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mLayoutViewport) &&
            ReadParam(aMsg, aIter, &aResult->mExtraResolution) &&
            ReadParam(aMsg, aIter, &aResult->mPaintRequestTime) &&
            ReadParam(aMsg, aIter, &aResult->mVisualDestination) &&
            ReadParam(aMsg, aIter, &aResult->mVisualScrollUpdateType) &&
            ReadParam(aMsg, aIter, &aResult->mFixedLayerMargins) &&
            ReadParam(aMsg, aIter,
                      &aResult->mCompositionSizeWithoutDynamicToolbar) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsRootContent) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsScrollInfoLayer) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetHasNonZeroDisplayPortMargins) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetMinimalDisplayPort));
  }
};

template <>
struct ParamTraits<mozilla::layers::RepaintRequest>
    : BitfieldHelper<mozilla::layers::RepaintRequest> {
  typedef mozilla::layers::RepaintRequest paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mPresShellResolution);
    WriteParam(aMsg, aParam.mCompositionBounds);
    WriteParam(aMsg, aParam.mCumulativeResolution);
    WriteParam(aMsg, aParam.mDevPixelsPerCSSPixel);
    WriteParam(aMsg, aParam.mScrollOffset);
    WriteParam(aMsg, aParam.mZoom);
    WriteParam(aMsg, aParam.mScrollGeneration);
    WriteParam(aMsg, aParam.mDisplayPortMargins);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mLayoutViewport);
    WriteParam(aMsg, aParam.mExtraResolution);
    WriteParam(aMsg, aParam.mPaintRequestTime);
    WriteParam(aMsg, aParam.mScrollUpdateType);
    WriteParam(aMsg, aParam.mIsRootContent);
    WriteParam(aMsg, aParam.mIsAnimationInProgress);
    WriteParam(aMsg, aParam.mIsScrollInfoLayer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellResolution) &&
            ReadParam(aMsg, aIter, &aResult->mCompositionBounds) &&
            ReadParam(aMsg, aIter, &aResult->mCumulativeResolution) &&
            ReadParam(aMsg, aIter, &aResult->mDevPixelsPerCSSPixel) &&
            ReadParam(aMsg, aIter, &aResult->mScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mZoom) &&
            ReadParam(aMsg, aIter, &aResult->mScrollGeneration) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPortMargins) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mLayoutViewport) &&
            ReadParam(aMsg, aIter, &aResult->mExtraResolution) &&
            ReadParam(aMsg, aIter, &aResult->mPaintRequestTime) &&
            ReadParam(aMsg, aIter, &aResult->mScrollUpdateType) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsRootContent) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsAnimationInProgress) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsScrollInfoLayer));
  }
};

template <>
struct ParamTraits<nsSize> {
  typedef nsSize paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height);
  }
};

template <>
struct ParamTraits<mozilla::layers::ScrollSnapInfo::ScrollSnapRange> {
  typedef mozilla::layers::ScrollSnapInfo::ScrollSnapRange paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStart);
    WriteParam(aMsg, aParam.mEnd);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mStart) &&
           ReadParam(aMsg, aIter, &aResult->mEnd);
  }
};

template <>
struct ParamTraits<mozilla::layers::ScrollSnapInfo> {
  typedef mozilla::layers::ScrollSnapInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mScrollSnapStrictnessX);
    WriteParam(aMsg, aParam.mScrollSnapStrictnessY);
    WriteParam(aMsg, aParam.mSnapPositionX);
    WriteParam(aMsg, aParam.mSnapPositionY);
    WriteParam(aMsg, aParam.mXRangeWiderThanSnapport);
    WriteParam(aMsg, aParam.mYRangeWiderThanSnapport);
    WriteParam(aMsg, aParam.mSnapportSize);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mScrollSnapStrictnessX) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapStrictnessY) &&
            ReadParam(aMsg, aIter, &aResult->mSnapPositionX) &&
            ReadParam(aMsg, aIter, &aResult->mSnapPositionY) &&
            ReadParam(aMsg, aIter, &aResult->mXRangeWiderThanSnapport) &&
            ReadParam(aMsg, aIter, &aResult->mYRangeWiderThanSnapport) &&
            ReadParam(aMsg, aIter, &aResult->mSnapportSize));
  }
};

template <>
struct ParamTraits<mozilla::layers::OverscrollBehaviorInfo> {
  // Not using PlainOldDataSerializer so we get enum validation
  // for the members.

  typedef mozilla::layers::OverscrollBehaviorInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBehaviorX);
    WriteParam(aMsg, aParam.mBehaviorY);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mBehaviorX) &&
            ReadParam(aMsg, aIter, &aResult->mBehaviorY));
  }
};

template <>
struct ParamTraits<mozilla::layers::LayerClip> {
  typedef mozilla::layers::LayerClip paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mClipRect);
    WriteParam(aMsg, aParam.mMaskLayerIndex);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mClipRect) &&
            ReadParam(aMsg, aIter, &aResult->mMaskLayerIndex));
  }
};

template <>
struct ParamTraits<mozilla::ScrollGeneration>
    : PlainOldDataSerializer<mozilla::ScrollGeneration> {};

template <>
struct ParamTraits<mozilla::ScrollPositionUpdate>
    : PlainOldDataSerializer<mozilla::ScrollPositionUpdate> {};

template <>
struct ParamTraits<mozilla::layers::ScrollMetadata>
    : BitfieldHelper<mozilla::layers::ScrollMetadata> {
  typedef mozilla::layers::ScrollMetadata paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mMetrics);
    WriteParam(aMsg, aParam.mSnapInfo);
    WriteParam(aMsg, aParam.mScrollParentId);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.GetContentDescription());
    WriteParam(aMsg, aParam.mLineScrollAmount);
    WriteParam(aMsg, aParam.mPageScrollAmount);
    WriteParam(aMsg, aParam.mScrollClip);
    WriteParam(aMsg, aParam.mHasScrollgrab);
    WriteParam(aMsg, aParam.mIsLayersIdRoot);
    WriteParam(aMsg, aParam.mIsAutoDirRootContentRTL);
    WriteParam(aMsg, aParam.mForceDisableApz);
    WriteParam(aMsg, aParam.mResolutionUpdated);
    WriteParam(aMsg, aParam.mIsRDMTouchSimulationActive);
    WriteParam(aMsg, aParam.mDidContentGetPainted);
    WriteParam(aMsg, aParam.mPrefersReducedMotion);
    WriteParam(aMsg, aParam.mDisregardedDirection);
    WriteParam(aMsg, aParam.mOverscrollBehavior);
    WriteParam(aMsg, aParam.mScrollUpdates);
  }

  static bool ReadContentDescription(const Message* aMsg, PickleIterator* aIter,
                                     paramType* aResult) {
    nsCString str;
    if (!ReadParam(aMsg, aIter, &str)) {
      return false;
    }
    aResult->SetContentDescription(str);
    return true;
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mMetrics) &&
            ReadParam(aMsg, aIter, &aResult->mSnapInfo) &&
            ReadParam(aMsg, aIter, &aResult->mScrollParentId) &&
            ReadParam(aMsg, aIter, &aResult->mBackgroundColor) &&
            ReadContentDescription(aMsg, aIter, aResult) &&
            ReadParam(aMsg, aIter, &aResult->mLineScrollAmount) &&
            ReadParam(aMsg, aIter, &aResult->mPageScrollAmount) &&
            ReadParam(aMsg, aIter, &aResult->mScrollClip) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetHasScrollgrab) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsLayersIdRoot) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsAutoDirRootContentRTL) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetForceDisableApz) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetResolutionUpdated) &&
            ReadBoolForBitfield(aMsg, aIter, aResult,
                                &paramType::SetIsRDMTouchSimulationActive)) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetDidContentGetPainted) &&
           ReadBoolForBitfield(aMsg, aIter, aResult,
                               &paramType::SetPrefersReducedMotion) &&
           ReadParam(aMsg, aIter, &aResult->mDisregardedDirection) &&
           ReadParam(aMsg, aIter, &aResult->mOverscrollBehavior) &&
           ReadParam(aMsg, aIter, &aResult->mScrollUpdates);
  }
};

template <>
struct ParamTraits<mozilla::layers::TextureFactoryIdentifier> {
  typedef mozilla::layers::TextureFactoryIdentifier paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mParentBackend);
    WriteParam(aMsg, aParam.mWebRenderBackend);
    WriteParam(aMsg, aParam.mWebRenderCompositor);
    WriteParam(aMsg, aParam.mParentProcessType);
    WriteParam(aMsg, aParam.mMaxTextureSize);
    WriteParam(aMsg, aParam.mSupportsTextureDirectMapping);
    WriteParam(aMsg, aParam.mCompositorUseANGLE);
    WriteParam(aMsg, aParam.mCompositorUseDComp);
    WriteParam(aMsg, aParam.mUseCompositorWnd);
    WriteParam(aMsg, aParam.mSupportsTextureBlitting);
    WriteParam(aMsg, aParam.mSupportsPartialUploads);
    WriteParam(aMsg, aParam.mSupportsComponentAlpha);
    WriteParam(aMsg, aParam.mUsingAdvancedLayers);
    WriteParam(aMsg, aParam.mSyncHandle);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    bool result =
        ReadParam(aMsg, aIter, &aResult->mParentBackend) &&
        ReadParam(aMsg, aIter, &aResult->mWebRenderBackend) &&
        ReadParam(aMsg, aIter, &aResult->mWebRenderCompositor) &&
        ReadParam(aMsg, aIter, &aResult->mParentProcessType) &&
        ReadParam(aMsg, aIter, &aResult->mMaxTextureSize) &&
        ReadParam(aMsg, aIter, &aResult->mSupportsTextureDirectMapping) &&
        ReadParam(aMsg, aIter, &aResult->mCompositorUseANGLE) &&
        ReadParam(aMsg, aIter, &aResult->mCompositorUseDComp) &&
        ReadParam(aMsg, aIter, &aResult->mUseCompositorWnd) &&
        ReadParam(aMsg, aIter, &aResult->mSupportsTextureBlitting) &&
        ReadParam(aMsg, aIter, &aResult->mSupportsPartialUploads) &&
        ReadParam(aMsg, aIter, &aResult->mSupportsComponentAlpha) &&
        ReadParam(aMsg, aIter, &aResult->mUsingAdvancedLayers) &&
        ReadParam(aMsg, aIter, &aResult->mSyncHandle);
    return result;
  }
};

template <>
struct ParamTraits<mozilla::layers::TextureInfo> {
  typedef mozilla::layers::TextureInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mCompositableType);
    WriteParam(aMsg, aParam.mTextureFlags);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mCompositableType) &&
           ReadParam(aMsg, aIter, &aResult->mTextureFlags);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositableType>
    : public ContiguousEnumSerializer<
          mozilla::layers::CompositableType,
          mozilla::layers::CompositableType::UNKNOWN,
          mozilla::layers::CompositableType::COUNT> {};

template <>
struct ParamTraits<mozilla::layers::ScrollableLayerGuid> {
  typedef mozilla::layers::ScrollableLayerGuid paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLayersId);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mScrollId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mLayersId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId));
  }
};

template <>
struct ParamTraits<nsEventStatus>
    : public ContiguousEnumSerializer<nsEventStatus, nsEventStatus_eIgnore,
                                      nsEventStatus_eSentinel> {};

template <>
struct ParamTraits<mozilla::layers::APZHandledPlace>
    : public ContiguousEnumSerializer<
          mozilla::layers::APZHandledPlace,
          mozilla::layers::APZHandledPlace::Unhandled,
          mozilla::layers::APZHandledPlace::Last> {};

template <>
struct ParamTraits<mozilla::layers::ScrollDirections> {
  typedef mozilla::layers::ScrollDirections paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.serialize());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint8_t value;
    if (!ReadParam(aMsg, aIter, &value)) {
      return false;
    }
    aResult->deserialize(value);
    return true;
  }
};

template <>
struct ParamTraits<mozilla::layers::APZHandledResult> {
  typedef mozilla::layers::APZHandledResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mPlace);
    WriteParam(aMsg, aParam.mScrollableDirections);
    WriteParam(aMsg, aParam.mOverscrollDirections);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mPlace) &&
            ReadParam(aMsg, aIter, &aResult->mScrollableDirections) &&
            ReadParam(aMsg, aIter, &aResult->mOverscrollDirections));
  }
};

template <>
struct ParamTraits<mozilla::layers::APZEventResult> {
  typedef mozilla::layers::APZEventResult paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.GetStatus());
    WriteParam(aMsg, aParam.GetHandledResult());
    WriteParam(aMsg, aParam.mTargetGuid);
    WriteParam(aMsg, aParam.mInputBlockId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsEventStatus status;
    if (!ReadParam(aMsg, aIter, &status)) {
      return false;
    }
    aResult->UpdateStatus(status);

    mozilla::Maybe<mozilla::layers::APZHandledResult> handledResult;
    if (!ReadParam(aMsg, aIter, &handledResult)) {
      return false;
    }
    aResult->UpdateHandledResult(handledResult);

    return (ReadParam(aMsg, aIter, &aResult->mTargetGuid) &&
            ReadParam(aMsg, aIter, &aResult->mInputBlockId));
  }
};

template <>
struct ParamTraits<mozilla::layers::ZoomConstraints> {
  typedef mozilla::layers::ZoomConstraints paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mAllowZoom);
    WriteParam(aMsg, aParam.mAllowDoubleTapZoom);
    WriteParam(aMsg, aParam.mMinZoom);
    WriteParam(aMsg, aParam.mMaxZoom);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mAllowZoom) &&
            ReadParam(aMsg, aIter, &aResult->mAllowDoubleTapZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMinZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMaxZoom));
  }
};

template <>
struct ParamTraits<mozilla::layers::EventRegions> {
  typedef mozilla::layers::EventRegions paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mHitRegion);
    WriteParam(aMsg, aParam.mDispatchToContentHitRegion);
    WriteParam(aMsg, aParam.mNoActionRegion);
    WriteParam(aMsg, aParam.mHorizontalPanRegion);
    WriteParam(aMsg, aParam.mVerticalPanRegion);
    WriteParam(aMsg, aParam.mDTCRequiresTargetConfirmation);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mHitRegion) &&
            ReadParam(aMsg, aIter, &aResult->mDispatchToContentHitRegion) &&
            ReadParam(aMsg, aIter, &aResult->mNoActionRegion) &&
            ReadParam(aMsg, aIter, &aResult->mHorizontalPanRegion) &&
            ReadParam(aMsg, aIter, &aResult->mVerticalPanRegion) &&
            ReadParam(aMsg, aIter, &aResult->mDTCRequiresTargetConfirmation));
  }
};

template <>
struct ParamTraits<mozilla::layers::FocusTarget::ScrollTargets> {
  typedef mozilla::layers::FocusTarget::ScrollTargets paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mHorizontal);
    WriteParam(aMsg, aParam.mVertical);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mHorizontal) &&
           ReadParam(aMsg, aIter, &aResult->mVertical);
  }
};

template <>
struct ParamTraits<mozilla::layers::FocusTarget::NoFocusTarget>
    : public EmptyStructSerializer<
          mozilla::layers::FocusTarget::NoFocusTarget> {};

template <>
struct ParamTraits<mozilla::layers::FocusTarget> {
  typedef mozilla::layers::FocusTarget paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mSequenceNumber);
    WriteParam(aMsg, aParam.mFocusHasKeyEventListeners);
    WriteParam(aMsg, aParam.mData);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mSequenceNumber) ||
        !ReadParam(aMsg, aIter, &aResult->mFocusHasKeyEventListeners) ||
        !ReadParam(aMsg, aIter, &aResult->mData)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<
    mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType,
          mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType::
              eScrollCharacter,
          mozilla::layers::KeyboardScrollAction::
              sHighestKeyboardScrollActionType> {};

template <>
struct ParamTraits<mozilla::layers::KeyboardScrollAction> {
  typedef mozilla::layers::KeyboardScrollAction paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mForward);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mForward);
  }
};

template <>
struct ParamTraits<mozilla::layers::KeyboardShortcut> {
  typedef mozilla::layers::KeyboardShortcut paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mAction);
    WriteParam(aMsg, aParam.mKeyCode);
    WriteParam(aMsg, aParam.mCharCode);
    WriteParam(aMsg, aParam.mModifiers);
    WriteParam(aMsg, aParam.mModifiersMask);
    WriteParam(aMsg, aParam.mEventType);
    WriteParam(aMsg, aParam.mDispatchToContent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mAction) &&
           ReadParam(aMsg, aIter, &aResult->mKeyCode) &&
           ReadParam(aMsg, aIter, &aResult->mCharCode) &&
           ReadParam(aMsg, aIter, &aResult->mModifiers) &&
           ReadParam(aMsg, aIter, &aResult->mModifiersMask) &&
           ReadParam(aMsg, aIter, &aResult->mEventType) &&
           ReadParam(aMsg, aIter, &aResult->mDispatchToContent);
  }
};

template <>
struct ParamTraits<mozilla::layers::KeyboardMap> {
  typedef mozilla::layers::KeyboardMap paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.Shortcuts());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    nsTArray<mozilla::layers::KeyboardShortcut> shortcuts;
    if (!ReadParam(aMsg, aIter, &shortcuts)) {
      return false;
    }
    *aResult = mozilla::layers::KeyboardMap(std::move(shortcuts));
    return true;
  }
};

template <>
struct ParamTraits<mozilla::layers::GeckoContentController_TapType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::GeckoContentController_TapType,
          mozilla::layers::GeckoContentController_TapType::eSingleTap,
          mozilla::layers::kHighestGeckoContentController_TapType> {};

template <>
struct ParamTraits<mozilla::layers::GeckoContentController_APZStateChange>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::GeckoContentController_APZStateChange,
          mozilla::layers::GeckoContentController_APZStateChange::
              eTransformBegin,
          mozilla::layers::kHighestGeckoContentController_APZStateChange> {};

template <>
struct ParamTraits<mozilla::layers::EventRegionsOverride>
    : public BitFlagsEnumSerializer<
          mozilla::layers::EventRegionsOverride,
          mozilla::layers::EventRegionsOverride::ALL_BITS> {};

template <>
struct ParamTraits<mozilla::layers::AsyncDragMetrics> {
  typedef mozilla::layers::AsyncDragMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mViewId);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mDragStartSequenceNumber);
    WriteParam(aMsg, aParam.mScrollbarDragOffset);
    WriteParam(aMsg, aParam.mDirection);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mViewId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mDragStartSequenceNumber) &&
            ReadParam(aMsg, aIter, &aResult->mScrollbarDragOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDirection));
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositorOptions> {
  typedef mozilla::layers::CompositorOptions paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mUseAPZ);
    WriteParam(aMsg, aParam.mUseSoftwareWebRender);
    WriteParam(aMsg, aParam.mAllowSoftwareWebRenderD3D11);
    WriteParam(aMsg, aParam.mAllowSoftwareWebRenderOGL);
    WriteParam(aMsg, aParam.mUseAdvancedLayers);
    WriteParam(aMsg, aParam.mInitiallyPaused);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mUseAPZ) &&
           ReadParam(aMsg, aIter, &aResult->mUseSoftwareWebRender) &&
           ReadParam(aMsg, aIter, &aResult->mAllowSoftwareWebRenderD3D11) &&
           ReadParam(aMsg, aIter, &aResult->mAllowSoftwareWebRenderOGL) &&
           ReadParam(aMsg, aIter, &aResult->mUseAdvancedLayers) &&
           ReadParam(aMsg, aIter, &aResult->mInitiallyPaused);
  }
};

template <>
struct ParamTraits<mozilla::layers::ScrollbarLayerType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::ScrollbarLayerType,
          mozilla::layers::ScrollbarLayerType::None,
          mozilla::layers::kHighestScrollbarLayerType> {};

template <>
struct ParamTraits<mozilla::layers::ScrollbarData> {
  typedef mozilla::layers::ScrollbarData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mDirection);
    WriteParam(aMsg, aParam.mScrollbarLayerType);
    WriteParam(aMsg, aParam.mThumbRatio);
    WriteParam(aMsg, aParam.mThumbStart);
    WriteParam(aMsg, aParam.mThumbLength);
    WriteParam(aMsg, aParam.mThumbIsAsyncDraggable);
    WriteParam(aMsg, aParam.mScrollTrackStart);
    WriteParam(aMsg, aParam.mScrollTrackLength);
    WriteParam(aMsg, aParam.mTargetViewId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mDirection) &&
           ReadParam(aMsg, aIter, &aResult->mScrollbarLayerType) &&
           ReadParam(aMsg, aIter, &aResult->mThumbRatio) &&
           ReadParam(aMsg, aIter, &aResult->mThumbStart) &&
           ReadParam(aMsg, aIter, &aResult->mThumbLength) &&
           ReadParam(aMsg, aIter, &aResult->mThumbIsAsyncDraggable) &&
           ReadParam(aMsg, aIter, &aResult->mScrollTrackStart) &&
           ReadParam(aMsg, aIter, &aResult->mScrollTrackLength) &&
           ReadParam(aMsg, aIter, &aResult->mTargetViewId);
  }
};

template <>
struct ParamTraits<mozilla::layers::SimpleLayerAttributes::FixedPositionData>
    : public PlainOldDataSerializer<
          mozilla::layers::SimpleLayerAttributes::FixedPositionData> {};

template <>
struct ParamTraits<mozilla::layers::SimpleLayerAttributes::StickyPositionData>
    : public PlainOldDataSerializer<
          mozilla::layers::SimpleLayerAttributes::StickyPositionData> {};

template <>
struct ParamTraits<mozilla::layers::SimpleLayerAttributes> {
  typedef mozilla::layers::SimpleLayerAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mTransform);
    WriteParam(aMsg, aParam.mTransformIsPerspective);
    WriteParam(aMsg, aParam.mScrolledClip);
    WriteParam(aMsg, aParam.mPostXScale);
    WriteParam(aMsg, aParam.mPostYScale);
    WriteParam(aMsg, aParam.mContentFlags);
    WriteParam(aMsg, aParam.mOpacity);
    WriteParam(aMsg, aParam.mIsFixedPosition);
    WriteParam(aMsg, aParam.mAsyncZoomContainerId);
    WriteParam(aMsg, aParam.mScrollbarData);
    WriteParam(aMsg, aParam.mMixBlendMode);
    WriteParam(aMsg, aParam.mForceIsolatedGroup);
    WriteParam(aMsg, aParam.mFixedPositionData);
    WriteParam(aMsg, aParam.mStickyPositionData);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mTransform) &&
           ReadParam(aMsg, aIter, &aResult->mTransformIsPerspective) &&
           ReadParam(aMsg, aIter, &aResult->mScrolledClip) &&
           ReadParam(aMsg, aIter, &aResult->mPostXScale) &&
           ReadParam(aMsg, aIter, &aResult->mPostYScale) &&
           ReadParam(aMsg, aIter, &aResult->mContentFlags) &&
           ReadParam(aMsg, aIter, &aResult->mOpacity) &&
           ReadParam(aMsg, aIter, &aResult->mIsFixedPosition) &&
           ReadParam(aMsg, aIter, &aResult->mAsyncZoomContainerId) &&
           ReadParam(aMsg, aIter, &aResult->mScrollbarData) &&
           ReadParam(aMsg, aIter, &aResult->mMixBlendMode) &&
           ReadParam(aMsg, aIter, &aResult->mForceIsolatedGroup) &&
           ReadParam(aMsg, aIter, &aResult->mFixedPositionData) &&
           ReadParam(aMsg, aIter, &aResult->mStickyPositionData);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositionPayloadType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::layers::CompositionPayloadType,
          mozilla::layers::CompositionPayloadType::eKeyPress,
          mozilla::layers::kHighestCompositionPayloadType> {};

template <>
struct ParamTraits<mozilla::layers::CompositionPayload> {
  typedef mozilla::layers::CompositionPayload paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mTimeStamp);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mTimeStamp);
  }
};

template <>
struct ParamTraits<mozilla::RayReferenceData> {
  typedef mozilla::RayReferenceData paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mInitialPosition);
    WriteParam(aMsg, aParam.mContainingBlockRect);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mInitialPosition) &&
            ReadParam(aMsg, aIter, &aResult->mContainingBlockRect));
  }
};

template <>
struct ParamTraits<mozilla::layers::ZoomTarget> {
  typedef mozilla::layers::ZoomTarget paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.targetRect);
    WriteParam(aMsg, aParam.elementBoundingRect);
    WriteParam(aMsg, aParam.documentRelativePointerPosition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->targetRect) &&
            ReadParam(aMsg, aIter, &aResult->elementBoundingRect) &&
            ReadParam(aMsg, aIter, &aResult->documentRelativePointerPosition));
  }
};

#define IMPL_PARAMTRAITS_BY_SERDE(type_)                                    \
  template <>                                                               \
  struct ParamTraits<mozilla::type_> {                                      \
    typedef mozilla::type_ paramType;                                       \
    static void Write(Message* aMsg, const paramType& aParam) {             \
      mozilla::ipc::ByteBuf v;                                              \
      mozilla::DebugOnly<bool> rv = Servo_##type_##_Serialize(&aParam, &v); \
      MOZ_ASSERT(rv, "Serialize ##type_## failed");                         \
      WriteParam(aMsg, std::move(v));                                       \
    }                                                                       \
    static bool Read(const Message* aMsg, PickleIterator* aIter,            \
                     paramType* aResult) {                                  \
      mozilla::ipc::ByteBuf in;                                             \
      bool rv = ReadParam(aMsg, aIter, &in);                                \
      if (!rv) {                                                            \
        return false;                                                       \
      }                                                                     \
      return in.mData && Servo_##type_##_Deserialize(&in, aResult);         \
    }                                                                       \
  };

IMPL_PARAMTRAITS_BY_SERDE(LengthPercentage)
IMPL_PARAMTRAITS_BY_SERDE(StyleOffsetPath)
IMPL_PARAMTRAITS_BY_SERDE(StyleOffsetRotate)
IMPL_PARAMTRAITS_BY_SERDE(StylePositionOrAuto)
IMPL_PARAMTRAITS_BY_SERDE(StyleRotate)
IMPL_PARAMTRAITS_BY_SERDE(StyleScale)
IMPL_PARAMTRAITS_BY_SERDE(StyleTranslate)
IMPL_PARAMTRAITS_BY_SERDE(StyleTransform)

} /* namespace IPC */

#endif /* mozilla_layers_LayersMessageUtils */
