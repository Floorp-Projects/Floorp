/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLExtensions.h"
#include "WebGLVertexArray.h"

#include "nsString.h"
#include "nsDebug.h"
#include "nsReadableUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"
#include "gfxUtils.h"

#include "jsfriendapi.h"

#include "WebGLTexelConversions.h"
#include "WebGLValidateStrings.h"
#include <algorithm>

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/EndianUtils.h"

namespace mozilla {

/*virtual*/
bool WebGLContext::IsTexParamValid(GLenum pname) const {
  switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
      return true;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
      return IsExtensionEnabled(
          WebGLExtensionID::EXT_texture_filter_anisotropic);

    default:
      return false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

void WebGLContext::BindTexture(GLenum rawTarget, WebGLTexture* newTex) {
  FuncScope funcScope(*this, "bindTexture");
  if (IsContextLost()) return;
  funcScope.mBindFailureGuard = true;

  if (newTex && !ValidateObject("tex", *newTex)) return;

  // Need to check rawTarget first before comparing against newTex->Target() as
  // newTex->Target() returns a TexTarget, which will assert on invalid value.
  RefPtr<WebGLTexture>* currentTexPtr = nullptr;
  switch (rawTarget) {
    case LOCAL_GL_TEXTURE_2D:
      currentTexPtr = &mBound2DTextures[mActiveTexture];
      break;

    case LOCAL_GL_TEXTURE_CUBE_MAP:
      currentTexPtr = &mBoundCubeMapTextures[mActiveTexture];
      break;

    case LOCAL_GL_TEXTURE_3D:
      if (IsWebGL2()) currentTexPtr = &mBound3DTextures[mActiveTexture];
      break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
      if (IsWebGL2()) currentTexPtr = &mBound2DArrayTextures[mActiveTexture];
      break;
  }

  if (!currentTexPtr) {
    ErrorInvalidEnumInfo("target", rawTarget);
    return;
  }

  const TexTarget texTarget(rawTarget);
  if (newTex) {
    if (!newTex->BindTexture(texTarget)) return;
  } else {
    gl->fBindTexture(texTarget.get(), 0);
  }

  *currentTexPtr = newTex;

  funcScope.mBindFailureGuard = false;
}

void WebGLContext::GenerateMipmap(GLenum texTarget) {
  const FuncScope funcScope(*this, "generateMipmap");

  const auto& tex = GetActiveTex(texTarget);
  if (!tex) return;
  tex->GenerateMipmap();
}

Maybe<double> WebGLContext::GetTexParameter(const WebGLTexture& tex,
                                            GLenum pname) const {
  const FuncScope funcScope(*this, "getTexParameter");

  if (!IsTexParamValid(pname)) {
    ErrorInvalidEnumInfo("pname", pname);
    return Nothing();
  }

  return tex.GetTexParameter(pname);
}

void WebGLContext::TexParameter_base(GLenum texTarget, GLenum pname,
                                     const FloatOrInt& param) {
  const FuncScope funcScope(*this, "texParameter");

  const auto& tex = GetActiveTex(texTarget);
  if (!tex) return;
  tex->TexParameter(texTarget, pname, param);
}

//////////////////////////////////////////////////////////////////////////////////////////
// Uploads

WebGLTexture* WebGLContext::GetActiveTex(const GLenum texTarget) const {
  const auto* list = &mBound2DTextures;
  switch (texTarget) {
    case LOCAL_GL_TEXTURE_2D:
      break;
    case LOCAL_GL_TEXTURE_CUBE_MAP:
      list = &mBoundCubeMapTextures;
      break;
    case LOCAL_GL_TEXTURE_3D:
      list = IsWebGL2() ? &mBound3DTextures : nullptr;
      break;
    case LOCAL_GL_TEXTURE_2D_ARRAY:
      list = IsWebGL2() ? &mBound2DArrayTextures : nullptr;
      break;
    default:
      list = nullptr;
  }
  if (!list) {
    ErrorInvalidEnumArg("target", texTarget);
    return nullptr;
  }
  const auto& tex = (*list)[mActiveTexture];
  if (!tex) {
    GenerateError(LOCAL_GL_INVALID_OPERATION, "No texture bound to %s[%u].",
                  EnumString(texTarget).c_str(), mActiveTexture);
    return nullptr;
  }
  return tex;
}

void WebGLContext::TexStorage(GLenum texTarget, uint32_t levels,
                              GLenum internalFormat, uvec3 size) const {
  const FuncScope funcScope(*this, "texStorage(Multisample)?");
  if (!IsTexTarget3D(texTarget)) {
    size.z = 1;
  }
  const auto tex = GetActiveTex(texTarget);
  if (!tex) return;
  tex->TexStorage(texTarget, levels, internalFormat, size);
}

void WebGLContext::TexImage(uint32_t level, GLenum respecFormat, uvec3 offset,
                            const webgl::PackingInfo& pi,
                            const webgl::TexUnpackBlobDesc& src) const {
  const WebGLContext::FuncScope funcScope(
      *this, bool(respecFormat) ? "texImage" : "texSubImage");

  const bool isUploadFromPbo = bool(src.pboOffset);
  const bool isPboBound = bool(mBoundPixelUnpackBuffer);
  if (isUploadFromPbo != isPboBound) {
    GenerateError(LOCAL_GL_INVALID_OPERATION,
                  "Tex upload from %s but PIXEL_UNPACK_BUFFER %s bound.",
                  isUploadFromPbo ? "PBO" : "non-PBO",
                  isPboBound ? "was" : "was not");
    return;
  }

  if (respecFormat) {
    offset = {0, 0, 0};
  }
  const auto texTarget = ImageToTexTarget(src.imageTarget);
  const auto tex = GetActiveTex(texTarget);
  if (!tex) return;
  tex->TexImage(level, respecFormat, offset, pi, src);
}

void WebGLContext::CompressedTexImage(bool sub, GLenum imageTarget,
                                      uint32_t level, GLenum format,
                                      uvec3 offset, uvec3 size,
                                      const Range<const uint8_t>& src,
                                      const uint32_t pboImageSize,
                                      const Maybe<uint64_t>& pboOffset) const {
  const WebGLContext::FuncScope funcScope(
      *this, !sub ? "compressedTexImage" : "compressedTexSubImage");

  if (!sub) {
    offset = {0, 0, 0};
  }
  const auto texTarget = ImageToTexTarget(imageTarget);
  if (!IsTexTarget3D(texTarget)) {
    size.z = 1;
  }
  const auto tex = GetActiveTex(texTarget);
  if (!tex) return;
  tex->CompressedTexImage(sub, imageTarget, level, format, offset, size, src,
                          pboImageSize, pboOffset);
}

void WebGLContext::CopyTexImage(GLenum imageTarget, uint32_t level,
                                GLenum respecFormat, uvec3 dstOffset,
                                const ivec2& srcOffset,
                                const uvec2& size) const {
  const WebGLContext::FuncScope funcScope(
      *this, bool(respecFormat) ? "copyTexImage" : "copyTexSubImage");

  if (respecFormat) {
    dstOffset = {0, 0, 0};
  }
  const auto tex = GetActiveTex(ImageToTexTarget(imageTarget));
  if (!tex) return;
  tex->CopyTexImage(imageTarget, level, respecFormat, dstOffset, srcOffset,
                    size);
}

}  // namespace mozilla
