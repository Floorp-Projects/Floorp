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
#include "chrome/common/ipc_message_utils.h"
#include "gfxFeature.h"
#include "gfxFontUtils.h"
#include "gfxFallback.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxTelemetry.h"
#include "gfxTypes.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/gfx/CrossProcessPaint.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/ScaleFactor.h"
#include "mozilla/gfx/ScaleFactors2D.h"
#include "SharedFontList.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "mozilla/Array.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/ShmemMessageUtils.h"

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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam._11);
    WriteParam(aWriter, aParam._12);
    WriteParam(aWriter, aParam._21);
    WriteParam(aWriter, aParam._22);
    WriteParam(aWriter, aParam._31);
    WriteParam(aWriter, aParam._32);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->_11) &&
        ReadParam(aReader, &aResult->_12) &&
        ReadParam(aReader, &aResult->_21) &&
        ReadParam(aReader, &aResult->_22) &&
        ReadParam(aReader, &aResult->_31) && ReadParam(aReader, &aResult->_32))
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

  static void Write(MessageWriter* writer, const paramType& param) {
#define Wr(_f) WriteParam(writer, param._f)
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

  static bool Read(MessageReader* reader, paramType* result) {
#define Rd(_f) ReadParam(reader, &result->_f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) && Rd(_21) && Rd(_22) &&
            Rd(_23) && Rd(_24) && Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template <>
struct ParamTraits<mozilla::gfx::Matrix5x4> {
  typedef mozilla::gfx::Matrix5x4 paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
#define Wr(_f) WriteParam(writer, param._f)
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

  static bool Read(MessageReader* reader, paramType* result) {
#define Rd(_f) ReadParam(reader, &result->_f)
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

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.x);
    WriteParam(aWriter, aParam.y);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return (ReadParam(aReader, &aResult->x) && ReadParam(aReader, &aResult->y));
  }
};

template <>
struct ParamTraits<gfxSize> {
  typedef gfxSize paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.width);
    WriteParam(aWriter, aParam.height);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (ReadParam(aReader, &aResult->width) &&
        ReadParam(aReader, &aResult->height))
      return true;

    return false;
  }
};

template <>
struct ParamTraits<gfxRect> {
  typedef gfxRect paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.X());
    WriteParam(aWriter, aParam.Y());
    WriteParam(aWriter, aParam.Width());
    WriteParam(aWriter, aParam.Height());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    auto x = aResult->X();
    auto y = aResult->Y();
    auto w = aResult->Width();
    auto h = aResult->Height();

    bool retVal = (ReadParam(aReader, &x) && ReadParam(aReader, &y) &&
                   ReadParam(aReader, &w) && ReadParam(aReader, &h));
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

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.r);
    WriteParam(writer, param.g);
    WriteParam(writer, param.b);
    WriteParam(writer, param.a);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->r) && ReadParam(reader, &result->g) &&
            ReadParam(reader, &result->b) && ReadParam(reader, &result->a));
  }
};

template <>
struct ParamTraits<mozilla::gfx::DeviceColor> {
  typedef mozilla::gfx::DeviceColor paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.r);
    WriteParam(writer, param.g);
    WriteParam(writer, param.b);
    WriteParam(writer, param.a);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->r) && ReadParam(reader, &result->g) &&
            ReadParam(reader, &result->b) && ReadParam(reader, &result->a));
  }
};

template <>
struct ParamTraits<nsPoint> {
  typedef nsPoint paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.x);
    WriteParam(writer, param.y);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->x) && ReadParam(reader, &result->y));
  }
};

template <>
struct ParamTraits<nsIntPoint> {
  typedef nsIntPoint paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.x);
    WriteParam(writer, param.y);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->x) && ReadParam(reader, &result->y));
  }
};

template <typename T>
struct ParamTraits<mozilla::gfx::IntSizeTyped<T>> {
  typedef mozilla::gfx::IntSizeTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.width);
    WriteParam(writer, param.height);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->width) &&
            ReadParam(reader, &result->height));
  }
};

