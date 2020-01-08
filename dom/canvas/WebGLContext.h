/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXT_H_
#define WEBGLCONTEXT_H_

#include <memory>
#include <stdarg.h>

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsLayoutUtils.h"
#include "nsTArray.h"
#include "SurfaceTypes.h"
#include "ScopedGLHelpers.h"
#include "TexUnpackBlob.h"
#include "mozilla/WeakPtr.h"

// Local
#include "CacheInvalidator.h"
#include "WebGLContextLossHandler.h"
#include "WebGLExtensions.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"

// Generated
#include "nsICanvasRenderingContextInternal.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsWrapperCache.h"
#include "nsLayoutUtils.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"

class nsIDocShell;

// WebGL-only GLenums
// clang-format off
#define LOCAL_GL_BROWSER_DEFAULT_WEBGL              0x9244
#define LOCAL_GL_CONTEXT_LOST_WEBGL                 0x9242
#define LOCAL_GL_MAX_CLIENT_WAIT_TIMEOUT_WEBGL      0x9247
#define LOCAL_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL 0x9243
#define LOCAL_GL_UNPACK_FLIP_Y_WEBGL                0x9240
#define LOCAL_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL     0x9241
// clang-format on

namespace mozilla {
class HostWebGLContext;
class ScopedCopyTexImageSource;
class ScopedDrawCallWrapper;
class ScopedResolveTexturesForDraw;
class ScopedUnpackReset;
class WebGLActiveInfo;
class WebGLBuffer;
class WebGLExtensionBase;
class WebGLFramebuffer;
class WebGLProgram;
class WebGLQuery;
class WebGLRenderbuffer;
class WebGLSampler;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLSync;
class WebGLTexture;
class WebGLTransformFeedback;
class WebGLUniformLocation;
class WebGLVertexArray;

namespace dom {
class Document;
class Element;
class ImageData;
class OwningHTMLCanvasElementOrOffscreenCanvas;
struct WebGLContextAttributes;
}  // namespace dom

namespace gfx {
class SourceSurface;
class VRLayerChild;
}  // namespace gfx

namespace gl {
class GLScreenBuffer;
class MozFramebuffer;
}  // namespace gl

namespace layers {
class CompositableHost;
}

namespace webgl {
class AvailabilityRunnable;
struct CachedDrawFetchLimits;
struct FbAttachInfo;
struct FormatInfo;
class FormatUsageAuthority;
struct FormatUsageInfo;
struct ImageInfo;
struct LinkedProgramInfo;
struct SamplingState;
class ScopedPrepForResourceClear;
class ShaderValidator;
class TexUnpackBlob;
struct UniformInfo;
struct UniformBlockInfo;
}  // namespace webgl

WebGLTexelFormat GetWebGLTexelFormat(TexInternalFormat format);

void AssertUintParamCorrect(gl::GLContext* gl, GLenum pname, GLuint shadow);

// From WebGLContextUtils
TexTarget TexImageTargetToTexTarget(TexImageTarget texImageTarget);

struct WebGLIntOrFloat {
  const enum { Int, Float, Uint } mType;

  union {
    GLint i;
    GLfloat f;
    GLuint u;
  } mValue;

  explicit WebGLIntOrFloat(GLint i) : mType(Int) { mValue.i = i; }
  explicit WebGLIntOrFloat(GLfloat f) : mType(Float) { mValue.f = f; }

  GLint AsInt() const {
    return (mType == Int) ? mValue.i : NS_lroundf(mValue.f);
  }
  GLfloat AsFloat() const {
    return (mType == Float) ? mValue.f : GLfloat(mValue.i);
  }
};

struct IndexedBufferBinding {
  WebGLRefPtr<WebGLBuffer> mBufferBinding;
  uint64_t mRangeStart;
  uint64_t mRangeSize;

  IndexedBufferBinding();

  uint64_t ByteCount() const;
};

////////////////////////////////////

namespace webgl {

class AvailabilityRunnable final : public Runnable {
 public:
  const RefPtr<WebGLContext> mWebGL;  // Prevent CC
  std::vector<RefPtr<WebGLQuery>> mQueries;
  std::vector<RefPtr<WebGLSync>> mSyncs;

  explicit AvailabilityRunnable(WebGLContext* webgl);
  ~AvailabilityRunnable();

  NS_IMETHOD Run() override;
};

struct BufferAndIndex final {
  const WebGLBuffer* buffer = nullptr;
  uint32_t id = -1;
};

// -

class DynDGpuManager final {
  static constexpr uint32_t TICK_MS = 3000;

  enum class State {
    Inactive,
    Primed,
    Active,
  };

  Mutex mMutex;
  bool mActivityThisTick = false;
  State mState = State::Inactive;
  RefPtr<gl::GLContext> mDGpuContext;

 public:
  static std::shared_ptr<DynDGpuManager> Get();

  DynDGpuManager();
  ~DynDGpuManager();

  void ReportActivity(const std::shared_ptr<DynDGpuManager>& strong);

 private:
  void SetState(const MutexAutoLock&, State);
  void Tick(const std::shared_ptr<DynDGpuManager>& strong);
  void DispatchTick(const std::shared_ptr<DynDGpuManager>& strong);
};

}  // namespace webgl

////////////////////////////////////////////////////////////////////////////////

class WebGLContext : public SupportsWeakPtr<WebGLContext> {
  friend class ScopedDrawCallWrapper;
  friend class ScopedDrawWithTransformFeedback;
  friend class ScopedFakeVertexAttrib0;
  friend class ScopedFBRebinder;
  friend class WebGL2Context;
  friend class WebGLContextUserData;
  friend class WebGLExtensionCompressedTextureASTC;
  friend class WebGLExtensionCompressedTextureBPTC;
  friend class WebGLExtensionCompressedTextureES3;
  friend class WebGLExtensionCompressedTextureETC1;
  friend class WebGLExtensionCompressedTexturePVRTC;
  friend class WebGLExtensionCompressedTextureRGTC;
  friend class WebGLExtensionCompressedTextureS3TC;
  friend class WebGLExtensionCompressedTextureS3TC_SRGB;
  friend class WebGLExtensionDepthTexture;
  friend class WebGLExtensionDisjointTimerQuery;
  friend class WebGLExtensionDrawBuffers;
  friend class WebGLExtensionLoseContext;
  friend class WebGLExtensionMOZDebug;
  friend class WebGLExtensionVertexArray;
  friend class WebGLMemoryTracker;
  friend class webgl::AvailabilityRunnable;
  friend struct webgl::LinkedProgramInfo;
  friend class webgl::ScopedPrepForResourceClear;
  friend struct webgl::UniformBlockInfo;

  friend const webgl::CachedDrawFetchLimits* ValidateDraw(WebGLContext*, GLenum,
                                                          uint32_t);

  enum {
    UNPACK_FLIP_Y_WEBGL = 0x9240,
    UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241,
    // We throw InvalidOperation in TexImage if we fail to use GPU fast-path
    // for texture copy when it is set to true, only for debug purpose.
    UNPACK_REQUIRE_FASTPATH = 0x10001,
    CONTEXT_LOST_WEBGL = 0x9242,
    UNPACK_COLORSPACE_CONVERSION_WEBGL = 0x9243,
    BROWSER_DEFAULT_WEBGL = 0x9244,
    UNMASKED_VENDOR_WEBGL = 0x9245,
    UNMASKED_RENDERER_WEBGL = 0x9246
  };

 private:
  // We've had issues in the past with nulling `gl` without actually releasing
  // all of our resources. This construction ensures that we are aware that we
  // should only null `gl` in DestroyResourcesAndContext.
  RefPtr<gl::GLContext> mGL_OnlyClearInDestroyResourcesAndContext;

 public:
  // Grab a const reference so we can see changes, but can't make changes.
  const decltype(mGL_OnlyClearInDestroyResourcesAndContext)& gl;

