/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CLIENTWEBGLEXTENSIONS_H_
#define CLIENTWEBGLEXTENSIONS_H_

#include "WebGLExtensions.h"
#include "ClientWebGLContext.h"

namespace mozilla {

/**
 * The ClientWebGLExtension... classes back the JS Extension classes.  They
 * direct their calls to the ClientWebGLContext, adding a boolean first
 * parameter, set to true, to indicate that an extension was the origin of
 * the call.
 */
class ClientWebGLExtensionBase : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ClientWebGLExtensionBase)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ClientWebGLExtensionBase)

  ClientWebGLExtensionBase(RefPtr<ClientWebGLContext> aClient)
      : mContext(aClient) {}

  ClientWebGLContext* GetParentObject() const { return mContext; }

 protected:
  friend ClientWebGLContext;
  virtual ~ClientWebGLExtensionBase() {}
  RefPtr<ClientWebGLContext> mContext;
};

// To be used for implementations of ClientWebGLExtensionBase
#define DECLARE_WEBGL_EXTENSION_GOOP(_Extension)                           \
 protected:                                                                \
  virtual ~Client##_Extension() {}                                         \
                                                                           \
 public:                                                                   \
  virtual JSObject* WrapObject(JSContext* cx,                              \
                               JS::Handle<JSObject*> givenProto) override; \
  Client##_Extension(RefPtr<ClientWebGLContext> aClient);

// To be used for implementations of ClientWebGLExtensionBase
#define DEFINE_WEBGL_EXTENSION_GOOP(_WebGLBindingType, _Extension)             \
  JSObject* Client##_Extension::WrapObject(JSContext* cx,                      \
                                           JS::Handle<JSObject*> givenProto) { \
    return dom::_WebGLBindingType##_Binding::Wrap(cx, this, givenProto);       \
  }                                                                            \
  Client##_Extension::Client##_Extension(RefPtr<ClientWebGLContext> aClient)   \
      : ClientWebGLExtensionBase(aClient) {}

// Many extensions have no methods.  This is a shorthand for declaring client
// versions of such classes.
#define DECLARE_SIMPLE_WEBGL_EXTENSION(_Extension)                           \
  class Client##_Extension : public ClientWebGLExtensionBase {               \
   protected:                                                                \
    virtual ~Client##_Extension() {}                                         \
                                                                             \
   public:                                                                   \
    virtual JSObject* WrapObject(JSContext* cx,                              \
                                 JS::Handle<JSObject*> givenProto) override; \
    Client##_Extension(RefPtr<ClientWebGLContext> aClient);                  \
  };

////

class ClientWebGLExtensionCompressedTextureASTC
    : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionCompressedTextureASTC)

  void GetSupportedProfiles(dom::Nullable<nsTArray<nsString> >& retval) const {
    mContext->GetASTCExtensionSupportedProfiles(retval);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionFloatBlend)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureBPTC)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureES3)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureETC1)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTexturePVRTC)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureRGTC)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionFBORenderMipmap)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureS3TC)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionCompressedTextureS3TC_SRGB)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionDebugRendererInfo)

class ClientWebGLExtensionDebugShaders : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionDebugShaders)

  void GetTranslatedShaderSource(const ClientWebGLShader& shader,
                                 nsAString& retval) const {
    mContext->GetTranslatedShaderSource(shader, retval);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionDepthTexture)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionElementIndexUint)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionEXTColorBufferFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionFragDepth)

class ClientWebGLExtensionLoseContext : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionLoseContext)

  void LoseContext() { mContext->LoseContext(); }
  void RestoreContext() { mContext->RestoreContext(); }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionSRGB)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionStandardDerivatives)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionShaderTextureLod)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFilterAnisotropic)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFloatLinear)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureHalfFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureHalfFloatLinear)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionColorBufferFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionColorBufferHalfFloat)

class ClientWebGLExtensionDrawBuffers : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionDrawBuffers)

  void DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers) {
    mContext->DrawBuffers(buffers, true);
  }
};

class ClientWebGLExtensionVertexArray : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionVertexArray)

  already_AddRefed<ClientWebGLVertexArray> CreateVertexArrayOES() {
    return mContext->CreateVertexArray(true);
  }
  void DeleteVertexArrayOES(ClientWebGLVertexArray* array) {
    mContext->DeleteVertexArray(array, true);
  }
  bool IsVertexArrayOES(const ClientWebGLVertexArray* array) {
    return mContext->IsVertexArray(array, true);
  }
  void BindVertexArrayOES(ClientWebGLVertexArray* array) {
    mContext->BindVertexArray(array, true);
  }
};

class ClientWebGLExtensionInstancedArrays : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionInstancedArrays)

  void DrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count,
                                GLsizei primcount) {
    mContext->DrawArraysInstanced(mode, first, count, primcount, true);
  }
  void DrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type,
                                  WebGLintptr offset, GLsizei primcount) {
    mContext->DrawElementsInstanced(mode, count, type, offset, primcount,
                                    WebGLContextEndpoint::drawElementsInstanced,
                                    true);
  }
  void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    mContext->VertexAttribDivisor(index, divisor, true);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionBlendMinMax)

class ClientWebGLExtensionDisjointTimerQuery : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionDisjointTimerQuery)

  already_AddRefed<ClientWebGLQuery> CreateQueryEXT() const {
    return mContext->CreateQuery(true);
  }
  void DeleteQueryEXT(ClientWebGLQuery* query) const {
    mContext->DeleteQuery(query, true);
  }
  bool IsQueryEXT(const ClientWebGLQuery* query) const {
    return mContext->IsQuery(query, true);
  }
  void BeginQueryEXT(GLenum target, ClientWebGLQuery& query) const {
    mContext->BeginQuery(target, query, true);
  }
  void EndQueryEXT(GLenum target) const { mContext->EndQuery(target, true); }
  void QueryCounterEXT(ClientWebGLQuery& query, GLenum target) const {
    mContext->QueryCounter(query, target);
  }
  void GetQueryEXT(JSContext* cx, GLenum target, GLenum pname,
                   JS::MutableHandleValue retval) const {
    mContext->GetQuery(cx, target, pname, retval, true);
  }
  void GetQueryObjectEXT(JSContext* cx, const ClientWebGLQuery& query,
                         GLenum pname, JS::MutableHandleValue retval) const {
    mContext->GetQueryParameter(cx, query, pname, retval, true);
  }
};

class ClientWebGLExtensionMOZDebug : public ClientWebGLExtensionBase {
  DECLARE_WEBGL_EXTENSION_GOOP(WebGLExtensionMOZDebug)

  void GetParameter(JSContext* cx, GLenum pname,
                    JS::MutableHandle<JS::Value> retval,
                    ErrorResult& er) const {
    mContext->MOZDebugGetParameter(cx, pname, retval, er);
  }
};

}  // namespace mozilla

#endif  // CLIENTWEBGLEXTENSIONS_H_