template <typename Region, typename Rect, typename Iter>
struct RegionParamTraits {
  typedef Region paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    for (auto iter = param.RectIter(); !iter.Done(); iter.Next()) {
      const Rect& r = iter.Get();
      MOZ_RELEASE_ASSERT(!r.IsEmpty(), "GFX: rect is empty.");
      WriteParam(writer, r);
    }
    // empty rects are sentinel values because nsRegions will never
    // contain them
    WriteParam(writer, Rect());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    RegionBuilder<Region> builder;
    Rect rect;
    while (ReadParam(reader, &rect)) {
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

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.width);
    WriteParam(writer, param.height);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->width) &&
            ReadParam(reader, &result->height));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::CoordTyped<T>> {
  typedef mozilla::gfx::CoordTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.value);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->value));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntCoordTyped<T>> {
  typedef mozilla::gfx::IntCoordTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.value);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->value));
  }
};

template <class T, class U>
struct ParamTraits<mozilla::gfx::ScaleFactor<T, U>> {
  typedef mozilla::gfx::ScaleFactor<T, U> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.scale);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->scale));
  }
};

template <class T, class U>
struct ParamTraits<mozilla::gfx::ScaleFactors2D<T, U>> {
  typedef mozilla::gfx::ScaleFactors2D<T, U> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.xScale);
    WriteParam(writer, param.yScale);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->xScale) &&
            ReadParam(reader, &result->yScale));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::PointTyped<T>> {
  typedef mozilla::gfx::PointTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.x);
    WriteParam(writer, param.y);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->x) && ReadParam(reader, &result->y));
  }
};

template <class F, class T>
struct ParamTraits<mozilla::gfx::Point3DTyped<F, T>> {
  typedef mozilla::gfx::Point3DTyped<F, T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.x);
    WriteParam(writer, param.y);
    WriteParam(writer, param.z);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->x) && ReadParam(reader, &result->y) &&
            ReadParam(reader, &result->z));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntPointTyped<T>> {
  typedef mozilla::gfx::IntPointTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.x);
    WriteParam(writer, param.y);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->x) && ReadParam(reader, &result->y));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::SizeTyped<T>> {
  typedef mozilla::gfx::SizeTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.width);
    WriteParam(writer, param.height);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (ReadParam(reader, &result->width) &&
            ReadParam(reader, &result->height));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::RectTyped<T>> {
  typedef mozilla::gfx::RectTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.X());
    WriteParam(writer, param.Y());
    WriteParam(writer, param.Width());
    WriteParam(writer, param.Height());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();

    bool retVal = (ReadParam(reader, &x) && ReadParam(reader, &y) &&
                   ReadParam(reader, &w) && ReadParam(reader, &h));
    result->SetRect(x, y, w, h);
    return retVal;
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::RectAbsoluteTyped<T>> {
  typedef mozilla::gfx::RectAbsoluteTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.Left());
    WriteParam(writer, param.Top());
    WriteParam(writer, param.Right());
    WriteParam(writer, param.Bottom());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    auto l = result->Left();
    auto t = result->Top();
    auto r = result->Right();
    auto b = result->Bottom();

    bool retVal = (ReadParam(reader, &l) && ReadParam(reader, &t) &&
                   ReadParam(reader, &r) && ReadParam(reader, &b));
    result->SetBox(l, t, r, b);
    return retVal;
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntRectTyped<T>> {
  typedef mozilla::gfx::IntRectTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.X());
    WriteParam(writer, param.Y());
    WriteParam(writer, param.Width());
    WriteParam(writer, param.Height());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();

    bool retVal = (ReadParam(reader, &x) && ReadParam(reader, &y) &&
                   ReadParam(reader, &w) && ReadParam(reader, &h));
    result->SetRect(x, y, w, h);
    return retVal;
  }
};

template <>
struct ParamTraits<mozilla::gfx::Margin> {
  typedef mozilla::gfx::Margin paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.top);
    WriteParam(writer, param.right);
    WriteParam(writer, param.bottom);
    WriteParam(writer, param.left);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (
        ReadParam(reader, &result->top) && ReadParam(reader, &result->right) &&
        ReadParam(reader, &result->bottom) && ReadParam(reader, &result->left));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::MarginTyped<T>> {
  typedef mozilla::gfx::MarginTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.top);
    WriteParam(writer, param.right);
    WriteParam(writer, param.bottom);
    WriteParam(writer, param.left);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (
        ReadParam(reader, &result->top) && ReadParam(reader, &result->right) &&
        ReadParam(reader, &result->bottom) && ReadParam(reader, &result->left));
  }
};

