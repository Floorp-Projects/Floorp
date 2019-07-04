/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/StaticPrefs.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionBase::WebGLExtensionBase(WebGLContext* context)
    : WebGLContextBoundObject(context), mIsLost(false) {}

WebGLExtensionBase::~WebGLExtensionBase() {}

void WebGLExtensionBase::MarkLost() {
  mIsLost = true;

  OnMarkLost();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLExtensionBase)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLExtensionBase, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLExtensionBase, Release)

// -

WebGLExtensionExplicitPresent::WebGLExtensionExplicitPresent(
    WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

bool WebGLExtensionExplicitPresent::IsSupported(
    const WebGLContext* const webgl) {
  return StaticPrefs::webgl_enable_draft_extensions();
}

void WebGLExtensionExplicitPresent::Present() const {
  if (mIsLost || !mContext) return;
  mContext->PresentScreenBuffer();
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionExplicitPresent, WEBGL_explicit_present)

// -

WebGLExtensionFloatBlend::WebGLExtensionFloatBlend(WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionFloatBlend::~WebGLExtensionFloatBlend() = default;

bool WebGLExtensionFloatBlend::IsSupported(const WebGLContext* const webgl) {
  if (!WebGLExtensionColorBufferFloat::IsSupported(webgl) &&
      !WebGLExtensionEXTColorBufferFloat::IsSupported(webgl))
    return false;

  const auto& gl = webgl->gl;
  if (!gl->IsGLES() && gl->Version() >= 300) return true;
  if (gl->IsGLES() && gl->Version() >= 320) return true;
  return gl->IsExtensionSupported(gl::GLContext::EXT_float_blend);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionFloatBlend, EXT_float_blend)

// -

WebGLExtensionFBORenderMipmap::WebGLExtensionFBORenderMipmap(
    WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionFBORenderMipmap::~WebGLExtensionFBORenderMipmap() = default;

bool WebGLExtensionFBORenderMipmap::IsSupported(
    const WebGLContext* const webgl) {
  if (webgl->IsWebGL2()) return false;
  if (!StaticPrefs::webgl_enable_draft_extensions()) {
    return false;
  }

  const auto& gl = webgl->gl;
  if (!gl->IsGLES()) return true;
  if (gl->Version() >= 300) return true;
  return gl->IsExtensionSupported(gl::GLContext::OES_fbo_render_mipmap);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionFBORenderMipmap, OES_fbo_render_mipmap)

// -

WebGLExtensionMultiview::WebGLExtensionMultiview(WebGLContext* const webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionMultiview::~WebGLExtensionMultiview() = default;

bool WebGLExtensionMultiview::IsSupported(const WebGLContext* const webgl) {
  if (!webgl->IsWebGL2()) return false;
  if (!StaticPrefs::webgl_enable_draft_extensions()) {
    return false;
  }

  const auto& gl = webgl->gl;
  return gl->IsSupported(gl::GLFeature::multiview);
}

void WebGLExtensionMultiview::FramebufferTextureMultiviewOVR(
    const GLenum target, const GLenum attachment, WebGLTexture* const texture,
    const GLint level, const GLint baseViewIndex,
    const GLsizei numViews) const {
  const WebGLContext::FuncScope funcScope(*mContext,
                                          "framebufferTextureMultiviewOVR");
  if (mIsLost) {
    mContext->ErrorInvalidOperation("Extension is lost.");
    return;
  }

  mContext->FramebufferTextureMultiview(target, attachment, texture, level,
                                        baseViewIndex, numViews);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionMultiview, OVR_multiview2)

}  // namespace mozilla
