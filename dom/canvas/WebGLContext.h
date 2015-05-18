/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXT_H_
#define WEBGLCONTEXT_H_

#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/LinkedList.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"

#include "GLDefs.h"
#include "WebGLActiveInfo.h"
#include "WebGLContextUnchecked.h"
#include "WebGLObjectModel.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLShaderValidator.h"
#include "WebGLStrongTypes.h"
#include <stdarg.h>

#include "nsTArray.h"
#include "nsCycleCollectionNoteChild.h"

#include "nsIDOMWebGLRenderingContext.h"
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsWrapperCache.h"
#include "nsIObserver.h"
#include "nsIDOMEventListener.h"
#include "nsLayoutUtils.h"

#include "GLContextProvider.h"

#include "mozilla/gfx/2D.h"

#ifdef XP_MACOSX
#include "ForceDiscreteGPUHelperCGL.h"
#endif

#include "mozilla/dom/TypedArray.h"
#include "mozilla/ErrorResult.h"

class nsIDocShell;

/*
 * Minimum value constants defined in 6.2 State Tables of OpenGL ES - 2.0.25
 *   https://bugzilla.mozilla.org/show_bug.cgi?id=686732
 *
 * Exceptions: some of the following values are set to higher values than in the spec because
 * the values in the spec are ridiculously low. They are explicitly marked below
 */
#define MINVALUE_GL_MAX_TEXTURE_SIZE                  1024  // Different from the spec, which sets it to 64 on page 162
#define MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE         512   // Different from the spec, which sets it to 16 on page 162
#define MINVALUE_GL_MAX_VERTEX_ATTRIBS                8     // Page 164
#define MINVALUE_GL_MAX_FRAGMENT_UNIFORM_VECTORS      16    // Page 164
#define MINVALUE_GL_MAX_VERTEX_UNIFORM_VECTORS        128   // Page 164
#define MINVALUE_GL_MAX_VARYING_VECTORS               8     // Page 164
#define MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS           8     // Page 164
#define MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS    0     // Page 164
#define MINVALUE_GL_MAX_RENDERBUFFER_SIZE             1024  // Different from the spec, which sets it to 1 on page 164
#define MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS  8     // Page 164

namespace mozilla {

class WebGLActiveInfo;
class WebGLContextLossHandler;
class WebGLBuffer;
class WebGLExtensionBase;
class WebGLFramebuffer;
class WebGLObserver;
class WebGLProgram;
class WebGLQuery;
class WebGLRenderbuffer;
class WebGLSampler;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLTexture;
class WebGLTransformFeedback;
class WebGLUniformLocation;
class WebGLVertexArray;

namespace dom {
class Element;
class ImageData;
struct WebGLContextAttributes;
template<typename> struct Nullable;
}

namespace gfx {
class SourceSurface;
}

namespace webgl {
struct LinkedProgramInfo;
}

WebGLTexelFormat GetWebGLTexelFormat(TexInternalFormat format);

void AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow);

struct WebGLContextOptions
{
    // these are defaults
    WebGLContextOptions();

    bool operator==(const WebGLContextOptions& other) const {
        return
            alpha == other.alpha &&
            depth == other.depth &&
            stencil == other.stencil &&
            premultipliedAlpha == other.premultipliedAlpha &&
            antialias == other.antialias &&
            preserveDrawingBuffer == other.preserveDrawingBuffer;
    }

    bool operator!=(const WebGLContextOptions& other) const {
        return !operator==(other);
    }

    bool alpha;
    bool depth;
    bool stencil;
    bool premultipliedAlpha;
    bool antialias;
    bool preserveDrawingBuffer;
};

// From WebGLContextUtils
TexTarget TexImageTargetToTexTarget(TexImageTarget texImageTarget);

class WebGLIntOrFloat {
    enum {
        Int,
        Float,
        Uint
    } mType;
    union {
        GLint i;
        GLfloat f;
        GLuint u;
    } mValue;

public:
    explicit WebGLIntOrFloat(GLint i) : mType(Int) { mValue.i = i; }
    explicit WebGLIntOrFloat(GLfloat f) : mType(Float) { mValue.f = f; }

    GLint AsInt() const { return (mType == Int) ? mValue.i : NS_lroundf(mValue.f); }
    GLfloat AsFloat() const { return (mType == Float) ? mValue.f : GLfloat(mValue.i); }
};

class WebGLContext
    : public nsIDOMWebGLRenderingContext
    , public nsICanvasRenderingContextInternal
    , public nsSupportsWeakReference
    , public WebGLContextUnchecked
    , public WebGLRectangleObject
    , public nsWrapperCache
    , public SupportsWeakPtr<WebGLContext>
{
    friend class WebGL2Context;
    friend class WebGLContextUserData;
    friend class WebGLExtensionCompressedTextureATC;
    friend class WebGLExtensionCompressedTextureETC1;
    friend class WebGLExtensionCompressedTexturePVRTC;
    friend class WebGLExtensionCompressedTextureS3TC;
    friend class WebGLExtensionDepthTexture;
    friend class WebGLExtensionDrawBuffers;
    friend class WebGLExtensionLoseContext;
    friend class WebGLExtensionVertexArray;
    friend class WebGLMemoryTracker;
    friend class WebGLObserver;

    enum {
        UNPACK_FLIP_Y_WEBGL = 0x9240,
        UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241,
        CONTEXT_LOST_WEBGL = 0x9242,
        UNPACK_COLORSPACE_CONVERSION_WEBGL = 0x9243,
        BROWSER_DEFAULT_WEBGL = 0x9244,
        UNMASKED_VENDOR_WEBGL = 0x9245,
        UNMASKED_RENDERER_WEBGL = 0x9246
    };

public:
    WebGLContext();

    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebGLContext)

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(WebGLContext,
                                                           nsIDOMWebGLRenderingContext)

    virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

    NS_DECL_NSIDOMWEBGLRENDERINGCONTEXT

    // nsICanvasRenderingContextInternal
#ifdef DEBUG
    virtual int32_t GetWidth() const override;
    virtual int32_t GetHeight() const override;
