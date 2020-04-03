/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GFXMESSAGEUTILS_H__
#define __GFXMESSAGEUTILS_H__

#include "FilterSupport.h"
#include "ImageTypes.h"
#include "RegionBuilder.h"
#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"
#include "gfxFeature.h"
#include "gfxFallback.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTelemetry.h"
#include "gfxTypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/gfx/Matrix.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "mozilla/Array.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Shmem.h"

#include <stdint.h>

#ifdef _MSC_VER
#  pragma warning(disable : 4800)
#endif

namespace mozilla {

typedef gfxImageFormat PixelFormat;

}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::gfx::Matrix> {
  typedef mozilla::gfx::Matrix paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam._11);
    WriteParam(aMsg, aParam._12);
    WriteParam(aMsg, aParam._21);
    WriteParam(aMsg, aParam._22);
    WriteParam(aMsg, aParam._31);
    WriteParam(aMsg, aParam._32);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (ReadParam(aMsg, aIter, &aResult->_11) &&
        ReadParam(aMsg, aIter, &aResult->_12) &&
        ReadParam(aMsg, aIter, &aResult->_21) &&
        ReadParam(aMsg, aIter, &aResult->_22) &&
        ReadParam(aMsg, aIter, &aResult->_31) &&
        ReadParam(aMsg, aIter, &aResult->_32))
      return true;

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    aLog->append(StringPrintf(L"[[%g %g] [%g %g] [%g %g]]", aParam._11,
                              aParam._12, aParam._21, aParam._22, aParam._31,
                              aParam._32));
  }
};

template <>
struct ParamTraits<mozilla::gfx::Matrix4x4> {
  typedef mozilla::gfx::Matrix4x4 paramType;

  static void Write(Message* msg, const paramType& param) {
#define Wr(_f) WriteParam(msg, param._f)
    Wr(_11);
    Wr(_12);
    Wr(_13);
    Wr(_14);
    Wr(_21);
    Wr(_22);
    Wr(_23);
    Wr(_24);
    Wr(_31);
    Wr(_32);
    Wr(_33);
    Wr(_34);
    Wr(_41);
    Wr(_42);
    Wr(_43);
    Wr(_44);
#undef Wr
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
#define Rd(_f) ReadParam(msg, iter, &result->_f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) && Rd(_21) && Rd(_22) &&
            Rd(_23) && Rd(_24) && Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template <>
struct ParamTraits<mozilla::gfx::Matrix5x4> {
  typedef mozilla::gfx::Matrix5x4 paramType;

  static void Write(Message* msg, const paramType& param) {
#define Wr(_f) WriteParam(msg, param._f)
    Wr(_11);
    Wr(_12);
    Wr(_13);
    Wr(_14);
    Wr(_21);
    Wr(_22);
    Wr(_23);
    Wr(_24);
    Wr(_31);
    Wr(_32);
    Wr(_33);
    Wr(_34);
    Wr(_41);
    Wr(_42);
    Wr(_43);
    Wr(_44);
    Wr(_51);
    Wr(_52);
    Wr(_53);
    Wr(_54);
#undef Wr
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
#define Rd(_f) ReadParam(msg, iter, &result->_f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) && Rd(_21) && Rd(_22) &&
            Rd(_23) && Rd(_24) && Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44) && Rd(_51) && Rd(_52) &&
            Rd(_53) && Rd(_54));
#undef Rd
  }
};

template <>
struct ParamTraits<gfxPoint> {
  typedef gfxPoint paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y));
  }
};

template <>
struct ParamTraits<gfxSize> {
  typedef gfxSize paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (ReadParam(aMsg, aIter, &aResult->width) &&
        ReadParam(aMsg, aIter, &aResult->height))
      return true;

    return false;
  }
};

template <>
struct ParamTraits<gfxRect> {
  typedef gfxRect paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.X());
    WriteParam(aMsg, aParam.Y());
    WriteParam(aMsg, aParam.Width());
    WriteParam(aMsg, aParam.Height());
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    auto x = aResult->X();
    auto y = aResult->Y();
    auto w = aResult->Width();
    auto h = aResult->Height();

    bool retVal = (ReadParam(aMsg, aIter, &x) && ReadParam(aMsg, aIter, &y) &&
                   ReadParam(aMsg, aIter, &w) && ReadParam(aMsg, aIter, &h));
    aResult->SetRect(x, y, w, h);
    return retVal;
  }
};

