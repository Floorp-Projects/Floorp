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

#include "GLDefs.h"
#include "WebGLActiveInfo.h"
#include "WebGLObjectModel.h"
#include "WebGLRenderbuffer.h"
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

class WebGLObserver;
class WebGLContextBoundObject;
class WebGLActiveInfo;
class WebGLExtensionBase;
class WebGLBuffer;
struct WebGLVertexAttribData;
class WebGLShader;
class WebGLProgram;
class WebGLQuery;
class WebGLUniformLocation;
class WebGLFramebuffer;
class WebGLRenderbuffer;
class WebGLShaderPrecisionFormat;
class WebGLTexture;
class WebGLVertexArray;

namespace dom {
class ImageData;
class Element;

struct WebGLContextAttributes;
template<typename> struct Nullable;
}

namespace gfx {
class SourceSurface;
}

WebGLTexelFormat GetWebGLTexelFormat(GLenum format, GLenum type);

void AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow);

struct WebGLContextOptions {
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

#ifdef DEBUG
static bool
IsTextureBinding(GLenum binding)
{
    switch (binding) {
    case LOCAL_GL_TEXTURE_BINDING_2D:
    case LOCAL_GL_TEXTURE_BINDING_CUBE_MAP:
        return true;
    }
    return false;
}
#endif

class WebGLContext :
    public nsIDOMWebGLRenderingContext,
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference,
    public WebGLRectangleObject,
    public nsWrapperCache
{
    friend class WebGLContextUserData;
    friend class WebGLExtensionCompressedTextureATC;
    friend class WebGLExtensionCompressedTextureETC1;
    friend class WebGLExtensionCompressedTexturePVRTC;
    friend class WebGLExtensionCompressedTextureS3TC;
    friend class WebGLExtensionDepthTexture;
    friend class WebGLExtensionDrawBuffers;
    friend class WebGLExtensionLoseContext;
    friend class WebGLExtensionVertexArray;
    friend class WebGLObserver;
    friend class WebGLMemoryTracker;

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

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(WebGLContext,
                                                           nsIDOMWebGLRenderingContext)

    virtual JSObject* WrapObject(JSContext *cx) = 0;

    NS_DECL_NSIDOMWEBGLRENDERINGCONTEXT

    // nsICanvasRenderingContextInternal
#ifdef DEBUG
    virtual int32_t GetWidth() const MOZ_OVERRIDE;
    virtual int32_t GetHeight() const MOZ_OVERRIDE;
#endif
    NS_IMETHOD SetDimensions(int32_t width, int32_t height) MOZ_OVERRIDE;
    NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, int32_t width, int32_t height) MOZ_OVERRIDE
        { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Reset() MOZ_OVERRIDE
        { /* (InitializeWithSurface) */ return NS_ERROR_NOT_IMPLEMENTED; }
    virtual void GetImageBuffer(uint8_t** aImageBuffer, int32_t* aFormat);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const char16_t* aEncoderOptions,
                              nsIInputStream **aStream) MOZ_OVERRIDE;
    mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(bool* aPremultAlpha) MOZ_OVERRIDE;

    NS_IMETHOD SetIsOpaque(bool b) MOZ_OVERRIDE { return NS_OK; };
    bool GetIsOpaque() MOZ_OVERRIDE { return false; }
    NS_IMETHOD SetContextOptions(JSContext* aCx,
                                 JS::Handle<JS::Value> aOptions) MOZ_OVERRIDE;

    NS_IMETHOD SetIsIPC(bool b) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Redraw(const gfxRect&) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(mozilla::ipc::Shmem& aBack,
                    int32_t x, int32_t y, int32_t w, int32_t h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(uint32_t nativeID,
                    int32_t x, int32_t y, int32_t w, int32_t h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }

    void SynthesizeGLError(GLenum err);
    void SynthesizeGLError(GLenum err, const char *fmt, ...);

    void ErrorInvalidEnum(const char *fmt = 0, ...);
    void ErrorInvalidOperation(const char *fmt = 0, ...);
    void ErrorInvalidValue(const char *fmt = 0, ...);
    void ErrorInvalidFramebufferOperation(const char *fmt = 0, ...);
    void ErrorInvalidEnumInfo(const char *info, GLenum enumvalue);
    void ErrorOutOfMemory(const char *fmt = 0, ...);

    const char *ErrorName(GLenum error);

    /**
     * Return displayable name for GLenum.
     * This version is like gl::GLenumToStr but with out the GL_ prefix to
     * keep consistency with how errors are reported from WebGL.
     */
    static const char *EnumName(GLenum glenum);

    bool IsTextureFormatCompressed(GLenum format);

    void DummyFramebufferOperation(const char *info);

    WebGLTexture* activeBoundTextureForTarget(GLenum target) const {
        MOZ_ASSERT(!IsTextureBinding(target));
        return target == LOCAL_GL_TEXTURE_2D ? mBound2DTextures[mActiveTexture]
                                             : mBoundCubeMapTextures[mActiveTexture];
    }

    already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                 CanvasLayer *aOldLayer,
                                                 LayerManager *aManager) MOZ_OVERRIDE;

    // Note that 'clean' here refers to its invalidation state, not the
    // contents of the buffer.
    void MarkContextClean() MOZ_OVERRIDE { mInvalidated = false; }

    gl::GLContext* GL() const { return gl; }

    bool IsPremultAlpha() const { return mOptions.premultipliedAlpha; }

    bool PresentScreenBuffer();

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    uint32_t Generation() { return mGeneration.value(); }

    // Returns null if the current bound FB is not likely complete.
    const WebGLRectangleObject* CurValidFBRectObject() const;

