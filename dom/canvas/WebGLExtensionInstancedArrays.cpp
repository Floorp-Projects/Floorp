/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionInstancedArrays::WebGLExtensionInstancedArrays(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionInstancedArrays::~WebGLExtensionInstancedArrays() {}

void WebGLExtensionInstancedArrays::DrawArraysInstancedANGLE(
    GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
  if (mIsLost) {
    if (mContext) {
      mContext->ErrorInvalidOperation("%s: Extension is lost.",
                                      "drawArraysInstancedANGLE");
    }
    return;
  }

  mContext->DrawArraysInstanced(mode, first, count, primcount);
}

void WebGLExtensionInstancedArrays::DrawElementsInstancedANGLE(
    GLenum mode, GLsizei count, GLenum type, WebGLintptr offset,
    GLsizei primcount) {
  if (mIsLost) {
    if (mContext) {
      mContext->ErrorInvalidOperation("%s: Extension is lost.",
                                      "drawElementsInstancedANGLE");
    }
    return;
  }

  mContext->DrawElementsInstanced(mode, count, type, offset, primcount);
}

void WebGLExtensionInstancedArrays::VertexAttribDivisorANGLE(GLuint index,
                                                             GLuint divisor) {
  if (mIsLost) {
    if (mContext) {
      mContext->ErrorInvalidOperation("%s: Extension is lost.",
                                      "vertexAttribDivisorANGLE");
    }
    return;
  }

  mContext->VertexAttribDivisor(index, divisor);
}

bool WebGLExtensionInstancedArrays::IsSupported(const WebGLContext* webgl) {
  if (webgl->IsWebGL2()) return false;

  gl::GLContext* gl = webgl->GL();
  return gl->IsSupported(gl::GLFeature::draw_instanced) &&
         gl->IsSupported(gl::GLFeature::instanced_arrays);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionInstancedArrays, ANGLE_instanced_arrays)

}  // namespace mozilla