template <>
struct ParamTraits<gfxContentType>
    : public ContiguousEnumSerializer<gfxContentType, gfxContentType::COLOR,
                                      gfxContentType::SENTINEL> {};

template <>
struct ParamTraits<gfxSurfaceType>
    : public ContiguousEnumSerializer<gfxSurfaceType, gfxSurfaceType::Image,
                                      gfxSurfaceType::Max> {};

template <>
struct ParamTraits<mozilla::gfx::SamplingFilter>
    : public ContiguousEnumSerializer<mozilla::gfx::SamplingFilter,
                                      mozilla::gfx::SamplingFilter::GOOD,
                                      mozilla::gfx::SamplingFilter::SENTINEL> {
};

template <>
struct ParamTraits<mozilla::gfx::BackendType>
    : public ContiguousEnumSerializer<mozilla::gfx::BackendType,
                                      mozilla::gfx::BackendType::NONE,
                                      mozilla::gfx::BackendType::BACKEND_LAST> {
};

template <>
struct ParamTraits<mozilla::gfx::Feature>
    : public ContiguousEnumSerializer<mozilla::gfx::Feature,
                                      mozilla::gfx::Feature::HW_COMPOSITING,
                                      mozilla::gfx::Feature::NumValues> {};

template <>
struct ParamTraits<mozilla::gfx::Fallback>
    : public ContiguousEnumSerializer<
          mozilla::gfx::Fallback,
          mozilla::gfx::Fallback::NO_CONSTANT_BUFFER_OFFSETTING,
          mozilla::gfx::Fallback::NumValues> {};

template <>
struct ParamTraits<mozilla::gfx::FeatureStatus>
    : public ContiguousEnumSerializer<mozilla::gfx::FeatureStatus,
                                      mozilla::gfx::FeatureStatus::Unused,
                                      mozilla::gfx::FeatureStatus::LAST> {};

template <>
struct ParamTraits<mozilla::gfx::LightType>
    : public ContiguousEnumSerializer<mozilla::gfx::LightType,
                                      mozilla::gfx::LightType::None,
                                      mozilla::gfx::LightType::Max> {};

template <>
struct ParamTraits<mozilla::gfx::ColorSpace>
    : public ContiguousEnumSerializer<mozilla::gfx::ColorSpace,
                                      mozilla::gfx::ColorSpace::SRGB,
                                      mozilla::gfx::ColorSpace::Max> {};

template <>
struct ParamTraits<mozilla::gfx::CompositionOp>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::CompositionOp, mozilla::gfx::CompositionOp::OP_OVER,
          mozilla::gfx::CompositionOp::OP_COUNT> {};

/*
template <>
struct ParamTraits<mozilla::PixelFormat>
  : public EnumSerializer<mozilla::PixelFormat,
                          SurfaceFormat::A8R8G8B8_UINT32,
                          SurfaceFormat::UNKNOWN>
{};
*/

template <>
struct ParamTraits<mozilla::gfx::sRGBColor> {
  typedef mozilla::gfx::sRGBColor paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.r);
    WriteParam(msg, param.g);
    WriteParam(msg, param.b);
    WriteParam(msg, param.a);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (
        ReadParam(msg, iter, &result->r) && ReadParam(msg, iter, &result->g) &&
        ReadParam(msg, iter, &result->b) && ReadParam(msg, iter, &result->a));
  }
};

template <>
struct ParamTraits<mozilla::gfx::DeviceColor> {
  typedef mozilla::gfx::DeviceColor paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.r);
    WriteParam(msg, param.g);
    WriteParam(msg, param.b);
    WriteParam(msg, param.a);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (
        ReadParam(msg, iter, &result->r) && ReadParam(msg, iter, &result->g) &&
        ReadParam(msg, iter, &result->b) && ReadParam(msg, iter, &result->a));
  }
};

template <>
struct ParamTraits<nsPoint> {
  typedef nsPoint paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template <>
struct ParamTraits<nsIntPoint> {
  typedef nsIntPoint paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template <typename T>
struct ParamTraits<mozilla::gfx::IntSizeTyped<T>> {
  typedef mozilla::gfx::IntSizeTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template <typename Region, typename Rect, typename Iter>
struct RegionParamTraits {
  typedef Region paramType;

