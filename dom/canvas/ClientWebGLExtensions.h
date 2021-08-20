/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CLIENTWEBGLEXTENSIONS_H_
#define CLIENTWEBGLEXTENSIONS_H_

#include "WebGLExtensions.h"
#include "ClientWebGLContext.h"

namespace mozilla {

class ClientWebGLExtensionBase : public nsWrapperCache {
  friend ClientWebGLContext;

 protected:
  WeakPtr<ClientWebGLContext> mContext;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ClientWebGLExtensionBase)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ClientWebGLExtensionBase)

 protected:
  explicit ClientWebGLExtensionBase(ClientWebGLContext& context)
      : mContext(&context) {}
  virtual ~ClientWebGLExtensionBase() = default;

 public:
  ClientWebGLContext* GetParentObject() const { return mContext.get(); }
};

// -

// To be used for implementations of ClientWebGLExtensionBase
#define DEFINE_WEBGL_EXTENSION_GOOP(_WebGLBindingType, _Extension)             \
  JSObject* Client##_Extension::WrapObject(JSContext* cx,                      \
                                           JS::Handle<JSObject*> givenProto) { \
    return dom::_WebGLBindingType##_Binding::Wrap(cx, this, givenProto);       \
  }                                                                            \
  Client##_Extension::Client##_Extension(ClientWebGLContext& aClient)          \
      : ClientWebGLExtensionBase(aClient) {}

// Many extensions have no methods.  This is a shorthand for declaring client
// versions of such classes.
#define DECLARE_SIMPLE_WEBGL_EXTENSION(_Extension)                           \
  class Client##_Extension : public ClientWebGLExtensionBase {               \
   public:                                                                   \
    virtual JSObject* WrapObject(JSContext* cx,                              \
                                 JS::Handle<JSObject*> givenProto) override; \
    explicit Client##_Extension(ClientWebGLContext&);                        \
  };

////

class ClientWebGLExtensionCompressedTextureASTC
    : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionCompressedTextureASTC(ClientWebGLContext&);

  void GetSupportedProfiles(dom::Nullable<nsTArray<nsString>>& retval) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("getSupportedProfiles: Extension is `invalidated`.");
      return;
    }
    mContext->GetSupportedProfilesASTC(retval);
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
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionDebugShaders(ClientWebGLContext&);

  void GetTranslatedShaderSource(const WebGLShaderJS& shader,
                                 nsAString& retval) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("getTranslatedShaderSource: Extension is `invalidated`.");
      return;
    }
    mContext->GetTranslatedShaderSource(shader, retval);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionDepthTexture)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionElementIndexUint)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionEXTColorBufferFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionFragDepth)

class ClientWebGLExtensionLoseContext : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionLoseContext(ClientWebGLContext&);

  void LoseContext() {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("loseContext: Extension is `invalidated`.");
      return;
    }
    mContext->EmulateLoseContext();
  }
  void RestoreContext() {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("restoreContext: Extension is `invalidated`.");
      return;
    }
    mContext->RestoreContext(webgl::LossStatus::LostManually);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionSRGB)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionStandardDerivatives)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionShaderTextureLod)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFilterAnisotropic)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureFloatLinear)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureHalfFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureHalfFloatLinear)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionTextureNorm16)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionColorBufferFloat)

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionColorBufferHalfFloat)

class ClientWebGLExtensionDrawBuffers : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionDrawBuffers(ClientWebGLContext&);

  void DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("drawBuffersWEBGL: Extension is `invalidated`.");
      return;
    }
    mContext->DrawBuffers(buffers);
  }
};

class ClientWebGLExtensionVertexArray : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionVertexArray(ClientWebGLContext&);

  already_AddRefed<WebGLVertexArrayJS> CreateVertexArrayOES() {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("createVertexArrayOES: Extension is `invalidated`.");
      return nullptr;
    }
    return mContext->CreateVertexArray();
  }
  void DeleteVertexArrayOES(WebGLVertexArrayJS* array) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("deleteVertexArrayOES: Extension is `invalidated`.");
      return;
    }
    mContext->DeleteVertexArray(array);
  }
  bool IsVertexArrayOES(const WebGLVertexArrayJS* array) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("isVertexArrayOES: Extension is `invalidated`.");
      return false;
    }
    return mContext->IsVertexArray(array);
  }
  void BindVertexArrayOES(WebGLVertexArrayJS* array) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("bindVertexArrayOES: Extension is `invalidated`.");
      return;
    }
    mContext->BindVertexArray(array);
  }
};

