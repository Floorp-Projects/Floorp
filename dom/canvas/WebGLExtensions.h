/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_EXTENSIONS_H_
#define WEBGL_EXTENSIONS_H_

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

namespace dom {
template<typename T>
class Sequence;
} // namespace dom

namespace webgl {
class FormatUsageAuthority;
} // namespace webgl

class WebGLContext;
class WebGLShader;
class WebGLQuery;
class WebGLTimerQuery;
class WebGLVertexArray;

class WebGLExtensionBase
    : public nsWrapperCache
    , public WebGLContextBoundObject
{
public:
    explicit WebGLExtensionBase(WebGLContext* webgl);

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    void MarkLost();

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLExtensionBase)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLExtensionBase)

protected:
    virtual ~WebGLExtensionBase();

    bool mIsLost;
};

#define DECL_WEBGL_EXTENSION_GOOP \
    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

#define IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionType, WebGLBindingType)\
    JSObject*                                                    \
    WebGLExtensionType::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) {              \
        return dom::WebGLBindingType##Binding::Wrap(cx, this, givenProto); \
    }

class WebGLExtensionCompressedTextureATC
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionCompressedTextureATC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureATC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTextureETC1
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionCompressedTextureETC1(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureETC1();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTexturePVRTC
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionCompressedTexturePVRTC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTexturePVRTC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionCompressedTextureS3TC
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionCompressedTextureS3TC(WebGLContext*);
    virtual ~WebGLExtensionCompressedTextureS3TC();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDebugRendererInfo
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionDebugRendererInfo(WebGLContext*);
    virtual ~WebGLExtensionDebugRendererInfo();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDebugShaders
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionDebugShaders(WebGLContext*);
    virtual ~WebGLExtensionDebugShaders();

    void GetTranslatedShaderSource(WebGLShader* shader, nsAString& retval);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDepthTexture
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionDepthTexture(WebGLContext*);
    virtual ~WebGLExtensionDepthTexture();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionElementIndexUint
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionElementIndexUint(WebGLContext*);
    virtual ~WebGLExtensionElementIndexUint();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionFragDepth
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionFragDepth(WebGLContext*);
    virtual ~WebGLExtensionFragDepth();

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionLoseContext
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionLoseContext(WebGLContext*);
    virtual ~WebGLExtensionLoseContext();

    void LoseContext();
    void RestoreContext();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionSRGB
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionSRGB(WebGLContext*);
    virtual ~WebGLExtensionSRGB();

    static bool IsSupported(const WebGLContext* context);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionStandardDerivatives
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionStandardDerivatives(WebGLContext*);
    virtual ~WebGLExtensionStandardDerivatives();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionShaderTextureLod
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionShaderTextureLod(WebGLContext*);
    virtual ~WebGLExtensionShaderTextureLod();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFilterAnisotropic
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionTextureFilterAnisotropic(WebGLContext*);
    virtual ~WebGLExtensionTextureFilterAnisotropic();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFloat
    : public WebGLExtensionBase
{
public:
    static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

    explicit WebGLExtensionTextureFloat(WebGLContext*);
    virtual ~WebGLExtensionTextureFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureFloatLinear
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionTextureFloatLinear(WebGLContext*);
    virtual ~WebGLExtensionTextureFloatLinear();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureHalfFloat
    : public WebGLExtensionBase
{
public:
    static void InitWebGLFormats(webgl::FormatUsageAuthority* authority);

    explicit WebGLExtensionTextureHalfFloat(WebGLContext*);
    virtual ~WebGLExtensionTextureHalfFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionTextureHalfFloatLinear
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionTextureHalfFloatLinear(WebGLContext*);
    virtual ~WebGLExtensionTextureHalfFloatLinear();

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionColorBufferFloat
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionColorBufferFloat(WebGLContext*);
    virtual ~WebGLExtensionColorBufferFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionColorBufferHalfFloat
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionColorBufferHalfFloat(WebGLContext*);
    virtual ~WebGLExtensionColorBufferHalfFloat();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDrawBuffers
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionDrawBuffers(WebGLContext*);
    virtual ~WebGLExtensionDrawBuffers();

    void DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers);

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionVertexArray
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionVertexArray(WebGLContext* webgl);
    virtual ~WebGLExtensionVertexArray();

    already_AddRefed<WebGLVertexArray> CreateVertexArrayOES();
    void DeleteVertexArrayOES(WebGLVertexArray* array);
    bool IsVertexArrayOES(WebGLVertexArray* array);
    void BindVertexArrayOES(WebGLVertexArray* array);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionInstancedArrays
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionInstancedArrays(WebGLContext* webgl);
    virtual ~WebGLExtensionInstancedArrays();

    void DrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count,
                                  GLsizei primcount);
    void DrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type,
                                    WebGLintptr offset, GLsizei primcount);
    void VertexAttribDivisorANGLE(GLuint index, GLuint divisor);

    static bool IsSupported(const WebGLContext* webgl);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionBlendMinMax
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionBlendMinMax(WebGLContext* webgl);
    virtual ~WebGLExtensionBlendMinMax();

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP
};

class WebGLExtensionDisjointTimerQuery
    : public WebGLExtensionBase
{
public:
    explicit WebGLExtensionDisjointTimerQuery(WebGLContext* webgl);
    virtual ~WebGLExtensionDisjointTimerQuery();

    already_AddRefed<WebGLTimerQuery> CreateQueryEXT();
    void DeleteQueryEXT(WebGLTimerQuery* query);
    bool IsQueryEXT(WebGLTimerQuery* query);
    void BeginQueryEXT(GLenum target, WebGLTimerQuery* query);
    void EndQueryEXT(GLenum target);
    void QueryCounterEXT(WebGLTimerQuery* query, GLenum target);
    void GetQueryEXT(JSContext *cx, GLenum target, GLenum pname,
                     JS::MutableHandle<JS::Value> retval);
    void GetQueryObjectEXT(JSContext *cx, WebGLTimerQuery* query,
                           GLenum pname,
                           JS::MutableHandle<JS::Value> retval);

    static bool IsSupported(const WebGLContext*);

    DECL_WEBGL_EXTENSION_GOOP

private:
    /**
     * An active TIME_ELAPSED query participating in a begin/end block.
     */
    WebGLRefPtr<WebGLTimerQuery> mActiveQuery;
};

} // namespace mozilla

#endif // WEBGL_EXTENSIONS_H_