  static void Write(Message* msg, const paramType& param) {
    for (auto iter = param.RectIter(); !iter.Done(); iter.Next()) {
      const Rect& r = iter.Get();
      MOZ_RELEASE_ASSERT(!r.IsEmpty(), "GFX: rect is empty.");
      WriteParam(msg, r);
    }
    // empty rects are sentinel values because nsRegions will never
    // contain them
    WriteParam(msg, Rect());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
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

template <class Units>
struct ParamTraits<mozilla::gfx::IntRegionTyped<Units>>
    : RegionParamTraits<
          mozilla::gfx::IntRegionTyped<Units>,
          mozilla::gfx::IntRectTyped<Units>,
          typename mozilla::gfx::IntRegionTyped<Units>::RectIterator> {};

template <>
struct ParamTraits<mozilla::gfx::IntSize> {
  typedef mozilla::gfx::IntSize paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::CoordTyped<T>> {
  typedef mozilla::gfx::CoordTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.value);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->value));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntCoordTyped<T>> {
  typedef mozilla::gfx::IntCoordTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.value);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->value));
  }
};

template <class T, class U>
struct ParamTraits<mozilla::gfx::ScaleFactor<T, U>> {
  typedef mozilla::gfx::ScaleFactor<T, U> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.scale);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->scale));
  }
};

template <class T, class U>
struct ParamTraits<mozilla::gfx::ScaleFactors2D<T, U>> {
  typedef mozilla::gfx::ScaleFactors2D<T, U> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.xScale);
    WriteParam(msg, param.yScale);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->xScale) &&
            ReadParam(msg, iter, &result->yScale));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::PointTyped<T>> {
  typedef mozilla::gfx::PointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template <class F, class T>
struct ParamTraits<mozilla::gfx::Point3DTyped<F, T>> {
  typedef mozilla::gfx::Point3DTyped<F, T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.z);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->z));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntPointTyped<T>> {
  typedef mozilla::gfx::IntPointTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::SizeTyped<T>> {
  typedef mozilla::gfx::SizeTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::RectTyped<T>> {
  typedef mozilla::gfx::RectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.X());
    WriteParam(msg, param.Y());
    WriteParam(msg, param.Width());
    WriteParam(msg, param.Height());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();

    bool retVal = (ReadParam(msg, iter, &x) && ReadParam(msg, iter, &y) &&
                   ReadParam(msg, iter, &w) && ReadParam(msg, iter, &h));
    result->SetRect(x, y, w, h);
    return retVal;
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::RectAbsoluteTyped<T>> {
  typedef mozilla::gfx::RectAbsoluteTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.Left());
    WriteParam(msg, param.Right());
    WriteParam(msg, param.Top());
    WriteParam(msg, param.Bottom());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    auto l = result->Left();
    auto r = result->Right();
    auto t = result->Top();
    auto b = result->Bottom();

    bool retVal = (ReadParam(msg, iter, &l) && ReadParam(msg, iter, &r) &&
                   ReadParam(msg, iter, &t) && ReadParam(msg, iter, &b));
    result->SetBox(l, r, t, b);
    return retVal;
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntRectTyped<T>> {
  typedef mozilla::gfx::IntRectTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.X());
    WriteParam(msg, param.Y());
    WriteParam(msg, param.Width());
    WriteParam(msg, param.Height());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();

    bool retVal = (ReadParam(msg, iter, &x) && ReadParam(msg, iter, &y) &&
                   ReadParam(msg, iter, &w) && ReadParam(msg, iter, &h));
    result->SetRect(x, y, w, h);
    return retVal;
  }
};

template <>
struct ParamTraits<mozilla::gfx::Margin> {
  typedef mozilla::gfx::Margin paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::MarginTyped<T>> {
  typedef mozilla::gfx::MarginTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntMarginTyped<T>> {
  typedef mozilla::gfx::IntMarginTyped<T> paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.top);
    WriteParam(msg, param.right);
    WriteParam(msg, param.bottom);
    WriteParam(msg, param.left);
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    return (ReadParam(msg, iter, &result->top) &&
            ReadParam(msg, iter, &result->right) &&
            ReadParam(msg, iter, &result->bottom) &&
            ReadParam(msg, iter, &result->left));
  }
};

template <>
struct ParamTraits<nsRect> {
  typedef nsRect paramType;