#endif
    NS_IMETHOD SetDimensions(int32_t width, int32_t height) override;
    NS_IMETHOD InitializeWithSurface(nsIDocShell*, gfxASurface*, int32_t,
                                     int32_t) override
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Reset() override {
        /* (InitializeWithSurface) */
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    virtual void GetImageBuffer(uint8_t** out_imageBuffer,
                                int32_t* out_format) override;
    NS_IMETHOD GetInputStream(const char* mimeType,
                              const char16_t* encoderOptions,
                              nsIInputStream** out_stream) override;

    mozilla::TemporaryRef<mozilla::gfx::SourceSurface>
    GetSurfaceSnapshot(bool* out_premultAlpha) override;

    NS_IMETHOD SetIsOpaque(bool) override { return NS_OK; };
    bool GetIsOpaque() override { return false; }
    NS_IMETHOD SetContextOptions(JSContext* cx,
                                 JS::Handle<JS::Value> options) override;

    NS_IMETHOD SetIsIPC(bool) override {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * An abstract base class to be implemented by callers wanting to be notified
     * that a refresh has occurred. Callers must ensure an observer is removed
     * before it is destroyed.
     */
    virtual void DidRefresh() override;

    NS_IMETHOD Redraw(const gfxRect&) override {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    void SynthesizeGLError(GLenum err);
    void SynthesizeGLError(GLenum err, const char* fmt, ...);

    void ErrorInvalidEnum(const char* fmt = 0, ...);
    void ErrorInvalidOperation(const char* fmt = 0, ...);
    void ErrorInvalidValue(const char* fmt = 0, ...);
    void ErrorInvalidFramebufferOperation(const char* fmt = 0, ...);
    void ErrorInvalidEnumInfo(const char* info, GLenum enumValue);
    void ErrorInvalidEnumInfo(const char* info, const char* funcName,
                              GLenum enumValue);
    void ErrorOutOfMemory(const char* fmt = 0, ...);

    const char* ErrorName(GLenum error);

    /**
     * Return displayable name for GLenum.
     * This version is like gl::GLenumToStr but with out the GL_ prefix to
     * keep consistency with how errors are reported from WebGL.
     */
    // Returns nullptr if glenum is unknown.
    static const char* EnumName(GLenum glenum);
    // Returns hex formatted version of glenum if glenum is unknown.
    static void EnumName(GLenum glenum, nsACString* out_name);

    bool IsCompressedTextureFormat(GLenum format);
    bool IsTextureFormatCompressed(TexInternalFormat format);

    void DummyFramebufferOperation(const char* funcName);

    WebGLTexture* ActiveBoundTextureForTarget(const TexTarget texTarget) const {
        switch (texTarget.get()) {
        case LOCAL_GL_TEXTURE_2D:
            return mBound2DTextures[mActiveTexture];
        case LOCAL_GL_TEXTURE_CUBE_MAP:
            return mBoundCubeMapTextures[mActiveTexture];
        case LOCAL_GL_TEXTURE_3D:
            return mBound3DTextures[mActiveTexture];
        default:
            MOZ_CRASH("bad target");
        }
    }

    /* Use this function when you have the texture image target, for example:
     * GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_[POSITIVE|NEGATIVE]_[X|Y|Z], and
     * not the actual texture binding target: GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
     */
    WebGLTexture*
    ActiveBoundTextureForTexImageTarget(const TexImageTarget texImgTarget) const
    {
        const TexTarget texTarget = TexImageTargetToTexTarget(texImgTarget);
        return ActiveBoundTextureForTarget(texTarget);
    }

    already_AddRefed<CanvasLayer>
    GetCanvasLayer(nsDisplayListBuilder* builder, CanvasLayer* oldLayer,
                   LayerManager* manager) override;

    // Note that 'clean' here refers to its invalidation state, not the
    // contents of the buffer.
    void MarkContextClean() override { mInvalidated = false; }

    gl::GLContext* GL() const { return gl; }

    bool IsPremultAlpha() const { return mOptions.premultipliedAlpha; }

    bool IsPreservingDrawingBuffer() const { return mOptions.preserveDrawingBuffer; }

    bool PresentScreenBuffer();

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    uint32_t Generation() { return mGeneration.value(); }

    // Returns null if the current bound FB is not likely complete.
    const WebGLRectangleObject* CurValidDrawFBRectObject() const;
    const WebGLRectangleObject* CurValidReadFBRectObject() const;

    static const size_t kMaxColorAttachments = 16;

    // This is similar to GLContext::ClearSafely, but tries to minimize the
    // amount of work it does.
    // It only clears the buffers we specify, and can reset its state without
    // first having to query anything, as WebGL knows its state at all times.
    void ForceClearFramebufferWithDefaultValues(bool fakeNoAlpha, GLbitfield mask,
                                                const bool colorAttachmentsMask[kMaxColorAttachments]);

    // Calls ForceClearFramebufferWithDefaultValues() for the Context's 'screen'.
    void ClearScreen();
    void ClearBackbufferIfNeeded();

    bool MinCapabilityMode() const { return mMinCapability; }

    void RunContextLossTimer();
    void UpdateContextLossStatus();
    void EnqueueUpdateContextLossStatus();

    bool TryToRestoreContext();

    void AssertCachedBindings();
    void AssertCachedState();

    // WebIDL WebGLRenderingContext API
    dom::HTMLCanvasElement* GetCanvas() const { return mCanvasElement; }
    GLsizei DrawingBufferWidth() const { return IsContextLost() ? 0 : mWidth; }
    GLsizei DrawingBufferHeight() const {
        return IsContextLost() ? 0 : mHeight;
    }

    void
    GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval);

    bool IsContextLost() const { return mContextStatus != ContextNotLost; }
    void GetSupportedExtensions(JSContext* cx,
                                dom::Nullable< nsTArray<nsString> >& retval);
    void GetExtension(JSContext* cx, const nsAString& name,
                      JS::MutableHandle<JSObject*> retval, ErrorResult& rv);
    void ActiveTexture(GLenum texture);
    void AttachShader(WebGLProgram* prog, WebGLShader* shader);
    void BindAttribLocation(WebGLProgram* prog, GLuint location,
                            const nsAString& name);
    void BindFramebuffer(GLenum target, WebGLFramebuffer* fb);
    void BindRenderbuffer(GLenum target, WebGLRenderbuffer* fb);
    void BindTexture(GLenum target, WebGLTexture* tex);
    void BindVertexArray(WebGLVertexArray* vao);
    void BlendColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
    void BlendEquation(GLenum mode);
    void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
    void BlendFunc(GLenum sfactor, GLenum dfactor);
    void BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                           GLenum srcAlpha, GLenum dstAlpha);
    GLenum CheckFramebufferStatus(GLenum target);
    void Clear(GLbitfield mask);
    void ClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
    void ClearDepth(GLclampf v);
    void ClearStencil(GLint v);
    void ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b, WebGLboolean a);
    void CompileShader(WebGLShader* shader);
    void CompileShaderANGLE(WebGLShader* shader);
    void CompileShaderBypass(WebGLShader* shader, const nsCString& shaderSource);
    void CompressedTexImage2D(GLenum target, GLint level,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, GLint border,
                              const dom::ArrayBufferView& view);
    void CompressedTexSubImage2D(GLenum texImageTarget, GLint level,
                                 GLint xoffset, GLint yoffset, GLsizei width,
                                 GLsizei height, GLenum format,
                                 const dom::ArrayBufferView& view);
    void CopyTexImage2D(GLenum texImageTarget, GLint level,
                        GLenum internalformat, GLint x, GLint y, GLsizei width,
                        GLsizei height, GLint border);
    void CopyTexSubImage2D(GLenum texImageTarget, GLint level, GLint xoffset,
                           GLint yoffset, GLint x, GLint y, GLsizei width,
                           GLsizei height);
    already_AddRefed<WebGLFramebuffer> CreateFramebuffer();
    already_AddRefed<WebGLProgram> CreateProgram();
    already_AddRefed<WebGLRenderbuffer> CreateRenderbuffer();
    already_AddRefed<WebGLTexture> CreateTexture();
    already_AddRefed<WebGLShader> CreateShader(GLenum type);
    already_AddRefed<WebGLVertexArray> CreateVertexArray();
    void CullFace(GLenum face);
    void DeleteFramebuffer(WebGLFramebuffer* fb);
    void DeleteProgram(WebGLProgram* prog);
    void DeleteRenderbuffer(WebGLRenderbuffer* rb);
    void DeleteShader(WebGLShader* shader);
    void DeleteVertexArray(WebGLVertexArray* vao);
    void DeleteTexture(WebGLTexture* tex);
    void DepthFunc(GLenum func);
    void DepthMask(WebGLboolean b);
    void DepthRange(GLclampf zNear, GLclampf zFar);
    void DetachShader(WebGLProgram* prog, WebGLShader* shader);
    void DrawBuffers(const dom::Sequence<GLenum>& buffers);
    void Flush();
    void Finish();
    void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                                 GLenum rbTarget, WebGLRenderbuffer* rb);
    void FramebufferTexture2D(GLenum target, GLenum attachment,
                              GLenum texImageTarget, WebGLTexture* tex,
                              GLint level);

    // Framebuffer validation
    bool ValidateFramebufferAttachment(const WebGLFramebuffer* fb,
                                       GLenum attachment, const char* funcName);

    void FrontFace(GLenum mode);
    void GenerateMipmap(GLenum target);
    already_AddRefed<WebGLActiveInfo> GetActiveAttrib(WebGLProgram* prog,
                                                      GLuint index);
    already_AddRefed<WebGLActiveInfo> GetActiveUniform(WebGLProgram* prog,
                                                       GLuint index);

    void
    GetAttachedShaders(WebGLProgram* prog,
                       dom::Nullable<nsTArray<nsRefPtr<WebGLShader>>>& retval);

    GLint GetAttribLocation(WebGLProgram* prog, const nsAString& name);
    JS::Value GetBufferParameter(GLenum target, GLenum pname);

    void GetBufferParameter(JSContext*, GLenum target, GLenum pname,
                            JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetBufferParameter(target, pname));
    }

    GLenum GetError();
    JS::Value GetFramebufferAttachmentParameter(JSContext* cx, GLenum target,
                                                GLenum attachment, GLenum pname,
                                                ErrorResult& rv);

    void GetFramebufferAttachmentParameter(JSContext* cx, GLenum target,
                                           GLenum attachment, GLenum pname,
                                           JS::MutableHandle<JS::Value> retval,
                                           ErrorResult& rv)
    {
        retval.set(GetFramebufferAttachmentParameter(cx, target, attachment,
                                                     pname, rv));
    }

    JS::Value GetProgramParameter(WebGLProgram* prog, GLenum pname);

    void  GetProgramParameter(JSContext*, WebGLProgram* prog, GLenum pname,
                              JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetProgramParameter(prog, pname));
    }

    void GetProgramInfoLog(WebGLProgram* prog, nsACString& retval);
    void GetProgramInfoLog(WebGLProgram* prog, nsAString& retval);
    JS::Value GetRenderbufferParameter(GLenum target, GLenum pname);

    void GetRenderbufferParameter(JSContext*, GLenum target, GLenum pname,
                                  JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetRenderbufferParameter(target, pname));
    }

    JS::Value GetShaderParameter(WebGLShader* shader, GLenum pname);

    void GetShaderParameter(JSContext*, WebGLShader* shader, GLenum pname,
                            JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetShaderParameter(shader, pname));
    }

    already_AddRefed<WebGLShaderPrecisionFormat>
    GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype);

    void GetShaderInfoLog(WebGLShader* shader, nsACString& retval);
    void GetShaderInfoLog(WebGLShader* shader, nsAString& retval);
    void GetShaderSource(WebGLShader* shader, nsAString& retval);
    void GetShaderTranslatedSource(WebGLShader* shader, nsAString& retval);
    JS::Value GetTexParameter(GLenum target, GLenum pname);

    void GetTexParameter(JSContext*, GLenum target, GLenum pname,
                         JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetTexParameter(target, pname));
    }

    JS::Value GetUniform(JSContext* cx, WebGLProgram* prog,
                         WebGLUniformLocation* loc);

    void GetUniform(JSContext* cx, WebGLProgram* prog,
                    WebGLUniformLocation* loc,
                    JS::MutableHandle<JS::Value> retval)
    {
        retval.set(GetUniform(cx, prog, loc));
    }

    already_AddRefed<WebGLUniformLocation>
    GetUniformLocation(WebGLProgram* prog, const nsAString& name);

    void Hint(GLenum target, GLenum mode);
    bool IsFramebuffer(WebGLFramebuffer* fb);
    bool IsProgram(WebGLProgram* prog);
    bool IsRenderbuffer(WebGLRenderbuffer* rb);
    bool IsShader(WebGLShader* shader);
    bool IsTexture(WebGLTexture* tex);
    bool IsVertexArray(WebGLVertexArray* vao);
    void LineWidth(GLfloat width);
    void LinkProgram(WebGLProgram* prog);
    void PixelStorei(GLenum pname, GLint param);
    void PolygonOffset(GLfloat factor, GLfloat units);
    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                    GLenum format, GLenum type,
                    const Nullable<dom::ArrayBufferView>& pixels,
                    ErrorResult& rv);
    void RenderbufferStorage(GLenum target, GLenum internalFormat,
                             GLsizei width, GLsizei height);
