/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_LayersMessageUtils
#define mozilla_layers_LayersMessageUtils

#include "FrameMetrics.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"
#include "gfxTelemetry.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/nsGUIEventIPC.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/Keyboard.h"
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Move.h"

#include <stdint.h>

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::LayersBackend>
  : public ContiguousEnumSerializer<
             mozilla::layers::LayersBackend,
             mozilla::layers::LayersBackend::LAYERS_NONE,
             mozilla::layers::LayersBackend::LAYERS_LAST>
{};

template <>
struct ParamTraits<mozilla::layers::ScaleMode>
  : public ContiguousEnumSerializer<
             mozilla::layers::ScaleMode,
             mozilla::layers::ScaleMode::SCALE_NONE,
             mozilla::layers::ScaleMode::SENTINEL>
{};

template <>
struct ParamTraits<mozilla::layers::TextureFlags>
  : public BitFlagsEnumSerializer<
            mozilla::layers::TextureFlags,
            mozilla::layers::TextureFlags::ALL_BITS>
{};

template <>
struct ParamTraits<mozilla::layers::DiagnosticTypes>
  : public BitFlagsEnumSerializer<
             mozilla::layers::DiagnosticTypes,
             mozilla::layers::DiagnosticTypes::ALL_BITS>
{};

template <>
struct ParamTraits<mozilla::layers::ScrollDirection>
  : public ContiguousEnumSerializer<
            mozilla::layers::ScrollDirection,
            mozilla::layers::ScrollDirection::NONE,
            mozilla::layers::ScrollDirection::SENTINEL>
{};

template<>
struct ParamTraits<mozilla::layers::FrameMetrics::ScrollOffsetUpdateType>
  : public ContiguousEnumSerializer<
             mozilla::layers::FrameMetrics::ScrollOffsetUpdateType,
             mozilla::layers::FrameMetrics::ScrollOffsetUpdateType::eNone,
             mozilla::layers::FrameMetrics::ScrollOffsetUpdateType::eSentinel>
{};

template<>
struct ParamTraits<mozilla::layers::LayerHandle>
{
  typedef mozilla::layers::LayerHandle paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mHandle);
  }
  static bool Read(const Message* msg, PickleIterator* iter, paramType* result) {
    return ReadParam(msg, iter, &result->mHandle);
  }
};

template<>
struct ParamTraits<mozilla::layers::CompositableHandle>
{
  typedef mozilla::layers::CompositableHandle paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mHandle);
  }
  static bool Read(const Message* msg, PickleIterator* iter, paramType* result) {
    return ReadParam(msg, iter, &result->mHandle);
  }
};

template<>
struct ParamTraits<mozilla::layers::ReadLockHandle>
{
  typedef mozilla::layers::ReadLockHandle paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.mHandle);
  }
  static bool Read(const Message* msg, PickleIterator* iter, paramType* result) {
    return ReadParam(msg, iter, &result->mHandle);
  }
};

