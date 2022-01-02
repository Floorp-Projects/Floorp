/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_RENDERBUFFER_H_
#define WEBGL_RENDERBUFFER_H_

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTexture.h"

namespace mozilla {
namespace webgl {
struct FormatUsageInfo;
}

class WebGLRenderbuffer final : public WebGLContextBoundObject,
                                public WebGLRectangleObject,
                                public CacheInvalidator {
  friend class WebGLFramebuffer;
  friend class WebGLFBAttachPoint;

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLRenderbuffer, override)

 public:
  const GLuint mPrimaryRB;

 protected:
  const bool mEmulatePackedDepthStencil;
  GLuint mSecondaryRB;
  webgl::ImageInfo mImageInfo;

 public:
  explicit WebGLRenderbuffer(WebGLContext* webgl);

  const auto& ImageInfo() const { return mImageInfo; }

  void RenderbufferStorage(uint32_t samples, GLenum internalFormat,
                           uint32_t width, uint32_t height);
  // Only handles a subset of `pname`s.
  GLint GetRenderbufferParameter(RBParam pname) const;

  auto MemoryUsage() const { return mImageInfo.MemoryUsage(); }

 protected:
  ~WebGLRenderbuffer() override;

  void DoFramebufferRenderbuffer(GLenum attachment) const;
  GLenum DoRenderbufferStorage(uint32_t samples,
                               const webgl::FormatUsageInfo* format,
                               uint32_t width, uint32_t height);
};

}  // namespace mozilla

#endif  // WEBGL_RENDERBUFFER_H_