protected:
    void RenderbufferStorage_base(const char* funcName, GLenum target,
                                  GLsizei samples, GLenum internalformat,
                                  GLsizei width, GLsizei height);
public:
    void SampleCoverage(GLclampf value, WebGLboolean invert);
    void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void ShaderSource(WebGLShader* shader, const nsAString& source);
    void StencilFunc(GLenum func, GLint ref, GLuint mask);
    void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
    void StencilMask(GLuint mask);
    void StencilMaskSeparate(GLenum face, GLuint mask);
    void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
    void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                           GLenum dppass);
    void TexImage2D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                    GLsizei width, GLsizei height, GLint border, GLenum format,
                    GLenum type, const Nullable<dom::ArrayBufferView>& pixels,
                    ErrorResult& rv);
    void TexImage2D(GLenum texImageTarget, GLint level, GLenum internalFormat,
                    GLenum format, GLenum type, dom::ImageData* pixels,
                    ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexImage2D
    bool TexImageFromVideoElement(TexImageTarget texImageTarget, GLint level,
                                  GLenum internalFormat, GLenum format,
                                  GLenum type, mozilla::dom::Element& image);

    template<class ElementType>
    void TexImage2D(GLenum rawTexImageTarget, GLint level,
                    GLenum internalFormat, GLenum format, GLenum type,
                    ElementType& elt, ErrorResult& rv)
    {
        if (IsContextLost())
            return;

        if (!ValidateTexImageTarget(rawTexImageTarget,
                                    WebGLTexImageFunc::TexImage,
                                    WebGLTexDimensions::Tex2D))
        {
            ErrorInvalidEnumInfo("texSubImage2D: target", rawTexImageTarget);
            return;
        }

        const TexImageTarget texImageTarget(rawTexImageTarget);

        if (level < 0)
            return ErrorInvalidValue("texImage2D: level is negative");

        const int32_t maxLevel = MaxTextureLevelForTexImageTarget(texImageTarget);
        if (level > maxLevel) {
            ErrorInvalidValue("texImage2D: level %d is too large, max is %d",
                              level, maxLevel);
            return;
        }

        WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

        if (!tex)
            return ErrorInvalidOperation("no texture is bound to this target");

        // Trying to handle the video by GPU directly first
        if (TexImageFromVideoElement(texImageTarget, level, internalFormat,
                                     format, type, elt))
        {
            return;
        }

        RefPtr<gfx::DataSourceSurface> data;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
        if (rv.Failed() || !data)
            return;

        gfx::IntSize size = data->GetSize();
        uint32_t byteLength = data->Stride() * size.height;
        return TexImage2D_base(texImageTarget, level, internalFormat,
                               size.width, size.height, data->Stride(), 0,
                               format, type, data->GetData(), byteLength,
                               js::Scalar::MaxTypedArrayViewType, srcFormat,
                               mPixelStorePremultiplyAlpha);
    }

    void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
        TexParameter_base(target, pname, nullptr, &param);
    }
    void TexParameteri(GLenum target, GLenum pname, GLint param) {
        TexParameter_base(target, pname, &param, nullptr);
    }

    void TexSubImage2D(GLenum texImageTarget, GLint level, GLint xoffset,
                       GLint yoffset, GLsizei width, GLsizei height,
                       GLenum format, GLenum type,
                       const Nullable<dom::ArrayBufferView>& pixels,
                       ErrorResult& rv);
    void TexSubImage2D(GLenum texImageTarget, GLint level, GLint xoffset,
                       GLint yoffset, GLenum format, GLenum type,
                       dom::ImageData* pixels, ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexSubImage2D
    template<class ElementType>
    void TexSubImage2D(GLenum rawTexImageTarget, GLint level, GLint xoffset,
                       GLint yoffset, GLenum format, GLenum type,
                       ElementType& elt, ErrorResult& rv)
    {
        // TODO: Consolidate all the parameter validation
        // checks. Instead of spreading out the cheks in multple
        // places, consolidate into one spot.

        if (IsContextLost())
            return;

        if (!ValidateTexImageTarget(rawTexImageTarget,
                                    WebGLTexImageFunc::TexSubImage,
                                    WebGLTexDimensions::Tex2D))
        {
            ErrorInvalidEnumInfo("texSubImage2D: target", rawTexImageTarget);
            return;
        }

        const TexImageTarget texImageTarget(rawTexImageTarget);

        if (level < 0)
            return ErrorInvalidValue("texSubImage2D: level is negative");

        const int32_t maxLevel = MaxTextureLevelForTexImageTarget(texImageTarget);
        if (level > maxLevel) {
            ErrorInvalidValue("texSubImage2D: level %d is too large, max is %d",
                              level, maxLevel);
            return;
        }

        WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
        if (!tex)
            return ErrorInvalidOperation("texSubImage2D: no texture bound on active texture unit");

        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
        const TexInternalFormat internalFormat = imageInfo.EffectiveInternalFormat();

        // Trying to handle the video by GPU directly first
        if (TexImageFromVideoElement(texImageTarget, level,
                                     internalFormat.get(), format, type, elt))
        {
            return;
        }

        RefPtr<gfx::DataSourceSurface> data;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, data, &srcFormat);
        if (rv.Failed() || !data)
            return;

        gfx::IntSize size = data->GetSize();
        uint32_t byteLength = data->Stride() * size.height;
        return TexSubImage2D_base(texImageTarget.get(), level, xoffset, yoffset,
                                  size.width, size.height, data->Stride(),
                                  format, type, data->GetData(), byteLength,
                                  js::Scalar::MaxTypedArrayViewType, srcFormat,
                                  mPixelStorePremultiplyAlpha);
    }

    void Uniform1i(WebGLUniformLocation* loc, GLint x);
    void Uniform2i(WebGLUniformLocation* loc, GLint x, GLint y);
    void Uniform3i(WebGLUniformLocation* loc, GLint x, GLint y, GLint z);
    void Uniform4i(WebGLUniformLocation* loc, GLint x, GLint y, GLint z,
                   GLint w);

    void Uniform1f(WebGLUniformLocation* loc, GLfloat x);
    void Uniform2f(WebGLUniformLocation* loc, GLfloat x, GLfloat y);
    void Uniform3f(WebGLUniformLocation* loc, GLfloat x, GLfloat y, GLfloat z);
    void Uniform4f(WebGLUniformLocation* loc, GLfloat x, GLfloat y, GLfloat z,
                   GLfloat w);

    // Int array
    void Uniform1iv(WebGLUniformLocation* loc, const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform1iv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform1iv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLint>& arr)
    {
        Uniform1iv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform1iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLint* data);

    void Uniform2iv(WebGLUniformLocation* loc, const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform2iv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform2iv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLint>& arr)
    {
        Uniform2iv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform2iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLint* data);

    void Uniform3iv(WebGLUniformLocation* loc, const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform3iv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform3iv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLint>& arr)
    {
        Uniform3iv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform3iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLint* data);

    void Uniform4iv(WebGLUniformLocation* loc, const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform4iv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform4iv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLint>& arr)
    {
        Uniform4iv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform4iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLint* data);

    // Float array
    void Uniform1fv(WebGLUniformLocation* loc, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform1fv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform1fv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLfloat>& arr)
    {
        Uniform1fv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform1fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLfloat* data);

    void Uniform2fv(WebGLUniformLocation* loc, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform2fv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform2fv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLfloat>& arr)
    {
        Uniform2fv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform2fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLfloat* data);

    void Uniform3fv(WebGLUniformLocation* loc, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform3fv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform3fv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLfloat>& arr)
    {
        Uniform3fv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform3fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLfloat* data);

    void Uniform4fv(WebGLUniformLocation* loc, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform4fv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform4fv(WebGLUniformLocation* loc,
                    const dom::Sequence<GLfloat>& arr)
    {
        Uniform4fv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform4fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                         const GLfloat* data);

    // Matrix
    void UniformMatrix2fv(WebGLUniformLocation* loc, WebGLboolean transpose,
                          const dom::Float32Array& value)
    {
        value.ComputeLengthAndData();
        UniformMatrix2fv_base(loc, transpose, value.Length(), value.Data());
    }
    void UniformMatrix2fv(WebGLUniformLocation* loc, WebGLboolean transpose,
                          const dom::Sequence<float>& value)
    {
        UniformMatrix2fv_base(loc, transpose, value.Length(),
                              value.Elements());
    }
    void UniformMatrix2fv_base(WebGLUniformLocation* loc, bool transpose,
                               size_t arrayLength, const float* data);

    void UniformMatrix3fv(WebGLUniformLocation* loc, WebGLboolean transpose,
                          const dom::Float32Array& value)
    {
        value.ComputeLengthAndData();
        UniformMatrix3fv_base(loc, transpose, value.Length(), value.Data());
    }
    void UniformMatrix3fv(WebGLUniformLocation* loc, WebGLboolean transpose,
                          const dom::Sequence<float>& value)
    {
        UniformMatrix3fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix3fv_base(WebGLUniformLocation* loc, bool transpose,
                               size_t arrayLength, const float* data);

    void UniformMatrix4fv(WebGLUniformLocation* loc, WebGLboolean transpose,
                          const dom::Float32Array& value)
    {
        value.ComputeLengthAndData();
        UniformMatrix4fv_base(loc, transpose, value.Length(), value.Data());
    }
    void UniformMatrix4fv(WebGLUniformLocation* loc, bool transpose,
                          const dom::Sequence<float>& value)
    {
        UniformMatrix4fv_base(loc, transpose, value.Length(),
                              value.Elements());
    }
    void UniformMatrix4fv_base(WebGLUniformLocation* loc, bool transpose,
                               size_t arrayLength, const float* data);

    void UseProgram(WebGLProgram* prog);

    bool ValidateAttribArraySetter(const char* name, uint32_t count,
                                   uint32_t arrayLength);
    bool ValidateUniformLocation(WebGLUniformLocation* loc, const char* funcName);
    bool ValidateUniformSetter(WebGLUniformLocation* loc, uint8_t setterSize,
                               GLenum setterType, const char* info,
                               GLuint* out_rawLoc);
    bool ValidateUniformArraySetter(WebGLUniformLocation* loc,
                                    uint8_t setterElemSize, GLenum setterType,
                                    size_t setterArraySize, const char* info,
                                    GLuint* out_rawLoc,
                                    GLsizei* out_numElementsToUpload);
    bool ValidateUniformMatrixArraySetter(WebGLUniformLocation* loc,
                                          uint8_t setterCols,
                                          uint8_t setterRows,
                                          GLenum setterType,
                                          size_t setterArraySize,
                                          bool setterTranspose,
                                          const char* info,
                                          GLuint* out_rawLoc,
                                          GLsizei* out_numElementsToUpload);
    void ValidateProgram(WebGLProgram* prog);
    bool ValidateUniformLocation(const char* info, WebGLUniformLocation* loc);
    bool ValidateSamplerUniformSetter(const char* info,
                                      WebGLUniformLocation* loc, GLint value);
    void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
// -----------------------------------------------------------------------------
// WEBGL_lose_context
public:
    void LoseContext();
    void RestoreContext();

// -----------------------------------------------------------------------------
// Buffer Objects (WebGLContextBuffers.cpp)
private:
    void UpdateBoundBuffer(GLenum target, WebGLBuffer* buffer);
    void UpdateBoundBufferIndexed(GLenum target, GLuint index, WebGLBuffer* buffer);

public:
    void BindBuffer(GLenum target, WebGLBuffer* buffer);
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buf);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buf,
                         WebGLintptr offset, WebGLsizeiptr size);