    static const size_t kMaxColorAttachments = 16;

    // This is similar to GLContext::ClearSafely, but tries to minimize the
    // amount of work it does.
    // It only clears the buffers we specify, and can reset its state without
    // first having to query anything, as WebGL knows its state at all times.
    void ForceClearFramebufferWithDefaultValues(GLbitfield mask, const bool colorAttachmentsMask[kMaxColorAttachments]);

    // Calls ForceClearFramebufferWithDefaultValues() for the Context's 'screen'.
    void ClearScreen();
    void ClearBackbufferIfNeeded();

    bool MinCapabilityMode() const { return mMinCapability; }

    void UpdateContextLossStatus();
    void EnqueueUpdateContextLossStatus();
    static void ContextLossCallbackStatic(nsITimer* timer, void* thisPointer);
    void RunContextLossTimer();
    void TerminateContextLossTimer();

    bool TryToRestoreContext();

    void AssertCachedBindings();
    void AssertCachedState();

    // WebIDL WebGLRenderingContext API
    dom::HTMLCanvasElement* GetCanvas() const { return mCanvasElement; }
    GLsizei DrawingBufferWidth() const { return IsContextLost() ? 0 : mWidth; }
    GLsizei DrawingBufferHeight() const { return IsContextLost() ? 0 : mHeight; }

