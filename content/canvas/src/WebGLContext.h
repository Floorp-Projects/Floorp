/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXT_H_
#define WEBGLCONTEXT_H_

#include "WebGLElementArrayCache.h"
#include "WebGLObjectModel.h"
#include "WebGLShader.h"
#include "WebGLBuffer.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShaderPrecisionFormat.h"
#include <stdarg.h>

#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsCycleCollectionNoteChild.h"

#include "nsIDocShell.h"

#include "nsIDOMWebGLRenderingContext.h"
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIMemoryReporter.h"
#include "nsIJSNativeInitializer.h"
#include "nsWrapperCache.h"
#include "nsIObserver.h"

#include "GLContextProvider.h"

#include "mozilla/LinkedList.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ImageData.h"

#ifdef XP_MACOSX
#include "ForceDiscreteGPUHelperCGL.h"
#endif

#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"

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

class nsIPropertyBag;

namespace mozilla {

class WebGLMemoryPressureObserver;
class WebGLContextBoundObject;
class WebGLActiveInfo;
class WebGLExtensionBase;

namespace dom {
struct WebGLContextAttributes;
struct WebGLContextAttributesInitializer;
}

struct VertexAttrib0Status {
    enum { Default, EmulatedUninitializedArray, EmulatedInitializedArray };
};

namespace WebGLTexelConversions {

/*
 * The formats that may participate, either as source or destination formats,
 * in WebGL texture conversions. This includes:
 *  - all the formats accepted by WebGL.texImage2D, e.g. RGBA4444
 *  - additional formats provided by extensions, e.g. RGB32F
 *  - additional source formats, depending on browser details, used when uploading
 *    textures from DOM elements. See gfxImageSurface::Format().
 */
enum WebGLTexelFormat
{
    // dummy error code returned by GetWebGLTexelFormat in error cases,
    // after assertion failure (so this never happens in debug builds)
    BadFormat,
    // dummy pseudo-format meaning "use the other format".
    // For example, if SrcFormat=Auto and DstFormat=RGB8, then the source
    // is implicitly treated as being RGB8 itself.
    Auto,
    // 1-channel formats
    R8,
    A8,
    D16, // used for WEBGL_depth_texture extension
    D32, // used for WEBGL_depth_texture extension
    R32F, // used for OES_texture_float extension
    A32F, // used for OES_texture_float extension
    // 2-channel formats
    RA8,
    RA32F,
    D24S8, // used for WEBGL_depth_texture extension
    // 3-channel formats
    RGB8,
    BGRX8, // used for DOM elements. Source format only.
    RGB565,
    RGB32F, // used for OES_texture_float extension
    // 4-channel formats
    RGBA8,
    BGRA8, // used for DOM elements
    RGBA5551,
    RGBA4444,
    RGBA32F // used for OES_texture_float extension
};

} // end namespace WebGLTexelConversions

using WebGLTexelConversions::WebGLTexelFormat;

WebGLTexelFormat GetWebGLTexelFormat(GLenum format, GLenum type);

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

class WebGLContext :
    public nsIDOMWebGLRenderingContext,
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference,
    public WebGLRectangleObject,
    public nsWrapperCache
{
    friend class WebGLContextUserData;
    friend class WebGLMemoryPressureObserver;
    friend class WebGLMemoryMultiReporterWrapper;
    friend class WebGLExtensionLoseContext;
    friend class WebGLExtensionCompressedTextureS3TC;
    friend class WebGLExtensionCompressedTextureATC;
    friend class WebGLExtensionCompressedTexturePVRTC;
    friend class WebGLExtensionDepthTexture;

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
    virtual ~WebGLContext();

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(WebGLContext,
                                                           nsIDOMWebGLRenderingContext)

    virtual JSObject* WrapObject(JSContext *cx,
                                 JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

    NS_DECL_NSIDOMWEBGLRENDERINGCONTEXT

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetDimensions(int32_t width, int32_t height);
    NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, int32_t width, int32_t height)
        { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Reset()
        { /* (InitializeWithSurface) */ return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Render(gfxContext *ctx,
                      gfxPattern::GraphicsFilter f,
                      uint32_t aFlags = RenderFlagPremultAlpha);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot()
        { return nullptr; }

    NS_IMETHOD SetIsOpaque(bool b) { return NS_OK; };
    NS_IMETHOD SetContextOptions(nsIPropertyBag *aOptions);

    NS_IMETHOD SetIsIPC(bool b) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Redraw(const gfxRect&) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(mozilla::ipc::Shmem& aBack,
                    int32_t x, int32_t y, int32_t w, int32_t h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD Swap(uint32_t nativeID,
                    int32_t x, int32_t y, int32_t w, int32_t h)
                    { return NS_ERROR_NOT_IMPLEMENTED; }

    bool LoseContext();
    bool RestoreContext();

    void SynthesizeGLError(WebGLenum err);
    void SynthesizeGLError(WebGLenum err, const char *fmt, ...);

    void ErrorInvalidEnum(const char *fmt = 0, ...);
    void ErrorInvalidOperation(const char *fmt = 0, ...);
    void ErrorInvalidValue(const char *fmt = 0, ...);
    void ErrorInvalidFramebufferOperation(const char *fmt = 0, ...);
    void ErrorInvalidEnumInfo(const char *info, WebGLenum enumvalue) {
        return ErrorInvalidEnum("%s: invalid enum value 0x%x", info, enumvalue);
    }
    void ErrorOutOfMemory(const char *fmt = 0, ...);

    const char *ErrorName(GLenum error);
    bool IsTextureFormatCompressed(GLenum format);

    void DummyFramebufferOperation(const char *info);

    WebGLTexture *activeBoundTextureForTarget(WebGLenum target) {
        return target == LOCAL_GL_TEXTURE_2D ? mBound2DTextures[mActiveTexture]
                                             : mBoundCubeMapTextures[mActiveTexture];
    }

    already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                 CanvasLayer *aOldLayer,
                                                 LayerManager *aManager);

    // Note that 'clean' here refers to its invalidation state, not the
    // contents of the buffer.
    void MarkContextClean() { mInvalidated = false; }

    gl::GLContext* GL() const {
        return gl;
    }

    bool IsPremultAlpha() const {
        return mOptions.premultipliedAlpha;
    }

    bool PresentScreenBuffer();

    // a number that increments every time we have an event that causes
    // all context resources to be lost.
    uint32_t Generation() { return mGeneration.value(); }

    const WebGLRectangleObject *FramebufferRectangleObject() const;

    // This is similar to GLContext::ClearSafely, but tries to minimize the
    // amount of work it does.
    // It only clears the buffers we specify, and can reset its state without
    // first having to query anything, as WebGL knows its state at all times.
    void ForceClearFramebufferWithDefaultValues(GLbitfield mask);

    // Calls ForceClearFramebufferWithDefaultValues() for the Context's 'screen'.
    void ClearScreen();

    // checks for GL errors, clears any pending GL error, stores the current GL error in currentGLError,
    // and copies it into mWebGLError if it doesn't already have an error set
    void UpdateWebGLErrorAndClearGLError(GLenum *currentGLError) {
        // get and clear GL error in ALL cases
        *currentGLError = gl->GetAndClearError();
        // only store in mWebGLError if is hasn't already recorded an error
        if (!mWebGLError)
            mWebGLError = *currentGLError;
    }
    
    // checks for GL errors, clears any pending GL error,
    // and stores the current GL error into mWebGLError if it doesn't already have an error set
    void UpdateWebGLErrorAndClearGLError() {
        GLenum currentGLError;
        UpdateWebGLErrorAndClearGLError(&currentGLError);
    }
    
    bool MinCapabilityMode() const {
        return mMinCapability;
    }

    void RobustnessTimerCallback(nsITimer* timer);

    static void RobustnessTimerCallbackStatic(nsITimer* timer, void *thisPointer) {
        static_cast<WebGLContext*>(thisPointer)->RobustnessTimerCallback(timer);
    }

    void SetupContextLossTimer() {
        // If the timer was already running, don't restart it here. Instead,
        // wait until the previous call is done, then fire it one more time.
        // This is an optimization to prevent unnecessary cross-communication
        // between threads.
        if (mContextLossTimerRunning) {
            mDrawSinceContextLossTimerSet = true;
            return;
        }

        mContextRestorer->InitWithFuncCallback(RobustnessTimerCallbackStatic,
                                               static_cast<void*>(this),
                                               1000,
                                               nsITimer::TYPE_ONE_SHOT);
        mContextLossTimerRunning = true;
        mDrawSinceContextLossTimerSet = false;
    }

    void TerminateContextLossTimer() {
        if (mContextLossTimerRunning) {
            mContextRestorer->Cancel();
            mContextLossTimerRunning = false;
        }
    }

    // WebIDL WebGLRenderingContext API
    dom::HTMLCanvasElement* GetCanvas() const {
        return mCanvasElement;
    }
    WebGLsizei DrawingBufferWidth() const {
        if (!IsContextStable())
            return 0;
        return mWidth;
    }
    WebGLsizei DrawingBufferHeight() const {
        if (!IsContextStable())
            return 0;
        return mHeight;
    }
        
    void GetContextAttributes(dom::Nullable<dom::WebGLContextAttributesInitializer>& retval);
    bool IsContextLost() const { return !IsContextStable(); }
    void GetSupportedExtensions(JSContext *cx, dom::Nullable< nsTArray<nsString> > &retval);
    JSObject* GetExtension(JSContext* cx, const nsAString& aName, ErrorResult& rv);
    void ActiveTexture(WebGLenum texture);
    void AttachShader(WebGLProgram* program, WebGLShader* shader);
    void BindAttribLocation(WebGLProgram* program, WebGLuint location,
                            const nsAString& name);
    void BindBuffer(WebGLenum target, WebGLBuffer* buf);
    void BindFramebuffer(WebGLenum target, WebGLFramebuffer* wfb);
    void BindRenderbuffer(WebGLenum target, WebGLRenderbuffer* wrb);
    void BindTexture(WebGLenum target, WebGLTexture *tex);
    void BlendColor(WebGLclampf r, WebGLclampf g, WebGLclampf b, WebGLclampf a) {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fBlendColor(r, g, b, a);
    }
    void BlendEquation(WebGLenum mode);
    void BlendEquationSeparate(WebGLenum modeRGB, WebGLenum modeAlpha);
    void BlendFunc(WebGLenum sfactor, WebGLenum dfactor);
    void BlendFuncSeparate(WebGLenum srcRGB, WebGLenum dstRGB,
                           WebGLenum srcAlpha, WebGLenum dstAlpha);
    void BufferData(WebGLenum target, WebGLsizeiptr size, WebGLenum usage);
    void BufferData(WebGLenum target, dom::ArrayBufferView &data,
                    WebGLenum usage);
    void BufferData(WebGLenum target, dom::ArrayBuffer *data, WebGLenum usage);
    void BufferSubData(WebGLenum target, WebGLsizeiptr byteOffset,
                       dom::ArrayBufferView &data);
    void BufferSubData(WebGLenum target, WebGLsizeiptr byteOffset,
                       dom::ArrayBuffer *data);
    WebGLenum CheckFramebufferStatus(WebGLenum target);
    void Clear(WebGLbitfield mask);
    void ClearColor(WebGLclampf r, WebGLclampf g, WebGLclampf b, WebGLclampf a);
    void ClearDepth(WebGLclampf v);
    void ClearStencil(WebGLint v);
    void ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b, WebGLboolean a);
    void CompileShader(WebGLShader *shader);
    void CompressedTexImage2D(WebGLenum target, WebGLint level,
                              WebGLenum internalformat, WebGLsizei width,
                              WebGLsizei height, WebGLint border,
                              dom::ArrayBufferView& view);
    void CompressedTexSubImage2D(WebGLenum target, WebGLint level,
                                 WebGLint xoffset, WebGLint yoffset,
                                 WebGLsizei width, WebGLsizei height,
                                 WebGLenum format, dom::ArrayBufferView& view);
    void CopyTexImage2D(WebGLenum target, WebGLint level,
                        WebGLenum internalformat, WebGLint x, WebGLint y,
                        WebGLsizei width, WebGLsizei height, WebGLint border);
    void CopyTexSubImage2D(WebGLenum target, WebGLint level, WebGLint xoffset,
                           WebGLint yoffset, WebGLint x, WebGLint y,
                           WebGLsizei width, WebGLsizei height);
    already_AddRefed<WebGLBuffer> CreateBuffer();
    already_AddRefed<WebGLFramebuffer> CreateFramebuffer();
    already_AddRefed<WebGLProgram> CreateProgram();
    already_AddRefed<WebGLRenderbuffer> CreateRenderbuffer();
    already_AddRefed<WebGLTexture> CreateTexture();
    already_AddRefed<WebGLShader> CreateShader(WebGLenum type);
    void CullFace(WebGLenum face);
    void DeleteBuffer(WebGLBuffer *buf);
    void DeleteFramebuffer(WebGLFramebuffer *fbuf);
    void DeleteProgram(WebGLProgram *prog);
    void DeleteRenderbuffer(WebGLRenderbuffer *rbuf);
    void DeleteShader(WebGLShader *shader);
    void DeleteTexture(WebGLTexture *tex);
    void DepthFunc(WebGLenum func);
    void DepthMask(WebGLboolean b);
    void DepthRange(WebGLclampf zNear, WebGLclampf zFar);
    void DetachShader(WebGLProgram *program, WebGLShader *shader);
    void Disable(WebGLenum cap);
    void DisableVertexAttribArray(WebGLuint index);
    void DrawArrays(GLenum mode, WebGLint first, WebGLsizei count);
    void DrawElements(WebGLenum mode, WebGLsizei count, WebGLenum type,
                      WebGLintptr byteOffset);
    void Enable(WebGLenum cap);
    void EnableVertexAttribArray(WebGLuint index);
    void Flush() {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fFlush();
    }
    void Finish() {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fFinish();
    }
    void FramebufferRenderbuffer(WebGLenum target, WebGLenum attachment,
                                 WebGLenum rbtarget, WebGLRenderbuffer *wrb);
    void FramebufferTexture2D(WebGLenum target, WebGLenum attachment,
                              WebGLenum textarget, WebGLTexture *tobj,
                              WebGLint level);
    void FrontFace(WebGLenum mode);
    void GenerateMipmap(WebGLenum target);
    already_AddRefed<WebGLActiveInfo> GetActiveAttrib(WebGLProgram *prog,
                                                      WebGLuint index);
    already_AddRefed<WebGLActiveInfo> GetActiveUniform(WebGLProgram *prog,
                                                       WebGLuint index);
    void GetAttachedShaders(WebGLProgram* prog,
                            dom::Nullable< nsTArray<WebGLShader*> > &retval);
    WebGLint GetAttribLocation(WebGLProgram* prog, const nsAString& name);
    JS::Value GetBufferParameter(WebGLenum target, WebGLenum pname);
    JS::Value GetBufferParameter(JSContext* /* unused */, WebGLenum target,
                                 WebGLenum pname) {
        return GetBufferParameter(target, pname);
    }
    JS::Value GetParameter(JSContext* cx, WebGLenum pname, ErrorResult& rv);
    WebGLenum GetError();
    JS::Value GetFramebufferAttachmentParameter(JSContext* cx,
                                                WebGLenum target,
                                                WebGLenum attachment,
                                                WebGLenum pname,
                                                ErrorResult& rv);
    JS::Value GetProgramParameter(WebGLProgram *prog, WebGLenum pname);
    JS::Value GetProgramParameter(JSContext* /* unused */, WebGLProgram *prog,
                                  WebGLenum pname) {
        return GetProgramParameter(prog, pname);
    }
    void GetProgramInfoLog(WebGLProgram *prog, nsACString& retval);
    void GetProgramInfoLog(WebGLProgram *prog, nsAString& retval);
    JS::Value GetRenderbufferParameter(WebGLenum target, WebGLenum pname);
    JS::Value GetRenderbufferParameter(JSContext* /* unused */,
                                       WebGLenum target, WebGLenum pname) {
        return GetRenderbufferParameter(target, pname);
    }
    JS::Value GetShaderParameter(WebGLShader *shader, WebGLenum pname);
    JS::Value GetShaderParameter(JSContext* /* unused */, WebGLShader *shader,
                                 WebGLenum pname) {
        return GetShaderParameter(shader, pname);
    }
    already_AddRefed<WebGLShaderPrecisionFormat>
      GetShaderPrecisionFormat(WebGLenum shadertype, WebGLenum precisiontype);
    void GetShaderInfoLog(WebGLShader *shader, nsACString& retval);
    void GetShaderInfoLog(WebGLShader *shader, nsAString& retval);
    void GetShaderSource(WebGLShader *shader, nsAString& retval);
    JS::Value GetTexParameter(WebGLenum target, WebGLenum pname);
    JS::Value GetTexParameter(JSContext * /* unused */, WebGLenum target,
                              WebGLenum pname) {
        return GetTexParameter(target, pname);
    }
    JS::Value GetUniform(JSContext* cx, WebGLProgram *prog,
                         WebGLUniformLocation *location, ErrorResult& rv);
    already_AddRefed<WebGLUniformLocation>
      GetUniformLocation(WebGLProgram *prog, const nsAString& name);
    JS::Value GetVertexAttrib(JSContext* cx, WebGLuint index, WebGLenum pname,
                              ErrorResult& rv);
    WebGLsizeiptr GetVertexAttribOffset(WebGLuint index, WebGLenum pname);
    void Hint(WebGLenum target, WebGLenum mode);
    bool IsBuffer(WebGLBuffer *buffer);
    bool IsEnabled(WebGLenum cap);
    bool IsFramebuffer(WebGLFramebuffer *fb);
    bool IsProgram(WebGLProgram *prog);
    bool IsRenderbuffer(WebGLRenderbuffer *rb);
    bool IsShader(WebGLShader *shader);
    bool IsTexture(WebGLTexture *tex);
    void LineWidth(WebGLfloat width) {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fLineWidth(width);
    }
    void LinkProgram(WebGLProgram *program);
    void PixelStorei(WebGLenum pname, WebGLint param);
    void PolygonOffset(WebGLfloat factor, WebGLfloat units) {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fPolygonOffset(factor, units);
    }
    void ReadPixels(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                    WebGLenum format, WebGLenum type,
                    dom::ArrayBufferView* pixels, ErrorResult& rv);
    void RenderbufferStorage(WebGLenum target, WebGLenum internalformat,
                             WebGLsizei width, WebGLsizei height);
    void SampleCoverage(WebGLclampf value, WebGLboolean invert) {
        if (!IsContextStable())
            return;
        MakeContextCurrent();
        gl->fSampleCoverage(value, invert);
    }
    void Scissor(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height);
    void ShaderSource(WebGLShader *shader, const nsAString& source);
    void StencilFunc(WebGLenum func, WebGLint ref, WebGLuint mask);
    void StencilFuncSeparate(WebGLenum face, WebGLenum func, WebGLint ref,
                             WebGLuint mask);
    void StencilMask(WebGLuint mask);
    void StencilMaskSeparate(WebGLenum face, WebGLuint mask);
    void StencilOp(WebGLenum sfail, WebGLenum dpfail, WebGLenum dppass);
    void StencilOpSeparate(WebGLenum face, WebGLenum sfail, WebGLenum dpfail,
                           WebGLenum dppass);
    void TexImage2D(WebGLenum target, WebGLint level,
                    WebGLenum internalformat, WebGLsizei width,
                    WebGLsizei height, WebGLint border, WebGLenum format,
                    WebGLenum type, dom::ArrayBufferView *pixels,
                    ErrorResult& rv);
    void TexImage2D(WebGLenum target, WebGLint level,
                    WebGLenum internalformat, WebGLenum format, WebGLenum type,
                    dom::ImageData* pixels, ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexImage2D
    template<class ElementType>
    void TexImage2D(WebGLenum target, WebGLint level,
                    WebGLenum internalformat, WebGLenum format, WebGLenum type,
                    const ElementType& elt, ErrorResult& rv) {
        if (!IsContextStable())
            return;
        nsRefPtr<gfxImageSurface> isurf;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, getter_AddRefs(isurf),
                                                    &srcFormat);
        if (rv.Failed())
            return;

        uint32_t byteLength = isurf->Stride() * isurf->Height();
        return TexImage2D_base(target, level, internalformat,
                               isurf->Width(), isurf->Height(), isurf->Stride(),
                               0, format, type, isurf->Data(), byteLength,
                               -1, srcFormat, mPixelStorePremultiplyAlpha);
    }
    void TexParameterf(WebGLenum target, WebGLenum pname, WebGLfloat param) {
        TexParameter_base(target, pname, nullptr, &param);
    }
    void TexParameteri(WebGLenum target, WebGLenum pname, WebGLint param) {
        TexParameter_base(target, pname, &param, nullptr);
    }
    
    void TexSubImage2D(WebGLenum target, WebGLint level,
                       WebGLint xoffset, WebGLint yoffset,
                       WebGLsizei width, WebGLsizei height, WebGLenum format,
                       WebGLenum type, dom::ArrayBufferView* pixels,
                       ErrorResult& rv);
    void TexSubImage2D(WebGLenum target, WebGLint level,
                       WebGLint xoffset, WebGLint yoffset, WebGLenum format,
                       WebGLenum type, dom::ImageData* pixels, ErrorResult& rv);
    // Allow whatever element types the bindings are willing to pass
    // us in TexSubImage2D
    template<class ElementType>
    void TexSubImage2D(WebGLenum target, WebGLint level,
                       WebGLint xoffset, WebGLint yoffset, WebGLenum format,
                       WebGLenum type, const ElementType& elt, ErrorResult& rv) {
        if (!IsContextStable())
            return;
        nsRefPtr<gfxImageSurface> isurf;
        WebGLTexelFormat srcFormat;
        nsLayoutUtils::SurfaceFromElementResult res = SurfaceFromElement(elt);
        rv = SurfaceFromElementResultToImageSurface(res, getter_AddRefs(isurf),
                                                    &srcFormat);
        if (rv.Failed())
            return;

        uint32_t byteLength = isurf->Stride() * isurf->Height();
        return TexSubImage2D_base(target, level, xoffset, yoffset,
                                  isurf->Width(), isurf->Height(),
                                  isurf->Stride(), format, type,
                                  isurf->Data(), byteLength,
                                  -1, srcFormat, mPixelStorePremultiplyAlpha);
        
    }

    void Uniform1i(WebGLUniformLocation* location, WebGLint x);
    void Uniform2i(WebGLUniformLocation* location, WebGLint x, WebGLint y);
    void Uniform3i(WebGLUniformLocation* location, WebGLint x, WebGLint y,
                   WebGLint z);
    void Uniform4i(WebGLUniformLocation* location, WebGLint x, WebGLint y,
                   WebGLint z, WebGLint w);

    void Uniform1f(WebGLUniformLocation* location, WebGLfloat x);
    void Uniform2f(WebGLUniformLocation* location, WebGLfloat x, WebGLfloat y);
    void Uniform3f(WebGLUniformLocation* location, WebGLfloat x, WebGLfloat y,
                   WebGLfloat z);
    void Uniform4f(WebGLUniformLocation* location, WebGLfloat x, WebGLfloat y,
                   WebGLfloat z, WebGLfloat w);
    
    void Uniform1iv(WebGLUniformLocation* location, dom::Int32Array& arr) {
        Uniform1iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform1iv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLint>& arr) {
        Uniform1iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform1iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLint* data);

    void Uniform2iv(WebGLUniformLocation* location, dom::Int32Array& arr) {
        Uniform2iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform2iv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLint>& arr) {
        Uniform2iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform2iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLint* data);

    void Uniform3iv(WebGLUniformLocation* location, dom::Int32Array& arr) {
        Uniform3iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform3iv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLint>& arr) {
        Uniform3iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform3iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLint* data);
    
    void Uniform4iv(WebGLUniformLocation* location, dom::Int32Array& arr) {
        Uniform4iv_base(location, arr.Length(), arr.Data());
    }
    void Uniform4iv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLint>& arr) {
        Uniform4iv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform4iv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLint* data);

    void Uniform1fv(WebGLUniformLocation* location, dom::Float32Array& arr) {
        Uniform1fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform1fv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLfloat>& arr) {
        Uniform1fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform1fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLfloat* data);

    void Uniform2fv(WebGLUniformLocation* location, dom::Float32Array& arr) {
        Uniform2fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform2fv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLfloat>& arr) {
        Uniform2fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform2fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLfloat* data);

    void Uniform3fv(WebGLUniformLocation* location, dom::Float32Array& arr) {
        Uniform3fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform3fv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLfloat>& arr) {
        Uniform3fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform3fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLfloat* data);
    
    void Uniform4fv(WebGLUniformLocation* location, dom::Float32Array& arr) {
        Uniform4fv_base(location, arr.Length(), arr.Data());
    }
    void Uniform4fv(WebGLUniformLocation* location,
                    const dom::Sequence<WebGLfloat>& arr) {
        Uniform4fv_base(location, arr.Length(), arr.Elements());
    }
    void Uniform4fv_base(WebGLUniformLocation* location, uint32_t arrayLength,
                         const WebGLfloat* data);

    void UniformMatrix2fv(WebGLUniformLocation* location,
                          WebGLboolean transpose,
                          dom::Float32Array &value) {
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
                          dom::Float32Array &value) {
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
                          dom::Float32Array &value) {
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
                                    WebGLint value);

    void VertexAttrib1f(WebGLuint index, WebGLfloat x0);
    void VertexAttrib2f(WebGLuint index, WebGLfloat x0, WebGLfloat x1);
    void VertexAttrib3f(WebGLuint index, WebGLfloat x0, WebGLfloat x1,
                        WebGLfloat x2);
    void VertexAttrib4f(WebGLuint index, WebGLfloat x0, WebGLfloat x1,
                        WebGLfloat x2, WebGLfloat x3);

    void VertexAttrib1fv(WebGLuint idx, dom::Float32Array &arr) {
        VertexAttrib1fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib1fv(WebGLuint idx, const dom::Sequence<WebGLfloat>& arr) {
        VertexAttrib1fv_base(idx, arr.Length(), arr.Elements());
    }
    void VertexAttrib1fv_base(WebGLuint idx, uint32_t arrayLength,
                              const WebGLfloat* ptr);

    void VertexAttrib2fv(WebGLuint idx, dom::Float32Array &arr) {
        VertexAttrib2fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib2fv(WebGLuint idx, const dom::Sequence<WebGLfloat>& arr) {
        VertexAttrib2fv_base(idx, arr.Length(), arr.Elements());
    }
    void VertexAttrib2fv_base(WebGLuint idx, uint32_t arrayLength,
                              const WebGLfloat* ptr);

    void VertexAttrib3fv(WebGLuint idx, dom::Float32Array &arr) {
        VertexAttrib3fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib3fv(WebGLuint idx, const dom::Sequence<WebGLfloat>& arr) {
        VertexAttrib3fv_base(idx, arr.Length(), arr.Elements());
    }
    void VertexAttrib3fv_base(WebGLuint idx, uint32_t arrayLength,
                              const WebGLfloat* ptr);

    void VertexAttrib4fv(WebGLuint idx, dom::Float32Array &arr) {
        VertexAttrib4fv_base(idx, arr.Length(), arr.Data());
    }
    void VertexAttrib4fv(WebGLuint idx, const dom::Sequence<WebGLfloat>& arr) {
        VertexAttrib4fv_base(idx, arr.Length(), arr.Elements());
    }
    void VertexAttrib4fv_base(WebGLuint idx, uint32_t arrayLength,
                              const WebGLfloat* ptr);
    
    void VertexAttribPointer(WebGLuint index, WebGLint size, WebGLenum type,
                             WebGLboolean normalized, WebGLsizei stride,
                             WebGLintptr byteOffset);
    void Viewport(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height);

protected:
    void SetDontKnowIfNeedFakeBlack() {
        mFakeBlackStatus = DontKnowIfNeedFakeBlack;
    }

    bool NeedFakeBlack();
    void BindFakeBlackTextures();
    void UnbindFakeBlackTextures();

    int WhatDoesVertexAttrib0Need();
    bool DoFakeVertexAttrib0(WebGLuint vertexCount);
    void UndoFakeVertexAttrib0();
    void InvalidateFakeVertexAttrib0();

    static CheckedUint32 GetImageSize(WebGLsizei height, 
                                      WebGLsizei width, 
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
    bool mHasRobustness;
    bool mIsMesa;
    bool mLoseContextOnHeapMinimize;
    bool mCanLoseContextInForeground;
    bool mShouldPresent;
    bool mIsScreenCleared;
    bool mDisableFragHighP;

    template<typename WebGLObjectType>
    void DeleteWebGLObjectsArray(nsTArray<WebGLObjectType>& array);

    WebGLuint mActiveTexture;
    WebGLenum mWebGLError;

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

    // Cache the max number of elements that can be read from bound VBOs
    // (result of ValidateBuffers).
    bool mMinInUseAttribArrayLengthCached;
    uint32_t mMinInUseAttribArrayLength;

    inline void InvalidateCachedMinInUseAttribArrayLength()
    {
        mMinInUseAttribArrayLengthCached = false;
        mMinInUseAttribArrayLength = 0;
    }

    // Represents current status, or state, of the context. That is, is it lost
    // or stable and what part of the context lost process are we currently at.
    // This is used to support the WebGL spec's asyncronous nature in handling
    // context loss.
    enum ContextStatus {
        // The context is stable; there either are none or we don't know of any.
        ContextStable,
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

    // extensions
    enum WebGLExtensionID {
        EXT_texture_filter_anisotropic,
        OES_element_index_uint,
        OES_standard_derivatives,
        OES_texture_float,
        WEBGL_compressed_texture_atc,
        WEBGL_compressed_texture_pvrtc,
        WEBGL_compressed_texture_s3tc,
        WEBGL_debug_renderer_info,
        WEBGL_depth_texture,
        WEBGL_lose_context,
        WebGLExtensionID_unknown_extension
    };
    nsTArray<nsRefPtr<WebGLExtensionBase> > mExtensions;

    // returns true if the extension has been enabled by calling getExtension.
    bool IsExtensionEnabled(WebGLExtensionID ext) const;

    // returns true if the extension is supported for this JSContext (this decides what getSupportedExtensions exposes)
    bool IsExtensionSupported(JSContext *cx, WebGLExtensionID ext) const;

    nsTArray<WebGLenum> mCompressedTextureFormats;

    bool InitAndValidateGL();
    bool ValidateBuffers(uint32_t *maxAllowedCount, const char *info);
    bool ValidateCapabilityEnum(WebGLenum cap, const char *info);
    bool ValidateBlendEquationEnum(WebGLenum cap, const char *info);
    bool ValidateBlendFuncDstEnum(WebGLenum mode, const char *info);
    bool ValidateBlendFuncSrcEnum(WebGLenum mode, const char *info);
    bool ValidateBlendFuncEnumsCompatibility(WebGLenum sfactor, WebGLenum dfactor, const char *info);
    bool ValidateTextureTargetEnum(WebGLenum target, const char *info);
    bool ValidateComparisonEnum(WebGLenum target, const char *info);
    bool ValidateStencilOpEnum(WebGLenum action, const char *info);
    bool ValidateFaceEnum(WebGLenum face, const char *info);
    bool ValidateBufferUsageEnum(WebGLenum target, const char *info);
    bool ValidateTexFormatAndType(WebGLenum format, WebGLenum type, int jsArrayType,
                                      uint32_t *texelSize, const char *info);
    bool ValidateDrawModeEnum(WebGLenum mode, const char *info);
    bool ValidateAttribIndex(WebGLuint index, const char *info);
    bool ValidateStencilParamsForDrawCall();
    
    bool ValidateGLSLVariableName(const nsAString& name, const char *info);
    bool ValidateGLSLCharacter(PRUnichar c);
    bool ValidateGLSLString(const nsAString& string, const char *info);

    bool ValidateTexImage2DTarget(WebGLenum target, WebGLsizei width, WebGLsizei height, const char* info);
    bool ValidateCompressedTextureSize(WebGLenum target, WebGLint level, WebGLenum format, WebGLsizei width, WebGLsizei height, uint32_t byteLength, const char* info);
    bool ValidateLevelWidthHeightForTarget(WebGLenum target, WebGLint level, WebGLsizei width, WebGLsizei height, const char* info);

    static uint32_t GetBitsPerTexel(WebGLenum format, WebGLenum type);

    void Invalidate();
    void DestroyResourcesAndContext();

    void MakeContextCurrent() { gl->MakeCurrent(); }

    // helpers
    void TexImage2D_base(WebGLenum target, WebGLint level, WebGLenum internalformat,
                         WebGLsizei width, WebGLsizei height, WebGLsizei srcStrideOrZero, WebGLint border,
                         WebGLenum format, WebGLenum type,
                         void *data, uint32_t byteLength,
                         int jsArrayType,
                         WebGLTexelFormat srcFormat, bool srcPremultiplied);
    void TexSubImage2D_base(WebGLenum target, WebGLint level,
                            WebGLint xoffset, WebGLint yoffset,
                            WebGLsizei width, WebGLsizei height, WebGLsizei srcStrideOrZero,
                            WebGLenum format, WebGLenum type,
                            void *pixels, uint32_t byteLength,
                            int jsArrayType,
                            WebGLTexelFormat srcFormat, bool srcPremultiplied);
    void TexParameter_base(WebGLenum target, WebGLenum pname,
                           WebGLint *intParamPtr, WebGLfloat *floatParamPtr);

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
            flags |= nsLayoutUtils::SFE_NO_PREMULTIPLY_ALPHA;
        return nsLayoutUtils::SurfaceFromElement(aElement, flags);
    }
    template<class ElementType>
    nsLayoutUtils::SurfaceFromElementResult SurfaceFromElement(const dom::NonNull<ElementType>& aElement) {
      return SurfaceFromElement(aElement.get());
    }

    nsresult SurfaceFromElementResultToImageSurface(nsLayoutUtils::SurfaceFromElementResult& res,
                                                    gfxImageSurface **imageOut,
                                                    WebGLTexelFormat *format);

    void CopyTexSubImage2D_base(WebGLenum target,
                                WebGLint level,
                                WebGLenum internalformat,
                                WebGLint xoffset,
                                WebGLint yoffset,
                                WebGLint x,
                                WebGLint y,
                                WebGLsizei width,
                                WebGLsizei height,
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
    int32_t MaxTextureSizeForTarget(WebGLenum target) const {
        return target == LOCAL_GL_TEXTURE_2D ? mGLMaxTextureSize : mGLMaxCubeMapTextureSize;
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

    void MaybeRestoreContext();
    bool IsContextStable() const {
        return mContextStatus == ContextStable;
    }
    void ForceLoseContext();
    void ForceRestoreContext();

    // the buffers bound to the current program's attribs
    nsTArray<WebGLVertexAttribData> mAttribBuffers;

    nsTArray<WebGLRefPtr<WebGLTexture> > mBound2DTextures;
    nsTArray<WebGLRefPtr<WebGLTexture> > mBoundCubeMapTextures;

    WebGLRefPtr<WebGLBuffer> mBoundArrayBuffer;
    WebGLRefPtr<WebGLBuffer> mBoundElementArrayBuffer;

    WebGLRefPtr<WebGLProgram> mCurrentProgram;

    uint32_t mMaxFramebufferColorAttachments;

    WebGLRefPtr<WebGLFramebuffer> mBoundFramebuffer;
    WebGLRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;

    LinkedList<WebGLTexture> mTextures;
    LinkedList<WebGLBuffer> mBuffers;
    LinkedList<WebGLProgram> mPrograms;
    LinkedList<WebGLShader> mShaders;
    LinkedList<WebGLRenderbuffer> mRenderbuffers;
    LinkedList<WebGLFramebuffer> mFramebuffers;

    // PixelStore parameters
    uint32_t mPixelStorePackAlignment, mPixelStoreUnpackAlignment, mPixelStoreColorspaceConversion;
    bool mPixelStoreFlipY, mPixelStorePremultiplyAlpha;

    FakeBlackStatus mFakeBlackStatus;

    WebGLuint mBlackTexture2D, mBlackTextureCubeMap;
    bool mBlackTexturesAreInitialized;

    WebGLfloat mVertexAttrib0Vector[4];
    WebGLfloat mFakeVertexAttrib0BufferObjectVector[4];
    size_t mFakeVertexAttrib0BufferObjectSize;
    GLuint mFakeVertexAttrib0BufferObject;
    int mFakeVertexAttrib0BufferStatus;

    WebGLint mStencilRefFront, mStencilRefBack;
    WebGLuint mStencilValueMaskFront, mStencilValueMaskBack,
              mStencilWriteMaskFront, mStencilWriteMaskBack;
    realGLboolean mColorWriteMask[4];
    realGLboolean mDepthWriteMask;
    realGLboolean mScissorTestEnabled;
    realGLboolean mDitherEnabled;
    WebGLfloat mColorClearValue[4];
    WebGLint mStencilClearValue;
    WebGLfloat mDepthClearValue;

    nsCOMPtr<nsITimer> mContextRestorer;
    bool mAllowRestore;
    bool mContextLossTimerRunning;
    bool mDrawSinceContextLossTimerSet;
    ContextStatus mContextStatus;
    bool mContextLostErrorSet;

    int mAlreadyGeneratedWarnings;
    bool mAlreadyWarnedAboutFakeVertexAttrib0;

    bool ShouldGenerateWarnings() const {
        return mAlreadyGeneratedWarnings < 32;
    }

    uint64_t mLastUseIndex;

    void LoseOldestWebGLContextIfLimitExceeded();
    void UpdateLastUseIndex();

    template <typename WebGLObjectType>
    JS::Value WebGLObjectAsJSValue(JSContext *cx, const WebGLObjectType *, ErrorResult& rv) const;
    template <typename WebGLObjectType>
    JSObject* WebGLObjectAsJSObject(JSContext *cx, const WebGLObjectType *, ErrorResult& rv) const;

    void ReattachTextureToAnyFramebufferToWorkAroundBugs(WebGLTexture *tex, WebGLint level);

#ifdef XP_MACOSX
    // see bug 713305. This RAII helper guarantees that we're on the discrete GPU, during its lifetime
    // Debouncing note: we don't want to switch GPUs too frequently, so try to not create and destroy
    // these objects at high frequency. Having WebGLContext's hold one such object seems fine,
    // because WebGLContext objects only go away during GC, which shouldn't happen too frequently.
    // If in the future GC becomes much more frequent, we may have to revisit then (maybe use a timer).
    ForceDiscreteGPUHelperCGL mForceDiscreteGPUHelper;
#endif

    nsRefPtr<WebGLMemoryPressureObserver> mMemoryPressureObserver;

public:
    // console logging helpers
    void GenerateWarning(const char *fmt, ...);
    void GenerateWarning(const char *fmt, va_list ap);

    friend class WebGLTexture;
    friend class WebGLFramebuffer;
    friend class WebGLRenderbuffer;
    friend class WebGLProgram;
    friend class WebGLBuffer;
    friend class WebGLShader;
    friend class WebGLUniformLocation;
};

// used by DOM bindings in conjunction with GetParentObject
inline nsISupports*
ToSupports(WebGLContext* context)
{
  return static_cast<nsIDOMWebGLRenderingContext*>(context);
}

class WebGLActiveInfo MOZ_FINAL
    : public nsISupports
{
public:
    WebGLActiveInfo(WebGLint size, WebGLenum type, const nsACString& name) :
        mSize(size),
        mType(type),
        mName(NS_ConvertASCIItoUTF16(name))
    {}

    // WebIDL attributes

    WebGLint Size() const {
        return mSize;
    }

    WebGLenum Type() const {
        return mType;
    }

    void GetName(nsString& retval) const {
        retval = mName;
    }

    JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> scope);

    NS_DECL_ISUPPORTS

protected:
    WebGLint mSize;
    WebGLenum mType;
    nsString mName;
};


inline const WebGLRectangleObject *WebGLContext::FramebufferRectangleObject() const {
    return mBoundFramebuffer ? mBoundFramebuffer->RectangleObject()
                             : static_cast<const WebGLRectangleObject*>(this);
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

class WebGLMemoryMultiReporterWrapper
{
    WebGLMemoryMultiReporterWrapper();
    ~WebGLMemoryMultiReporterWrapper();
    static WebGLMemoryMultiReporterWrapper* sUniqueInstance;

    // here we store plain pointers, not RefPtrs: we don't want the 
    // WebGLMemoryMultiReporterWrapper unique instance to keep alive all		
    // WebGLContexts ever created.
    typedef nsTArray<const WebGLContext*> ContextsArrayType;
    ContextsArrayType mContexts;

    nsCOMPtr<nsIMemoryMultiReporter> mReporter;

    static WebGLMemoryMultiReporterWrapper* UniqueInstance();

    static ContextsArrayType & Contexts() { return UniqueInstance()->mContexts; }

    friend class WebGLContext;

  public:

    static void AddWebGLContext(const WebGLContext* c) {
        Contexts().AppendElement(c);
    }

    static void RemoveWebGLContext(const WebGLContext* c) {
        ContextsArrayType & contexts = Contexts();
        contexts.RemoveElement(c);
        if (contexts.IsEmpty()) {
            delete sUniqueInstance; 
            sUniqueInstance = nullptr;
        }
    }

    static int64_t GetTextureMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLTexture *texture = contexts[i]->mTextures.getFirst();
                 texture;
                 texture = texture->getNext())
            {
                result += texture->MemoryUsage();
            }
        }
        return result;
    }

    static int64_t GetTextureCount() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLTexture *texture = contexts[i]->mTextures.getFirst();
                 texture;
                 texture = texture->getNext())
            {
                result++;
            }
        }
        return result;
    }

    static int64_t GetBufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLBuffer *buffer = contexts[i]->mBuffers.getFirst();
                 buffer;
                 buffer = buffer->getNext())
            {
                result += buffer->ByteLength();
            }
        }
        return result;
    }

    static int64_t GetBufferCacheMemoryUsed();

    static int64_t GetBufferCount() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLBuffer *buffer = contexts[i]->mBuffers.getFirst();
                 buffer;
                 buffer = buffer->getNext())
            {
                result++;
            }
        }
        return result;
    }

    static int64_t GetRenderbufferMemoryUsed() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLRenderbuffer *rb = contexts[i]->mRenderbuffers.getFirst();
                 rb;
                 rb = rb->getNext())
            {
                result += rb->MemoryUsage();
            }
        }
        return result;
    }

    static int64_t GetRenderbufferCount() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLRenderbuffer *rb = contexts[i]->mRenderbuffers.getFirst();
                 rb;
                 rb = rb->getNext())
            {
                result++;
            }
        }
        return result;
    }

    static int64_t GetShaderSize();

    static int64_t GetShaderCount() {
        const ContextsArrayType & contexts = Contexts();
        int64_t result = 0;
        for(size_t i = 0; i < contexts.Length(); ++i) {
            for (const WebGLShader *shader = contexts[i]->mShaders.getFirst();
                 shader;
                 shader = shader->getNext())
            {
                result++;
            }
        }
        return result;
    }

    static int64_t GetContextCount() {
        return Contexts().Length();
    }
};

class WebGLMemoryPressureObserver MOZ_FINAL
    : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  WebGLMemoryPressureObserver(WebGLContext *context)
    : mContext(context)
  {}

private:
  WebGLContext *mContext;
};

} // namespace mozilla

#endif