// Helper class for reading bitfields.
// If T has bitfields members, derive ParamTraits<T> from BitfieldHelper<T>.
template <typename ParamType>
struct BitfieldHelper
{
  // We need this helper because we can't get the address of a bitfield to
  // pass directly to ReadParam. So instead we read it into a temporary bool
  // and set the bitfield using a setter function
  static bool ReadBoolForBitfield(const Message* aMsg, PickleIterator* aIter,
        ParamType* aResult, void (ParamType::*aSetter)(bool))
  {
    bool value;
    if (ReadParam(aMsg, aIter, &value)) {
      (aResult->*aSetter)(value);
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<mozilla::layers::FrameMetrics>
    : BitfieldHelper<mozilla::layers::FrameMetrics>
{
  typedef mozilla::layers::FrameMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mPresShellResolution);
    WriteParam(aMsg, aParam.mCompositionBounds);
    WriteParam(aMsg, aParam.mDisplayPort);
    WriteParam(aMsg, aParam.mCriticalDisplayPort);
    WriteParam(aMsg, aParam.mScrollableRect);
    WriteParam(aMsg, aParam.mCumulativeResolution);
    WriteParam(aMsg, aParam.mDevPixelsPerCSSPixel);
    WriteParam(aMsg, aParam.mScrollOffset);
    WriteParam(aMsg, aParam.mZoom);
    WriteParam(aMsg, aParam.mScrollGeneration);
    WriteParam(aMsg, aParam.mSmoothScrollOffset);
    WriteParam(aMsg, aParam.mRootCompositionSize);
    WriteParam(aMsg, aParam.mDisplayPortMargins);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mViewport);
    WriteParam(aMsg, aParam.mExtraResolution);
    WriteParam(aMsg, aParam.mPaintRequestTime);
    WriteParam(aMsg, aParam.mScrollUpdateType);
    WriteParam(aMsg, aParam.mIsRootContent);
    WriteParam(aMsg, aParam.mDoSmoothScroll);
    WriteParam(aMsg, aParam.mUseDisplayPortMargins);
    WriteParam(aMsg, aParam.mIsScrollInfoLayer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellResolution) &&
            ReadParam(aMsg, aIter, &aResult->mCompositionBounds) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCriticalDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mScrollableRect) &&
            ReadParam(aMsg, aIter, &aResult->mCumulativeResolution) &&
            ReadParam(aMsg, aIter, &aResult->mDevPixelsPerCSSPixel) &&
            ReadParam(aMsg, aIter, &aResult->mScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mZoom) &&
            ReadParam(aMsg, aIter, &aResult->mScrollGeneration) &&
            ReadParam(aMsg, aIter, &aResult->mSmoothScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mRootCompositionSize) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPortMargins) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mViewport) &&
            ReadParam(aMsg, aIter, &aResult->mExtraResolution) &&
            ReadParam(aMsg, aIter, &aResult->mPaintRequestTime) &&
            ReadParam(aMsg, aIter, &aResult->mScrollUpdateType) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetIsRootContent) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetDoSmoothScroll) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetUseDisplayPortMargins) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetIsScrollInfoLayer));
  }
};

template <>
struct ParamTraits<mozilla::layers::ScrollSnapInfo>
{
  typedef mozilla::layers::ScrollSnapInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mScrollSnapTypeX);
    WriteParam(aMsg, aParam.mScrollSnapTypeY);
    WriteParam(aMsg, aParam.mScrollSnapIntervalX);
    WriteParam(aMsg, aParam.mScrollSnapIntervalY);
    WriteParam(aMsg, aParam.mScrollSnapDestination);
    WriteParam(aMsg, aParam.mScrollSnapCoordinates);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mScrollSnapTypeX) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapTypeY) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapIntervalX) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapIntervalY) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapDestination) &&
            ReadParam(aMsg, aIter, &aResult->mScrollSnapCoordinates));
  }
};

template <>
struct ParamTraits<mozilla::layers::LayerClip>
{
  typedef mozilla::layers::LayerClip paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mClipRect);
    WriteParam(aMsg, aParam.mMaskLayerIndex);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mClipRect) &&
            ReadParam(aMsg, aIter, &aResult->mMaskLayerIndex));
  }
};

