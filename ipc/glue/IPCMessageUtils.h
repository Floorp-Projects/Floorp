/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __IPC_GLUE_IPCMESSAGEUTILS_H__
#define __IPC_GLUE_IPCMESSAGEUTILS_H__

#include "chrome/common/ipc_message_utils.h"

#include "prtypes.h"
#include "nsID.h"
#include "nsMemory.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "gfx3DMatrix.h"
#include "gfxColor.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "gfxASurface.h"
#include "Layers.h"

#ifdef _MSC_VER
#pragma warning( disable : 4800 )
#endif

#if !defined(OS_POSIX)
// This condition must be kept in sync with the one in
// ipc_message_utils.h, but this dummy definition of
// base::FileDescriptor acts as a static assert that we only get one
// def or the other (or neither, in which case code using
// FileDescriptor fails to build)
namespace base { class FileDescriptor { }; }
#endif

using mozilla::layers::LayerManager;

namespace mozilla {

typedef gfxPattern::GraphicsFilter GraphicsFilterType;
typedef gfxASurface::gfxSurfaceType gfxSurfaceType;
typedef LayerManager::LayersBackend LayersBackend;
typedef gfxASurface::gfxImageFormat PixelFormat;

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

} // namespace mozilla

namespace IPC {

template<>
struct ParamTraits<PRInt8>
{
  typedef PRInt8 paramType;

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
struct ParamTraits<PRUint8>
{
  typedef PRUint8 paramType;

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

    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(PR_TRUE);
      return true;
    }

    PRUint32 length;
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

    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    aMsg->WriteBytes(aParam.BeginReading(), length * sizeof(PRUnichar));
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isVoid;
    if (!aMsg->ReadBool(aIter, &isVoid))
      return false;

    if (isVoid) {
      aResult->SetIsVoid(PR_TRUE);
      return true;
    }

    PRUint32 length;
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
      PRUint32 length = aParam.Length();
      for (PRUint32 index = 0; index < length; index++) {
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
struct ParamTraits<nsCAutoString> : ParamTraits<nsCString>
{
  typedef nsCAutoString paramType;
};

#endif  // MOZILLA_INTERNAL_API

template <>
struct ParamTraits<nsString> : ParamTraits<nsAString>
{
  typedef nsString paramType;
};

template <typename E, class A>
struct ParamTraits<nsTArray<E, A> >
{
  typedef nsTArray<E, A> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    PRUint32 length = aParam.Length();
    WriteParam(aMsg, length);
    for (PRUint32 index = 0; index < length; index++) {
      WriteParam(aMsg, aParam[index]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    PRUint32 length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    aResult->SetCapacity(length);
    for (PRUint32 index = 0; index < length; index++) {
      E* element = aResult->AppendElement();
      if (!(element && ReadParam(aMsg, aIter, element))) {
        return false;
      }
    }

    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    for (PRUint32 index = 0; index < aParam.Length(); index++) {
      if (index) {
        aLog->append(L" ");
      }
      LogParam(aParam[index], aLog);
    }
  }
};

template<typename E>
struct ParamTraits<InfallibleTArray<E> > :
  ParamTraits<nsTArray<E, nsTArrayInfallibleAllocator> >
{
  typedef InfallibleTArray<E> paramType;

  // use nsTArray Write() method

  // deserialize the array fallibly, but return an InfallibleTArray
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsTArray<E> temp;
    if (!ReadParam(aMsg, aIter, &temp))
      return false;

    aResult->SwapElements(temp);
    return true;
  }

  // use nsTArray Log() method
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

 template<>
struct ParamTraits<mozilla::GraphicsFilterType>
{
  typedef mozilla::GraphicsFilterType paramType;

  static void Write(Message* msg, const paramType& param)
  {
    switch (param) {
    case gfxPattern::FILTER_FAST:
    case gfxPattern::FILTER_GOOD:
    case gfxPattern::FILTER_BEST:
    case gfxPattern::FILTER_NEAREST:
    case gfxPattern::FILTER_BILINEAR:
    case gfxPattern::FILTER_GAUSSIAN:
      WriteParam(msg, int32(param));
      return;

    default:
      NS_RUNTIMEABORT("not reached");
      return;
    }
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32 filter;
    if (!ReadParam(msg, iter, &filter))
      return false;

    switch (filter) {
    case gfxPattern::FILTER_FAST:
    case gfxPattern::FILTER_GOOD:
    case gfxPattern::FILTER_BEST:
    case gfxPattern::FILTER_NEAREST:
    case gfxPattern::FILTER_BILINEAR:
    case gfxPattern::FILTER_GAUSSIAN:
      *result = paramType(filter);
      return true;

    default:
      return false;
    }
  }
};

 template<>
struct ParamTraits<mozilla::gfxSurfaceType>
{
  typedef mozilla::gfxSurfaceType paramType;

  static void Write(Message* msg, const paramType& param)
  {
    if (gfxASurface::SurfaceTypeImage <= param &&
        param < gfxASurface::SurfaceTypeMax) {
      WriteParam(msg, int32(param));
      return;
    }
    NS_RUNTIMEABORT("surface type not reached");
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32 filter;
    if (!ReadParam(msg, iter, &filter))
      return false;

    if (gfxASurface::SurfaceTypeImage <= filter &&
        filter < gfxASurface::SurfaceTypeMax) {
      *result = paramType(filter);
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<mozilla::LayersBackend>
{
  typedef mozilla::LayersBackend paramType;

  static void Write(Message* msg, const paramType& param)
  {
    if (LayerManager::LAYERS_NONE <= param &&
        param < LayerManager::LAYERS_LAST) {
      WriteParam(msg, int32(param));
      return;
    }
    NS_RUNTIMEABORT("backend type not reached");
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32 type;
    if (!ReadParam(msg, iter, &type))
      return false;

    if (LayerManager::LAYERS_NONE <= type &&
        type < LayerManager::LAYERS_LAST) {
      *result = paramType(type);
      return true;
    }
    return false;
  }
};

template<>
struct ParamTraits<mozilla::PixelFormat>
{
  typedef mozilla::PixelFormat paramType;

  static bool IsLegalPixelFormat(const paramType& format)
  {
    return (gfxASurface::ImageFormatARGB32 <= format &&
            format < gfxASurface::ImageFormatUnknown);
  }

  static void Write(Message* msg, const paramType& param)
  {
    if (!IsLegalPixelFormat(param)) {
      NS_RUNTIMEABORT("Unknown pixel format");
    }
    WriteParam(msg, int32(param));
    return;
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32 format;
    if (!ReadParam(msg, iter, &format) ||
        !IsLegalPixelFormat(paramType(format))) {
      return false;
    }
    *result = paramType(format);
    return true;
  }
};

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
    for (unsigned int i = 0; i < NS_ARRAY_LENGTH(aParam.m3); i++) {
      WriteParam(aMsg, aParam.m3[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if(!ReadParam(aMsg, aIter, &(aResult->m0)) ||
       !ReadParam(aMsg, aIter, &(aResult->m1)) ||
       !ReadParam(aMsg, aIter, &(aResult->m2)))
      return false;

    for (unsigned int i = 0; i < NS_ARRAY_LENGTH(aResult->m3); i++)
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
    for (unsigned int i = 0; i < NS_ARRAY_LENGTH(aParam.m3); i++)
      aLog->append(StringPrintf(L"%2.2X", aParam.m3[i]));
    aLog->append(L"}");
  }
};

} /* namespace IPC */

#endif /* __IPC_GLUE_IPCMESSAGEUTILS_H__ */