 private:
  std::shared_ptr<webgl::DynDGpuManager> mDynDGpuManager;

  void ReportActivity() const {
    if (mDynDGpuManager) {
      mDynDGpuManager->ReportActivity(mDynDGpuManager);
    }
  }

 public:
  void CheckForInactivity();

 protected:
  const uint32_t mMaxPerfWarnings;
  mutable uint64_t mNumPerfWarnings;
  const uint32_t mMaxAcceptableFBStatusInvals;

  uint64_t mNextFenceId = 1;
  uint64_t mCompletedFenceId = 0;

 public:
  class FuncScope;

 private:
  mutable FuncScope* mFuncScope = nullptr;

 public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebGLContext)

  WebGLContext();

 protected:
  virtual ~WebGLContext();

 public:
  void SetHost(HostWebGLContext* aHost) { mHost = aHost; }

 protected:
  // The HostWebGLContext owns us so this should be set for our entire
  // lifetime
  HostWebGLContext* mHost = nullptr;

  RefPtr<layers::CompositableHost> mCompositableHost;

  layers::LayersBackend mBackend = layers::LayersBackend::LAYERS_NONE;

  struct DoSetDimensionsData {
    nsresult result;
    bool maybeLoseOldContext;
  };

  DoSetDimensionsData DoSetDimensions(int32_t width, int32_t height);

 public:
  NS_INLINE_DECL_REFCOUNTING(WebGLContext)

  // nsICanvasRenderingContextInternal
  virtual int32_t GetWidth() { return DrawingBufferWidth(); }
  virtual int32_t GetHeight() { return DrawingBufferHeight(); }

  SetDimensionsData SetDimensions(int32_t width, int32_t height) {
    DoSetDimensionsData result = DoSetDimensions(width, height);
    return {mOptions,      mOptionsFrozen,
            mResetLayer,   result.maybeLoseOldContext,
            result.result, mPixelStore};
  }

  void SetCompositableHost(RefPtr<layers::CompositableHost>& aCompositableHost);

