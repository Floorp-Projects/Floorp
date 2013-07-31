/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "base/process_util.h"
#include "chrome/common/ipc_message_utils.h"

#include "mozilla/TimeStamp.h"
#ifdef XP_WIN
#include "mozilla/TimeStamp_windows.h"
#endif
#include "mozilla/Util.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"

#include <stdint.h>

#include "nsID.h"
#include "nsMemory.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "gfx3DMatrix.h"
#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "gfxASurface.h"
#include "jsapi.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/CompositorTypes.h"
#include "FrameMetrics.h"
#include "nsCSSProperty.h"
#include "ImageLayers.h"

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

#if !defined(OS_POSIX)
// This condition must be kept in sync with the one in
// ipc_message_utils.h, but this dummy definition of
// base::FileDescriptor acts as a static assert that we only get one
// def or the other (or neither, in which case code using
// FileDescriptor fails to build)
namespace base { struct FileDescriptor { }; }
#endif

namespace mozilla {

typedef gfxASurface::gfxContentType gfxContentType;
typedef gfxASurface::gfxImageFormat PixelFormat;
typedef gfxASurface::gfxSurfaceType gfxSurfaceType;
typedef gfxPattern::GraphicsFilter GraphicsFilterType;
typedef layers::LayersBackend LayersBackend;
typedef layers::ImageLayer::ScaleMode ScaleMode;

// This is a cross-platform approximation to HANDLE, which we expect
// to be typedef'd to void* or thereabouts.
typedef uintptr_t WindowsHandle;

// XXX there are out of place and might be generally useful.  Could
// move to nscore.h or something.
struct void_t {
  bool operator==(const void_t&) const { return true; }
};
struct null_t {
  bool operator==(const null_t&) const { return true; }
};

struct SerializedStructuredCloneBuffer
{
  SerializedStructuredCloneBuffer()
  : data(nullptr), dataLength(0)
  { }

  SerializedStructuredCloneBuffer(const JSAutoStructuredCloneBuffer& aOther)
  {
    *this = aOther;
  }

  bool
  operator==(const SerializedStructuredCloneBuffer& aOther) const
  {
    return this->data == aOther.data &&
           this->dataLength == aOther.dataLength;
  }

  SerializedStructuredCloneBuffer&
  operator=(const JSAutoStructuredCloneBuffer& aOther)
  {
    data = aOther.data();
    dataLength = aOther.nbytes();
    return *this;
  }

  uint64_t* data;
  size_t dataLength;
};

} // namespace mozilla

namespace IPC {

/**
 * Generic enum serializer.
 *
 * This is a generic serializer for any enum type used in IPDL.
 * Programmers can define ParamTraits<E> for enum type E by deriving
 * EnumSerializer<E, smallestLegal, highGuard>.
 *
 * The serializer would check value againts a range specified by
 * smallestLegal and highGuard.  Only values from smallestLegal to
 * highGuard are valid, include smallestLegal but highGuard.
 *
 * For example, following is definition of serializer for enum type FOO.
 * \code
 * enum FOO { FOO_FIRST, FOO_SECOND, FOO_LAST, NUM_FOO };
 *
 * template <>
 * struct ParamTraits<FOO>:
 *     public EnumSerializer<FOO, FOO_FIRST, NUM_FOO> {};
 * \endcode
 * FOO_FIRST, FOO_SECOND, and FOO_LAST are valid value.
 *
 * \sa https://developer.mozilla.org/en/IPDL/Type_Serialization
 */
template <typename E, E smallestLegal, E highBound>
struct EnumSerializer {
  typedef E paramType;

  static bool IsLegalValue(const paramType &aValue) {
    return smallestLegal <= aValue && aValue < highBound;
  }

