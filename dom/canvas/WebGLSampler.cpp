/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLSampler.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLSampler::WebGLSampler(WebGLContext* const webgl)
    : WebGLContextBoundObject(webgl), mGLName([&]() {
        GLuint ret = 0;
        webgl->gl->fGenSamplers(1, &ret);
        return ret;
      }()) {}

WebGLSampler::~WebGLSampler() {
  if (!mContext) return;
  mContext->gl->fDeleteSamplers(1, &mGLName);
}

static bool ValidateSamplerParameterParams(WebGLContext* webgl, GLenum pname,
                                           const FloatOrInt& param) {
  const auto& paramInt = param.i;

  switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
      switch (paramInt) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
          return true;

        default:
          break;
      }
      break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
      switch (paramInt) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
          return true;

        default:
          break;
      }
      break;

    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
      switch (paramInt) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_REPEAT:
        case LOCAL_GL_MIRRORED_REPEAT:
          return true;

        default:
          break;
      }
      break;

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
      return true;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
      switch (paramInt) {
        case LOCAL_GL_NONE:
        case LOCAL_GL_COMPARE_REF_TO_TEXTURE:
          return true;

        default:
          break;
      }
      break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
      switch (paramInt) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
          return true;

        default:
          break;
      }
      break;

    default:
      webgl->ErrorInvalidEnumInfo("pname", pname);
      return false;
  }

  webgl->ErrorInvalidEnumInfo("param", paramInt);
  return false;
}

void WebGLSampler::SamplerParameter(GLenum pname, const FloatOrInt& param) {
  if (!ValidateSamplerParameterParams(mContext, pname, param)) return;

  bool invalidate = true;
  switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
      mState.minFilter = param.i;
      break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
      mState.magFilter = param.i;
      break;

    case LOCAL_GL_TEXTURE_WRAP_S:
      mState.wrapS = param.i;
      break;

    case LOCAL_GL_TEXTURE_WRAP_T:
      mState.wrapT = param.i;
      break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
      mState.compareMode = param.i;
      break;

    default:
      invalidate = false;
      break;
  }

  if (invalidate) {
    InvalidateCaches();
  }

  ////

  if (param.isFloat) {
    mContext->gl->fSamplerParameterf(mGLName, pname, param.f);
  } else {
    mContext->gl->fSamplerParameteri(mGLName, pname, param.i);
  }
}

}  // namespace mozilla