  virtual already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* out_alphaType);

  /**
   * An abstract base class to be implemented by callers wanting to be notified
   * that a refresh has occurred. Callers must ensure an observer is removed
   * before it is destroyed.
   */
  virtual void DidRefresh();

  void OnMemoryPressure();

  // -

  const auto& CurFuncScope() const { return *mFuncScope; }
  const char* FuncName() const;

  class FuncScope final {
   public:
    const WebGLContext& mWebGL;
    const char* const mFuncName;

   private:
#ifdef DEBUG
    mutable bool mStillNeedsToCheckContextLost = true;
#endif

   public:
    FuncScope(const WebGLContext& webgl, const char* funcName);
    ~FuncScope();

    void OnCheckContextLost() const {
#ifdef DEBUG
      mStillNeedsToCheckContextLost = false;
#endif
    }
  };

  void SynthesizeGLError(GLenum err) const;
  void GenerateError(GLenum err, const char* fmt, ...) const
      MOZ_FORMAT_PRINTF(3, 4);

  void ErrorInvalidEnum(const char* fmt = 0, ...) const MOZ_FORMAT_PRINTF(2, 3);
  void ErrorInvalidOperation(const char* fmt = 0, ...) const
      MOZ_FORMAT_PRINTF(2, 3);
  void ErrorInvalidValue(const char* fmt = 0, ...) const
      MOZ_FORMAT_PRINTF(2, 3);
  void ErrorInvalidFramebufferOperation(const char* fmt = 0, ...) const
      MOZ_FORMAT_PRINTF(2, 3);
  void ErrorInvalidEnumInfo(const char* info, GLenum enumValue) const;
  void ErrorOutOfMemory(const char* fmt = 0, ...) const MOZ_FORMAT_PRINTF(2, 3);
  void ErrorImplementationBug(const char* fmt = 0, ...) const
      MOZ_FORMAT_PRINTF(2, 3);

  void ErrorInvalidEnumArg(const char* argName, GLenum val) const;

  static const char* ErrorName(GLenum error);

  /**
   * Return displayable name for GLenum.
   * This version is like gl::GLenumToStr but with out the GL_ prefix to
   * keep consistency with how errors are reported from WebGL.
   * Returns hex formatted version of glenum if glenum is unknown.
   */
  static void EnumName(GLenum val, nsCString* out_name);

  void DummyReadFramebufferOperation();

  WebGLTexture* ActiveBoundTextureForTarget(const TexTarget texTarget) const {
    switch (texTarget.get()) {
      case LOCAL_GL_TEXTURE_2D:
        return mBound2DTextures[mActiveTexture];
      case LOCAL_GL_TEXTURE_CUBE_MAP:
        return mBoundCubeMapTextures[mActiveTexture];
      case LOCAL_GL_TEXTURE_3D:
        return mBound3DTextures[mActiveTexture];
      case LOCAL_GL_TEXTURE_2D_ARRAY:
        return mBound2DArrayTextures[mActiveTexture];
      default:
        MOZ_CRASH("GFX: bad target");
    }
  }

  /* Use this function when you have the texture image target, for example:
   * GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_[POSITIVE|NEGATIVE]_[X|Y|Z], and
   * not the actual texture binding target: GL_TEXTURE_2D or
   * GL_TEXTURE_CUBE_MAP.
   */
  WebGLTexture* ActiveBoundTextureForTexImageTarget(
      const TexImageTarget texImgTarget) const {
    const TexTarget texTarget = TexImageTargetToTexTarget(texImgTarget);
    return ActiveBoundTextureForTarget(texTarget);
  }

  already_AddRefed<layers::Layer> GetCanvasLayer(nsDisplayListBuilder* builder,
                                                 layers::Layer* oldLayer,
                                                 layers::LayerManager* manager);

  bool UpdateWebRenderCanvasData(nsDisplayListBuilder* aBuilder,
                                 layers::WebRenderCanvasData* aCanvasData);

  Maybe<ICRData> InitializeCanvasRenderer(layers::LayersBackend backend);

  gl::GLContext* GL() const { return gl; }

  bool IsPremultAlpha() const { return mOptions.premultipliedAlpha; }

  bool IsPreservingDrawingBuffer() const {
    return mOptions.preserveDrawingBuffer;
  }

  bool IsXRCompatible() const { return mXRCompatible; }

  // Prepare the context for capture before compositing
  bool PresentScreenBuffer(gl::GLScreenBuffer* const screen = nullptr);

  // Present to compositor
  bool Present();

  // a number that increments every time we have an event that causes
  // all context resources to be lost.
  uint32_t Generation() const { return mGeneration.value(); }

  void RunContextLossTimer();
  void UpdateContextLossStatus();
  void EnqueueUpdateContextLossStatus();

  bool TryToRestoreContext();

  void AssertCachedBindings() const;
  void AssertCachedGlobalState() const;

  // WebIDL WebGLRenderingContext API
  void Commit();

  gfx::IntSize DrawingBufferSize();

 public:
  GLsizei DrawingBufferWidth() {
    const FuncScope funcScope(*this, "drawingBufferWidth");
    return DrawingBufferSize().width;
  }
  GLsizei DrawingBufferHeight() {
    const FuncScope funcScope(*this, "drawingBufferHeight");
    return DrawingBufferSize().height;
  }

  void GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval);

  // This is the entrypoint. Don't test against it directly.
  bool IsContextLost() const;

  const Maybe<ExtensionSets> GetSupportedExtensions();
  void GetExtension(const nsAString& name, dom::CallerType callerType);
  void AttachShader(WebGLProgram& prog, WebGLShader& shader);
  void BindAttribLocation(WebGLProgram& prog, GLuint location,
                          const nsAString& name);
  void BindFramebuffer(GLenum target, WebGLFramebuffer* fb);
  void BindRenderbuffer(GLenum target, WebGLRenderbuffer* fb);
  void BindVertexArray(WebGLVertexArray* vao);
  void BlendColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
  void BlendEquation(GLenum mode);
  void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
  void BlendFunc(GLenum sfactor, GLenum dfactor);
  void BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
                         GLenum dstAlpha);
  GLenum CheckFramebufferStatus(GLenum target);
  void Clear(GLbitfield mask);
  void ClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
  void ClearDepth(GLclampf v);
  void ClearStencil(GLint v);
  void ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b,
                 WebGLboolean a);
  void CompileShader(WebGLShader& shader);
  void CompileShaderANGLE(WebGLShader* shader);
  void CompileShaderBypass(WebGLShader* shader, const nsCString& shaderSource);
  already_AddRefed<WebGLFramebuffer> CreateFramebuffer();
  already_AddRefed<WebGLProgram> CreateProgram();
  already_AddRefed<WebGLRenderbuffer> CreateRenderbuffer();
  already_AddRefed<WebGLShader> CreateShader(GLenum type);
  already_AddRefed<WebGLVertexArray> CreateVertexArray();
  void CullFace(GLenum face);
  void DeleteFramebuffer(WebGLFramebuffer* fb);
  void DeleteProgram(WebGLProgram* prog);
  void DeleteRenderbuffer(WebGLRenderbuffer* rb);
  void DeleteShader(WebGLShader* shader);
  void DeleteVertexArray(WebGLVertexArray* vao);
  void DepthFunc(GLenum func);
  void DepthMask(WebGLboolean b);
  void DepthRange(GLclampf zNear, GLclampf zFar);
  void DetachShader(WebGLProgram& prog, const WebGLShader& shader);
  void DrawBuffers(const nsTArray<GLenum>& buffers);
  void Flush();
  void Finish();

 private:
  void FramebufferAttach(GLenum target, GLenum attachEnum,
                         TexTarget reqTexTarget,
                         const webgl::FbAttachInfo& toAttach) const;

 public:
  void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum rbTarget, WebGLRenderbuffer* rb) const;
  void FramebufferTexture2D(GLenum target, GLenum attachment,
                            GLenum texImageTarget, WebGLTexture* tex,
                            GLint level) const;
  void FramebufferTextureLayer(GLenum target, GLenum attachment,
                               WebGLTexture* tex, GLint level,
                               GLint layer) const;
  void FramebufferTextureMultiview(GLenum target, GLenum attachment,
                                   WebGLTexture* texture, GLint level,
                                   GLint baseViewIndex, GLsizei numViews) const;

  void FrontFace(GLenum mode);

  Maybe<WebGLActiveInfo> GetActiveAttrib(const WebGLProgram& prog,
                                         GLuint index);
  Maybe<WebGLActiveInfo> GetActiveUniform(const WebGLProgram& prog,
                                          GLuint index);

  MaybeAttachedShaders GetAttachedShaders(const WebGLProgram& prog);

  GLint GetAttribLocation(const WebGLProgram& prog, const nsAString& name);
  MaybeWebGLVariant GetBufferParameter(GLenum target, GLenum pname);

  GLenum GetError();
  virtual MaybeWebGLVariant GetFramebufferAttachmentParameter(GLenum target,
                                                              GLenum attachment,
                                                              GLenum pname);

  MaybeWebGLVariant GetProgramParameter(const WebGLProgram& prog, GLenum pname);

  nsString GetProgramInfoLog(const WebGLProgram& prog);
  MaybeWebGLVariant GetRenderbufferParameter(GLenum target, GLenum pname);

  MaybeWebGLVariant GetShaderParameter(const WebGLShader& shader, GLenum pname);

  Maybe<WebGLShaderPrecisionFormat> GetShaderPrecisionFormat(
      GLenum shadertype, GLenum precisiontype);

  nsString GetShaderInfoLog(const WebGLShader& shader);
  nsString GetShaderSource(const WebGLShader& shader);

  MaybeWebGLVariant GetUniform(const WebGLProgram& prog,
                               const WebGLUniformLocation& loc);

  already_AddRefed<WebGLUniformLocation> GetUniformLocation(
      const WebGLProgram& prog, const nsAString& name);

  void Hint(GLenum target, GLenum mode);

  bool IsBuffer(const WebGLBuffer* obj);
  bool IsFramebuffer(const WebGLFramebuffer* obj);
  bool IsProgram(const WebGLProgram* obj);
  bool IsRenderbuffer(const WebGLRenderbuffer* obj);
  bool IsShader(const WebGLShader* obj);
  bool IsTexture(const WebGLTexture* obj);
  bool IsVertexArray(const WebGLVertexArray* obj);

  void LineWidth(GLfloat width);
  void LinkProgram(WebGLProgram& prog);
  WebGLPixelStore PixelStorei(GLenum pname, GLint param);
  void PolygonOffset(GLfloat factor, GLfloat units);

  already_AddRefed<layers::SharedSurfaceTextureClient> GetVRFrame();
  void ClearVRFrame();
  void EnsureVRReady();
  already_AddRefed<dom::Promise> MakeXRCompatible(ErrorResult& aRv);

  ////

  webgl::PackingInfo ValidImplementationColorReadPI(
      const webgl::FormatUsageInfo* usage) const;

 protected:
  void ReadPixelsImpl(GLint x, GLint y, GLsizei width, GLsizei height,
                      GLenum format, GLenum type, void* data, uint32_t dataLen);
  bool DoReadPixelsAndConvert(const webgl::FormatInfo* srcFormat, GLint x,
                              GLint y, GLsizei width, GLsizei height,
                              GLenum format, GLenum destType, void* dest,
                              uint32_t dataLen, uint32_t rowStride);

 public:
  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, WebGLsizeiptr offset);

  Maybe<UniquePtr<RawBuffer<>>> ReadPixels(GLint x, GLint y, GLsizei width,
                                           GLsizei height, GLenum format,
                                           GLenum type, size_t byteLen);

  ////

  void RenderbufferStorage_base(GLenum target, GLsizei samples,
                                GLenum internalformat, GLsizei width,
                                GLsizei height);

 public:
  void SampleCoverage(GLclampf value, WebGLboolean invert);
  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);
  void ShaderSource(WebGLShader& shader, const nsAString& source);
  void StencilFunc(GLenum func, GLint ref, GLuint mask);
  void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
  void StencilMask(GLuint mask);
  void StencilMaskSeparate(GLenum face, GLuint mask);
  void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
  void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                         GLenum dppass);

  //////

  void Uniform1f(WebGLUniformLocation* loc, GLfloat x);
  void Uniform2f(WebGLUniformLocation* loc, GLfloat x, GLfloat y);
  void Uniform3f(WebGLUniformLocation* loc, GLfloat x, GLfloat y, GLfloat z);
  void Uniform4f(WebGLUniformLocation* loc, GLfloat x, GLfloat y, GLfloat z,
                 GLfloat w);

  void Uniform1i(WebGLUniformLocation* loc, GLint x);
  void Uniform2i(WebGLUniformLocation* loc, GLint x, GLint y);
  void Uniform3i(WebGLUniformLocation* loc, GLint x, GLint y, GLint z);
  void Uniform4i(WebGLUniformLocation* loc, GLint x, GLint y, GLint z, GLint w);

  void Uniform1ui(WebGLUniformLocation* loc, GLuint v0);
  void Uniform2ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1);
  void Uniform3ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2);
  void Uniform4ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2,
                  GLuint v3);

  //////////////////////////

  void UniformNfv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                  const RawBuffer<const GLfloat>& arr, GLuint elemOffset,
                  GLuint elemCountOverride);
  void UniformNiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                  const RawBuffer<const GLint>& arr, GLuint elemOffset,
                  GLuint elemCountOverride);
  void UniformNuiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                   const RawBuffer<const GLuint>& arr, GLuint elemOffset,
                   GLuint elemCountOverride);

  void UniformMatrixAxBfv(const char* funcName, uint8_t A, uint8_t B,
                          WebGLUniformLocation* loc, bool transpose,
                          const RawBuffer<const float>& arr, GLuint elemOffset,
                          GLuint elemCountOverride);

  ////////////////////////////////////

  void UseProgram(WebGLProgram* prog);

  bool ValidateAttribArraySetter(uint32_t count, uint32_t arrayLength);
  bool ValidateUniformLocation(const WebGLUniformLocation* loc);
  bool ValidateUniformSetter(const WebGLUniformLocation* loc,
                             uint8_t setterElemSize,
                             webgl::AttribBaseType setterType);
  bool ValidateUniformArraySetter(const WebGLUniformLocation* loc,
                                  uint8_t setterElemSize,
                                  webgl::AttribBaseType setterType,
                                  uint32_t setterArraySize,
                                  uint32_t* out_numElementsToUpload);
  bool ValidateUniformMatrixArraySetter(const WebGLUniformLocation* loc,
                                        uint8_t setterCols, uint8_t setterRows,
                                        webgl::AttribBaseType setterType,
                                        uint32_t setterArraySize,
                                        bool setterTranspose,
                                        uint32_t* out_numElementsToUpload);
  void ValidateProgram(const WebGLProgram& prog);
  bool ValidateUniformLocation(const char* info, WebGLUniformLocation* loc);
  bool ValidateSamplerUniformSetter(const char* info, WebGLUniformLocation* loc,
                                    GLint value);
  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
  // -----------------------------------------------------------------------------
  // WEBGL_lose_context
 public:
  void LoseContext();
  void RestoreContext();

  // -----------------------------------------------------------------------------
  // Buffer Objects (WebGLContextBuffers.cpp)
  void BindBuffer(GLenum target, WebGLBuffer* buffer);

 private:
  void BindBufferRangeImpl(GLenum target, GLuint index, WebGLBuffer* buf,
                           WebGLintptr offset, WebGLsizeiptr size);

 public:
  void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buf) {
    const FuncScope funcScope(*this, "bindBufferBase");
    if (IsContextLost()) return;

    BindBufferRangeImpl(target, index, buf, 0, 0);
  }

  void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buf,
                       WebGLintptr offset, WebGLsizeiptr size) {
    const FuncScope funcScope(*this, "bindBufferRange");
    if (IsContextLost()) return;

    if (!ValidateNonNegative("offset", offset) ||
        !ValidateNonNegative("size", size)) {
      return;
    }
    if (buf && !size) {
      ErrorInvalidValue("Size must be non-zero for non-null buffer.");
      return;
    }
    BindBufferRangeImpl(target, index, buf, offset, size);
  }

  void BufferDataImpl(GLenum target, uint64_t dataLen, const uint8_t* data,
                      GLenum usage);

  void BufferData(GLenum target, WebGLsizeiptr size, GLenum usage);
  void BufferData(GLenum target,
                  const dom::Nullable<dom::ArrayBuffer>& maybeSrc,
                  GLenum usage);
  void BufferData(GLenum target, const dom::ArrayBufferView& srcData,
                  GLenum usage, GLuint srcElemOffset = 0,
                  GLuint srcElemCountOverride = 0);

  void BufferSubDataImpl(GLenum target, WebGLsizeiptr dstByteOffset,
                         uint64_t srcDataLen, const uint8_t* srcData);

  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const dom::ArrayBufferView& src, GLuint srcElemOffset = 0,
                     GLuint srcElemCountOverride = 0);
  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const dom::ArrayBuffer& src);
  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const dom::SharedArrayBuffer& src);

  already_AddRefed<WebGLBuffer> CreateBuffer();
  void DeleteBuffer(WebGLBuffer* buf);

 protected:
  // bound buffer state
  WebGLRefPtr<WebGLBuffer> mBoundArrayBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundCopyReadBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundCopyWriteBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundPixelPackBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundPixelUnpackBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundTransformFeedbackBuffer;
  WebGLRefPtr<WebGLBuffer> mBoundUniformBuffer;

  std::vector<IndexedBufferBinding> mIndexedUniformBufferBindings;

  WebGLRefPtr<WebGLBuffer>& GetBufferSlotByTarget(GLenum target);
  WebGLRefPtr<WebGLBuffer>& GetBufferSlotByTargetIndexed(GLenum target,
                                                         GLuint index);

  // -

  void GenErrorIllegalUse(GLenum useTarget, uint32_t useId, GLenum boundTarget,
                          uint32_t boundId) const;

  bool ValidateBufferForNonTf(const WebGLBuffer&, GLenum nonTfTarget,
                              uint32_t nonTfId) const;

  bool ValidateBufferForNonTf(const WebGLBuffer* const nonTfBuffer,
                              const GLenum nonTfTarget,
                              const uint32_t nonTfId = -1) const {
    if (!nonTfBuffer) return true;
    return ValidateBufferForNonTf(*nonTfBuffer, nonTfTarget, nonTfId);
  }

  bool ValidateBuffersForTf(const WebGLTransformFeedback&,
                            const webgl::LinkedProgramInfo&) const;
  bool ValidateBuffersForTf(
      const std::vector<webgl::BufferAndIndex>& tfBuffers) const;

  // -----------------------------------------------------------------------------
  // Queries (WebGL2ContextQueries.cpp)
 protected:
  WebGLRefPtr<WebGLQuery> mQuerySlot_SamplesPassed;
  WebGLRefPtr<WebGLQuery> mQuerySlot_TFPrimsWritten;
  WebGLRefPtr<WebGLQuery> mQuerySlot_TimeElapsed;

  WebGLRefPtr<WebGLQuery>* ValidateQuerySlotByTarget(GLenum target);

 public:
  already_AddRefed<WebGLQuery> CreateQuery();
  void DeleteQuery(WebGLQuery* query);
  bool IsQuery(const WebGLQuery* query);
  void BeginQuery(GLenum target, WebGLQuery& query);
  void EndQuery(GLenum target);
  MaybeWebGLVariant GetQuery(GLenum target, GLenum pname);
  MaybeWebGLVariant GetQueryParameter(const WebGLQuery& query, GLenum pname);

  // -----------------------------------------------------------------------------
  // State and State Requests (WebGLContextState.cpp)
 private:
  void SetEnabled(const char* funcName, GLenum cap, bool enabled);

 public:
  void Disable(GLenum cap) { SetEnabled("disabled", cap, false); }
  void Enable(GLenum cap) { SetEnabled("enabled", cap, true); }
  bool GetStencilBits(GLint* const out_stencilBits) const;
  virtual MaybeWebGLVariant GetParameter(GLenum pname);

  bool IsEnabled(GLenum cap);

 private:
  // State tracking slots
  realGLboolean mDitherEnabled;
  realGLboolean mRasterizerDiscardEnabled;
  realGLboolean mScissorTestEnabled;
  realGLboolean mDepthTestEnabled = 0;
  realGLboolean mStencilTestEnabled;
  realGLboolean mBlendEnabled = 0;
  GLenum mGenerateMipmapHint = 0;

  struct ScissorRect final {
    GLint x;
    GLint y;
    GLsizei w;
    GLsizei h;

    void Apply(gl::GLContext&) const;
  };
  ScissorRect mScissorRect = {};

  bool ValidateCapabilityEnum(GLenum cap);
  realGLboolean* GetStateTrackingSlot(GLenum cap);

  // Allocation debugging variables
  mutable uint64_t mDataAllocGLCallCount;

  void OnDataAllocCall() const { mDataAllocGLCallCount++; }

  uint64_t GetNumGLDataAllocCalls() const { return mDataAllocGLCallCount; }

  void OnEndOfFrame() const;

  // -----------------------------------------------------------------------------
  // Texture funcions (WebGLContextTextures.cpp)
 public:
  void ActiveTexture(GLenum texUnit);
  void BindTexture(GLenum texTarget, WebGLTexture* tex);
  already_AddRefed<WebGLTexture> CreateTexture();
  void DeleteTexture(WebGLTexture* tex);
  void GenerateMipmap(GLenum texTarget);

  MaybeWebGLVariant GetTexParameter(GLenum texTarget, GLenum pname);
  void TexParameter_base(GLenum texTarget, GLenum pname,
                         const FloatOrInt& param);

  virtual bool IsTexParamValid(GLenum pname) const;

  ////////////////////////////////////

  void CompressedTexImage(uint8_t funcDims, GLenum target, GLint level,
                          GLenum internalFormat, GLsizei width, GLsizei height,
                          GLsizei depth, GLint border,
                          UniquePtr<webgl::TexUnpackBytes>&& src,
                          const Maybe<GLsizei>& expectedImageSize);
  void CompressedTexSubImage(uint8_t funcDims, GLenum target, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat,
                             UniquePtr<webgl::TexUnpackBytes>&& src,
                             const Maybe<GLsizei>& expectedImageSize);

  ////////////////////////////////////

 public:
  void CopyTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                      GLint x, GLint y, uint32_t width, uint32_t height,
                      uint32_t depth);

  void CopyTexSubImage(uint8_t funcDims, GLenum target, GLint level,
                       GLint xOffset, GLint yOffset, GLint zOffset, GLint x,
                       GLint y, uint32_t width, uint32_t height,
                       uint32_t depth);

  ////////////////////////////////////
  // TexImage

  // Implicit width/height uploads

 public:
  void TexImage(uint8_t funcDims, GLenum target, GLint level,
                GLenum internalFormat, uint32_t width, uint32_t height,
                uint32_t depth, GLint border, GLenum unpackFormat,
                GLenum unpackType, UniquePtr<webgl::TexUnpackBlob>&& src);

  ////
  void TexSubImage(uint8_t funcDims, GLenum target, GLint level, GLint xOffset,
                   GLint yOffset, GLint zOffset, uint32_t width,
                   uint32_t height, uint32_t depth, GLenum unpackFormat,
                   GLenum unpackType, UniquePtr<webgl::TexUnpackBlob>&& src);

  bool ValidateNullPixelUnpackBuffer() {
    if (mBoundPixelUnpackBuffer) {
      ErrorInvalidOperation("PIXEL_UNPACK_BUFFER must be null.");
      return false;
    }
    return true;
  }

  UniquePtr<webgl::TexUnpackBlob> ToTexUnpackBytes(
      const WebGLTexImageData& imageData);

  UniquePtr<webgl::TexUnpackBytes> ToTexUnpackBytes(WebGLTexPboOffset& aPbo);

  ////////////////////////////////////
  // WebGLTextureUpload.cpp
 protected:
  bool ValidateTexImageSpecification(uint8_t funcDims, GLenum texImageTarget,
                                     GLint level, GLsizei width, GLsizei height,
                                     GLsizei depth, GLint border,
                                     TexImageTarget* const out_target,
                                     WebGLTexture** const out_texture,
                                     webgl::ImageInfo** const out_imageInfo);
  bool ValidateTexImageSelection(uint8_t funcDims, GLenum texImageTarget,
                                 GLint level, GLint xOffset, GLint yOffset,
                                 GLint zOffset, GLsizei width, GLsizei height,
                                 GLsizei depth,
                                 TexImageTarget* const out_target,
                                 WebGLTexture** const out_texture,
                                 webgl::ImageInfo** const out_imageInfo);
  bool ValidateUnpackInfo(bool usePBOs, GLenum format, GLenum type,
                          webgl::PackingInfo* const out);

  // -----------------------------------------------------------------------------
  // Vertices Feature (WebGLContextVertices.cpp)
  GLenum mPrimRestartTypeBytes = 0;

 public:
  void DrawArraysInstanced(GLenum mode, GLint first, GLsizei vertexCount,
                           GLsizei instanceCount);
  void DrawElementsInstanced(GLenum mode, GLsizei vertexCount, GLenum type,
                             WebGLintptr byteOffset, GLsizei instanceCount);

  void EnableVertexAttribArray(GLuint index);
  void DisableVertexAttribArray(GLuint index);

  MaybeWebGLVariant GetVertexAttrib(GLuint index, GLenum pname);

  WebGLsizeiptr GetVertexAttribOffset(GLuint index, GLenum pname);

  ////

  void VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

  ////

  void VertexAttribAnyPointer(bool isFuncInt, GLuint index, GLint size,
                              GLenum type, bool normalized, GLsizei stride,
                              WebGLintptr byteOffset);

 public:
  void VertexAttribDivisor(GLuint index, GLuint divisor);

 private:
  WebGLBuffer* DrawElements_check(GLsizei indexCount, GLenum type,
                                  WebGLintptr byteOffset,
                                  GLsizei instanceCount);
  void Draw_cleanup();

  void VertexAttrib1fv_base(GLuint index, uint32_t arrayLength,
                            const GLfloat* ptr);
  void VertexAttrib2fv_base(GLuint index, uint32_t arrayLength,
                            const GLfloat* ptr);
  void VertexAttrib3fv_base(GLuint index, uint32_t arrayLength,
                            const GLfloat* ptr);
  void VertexAttrib4fv_base(GLuint index, uint32_t arrayLength,
                            const GLfloat* ptr);

  bool BindArrayAttribToLocation0(WebGLProgram* prog);

  // -----------------------------------------------------------------------------
  // PROTECTED
 protected:
  WebGLVertexAttrib0Status WhatDoesVertexAttrib0Need() const;
  bool DoFakeVertexAttrib0(uint64_t vertexCount);
  void UndoFakeVertexAttrib0();

  CheckedUint32 mGeneration;

  bool mResetLayer = true;
  bool mOptionsFrozen;
  bool mDisableExtensions;
  bool mIsMesa;
  bool mLoseContextOnMemoryPressure = false;
  bool mCanLoseContextInForeground = true;
  bool mShouldPresent;
  bool mDisableFragHighP;
  bool mVRReady = false;
  // https://immersive-web.github.io/webxr/#xr-compatible
  bool mXRCompatible = false;

  template <typename WebGLObjectType>
  void DeleteWebGLObjectsArray(nsTArray<WebGLObjectType>& array);

  GLuint mActiveTexture = 0;
  GLenum mDefaultFB_DrawBuffer0 = 0;
  GLenum mDefaultFB_ReadBuffer = 0;

  mutable GLenum mWebGLError;

  std::unique_ptr<webgl::ShaderValidator> CreateShaderValidator(
      GLenum shaderType) const;

  // some GL constants
  uint32_t mGLMaxTextureUnits = 0;

  uint32_t mGLMaxVertexAttribs = 0;
  uint32_t mGLMaxFragmentUniformVectors = 0;
  uint32_t mGLMaxVertexUniformVectors = 0;
  uint32_t mGLMaxVertexOutputVectors = 0;
  uint32_t mGLMaxFragmentInputVectors = 0;

  uint32_t mGLMaxTransformFeedbackSeparateAttribs = 0;
  uint32_t mGLMaxUniformBufferBindings = 0;

  uint32_t mGLMaxVertexTextureImageUnits = 0;
  uint32_t mGLMaxFragmentTextureImageUnits = 0;
  uint32_t mGLMaxCombinedTextureImageUnits = 0;

  uint32_t mGLMaxColorAttachments = 0;
  uint32_t mGLMaxDrawBuffers = 0;

  // ES3:
  uint32_t mGLMinProgramTexelOffset = 0;
  uint32_t mGLMaxProgramTexelOffset = 0;

  uint32_t mGLMaxViewportDims[2];

  WebGLContextOptions mOptions;

 public:
  GLenum LastColorAttachmentEnum() const {
    return LOCAL_GL_COLOR_ATTACHMENT0 + mGLMaxColorAttachments - 1;
  }
  const auto& GLMaxDrawBuffers() const { return mGLMaxDrawBuffers; }

  const decltype(mOptions)& Options() const { return mOptions; }
  void SetOptions(const WebGLContextOptions& options) { mOptions = options; }

 protected:
  // Texture sizes are often not actually the GL values. Let's be explicit that
  // these are implementation limits.
  uint32_t mGLMaxTextureSize = 0;
  uint32_t mGLMaxCubeMapTextureSize = 0;
  uint32_t mGLMax3DTextureSize = 0;
  uint32_t mGLMaxArrayTextureLayers = 0;
  uint32_t mGLMaxMultiviewViews = 1;
  uint32_t mGLMaxRenderbufferSize = 0;

 public:
  GLuint MaxVertexAttribs() const { return mGLMaxVertexAttribs; }

  GLuint GLMaxTextureUnits() const { return mGLMaxTextureUnits; }

  float mGLAliasedLineWidthRange[2];
  float mGLAliasedPointSizeRange[2];

  bool IsFormatValidForFB(TexInternalFormat format) const;

 protected:
  // Represents current status of the context with respect to context loss.
  // That is, whether the context is lost, and what part of the context loss
  // process we currently are at.
  // This is used to support the WebGL spec's asyncronous nature in handling
  // context loss.
  enum class ContextStatus {
    // The context is stable; there either are none or we don't know of any.
    NotLost,
    // The context has been lost, but we have not yet sent an event to the
    // script informing it of this.
    LostAwaitingEvent,
    // The context has been lost, and we have sent the script an event
    // informing it of this.
    Lost,
    // The context is lost, an event has been sent to the script, and the
    // script correctly handled the event. We are waiting for the context to
    // be restored.
    LostAwaitingRestore
  };

  // -------------------------------------------------------------------------
  // WebGL extensions (implemented in WebGLContextExtensions.cpp)
  typedef EnumeratedArray<WebGLExtensionID, WebGLExtensionID::Max,
                          RefPtr<WebGLExtensionBase>>
      ExtensionsArrayType;

  ExtensionsArrayType mExtensions;

  // enable an extension. the extension should not be enabled before.
  void EnableExtension(WebGLExtensionID ext);

  // Enable an extension if it's supported. Return the extension on success.
  WebGLExtensionBase* EnableSupportedExtension(dom::CallerType callerType,
                                               WebGLExtensionID ext,
                                               bool explict = true);

 public:
  // Return an extension if it's supported, optionally enabling it if necessary.
  template <
      WebGLExtensionID ext,
      typename ExtensionClass = typename WebGLExtensionClassMap<ext>::Type>
  ExtensionClass* GetExtension(
      bool toEnable = false,
      dom::CallerType callerType = dom::CallerType::NonSystem) {
    if (!IsExtensionEnabled(ext)) {
      if ((!toEnable) || (!IsExtensionSupported(callerType, ext))) {
        return nullptr;
      }

      CreateExtension(ext);
    }

    return static_cast<ExtensionClass*>(mExtensions[ext].get());
  }

  void EnableExtension(WebGLExtensionID ext,
                       dom::CallerType callerType = dom::CallerType::NonSystem);

  // returns true if the extension has been enabled by calling getExtension.
  bool IsExtensionEnabled(const WebGLExtensionID ext) const {
    return mExtensions[ext];
  }

  bool IsExtensionExplicit(const WebGLExtensionID ext) const;

  void WarnIfImplicit(const WebGLExtensionID ext);

 protected:
  // returns true if the extension is supported for this caller type (this
  // decides what getSupportedExtensions exposes)
  bool IsExtensionSupported(dom::CallerType callerType,
                            WebGLExtensionID ext) const;
  bool IsExtensionSupported(WebGLExtensionID ext) const;

  nsTArray<GLenum> mCompressedTextureFormats;

  // -------------------------------------------------------------------------
  // WebGL 2 specifics (implemented in WebGL2Context.cpp)
 public:
  virtual bool IsWebGL2() const = 0;

  struct FailureReason {
    nsCString key;  // For reporting.
    nsCString info;

    FailureReason() {}

    template <typename A, typename B>
    FailureReason(const A& _key, const B& _info)
        : key(nsCString(_key)), info(nsCString(_info)) {}
  };

 protected:
  bool InitWebGL2(FailureReason* const out_failReason);

  bool CreateAndInitGL(bool forceEnabled,
                       std::vector<FailureReason>* const out_failReasons);

  void ThrowEvent_WebGLContextCreationError(const nsACString& text);

  // -------------------------------------------------------------------------
  // Validation functions (implemented in WebGLContextValidate.cpp)
  bool InitAndValidateGL(FailureReason* const out_failReason);

  bool ValidateBlendEquationEnum(GLenum cap, const char* info);
  bool ValidateBlendFuncEnumsCompatibility(GLenum sfactor, GLenum dfactor,
                                           const char* info);
  bool ValidateStencilOpEnum(GLenum action, const char* info);
  bool ValidateFaceEnum(GLenum face);
  bool ValidateTexInputData(GLenum type, js::Scalar::Type jsArrayType,
                            WebGLTexImageFunc func, WebGLTexDimensions dims);
  bool ValidateAttribPointer(bool integerMode, GLuint index, GLint size,
                             GLenum type, WebGLboolean normalized,
                             GLsizei stride, WebGLintptr byteOffset,
                             const char* info);
  bool ValidateStencilParamsForDrawCall() const;

  bool ValidateCopyTexImage(TexInternalFormat srcFormat,
                            TexInternalFormat dstformat, WebGLTexImageFunc func,
                            WebGLTexDimensions dims);

  bool ValidateTexImage(TexImageTarget texImageTarget, GLint level,
                        GLenum internalFormat, GLint xoffset, GLint yoffset,
                        GLint zoffset, GLint width, GLint height, GLint depth,
                        GLint border, GLenum format, GLenum type,
                        WebGLTexImageFunc func, WebGLTexDimensions dims);
  bool ValidateTexImageFormat(GLenum internalFormat, WebGLTexImageFunc func,
                              WebGLTexDimensions dims);
  bool ValidateTexImageType(GLenum type, WebGLTexImageFunc func,
                            WebGLTexDimensions dims);
  bool ValidateTexImageFormatAndType(GLenum format, GLenum type,
                                     WebGLTexImageFunc func,
                                     WebGLTexDimensions dims);
  bool ValidateCompTexImageInternalFormat(GLenum format, WebGLTexImageFunc func,
                                          WebGLTexDimensions dims);
  bool ValidateCopyTexImageInternalFormat(GLenum format, WebGLTexImageFunc func,
                                          WebGLTexDimensions dims);
  bool ValidateTexImageSize(TexImageTarget texImageTarget, GLint level,
                            GLint width, GLint height, GLint depth,
                            WebGLTexImageFunc func, WebGLTexDimensions dims);
  bool ValidateTexSubImageSize(GLint x, GLint y, GLint z, GLsizei width,
                               GLsizei height, GLsizei depth, GLsizei baseWidth,
                               GLsizei baseHeight, GLsizei baseDepth,
                               WebGLTexImageFunc func, WebGLTexDimensions dims);
  bool ValidateCompTexImageSize(GLint level, GLenum internalFormat,
                                GLint xoffset, GLint yoffset, GLsizei width,
                                GLsizei height, GLsizei levelWidth,
                                GLsizei levelHeight, WebGLTexImageFunc func,
                                WebGLTexDimensions dims);
  bool ValidateCompTexImageDataSize(GLint level, GLenum internalFormat,
                                    GLsizei width, GLsizei height,
                                    uint32_t byteLength, WebGLTexImageFunc func,
                                    WebGLTexDimensions dims);

  bool ValidateUniformLocationForProgram(WebGLUniformLocation* location,
                                         WebGLProgram* program);

  bool HasDrawBuffers() const {
    return IsWebGL2() ||
           IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers);
  }

  WebGLRefPtr<WebGLBuffer>* ValidateBufferSlot(GLenum target);

 public:
  WebGLBuffer* ValidateBufferSelection(GLenum target);

 protected:
  IndexedBufferBinding* ValidateIndexedBufferSlot(GLenum target, GLuint index);

  bool ValidateIndexedBufferBinding(
      GLenum target, GLuint index,
      WebGLRefPtr<WebGLBuffer>** const out_genericBinding,
      IndexedBufferBinding** const out_indexedBinding);

  bool ValidateNonNegative(const char* argName, int64_t val) {
    if (MOZ_UNLIKELY(val < 0)) {
      ErrorInvalidValue("`%s` must be non-negative.", argName);
      return false;
    }
    return true;
  }

 public:
  template <typename T>
  bool ValidateNonNull(const char* const argName,
                       const dom::Nullable<T>& maybe) {
    if (maybe.IsNull()) {
      ErrorInvalidValue("%s: Cannot be null.", argName);
      return false;
    }
    return true;
  }

  bool ValidateArrayBufferView(const dom::ArrayBufferView& view,
                               GLuint elemOffset, GLuint elemCountOverride,
                               GLenum errorVal, uint8_t** const out_bytes,
                               size_t* const out_byteLen) const;

 protected:
  ////

  void DestroyResourcesAndContext();

  // helpers

  bool ConvertImage(size_t width, size_t height, size_t srcStride,
                    size_t dstStride, const uint8_t* src, uint8_t* dst,
                    WebGLTexelFormat srcFormat, bool srcPremultiplied,
                    WebGLTexelFormat dstFormat, bool dstPremultiplied,
                    size_t dstTexelSize);

  //////
 public:
  template <typename T>
  bool ValidateObjectAllowDeleted(const char* const argName,
                                  const WebGLContextBoundObject<T>& object) {
    if (!object.IsCompatibleWithContext(this)) {
      ErrorInvalidOperation(
          "%s: Object from different WebGL context (or older"
          " generation of this one) passed as argument.",
          argName);
      return false;
    }

    return true;
  }

  template <typename T>
  bool ValidateObject(const char* const argName,
                      const WebGLRefCountedObject<T>& object,
                      const bool isShaderOrProgram = false) {
    if (!ValidateObjectAllowDeleted(argName, object)) return false;

    if (isShaderOrProgram) {
      /* GLES 3.0.5 p45:
       * "Commands that accept shader or program object names will generate the
       *  error INVALID_VALUE if the provided name is not the name of either a
       *  shader or program object[.]"
       * Further, shaders and programs appear to be different from other
       * objects, in that their lifetimes are better defined. However, they also
       * appear to allow use of objects marked for deletion, and only reject
       * actually-destroyed objects.
       */
      if (object.IsDeleted()) {
        ErrorInvalidValue(
            "%s: Shader or program object argument cannot have been"
            " deleted.",
            argName);
        return false;
      }
    } else {
      if (object.IsDeleteRequested()) {
        ErrorInvalidOperation(
            "%s: Object argument cannot have been marked for"
            " deletion.",
            argName);
        return false;
      }
    }

    return true;
  }

  ////

  // Program and Shader are incomplete, so we can't inline the conversion to
  // WebGLDeletableObject here.
  bool ValidateObject(const char* const argName,
                      const WebGLProgram& object) const;
  bool ValidateObject(const char* const argName,
                      const WebGLShader& object) const;

  ////

  template <typename T>
  bool ValidateIsObject(const WebGLRefCountedObject<T>* const object) const {
    if (IsContextLost()) return false;
    if (!object) return false;
    if (!object->IsCompatibleWithContext(this)) return false;
    if (object->IsDeleted()) return false;
    return true;
  }

  template <typename T>
  bool ValidateDeleteObject(const WebGLRefCountedObject<T>* const object) {
    if (IsContextLost()) return false;
    if (!object) return false;
    if (!ValidateObjectAllowDeleted("obj", *object)) return false;
    if (object->IsDeleteRequested()) return false;
    return true;
  }

  ////

 private:
  // -------------------------------------------------------------------------
  // Context customization points
  virtual WebGLVertexArray* CreateVertexArrayImpl();

 public:
  void ForceLoseContext(bool simulateLoss = false);
  const WebGLPixelStore GetPixelStore() const { return mPixelStore; }

 protected:
  void ForceRestoreContext();

  nsTArray<WebGLRefPtr<WebGLTexture>> mBound2DTextures;
  nsTArray<WebGLRefPtr<WebGLTexture>> mBoundCubeMapTextures;
  nsTArray<WebGLRefPtr<WebGLTexture>> mBound3DTextures;
  nsTArray<WebGLRefPtr<WebGLTexture>> mBound2DArrayTextures;
  nsTArray<WebGLRefPtr<WebGLSampler>> mBoundSamplers;

  void ResolveTexturesForDraw() const;

  WebGLRefPtr<WebGLProgram> mCurrentProgram;
  RefPtr<const webgl::LinkedProgramInfo> mActiveProgramLinkInfo;

  bool ValidateFramebufferTarget(GLenum target) const;
  bool ValidateInvalidateFramebuffer(GLenum target,
                                     const nsTArray<GLenum>& attachments,
                                     std::vector<GLenum>* const scopedVector,
                                     GLsizei* const out_glNumAttachments,
                                     const GLenum** const out_glAttachments);

  WebGLRefPtr<WebGLFramebuffer> mBoundDrawFramebuffer;
  WebGLRefPtr<WebGLFramebuffer> mBoundReadFramebuffer;
  WebGLRefPtr<WebGLRenderbuffer> mBoundRenderbuffer;
  WebGLRefPtr<WebGLTransformFeedback> mBoundTransformFeedback;
  WebGLRefPtr<WebGLVertexArray> mBoundVertexArray;

 public:
  const auto& BoundReadFb() const { return mBoundReadFramebuffer; }

 protected:
  LinkedList<WebGLBuffer> mBuffers;
  LinkedList<WebGLFramebuffer> mFramebuffers;
  LinkedList<WebGLProgram> mPrograms;
  LinkedList<WebGLQuery> mQueries;
  LinkedList<WebGLRenderbuffer> mRenderbuffers;
  LinkedList<WebGLSampler> mSamplers;
  LinkedList<WebGLShader> mShaders;
  LinkedList<WebGLSync> mSyncs;
  LinkedList<WebGLTexture> mTextures;
  LinkedList<WebGLTransformFeedback> mTransformFeedbacks;
  LinkedList<WebGLVertexArray> mVertexArrays;

  WebGLRefPtr<WebGLTransformFeedback> mDefaultTransformFeedback;
  WebGLRefPtr<WebGLVertexArray> mDefaultVertexArray;

  WebGLPixelStore mPixelStore;

  CheckedUint32 GetUnpackSize(bool isFunc3D, uint32_t width, uint32_t height,
                              uint32_t depth, uint8_t bytesPerPixel);

  bool ValidatePackSize(uint32_t width, uint32_t height, uint8_t bytesPerPixel,
                        uint32_t* const out_rowStride,
                        uint32_t* const out_endOffset);

  ////////////////////////////////////

 protected:
  GLuint mEmptyTFO;

  // Generic Vertex Attributes
  // Though CURRENT_VERTEX_ATTRIB is listed under "Vertex Shader State" in the
  // spec state tables, this isn't vertex shader /object/ state. This array is
  // merely state useful to vertex shaders, but is global state.
  std::vector<webgl::AttribBaseType> mGenericVertexAttribTypes;
  uint8_t mGenericVertexAttrib0Data[sizeof(float) * 4];
  CacheInvalidator mGenericVertexAttribTypeInvalidator;

  GLuint mFakeVertexAttrib0BufferObject = 0;
  size_t mFakeVertexAttrib0BufferObjectSize = 0;
  bool mFakeVertexAttrib0DataDefined = false;
  uint8_t mFakeVertexAttrib0Data[sizeof(float) * 4];

  Float32Array4&& GetVertexAttribFloat32Array(GLuint index);
  Int32Array4&& GetVertexAttribInt32Array(GLuint index);
  Uint32Array4&& GetVertexAttribUint32Array(GLuint index);

  GLint mStencilRefFront = 0;
  GLint mStencilRefBack = 0;
  GLuint mStencilValueMaskFront = 0;
  GLuint mStencilValueMaskBack = 0;
  GLuint mStencilWriteMaskFront = 0;
  GLuint mStencilWriteMaskBack = 0;
  uint8_t mColorWriteMask = 0;  // bitmask
  realGLboolean mDepthWriteMask = 0;
  GLfloat mColorClearValue[4];
  GLint mStencilClearValue = 0;
  GLfloat mDepthClearValue = 0.0;

  GLint mViewportX;
  GLint mViewportY;
  GLsizei mViewportWidth;
  GLsizei mViewportHeight;
  bool mAlreadyWarnedAboutViewportLargerThanDest;

  GLfloat mLineWidth = 0.0;

  WebGLContextLossHandler mContextLossHandler;
  bool mAllowContextRestore;
  // true if the context lost event has not yet given permission to restore
  bool mDisallowContextRestore;
  bool mLastLossWasSimulated;
  ContextStatus mContextStatus = ContextStatus::NotLost;

  // Used for some hardware (particularly Tegra 2 and 4) that likes to
  // be Flushed while doing hundreds of draw calls.
  int mDrawCallsSinceLastFlush;

  mutable int mAlreadyGeneratedWarnings;
  int mMaxWarnings;
  bool mAlreadyWarnedAboutFakeVertexAttrib0;

  bool ShouldGenerateWarnings() const;

  bool ShouldGeneratePerfWarnings() const {
    return mNumPerfWarnings < mMaxPerfWarnings;
  }

  bool mNeedsFakeNoAlpha;
  bool mNeedsFakeNoDepth;
  bool mNeedsFakeNoStencil;
  bool mNeedsFakeNoStencil_UserFBs = false;

  mutable uint8_t mDriverColorMask = 0;
  bool mDriverDepthTest = false;
  bool mDriverStencilTest = false;

  bool mNeedsIndexValidation = false;

  const bool mAllowFBInvalidation;