private:
    void BufferDataUnchecked(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
    void BufferData(GLenum target, WebGLsizeiptr size, void* data, GLenum usage);

public:
    void BufferData(GLenum target, WebGLsizeiptr size, GLenum usage);
    void BufferData(GLenum target, const dom::ArrayBufferView& data,
                    GLenum usage);
    void BufferData(GLenum target, const Nullable<dom::ArrayBuffer>& maybeData,
                    GLenum usage);

private:
    void BufferSubDataUnchecked(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
    void BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);

public:
    void BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                       const dom::ArrayBufferView& data);
    void BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                       const Nullable<dom::ArrayBuffer>& maybeData);
    already_AddRefed<WebGLBuffer> CreateBuffer();
    void DeleteBuffer(WebGLBuffer* buf);
    bool IsBuffer(WebGLBuffer* buf);

protected:
    // bound buffer state
    WebGLRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundCopyReadBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundCopyWriteBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundPixelPackBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundPixelUnpackBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundTransformFeedbackBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundUniformBuffer;

    nsTArray<WebGLRefPtr<WebGLBuffer>> mBoundUniformBuffers;
    nsTArray<WebGLRefPtr<WebGLBuffer>> mBoundTransformFeedbackBuffers;

    WebGLRefPtr<WebGLBuffer>& GetBufferSlotByTarget(GLenum target);
    WebGLRefPtr<WebGLBuffer>& GetBufferSlotByTargetIndexed(GLenum target,
                                                           GLuint index);