template <class T>
struct ParamTraits<mozilla::gfx::IntMarginTyped<T>> {
  typedef mozilla::gfx::IntMarginTyped<T> paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.top);
    WriteParam(writer, param.right);
    WriteParam(writer, param.bottom);
    WriteParam(writer, param.left);
  }

  static bool Read(MessageReader* reader, paramType* result) {
    return (
        ReadParam(reader, &result->top) && ReadParam(reader, &result->right) &&
        ReadParam(reader, &result->bottom) && ReadParam(reader, &result->left));
  }
};

template <>
struct ParamTraits<nsRect> {
  typedef nsRect paramType;

  static void Write(MessageWriter* writer, const paramType& param) {
    WriteParam(writer, param.X());
    WriteParam(writer, param.Y());
    WriteParam(writer, param.Width());
    WriteParam(writer, param.Height());
  }

  static bool Read(MessageReader* reader, paramType* result) {
    auto x = result->X();
    auto y = result->Y();
    auto w = result->Width();
    auto h = result->Height();
    bool retVal = (ReadParam(reader, &x) && ReadParam(reader, &y) &&
                   ReadParam(reader, &w) && ReadParam(reader, &h));
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
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::ColorDepth, mozilla::gfx::ColorDepth::_First,
          mozilla::gfx::ColorDepth::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::TransferFunction>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::TransferFunction,
          mozilla::gfx::TransferFunction::_First,
          mozilla::gfx::TransferFunction::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::ColorRange>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::ColorRange, mozilla::gfx::ColorRange::_First,
          mozilla::gfx::ColorRange::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::YUVColorSpace>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::YUVColorSpace, mozilla::gfx::YUVColorSpace::_First,
          mozilla::gfx::YUVColorSpace::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::YUVRangedColorSpace>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::YUVRangedColorSpace,
          mozilla::gfx::YUVRangedColorSpace::_First,
          mozilla::gfx::YUVRangedColorSpace::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::ColorSpace2>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::ColorSpace2, mozilla::gfx::ColorSpace2::_First,
          mozilla::gfx::ColorSpace2::_Last> {};

template <>
struct ParamTraits<mozilla::StereoMode>
    : public ContiguousEnumSerializer<mozilla::StereoMode,
                                      mozilla::StereoMode::MONO,
                                      mozilla::StereoMode::MAX> {};

template <>
struct ParamTraits<mozilla::gfx::ChromaSubsampling>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::ChromaSubsampling,
          mozilla::gfx::ChromaSubsampling::_First,
          mozilla::gfx::ChromaSubsampling::_Last> {};

template <>
struct ParamTraits<mozilla::gfx::ImplicitlyCopyableFloatArray>
    : public ParamTraits<nsTArray<float>> {
  typedef mozilla::gfx::ImplicitlyCopyableFloatArray paramType;
};

template <>
struct ParamTraits<mozilla::gfx::EmptyAttributes> {
  typedef mozilla::gfx::EmptyAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {}

  static bool Read(MessageReader* aReader, paramType* aResult) { return true; }
};

template <>
struct ParamTraits<mozilla::gfx::MergeAttributes> {
  typedef mozilla::gfx::MergeAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {}

  static bool Read(MessageReader* aReader, paramType* aResult) { return true; }
};

template <>
struct ParamTraits<mozilla::gfx::ToAlphaAttributes> {
  typedef mozilla::gfx::ToAlphaAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {}

  static bool Read(MessageReader* aReader, paramType* aResult) { return true; }
};

template <>
struct ParamTraits<mozilla::gfx::TileAttributes> {
  typedef mozilla::gfx::TileAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {}

  static bool Read(MessageReader* aReader, paramType* aResult) { return true; }
};

template <>
struct ParamTraits<mozilla::gfx::BlendAttributes> {
  typedef mozilla::gfx::BlendAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mBlendMode);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mBlendMode);
  }
};