class ClientWebGLExtensionInstancedArrays : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionInstancedArrays(ClientWebGLContext&);

  void DrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count,
                                GLsizei primcount) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("drawArraysInstancedANGLE: Extension is `invalidated`.");
      return;
    }
    mContext->DrawArraysInstanced(mode, first, count, primcount);
  }
  void DrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type,
                                  WebGLintptr offset, GLsizei primcount) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("drawElementsInstancedANGLE: Extension is `invalidated`.");
      return;
    }
    mContext->DrawElementsInstanced(mode, count, type, offset, primcount,
                                    FuncScopeId::drawElementsInstanced);
  }
  void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("vertexAttribDivisorANGLE: Extension is `invalidated`.");
      return;
    }
    mContext->VertexAttribDivisor(index, divisor);
  }
};

DECLARE_SIMPLE_WEBGL_EXTENSION(WebGLExtensionBlendMinMax)

class ClientWebGLExtensionDisjointTimerQuery : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionDisjointTimerQuery(ClientWebGLContext&);

  already_AddRefed<WebGLQueryJS> CreateQueryEXT() const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("createQueryEXT: Extension is `invalidated`.");
      return nullptr;
    }
    return mContext->CreateQuery();
  }
  void DeleteQueryEXT(WebGLQueryJS* query) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("deleteQueryEXT: Extension is `invalidated`.");
      return;
    }
    mContext->DeleteQuery(query);
  }
  bool IsQueryEXT(const WebGLQueryJS* query) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("isQueryEXT: Extension is `invalidated`.");
      return false;
    }
    return mContext->IsQuery(query);
  }
  void BeginQueryEXT(GLenum target, WebGLQueryJS& query) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("beginQueryEXT: Extension is `invalidated`.");
      return;
    }
    mContext->BeginQuery(target, query);
  }
  void EndQueryEXT(GLenum target) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("endQueryEXT: Extension is `invalidated`.");
      return;
    }
    mContext->EndQuery(target);
  }
  void QueryCounterEXT(WebGLQueryJS& query, GLenum target) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("queryCounterEXT: Extension is `invalidated`.");
      return;
    }
    mContext->QueryCounter(query, target);
  }
  void GetQueryEXT(JSContext* cx, GLenum target, GLenum pname,
                   JS::MutableHandle<JS::Value> retval) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("getQueryEXT: Extension is `invalidated`.");
      return;
    }
    mContext->GetQuery(cx, target, pname, retval);
  }
  void GetQueryObjectEXT(JSContext* cx, WebGLQueryJS& query, GLenum pname,
                         JS::MutableHandle<JS::Value> retval) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("getQueryObjectEXT: Extension is `invalidated`.");
      return;
    }
    mContext->GetQueryParameter(cx, query, pname, retval);
  }
};

class ClientWebGLExtensionExplicitPresent : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionExplicitPresent(ClientWebGLContext&);

  void Present() const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("present: Extension is `invalidated`.");
      return;
    }
    mContext->OnBeforePaintTransaction();
  }
};

class ClientWebGLExtensionMOZDebug : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionMOZDebug(ClientWebGLContext&);

  void GetParameter(JSContext* cx, GLenum pname,
                    JS::MutableHandle<JS::Value> retval,
                    ErrorResult& er) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning("getParameter: Extension is `invalidated`.");
      return;
    }
    mContext->MOZDebugGetParameter(cx, pname, retval, er);
  }
};

class ClientWebGLExtensionMultiview : public ClientWebGLExtensionBase {
 public:
  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;
  explicit ClientWebGLExtensionMultiview(ClientWebGLContext&);

  void FramebufferTextureMultiviewOVR(const GLenum target,
                                      const GLenum attachment,
                                      WebGLTextureJS* const texture,
                                      const GLint level,
                                      const GLint baseViewIndex,
                                      const GLsizei numViews) const {
    if (MOZ_UNLIKELY(!mContext)) {
      AutoJsWarning(
          "framebufferTextureMultiviewOVR: Extension is `invalidated`.");
      return;
    }
    mContext->FramebufferTextureMultiview(target, attachment, texture, level,
                                          baseViewIndex, numViews);
  }
};

}  // namespace mozilla

#endif  // CLIENTWEBGLEXTENSIONS_H_
