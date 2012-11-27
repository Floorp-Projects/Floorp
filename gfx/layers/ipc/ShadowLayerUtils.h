/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_ShadowLayerUtils_h
#define IPC_ShadowLayerUtils_h

#include "ipc/IPCMessageUtils.h"
#include "GLContext.h"
#include "mozilla/WidgetUtils.h"

#if defined(MOZ_ENABLE_D3D10_LAYER)
# include "mozilla/layers/ShadowLayerUtilsD3D10.h"
#endif

#if defined(XP_MACOSX)
#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS
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

#if !defined(MOZ_HAVE_SURFACEDESCRIPTORX11)
template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};
#endif  // !defined(MOZ_HAVE_XSURFACEDESCRIPTORX11)

template<>
struct ParamTraits<mozilla::gl::GLContext::SharedTextureShareType>
{
  typedef mozilla::gl::GLContext::SharedTextureShareType paramType;

  static void Write(Message* msg, const paramType& param)
  {
    MOZ_STATIC_ASSERT(sizeof(paramType) <= sizeof(int32_t),
                      "TextureShareType assumes to be int32_t");
    WriteParam(msg, int32_t(param));
  }

  static bool Read(const Message* msg, void** iter, paramType* result)
  {
    int32_t type;
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
