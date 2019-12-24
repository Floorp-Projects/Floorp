/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureHalfFloat::WebGLExtensionTextureHalfFloat(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  auto& fua = webgl->mFormatUsage;
  gl::GLContext* gl = webgl->GL();

  webgl::PackingInfo pi;
  webgl::DriverUnpackInfo dui;
  const GLint* swizzle = nullptr;

  const auto fnAdd = [&](webgl::EffectiveFormat effFormat) {
    MOZ_ASSERT_IF(swizzle, gl->IsSupported(gl::GLFeature::texture_swizzle));

    auto usage = fua->EditUsage(effFormat);
    usage->textureSwizzleRGBA = swizzle;
    fua->AddTexUnpack(usage, pi, dui);

    fua->AllowUnsizedTexFormat(pi, usage);
  };

  bool useSizedFormats = true;
  const bool hasSizedLegacyFormats = gl->IsCompatibilityProfile();
  if (gl->IsGLES() && gl->Version() < 300) {
    useSizedFormats = false;
  }

  GLenum driverUnpackType = LOCAL_GL_HALF_FLOAT;
  if (!gl->IsSupported(gl::GLFeature::texture_half_float)) {
    MOZ_ASSERT(gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float));
    driverUnpackType = LOCAL_GL_HALF_FLOAT_OES;
  }

  ////////////////

  pi = {LOCAL_GL_RGBA, LOCAL_GL_HALF_FLOAT_OES};
  dui = {pi.format, pi.format, driverUnpackType};
  swizzle = nullptr;
  if (useSizedFormats) {
    dui.internalFormat = LOCAL_GL_RGBA16F;
  }
  fnAdd(webgl::EffectiveFormat::RGBA16F);

  //////

  pi = {LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT_OES};
  dui = {pi.format, pi.format, driverUnpackType};
  swizzle = nullptr;
  if (useSizedFormats) {
    dui.internalFormat = LOCAL_GL_RGB16F;
  }
  fnAdd(webgl::EffectiveFormat::RGB16F);

  //////

  pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_HALF_FLOAT_OES};
  dui = {pi.format, pi.format, driverUnpackType};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_LUMINANCE16F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_R16F;
      dui.unpackFormat = LOCAL_GL_RED;
      swizzle = webgl::FormatUsageInfo::kLuminanceSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Luminance16F);

  //////

  pi = {LOCAL_GL_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
  dui = {pi.format, pi.format, driverUnpackType};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_ALPHA16F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_R16F;
      dui.unpackFormat = LOCAL_GL_RED;
      swizzle = webgl::FormatUsageInfo::kAlphaSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Alpha16F);

  //////

  pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT_OES};
  dui = {pi.format, pi.format, driverUnpackType};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_LUMINANCE_ALPHA16F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_RG16F;
      dui.unpackFormat = LOCAL_GL_RG;
      swizzle = webgl::FormatUsageInfo::kLumAlphaSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Luminance16FAlpha16F);
}

WebGLExtensionTextureHalfFloat::~WebGLExtensionTextureHalfFloat() {}

bool WebGLExtensionTextureHalfFloat::IsSupported(const WebGLContext* webgl) {
  if (webgl->IsWebGL2()) return false;

  gl::GLContext* gl = webgl->GL();
  if (!gl->IsSupported(gl::GLFeature::texture_half_float) &&
      !gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float)) {
    return false;
  }

  const bool needsSwizzle = gl->IsCoreProfile();
  const bool hasSwizzle = gl->IsSupported(gl::GLFeature::texture_swizzle);
  if (needsSwizzle && !hasSwizzle) return false;

  return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionTextureHalfFloat,
                          OES_texture_half_float)

}  // namespace mozilla