    void GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval);
    bool IsContextLost() const { return mContextStatus != ContextNotLost; }
    void GetSupportedExtensions(JSContext *cx, dom::Nullable< nsTArray<nsString> > &retval);
    void GetExtension(JSContext* cx, const nsAString& aName,
                      JS::MutableHandle<JSObject*> aRetval,
                      ErrorResult& rv);
    void ActiveTexture(GLenum texture);
    void AttachShader(WebGLProgram* program, WebGLShader* shader);
    void BindAttribLocation(WebGLProgram* program, GLuint location,
                            const nsAString& name);
    void BindFramebuffer(GLenum target, WebGLFramebuffer* wfb);
    void BindRenderbuffer(GLenum target, WebGLRenderbuffer* wrb);
    void BindTexture(GLenum target, WebGLTexture *tex);
    void BindVertexArray(WebGLVertexArray *vao);
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
    void CompileShader(WebGLShader *shader);
    void CompressedTexImage2D(GLenum target, GLint level,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, GLint border,
                              const dom::ArrayBufferView& view);
    void CompressedTexSubImage2D(GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLsizei width, GLsizei height,
                                 GLenum format,
                                 const dom::ArrayBufferView& view);
    void CopyTexImage2D(GLenum target, GLint level,
                        GLenum internalformat, GLint x, GLint y,
                        GLsizei width, GLsizei height, GLint border);
    void CopyTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                           GLint yoffset, GLint x, GLint y,
                           GLsizei width, GLsizei height);
    already_AddRefed<WebGLFramebuffer> CreateFramebuffer();
    already_AddRefed<WebGLProgram> CreateProgram();
    already_AddRefed<WebGLRenderbuffer> CreateRenderbuffer();
    already_AddRefed<WebGLTexture> CreateTexture();
    already_AddRefed<WebGLShader> CreateShader(GLenum type);
    already_AddRefed<WebGLVertexArray> CreateVertexArray();
    void CullFace(GLenum face);
    void DeleteFramebuffer(WebGLFramebuffer *fbuf);
    void DeleteProgram(WebGLProgram *prog);
    void DeleteRenderbuffer(WebGLRenderbuffer *rbuf);
    void DeleteShader(WebGLShader *shader);
    void DeleteVertexArray(WebGLVertexArray *vao);
    void DeleteTexture(WebGLTexture *tex);
    void DepthFunc(GLenum func);
    void DepthMask(WebGLboolean b);
    void DepthRange(GLclampf zNear, GLclampf zFar);
    void DetachShader(WebGLProgram *program, WebGLShader *shader);
    void DrawBuffers(const dom::Sequence<GLenum>& buffers);
    void Flush();
    void Finish();
    void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                                 GLenum rbtarget, WebGLRenderbuffer *wrb);
    void FramebufferTexture2D(GLenum target, GLenum attachment,
                              GLenum textarget, WebGLTexture *tobj,
                              GLint level);
    void FrontFace(GLenum mode);
    void GenerateMipmap(GLenum target);
    already_AddRefed<WebGLActiveInfo> GetActiveAttrib(WebGLProgram *prog,
                                                      GLuint index);
    already_AddRefed<WebGLActiveInfo> GetActiveUniform(WebGLProgram *prog,
                                                       GLuint index);
    void GetAttachedShaders(WebGLProgram* prog,
                            dom::Nullable< nsTArray<WebGLShader*> > &retval);
    GLint GetAttribLocation(WebGLProgram* prog, const nsAString& name);
    JS::Value GetBufferParameter(GLenum target, GLenum pname);
    void GetBufferParameter(JSContext* /* unused */, GLenum target,
                            GLenum pname,
                            JS::MutableHandle<JS::Value> retval) {
        retval.set(GetBufferParameter(target, pname));
    }
    GLenum GetError();
    JS::Value GetFramebufferAttachmentParameter(JSContext* cx,
                                                GLenum target,
                                                GLenum attachment,
                                                GLenum pname,
                                                ErrorResult& rv);
    void GetFramebufferAttachmentParameter(JSContext* cx,
                                           GLenum target,
                                           GLenum attachment,
                                           GLenum pname,
                                           JS::MutableHandle<JS::Value> retval,
                                           ErrorResult& rv) {
        retval.set(GetFramebufferAttachmentParameter(cx, target, attachment,
                                                     pname, rv));
    }
    JS::Value GetProgramParameter(WebGLProgram *prog, GLenum pname);
    void  GetProgramParameter(JSContext* /* unused */, WebGLProgram *prog,
                              GLenum pname,
                              JS::MutableHandle<JS::Value> retval) {
        retval.set(GetProgramParameter(prog, pname));
    }
    void GetProgramInfoLog(WebGLProgram *prog, nsACString& retval);
    void GetProgramInfoLog(WebGLProgram *prog, nsAString& retval);
    JS::Value GetRenderbufferParameter(GLenum target, GLenum pname);
    void GetRenderbufferParameter(JSContext* /* unused */,
                                  GLenum target, GLenum pname,
                                  JS::MutableHandle<JS::Value> retval) {
        retval.set(GetRenderbufferParameter(target, pname));
    }
    JS::Value GetShaderParameter(WebGLShader *shader, GLenum pname);
    void GetShaderParameter(JSContext* /* unused */, WebGLShader *shader,
                            GLenum pname,
                            JS::MutableHandle<JS::Value> retval) {
        retval.set(GetShaderParameter(shader, pname));
    }
    already_AddRefed<WebGLShaderPrecisionFormat>
      GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype);
    void GetShaderInfoLog(WebGLShader *shader, nsACString& retval);
    void GetShaderInfoLog(WebGLShader *shader, nsAString& retval);
    void GetShaderSource(WebGLShader *shader, nsAString& retval);
    void GetShaderTranslatedSource(WebGLShader *shader, nsAString& retval);
    JS::Value GetTexParameter(GLenum target, GLenum pname);
    void GetTexParameter(JSContext * /* unused */, GLenum target,
                         GLenum pname,
                         JS::MutableHandle<JS::Value> retval) {
        retval.set(GetTexParameter(target, pname));
    }
    JS::Value GetUniform(JSContext* cx, WebGLProgram *prog,
                         WebGLUniformLocation *location);
    void GetUniform(JSContext* cx, WebGLProgram *prog,
                    WebGLUniformLocation *location,
                    JS::MutableHandle<JS::Value> retval) {
        retval.set(GetUniform(cx, prog, location));
    }
    already_AddRefed<WebGLUniformLocation>
      GetUniformLocation(WebGLProgram *prog, const nsAString& name);
    void Hint(GLenum target, GLenum mode);
    bool IsFramebuffer(WebGLFramebuffer *fb);
    bool IsProgram(WebGLProgram *prog);
    bool IsRenderbuffer(WebGLRenderbuffer *rb);
    bool IsShader(WebGLShader *shader);
    bool IsTexture(WebGLTexture *tex);
    bool IsVertexArray(WebGLVertexArray *vao);
    void LineWidth(GLfloat width);
    void LinkProgram(WebGLProgram *program);
    void PixelStorei(GLenum pname, GLint param);
    void PolygonOffset(GLfloat factor, GLfloat units);
    void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                    GLenum format, GLenum type,
                    const Nullable<dom::ArrayBufferView> &pixels,
                    ErrorResult& rv);
    void RenderbufferStorage(GLenum target, GLenum internalformat,
                             GLsizei width, GLsizei height);
    void SampleCoverage(GLclampf value, WebGLboolean invert);
    void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void ShaderSource(WebGLShader *shader, const nsAString& source);
    void StencilFunc(GLenum func, GLint ref, GLuint mask);
    void StencilFuncSeparate(GLenum face, GLenum func, GLint ref,
                             GLuint mask);
    void StencilMask(GLuint mask);
    void StencilMaskSeparate(GLenum face, GLuint mask);
    void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
    void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                           GLenum dppass);
    void TexImage2D(GLenum target, GLint level,
                    GLenum internalformat, GLsizei width,
                    GLsizei height, GLint border, GLenum format,
                    GLenum type,
                    const Nullable<dom::ArrayBufferView> &pixels,
                    ErrorResult& rv);
    void TexImage2D(GLenum target, GLint level,
                    GLenum internalformat, GLenum format, GLenum type,
                    dom::ImageData* pixels, ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexImage2D
    bool TexImageFromVideoElement(GLenum target, GLint level,
                                  GLenum internalformat, GLenum format, GLenum type,
                                  mozilla::dom::Element& image);

    template<class ElementType>
    void TexImage2D(GLenum target, GLint level,
                    GLenum internalformat, GLenum format, GLenum type,
                    ElementType& elt, ErrorResult& rv)
    {
        if (IsContextLost())
            return;

        WebGLTexture* tex = activeBoundTextureForTarget(target);

        if (!tex)
            return ErrorInvalidOperation("no texture is bound to this target");

        // Trying to handle the video by GPU directly first
        if (TexImageFromVideoElement(target, level, internalformat, format, type, elt)) {
            return;
        }

        RefPtr<gfx::DataSourceSurface> data;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, data,
                                                    &srcFormat);
        if (rv.Failed() || !data)
            return;

        gfx::IntSize size = data->GetSize();
        uint32_t byteLength = data->Stride() * size.height;
        return TexImage2D_base(target, level, internalformat,
                               size.width, size.height, data->Stride(),
                               0, format, type, data->GetData(), byteLength,
                               -1, srcFormat, mPixelStorePremultiplyAlpha);
    }

    void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
        TexParameter_base(target, pname, nullptr, &param);
    }
    void TexParameteri(GLenum target, GLenum pname, GLint param) {
        TexParameter_base(target, pname, &param, nullptr);
    }

    void TexSubImage2D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset,
                       GLsizei width, GLsizei height, GLenum format,
                       GLenum type,
                       const Nullable<dom::ArrayBufferView> &pixels,
                       ErrorResult& rv);
    void TexSubImage2D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLenum format,
                       GLenum type, dom::ImageData* pixels, ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexSubImage2D
    template<class ElementType>
    void TexSubImage2D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLenum format,
                       GLenum type, ElementType& elt, ErrorResult& rv)
    {
        if (IsContextLost())
            return;

        // Trying to handle the video by GPU directly first
        if (TexImageFromVideoElement(target, level, format, format, type, elt)) {
            return;
        }

        RefPtr<gfx::DataSourceSurface> data;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, data,
                                                    &srcFormat);
        if (rv.Failed() || !data)
            return;

        gfx::IntSize size = data->GetSize();
        uint32_t byteLength = data->Stride() * size.height;
        return TexSubImage2D_base(target, level, xoffset, yoffset,
                                  size.width, size.height,
                                  data->Stride(), format, type,
                                  data->GetData(), byteLength,
                                  -1, srcFormat, mPixelStorePremultiplyAlpha);

    }

    void Uniform1i(WebGLUniformLocation* location, GLint x);
    void Uniform2i(WebGLUniformLocation* location, GLint x, GLint y);
    void Uniform3i(WebGLUniformLocation* location, GLint x, GLint y,
                   GLint z);
    void Uniform4i(WebGLUniformLocation* location, GLint x, GLint y,
                   GLint z, GLint w);

    void Uniform1f(WebGLUniformLocation* location, GLfloat x);
    void Uniform2f(WebGLUniformLocation* location, GLfloat x, GLfloat y);
    void Uniform3f(WebGLUniformLocation* location, GLfloat x, GLfloat y,
                   GLfloat z);
    void Uniform4f(WebGLUniformLocation* location, GLfloat x, GLfloat y,
                   GLfloat z, GLfloat w);

    void Uniform1iv(WebGLUniformLocation* location,
                    const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform1iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform1iv(WebGLUniformLocation* location,
                    const dom::Sequence<GLint>& arr) {
        Uniform1iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform1iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLint* data);

    void Uniform2iv(WebGLUniformLocation* location,
                    const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform2iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform2iv(WebGLUniformLocation* location,
                    const dom::Sequence<GLint>& arr) {
        Uniform2iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform2iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLint* data);

    void Uniform3iv(WebGLUniformLocation* location,
                    const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform3iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform3iv(WebGLUniformLocation* location,
                    const dom::Sequence<GLint>& arr) {
        Uniform3iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform3iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLint* data);

    void Uniform4iv(WebGLUniformLocation* location,
                    const dom::Int32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform4iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform4iv(WebGLUniformLocation* location,
                    const dom::Sequence<GLint>& arr) {
        Uniform4iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform4iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLint* data);

    void Uniform1fv(WebGLUniformLocation* location,
                    const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform1fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform1fv(WebGLUniformLocation* location,
                    const dom::Sequence<GLfloat>& arr) {
        Uniform1fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform1fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLfloat* data);

    void Uniform2fv(WebGLUniformLocation* location,
                    const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform2fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform2fv(WebGLUniformLocation* location,
                    const dom::Sequence<GLfloat>& arr) {
        Uniform2fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform2fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLfloat* data);

    void Uniform3fv(WebGLUniformLocation* location,
                    const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform3fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform3fv(WebGLUniformLocation* location,
                    const dom::Sequence<GLfloat>& arr) {
        Uniform3fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform3fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLfloat* data);

    void Uniform4fv(WebGLUniformLocation* location,
                    const dom::Float32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform4fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform4fv(WebGLUniformLocation* location,
                    const dom::Sequence<GLfloat>& arr) {
        Uniform4fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform4fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const GLfloat* data);

    void UniformMatrix2fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Float32Array &value) {
        value.ComputeLengthAndData();
        UniformMatrix2fv_base(location, transpose, value.Length(), value.Data());
    }
    void UniformMatrix2fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Sequence<float> &value) {
        UniformMatrix2fv_base(location, transpose, value.Length(),
                              value.Elements());
    }
    void UniformMatrix2fv_base(WebGLUniformLocation* location,
                               WebGLboolean transpose, uint32_t arrayLength,
                               const float* data);

    void UniformMatrix3fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Float32Array &value) {
        value.ComputeLengthAndData();
        UniformMatrix3fv_base(location, transpose, value.Length(), value.Data());
    }
    void UniformMatrix3fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Sequence<float> &value) {
        UniformMatrix3fv_base(location, transpose, value.Length(),
                              value.Elements());
    }
    void UniformMatrix3fv_base(WebGLUniformLocation* location,
                               WebGLboolean transpose, uint32_t arrayLength,
                               const float* data);

    void UniformMatrix4fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Float32Array &value) {
        value.ComputeLengthAndData();
        UniformMatrix4fv_base(location, transpose, value.Length(), value.Data());
    }
    void UniformMatrix4fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          const dom::Sequence<float> &value) {
        UniformMatrix4fv_base(location, transpose, value.Length(),
                              value.Elements());
    }
    void UniformMatrix4fv_base(WebGLUniformLocation* location,
                               WebGLboolean transpose, uint32_t arrayLength,
                               const float* data);

    void UseProgram(WebGLProgram *prog);
    bool ValidateAttribArraySetter(const char* name, uint32_t cnt, uint32_t arrayLength);
    bool ValidateUniformArraySetter(const char* name, uint32_t expectedElemSize, WebGLUniformLocation *location_object,
                                    GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength);
    bool ValidateUniformMatrixArraySetter(const char* name, int dim, WebGLUniformLocation *location_object,
                                          GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength,
                                          WebGLboolean aTranspose);
    bool ValidateUniformSetter(const char* name, WebGLUniformLocation *location_object, GLint& location);
    void ValidateProgram(WebGLProgram *prog);
    bool ValidateUniformLocation(const char* info, WebGLUniformLocation *location_object);
    bool ValidateSamplerUniformSetter(const char* info,
                                    WebGLUniformLocation *location,
                                    GLint value);
    void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
// -----------------------------------------------------------------------------
// WEBGL_lose_context
public:
    void LoseContext();
    void RestoreContext();

// -----------------------------------------------------------------------------
// Asynchronous Queries (WebGLContextAsyncQueries.cpp)
public:
    already_AddRefed<WebGLQuery> CreateQuery();
    void DeleteQuery(WebGLQuery *query);
    void BeginQuery(GLenum target, WebGLQuery *query);
    void EndQuery(GLenum target);
    bool IsQuery(WebGLQuery *query);
    already_AddRefed<WebGLQuery> GetQuery(GLenum target, GLenum pname);
    JS::Value GetQueryObject(JSContext* cx, WebGLQuery *query, GLenum pname);
    void GetQueryObject(JSContext* cx, WebGLQuery *query, GLenum pname,
                        JS::MutableHandle<JS::Value> retval) {
        retval.set(GetQueryObject(cx, query, pname));
    }

private:
    // ANY_SAMPLES_PASSED(_CONSERVATIVE) slot
    WebGLRefPtr<WebGLQuery> mActiveOcclusionQuery;

    // LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN slot
    WebGLRefPtr<WebGLQuery> mActiveTransformFeedbackQuery;

    WebGLRefPtr<WebGLQuery>* GetQueryTargetSlot(GLenum target, const char* infos);

// -----------------------------------------------------------------------------
// Buffer Objects (WebGLContextBuffers.cpp)
public:
    void BindBuffer(GLenum target, WebGLBuffer* buf);
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                         WebGLintptr offset, WebGLsizeiptr size);
    void BufferData(GLenum target, WebGLsizeiptr size, GLenum usage);
    void BufferData(GLenum target, const dom::ArrayBufferView &data,
                    GLenum usage);
    void BufferData(GLenum target,
                    const Nullable<dom::ArrayBuffer> &maybeData,
                    GLenum usage);
    void BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                       const dom::ArrayBufferView &data);
    void BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                       const Nullable<dom::ArrayBuffer> &maybeData);
    already_AddRefed<WebGLBuffer> CreateBuffer();
    void DeleteBuffer(WebGLBuffer *buf);
    bool IsBuffer(WebGLBuffer *buffer);