  static void Write(Message* aMsg, const paramType& aValue) {
    MOZ_ASSERT(IsLegalValue(aValue));
    WriteParam(aMsg, (int32_t)aValue);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    int32_t value;
    if(!ReadParam(aMsg, aIter, &value) ||
       !IsLegalValue(paramType(value))) {
      return false;
    }
    *aResult = paramType(value);
    return true;
  }
};

template <>
struct ParamTraits<base::ChildPrivileges>
  : public EnumSerializer<base::ChildPrivileges,
                          base::PRIVILEGES_DEFAULT,
                          base::PRIVILEGES_LAST>
{ };

template<>
struct ParamTraits<int8_t>
{
  typedef int8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

template<>
struct ParamTraits<uint8_t>
{
  typedef uint8_t paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(aParam));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outp;
    if (!aMsg->ReadBytes(aIter, &outp, sizeof(*aResult)))
      return false;

    *aResult = *reinterpret_cast<const paramType*>(outp);
    return true;
  }
};

#if !defined(OS_POSIX)
// See above re: keeping definitions in sync
template<>
struct ParamTraits<base::FileDescriptor>
{
  typedef base::FileDescriptor paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult) {
    NS_RUNTIMEABORT("FileDescriptor isn't meaningful on this platform");
    return false;
  }
};
#endif  // !defined(OS_POSIX)

template <>
struct ParamTraits<nsACString>
{
  typedef nsACString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a NULL pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const char* buf;
      if (aMsg->ReadBytes(aIter, &buf, length)) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else
      aLog->append(UTF8ToWide(aParam.BeginReading()));
  }
};

template <>
struct ParamTraits<nsAString>
{
  typedef nsAString paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isVoid = aParam.IsVoid();
    aMsg->WriteBool(isVoid);

    if (isVoid)
      // represents a NULL pointer
      return;

    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(PRUnichar));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(true);
      return true;
    }

    uint32_t length;
    if (ReadParam(aMsg, aIter, &length)) {
      const PRUnichar* buf;
      if (aMsg->ReadBytes(aIter, reinterpret_cast<const char**>(&buf),
                       length * sizeof(PRUnichar))) {
        aResult->Assign(buf, length);
        return true;
      }
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    if (aParam.IsVoid())
      aLog->append(L"(NULL)");
    else {
#ifdef WCHAR_T_IS_UTF16
      aLog->append(reinterpret_cast<const wchar_t*>(aParam.BeginReading()));
#else
      uint32_t length = aParam.Length();
      for (uint32_t index = 0; index < length; index++) {
        aLog->push_back(std::wstring::value_type(aParam[index]));
      }
#endif
    }
  }
};

template <>
struct ParamTraits<nsCString> : ParamTraits<nsACString>
{
  typedef nsCString paramType;
};

#ifdef MOZILLA_INTERNAL_API

template<>
struct ParamTraits<nsAutoCString> : ParamTraits<nsCString>
{
  typedef nsAutoCString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsString> : ParamTraits<nsAString>
{
  typedef nsString paramType;
};

template <typename E>
struct ParamTraits<FallibleTArray<E> >
{
  typedef FallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    uint32_t length = aParam.Length();
    WriteParam(aMsg, length);
    for (uint32_t index = 0; index < length; index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint32_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    aResult->SetCapacity(length);
    for (uint32_t index = 0; index < length; index++) {
      E* element = aResult->AppendElement();
      if (!(element && ReadParam(aMsg, aIter, element))) {
        return false;
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    for (uint32_t index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template<typename E>
struct ParamTraits<InfallibleTArray<E> >
{
  typedef InfallibleTArray<E> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, static_cast<const FallibleTArray<E>&>(aParam));
  }

  // deserialize the array fallibly, but return an InfallibleTArray
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    FallibleTArray<E> temp;
    if (!ReadParam(aMsg, aIter, &temp))
      return false;

    aResult->SwapElements(temp);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(static_cast<const FallibleTArray<E>&>(aParam), aLog);
  }
};

template<>
struct ParamTraits<float>
{
  typedef float paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    aMsg->WriteBytes(&aParam, sizeof(paramType));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    const char* outFloat;
    if (!aMsg->ReadBytes(aIter, &outFloat, sizeof(float)))
      return false;
    *aResult = *reinterpret_cast<const float*>(outFloat);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"%g", aParam));
  }
};