  static void Write(Message* msg, const paramType& param) {
    WriteParam(msg, param.X());
    WriteParam(msg, param.Y());
    WriteParam(msg, param.Width());
    WriteParam(msg, param.Height());
  }

  static bool Read(const Message* msg, PickleIterator* iter,
                   paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();
    bool retVal = (ReadParam(msg, iter, &x) && ReadParam(msg, iter, &y) &&
                   ReadParam(msg, iter, &w) && ReadParam(msg, iter, &h));
    result->SetRect(x, y, w, h);
    return retVal;
  }
};

template <>
struct ParamTraits<nsRegion>
    : RegionParamTraits<nsRegion, nsRect, nsRegion::RectIterator> {};

template <>
struct ParamTraits<GeckoProcessType>
    : public ContiguousEnumSerializer<
          GeckoProcessType, GeckoProcessType_Default, GeckoProcessType_End> {};

template <>
struct ParamTraits<mozilla::gfx::SurfaceFormat>
    : public ContiguousEnumSerializer<mozilla::gfx::SurfaceFormat,
                                      mozilla::gfx::SurfaceFormat::B8G8R8A8,
                                      mozilla::gfx::SurfaceFormat::UNKNOWN> {};

template <>
struct ParamTraits<mozilla::gfx::ColorDepth>
    : public ContiguousEnumSerializer<mozilla::gfx::ColorDepth,
                                      mozilla::gfx::ColorDepth::COLOR_8,
                                      mozilla::gfx::ColorDepth::UNKNOWN> {};

template <>
struct ParamTraits<mozilla::gfx::ColorRange>
    : public ContiguousEnumSerializer<mozilla::gfx::ColorRange,
                                      mozilla::gfx::ColorRange::LIMITED,
                                      mozilla::gfx::ColorRange::UNKNOWN> {};

template <>
struct ParamTraits<mozilla::gfx::YUVColorSpace>
    : public ContiguousEnumSerializer<
          mozilla::gfx::YUVColorSpace, mozilla::gfx::YUVColorSpace::BT601,
          mozilla::gfx::YUVColorSpace::_NUM_COLORSPACE> {};

template <>
struct ParamTraits<mozilla::StereoMode>
    : public ContiguousEnumSerializer<mozilla::StereoMode,
                                      mozilla::StereoMode::MONO,
                                      mozilla::StereoMode::MAX> {};

template <>
struct ParamTraits<mozilla::gfx::ImplicitlyCopyableFloatArray>
    : public ParamTraits<nsTArray<float>> {
  typedef mozilla::gfx::ImplicitlyCopyableFloatArray paramType;
};