// -----------------------------------------------------------------------------
// Queries (WebGL2ContextQueries.cpp)
protected:
    WebGLRefPtr<WebGLQuery>& GetQuerySlotByTarget(GLenum target);

    WebGLRefPtr<WebGLQuery> mActiveOcclusionQuery;
    WebGLRefPtr<WebGLQuery> mActiveTransformFeedbackQuery;

// -----------------------------------------------------------------------------
// State and State Requests (WebGLContextState.cpp)
public:
    void Disable(GLenum cap);
    void Enable(GLenum cap);
    bool GetStencilBits(GLint* out_stencilBits);
    JS::Value GetParameter(JSContext* cx, GLenum pname, ErrorResult& rv);

    void GetParameter(JSContext* cx, GLenum pname,
                      JS::MutableHandle<JS::Value> retval, ErrorResult& rv)
    {
        retval.set(GetParameter(cx, pname, rv));
    }

    void GetParameterIndexed(JSContext* cx, GLenum pname, GLuint index,
                             JS::MutableHandle<JS::Value> retval);
    bool IsEnabled(GLenum cap);

private:
    // State tracking slots
    realGLboolean mDitherEnabled;
    realGLboolean mRasterizerDiscardEnabled;
    realGLboolean mScissorTestEnabled;
    realGLboolean mStencilTestEnabled;

    bool ValidateCapabilityEnum(GLenum cap, const char* info);
    realGLboolean* GetStateTrackingSlot(GLenum cap);