template <>
struct ParamTraits<mozilla::gfx::MorphologyAttributes> {
  typedef mozilla::gfx::MorphologyAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOperator);
    WriteParam(aWriter, aParam.mRadii);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mOperator) ||
        !ReadParam(aReader, &aResult->mRadii)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::FloodAttributes> {
  typedef mozilla::gfx::FloodAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mColor);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mColor)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::OpacityAttributes> {
  typedef mozilla::gfx::OpacityAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOpacity);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mOpacity)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::OffsetAttributes> {
  typedef mozilla::gfx::OffsetAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mValue)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DisplacementMapAttributes> {
  typedef mozilla::gfx::DisplacementMapAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mScale);
    WriteParam(aWriter, aParam.mXChannel);
    WriteParam(aWriter, aParam.mYChannel);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mScale) ||
        !ReadParam(aReader, &aResult->mXChannel) ||
        !ReadParam(aReader, &aResult->mYChannel)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::TurbulenceAttributes> {
  typedef mozilla::gfx::TurbulenceAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mBaseFrequency);
    WriteParam(aWriter, aParam.mSeed);
    WriteParam(aWriter, aParam.mOctaves);
    WriteParam(aWriter, aParam.mStitchable);
    WriteParam(aWriter, aParam.mType);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mOffset) ||
        !ReadParam(aReader, &aResult->mBaseFrequency) ||
        !ReadParam(aReader, &aResult->mSeed) ||
        !ReadParam(aReader, &aResult->mOctaves) ||
        !ReadParam(aReader, &aResult->mStitchable) ||
        !ReadParam(aReader, &aResult->mType)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ImageAttributes> {
  typedef mozilla::gfx::ImageAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mFilter);
    WriteParam(aWriter, aParam.mInputIndex);
    WriteParam(aWriter, aParam.mTransform);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mFilter) ||
        !ReadParam(aReader, &aResult->mInputIndex) ||
        !ReadParam(aReader, &aResult->mTransform)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::GaussianBlurAttributes> {
  typedef mozilla::gfx::GaussianBlurAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStdDeviation);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mStdDeviation)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DropShadowAttributes> {
  typedef mozilla::gfx::DropShadowAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mStdDeviation);
    WriteParam(aWriter, aParam.mOffset);
    WriteParam(aWriter, aParam.mColor);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mStdDeviation) ||
        !ReadParam(aReader, &aResult->mOffset) ||
        !ReadParam(aReader, &aResult->mColor)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ColorMatrixAttributes> {
  typedef mozilla::gfx::ColorMatrixAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mType);
    WriteParam(aWriter, aParam.mValues);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mType) ||
        !ReadParam(aReader, &aResult->mValues)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ComponentTransferAttributes> {
  typedef mozilla::gfx::ComponentTransferAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    for (int i = 0; i < 4; ++i) {
      WriteParam(aWriter, aParam.mTypes[i]);
    }
    for (int i = 0; i < 4; ++i) {
      WriteParam(aWriter, aParam.mValues[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    for (int i = 0; i < 4; ++i) {
      if (!ReadParam(aReader, &aResult->mTypes[i])) {
        return false;
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (!ReadParam(aReader, &aResult->mValues[i])) {
        return false;
      }
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::ConvolveMatrixAttributes> {
  typedef mozilla::gfx::ConvolveMatrixAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mKernelSize);
    WriteParam(aWriter, aParam.mKernelMatrix);
    WriteParam(aWriter, aParam.mDivisor);
    WriteParam(aWriter, aParam.mBias);
    WriteParam(aWriter, aParam.mTarget);
    WriteParam(aWriter, aParam.mEdgeMode);
    WriteParam(aWriter, aParam.mKernelUnitLength);
    WriteParam(aWriter, aParam.mPreserveAlpha);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mKernelSize) ||
        !ReadParam(aReader, &aResult->mKernelMatrix) ||
        !ReadParam(aReader, &aResult->mDivisor) ||
        !ReadParam(aReader, &aResult->mBias) ||
        !ReadParam(aReader, &aResult->mTarget) ||
        !ReadParam(aReader, &aResult->mEdgeMode) ||
        !ReadParam(aReader, &aResult->mKernelUnitLength) ||
        !ReadParam(aReader, &aResult->mPreserveAlpha)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::DiffuseLightingAttributes> {
  typedef mozilla::gfx::DiffuseLightingAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLightType);
    WriteParam(aWriter, aParam.mLightValues);
    WriteParam(aWriter, aParam.mSurfaceScale);
    WriteParam(aWriter, aParam.mKernelUnitLength);
    WriteParam(aWriter, aParam.mColor);
    WriteParam(aWriter, aParam.mLightingConstant);
    WriteParam(aWriter, aParam.mSpecularExponent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mLightType) ||
        !ReadParam(aReader, &aResult->mLightValues) ||
        !ReadParam(aReader, &aResult->mSurfaceScale) ||
        !ReadParam(aReader, &aResult->mKernelUnitLength) ||
        !ReadParam(aReader, &aResult->mColor) ||
        !ReadParam(aReader, &aResult->mLightingConstant) ||
        !ReadParam(aReader, &aResult->mSpecularExponent)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::SpecularLightingAttributes> {
  typedef mozilla::gfx::SpecularLightingAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLightType);
    WriteParam(aWriter, aParam.mLightValues);
    WriteParam(aWriter, aParam.mSurfaceScale);
    WriteParam(aWriter, aParam.mKernelUnitLength);
    WriteParam(aWriter, aParam.mColor);
    WriteParam(aWriter, aParam.mLightingConstant);
    WriteParam(aWriter, aParam.mSpecularExponent);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mLightType) ||
        !ReadParam(aReader, &aResult->mLightValues) ||
        !ReadParam(aReader, &aResult->mSurfaceScale) ||
        !ReadParam(aReader, &aResult->mKernelUnitLength) ||
        !ReadParam(aReader, &aResult->mColor) ||
        !ReadParam(aReader, &aResult->mLightingConstant) ||
        !ReadParam(aReader, &aResult->mSpecularExponent)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::CompositeAttributes> {
  typedef mozilla::gfx::CompositeAttributes paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mOperator);
    WriteParam(aWriter, aParam.mCoefficients);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mOperator) ||
        !ReadParam(aReader, &aResult->mCoefficients)) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::Glyph> {
  typedef mozilla::gfx::Glyph paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mIndex);
    WriteParam(aWriter, aParam.mPosition);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return (ReadParam(aReader, &aResult->mIndex) &&
            ReadParam(aReader, &aResult->mPosition));
  }
};

