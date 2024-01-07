/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERTEXTUREHOST_H

#include "GLConsts.h"
#include "GLTypes.h"
#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/RefPtr.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace wr {

class RenderAndroidHardwareBufferTextureHost;
class RenderAndroidSurfaceTextureHost;
class RenderCompositor;
class RenderDXGITextureHost;
class RenderDXGIYCbCrTextureHost;
class RenderDcompSurfaceTextureHost;
class RenderMacIOSurfaceTextureHost;
class RenderBufferTextureHost;
class RenderTextureHostSWGL;
class RenderTextureHostWrapper;

void ActivateBindAndTexParameteri(gl::GLContext* aGL, GLenum aActiveTexture,
                                  GLenum aBindTarget, GLuint aBindTexture);

class RenderTextureHost {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderTextureHost)

 public:
  RenderTextureHost();

  virtual gfx::SurfaceFormat GetFormat() const {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual gfx::YUVRangedColorSpace GetYUVColorSpace() const {
    return gfx::YUVRangedColorSpace::Default;
  }

  virtual wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL);

  virtual void Unlock() {}

  virtual wr::WrExternalImage LockSWGL(uint8_t aChannelIndex, void* aContext,
                                       RenderCompositor* aCompositor);

  virtual void UnlockSWGL() {}

  virtual void ClearCachedResources() {}

  // Called asynchronouly when corresponding TextureHost's mCompositableCount
  // becomes from 0 to 1. For now, it is used only for
  // SurfaceTextureHost/RenderAndroidSurfaceTextureHost.
  virtual void PrepareForUse() {}
  // Called asynchronouly when corresponding TextureHost's is actually going to
  // be used by WebRender. For now, it is used only for
  // SurfaceTextureHost/RenderAndroidSurfaceTextureHost.
  virtual void NotifyForUse() {}
  // Called asynchronouly when corresponding TextureHost's mCompositableCount
  // becomes 0. For now, it is used only for
  // SurfaceTextureHost/RenderAndroidSurfaceTextureHost.
  virtual void NotifyNotUsed() {}
  // Returns true when RenderTextureHost needs SyncObjectHost::Synchronize()
  // call, before its usage.
  virtual bool SyncObjectNeeded() { return false; }
  // Returns true when this texture was generated from a DRM-protected source.
  bool IsFromDRMSource() { return mIsFromDRMSource; }
  void SetIsFromDRMSource(bool aIsFromDRMSource) {
    mIsFromDRMSource = aIsFromDRMSource;
  }

  virtual size_t Bytes() = 0;

  virtual RenderDXGITextureHost* AsRenderDXGITextureHost() { return nullptr; }
  virtual RenderDXGIYCbCrTextureHost* AsRenderDXGIYCbCrTextureHost() {
    return nullptr;
  }

  virtual RenderMacIOSurfaceTextureHost* AsRenderMacIOSurfaceTextureHost() {
    return nullptr;
  }

  virtual RenderAndroidHardwareBufferTextureHost*
  AsRenderAndroidHardwareBufferTextureHost() {
    return nullptr;
  }

  virtual RenderAndroidSurfaceTextureHost* AsRenderAndroidSurfaceTextureHost() {
    return nullptr;
  }

  virtual RenderTextureHostSWGL* AsRenderTextureHostSWGL() { return nullptr; }

  virtual RenderDcompSurfaceTextureHost* AsRenderDcompSurfaceTextureHost() {
    return nullptr;
  }

  virtual void Destroy();

  virtual void SetIsSoftwareDecodedVideo() {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  }
  virtual bool IsSoftwareDecodedVideo() {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return false;
  }

 protected:
  virtual ~RenderTextureHost();

  // Returns the UV coordinates to be used when sampling the texture, in pixels.
  // For most implementations these will be (0, 0) and (size.x, size.y), but
  // some texture types (such as RenderAndroidSurfaceTextureHost) require an
  // additional transform to be applied to the coordinates.
  virtual std::pair<gfx::Point, gfx::Point> GetUvCoords(
      gfx::IntSize aTextureSize) const;

  bool mIsFromDRMSource;

  friend class RenderTextureHostWrapper;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERTEXTUREHOST_H