template <>
struct ParamTraits<mozilla::layers::ScrollMetadata>
    : BitfieldHelper<mozilla::layers::ScrollMetadata>
{
  typedef mozilla::layers::ScrollMetadata paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mMetrics);
    WriteParam(aMsg, aParam.mSnapInfo);
    WriteParam(aMsg, aParam.mScrollParentId);
    WriteParam(aMsg, aParam.mBackgroundColor);
    WriteParam(aMsg, aParam.GetContentDescription());
    WriteParam(aMsg, aParam.mLineScrollAmount);
    WriteParam(aMsg, aParam.mPageScrollAmount);
    WriteParam(aMsg, aParam.mScrollClip);
    WriteParam(aMsg, aParam.mHasScrollgrab);
    WriteParam(aMsg, aParam.mAllowVerticalScrollWithWheel);
    WriteParam(aMsg, aParam.mIsLayersIdRoot);
    WriteParam(aMsg, aParam.mUsesContainerScrolling);
    WriteParam(aMsg, aParam.mForceDisableApz);
  }

  static bool ReadContentDescription(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    nsCString str;
    if (!ReadParam(aMsg, aIter, &str)) {
      return false;
    }
    aResult->SetContentDescription(str);
    return true;
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mMetrics) &&
            ReadParam(aMsg, aIter, &aResult->mSnapInfo) &&
            ReadParam(aMsg, aIter, &aResult->mScrollParentId) &&
            ReadParam(aMsg, aIter, &aResult->mBackgroundColor) &&
            ReadContentDescription(aMsg, aIter, aResult) &&
            ReadParam(aMsg, aIter, &aResult->mLineScrollAmount) &&
            ReadParam(aMsg, aIter, &aResult->mPageScrollAmount) &&
            ReadParam(aMsg, aIter, &aResult->mScrollClip) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetHasScrollgrab) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetAllowVerticalScrollWithWheel) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetIsLayersIdRoot) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetUsesContainerScrolling) &&
            ReadBoolForBitfield(aMsg, aIter, aResult, &paramType::SetForceDisableApz));
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureFactoryIdentifier>
{
  typedef mozilla::layers::TextureFactoryIdentifier paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mParentBackend);
    WriteParam(aMsg, aParam.mParentProcessType);
    WriteParam(aMsg, aParam.mMaxTextureSize);
    WriteParam(aMsg, aParam.mCompositorUseANGLE);
    WriteParam(aMsg, aParam.mSupportsTextureBlitting);
    WriteParam(aMsg, aParam.mSupportsPartialUploads);
    WriteParam(aMsg, aParam.mSupportsComponentAlpha);
    WriteParam(aMsg, aParam.mSupportsBackdropCopyForComponentAlpha);
    WriteParam(aMsg, aParam.mUsingAdvancedLayers);
    WriteParam(aMsg, aParam.mSyncHandle);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool result = ReadParam(aMsg, aIter, &aResult->mParentBackend) &&
                  ReadParam(aMsg, aIter, &aResult->mParentProcessType) &&
                  ReadParam(aMsg, aIter, &aResult->mMaxTextureSize) &&
                  ReadParam(aMsg, aIter, &aResult->mCompositorUseANGLE) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsTextureBlitting) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsPartialUploads) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsComponentAlpha) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsBackdropCopyForComponentAlpha) &&
                  ReadParam(aMsg, aIter, &aResult->mUsingAdvancedLayers) &&
                  ReadParam(aMsg, aIter, &aResult->mSyncHandle);
    return result;
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureInfo>
{
  typedef mozilla::layers::TextureInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCompositableType);
    WriteParam(aMsg, aParam.mTextureFlags);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mCompositableType) &&
           ReadParam(aMsg, aIter, &aResult->mTextureFlags);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositableType>
  : public ContiguousEnumSerializer<
             mozilla::layers::CompositableType,
             mozilla::layers::CompositableType::UNKNOWN,
             mozilla::layers::CompositableType::COUNT>
{};

template <>
struct ParamTraits<mozilla::layers::ScrollableLayerGuid>
{
  typedef mozilla::layers::ScrollableLayerGuid paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mLayersId);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mScrollId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mLayersId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId));
  }
};


template <>
struct ParamTraits<mozilla::layers::ZoomConstraints>
{
  typedef mozilla::layers::ZoomConstraints paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mAllowZoom);
    WriteParam(aMsg, aParam.mAllowDoubleTapZoom);
    WriteParam(aMsg, aParam.mMinZoom);
    WriteParam(aMsg, aParam.mMaxZoom);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mAllowZoom) &&
            ReadParam(aMsg, aIter, &aResult->mAllowDoubleTapZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMinZoom) &&
            ReadParam(aMsg, aIter, &aResult->mMaxZoom));
  }
};

template <>
struct ParamTraits<mozilla::layers::EventRegions>
{
  typedef mozilla::layers::EventRegions paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHitRegion);
    WriteParam(aMsg, aParam.mDispatchToContentHitRegion);
    WriteParam(aMsg, aParam.mNoActionRegion);
    WriteParam(aMsg, aParam.mHorizontalPanRegion);
    WriteParam(aMsg, aParam.mVerticalPanRegion);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mHitRegion) &&
            ReadParam(aMsg, aIter, &aResult->mDispatchToContentHitRegion) &&
            ReadParam(aMsg, aIter, &aResult->mNoActionRegion) &&
            ReadParam(aMsg, aIter, &aResult->mHorizontalPanRegion) &&
            ReadParam(aMsg, aIter, &aResult->mVerticalPanRegion));
  }
};

template <>
struct ParamTraits<mozilla::layers::FocusTarget::ScrollTargets>
{
  typedef mozilla::layers::FocusTarget::ScrollTargets paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHorizontal);
    WriteParam(aMsg, aParam.mVertical);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mHorizontal) &&
           ReadParam(aMsg, aIter, &aResult->mVertical);
  }
};

template <>
struct ParamTraits<mozilla::layers::FocusTarget::FocusTargetType>
  : public ContiguousEnumSerializer<
             mozilla::layers::FocusTarget::FocusTargetType,
             mozilla::layers::FocusTarget::eNone,
             mozilla::layers::FocusTarget::eSentinel>
{};

