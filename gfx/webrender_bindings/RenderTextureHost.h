/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERTEXTUREHOST_H

#include "GLConsts.h"
#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/RefPtr.h"
#include "mozilla/webrender/webrender_ffi.h"  // for wr::ImageRendering

namespace mozilla {

namespace gl {
class GLContext;
}

namespace wr {

class RenderBufferTextureHost;
class RenderTextureHostOGL;
class RenderTextureHostWrapper;

void ActivateBindAndTexParameteri(gl::GLContext* aGL, GLenum aActiveTexture,
                                  GLenum aBindTarget, GLuint aBindTexture,
                                  wr::ImageRendering aRendering);

class RenderTextureHost {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderTextureHost)

 public:
  RenderTextureHost();

  virtual wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL,
                                   wr::ImageRendering aRendering) = 0;
  virtual void Unlock() = 0;
  virtual void ClearCachedResources() {}

  virtual RenderTextureHostWrapper* AsRenderTextureHostWrapper() {
    return nullptr;
  }

  virtual void PrepareForUse() {}
  virtual void NofityForUse() {}
  virtual void NotifyNotUsed() {}

 protected:
  virtual ~RenderTextureHost();

  bool IsFilterUpdateNecessary(wr::ImageRendering aRendering);

  wr::ImageRendering mCachedRendering;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERTEXTUREHOST_H