template <>
struct ParamTraits<mozilla::gfx::EmptyAttributes> {
  typedef mozilla::gfx::EmptyAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {}

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::MergeAttributes> {
  typedef mozilla::gfx::MergeAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {}

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ToAlphaAttributes> {
  typedef mozilla::gfx::ToAlphaAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {}

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::TileAttributes> {
  typedef mozilla::gfx::TileAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {}

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::BlendAttributes> {
  typedef mozilla::gfx::BlendAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBlendMode);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mBlendMode);
  }
};

template <>
struct ParamTraits<mozilla::gfx::MorphologyAttributes> {
  typedef mozilla::gfx::MorphologyAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mOperator);
    WriteParam(aMsg, aParam.mRadii);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mOperator) ||
        !ReadParam(aMsg, aIter, &aResult->mRadii)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::FloodAttributes> {
  typedef mozilla::gfx::FloodAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mColor);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mColor)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::OpacityAttributes> {
  typedef mozilla::gfx::OpacityAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mOpacity);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mOpacity)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::OffsetAttributes> {
  typedef mozilla::gfx::OffsetAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DisplacementMapAttributes> {
  typedef mozilla::gfx::DisplacementMapAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mScale);
    WriteParam(aMsg, aParam.mXChannel);
    WriteParam(aMsg, aParam.mYChannel);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mScale) ||
        !ReadParam(aMsg, aIter, &aResult->mXChannel) ||
        !ReadParam(aMsg, aIter, &aResult->mYChannel)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::TurbulenceAttributes> {
  typedef mozilla::gfx::TurbulenceAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mBaseFrequency);
    WriteParam(aMsg, aParam.mSeed);
    WriteParam(aMsg, aParam.mOctaves);
    WriteParam(aMsg, aParam.mStitchable);
    WriteParam(aMsg, aParam.mType);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mOffset) ||
        !ReadParam(aMsg, aIter, &aResult->mBaseFrequency) ||
        !ReadParam(aMsg, aIter, &aResult->mSeed) ||
        !ReadParam(aMsg, aIter, &aResult->mOctaves) ||
        !ReadParam(aMsg, aIter, &aResult->mStitchable) ||
        !ReadParam(aMsg, aIter, &aResult->mType)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ImageAttributes> {
  typedef mozilla::gfx::ImageAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mFilter);
    WriteParam(aMsg, aParam.mInputIndex);
    WriteParam(aMsg, aParam.mTransform);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mFilter) ||
        !ReadParam(aMsg, aIter, &aResult->mInputIndex) ||
        !ReadParam(aMsg, aIter, &aResult->mTransform)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::GaussianBlurAttributes> {
  typedef mozilla::gfx::GaussianBlurAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStdDeviation);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mStdDeviation)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DropShadowAttributes> {
  typedef mozilla::gfx::DropShadowAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mStdDeviation);
    WriteParam(aMsg, aParam.mOffset);
    WriteParam(aMsg, aParam.mColor);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mStdDeviation) ||
        !ReadParam(aMsg, aIter, &aResult->mOffset) ||
        !ReadParam(aMsg, aIter, &aResult->mColor)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ColorMatrixAttributes> {
  typedef mozilla::gfx::ColorMatrixAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mValues);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mType) ||
        !ReadParam(aMsg, aIter, &aResult->mValues)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ComponentTransferAttributes> {
  typedef mozilla::gfx::ComponentTransferAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    for (int i = 0; i < 4; ++i) {
      WriteParam(aMsg, aParam.mTypes[i]);
    }
    for (int i = 0; i < 4; ++i) {
      WriteParam(aMsg, aParam.mValues[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    for (int i = 0; i < 4; ++i) {
      if (!ReadParam(aMsg, aIter, &aResult->mTypes[i])) {
        return false;
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (!ReadParam(aMsg, aIter, &aResult->mValues[i])) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ConvolveMatrixAttributes> {
  typedef mozilla::gfx::ConvolveMatrixAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mKernelSize);
    WriteParam(aMsg, aParam.mKernelMatrix);
    WriteParam(aMsg, aParam.mDivisor);
    WriteParam(aMsg, aParam.mBias);
    WriteParam(aMsg, aParam.mTarget);
    WriteParam(aMsg, aParam.mEdgeMode);
    WriteParam(aMsg, aParam.mKernelUnitLength);
    WriteParam(aMsg, aParam.mPreserveAlpha);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mKernelSize) ||
        !ReadParam(aMsg, aIter, &aResult->mKernelMatrix) ||
        !ReadParam(aMsg, aIter, &aResult->mDivisor) ||
        !ReadParam(aMsg, aIter, &aResult->mBias) ||
        !ReadParam(aMsg, aIter, &aResult->mTarget) ||
        !ReadParam(aMsg, aIter, &aResult->mEdgeMode) ||
        !ReadParam(aMsg, aIter, &aResult->mKernelUnitLength) ||
        !ReadParam(aMsg, aIter, &aResult->mPreserveAlpha)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DiffuseLightingAttributes> {
  typedef mozilla::gfx::DiffuseLightingAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLightType);
    WriteParam(aMsg, aParam.mLightValues);
    WriteParam(aMsg, aParam.mSurfaceScale);
    WriteParam(aMsg, aParam.mKernelUnitLength);
    WriteParam(aMsg, aParam.mColor);
    WriteParam(aMsg, aParam.mLightingConstant);
    WriteParam(aMsg, aParam.mSpecularExponent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mLightType) ||
        !ReadParam(aMsg, aIter, &aResult->mLightValues) ||
        !ReadParam(aMsg, aIter, &aResult->mSurfaceScale) ||
        !ReadParam(aMsg, aIter, &aResult->mKernelUnitLength) ||
        !ReadParam(aMsg, aIter, &aResult->mColor) ||
        !ReadParam(aMsg, aIter, &aResult->mLightingConstant) ||
        !ReadParam(aMsg, aIter, &aResult->mSpecularExponent)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::SpecularLightingAttributes> {
  typedef mozilla::gfx::SpecularLightingAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLightType);
    WriteParam(aMsg, aParam.mLightValues);
    WriteParam(aMsg, aParam.mSurfaceScale);
    WriteParam(aMsg, aParam.mKernelUnitLength);
    WriteParam(aMsg, aParam.mColor);
    WriteParam(aMsg, aParam.mLightingConstant);
    WriteParam(aMsg, aParam.mSpecularExponent);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mLightType) ||
        !ReadParam(aMsg, aIter, &aResult->mLightValues) ||
        !ReadParam(aMsg, aIter, &aResult->mSurfaceScale) ||
        !ReadParam(aMsg, aIter, &aResult->mKernelUnitLength) ||
        !ReadParam(aMsg, aIter, &aResult->mColor) ||
        !ReadParam(aMsg, aIter, &aResult->mLightingConstant) ||
        !ReadParam(aMsg, aIter, &aResult->mSpecularExponent)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::CompositeAttributes> {
  typedef mozilla::gfx::CompositeAttributes paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mOperator);
    WriteParam(aMsg, aParam.mCoefficients);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mOperator) ||
        !ReadParam(aMsg, aIter, &aResult->mCoefficients)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::FilterPrimitiveDescription> {
  typedef mozilla::gfx::FilterPrimitiveDescription paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
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

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    mozilla::gfx::IntRect primitiveSubregion;
    mozilla::gfx::IntRect filterSpaceBounds;
    bool isTainted = false;
    mozilla::gfx::ColorSpace outputColorSpace;
    size_t numberOfInputs = 0;
    if (!ReadParam(aMsg, aIter, &primitiveSubregion) ||
        !ReadParam(aMsg, aIter, &filterSpaceBounds) ||
        !ReadParam(aMsg, aIter, &isTainted) ||
        !ReadParam(aMsg, aIter, &outputColorSpace) ||
        !ReadParam(aMsg, aIter, &numberOfInputs)) {
      return false;
    }

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
struct ParamTraits<mozilla::gfx::FilterDescription> {
  typedef mozilla::gfx::FilterDescription paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mPrimitives);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mPrimitives));
  }
};

