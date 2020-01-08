/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFormats.h"

namespace mozilla {

WebGLExtensionTextureFloat::WebGLExtensionTextureFloat(WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl));

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

  ////////////////

  pi = {LOCAL_GL_RGBA, LOCAL_GL_FLOAT};
  dui = {pi.format, pi.format, pi.type};
  swizzle = nullptr;
  if (useSizedFormats || gl->IsExtensionSupported(
                             gl::GLContext::CHROMIUM_color_buffer_float_rgba)) {
    // ANGLE only exposes renderable RGBA32F via
    // CHROMIUM_color_buffer_float_rgba, which uses sized formats.
    dui.internalFormat = LOCAL_GL_RGBA32F;
  }
  fnAdd(webgl::EffectiveFormat::RGBA32F);

  //////

  pi = {LOCAL_GL_RGB, LOCAL_GL_FLOAT};
  dui = {pi.format, pi.format, pi.type};
  swizzle = nullptr;
  if (useSizedFormats) {
    dui.internalFormat = LOCAL_GL_RGB32F;
  }
  fnAdd(webgl::EffectiveFormat::RGB32F);

  //////

  pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_FLOAT};
  dui = {pi.format, pi.format, pi.type};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_LUMINANCE32F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_R32F;
      dui.unpackFormat = LOCAL_GL_RED;
      swizzle = webgl::FormatUsageInfo::kLuminanceSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Luminance32F);

  //////

  pi = {LOCAL_GL_ALPHA, LOCAL_GL_FLOAT};
  dui = {pi.format, pi.format, pi.type};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_ALPHA32F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_R32F;
      dui.unpackFormat = LOCAL_GL_RED;
      swizzle = webgl::FormatUsageInfo::kAlphaSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Alpha32F);

  //////

  pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT};
  dui = {pi.format, pi.format, pi.type};
  swizzle = nullptr;
  if (useSizedFormats) {
    if (hasSizedLegacyFormats) {
      dui.internalFormat = LOCAL_GL_LUMINANCE_ALPHA32F_ARB;
    } else {
      dui.internalFormat = LOCAL_GL_RG32F;
      dui.unpackFormat = LOCAL_GL_RG;
      swizzle = webgl::FormatUsageInfo::kLumAlphaSwizzleRGBA;
    }
  }
  fnAdd(webgl::EffectiveFormat::Luminance32FAlpha32F);
}

bool WebGLExtensionTextureFloat::IsSupported(const WebGLContext* webgl) {
  if (webgl->IsWebGL2()) return false;

  gl::GLContext* gl = webgl->GL();
  if (!gl->IsSupported(gl::GLFeature::texture_float)) return false;

  const bool needsSwizzle = gl->IsCoreProfile();
  const bool hasSwizzle = gl->IsSupported(gl::GLFeature::texture_swizzle);
  if (needsSwizzle && !hasSwizzle) return false;

  return true;
}

}  // namespace mozilla
