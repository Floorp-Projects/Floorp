/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurface.h"

#include "../2d/2D.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "nsThreadUtils.h"
#include "ScopedGLHelpers.h"
#include "SharedSurfaceGL.h"
#include "SharedSurfaceEGL.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/Unused.h"
#include "VRManagerChild.h"

#ifdef XP_WIN
#  include "SharedSurfaceANGLE.h"
#  include "SharedSurfaceD3D11Interop.h"
#endif

#ifdef XP_MACOSX
#  include "SharedSurfaceIO.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include "gfxPlatformGtk.h"
#  include "SharedSurfaceDMABUF.h"
#  include "mozilla/widget/DMABufLibWrapper.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "SharedSurfaceAndroidHardwareBuffer.h"
#endif

namespace mozilla {
namespace gl {

////////////////////////////////////////////////////////////////////////
// SharedSurface

SharedSurface::SharedSurface(const SharedSurfaceDesc& desc,
                             UniquePtr<MozFramebuffer> fb)
    : mDesc(desc), mFb(std::move(fb)) {}

SharedSurface::~SharedSurface() = default;

void SharedSurface::LockProd() {
  MOZ_ASSERT(!mIsLocked);

  LockProdImpl();

  mDesc.gl->LockSurface(this);
  mIsLocked = true;
}

void SharedSurface::UnlockProd() {
  if (!mIsLocked) return;

  UnlockProdImpl();

  mDesc.gl->UnlockSurface(this);
  mIsLocked = false;
}

////////////////////////////////////////////////////////////////////////
// SurfaceFactory

/* static */
UniquePtr<SurfaceFactory> SurfaceFactory::Create(
    GLContext* const pGl, const layers::TextureType consumerType) {
  auto& gl = *pGl;

  switch (consumerType) {
    case layers::TextureType::D3D11:
#ifdef XP_WIN
      if (gl.IsANGLE()) {
        return SurfaceFactory_ANGLEShareHandle::Create(gl);
      }
      if (StaticPrefs::webgl_dxgl_enabled()) {
        return SurfaceFactory_D3D11Interop::Create(gl);
      }
#endif
      return nullptr;

    case layers::TextureType::MacIOSurface:
#ifdef XP_MACOSX
      return MakeUnique<SurfaceFactory_IOSurface>(gl);
#else
      return nullptr;
#endif

    case layers::TextureType::DMABUF:
#ifdef MOZ_WIDGET_GTK
      if (gl.GetContextType() == GLContextType::EGL &&
          widget::DMABufDevice::IsDMABufWebGLEnabled()) {
        return SurfaceFactory_DMABUF::Create(gl);
      }
#endif
      return nullptr;

    case layers::TextureType::AndroidNativeWindow:
#ifdef MOZ_WIDGET_ANDROID
      return MakeUnique<SurfaceFactory_SurfaceTexture>(gl);
#else
      return nullptr;
#endif

    case layers::TextureType::AndroidHardwareBuffer:
#ifdef MOZ_WIDGET_ANDROID
      if (XRE_IsGPUProcess()) {
        // Enable SharedSurface of AndroidHardwareBuffer only in GPU process.
        return SurfaceFactory_AndroidHardwareBuffer::Create(gl);
      }
#endif
      return nullptr;

    case layers::TextureType::EGLImage:
#ifdef MOZ_WIDGET_ANDROID
      if (XRE_IsParentProcess()) {
        return SurfaceFactory_EGLImage::Create(gl);
      }
#endif
      return nullptr;

    case layers::TextureType::Unknown:
    case layers::TextureType::Last:
      break;
  }

#ifdef MOZ_X11
  // Silence a warning.
  Unused << gl;
#endif

  return nullptr;
}

SurfaceFactory::SurfaceFactory(const PartialSharedSurfaceDesc& partialDesc)
    : mDesc(partialDesc), mMutex("SurfaceFactor::mMutex") {}

SurfaceFactory::~SurfaceFactory() = default;

}  // namespace gl
}  // namespace mozilla