template <>
struct ParamTraits<mozilla::layers::FocusTarget>
{
  typedef mozilla::layers::FocusTarget paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mFocusHasKeyEventListeners);
    WriteParam(aMsg, aParam.mType);
    if (aParam.mType == mozilla::layers::FocusTarget::eRefLayer) {
      WriteParam(aMsg, aParam.mData.mRefLayerId);
    } else if (aParam.mType == mozilla::layers::FocusTarget::eScrollLayer) {
      WriteParam(aMsg, aParam.mData.mScrollTargets);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mFocusHasKeyEventListeners) ||
        !ReadParam(aMsg, aIter, &aResult->mType)) {
      return false;
    }

    if (aResult->mType == mozilla::layers::FocusTarget::eRefLayer) {
      return ReadParam(aMsg, aIter, &aResult->mData.mRefLayerId);
    } else if (aResult->mType == mozilla::layers::FocusTarget::eScrollLayer) {
      return ReadParam(aMsg, aIter, &aResult->mData.mScrollTargets);
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType>
  : public ContiguousEnumSerializer<
             mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType,
             mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType::eScrollCharacter,
             mozilla::layers::KeyboardScrollAction::KeyboardScrollActionType::eSentinel>
{};

template <>
struct ParamTraits<mozilla::layers::KeyboardScrollAction>
{
  typedef mozilla::layers::KeyboardScrollAction paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mForward);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mForward);
  }
};

template <>
struct ParamTraits<mozilla::layers::KeyboardShortcut>
{
  typedef mozilla::layers::KeyboardShortcut paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mAction);
    WriteParam(aMsg, aParam.mKeyCode);
    WriteParam(aMsg, aParam.mCharCode);
    WriteParam(aMsg, aParam.mModifiers);
    WriteParam(aMsg, aParam.mModifiersMask);
    WriteParam(aMsg, aParam.mEventType);
    WriteParam(aMsg, aParam.mDispatchToContent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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
struct ParamTraits<mozilla::layers::KeyboardMap>
{
  typedef mozilla::layers::KeyboardMap paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.Shortcuts());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    nsTArray<mozilla::layers::KeyboardShortcut> shortcuts;
    if (!ReadParam(aMsg, aIter, &shortcuts)) {
      return false;
    }
    *aResult = mozilla::layers::KeyboardMap(mozilla::Move(shortcuts));
    return true;
  }
};

typedef mozilla::layers::GeckoContentController::TapType TapType;

template <>
struct ParamTraits<TapType>
  : public ContiguousEnumSerializer<
             TapType,
             TapType::eSingleTap,
             TapType::eSentinel>
{};

typedef mozilla::layers::GeckoContentController::APZStateChange APZStateChange;

template <>
struct ParamTraits<APZStateChange>
  : public ContiguousEnumSerializer<
             APZStateChange,
             APZStateChange::eTransformBegin,
             APZStateChange::eSentinel>
{};

template<>
struct ParamTraits<mozilla::layers::EventRegionsOverride>
  : public BitFlagsEnumSerializer<
            mozilla::layers::EventRegionsOverride,
            mozilla::layers::EventRegionsOverride::ALL_BITS>
{};

template<>
struct ParamTraits<mozilla::layers::AsyncDragMetrics>
{
  typedef mozilla::layers::AsyncDragMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mViewId);
    WriteParam(aMsg, aParam.mPresShellId);
    WriteParam(aMsg, aParam.mDragStartSequenceNumber);
    WriteParam(aMsg, aParam.mScrollbarDragOffset);
    WriteParam(aMsg, aParam.mDirection);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mViewId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mDragStartSequenceNumber) &&
            ReadParam(aMsg, aIter, &aResult->mScrollbarDragOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDirection));
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositorOptions>
{
  typedef mozilla::layers::CompositorOptions paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mUseAPZ);
    WriteParam(aMsg, aParam.mUseWebRender);
    WriteParam(aMsg, aParam.mUseAdvancedLayers);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mUseAPZ)
        && ReadParam(aMsg, aIter, &aResult->mUseWebRender)
        && ReadParam(aMsg, aIter, &aResult->mUseAdvancedLayers);
  }
};

template <>
struct ParamTraits<mozilla::layers::SimpleLayerAttributes>
  : public PlainOldDataSerializer<mozilla::layers::SimpleLayerAttributes>
{ };

} /* namespace IPC */

#endif /* mozilla_layers_LayersMessageUtils */
