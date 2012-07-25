/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_ShadowLayerUtils_h
#define IPC_ShadowLayerUtils_h

#include "IPC/IPCMessageUtils.h"
#include "Layers.h"
#include "GLContext.h"
#include "mozilla/WidgetUtils.h"

#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "mozilla/layers/ShadowLayerUtilsD3D10.h"
#endif

#if defined(MOZ_X11)
# include "mozilla/layers/ShadowLayerUtilsX11.h"
#else
namespace mozilla { namespace layers {
struct SurfaceDescriptorX11 {
  bool operator==(const SurfaceDescriptorX11&) const { return false; }
};
} }
#endif

#if defined(MOZ_WIDGET_GONK)
# include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#else
namespace mozilla { namespace layers {
struct MagicGrallocBufferHandle {
  bool operator==(const MagicGrallocBufferHandle&) const { return false; }
};
} }
#endif

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::FrameMetrics>
{
  typedef mozilla::layers::FrameMetrics paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mCSSContentRect);
    WriteParam(aMsg, aParam.mViewport);
    WriteParam(aMsg, aParam.mContentRect);
    WriteParam(aMsg, aParam.mViewportScrollOffset);
    WriteParam(aMsg, aParam.mDisplayPort);
    WriteParam(aMsg, aParam.mScrollId);
    WriteParam(aMsg, aParam.mResolution);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->mCSSContentRect) &&
            ReadParam(aMsg, aIter, &aResult->mViewport) &&
            ReadParam(aMsg, aIter, &aResult->mContentRect) &&
            ReadParam(aMsg, aIter, &aResult->mViewportScrollOffset) &&
            ReadParam(aMsg, aIter, &aResult->mDisplayPort) &&
            ReadParam(aMsg, aIter, &aResult->mScrollId) &&
            ReadParam(aMsg, aIter, &aResult->mResolution));
  }
};

#if !defined(MOZ_HAVE_SURFACEDESCRIPTORX11)
template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};
#endif  // !defined(MOZ_HAVE_XSURFACEDESCRIPTORX11)

template<>
struct ParamTraits<mozilla::gl::TextureImage::TextureShareType>
{
  typedef mozilla::gl::TextureImage::TextureShareType paramType;

  static void Write(Message* msg, const paramType& param)
  {
    MOZ_STATIC_ASSERT(sizeof(paramType) <= sizeof(int32),
                      "TextureShareType assumes to be int32");
    WriteParam(msg, int32(param));
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32 type;
    if (!ReadParam(msg, iter, &type))
      return false;

    *result = paramType(type);
    return true;
  }
};

#if !defined(MOZ_HAVE_SURFACEDESCRIPTORGRALLOC)
template <>
struct ParamTraits<mozilla::layers::MagicGrallocBufferHandle> {
  typedef mozilla::layers::MagicGrallocBufferHandle paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};
#endif  // !defined(MOZ_HAVE_XSURFACEDESCRIPTORGRALLOC)

template <>
struct ParamTraits<mozilla::ScreenRotation>
  : public EnumSerializer<mozilla::ScreenRotation,
                          mozilla::ROTATION_0,
                          mozilla::ROTATION_COUNT>
{};

} // namespace IPC

#endif // IPC_ShadowLayerUtils_h
