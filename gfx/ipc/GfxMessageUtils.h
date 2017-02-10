/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GFXMESSAGEUTILS_H__
#define __GFXMESSAGEUTILS_H__

#include "FilterSupport.h"
#include "FrameMetrics.h"
#include "ImageTypes.h"
#include "RegionBuilder.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTelemetry.h"
#include "gfxTypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/AsyncDragMetrics.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "mozilla/Array.h"

#include <stdint.h>

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

namespace mozilla {

typedef gfxImageFormat PixelFormat;

} // namespace mozilla

namespace IPC {

template<>
struct ParamTraits<mozilla::gfx::Matrix>
{
  typedef mozilla::gfx::Matrix paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam._11);
    WriteParam(aMsg, aParam._12);
    WriteParam(aMsg, aParam._21);
    WriteParam(aMsg, aParam._22);
    WriteParam(aMsg, aParam._31);
    WriteParam(aMsg, aParam._32);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->_11) &&
        ReadParam(aMsg, aIter, &aResult->_12) &&
        ReadParam(aMsg, aIter, &aResult->_21) &&
        ReadParam(aMsg, aIter, &aResult->_22) &&
        ReadParam(aMsg, aIter, &aResult->_31) &&
        ReadParam(aMsg, aIter, &aResult->_32))
      return true;

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[[%g %g] [%g %g] [%g %g]]", aParam._11, aParam._12, aParam._21, aParam._22,
                                                            aParam._31, aParam._32));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Matrix4x4>
{
  typedef mozilla::gfx::Matrix4x4 paramType;

  static void Write(Message* msg, const paramType& param)
  {
#define Wr(_f)  WriteParam(msg, param. _f)
    Wr(_11); Wr(_12); Wr(_13); Wr(_14);
    Wr(_21); Wr(_22); Wr(_23); Wr(_24);
    Wr(_31); Wr(_32); Wr(_33); Wr(_34);
    Wr(_41); Wr(_42); Wr(_43); Wr(_44);
#undef Wr
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
#define Rd(_f)  ReadParam(msg, iter, &result-> _f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) &&
            Rd(_21) && Rd(_22) && Rd(_23) && Rd(_24) &&
            Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template<>
struct ParamTraits<mozilla::gfx::Matrix5x4>
{
  typedef mozilla::gfx::Matrix5x4 paramType;

  static void Write(Message* msg, const paramType& param)
  {
#define Wr(_f)  WriteParam(msg, param. _f)
    Wr(_11); Wr(_12); Wr(_13); Wr(_14);
    Wr(_21); Wr(_22); Wr(_23); Wr(_24);
    Wr(_31); Wr(_32); Wr(_33); Wr(_34);
    Wr(_41); Wr(_42); Wr(_43); Wr(_44);
    Wr(_51); Wr(_52); Wr(_53); Wr(_54);
#undef Wr
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
#define Rd(_f)  ReadParam(msg, iter, &result-> _f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) &&
            Rd(_21) && Rd(_22) && Rd(_23) && Rd(_24) &&
            Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44) &&
            Rd(_51) && Rd(_52) && Rd(_53) && Rd(_54));
#undef Rd
  }
};

template<>
struct ParamTraits<gfxPoint>
{
  typedef gfxPoint paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y));
 }
};

template<>
struct ParamTraits<gfxSize>
{
  typedef gfxSize paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->width) &&
        ReadParam(aMsg, aIter, &aResult->height))
      return true;

    return false;
  }
};

template<>
struct ParamTraits<gfxRect>
{
  typedef gfxRect paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x) &&
           ReadParam(aMsg, aIter, &aResult->y) &&
           ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height);
  }
};

template <>
struct ParamTraits<gfxContentType>
  : public ContiguousEnumSerializer<
             gfxContentType,
             gfxContentType::COLOR,
             gfxContentType::SENTINEL>
{};

template <>
struct ParamTraits<gfxSurfaceType>
  : public ContiguousEnumSerializer<
             gfxSurfaceType,
             gfxSurfaceType::Image,
             gfxSurfaceType::Max>
{};

