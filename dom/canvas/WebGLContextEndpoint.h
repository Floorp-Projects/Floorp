/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXTENDPOINT_H_
#define WEBGLCONTEXTENDPOINT_H_

#include "mozilla/GfxMessageUtils.h"
#include "nsString.h"
#include "mozilla/dom/WebGLTypes.h"

namespace mozilla {

class ClientWebGLContext;
class HostWebGLContext;
class WebGLActiveInfo;
class WebGLBuffer;
class WebGLContext;
class WebGLQuery;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLTexture;
class WebGLVertexArray;

namespace dom {
class Float32ArrayOrUnrestrictedFloatSequence;
class Int32ArrayOrLongSequence;
class Uint32ArrayOrUnsignedLongSequence;
}

namespace layers {
class CanvasRenderer;
class Layer;
class LayerManager;
class WebRenderCanvasData;
}

/**
 * Pure virtual interface for host and client WebGLContext endpoints.
 */
class WebGLContextEndpoint {
 public:
  WebGLVersion GetVersion() { return mVersion; }

  // When we want to let the host know the "real" method that called it, we
  // pass one of these.  This information should only be used for diagnostic
  // purposes (e.g. warning and error messages)
  enum FuncScopeId {
    FuncScopeIdError = 0,
    compressedTexImage2D,
    compressedTexImage3D,
    compressedTexSubImage2D,
    compressedTexSubImage3D,
    copyTexSubImage2D,
    copyTexSubImage3D,
    drawElements,
    drawElementsInstanced,
    drawRangeElements,
    renderbufferStorage,
    renderbufferStorageMultisample,
    texImage2D,
    texImage3D,
    TexStorage2D,
    TexStorage3D,
    texSubImage2D,
    texSubImage3D,
    vertexAttrib1f,
    vertexAttrib1fv,
    vertexAttrib2f,
    vertexAttrib2fv,
    vertexAttrib3f,
    vertexAttrib3fv,
    vertexAttrib4f,
    vertexAttrib4fv,
    vertexAttribI4i,
    vertexAttribI4iv,
    vertexAttribI4ui,
    vertexAttribI4uiv,
    vertexAttribIPointer,
    vertexAttribPointer
  };

 protected:
  static const char* GetFuncScopeName(FuncScopeId aId) {
#define FUNC_SCOPE_NAME(_name) [FuncScopeId::_name] = #_name,
    static constexpr const char* const FuncScopeNames[] = {
        FUNC_SCOPE_NAME(compressedTexImage2D) FUNC_SCOPE_NAME(compressedTexImage3D) FUNC_SCOPE_NAME(
            compressedTexSubImage2D) FUNC_SCOPE_NAME(compressedTexSubImage3D)
            FUNC_SCOPE_NAME(copyTexSubImage2D) FUNC_SCOPE_NAME(copyTexSubImage3D) FUNC_SCOPE_NAME(
                drawElements) FUNC_SCOPE_NAME(drawElementsInstanced)
                FUNC_SCOPE_NAME(drawRangeElements) FUNC_SCOPE_NAME(
                    renderbufferStorage) FUNC_SCOPE_NAME(renderbufferStorageMultisample)
                    FUNC_SCOPE_NAME(texImage2D) FUNC_SCOPE_NAME(texImage3D) FUNC_SCOPE_NAME(
                        TexStorage2D) FUNC_SCOPE_NAME(TexStorage3D) FUNC_SCOPE_NAME(texSubImage2D)
                        FUNC_SCOPE_NAME(texSubImage3D) FUNC_SCOPE_NAME(vertexAttrib1f)
                            FUNC_SCOPE_NAME(vertexAttrib1fv) FUNC_SCOPE_NAME(vertexAttrib2f)
                                FUNC_SCOPE_NAME(vertexAttrib2fv) FUNC_SCOPE_NAME(vertexAttrib3f)
                                    FUNC_SCOPE_NAME(vertexAttrib3fv) FUNC_SCOPE_NAME(vertexAttrib4f)
                                        FUNC_SCOPE_NAME(vertexAttrib4fv) FUNC_SCOPE_NAME(
                                            vertexAttribI4i) FUNC_SCOPE_NAME(vertexAttribI4iv)
                                            FUNC_SCOPE_NAME(vertexAttribI4ui)
                                                FUNC_SCOPE_NAME(vertexAttribI4uiv)
                                                    FUNC_SCOPE_NAME(vertexAttribIPointer)
                                                        FUNC_SCOPE_NAME(vertexAttribPointer)};
#undef FUNC_SCOPE_NAME
    if (static_cast<uint32_t>(aId) < static_cast<uint32_t>(arraysize(FuncScopeNames))) {
      return FuncScopeNames[aId];
    }
    return "<Illegal FuncScope ID>";
  }

 protected:
  WebGLContextEndpoint(WebGLVersion aVersion) : mVersion(aVersion) {}

  virtual ~WebGLContextEndpoint(){};

  WebGLVersion mVersion;
};

}  // namespace mozilla

#endif  // WEBGLCONTEXTENDPOINT_H_