template<>
struct ParamTraits<gfxMatrix>
{
  typedef gfxMatrix paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.xx);
    WriteParam(aMsg, aParam.xy);
    WriteParam(aMsg, aParam.yx);
    WriteParam(aMsg, aParam.yy);
    WriteParam(aMsg, aParam.x0);
    WriteParam(aMsg, aParam.y0);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (ReadParam(aMsg, aIter, &aResult->xx) &&
        ReadParam(aMsg, aIter, &aResult->xy) &&
        ReadParam(aMsg, aIter, &aResult->yx) &&
        ReadParam(aMsg, aIter, &aResult->yy) &&
        ReadParam(aMsg, aIter, &aResult->x0) &&
        ReadParam(aMsg, aIter, &aResult->y0))
      return true;

    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[[%g %g] [%g %g] [%g %g]]", aParam.xx, aParam.xy, aParam.yx, aParam.yy,
	  						    aParam.x0, aParam.y0));
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y));
 }
};

template<>
struct ParamTraits<gfxPoint3D>
{
  typedef gfxPoint3D paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.z);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y) &&
            ReadParam(aMsg, aIter, &aResult->z));
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x) &&
           ReadParam(aMsg, aIter, &aResult->y) &&
           ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<gfx3DMatrix>
{
  typedef gfx3DMatrix paramType;

  static void Write(Message* msg, const paramType& param)
  {
#define Wr(_f)  WriteParam(msg, param. _f)
    Wr(_11); Wr(_12); Wr(_13); Wr(_14);
    Wr(_21); Wr(_22); Wr(_23); Wr(_24);
    Wr(_31); Wr(_32); Wr(_33); Wr(_34);
    Wr(_41); Wr(_42); Wr(_43); Wr(_44);
#undef Wr
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
#define Rd(_f)  ReadParam(msg, iter, &result-> _f)
    return (Rd(_11) && Rd(_12) && Rd(_13) && Rd(_14) &&
            Rd(_21) && Rd(_22) && Rd(_23) && Rd(_24) &&
            Rd(_31) && Rd(_32) && Rd(_33) && Rd(_34) &&
            Rd(_41) && Rd(_42) && Rd(_43) && Rd(_44));
#undef Rd
  }
};

template <>
struct ParamTraits<mozilla::gfxContentType>
  : public EnumSerializer<mozilla::gfxContentType,
                          gfxASurface::CONTENT_COLOR,
                          gfxASurface::CONTENT_SENTINEL>
{};

template <>
struct ParamTraits<mozilla::gfxSurfaceType>
  : public EnumSerializer<gfxASurface::gfxSurfaceType,
                          gfxASurface::SurfaceTypeImage,
                          gfxASurface::SurfaceTypeMax>
{};

template <>
struct ParamTraits<mozilla::GraphicsFilterType>
  : public EnumSerializer<mozilla::GraphicsFilterType,
                          gfxPattern::FILTER_FAST,
                          gfxPattern::FILTER_SENTINEL>
{};

template <>
struct ParamTraits<mozilla::layers::LayersBackend>
  : public EnumSerializer<mozilla::layers::LayersBackend,
                          mozilla::layers::LAYERS_NONE,
                          mozilla::layers::LAYERS_LAST>
{};

template <>
struct ParamTraits<mozilla::ScaleMode>
  : public EnumSerializer<mozilla::ScaleMode,
                          mozilla::layers::ImageLayer::SCALE_NONE,
                          mozilla::layers::ImageLayer::SCALE_SENTINEL>
{};

template <>
struct ParamTraits<mozilla::PixelFormat>
  : public EnumSerializer<mozilla::PixelFormat,
                          gfxASurface::ImageFormatARGB32,
                          gfxASurface::ImageFormatUnknown>
{};

template <>
struct ParamTraits<nsCSSProperty>
  : public EnumSerializer<nsCSSProperty,
                          eCSSProperty_UNKNOWN,
                          eCSSProperty_COUNT>
{};