// -----------------------------------------------------------------------------
// Vertices Feature (WebGLContextVertices.cpp)
public:
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                             GLsizei primcount);
    void DrawElements(GLenum mode, GLsizei count, GLenum type,
                      WebGLintptr byteOffset);
    void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                               WebGLintptr byteOffset, GLsizei primcount);

    void EnableVertexAttribArray(GLuint index);
    void DisableVertexAttribArray(GLuint index);

    JS::Value GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                              ErrorResult& rv);

    void GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                         JS::MutableHandle<JS::Value> retval, ErrorResult& rv)
    {
        retval.set(GetVertexAttrib(cx, index, pname, rv));
    }

    WebGLsizeiptr GetVertexAttribOffset(GLuint index, GLenum pname);

    void VertexAttrib1f(GLuint index, GLfloat x0);
    void VertexAttrib2f(GLuint index, GLfloat x0, GLfloat x1);
    void VertexAttrib3f(GLuint index, GLfloat x0, GLfloat x1, GLfloat x2);
    void VertexAttrib4f(GLuint index, GLfloat x0, GLfloat x1, GLfloat x2,
                        GLfloat x3);

    void VertexAttrib1fv(GLuint idx, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        VertexAttrib1fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib1fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib1fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib2fv(GLuint idx, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        VertexAttrib2fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib2fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib2fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib3fv(GLuint idx, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        VertexAttrib3fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib3fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib3fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib4fv(GLuint idx, const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        VertexAttrib4fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib4fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib4fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                             WebGLboolean normalized, GLsizei stride,
                             WebGLintptr byteOffset);
    void VertexAttribDivisor(GLuint index, GLuint divisor);

private:
    // Cache the max number of vertices and instances that can be read from
    // bound VBOs (result of ValidateBuffers).
    bool mBufferFetchingIsVerified;
    bool mBufferFetchingHasPerVertex;
    uint32_t mMaxFetchedVertices;
    uint32_t mMaxFetchedInstances;

    bool DrawArrays_check(GLint first, GLsizei count, GLsizei primcount,
                          const char* info);
    bool DrawElements_check(GLsizei count, GLenum type, WebGLintptr byteOffset,
                            GLsizei primcount, const char* info,
                            GLuint* out_upperBound);
    bool DrawInstanced_check(const char* info);
    void Draw_cleanup();

    void VertexAttrib1fv_base(GLuint index, uint32_t arrayLength,
                              const GLfloat* ptr);
    void VertexAttrib2fv_base(GLuint index, uint32_t arrayLength,
                              const GLfloat* ptr);
    void VertexAttrib3fv_base(GLuint index, uint32_t arrayLength,
                              const GLfloat* ptr);
    void VertexAttrib4fv_base(GLuint index, uint32_t arrayLength,
                              const GLfloat* ptr);

    bool ValidateBufferFetching(const char* info);
    bool BindArrayAttribToLocation0(WebGLProgram* prog);

// -----------------------------------------------------------------------------
// PROTECTED
protected:
    virtual ~WebGLContext();

    void SetFakeBlackStatus(WebGLContextFakeBlackStatus x) {
        mFakeBlackStatus = x;
    }
    // Returns the current fake-black-status, except if it was Unknown,
    // in which case this function resolves it first, so it never returns Unknown.
    WebGLContextFakeBlackStatus ResolvedFakeBlackStatus();

    void BindFakeBlackTextures();
    void UnbindFakeBlackTextures();

    WebGLVertexAttrib0Status WhatDoesVertexAttrib0Need();
    bool DoFakeVertexAttrib0(GLuint vertexCount);
    void UndoFakeVertexAttrib0();

    static CheckedUint32 GetImageSize(GLsizei height, GLsizei width,
                                      GLsizei depth, uint32_t pixelSize,
                                      uint32_t alignment);

    virtual JS::Value GetTexParameterInternal(const TexTarget& target,
                                              GLenum pname);

    // Returns x rounded to the next highest multiple of y.
    static CheckedUint32 RoundedToNextMultipleOf(CheckedUint32 x, CheckedUint32 y) {
        return ((x + y - 1) / y) * y;
    }

    inline void InvalidateBufferFetching()
    {
        mBufferFetchingIsVerified = false;
        mBufferFetchingHasPerVertex = false;
        mMaxFetchedVertices = 0;
        mMaxFetchedInstances = 0;
    }

    CheckedUint32 mGeneration;

    WebGLContextOptions mOptions;

    bool mInvalidated;
    bool mResetLayer;
    bool mOptionsFrozen;
    bool mMinCapability;
    bool mDisableExtensions;
    bool mIsMesa;
    bool mLoseContextOnMemoryPressure;
    bool mCanLoseContextInForeground;
    bool mRestoreWhenVisible;
    bool mShouldPresent;
    bool mBackbufferNeedsClear;
    bool mDisableFragHighP;

    template<typename WebGLObjectType>
    void DeleteWebGLObjectsArray(nsTArray<WebGLObjectType>& array);

    GLuint mActiveTexture;

    // glGetError sources:
    bool mEmitContextLostErrorOnce;
    GLenum mWebGLError;
    GLenum mUnderlyingGLError;
    GLenum GetAndFlushUnderlyingGLErrors();

    bool mBypassShaderValidation;

    webgl::ShaderValidator* CreateShaderValidator(GLenum shaderType) const;

    // some GL constants
    int32_t mGLMaxVertexAttribs;
    int32_t mGLMaxTextureUnits;
    int32_t mGLMaxTextureSize;
    int32_t mGLMaxTextureSizeLog2;
    int32_t mGLMaxCubeMapTextureSize;
    int32_t mGLMaxCubeMapTextureSizeLog2;
    int32_t mGLMaxRenderbufferSize;
    int32_t mGLMaxTextureImageUnits;
    int32_t mGLMaxVertexTextureImageUnits;
    int32_t mGLMaxVaryingVectors;
    int32_t mGLMaxFragmentUniformVectors;
    int32_t mGLMaxVertexUniformVectors;
    int32_t mGLMaxColorAttachments;
    int32_t mGLMaxDrawBuffers;
    uint32_t  mGLMaxTransformFeedbackSeparateAttribs;
    GLuint  mGLMaxUniformBufferBindings;
    GLsizei mGLMaxSamples;

public:
    GLuint MaxVertexAttribs() const {
        return mGLMaxVertexAttribs;
    }

    GLuint GLMaxTextureUnits() const {
        return mGLMaxTextureUnits;
    }


    bool IsFormatValidForFB(GLenum sizedFormat) const;

protected:
    // Represents current status of the context with respect to context loss.
    // That is, whether the context is lost, and what part of the context loss
    // process we currently are at.
    // This is used to support the WebGL spec's asyncronous nature in handling
    // context loss.
    enum ContextStatus {
        // The context is stable; there either are none or we don't know of any.
        ContextNotLost,
        // The context has been lost, but we have not yet sent an event to the
        // script informing it of this.
        ContextLostAwaitingEvent,
        // The context has been lost, and we have sent the script an event
        // informing it of this.
        ContextLost,
        // The context is lost, an event has been sent to the script, and the
        // script correctly handled the event. We are waiting for the context to
        // be restored.
        ContextLostAwaitingRestore
    };

    // -------------------------------------------------------------------------
    // WebGL extensions (implemented in WebGLContextExtensions.cpp)
    typedef EnumeratedArray<WebGLExtensionID, WebGLExtensionID::Max,
                            nsRefPtr<WebGLExtensionBase>> ExtensionsArrayType;

    ExtensionsArrayType mExtensions;

    // enable an extension. the extension should not be enabled before.
    void EnableExtension(WebGLExtensionID ext);

    // Enable an extension if it's supported. Return the extension on success.
    WebGLExtensionBase* EnableSupportedExtension(JSContext* js,
                                                 WebGLExtensionID ext);

    // returns true if the extension has been enabled by calling getExtension.
    bool IsExtensionEnabled(WebGLExtensionID ext) const;

    // returns true if the extension is supported for this JSContext (this decides what getSupportedExtensions exposes)
    bool IsExtensionSupported(JSContext* cx, WebGLExtensionID ext) const;
    bool IsExtensionSupported(WebGLExtensionID ext) const;

    static const char* GetExtensionString(WebGLExtensionID ext);

    nsTArray<GLenum> mCompressedTextureFormats;

    // -------------------------------------------------------------------------
    // WebGL 2 specifics (implemented in WebGL2Context.cpp)
public:
    virtual bool IsWebGL2() const = 0;

protected:
    bool InitWebGL2();

    // -------------------------------------------------------------------------
    // Validation functions (implemented in WebGLContextValidate.cpp)
    bool CreateOffscreenGL(bool forceEnabled);
    bool InitAndValidateGL();
    bool ResizeBackbuffer(uint32_t width, uint32_t height);
    bool ValidateBlendEquationEnum(GLenum cap, const char* info);
    bool ValidateBlendFuncDstEnum(GLenum mode, const char* info);
    bool ValidateBlendFuncSrcEnum(GLenum mode, const char* info);
    bool ValidateBlendFuncEnumsCompatibility(GLenum sfactor, GLenum dfactor,
                                             const char* info);
    bool ValidateDataOffsetSize(WebGLintptr offset, WebGLsizeiptr size, WebGLsizeiptr bufferSize, const char* info);
    bool ValidateDataRanges(WebGLintptr readOffset, WebGLintptr writeOffset, WebGLsizeiptr size, const char* info);
    bool ValidateTextureTargetEnum(GLenum target, const char* info);
    bool ValidateComparisonEnum(GLenum target, const char* info);
    bool ValidateStencilOpEnum(GLenum action, const char* info);
    bool ValidateFaceEnum(GLenum face, const char* info);
    bool ValidateTexInputData(GLenum type, js::Scalar::Type jsArrayType,
                              WebGLTexImageFunc func, WebGLTexDimensions dims);
    bool ValidateDrawModeEnum(GLenum mode, const char* info);
    bool ValidateAttribIndex(GLuint index, const char* info);
    bool ValidateAttribPointer(bool integerMode, GLuint index, GLint size, GLenum type,
                               WebGLboolean normalized, GLsizei stride,
                               WebGLintptr byteOffset, const char* info);
    bool ValidateStencilParamsForDrawCall();

    bool ValidateCopyTexImage(GLenum internalFormat, WebGLTexImageFunc func,
                              WebGLTexDimensions dims);

    bool ValidateSamplerParameterName(GLenum pname, const char* info);
    bool ValidateSamplerParameterParams(GLenum pname, const WebGLIntOrFloat& param, const char* info);

    bool ValidateTexImage(TexImageTarget texImageTarget,
                          GLint level, GLenum internalFormat,
                          GLint xoffset, GLint yoffset, GLint zoffset,
                          GLint width, GLint height, GLint depth,
                          GLint border, GLenum format, GLenum type,
                          WebGLTexImageFunc func, WebGLTexDimensions dims);
    bool ValidateTexImageTarget(GLenum texImageTarget, WebGLTexImageFunc func,
                                WebGLTexDimensions dims);
    bool ValidateTexImageFormat(GLenum internalFormat, WebGLTexImageFunc func,
                                WebGLTexDimensions dims);
    bool ValidateTexImageType(GLenum type, WebGLTexImageFunc func,
                              WebGLTexDimensions dims);
    bool ValidateTexImageFormatAndType(GLenum format, GLenum type,
                                       WebGLTexImageFunc func,
                                       WebGLTexDimensions dims);
    bool ValidateCompTexImageInternalFormat(GLenum format,
                                            WebGLTexImageFunc func,
                                            WebGLTexDimensions dims);
    bool ValidateCopyTexImageInternalFormat(GLenum format,
                                            WebGLTexImageFunc func,
                                            WebGLTexDimensions dims);
    bool ValidateTexImageSize(TexImageTarget texImageTarget, GLint level,
                              GLint width, GLint height, GLint depth,
                              WebGLTexImageFunc func, WebGLTexDimensions dims);
    bool ValidateTexSubImageSize(GLint x, GLint y, GLint z, GLsizei width,
                                 GLsizei height, GLsizei depth,
                                 GLsizei baseWidth, GLsizei baseHeight,
                                 GLsizei baseDepth, WebGLTexImageFunc func,
                                 WebGLTexDimensions dims);
    bool ValidateCompTexImageSize(GLint level, GLenum internalFormat,
                                  GLint xoffset, GLint yoffset, GLsizei width,
                                  GLsizei height, GLsizei levelWidth,
                                  GLsizei levelHeight, WebGLTexImageFunc func,
                                  WebGLTexDimensions dims);
    bool ValidateCompTexImageDataSize(GLint level, GLenum internalFormat,
                                      GLsizei width, GLsizei height,
                                      uint32_t byteLength,
                                      WebGLTexImageFunc func,
                                      WebGLTexDimensions dims);

    bool ValidateUniformLocationForProgram(WebGLUniformLocation* location,
                                           WebGLProgram* program,
                                           const char* funcName);

    void Invalidate();
    void DestroyResourcesAndContext();

    void MakeContextCurrent() const;

    // helpers

    // If jsArrayType is MaxTypedArrayViewType, it means no array.
    void TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLsizei width,
                         GLsizei height, GLsizei srcStrideOrZero, GLint border,
                         GLenum format, GLenum type, void* data,
                         uint32_t byteLength, js::Scalar::Type jsArrayType,
                         WebGLTexelFormat srcFormat, bool srcPremultiplied);
    void TexSubImage2D_base(GLenum texImageTarget, GLint level,
                            GLint xoffset, GLint yoffset, GLsizei width,
                            GLsizei height, GLsizei srcStrideOrZero,
                            GLenum format, GLenum type, void* pixels,
                            uint32_t byteLength, js::Scalar::Type jsArrayType,
                            WebGLTexelFormat srcFormat, bool srcPremultiplied);

    void TexParameter_base(GLenum target, GLenum pname,
                           GLint* const out_intParam,
                           GLfloat* const out_floatParam);

    bool ConvertImage(size_t width, size_t height, size_t srcStride,
                      size_t dstStride, const uint8_t* src, uint8_t* dst,
                      WebGLTexelFormat srcFormat, bool srcPremultiplied,
                      WebGLTexelFormat dstFormat, bool dstPremultiplied,
                      size_t dstTexelSize);

    template<class ElementType>
    nsLayoutUtils::SurfaceFromElementResult
    SurfaceFromElement(ElementType* element) {
        MOZ_ASSERT(element);

        uint32_t flags = nsLayoutUtils::SFE_WANT_IMAGE_SURFACE;
        if (mPixelStoreColorspaceConversion == LOCAL_GL_NONE)
            flags |= nsLayoutUtils::SFE_NO_COLORSPACE_CONVERSION;
        if (!mPixelStorePremultiplyAlpha)
            flags |= nsLayoutUtils::SFE_PREFER_NO_PREMULTIPLY_ALPHA;

        return nsLayoutUtils::SurfaceFromElement(element, flags);
    }

    template<class ElementType>
    nsLayoutUtils::SurfaceFromElementResult
    SurfaceFromElement(ElementType& element) {
       return SurfaceFromElement(&element);
    }

    nsresult
    SurfaceFromElementResultToImageSurface(nsLayoutUtils::SurfaceFromElementResult& res,
                                           RefPtr<gfx::DataSourceSurface>& imageOut,
                                           WebGLTexelFormat* format);

    void CopyTexSubImage2D_base(TexImageTarget texImageTarget,
                                GLint level, TexInternalFormat internalFormat,
                                GLint xoffset, GLint yoffset, GLint x, GLint y,
                                GLsizei width, GLsizei height, bool isSub);

    // Returns false if `object` is null or not valid.
    template<class ObjectType>
    bool ValidateObject(const char* info, ObjectType* object);

    // Returns false if `object` is not valid.  Considers null to be valid.
    template<class ObjectType>
    bool ValidateObjectAllowNull(const char* info, ObjectType* object);

    // Returns false if `object` is not valid, but considers deleted objects and
    // null objects valid.
    template<class ObjectType>
    bool ValidateObjectAllowDeletedOrNull(const char* info, ObjectType* object);

    // Returns false if `object` is null or not valid, but considers deleted
    // objects valid.
    template<class ObjectType>
    bool ValidateObjectAllowDeleted(const char* info, ObjectType* object);

private:
    // Like ValidateObject, but only for cases when `object` is known to not be
    // null already.
    template<class ObjectType>
    bool ValidateObjectAssumeNonNull(const char* info, ObjectType* object);

private:
    // -------------------------------------------------------------------------
    // Context customization points
    virtual bool ValidateAttribPointerType(bool integerMode, GLenum type, GLsizei* alignment, const char* info) = 0;
    virtual bool ValidateBufferTarget(GLenum target, const char* info) = 0;
    virtual bool ValidateBufferIndexedTarget(GLenum target, const char* info) = 0;
    virtual bool ValidateBufferForTarget(GLenum target, WebGLBuffer* buffer, const char* info) = 0;
    virtual bool ValidateBufferUsageEnum(GLenum usage, const char* info) = 0;
    virtual bool ValidateQueryTarget(GLenum usage, const char* info) = 0;
    virtual bool ValidateUniformMatrixTranspose(bool transpose, const char* info) = 0;

protected:
    int32_t MaxTextureSizeForTarget(TexTarget target) const {
        return (target == LOCAL_GL_TEXTURE_2D) ? mGLMaxTextureSize
                                               : mGLMaxCubeMapTextureSize;
    }

    int32_t
    MaxTextureLevelForTexImageTarget(TexImageTarget texImageTarget) const {
        const TexTarget target = TexImageTargetToTexTarget(texImageTarget);
        return (target == LOCAL_GL_TEXTURE_2D) ? mGLMaxTextureSizeLog2
                                               : mGLMaxCubeMapTextureSizeLog2;
    }

    /** Like glBufferData, but if the call may change the buffer size, checks
     *  any GL error generated by this glBufferData call and returns it.
     */
    GLenum CheckedBufferData(GLenum target, GLsizeiptr size, const GLvoid* data,
                             GLenum usage);

    /** Like glTexImage2D, but if the call may change the texture size, checks
     * any GL error generated by this glTexImage2D call and returns it.
     */
    GLenum CheckedTexImage2D(TexImageTarget texImageTarget, GLint level,
                             TexInternalFormat internalFormat, GLsizei width,
                             GLsizei height, GLint border, TexFormat format,
                             TexType type, const GLvoid* data);

    void ForceLoseContext(bool simulateLoss = false);
    void ForceRestoreContext();

    nsTArray<WebGLRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLRefPtr<WebGLTexture> > mBoundCubeMapTextures;
    nsTArray<WebGLRefPtr<WebGLTexture> > mBound3DTextures;

    void ResolveTexturesForDraw() const;

    WebGLRefPtr<WebGLProgram> mCurrentProgram;
    RefPtr<const webgl::LinkedProgramInfo> mActiveProgramLinkInfo;

    uint32_t mMaxFramebufferColorAttachments;

    GLenum LastColorAttachment() const {
        return LOCAL_GL_COLOR_ATTACHMENT0 + mMaxFramebufferColorAttachments - 1;
    }

    bool ValidateFramebufferTarget(GLenum target, const char* const info);

    WebGLRefPtr<WebGLFramebuffer> mBoundDrawFramebuffer;
    WebGLRefPtr<WebGLFramebuffer> mBoundReadFramebuffer;
    WebGLRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;
    WebGLRefPtr<WebGLTransformFeedback> mBoundTransformFeedback;
    WebGLRefPtr<WebGLVertexArray> mBoundVertexArray;

    LinkedList<WebGLTexture> mTextures;
    LinkedList<WebGLBuffer> mBuffers;
    LinkedList<WebGLProgram> mPrograms;
    LinkedList<WebGLQuery> mQueries;
    LinkedList<WebGLShader> mShaders;
    LinkedList<WebGLRenderbuffer> mRenderbuffers;
    LinkedList<WebGLFramebuffer> mFramebuffers;
    LinkedList<WebGLVertexArray> mVertexArrays;

    // TODO(djg): Does this need a rethink? Should it be WebGL2Context?
    LinkedList<WebGLSampler> mSamplers;
    LinkedList<WebGLTransformFeedback> mTransformFeedbacks;

    WebGLRefPtr<WebGLTransformFeedback> mDefaultTransformFeedback;
    WebGLRefPtr<WebGLVertexArray> mDefaultVertexArray;

    // PixelStore parameters
    uint32_t mPixelStorePackAlignment;
    uint32_t mPixelStoreUnpackAlignment;
    uint32_t mPixelStoreColorspaceConversion;
    bool mPixelStoreFlipY;
    bool mPixelStorePremultiplyAlpha;

    WebGLContextFakeBlackStatus mFakeBlackStatus;

    class FakeBlackTexture {
        gl::GLContext* const mGL;
        GLuint mGLName;

    public:
        FakeBlackTexture(gl::GLContext* gl, TexTarget target, GLenum format);
        ~FakeBlackTexture();
        GLuint GLName() const { return mGLName; }
    };

    UniquePtr<FakeBlackTexture> mBlackOpaqueTexture2D;
    UniquePtr<FakeBlackTexture> mBlackOpaqueTextureCubeMap;
    UniquePtr<FakeBlackTexture> mBlackTransparentTexture2D;
    UniquePtr<FakeBlackTexture> mBlackTransparentTextureCubeMap;

    void
    BindFakeBlackTexturesHelper(GLenum target,
                                const nsTArray<WebGLRefPtr<WebGLTexture> >& boundTexturesArray,
                                UniquePtr<FakeBlackTexture>& opaqueTextureScopedPtr,
                                UniquePtr<FakeBlackTexture>& transparentTextureScopedPtr);

    GLfloat mVertexAttrib0Vector[4];
    GLfloat mFakeVertexAttrib0BufferObjectVector[4];
    size_t mFakeVertexAttrib0BufferObjectSize;
    GLuint mFakeVertexAttrib0BufferObject;
    WebGLVertexAttrib0Status mFakeVertexAttrib0BufferStatus;

    GLint mStencilRefFront;
    GLint mStencilRefBack;
    GLuint mStencilValueMaskFront;
    GLuint mStencilValueMaskBack;
    GLuint mStencilWriteMaskFront;
    GLuint mStencilWriteMaskBack;
    realGLboolean mColorWriteMask[4];
    realGLboolean mDepthWriteMask;
    GLfloat mColorClearValue[4];
    GLint mStencilClearValue;
    GLfloat mDepthClearValue;

    GLint mViewportX;
    GLint mViewportY;
    GLsizei mViewportWidth;
    GLsizei mViewportHeight;
    bool mAlreadyWarnedAboutViewportLargerThanDest;

    RefPtr<WebGLContextLossHandler> mContextLossHandler;
    bool mAllowContextRestore;
    bool mLastLossWasSimulated;
    ContextStatus mContextStatus;
    bool mContextLostErrorSet;

    // Used for some hardware (particularly Tegra 2 and 4) that likes to
    // be Flushed while doing hundreds of draw calls.
    int mDrawCallsSinceLastFlush;

    int mAlreadyGeneratedWarnings;
    int mMaxWarnings;
    bool mAlreadyWarnedAboutFakeVertexAttrib0;

    bool ShouldGenerateWarnings() const;

    uint64_t mLastUseIndex;

    bool mNeedsFakeNoAlpha;
    bool mNeedsFakeNoStencil;

    struct ScopedMaskWorkaround {
        WebGLContext& mWebGL;
        const bool mFakeNoAlpha;
        const bool mFakeNoStencil;

        static bool ShouldFakeNoAlpha(WebGLContext& webgl) {
            // We should only be doing this if we're about to draw to the backbuffer, but
            // the backbuffer needs to have this fake-no-alpha workaround.
            return !webgl.mBoundDrawFramebuffer &&
                   webgl.mNeedsFakeNoAlpha &&
                   webgl.mColorWriteMask[3] != false;
        }

        static bool ShouldFakeNoStencil(WebGLContext& webgl) {
            // We should only be doing this if we're about to draw to the backbuffer.
            return !webgl.mBoundDrawFramebuffer &&
                   webgl.mNeedsFakeNoStencil &&
                   webgl.mStencilTestEnabled;
        }

        explicit ScopedMaskWorkaround(WebGLContext& webgl);

        ~ScopedMaskWorkaround();
    };

    void LoseOldestWebGLContextIfLimitExceeded();
    void UpdateLastUseIndex();

    template <typename WebGLObjectType>
    JS::Value WebGLObjectAsJSValue(JSContext* cx, const WebGLObjectType*,
                                   ErrorResult& rv) const;
    template <typename WebGLObjectType>
    JSObject* WebGLObjectAsJSObject(JSContext* cx, const WebGLObjectType*,
                                    ErrorResult& rv) const;

#ifdef XP_MACOSX
    // see bug 713305. This RAII helper guarantees that we're on the discrete GPU, during its lifetime
    // Debouncing note: we don't want to switch GPUs too frequently, so try to not create and destroy
    // these objects at high frequency. Having WebGLContext's hold one such object seems fine,
    // because WebGLContext objects only go away during GC, which shouldn't happen too frequently.
    // If in the future GC becomes much more frequent, we may have to revisit then (maybe use a timer).
    ForceDiscreteGPUHelperCGL mForceDiscreteGPUHelper;
#endif

    nsRefPtr<WebGLObserver> mContextObserver;

public:
    // console logging helpers
    void GenerateWarning(const char* fmt, ...);
    void GenerateWarning(const char* fmt, va_list ap);

    friend class WebGLTexture;
    friend class WebGLFramebuffer;
    friend class WebGLRenderbuffer;
    friend class WebGLProgram;
    friend class WebGLQuery;
    friend class WebGLBuffer;
    friend class WebGLSampler;
    friend class WebGLShader;
    friend class WebGLSync;
    friend class WebGLTransformFeedback;
    friend class WebGLUniformLocation;
    friend class WebGLVertexArray;
    friend class WebGLVertexArrayFake;
    friend class WebGLVertexArrayGL;
};

// used by DOM bindings in conjunction with GetParentObject
inline nsISupports*
ToSupports(WebGLContext* webgl)
{
    return static_cast<nsIDOMWebGLRenderingContext*>(webgl);
}

/**
 ** Template implementations
 **/

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowDeletedOrNull(const char* info,
                                               ObjectType* object)
{
    if (object && !object->IsCompatibleWithContext(this)) {
        ErrorInvalidOperation("%s: object from different WebGL context "
                              "(or older generation of this one) "
                              "passed as argument", info);
        return false;
    }

    return true;
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAssumeNonNull(const char* info, ObjectType* object)
{
    MOZ_ASSERT(object);

    if (!ValidateObjectAllowDeletedOrNull(info, object))
        return false;

    if (object->IsDeleted()) {
        ErrorInvalidValue("%s: Deleted object passed as argument.", info);
        return false;
    }

    return true;
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowNull(const char* info, ObjectType* object)
{
    if (!object)
        return true;

    return ValidateObjectAssumeNonNull(info, object);
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowDeleted(const char* info, ObjectType* object)
{
    if (!object) {
        ErrorInvalidValue("%s: null object passed as argument", info);
        return false;
    }

    return ValidateObjectAllowDeletedOrNull(info, object);
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObject(const char* info, ObjectType* object)
{
    if (!object) {
        ErrorInvalidValue("%s: null object passed as argument", info);
        return false;
    }

    return ValidateObjectAssumeNonNull(info, object);
}

// Listen visibilitychange and memory-pressure event for context lose/restore
class WebGLObserver final
    : public nsIObserver
    , public nsIDOMEventListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIDOMEVENTLISTENER

    explicit WebGLObserver(WebGLContext* webgl);

    void Destroy();

    void RegisterVisibilityChangeEvent();
    void UnregisterVisibilityChangeEvent();

    void RegisterMemoryPressureEvent();
    void UnregisterMemoryPressureEvent();

private:
    ~WebGLObserver();

    WebGLContext* mWebGL;
};

size_t RoundUpToMultipleOf(size_t value, size_t multiple);

} // namespace mozilla

#endif