#if defined(MOZ_WIDGET_ANDROID)
  UniquePtr<gl::GLScreenBuffer> mVRScreen;
#endif

  bool Has64BitTimestamps() const;

  // --

  const uint8_t mMsaaSamples;
  mutable gfx::IntSize mRequestedSize;
  mutable UniquePtr<gl::MozFramebuffer> mDefaultFB;
  mutable bool mDefaultFB_IsInvalid = false;
  mutable UniquePtr<gl::MozFramebuffer> mResolvedDefaultFB;

  // --

  bool EnsureDefaultFB();
  bool ValidateAndInitFB(
      const WebGLFramebuffer* fb,
      GLenum incompleteFbError = LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
  void DoBindFB(const WebGLFramebuffer* fb,
                GLenum target = LOCAL_GL_FRAMEBUFFER) const;

  bool BindCurFBForDraw();
  bool BindCurFBForColorRead(
      const webgl::FormatUsageInfo** out_format, uint32_t* out_width,
      uint32_t* out_height,
      GLenum incompleteFbError = LOCAL_GL_INVALID_FRAMEBUFFER_OPERATION);
  void DoColorMask(uint8_t bitmask) const;
  void BlitBackbufferToCurDriverFB() const;
  bool BindDefaultFBForRead();

  // --

  bool ShouldResistFingerprinting() const;

 public:
  // console logging helpers
  void GenerateWarning(const char* fmt, ...) const MOZ_FORMAT_PRINTF(2, 3);
  void GenerateWarning(const char* fmt, va_list ap) const
      MOZ_FORMAT_PRINTF(2, 0);

  void GeneratePerfWarning(const char* fmt, ...) const MOZ_FORMAT_PRINTF(2, 3);

 public:
  UniquePtr<webgl::FormatUsageAuthority> mFormatUsage;

  virtual UniquePtr<webgl::FormatUsageAuthority> CreateFormatUsage(
      gl::GLContext* gl) const = 0;

  const decltype(mBound2DTextures)* TexListForElemType(GLenum elemType) const;

  void UpdateMaxDrawBuffers();

  void AllowContextRestore() {
    mDisallowContextRestore = false;
    EnqueueUpdateContextLossStatus();
  }

  // --
 private:
  webgl::AvailabilityRunnable* mAvailabilityRunnable = nullptr;

 public:
  webgl::AvailabilityRunnable* EnsureAvailabilityRunnable();

  // -

  // Friend list
  friend class ScopedCopyTexImageSource;
  friend class ScopedResolveTexturesForDraw;
  friend class ScopedUnpackReset;
  friend class webgl::TexUnpackBlob;
  friend class webgl::TexUnpackBytes;
  friend class webgl::TexUnpackImage;
  friend class webgl::TexUnpackSurface;
  friend struct webgl::UniformInfo;
  friend class WebGLTexture;
  friend class WebGLFBAttachPoint;
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

// Returns `value` rounded to the next highest multiple of `multiple`.
// AKA PadToAlignment, StrideForAlignment.
template <typename V, typename M>
V RoundUpToMultipleOf(const V& value, const M& multiple) {
  return ((value + multiple - 1) / multiple) * multiple;
}

const char* GetEnumName(GLenum val, const char* defaultRet = "<unknown>");
std::string EnumString(GLenum val);

bool ValidateTexTarget(WebGLContext* webgl, uint8_t funcDims,
                       GLenum rawTexTarget, TexTarget* const out_texTarget,
                       WebGLTexture** const out_tex);
bool ValidateTexImageTarget(WebGLContext* webgl, uint8_t funcDims,
                            GLenum rawTexImageTarget,
                            TexImageTarget* const out_texImageTarget,
                            WebGLTexture** const out_tex);

class ScopedUnpackReset final {
 private:
  const WebGLContext* const mWebGL;

 public:
  explicit ScopedUnpackReset(const WebGLContext* webgl);
  ~ScopedUnpackReset();
};

class ScopedFBRebinder final {
 private:
  const WebGLContext* const mWebGL;

 public:
  explicit ScopedFBRebinder(const WebGLContext* const webgl) : mWebGL(webgl) {}
  ~ScopedFBRebinder();
};

class ScopedLazyBind final {
 private:
  gl::GLContext* const mGL;
  const GLenum mTarget;

 public:
  ScopedLazyBind(gl::GLContext* gl, GLenum target, const WebGLBuffer* buf);
  ~ScopedLazyBind();
};

////

bool Intersect(int32_t srcSize, int32_t read0, int32_t readSize,
               int32_t* out_intRead0, int32_t* out_intWrite0,
               int32_t* out_intSize);

uint64_t AvailGroups(uint64_t totalAvailItems, uint64_t firstItemOffset,
                     uint32_t groupSize, uint32_t groupStride);

////

class ScopedDrawCallWrapper final {
 public:
  WebGLContext& mWebGL;

  explicit ScopedDrawCallWrapper(WebGLContext& webgl);
  ~ScopedDrawCallWrapper();
};

namespace webgl {
class ScopedPrepForResourceClear final {
  const WebGLContext& webgl;

 public:
  explicit ScopedPrepForResourceClear(const WebGLContext&);
  ~ScopedPrepForResourceClear();
};
}  // namespace webgl

}  // namespace mozilla

#endif
