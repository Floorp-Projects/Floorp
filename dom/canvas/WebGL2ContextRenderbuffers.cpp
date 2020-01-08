/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFormats.h"

namespace mozilla {

Maybe<nsTArray<int32_t>> WebGL2Context::GetInternalformatParameter(
    GLenum target, GLenum internalformat, GLenum pname) {
  const FuncScope funcScope(*this, "getInternalfomratParameter");
  if (IsContextLost()) return Nothing();

  if (target != LOCAL_GL_RENDERBUFFER) {
    ErrorInvalidEnum("`target` must be RENDERBUFFER.");
    return Nothing();
  }

  // GLES 3.0.4 $4.4.4 p212:
  // "An internal format is color-renderable if it is one of the formats from
  // table 3.13
  //  noted as color-renderable or if it is unsized format RGBA or RGB."

  GLenum sizedFormat;
  switch (internalformat) {
    case LOCAL_GL_RGB:
      sizedFormat = LOCAL_GL_RGB8;
      break;
    case LOCAL_GL_RGBA:
      sizedFormat = LOCAL_GL_RGBA8;
      break;
    default:
      sizedFormat = internalformat;
      break;
  }

  // In RenderbufferStorage, we allow DEPTH_STENCIL. Therefore, it is accepted
  // for internalformat as well. Please ignore the conformance test fail for
  // DEPTH_STENCIL.

  const auto usage = mFormatUsage->GetRBUsage(sizedFormat);
  if (!usage) {
    ErrorInvalidEnum(
        "`internalformat` must be color-, depth-, or stencil-renderable, was: "
        "0x%04x.",
        internalformat);
    return Nothing();
  }

  if (pname != LOCAL_GL_SAMPLES) {
    ErrorInvalidEnum("`pname` must be SAMPLES.");
    return Nothing();
  }

  nsTArray<int32_t> obj;
  GLint sampleCount = 0;
  gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat,
                           LOCAL_GL_NUM_SAMPLE_COUNTS, 1, &sampleCount);
  if (sampleCount > 0) {
    GLint* samples =
        static_cast<GLint*>(obj.AppendElements(sampleCount, fallible));
    // If we don't have 'samples' then we will return a zero-length array, which
    // will be interpreted by the ClientWebGLContext as an out-of-memory
    // condition.
    if (samples) {
      gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat,
                               LOCAL_GL_SAMPLES, sampleCount, samples);
    }
  }

  return Some(obj);
}

}  // namespace mozilla
