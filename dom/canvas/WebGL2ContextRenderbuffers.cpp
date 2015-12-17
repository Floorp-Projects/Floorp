/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLContextUtils.h"

namespace mozilla {

void
WebGL2Context::GetInternalformatParameter(JSContext* cx, GLenum target,
                                          GLenum internalformat, GLenum pname,
                                          JS::MutableHandleValue retval,
                                          ErrorResult& rv)
{
  if (IsContextLost())
    return;

  if (target != LOCAL_GL_RENDERBUFFER) {
    return ErrorInvalidEnumInfo("getInternalfomratParameter: target must be "
                                "RENDERBUFFER. Was:", target);
  }

  // GL_INVALID_ENUM is generated if internalformat is not color-,
  // depth-, or stencil-renderable.
  // TODO: When format table queries lands.

  if (pname != LOCAL_GL_SAMPLES) {
    return ErrorInvalidEnumInfo("getInternalformatParameter: pname must be SAMPLES. "
                                "Was:", pname);
  }

  GLint* samples = nullptr;
  GLint sampleCount = 0;
  gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat,
                           LOCAL_GL_NUM_SAMPLE_COUNTS, 1, &sampleCount);
  if (sampleCount > 0) {
    samples = new GLint[sampleCount];
    gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat, LOCAL_GL_SAMPLES,
                             sampleCount, samples);
  }

  JSObject* obj = dom::Int32Array::Create(cx, this, sampleCount, samples);
  if (!obj) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  delete[] samples;

  retval.setObjectOrNull(obj);
}

void
WebGL2Context::RenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                              GLenum internalFormat,
                                              GLsizei width, GLsizei height)
{
  const char funcName[] = "renderbufferStorageMultisample";
  if (IsContextLost())
    return;

  //RenderbufferStorage_base(funcName, target, samples, internalFormat, width, height);

  ErrorInvalidOperation("%s: Multisampling is still under development, and is currently"
                        " disabled.", funcName);
}

} // namespace mozilla