template <>
struct ParamTraits<mozilla::gfx::SamplingFilter>
  : public ContiguousEnumSerializer<
             mozilla::gfx::SamplingFilter,
             mozilla::gfx::SamplingFilter::GOOD,
             mozilla::gfx::SamplingFilter::SENTINEL>
{};

template <>
struct ParamTraits<mozilla::layers::LayersBackend>
  : public ContiguousEnumSerializer<
             mozilla::layers::LayersBackend,
             mozilla::layers::LayersBackend::LAYERS_NONE,
             mozilla::layers::LayersBackend::LAYERS_LAST>
{};

template <>
struct ParamTraits<mozilla::gfx::BackendType>
  : public ContiguousEnumSerializer<
             mozilla::gfx::BackendType,
             mozilla::gfx::BackendType::NONE,
             mozilla::gfx::BackendType::BACKEND_LAST>
{};

template <>
struct ParamTraits<mozilla::gfx::FeatureStatus>
  : public ContiguousEnumSerializer<
             mozilla::gfx::FeatureStatus,
             mozilla::gfx::FeatureStatus::Unused,
             mozilla::gfx::FeatureStatus::LAST>
{};

template <>
struct ParamTraits<mozilla::layers::ScaleMode>
  : public ContiguousEnumSerializer<
             mozilla::layers::ScaleMode,
             mozilla::layers::ScaleMode::SCALE_NONE,
             mozilla::layers::ScaleMode::SENTINEL>
{};

template <>
struct ParamTraits<mozilla::gfx::AttributeName>
  : public ContiguousEnumSerializer<
             mozilla::gfx::AttributeName,
             mozilla::gfx::eBlendBlendmode,
             mozilla::gfx::eLastAttributeName>
{};

template <>
struct ParamTraits<mozilla::gfx::AttributeType>
  : public ContiguousEnumSerializer<
             mozilla::gfx::AttributeType,
             mozilla::gfx::AttributeType::eBool,
             mozilla::gfx::AttributeType::Max>
{};

template <>
struct ParamTraits<mozilla::gfx::PrimitiveType>
  : public ContiguousEnumSerializer<
             mozilla::gfx::PrimitiveType,
             mozilla::gfx::PrimitiveType::Empty,
             mozilla::gfx::PrimitiveType::Max>
{};

template <>
struct ParamTraits<mozilla::gfx::ColorSpace>
  : public ContiguousEnumSerializer<
             mozilla::gfx::ColorSpace,
             mozilla::gfx::ColorSpace::SRGB,
             mozilla::gfx::ColorSpace::Max>
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

/*
template <>
struct ParamTraits<mozilla::PixelFormat>
  : public EnumSerializer<mozilla::PixelFormat,
                          SurfaceFormat::A8R8G8B8_UINT32,
                          SurfaceFormat::UNKNOWN>
{};
*/

template<>
struct ParamTraits<mozilla::gfx::Color>
{
  typedef mozilla::gfx::Color paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.r);
    WriteParam(msg, param.g);
    WriteParam(msg, param.b);
    WriteParam(msg, param.a);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->r) &&
            ReadParam(msg, iter, &result->g) &&
            ReadParam(msg, iter, &result->b) &&
            ReadParam(msg, iter, &result->a));
  }
};

template<>
struct ParamTraits<nsPoint>
{
  typedef nsPoint paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<nsIntPoint>
{
  typedef nsIntPoint paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<typename T>
struct ParamTraits<mozilla::gfx::IntSizeTyped<T> >
{
  typedef mozilla::gfx::IntSizeTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<typename Region, typename Rect, typename Iter>
struct RegionParamTraits
{
  typedef Region paramType;

  static void Write(Message* msg, const paramType& param)
  {

    for (auto iter = param.RectIter(); !iter.Done(); iter.Next()) {
      const Rect& r = iter.Get();
      MOZ_RELEASE_ASSERT(!r.IsEmpty(), "GFX: rect is empty.");
      WriteParam(msg, r);
    }
    // empty rects are sentinel values because nsRegions will never
    // contain them
    WriteParam(msg, Rect());
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    RegionBuilder<Region> builder;
    Rect rect;
    while (ReadParam(msg, iter, &rect)) {
      if (rect.IsEmpty()) {
        *result = builder.ToRegion();
        return true;
      }
      builder.OrWith(rect);
    }

    return false;
  }
};

template<class Units>
struct ParamTraits<mozilla::gfx::IntRegionTyped<Units>>
  : RegionParamTraits<mozilla::gfx::IntRegionTyped<Units>,
                      mozilla::gfx::IntRectTyped<Units>,
                      typename mozilla::gfx::IntRegionTyped<Units>::RectIterator>
{};

template<>
struct ParamTraits<mozilla::gfx::IntSize>
{
  typedef mozilla::gfx::IntSize paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::CoordTyped<T> >
{
  typedef mozilla::gfx::CoordTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.value);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->value));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::IntCoordTyped<T> >
{
  typedef mozilla::gfx::IntCoordTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.value);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->value));
  }
};