template <typename T, size_t Length>
struct ParamTraits<mozilla::Array<T, Length>> {
  typedef mozilla::Array<T, Length> paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    for (size_t i = 0; i < Length; i++) {
      WriteParam(aWriter, aParam[i]);
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    for (size_t i = 0; i < Length; i++) {
      if (!ReadParam<T>(aReader, &aResult->operator[](i))) {
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

template <>
struct ParamTraits<gfxSparseBitSet> {
  typedef gfxSparseBitSet paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mBlockIndex);
    WriteParam(aWriter, aParam.mBlocks);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mBlockIndex) &&
           ReadParam(aReader, &aResult->mBlocks);
  }
};

template <>
struct ParamTraits<gfxSparseBitSet::Block> {
  typedef gfxSparseBitSet::Block paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    aWriter->WriteBytes(&aParam, sizeof(aParam));
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    return aReader->ReadBytesInto(aResult, sizeof(*aResult));
  }
};

// The actual FontVisibility enum is defined in gfxTypes.h
template <>
struct ParamTraits<FontVisibility>
    : public ContiguousEnumSerializer<FontVisibility, FontVisibility::Unknown,
                                      FontVisibility::Count> {};

template <>
struct ParamTraits<mozilla::fontlist::Pointer> {
  typedef mozilla::fontlist::Pointer paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    uint32_t v = aParam.mBlockAndOffset;
    WriteParam(aWriter, v);
  }
  static bool Read(MessageReader* aReader, paramType* aResult) {
    uint32_t v;
    if (ReadParam(aReader, &v)) {
      aResult->mBlockAndOffset.store(v);
      return true;
    }
    return false;
  }
};

}  // namespace IPC

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<gfx::PaintFragment> {
  typedef mozilla::gfx::PaintFragment paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    paramType&& aParam) {
    Shmem shmem;
    if (aParam.mSize.IsEmpty() ||
        !aActor->AllocShmem(aParam.mRecording.mLen, SharedMemory::TYPE_BASIC,
                            &shmem)) {
      WriteParam(aWriter, gfx::IntSize(0, 0));
      return;
    }

    memcpy(shmem.get<uint8_t>(), aParam.mRecording.mData,
           aParam.mRecording.mLen);

    WriteParam(aWriter, aParam.mSize);
    WriteIPDLParam(aWriter, aActor, std::move(shmem));
    WriteParam(aWriter, aParam.mDependencies);
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    if (!ReadParam(aReader, &aResult->mSize)) {
      return false;
    }
    if (aResult->mSize.IsEmpty()) {
      return true;
    }
    Shmem shmem;
    if (!ReadIPDLParam(aReader, aActor, &shmem) ||
        !ReadParam(aReader, &aResult->mDependencies)) {
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