template<>
struct ParamTraits<gfxRGBA>
{
  typedef gfxRGBA paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.r);
    WriteParam(msg, param.g);
    WriteParam(msg, param.b);
    WriteParam(msg, param.a);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->r) &&
            ReadParam(msg, iter, &result->g) &&
            ReadParam(msg, iter, &result->b) &&
            ReadParam(msg, iter, &result->a));
  }
};

template<>
struct ParamTraits<mozilla::void_t>
{
  typedef mozilla::void_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
  }
};

template<>
struct ParamTraits<mozilla::null_t>
{
  typedef mozilla::null_t paramType;
  static void Write(Message* aMsg, const paramType& aParam) { }
  static bool
  Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    *aResult = paramType();
    return true;
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

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<mozilla::gfx::IntSize>
{
  typedef mozilla::gfx::IntSize paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsIntRect>
{
  typedef nsIntRect paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.x);
    WriteParam(msg, param.y);
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsIntRegion>
{
  typedef nsIntRegion paramType;

  static void Write(Message* msg, const paramType& param)
  {
    nsIntRegionRectIterator it(param);
    while (const nsIntRect* r = it.Next())
      WriteParam(msg, *r);
    // empty rects are sentinel values because nsRegions will never
    // contain them
    WriteParam(msg, nsIntRect());
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    nsIntRect rect;
    while (ReadParam(msg, iter, &rect)) {
      if (rect.IsEmpty())
        return true;
      result->Or(*result, rect);
    }
    return false;
  }
};

template<>
struct ParamTraits<nsIntSize>
{
  typedef nsIntSize paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
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

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->scale));
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

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
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

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y));
  }
};

template<>
struct ParamTraits<mozilla::gfx::Size>
{
  typedef mozilla::gfx::Size paramType;

  static void Write(Message* msg, const paramType& param)
  {
    WriteParam(msg, param.width);
    WriteParam(msg, param.height);
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
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

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    return (ReadParam(msg, iter, &result->x) &&
            ReadParam(msg, iter, &result->y) &&
            ReadParam(msg, iter, &result->width) &&
            ReadParam(msg, iter, &result->height));
  }
};

template<>
struct ParamTraits<nsID>
{
  typedef nsID paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.m0);
    WriteParam(aMsg, aParam.m1);
    WriteParam(aMsg, aParam.m2);
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++) {
      WriteParam(aMsg, aParam.m3[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if(!ReadParam(aMsg, aIter, &(aResult->m0)) ||
       !ReadParam(aMsg, aIter, &(aResult->m1)) ||
       !ReadParam(aMsg, aIter, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < mozilla::ArrayLength(aResult->m3); i++)
      if (!ReadParam(aMsg, aIter, &(aResult->m3[i])))
        return false;

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(L"{");
    aLog->append(StringPrintf(L"%8.8X-%4.4X-%4.4X-",
                              aParam.m0,
                              aParam.m1,
                              aParam.m2));
    for (unsigned int i = 0; i < mozilla::ArrayLength(aParam.m3); i++)
      aLog->append(StringPrintf(L"%2.2X", aParam.m3[i]));
    aLog->append(L"}");
  }
};

template<>
struct ParamTraits<mozilla::TimeDuration>
{
  typedef mozilla::TimeDuration paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

template<>
struct ParamTraits<mozilla::TimeStamp>
{
  typedef mozilla::TimeStamp paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mValue);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mValue);
  };
};

#ifdef XP_WIN

template<>
struct ParamTraits<mozilla::TimeStampValue>
{
  typedef mozilla::TimeStampValue paramType;
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mGTC);
    WriteParam(aMsg, aParam.mQPC);
    WriteParam(aMsg, aParam.mHasQPC);
    WriteParam(aMsg, aParam.mIsNull);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mGTC) &&
            ReadParam(aMsg, aIter, &aResult->mQPC) &&
            ReadParam(aMsg, aIter, &aResult->mHasQPC) &&
            ReadParam(aMsg, aIter, &aResult->mIsNull));
  }
};