template<class T, class U>
struct ParamTraits< mozilla::gfx::ScaleFactor<T, U> >
{
  typedef mozilla::gfx::ScaleFactor<T, U> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.scale);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->scale));
  }
};

template<class T, class U>
struct ParamTraits< mozilla::gfx::ScaleFactors2D<T, U> >
{
  typedef mozilla::gfx::ScaleFactors2D<T, U> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.xScale);
    WriteParam(msg, param.yScale);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->xScale) &&
            ReadParam(msg, iter, &result->yScale));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::PointTyped<T> >
{
  typedef mozilla::gfx::PointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<class F, class T>
struct ParamTraits< mozilla::gfx::Point3DTyped<F, T> >
{
  typedef mozilla::gfx::Point3DTyped<F, T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.z);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->z));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::IntPointTyped<T> >
{
  typedef mozilla::gfx::IntPointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::SizeTyped<T> >
{
  typedef mozilla::gfx::SizeTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::RectTyped<T> >
{
  typedef mozilla::gfx::RectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::IntRectTyped<T> >
{
  typedef mozilla::gfx::IntRectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Margin>
{
  typedef mozilla::gfx::Margin paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template<class T>
struct ParamTraits< mozilla::gfx::MarginTyped<T> >
{
  typedef mozilla::gfx::MarginTyped<T> paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template<>
struct ParamTraits<nsRect>
{
  typedef nsRect paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsRegion>
  : RegionParamTraits<nsRegion, nsRect, nsRegion::RectIterator>
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
struct ParamTraits<GeckoProcessType>
  : public ContiguousEnumSerializer<
             GeckoProcessType,
             GeckoProcessType_Default,
             GeckoProcessType_End>
{};

template<>
struct ParamTraits<mozilla::layers::TextureFactoryIdentifier>
{
  typedef mozilla::layers::TextureFactoryIdentifier paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mParentBackend);
    WriteParam(aMsg, aParam.mParentProcessType);
    WriteParam(aMsg, aParam.mMaxTextureSize);
    WriteParam(aMsg, aParam.mSupportsTextureBlitting);
    WriteParam(aMsg, aParam.mSupportsPartialUploads);
    WriteParam(aMsg, aParam.mSupportsComponentAlpha);
    WriteParam(aMsg, aParam.mSyncHandle);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    bool result = ReadParam(aMsg, aIter, &aResult->mParentBackend) &&
                  ReadParam(aMsg, aIter, &aResult->mParentProcessType) &&
                  ReadParam(aMsg, aIter, &aResult->mMaxTextureSize) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsTextureBlitting) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsPartialUploads) &&
                  ReadParam(aMsg, aIter, &aResult->mSupportsComponentAlpha) &&
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
struct ParamTraits<mozilla::gfx::SurfaceFormat>
  : public ContiguousEnumSerializer<
             mozilla::gfx::SurfaceFormat,
             mozilla::gfx::SurfaceFormat::B8G8R8A8,
             mozilla::gfx::SurfaceFormat::UNKNOWN>
{};

template <>
struct ParamTraits<mozilla::StereoMode>
  : public ContiguousEnumSerializer<
             mozilla::StereoMode,
             mozilla::StereoMode::MONO,
             mozilla::StereoMode::MAX>
{};

template <>
struct ParamTraits<mozilla::YUVColorSpace>
  : public ContiguousEnumSerializer<
             mozilla::YUVColorSpace,
             mozilla::YUVColorSpace::BT601,
             mozilla::YUVColorSpace::UNKNOWN>
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
struct ParamTraits<mozilla::gfx::AttributeMap>
{
  typedef mozilla::gfx::AttributeMap paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.Count());
    for (auto iter = aParam.ConstIter(); !iter.Done(); iter.Next()) {
      mozilla::gfx::AttributeName name =
        mozilla::gfx::AttributeName(iter.Key());
      mozilla::gfx::AttributeType type =
        mozilla::gfx::AttributeMap::GetType(iter.UserData());

      WriteParam(aMsg, type);
      WriteParam(aMsg, name);

      switch (type) {

#define CASE_TYPE(typeName)                                          \
    case mozilla::gfx::AttributeType::e##typeName:                     \
      WriteParam(aMsg, aParam.Get##typeName(name)); \
      break;

    CASE_TYPE(Bool)
    CASE_TYPE(Uint)
    CASE_TYPE(Float)
    CASE_TYPE(Size)
    CASE_TYPE(IntSize)
    CASE_TYPE(IntPoint)
    CASE_TYPE(Matrix)
    CASE_TYPE(Matrix5x4)
    CASE_TYPE(Point3D)
    CASE_TYPE(Color)
    CASE_TYPE(AttributeMap)
    CASE_TYPE(Floats)

#undef CASE_TYPE

        default:
          MOZ_CRASH("GFX: unhandled attribute type");
      }
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    uint32_t count;
    if (!ReadParam(aMsg, aIter, &count)) {
      return false;
    }
    for (uint32_t i = 0; i < count; i++) {
      mozilla::gfx::AttributeType type;
      if (!ReadParam(aMsg, aIter, &type)) {
        return false;
      }
      mozilla::gfx::AttributeName name;
      if (!ReadParam(aMsg, aIter, &name)) {
        return false;
      }
      switch (type) {

#define HANDLE_TYPE(type, typeName)                                    \
        case mozilla::gfx::AttributeType::e##typeName:                 \
        {                                                              \
          type value;                                                  \
          if (!ReadParam(aMsg, aIter, &value)) {                       \
            return false;                                              \
          }                                                            \
          aResult->Set(name, value);                                   \
          break;                                                       \
        }

        HANDLE_TYPE(bool, Bool)
        HANDLE_TYPE(uint32_t, Uint)
        HANDLE_TYPE(float, Float)
        HANDLE_TYPE(mozilla::gfx::Size, Size)
        HANDLE_TYPE(mozilla::gfx::IntSize, IntSize)
        HANDLE_TYPE(mozilla::gfx::IntPoint, IntPoint)
        HANDLE_TYPE(mozilla::gfx::Matrix, Matrix)
        HANDLE_TYPE(mozilla::gfx::Matrix5x4, Matrix5x4)
        HANDLE_TYPE(mozilla::gfx::Point3D, Point3D)
        HANDLE_TYPE(mozilla::gfx::Color, Color)
        HANDLE_TYPE(mozilla::gfx::AttributeMap, AttributeMap)

#undef HANDLE_TYPE

        case mozilla::gfx::AttributeType::eFloats:
        {
          nsTArray<float> value;
          if (!ReadParam(aMsg, aIter, &value)) {
            return false;
          }
          aResult->Set(name, &value[0], value.Length());
          break;
        }
        default:
          MOZ_CRASH("GFX: unhandled attribute type");
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::FilterPrimitiveDescription>
{
  typedef mozilla::gfx::FilterPrimitiveDescription paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.Type());
    WriteParam(aMsg, aParam.PrimitiveSubregion());
    WriteParam(aMsg, aParam.FilterSpaceBounds());
    WriteParam(aMsg, aParam.IsTainted());
    WriteParam(aMsg, aParam.OutputColorSpace());
    WriteParam(aMsg, aParam.NumberOfInputs());
    for (size_t i = 0; i < aParam.NumberOfInputs(); i++) {
      WriteParam(aMsg, aParam.InputPrimitiveIndex(i));
      WriteParam(aMsg, aParam.InputColorSpace(i));
    }
    WriteParam(aMsg, aParam.Attributes());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    mozilla::gfx::PrimitiveType type;
    mozilla::gfx::IntRect primitiveSubregion;
    mozilla::gfx::IntRect filterSpaceBounds;
    bool isTainted = false;
    mozilla::gfx::ColorSpace outputColorSpace;
    size_t numberOfInputs = 0;
    if (!ReadParam(aMsg, aIter, &type) ||
        !ReadParam(aMsg, aIter, &primitiveSubregion) ||
        !ReadParam(aMsg, aIter, &filterSpaceBounds) ||
        !ReadParam(aMsg, aIter, &isTainted) ||
        !ReadParam(aMsg, aIter, &outputColorSpace) ||
        !ReadParam(aMsg, aIter, &numberOfInputs)) {
      return false;
    }

    aResult->SetType(type);
    aResult->SetPrimitiveSubregion(primitiveSubregion);
    aResult->SetFilterSpaceBounds(filterSpaceBounds);
    aResult->SetIsTainted(isTainted);
    aResult->SetOutputColorSpace(outputColorSpace);

    for (size_t i = 0; i < numberOfInputs; i++) {
      int32_t inputPrimitiveIndex = 0;
      mozilla::gfx::ColorSpace inputColorSpace;
      if (!ReadParam(aMsg, aIter, &inputPrimitiveIndex) ||
          !ReadParam(aMsg, aIter, &inputColorSpace)) {
        return false;
      }
      aResult->SetInputPrimitive(i, inputPrimitiveIndex);
      aResult->SetInputColorSpace(i, inputColorSpace);
    }

    return ReadParam(aMsg, aIter, &aResult->Attributes());
  }
};

template <>
struct ParamTraits<mozilla::gfx::FilterDescription>
{
  typedef mozilla::gfx::FilterDescription paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mPrimitives);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mPrimitives));
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
struct ParamTraits<mozilla::layers::AsyncDragMetrics::DragDirection>
  : public ContiguousEnumSerializer<
             mozilla::layers::AsyncDragMetrics::DragDirection,
             mozilla::layers::AsyncDragMetrics::DragDirection::NONE,
             mozilla::layers::AsyncDragMetrics::DragDirection::SENTINEL>
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
    WriteParam(aMsg, aParam.mScrollTrack);
    WriteParam(aMsg, aParam.mDirection);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mViewId) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId) &&
            ReadParam(aMsg, aIter, &aResult->mDragStartSequenceNumber) &&
            ReadParam(aMsg, aIter, &aResult->mScrollbarDragOffset) &&
            ReadParam(aMsg, aIter, &aResult->mScrollTrack) &&
            ReadParam(aMsg, aIter, &aResult->mDirection));
  }
};

template <>
struct ParamTraits<mozilla::gfx::Glyph>
{
  typedef mozilla::gfx::Glyph paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mIndex);
    WriteParam(aMsg, aParam.mPosition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mIndex) &&
            ReadParam(aMsg, aIter, &aResult->mPosition)
      );
  }
};

template<typename T, size_t Length>
struct ParamTraits<mozilla::Array<T, Length>>
{
  typedef mozilla::Array<T, Length> paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    for (size_t i = 0; i < Length; i++) {
      WriteParam(aMsg, aParam[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    for (size_t i = 0; i < Length; i++) {
      if (!ReadParam(aMsg, aIter, &aResult[i])) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositorOptions>
{
  typedef mozilla::layers::CompositorOptions paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mUseAPZ);
    WriteParam(aMsg, aParam.mUseWebRender);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mUseAPZ)
        && ReadParam(aMsg, aIter, &aResult->mUseWebRender);
  }
};

template <>
struct ParamTraits<mozilla::layers::SimpleLayerAttributes>
{
  typedef mozilla::layers::SimpleLayerAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    return aMsg->ReadBytesInto(aIter, aResult, sizeof(paramType));
  }
};

} /* namespace IPC */

#endif /* __GFXMESSAGEUTILS_H__ */