private:
    // ARRAY_BUFFER slot
    WebGLRefPtr<WebGLBuffer> mBoundArrayBuffer;

    // TRANSFORM_FEEDBACK_BUFFER slot
    WebGLRefPtr<WebGLBuffer> mBoundTransformFeedbackBuffer;

    // these two functions emit INVALID_ENUM for invalid `target`.
    WebGLRefPtr<WebGLBuffer>* GetBufferSlotByTarget(GLenum target, const char* infos);
    WebGLRefPtr<WebGLBuffer>* GetBufferSlotByTargetIndexed(GLenum target, GLuint index, const char* infos);
    bool ValidateBufferUsageEnum(GLenum target, const char* infos);

// -----------------------------------------------------------------------------
// State and State Requests (WebGLContextState.cpp)
public:
    void Disable(GLenum cap);
    void Enable(GLenum cap);
    JS::Value GetParameter(JSContext* cx, GLenum pname, ErrorResult& rv);
    void GetParameter(JSContext* cx, GLenum pname,
                      JS::MutableHandle<JS::Value> retval, ErrorResult& rv) {
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

    bool ValidateCapabilityEnum(GLenum cap, const char* info);
    realGLboolean* GetStateTrackingSlot(GLenum cap);

// -----------------------------------------------------------------------------
// Vertices Feature (WebGLContextVertices.cpp)
public:
    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
    void DrawElements(GLenum mode, GLsizei count, GLenum type, WebGLintptr byteOffset);
    void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                               WebGLintptr byteOffset, GLsizei primcount);

    void EnableVertexAttribArray(GLuint index);
    void DisableVertexAttribArray(GLuint index);

    JS::Value GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                              ErrorResult& rv);
    void GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                         JS::MutableHandle<JS::Value> retval,
                         ErrorResult& rv) {
        retval.set(GetVertexAttrib(cx, index, pname, rv));
    }
    WebGLsizeiptr GetVertexAttribOffset(GLuint index, GLenum pname);

    void VertexAttrib1f(GLuint index, GLfloat x0);
    void VertexAttrib2f(GLuint index, GLfloat x0, GLfloat x1);
    void VertexAttrib3f(GLuint index, GLfloat x0, GLfloat x1,
                        GLfloat x2);
    void VertexAttrib4f(GLuint index, GLfloat x0, GLfloat x1,
                        GLfloat x2, GLfloat x3);

    void VertexAttrib1fv(GLuint idx, const dom::Float32Array &arr) {
        arr.ComputeLengthAndData();
        VertexAttrib1fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib1fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib1fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib2fv(GLuint idx, const dom::Float32Array &arr) {
        arr.ComputeLengthAndData();
        VertexAttrib2fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib2fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib2fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib3fv(GLuint idx, const dom::Float32Array &arr) {
        arr.ComputeLengthAndData();
        VertexAttrib3fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib3fv(GLuint idx, const dom::Sequence<GLfloat>& arr) {
        VertexAttrib3fv_base(idx, arr.Length(), arr.Elements());
    }

    void VertexAttrib4fv(GLuint idx, const dom::Float32Array &arr) {
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

    inline void InvalidateBufferFetching()
    {
        mBufferFetchingIsVerified = false;
        mBufferFetchingHasPerVertex = false;
        mMaxFetchedVertices = 0;
        mMaxFetchedInstances = 0;
    }

    bool DrawArrays_check(GLint first, GLsizei count, GLsizei primcount, const char* info);
    bool DrawElements_check(GLsizei count, GLenum type, WebGLintptr byteOffset,
                            GLsizei primcount, const char* info,
                            GLuint* out_upperBound);
    bool DrawInstanced_check(const char* info);
    void Draw_cleanup();

    void VertexAttrib1fv_base(GLuint idx, uint32_t arrayLength, const GLfloat* ptr);
    void VertexAttrib2fv_base(GLuint idx, uint32_t arrayLength, const GLfloat* ptr);
    void VertexAttrib3fv_base(GLuint idx, uint32_t arrayLength, const GLfloat* ptr);
    void VertexAttrib4fv_base(GLuint idx, uint32_t arrayLength, const GLfloat* ptr);

    bool ValidateBufferFetching(const char *info);
    bool BindArrayAttribToLocation0(WebGLProgram *program);

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

    static CheckedUint32 GetImageSize(GLsizei height,
                                      GLsizei width,
                                      uint32_t pixelSize,
                                      uint32_t alignment);

    // Returns x rounded to the next highest multiple of y.
    static CheckedUint32 RoundedToNextMultipleOf(CheckedUint32 x, CheckedUint32 y) {
        return ((x + y - 1) / y) * y;
    }

    nsRefPtr<gl::GLContext> gl;

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

    // whether shader validation is supported
    bool mShaderValidation;

    // some GL constants
    int32_t mGLMaxVertexAttribs;
    int32_t mGLMaxTextureUnits;
    int32_t mGLMaxTextureSize;
    int32_t mGLMaxCubeMapTextureSize;
    int32_t mGLMaxRenderbufferSize;
    int32_t mGLMaxTextureImageUnits;
    int32_t mGLMaxVertexTextureImageUnits;
    int32_t mGLMaxVaryingVectors;
    int32_t mGLMaxFragmentUniformVectors;
    int32_t mGLMaxVertexUniformVectors;
    int32_t mGLMaxColorAttachments;
    int32_t mGLMaxDrawBuffers;
    uint32_t mGLMaxTransformFeedbackSeparateAttribs;

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
    typedef EnumeratedArray<WebGLExtensionID,
                            WebGLExtensionID::Max,
                            nsRefPtr<WebGLExtensionBase>> ExtensionsArrayType;

    ExtensionsArrayType mExtensions;

    // enable an extension. the extension should not be enabled before.
    void EnableExtension(WebGLExtensionID ext);

    // returns true if the extension has been enabled by calling getExtension.
    bool IsExtensionEnabled(WebGLExtensionID ext) const;

    // returns true if the extension is supported for this JSContext (this decides what getSupportedExtensions exposes)
    bool IsExtensionSupported(JSContext *cx, WebGLExtensionID ext) const;
    bool IsExtensionSupported(WebGLExtensionID ext) const;

    static const char* GetExtensionString(WebGLExtensionID ext);

    nsTArray<GLenum> mCompressedTextureFormats;

    // -------------------------------------------------------------------------
    // WebGL 2 specifics (implemented in WebGL2Context.cpp)

    virtual bool IsWebGL2() const = 0;

    bool InitWebGL2();


    // -------------------------------------------------------------------------
    // Validation functions (implemented in WebGLContextValidate.cpp)
    GLenum BaseTexFormat(GLenum internalFormat) const;

    bool InitAndValidateGL();
    bool ValidateBlendEquationEnum(GLenum cap, const char *info);
    bool ValidateBlendFuncDstEnum(GLenum mode, const char *info);
    bool ValidateBlendFuncSrcEnum(GLenum mode, const char *info);
    bool ValidateBlendFuncEnumsCompatibility(GLenum sfactor, GLenum dfactor, const char *info);
    bool ValidateTextureTargetEnum(GLenum target, const char *info);
    bool ValidateComparisonEnum(GLenum target, const char *info);
    bool ValidateStencilOpEnum(GLenum action, const char *info);
    bool ValidateFaceEnum(GLenum face, const char *info);
    bool ValidateTexInputData(GLenum type, int jsArrayType, WebGLTexImageFunc func);
    bool ValidateDrawModeEnum(GLenum mode, const char *info);
    bool ValidateAttribIndex(GLuint index, const char *info);
    bool ValidateStencilParamsForDrawCall();

    bool ValidateGLSLVariableName(const nsAString& name, const char *info);
    bool ValidateGLSLCharacter(char16_t c);
    bool ValidateGLSLString(const nsAString& string, const char *info);

    bool ValidateCopyTexImage(GLenum format, WebGLTexImageFunc func);
    bool ValidateTexImage(GLuint dims, GLenum target,
                          GLint level, GLint internalFormat,
                          GLint xoffset, GLint yoffset, GLint zoffset,
                          GLint width, GLint height, GLint depth,
                          GLint border, GLenum format, GLenum type,
                          WebGLTexImageFunc func);
    bool ValidateTexImageTarget(GLuint dims, GLenum target, WebGLTexImageFunc func);
    bool ValidateTexImageFormat(GLenum format, WebGLTexImageFunc func);
    bool ValidateTexImageType(GLenum type, WebGLTexImageFunc func);
    bool ValidateTexImageFormatAndType(GLenum format, GLenum type, WebGLTexImageFunc func);
    bool ValidateTexImageSize(GLenum target, GLint level,
                              GLint width, GLint height, GLint depth,
                              WebGLTexImageFunc func);
    bool ValidateTexSubImageSize(GLint x, GLint y, GLint z,
                                 GLsizei width, GLsizei height, GLsizei depth,
                                 GLsizei baseWidth, GLsizei baseHeight, GLsizei baseDepth,
                                 WebGLTexImageFunc func);

    bool ValidateCompTexImageSize(GLenum target, GLint level, GLenum format,
                                  GLint xoffset, GLint yoffset,
                                  GLsizei width, GLsizei height,
                                  GLsizei levelWidth, GLsizei levelHeight,
                                  WebGLTexImageFunc func);
    bool ValidateCompTexImageDataSize(GLint level, GLenum format,
                                      GLsizei width, GLsizei height,
                                      uint32_t byteLength, WebGLTexImageFunc func);


    static uint32_t GetBitsPerTexel(GLenum format, GLenum type);

    void Invalidate();
    void DestroyResourcesAndContext();

    void MakeContextCurrent() const;

    // helpers
    void TexImage2D_base(GLenum target, GLint level, GLenum internalformat,
                         GLsizei width, GLsizei height, GLsizei srcStrideOrZero, GLint border,
                         GLenum format, GLenum type,
                         void *data, uint32_t byteLength,
                         int jsArrayType,
                         WebGLTexelFormat srcFormat, bool srcPremultiplied);
    void TexSubImage2D_base(GLenum target, GLint level,
                            GLint xoffset, GLint yoffset,
                            GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                            GLenum format, GLenum type,
                            void *pixels, uint32_t byteLength,
                            int jsArrayType,
                            WebGLTexelFormat srcFormat, bool srcPremultiplied);
    void TexParameter_base(GLenum target, GLenum pname,
                           GLint *intParamPtr, GLfloat *floatParamPtr);

    void ConvertImage(size_t width, size_t height, size_t srcStride, size_t dstStride,
                      const uint8_t* src, uint8_t *dst,
                      WebGLTexelFormat srcFormat, bool srcPremultiplied,
                      WebGLTexelFormat dstFormat, bool dstPremultiplied,
                      size_t dstTexelSize);

    template<class ElementType>
    nsLayoutUtils::SurfaceFromElementResult SurfaceFromElement(ElementType* aElement) {
        MOZ_ASSERT(aElement);
        uint32_t flags =
             nsLayoutUtils::SFE_WANT_IMAGE_SURFACE;

        if (mPixelStoreColorspaceConversion == LOCAL_GL_NONE)
            flags |= nsLayoutUtils::SFE_NO_COLORSPACE_CONVERSION;
        if (!mPixelStorePremultiplyAlpha)
            flags |= nsLayoutUtils::SFE_PREFER_NO_PREMULTIPLY_ALPHA;
        return nsLayoutUtils::SurfaceFromElement(aElement, flags);
    }
    template<class ElementType>
    nsLayoutUtils::SurfaceFromElementResult SurfaceFromElement(ElementType& aElement)
    {
      return SurfaceFromElement(&aElement);
    }

    nsresult SurfaceFromElementResultToImageSurface(nsLayoutUtils::SurfaceFromElementResult& res,
                                                    RefPtr<gfx::DataSourceSurface>& imageOut,
                                                    WebGLTexelFormat *format);

    void CopyTexSubImage2D_base(GLenum target,
                                GLint level,
                                GLenum internalformat,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                bool sub);

    // Returns false if aObject is null or not valid
    template<class ObjectType>
    bool ValidateObject(const char* info, ObjectType *aObject);
    // Returns false if aObject is not valid.  Considers null to be valid.
    template<class ObjectType>
    bool ValidateObjectAllowNull(const char* info, ObjectType *aObject);
    // Returns false if aObject is not valid, but considers deleted
    // objects and null objects valid.
    template<class ObjectType>
    bool ValidateObjectAllowDeletedOrNull(const char* info, ObjectType *aObject);
    // Returns false if aObject is null or not valid, but considers deleted
    // objects valid.
    template<class ObjectType>
    bool ValidateObjectAllowDeleted(const char* info, ObjectType *aObject);
private:
    // Like ValidateObject, but only for cases when aObject is known
    // to not be null already.
    template<class ObjectType>
    bool ValidateObjectAssumeNonNull(const char* info, ObjectType *aObject);

protected:
    int32_t MaxTextureSizeForTarget(GLenum target) const {
        MOZ_ASSERT(target == LOCAL_GL_TEXTURE_2D ||
                   (target >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                    target <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z),
                   "Invalid target enum");
        return (target == LOCAL_GL_TEXTURE_2D) ? mGLMaxTextureSize : mGLMaxCubeMapTextureSize;
    }

    /** like glBufferData but if the call may change the buffer size, checks any GL error generated
     * by this glBufferData call and returns it */
    GLenum CheckedBufferData(GLenum target,
                             GLsizeiptr size,
                             const GLvoid *data,
                             GLenum usage);
    /** like glTexImage2D but if the call may change the texture size, checks any GL error generated
     * by this glTexImage2D call and returns it */
    GLenum CheckedTexImage2D(GLenum target,
                             GLint level,
                             GLenum internalFormat,
                             GLsizei width,
                             GLsizei height,
                             GLint border,
                             GLenum format,
                             GLenum type,
                             const GLvoid *data);

    void ForceLoseContext(bool simulateLosing = false);
    void ForceRestoreContext();

    nsTArray<WebGLRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLRefPtr<WebGLProgram> mCurrentProgram;

    uint32_t mMaxFramebufferColorAttachments;

    WebGLRefPtr<WebGLFramebuffer> mBoundFramebuffer;
    WebGLRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;
    WebGLRefPtr<WebGLVertexArray> mBoundVertexArray;

    LinkedList<WebGLTexture> mTextures;
    LinkedList<WebGLBuffer> mBuffers;
    LinkedList<WebGLProgram> mPrograms;
    LinkedList<WebGLQuery> mQueries;
    LinkedList<WebGLShader> mShaders;
    LinkedList<WebGLRenderbuffer> mRenderbuffers;
    LinkedList<WebGLFramebuffer> mFramebuffers;
    LinkedList<WebGLVertexArray> mVertexArrays;

    WebGLRefPtr<WebGLVertexArray> mDefaultVertexArray;

    // PixelStore parameters
    uint32_t mPixelStorePackAlignment, mPixelStoreUnpackAlignment, mPixelStoreColorspaceConversion;
    bool mPixelStoreFlipY, mPixelStorePremultiplyAlpha;

    WebGLContextFakeBlackStatus mFakeBlackStatus;

    class FakeBlackTexture
    {
        gl::GLContext* mGL;
        GLuint mGLName;

    public:
        FakeBlackTexture(gl::GLContext* gl, GLenum target, GLenum format);
        ~FakeBlackTexture();
        GLuint GLName() const { return mGLName; }
    };

    UniquePtr<FakeBlackTexture> mBlackOpaqueTexture2D,
                                mBlackOpaqueTextureCubeMap,
                                mBlackTransparentTexture2D,
                                mBlackTransparentTextureCubeMap;

    void BindFakeBlackTexturesHelper(
        GLenum target,
        const nsTArray<WebGLRefPtr<WebGLTexture> >& boundTexturesArray,
        UniquePtr<FakeBlackTexture> & opaqueTextureScopedPtr,
        UniquePtr<FakeBlackTexture> & transparentTextureScopedPtr);

    GLfloat mVertexAttrib0Vector[4];
    GLfloat mFakeVertexAttrib0BufferObjectVector[4];
    size_t mFakeVertexAttrib0BufferObjectSize;
    GLuint mFakeVertexAttrib0BufferObject;
    WebGLVertexAttrib0Status mFakeVertexAttrib0BufferStatus;

    GLint mStencilRefFront, mStencilRefBack;
    GLuint mStencilValueMaskFront, mStencilValueMaskBack,
           mStencilWriteMaskFront, mStencilWriteMaskBack;
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

    nsCOMPtr<nsITimer> mContextRestorer;
    bool mAllowContextRestore;
    bool mLastLossWasSimulated;
    bool mContextLossTimerRunning;
    bool mRunContextLossTimerAgain;
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

    void LoseOldestWebGLContextIfLimitExceeded();
    void UpdateLastUseIndex();

    template <typename WebGLObjectType>
    JS::Value WebGLObjectAsJSValue(JSContext *cx, const WebGLObjectType *, ErrorResult& rv) const;
    template <typename WebGLObjectType>
    JSObject* WebGLObjectAsJSObject(JSContext *cx, const WebGLObjectType *, ErrorResult& rv) const;

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
    void GenerateWarning(const char *fmt, ...);
    void GenerateWarning(const char *fmt, va_list ap);

    friend class WebGLTexture;
    friend class WebGLFramebuffer;
    friend class WebGLRenderbuffer;
    friend class WebGLProgram;
    friend class WebGLQuery;
    friend class WebGLBuffer;
    friend class WebGLShader;
    friend class WebGLUniformLocation;
    friend class WebGLVertexArray;
    friend class WebGLVertexArrayFake;
    friend class WebGLVertexArrayGL;
};

// used by DOM bindings in conjunction with GetParentObject
inline nsISupports*
ToSupports(WebGLContext* context)
{
  return static_cast<nsIDOMWebGLRenderingContext*>(context);
}

/**
 ** Template implementations
 **/

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowDeletedOrNull(const char* info,
                                               ObjectType *aObject)
{
    if (aObject && !aObject->IsCompatibleWithContext(this)) {
        ErrorInvalidOperation("%s: object from different WebGL context "
                              "(or older generation of this one) "
                              "passed as argument", info);
        return false;
    }

    return true;
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAssumeNonNull(const char* info, ObjectType *aObject)
{
    MOZ_ASSERT(aObject);

    if (!ValidateObjectAllowDeletedOrNull(info, aObject))
        return false;

    if (aObject->IsDeleted()) {
        ErrorInvalidValue("%s: deleted object passed as argument", info);
        return false;
    }

    return true;
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowNull(const char* info, ObjectType *aObject)
{
    if (!aObject) {
        return true;
    }

    return ValidateObjectAssumeNonNull(info, aObject);
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObjectAllowDeleted(const char* info, ObjectType *aObject)
{
    if (!aObject) {
        ErrorInvalidValue("%s: null object passed as argument", info);
        return false;
    }

    return ValidateObjectAllowDeletedOrNull(info, aObject);
}

template<class ObjectType>
inline bool
WebGLContext::ValidateObject(const char* info, ObjectType *aObject)
{
    if (!aObject) {
        ErrorInvalidValue("%s: null object passed as argument", info);
        return false;
    }

    return ValidateObjectAssumeNonNull(info, aObject);
}

// Listen visibilitychange and memory-pressure event for context lose/restore
class WebGLObserver MOZ_FINAL
    : public nsIObserver
    , public nsIDOMEventListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIDOMEVENTLISTENER

    WebGLObserver(WebGLContext* aContext);

    void Destroy();

    void RegisterVisibilityChangeEvent();
    void UnregisterVisibilityChangeEvent();

    void RegisterMemoryPressureEvent();
    void UnregisterMemoryPressureEvent();

private:
    ~WebGLObserver();

    WebGLContext* mContext;
};

} // namespace mozilla

#endif