template <>
struct ParamTraits<mozilla::gfx::Glyph> {
  typedef mozilla::gfx::Glyph paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mIndex);
    WriteParam(aMsg, aParam.mPosition);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mIndex) &&
            ReadParam(aMsg, aIter, &aResult->mPosition));
  }
};

template <typename T, size_t Length>
struct ParamTraits<mozilla::Array<T, Length>> {
  typedef mozilla::Array<T, Length> paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    for (size_t i = 0; i < Length; i++) {
      WriteParam(aMsg, aParam[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    for (size_t i = 0; i < Length; i++) {
      if (!ReadParam<T>(aMsg, aIter, &aResult->operator[](i))) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::SideBits>
    : public BitFlagsEnumSerializer<mozilla::SideBits,
                                    mozilla::SideBits::eAll> {};

} /* namespace IPC */

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<gfx::PaintFragment> {
  typedef mozilla::gfx::PaintFragment paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
    Shmem shmem;
    if (aParam.mSize.IsEmpty() ||
        !aActor->AllocShmem(aParam.mRecording.mLen, SharedMemory::TYPE_BASIC,
                            &shmem)) {
      WriteParam(aMsg, gfx::IntSize(0, 0));
      return;
    }

    memcpy(shmem.get<uint8_t>(), aParam.mRecording.mData,
           aParam.mRecording.mLen);

    WriteParam(aMsg, aParam.mSize);
    WriteIPDLParam(aMsg, aActor, std::move(shmem));
    WriteParam(aMsg, aParam.mDependencies);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mSize)) {
      return false;
    }
    if (aResult->mSize.IsEmpty()) {
      return true;
    }
    Shmem shmem;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &shmem) ||
        !ReadParam(aMsg, aIter, &aResult->mDependencies)) {
      aActor->DeallocShmem(shmem);
      return false;
    }

    if (!aResult->mRecording.Allocate(shmem.Size<uint8_t>())) {
      aResult->mSize.SizeTo(0, 0);
      aActor->DeallocShmem(shmem);
      return true;
    }

    memcpy(aResult->mRecording.mData, shmem.get<uint8_t>(),
           shmem.Size<uint8_t>());
    aActor->DeallocShmem(shmem);
    return true;
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif /* __GFXMESSAGEUTILS_H__ */