#endif

template <>
struct ParamTraits<mozilla::SerializedStructuredCloneBuffer>
{
  typedef mozilla::SerializedStructuredCloneBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.dataLength);
    if (aParam.dataLength) {
      // Structured clone data must be 64-bit aligned.
      aMsg->WriteBytes(aParam.data, aParam.dataLength, sizeof(uint64_t));
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->dataLength)) {
      return false;
    }

    if (aResult->dataLength) {
      const char** buffer =
        const_cast<const char**>(reinterpret_cast<char**>(&aResult->data));
      // Structured clone data must be 64-bit aligned.
      if (!aMsg->ReadBytes(aIter, buffer, aResult->dataLength,
                           sizeof(uint64_t))) {
        return false;
      }
    } else {
      aResult->data = NULL;
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.dataLength, aLog);
  }
};

template <>
struct ParamTraits<mozilla::layers::FrameMetrics>
{
  typedef mozilla::layers::FrameMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mScrollableRect);
    WriteParam(aMsg, aParam.mViewport);
    WriteParam(aMsg, aParam.mScrollOffset);
    WriteParam(aMsg, aParam.mDisplayPort);
    WriteParam(aMsg, aParam.mCriticalDisplayPort);
    WriteParam(aMsg, aParam.mCompositionBounds);
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mResolution);
    WriteParam(aMsg, aParam.mZoom);
    WriteParam(aMsg, aParam.mDevPixelsPerCSSPixel);
    WriteParam(aMsg, aParam.mMayHaveTouchListeners);
    WriteParam(aMsg, aParam.mPresShellId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mScrollableRect) &&
            ReadParam(aMsg, aIter, &aResult->mViewport) &&
            ReadParam(aMsg, aIter, &aResult->mScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCriticalDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mCompositionBounds) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mResolution) &&
            ReadParam(aMsg, aIter, &aResult->mZoom) &&
            ReadParam(aMsg, aIter, &aResult->mDevPixelsPerCSSPixel) &&
            ReadParam(aMsg, aIter, &aResult->mMayHaveTouchListeners) &&
            ReadParam(aMsg, aIter, &aResult->mPresShellId));
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureFactoryIdentifier>
{
  typedef mozilla::layers::TextureFactoryIdentifier paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mParentBackend);
    WriteParam(aMsg, aParam.mMaxTextureSize);
    WriteParam(aMsg, aParam.mSupportsTextureBlitting);
    WriteParam(aMsg, aParam.mSupportsPartialUploads);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mParentBackend) &&
           ReadParam(aMsg, aIter, &aResult->mMaxTextureSize) &&
           ReadParam(aMsg, aIter, &aResult->mSupportsTextureBlitting) &&
           ReadParam(aMsg, aIter, &aResult->mSupportsPartialUploads);
  }
};

template<>
struct ParamTraits<mozilla::layers::TextureInfo>
{
  typedef mozilla::layers::TextureInfo paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCompositableType);
    WriteParam(aMsg, aParam.mDeprecatedTextureHostFlags);
    WriteParam(aMsg, aParam.mTextureFlags);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mCompositableType) &&
           ReadParam(aMsg, aIter, &aResult->mDeprecatedTextureHostFlags) &&
           ReadParam(aMsg, aIter, &aResult->mTextureFlags);
  }
};

template <>
struct ParamTraits<mozilla::layers::CompositableType>
  : public EnumSerializer<mozilla::layers::CompositableType,
                          mozilla::layers::BUFFER_UNKNOWN,
                          mozilla::layers::BUFFER_COUNT>
{};

template <>
struct ParamTraits<mozilla::gfx::SurfaceFormat>
  : public EnumSerializer<mozilla::gfx::SurfaceFormat,
                          mozilla::gfx::FORMAT_B8G8R8A8,
                          mozilla::gfx::FORMAT_UNKNOWN>
{};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
