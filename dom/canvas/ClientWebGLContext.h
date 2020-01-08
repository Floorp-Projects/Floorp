/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CLIENTWEBGLCONTEXT_H_
#define CLIENTWEBGLCONTEXT_H_

#include "GLConsts.h"
#include "mozilla/dom/ImageData.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "WebGLActiveInfo.h"
#include "WebGLContextEndpoint.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"

#include "mozilla/Logging.h"
#include "WebGLCrossProcessCommandQueue.h"

#ifndef WEBGL_BRIDGE_LOG_
#  define WEBGL_BRIDGE_LOG_(lvl, ...) \
    MOZ_LOG(mozilla::gWebGLBridgeLog, lvl, (__VA_ARGS__))
#  define WEBGL_BRIDGE_LOGV(...) \
    WEBGL_BRIDGE_LOG_(LogLevel::Verbose, __VA_ARGS__)
#  define WEBGL_BRIDGE_LOGD(...) WEBGL_BRIDGE_LOG_(LogLevel::Debug, __VA_ARGS__)
#  define WEBGL_BRIDGE_LOGI(...) WEBGL_BRIDGE_LOG_(LogLevel::Info, __VA_ARGS__)
#  define WEBGL_BRIDGE_LOGE(...) WEBGL_BRIDGE_LOG_(LogLevel::Error, __VA_ARGS__)
#endif  // WEBGL_BRIDGE_LOG_

namespace mozilla {

namespace dom {
class WebGLChild;
}

namespace layers {
class SharedSurfaceTextureClient;
}

namespace webgl {
class TexUnpackBlob;
class TexUnpackBytes;
}  // namespace webgl

extern LazyLogModule gWebGLBridgeLog;

class ClientWebGLExtensionBase;
class ClientWebGLErrorSink;

void DrainWebGLError(nsWeakPtr aWeakContext);

class ClientWebGLRefCount : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING_VIRTUAL(
      ClientWebGLRefCount)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ClientWebGLRefCount)
 protected:
  virtual ~ClientWebGLRefCount() {}
};

/**
 * The client-side representation of WebGL types is little more than
 * an ID and a ref-count.
 */
template <typename WebGLType>
class ClientWebGLObject : public WebGLId<WebGLType>,
                          public ClientWebGLRefCount {
 public:
  ClientWebGLContext* GetParentObject() const { return GetContext(); }

  bool IsValidForContext(ClientWebGLContext* aContext) const {
    auto context = GetContext();
    return context && (context == aContext) &&
           (mGeneration == aContext->Generation());
  }

  MozExternalRefCountType AddRef() override {
    if (mLogMe) {
      WEBGL_BRIDGE_LOGD("[%p] AddRefing WebGLObject %d from %d to %d", this,
                        (int)WebGLId<WebGLType>::Id(),
                        static_cast<int32_t>(mRefCnt),
                        static_cast<int32_t>(mRefCnt) + 1);
    }
    return ClientWebGLRefCount::AddRef();
  }

  MozExternalRefCountType Release() override {
    // If we are deleting the object, let the host know that it can, too.
    if (mLogMe) {
      WEBGL_BRIDGE_LOGD("[%p] Releasing WebGLObject %d from %d to %d", this,
                        (int)WebGLId<WebGLType>::Id(),
                        static_cast<int32_t>(mRefCnt),
                        static_cast<int32_t>(mRefCnt) - 1);
    }

    auto context = GetContext();
    // If the context is still around then it has a reference to us that we
    // should release it also via ReleaseWebGLObject when it is the last one
    // left (so, this call would be going from 2 to 1).  If the context is
    // gone then so is that reference, so we delete when we go from 1 to 0.
    int32_t refCountToDeleteAt = context ? 2 : 1;

    if (static_cast<int32_t>(mRefCnt) == refCountToDeleteAt) {
      // Must release first to avoid an infinite loop
      bool ret = ClientWebGLRefCount::Release();
      if (context) {
        // This will release us again.
        context->ReleaseWebGLObject(this);
      }
      return ret;
    }
    return ClientWebGLRefCount::Release();
  }

  ClientWebGLObject(uint64_t aId, ClientWebGLContext* aContext)
      : WebGLId<WebGLType>(aId),
        mGeneration(aContext->Generation()),
        mLogMe(sLogMe) {
    if (mLogMe) {
      WEBGL_BRIDGE_LOGD("[%p] Created WebGLObject %d", this,
                        WebGLId<WebGLType>::Id());
    }
    mContext = do_GetWeakReference(aContext);
    sLogMe = false;
  }

 protected:
  ClientWebGLContext* GetContext() const {
    nsCOMPtr<nsICanvasRenderingContextInternal> ret =
        do_QueryReferent(mContext);
    if (!ret) {
      return nullptr;
    }
    return static_cast<ClientWebGLContext*>(ret.get());
  }

  virtual ~ClientWebGLObject(){};

  nsWeakPtr mContext;
  uint64_t mGeneration;
  bool mLogMe;
  static bool sLogMe;
};

template <typename WebGLType>
bool ClientWebGLObject<WebGLType>::sLogMe = true;

// Every WebGL type with a client version exposed to JS needs to use this macro
// to associate its C++ type with the JS binding interface.
#define DEFINE_WEBGL_CLIENT_TYPE_2(_WebGLType, _WebGLBindingType)       \
  class ClientWebGL##_WebGLType                                         \
      : public ClientWebGLObject<WebGL##_WebGLType> {                   \
   public:                                                              \
    ClientWebGL##_WebGLType(uint64_t aId, ClientWebGLContext* aContext) \
        : ClientWebGLObject<WebGL##_WebGLType>(aId, aContext) {}        \
    JSObject* WrapObject(JSContext* cx,                                 \
                         JS::Handle<JSObject*> givenProto) override {   \
      return dom::WebGL##_WebGLBindingType##_Binding::Wrap(cx, this,    \
                                                           givenProto); \
    }                                                                   \
                                                                        \
   protected:                                                           \
    virtual ~ClientWebGL##_WebGLType(){};                               \
  };                                                                    \
  RefPtr<ClientWebGL##_WebGLType> downcast(                             \
      RefPtr<ClientWebGLObject<WebGL##_WebGLType>>&& obj) {             \
    MOZ_ASSERT(obj);                                                    \
    return obj.forget().template downcast<ClientWebGL##_WebGLType>();   \
  }

// Usually, the JS binding name is the same as the WebGL type name
#define DEFINE_WEBGL_CLIENT_TYPE(_WebGLType) \
  DEFINE_WEBGL_CLIENT_TYPE_2(_WebGLType, _WebGLType)

DEFINE_WEBGL_CLIENT_TYPE(Buffer)
DEFINE_WEBGL_CLIENT_TYPE(Framebuffer)
DEFINE_WEBGL_CLIENT_TYPE(Program)
DEFINE_WEBGL_CLIENT_TYPE(Query)
DEFINE_WEBGL_CLIENT_TYPE(Renderbuffer)
DEFINE_WEBGL_CLIENT_TYPE(Sampler)
DEFINE_WEBGL_CLIENT_TYPE(Shader)
DEFINE_WEBGL_CLIENT_TYPE(Sync)
DEFINE_WEBGL_CLIENT_TYPE(Texture)
DEFINE_WEBGL_CLIENT_TYPE(TransformFeedback)
DEFINE_WEBGL_CLIENT_TYPE(UniformLocation)
DEFINE_WEBGL_CLIENT_TYPE_2(VertexArray, VertexArrayObject)

#undef DEFINE_WEBGL_CLIENT_TYPE
#undef DEFINE_WEBGL_CLIENT_TYPE_2

////////////////////////////////////

struct TexImageSource {
  const dom::ArrayBufferView* mView;
  GLuint mViewElemOffset;
  GLuint mViewElemLengthOverride;

  const WebGLsizeiptr* mPboOffset;

  const dom::ImageBitmap* mImageBitmap;
  const dom::ImageData* mImageData;

  const dom::Element* mDomElem;
  ErrorResult* mOut_error;

 protected:
  TexImageSource() { memset(this, 0, sizeof(*this)); }
};

////

struct TexImageSourceAdapter final : public TexImageSource {
  TexImageSourceAdapter(const dom::Nullable<dom::ArrayBufferView>* maybeView,
                        ErrorResult*) {
    if (!maybeView->IsNull()) {
      mView = &(maybeView->Value());
    }
  }

  TexImageSourceAdapter(const dom::Nullable<dom::ArrayBufferView>* maybeView,
                        GLuint viewElemOffset) {
    if (!maybeView->IsNull()) {
      mView = &(maybeView->Value());
    }
    mViewElemOffset = viewElemOffset;
  }

  TexImageSourceAdapter(const dom::ArrayBufferView* view, ErrorResult*) {
    mView = view;
  }

  TexImageSourceAdapter(const dom::ArrayBufferView* view, GLuint viewElemOffset,
                        GLuint viewElemLengthOverride = 0) {
    mView = view;
    mViewElemOffset = viewElemOffset;
    mViewElemLengthOverride = viewElemLengthOverride;
  }

  TexImageSourceAdapter(const WebGLsizeiptr* pboOffset, GLuint ignored1,
                        GLuint ignored2 = 0) {
    mPboOffset = pboOffset;
  }

  TexImageSourceAdapter(const WebGLsizeiptr* pboOffset, ErrorResult* ignored) {
    mPboOffset = pboOffset;
  }

  TexImageSourceAdapter(const dom::ImageBitmap* imageBitmap,
                        ErrorResult* out_error) {
    mImageBitmap = imageBitmap;
    mOut_error = out_error;
  }

  TexImageSourceAdapter(const dom::ImageData* imageData, ErrorResult*) {
    mImageData = imageData;
  }

  TexImageSourceAdapter(const dom::Element* domElem,
                        ErrorResult* const out_error) {
    mDomElem = domElem;
    mOut_error = out_error;
  }
};

/**
 * Base class for all IDL implementations of WebGLContext
 */
class ClientWebGLContext : public nsICanvasRenderingContextInternal,
                           public nsSupportsWeakReference,
                           public nsWrapperCache,
                           public WebGLContextEndpoint {
  // ----------------------------- Lifetime and DOM ---------------------------
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
      ClientWebGLContext, nsICanvasRenderingContextInternal)

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> givenProto) override {
    switch (mVersion) {
      case WEBGL1:
        return dom::WebGLRenderingContext_Binding::Wrap(cx, this, givenProto);
      case WEBGL2:
        return dom::WebGL2RenderingContext_Binding::Wrap(cx, this, givenProto);
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid WebGL Version");
        return nullptr;
    }
  }

  static RefPtr<ClientWebGLContext> Create(WebGLVersion aVersion);

  uint64_t Generation() { return mGeneration; }

 protected:
  uint64_t mGeneration = 0;

  // -------------------------------------------------------------------------
  // Client WebGL Object Tracking
  // -------------------------------------------------------------------------
 public:
  template <typename ObjectType>
  using ClientObjectIdMap =
      HashMap<WebGLId<ObjectType>, RefPtr<ClientWebGLObject<ObjectType>>>;

 public:
  JS::Value ToJSValue(JSContext* cx, const MaybeWebGLVariant& aVariant,
                      ErrorResult& rv) const;

 protected:
  friend struct MaybeWebGLVariantMatcher;

  template <typename WebGLObjectType>
  JS::Value WebGLObjectAsJSValue(JSContext* cx,
                                 RefPtr<WebGLObjectType>&& object,
                                 ErrorResult& rv) const;

  template <typename WebGLObjectType>
  JS::Value WebGLObjectAsJSValue(JSContext* cx, const WebGLObjectType*,
                                 ErrorResult& rv) const;

  template <typename WebGLObjectType>
  JSObject* WebGLObjectAsJSObject(JSContext* cx, const WebGLObjectType*,
                                  ErrorResult& rv) const;

  template <typename WebGLType>
  RefPtr<ClientWebGLObject<WebGLType>> EnsureWebGLObject(
      const WebGLId<WebGLType>& aId);

  template <typename WebGLType>
  RefPtr<ClientWebGLObject<WebGLType>> Make(const WebGLId<WebGLType>& aId);

  template <typename WebGLType>
  WebGLId<WebGLType> GenerateId();

#define DEFINE_GENERATEID(_TYPE)                         \
  typename WebGLId<WebGL##_TYPE>::IdType mId##_TYPE = 1; \
  template <>                                            \
  WebGLId<WebGL##_TYPE> GenerateId<WebGL##_TYPE>() {     \
    return WebGLId<WebGL##_TYPE>(mId##_TYPE++);          \
  }

  // All but Buffer, Texture and UniformLocation
  DEFINE_GENERATEID(Framebuffer)
  DEFINE_GENERATEID(Program)
  DEFINE_GENERATEID(Renderbuffer)
  DEFINE_GENERATEID(Sampler)
  DEFINE_GENERATEID(Shader)
  DEFINE_GENERATEID(Sync)
  DEFINE_GENERATEID(TransformFeedback)
  DEFINE_GENERATEID(Query)
  DEFINE_GENERATEID(VertexArray)

#undef DEFINE_GENERATEID

 public:
  // Define the client ID map and accessors for the given type
#define DEFINE_CLIENTWEBGLOBJECT_MAP(_TYPE)                    \
  ClientObjectIdMap<WebGL##_TYPE> m##_TYPE##Map;               \
  bool Insert(RefPtr<ClientWebGLObject<WebGL##_TYPE>>& aObj) { \
    MOZ_ASSERT(aObj->Id());                                    \
    return m##_TYPE##Map.put(*aObj, aObj);                     \
  }                                                            \
  RefPtr<ClientWebGLObject<WebGL##_TYPE>> Find(                \
      const WebGLId<WebGL##_TYPE>& aId) {                      \
    auto it = m##_TYPE##Map.lookup(aId);                       \
    return it ? it->value() : nullptr;                         \
  }                                                            \
  void Remove(const WebGLId<WebGL##_TYPE>& aId) { m##_TYPE##Map.remove(aId); }

  // Annoying but, since we don't want to include the WebGLMethodDispatcher
  // in this header due to its size, we can't define ReleaseWebGLObject as
  // a template method.  We expand the template with macros instead.
#define DECLARE_CLIENTWEBGLOBJECT(_TYPE) \
  DEFINE_CLIENTWEBGLOBJECT_MAP(_TYPE)    \
  void ReleaseWebGLObject(const WebGLId<WebGL##_TYPE>* aId);

  DECLARE_CLIENTWEBGLOBJECT(Buffer)
  DECLARE_CLIENTWEBGLOBJECT(Framebuffer)
  DECLARE_CLIENTWEBGLOBJECT(Program)
  DECLARE_CLIENTWEBGLOBJECT(Query)
  DECLARE_CLIENTWEBGLOBJECT(Renderbuffer)
  DECLARE_CLIENTWEBGLOBJECT(Sampler)
  DECLARE_CLIENTWEBGLOBJECT(Shader)
  DECLARE_CLIENTWEBGLOBJECT(Sync)
  DECLARE_CLIENTWEBGLOBJECT(Texture)
  DECLARE_CLIENTWEBGLOBJECT(TransformFeedback)
  DECLARE_CLIENTWEBGLOBJECT(UniformLocation)
  DECLARE_CLIENTWEBGLOBJECT(VertexArray)

#undef DECLARE_CLIENTWEBGLOBJECT
#undef DEFINE_CLIENTWEBGLOBJECT_MAP

  // -------------------------------------------------------------------------
  // Binary data access/conversion for IPC
  // -------------------------------------------------------------------------
 protected:
  typedef dom::Float32ArrayOrUnrestrictedFloatSequence Float32ListU;
  typedef dom::Int32ArrayOrLongSequence Int32ListU;
  typedef dom::Uint32ArrayOrUnsignedLongSequence Uint32ListU;

  // Adapter that converts a JS array parameters to pointer/count C-style arrays
  template <typename elemT, typename viewT>
  struct Arr {
    const size_t elemCount;
    const elemT* const elemBytes;

   private:
    static size_t ComputeAndReturnLength(const viewT& view) {
      view.ComputeLengthAndData();
      return view.LengthAllowShared();
    }

   public:
    explicit Arr(const viewT& view)
        : elemCount(ComputeAndReturnLength(view)),
          elemBytes(view.DataAllowShared()) {}

    explicit Arr(const dom::Sequence<elemT>& seq)
        : elemCount(seq.Length()), elemBytes(seq.Elements()) {}

    Arr(size_t _elemCount, const elemT* _elemBytes)
        : elemCount(_elemCount), elemBytes(_elemBytes) {}

    ////

    static Arr From(const dom::Float32ArrayOrUnrestrictedFloatSequence& list) {
      if (list.IsFloat32Array()) return Arr(list.GetAsFloat32Array());

      return Arr(list.GetAsUnrestrictedFloatSequence());
    }

    static Arr From(const dom::Int32ArrayOrLongSequence& list) {
      if (list.IsInt32Array()) return Arr(list.GetAsInt32Array());

      return Arr(list.GetAsLongSequence());
    }

    static Arr From(const dom::Uint32ArrayOrUnsignedLongSequence& list) {
      if (list.IsUint32Array()) return Arr(list.GetAsUint32Array());

      return Arr(list.GetAsUnsignedLongSequence());
    }
  };

  typedef Arr<GLfloat, dom::Float32Array> Float32Arr;
  typedef Arr<GLint, dom::Int32Array> Int32Arr;
  typedef Arr<GLuint, dom::Uint32Array> Uint32Arr;

  MaybeWebGLTexUnpackVariant From(TexImageTarget target, GLsizei rawWidth,
                                  GLsizei rawHeight, GLsizei rawDepth,
                                  GLint border, const TexImageSource& src);

  MaybeWebGLTexUnpackVariant ClientFromDomElem(TexImageTarget target,
                                               uint32_t width, uint32_t height,
                                               uint32_t depth,
                                               const dom::Element& elem,
                                               ErrorResult* const out_error);

  MaybeWebGLTexUnpackVariant FromCompressed(
      TexImageTarget target, GLsizei rawWidth, GLsizei rawHeight,
      GLsizei rawDepth, GLint border, const TexImageSource& src,
      const Maybe<GLsizei>& expectedImageSize);

  // -------------------------------------------------------------------------
  // Client WebGL API call tracking and error message reporting
  // -------------------------------------------------------------------------
 public:
  // Remembers the WebGL function that is lowest on the stack for client-side
  // error generation.
  class FuncScope final {
   public:
    const ClientWebGLContext* mWebGL;
    const char* const mFuncName;
    FuncScopeId mId;

    FuncScope(const ClientWebGLContext* webgl, const char* funcName)
        : mWebGL(webgl),
          mFuncName(funcName),
          mId(FuncScopeId::FuncScopeIdError) {
      // Only set if an "outer" scope hasn't already been set.
      if (!mWebGL->mFuncScope) {
        mWebGL->mFuncScope = this;
      }
    }

    FuncScope(const ClientWebGLContext* webgl, FuncScopeId aId)
        : mWebGL(webgl), mFuncName(GetFuncScopeName(aId)), mId(aId) {
      mWebGL->mFuncScope = this;
    }

    ~FuncScope() {
      if (this == mWebGL->mFuncScope) {
        mWebGL->mFuncScope = nullptr;
      }
    }
  };

 protected:
  // The scope of the function at the top of the current WebGL function call
  // stack
  mutable FuncScope* mFuncScope = nullptr;

  const auto& CurFuncScope() const { return *mFuncScope; }
  FuncScopeId GetFuncScopeId() const {
    return mFuncScope ? mFuncScope->mId : FuncScopeId::FuncScopeIdError;
  }
  const char* FuncName() const {
    return mFuncScope ? mFuncScope->mFuncName : nullptr;
  }

 public:
  template <typename... Args>
  void EnqueueErrorPrintf(GLenum aGLError, const char* aFmt,
                          const Args&... aArgs) {
    MOZ_ASSERT(FuncName());
    nsCString buf;
    buf.AppendPrintf(aFmt, aArgs...);
    nsCString msg;
    msg.AppendPrintf("WebGL warning: %s: %s", FuncName(), buf.Data());
    EnqueueErrorPrintfHelper(aGLError, msg);
  }

  // Post a message to the host telling it to post a message back to us
  // (the client) notifying of a failure that was detected in the client.
  // We take this roundtrip to guarantee that error messages are received
  // in the correct order.
  template <typename... Args>
  void EnqueueErrorInvalidValue(const char* aFmt, const Args&... aArgs) {
    EnqueueErrorPrintf(LOCAL_GL_INVALID_VALUE, aFmt, aArgs...);
  }

  template <typename... Args>
  void EnqueueErrorInvalidEnumInfo(const char* aFmt, const Args&... aArgs) {
    EnqueueErrorPrintf(LOCAL_GL_INVALID_ENUM, aFmt, aArgs...);
  }

  template <typename... Args>
  void EnqueueErrorInvalidOperation(const char* aFmt, const Args&... aArgs) {
    EnqueueErrorPrintf(LOCAL_GL_INVALID_OPERATION, aFmt, aArgs...);
  }

  template <typename... Args>
  void EnqueueErrorOutOfMemory(const char* aFmt, const Args&... aArgs) {
    EnqueueErrorPrintf(LOCAL_GL_OUT_OF_MEMORY, aFmt, aArgs...);
  }

  void EnqueueWarning(const nsCString& msg);
  void EnqueueWarning(const char* msg) { EnqueueWarning(nsCString(msg)); }

  bool ValidateArrayBufferView(const dom::ArrayBufferView& view,
                               GLuint elemOffset, GLuint elemCountOverride,
                               const GLenum errorEnum,
                               uint8_t** const out_bytes,
                               size_t* const out_byteLen);

 protected:
  void EnqueueErrorPrintfHelper(GLenum aGLError, const nsCString& msg);

  bool ValidateAttribArraySetter(uint32_t setterElemSize,
                                 uint32_t arrayLength) {
    if (arrayLength < setterElemSize) {
      EnqueueErrorInvalidValue("Array must have >= %d elements.",
                               setterElemSize);
      return false;
    }

    return true;
  }

  template <typename T>
  bool ValidateNonNull(const char* const argName,
                       const dom::Nullable<T>& maybe) {
    if (maybe.IsNull()) {
      EnqueueErrorInvalidValue("%s: Cannot be null.", argName);
      return false;
    }
    return true;
  }

  bool ValidateNonNegative(const char* argName, int64_t val) {
    if (MOZ_UNLIKELY(val < 0)) {
      EnqueueErrorInvalidValue("`%s` must be non-negative.", argName);
      return false;
    }
    return true;
  }

  bool ValidateViewType(GLenum unpackType, const TexImageSource& src);

  bool ValidateExtents(GLsizei width, GLsizei height, GLsizei depth,
                       GLint border, uint32_t* const out_width,
                       uint32_t* const out_height, uint32_t* const out_depth);

  // -------------------------------------------------------------------------
  // nsICanvasRenderingContextInternal / nsAPostRefreshObserver
  // -------------------------------------------------------------------------
 public:
  already_AddRefed<layers::Layer> GetCanvasLayer(
      nsDisplayListBuilder* builder, layers::Layer* oldLayer,
      layers::LayerManager* manager) override;
  bool InitializeCanvasRenderer(nsDisplayListBuilder* aBuilder,
                                layers::CanvasRenderer* aRenderer) override;
  // Note that 'clean' here refers to its invalidation state, not the
  // contents of the buffer.
  bool IsContextCleanForFrameCapture() override {
    return !mCapturedFrameInvalidated;
  }
  void MarkContextClean() override { mInvalidated = false; }
  void MarkContextCleanForFrameCapture() override {
    mCapturedFrameInvalidated = false;
  }

  void OnMemoryPressure() override;
  NS_IMETHOD
  SetContextOptions(JSContext* cx, JS::Handle<JS::Value> options,
                    ErrorResult& aRvForDictionaryInit) override;
  NS_IMETHOD
  SetDimensions(int32_t width, int32_t height) override;
  bool UpdateWebRenderCanvasData(
      nsDisplayListBuilder* aBuilder,
      layers::WebRenderCanvasData* aCanvasData) override;

  bool UpdateCompositableHandle(LayerTransactionChild* aLayerTransaction,
                                CompositableHandle aHandle);

  // ------

  int32_t GetWidth() override { return DrawingBufferWidth(); }
  int32_t GetHeight() override { return DrawingBufferHeight(); }

  NS_IMETHOD InitializeWithDrawTarget(nsIDocShell*,
                                      NotNull<gfx::DrawTarget*>) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD Reset() override {
    /* (InitializeWithSurface) */
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  UniquePtr<uint8_t[]> GetImageBuffer(int32_t* out_format) override;
  NS_IMETHOD GetInputStream(const char* mimeType,
                            const nsAString& encoderOptions,
                            nsIInputStream** out_stream) override;

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* out_alphaType) override;

  void SetOpaqueValueFromOpaqueAttr(bool) override{};
  bool GetIsOpaque() override { return !mOptions.alpha; }

  NS_IMETHOD SetIsIPC(bool) override { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * An abstract base class to be implemented by callers wanting to be notified
   * that a refresh has occurred. Callers must ensure an observer is removed
   * before it is destroyed.
   */
  void DidRefresh() override;

  NS_IMETHOD Redraw(const gfxRect&) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // ------

  void Invalidate();

 protected:
  layers::LayersBackend GetCompositorBackendType() const;

  bool mInvalidated = false;
  bool mCapturedFrameInvalidated = false;

  // -------------------------------------------------------------------------
  // WebGLRenderingContext Basic Properties and Methods
  // -------------------------------------------------------------------------
 public:
  dom::HTMLCanvasElement* GetCanvas() const { return mCanvasElement; }
  void Commit();
  void GetCanvas(
      dom::Nullable<dom::OwningHTMLCanvasElementOrOffscreenCanvas>& retval);

  GLsizei DrawingBufferWidth() {
    const FuncScope funcScope(this, "drawingBufferWidth");
    return DrawingBufferSize().width;
  }
  GLsizei DrawingBufferHeight() {
    const FuncScope funcScope(this, "drawingBufferHeight");
    return DrawingBufferSize().height;
  }
  void GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval);

  mozilla::dom::PWebGLChild* GetWebGLChild();

 private:
  gfx::IntSize DrawingBufferSize();
  bool HasAlphaSupport() { return mSurfaceInfo.supportsAlpha; }
  void AllowContextRestore();

  ICRData mSurfaceInfo;

  // -------------------------------------------------------------------------
  // Client-side helper methods.  Dispatch to a Host method.
  // -------------------------------------------------------------------------

  // ------------------------- GL State -------------------------
 public:
  bool IsContextLost() const;

  void Disable(GLenum cap);

  void Enable(GLenum cap);

  bool IsEnabled(GLenum cap);

  void GetProgramInfoLog(const WebGLId<WebGLProgram>& prog, nsAString& retval);

  void GetShaderInfoLog(const WebGLId<WebGLShader>& shader, nsAString& retval);

  void GetShaderSource(const WebGLId<WebGLShader>& shader, nsAString& retval);

  void GetParameter(JSContext* cx, GLenum pname,
                    JS::MutableHandle<JS::Value> retval, ErrorResult& rv);

  void GetBufferParameter(JSContext* cx, GLenum target, GLenum pname,
                          JS::MutableHandle<JS::Value> retval);

  void GetFramebufferAttachmentParameter(JSContext* cx, GLenum target,
                                         GLenum attachment, GLenum pname,
                                         JS::MutableHandle<JS::Value> retval,
                                         ErrorResult& rv);

  void GetProgramParameter(JSContext* cx, const WebGLId<WebGLProgram>& prog,
                           GLenum pname, JS::MutableHandle<JS::Value> retval);

  void GetRenderbufferParameter(JSContext* cx, GLenum target, GLenum pname,
                                JS::MutableHandle<JS::Value> retval);

  void GetShaderParameter(JSContext* cx, const WebGLId<WebGLShader>& shader,
                          GLenum pname, JS::MutableHandle<JS::Value> retval);

  void GetIndexedParameter(JSContext* cx, GLenum target, GLuint index,
                           JS::MutableHandleValue retval, ErrorResult& rv);

  void GetUniform(JSContext* cx, const WebGLId<WebGLProgram>& prog,
                  const WebGLId<WebGLUniformLocation>& loc,
                  JS::MutableHandle<JS::Value> retval);

  already_AddRefed<ClientWebGLUniformLocation> GetUniformLocation(
      const WebGLId<WebGLProgram>& prog, const nsAString& name);

  already_AddRefed<ClientWebGLShaderPrecisionFormat> GetShaderPrecisionFormat(
      GLenum shadertype, GLenum precisiontype);

  bool IsBuffer(const ClientWebGLBuffer* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsFramebuffer(const ClientWebGLFramebuffer* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsProgram(const ClientWebGLProgram* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsRenderbuffer(const ClientWebGLRenderbuffer* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsShader(const ClientWebGLShader* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsTexture(const ClientWebGLTexture* obj) {
    return obj && obj->IsValidForContext(this);
  }

  bool IsVertexArray(const ClientWebGLVertexArray* obj,
                     bool aFromExtension = false);

  void BindAttribLocation(const WebGLId<WebGLProgram>& prog, GLuint location,
                          const nsAString& name);

  GLint GetAttribLocation(const WebGLId<WebGLProgram>& prog,
                          const nsAString& name);

  void AttachShader(const WebGLId<WebGLProgram>& progId,
                    const WebGLId<WebGLShader>& shaderId);

  void ShaderSource(const WebGLId<WebGLShader>& shader,
                    const nsAString& source);

  void BindFramebuffer(GLenum target, const WebGLId<WebGLFramebuffer>& fb);

  void BindRenderbuffer(GLenum target, const WebGLId<WebGLRenderbuffer>& fb);

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

  void CompileShader(const WebGLId<WebGLShader>& shaderId);

  void CullFace(GLenum face);

  void DeleteFramebuffer(const WebGLId<WebGLFramebuffer>& aFb);

  void DeleteProgram(const WebGLId<WebGLProgram>& aProg);

  void DeleteRenderbuffer(const WebGLId<WebGLRenderbuffer>& aRb);

  void DeleteShader(const WebGLId<WebGLShader>& aShader);

  void DepthFunc(GLenum func);

  void DepthMask(WebGLboolean b);

  void DepthRange(GLclampf zNear, GLclampf zFar);

  void DetachShader(const WebGLId<WebGLProgram>& progId,
                    const WebGLId<WebGLShader>& shaderId);

  void Flush();

  void Finish();

  void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum rbTarget,
                               const WebGLId<WebGLRenderbuffer>& rb);

  void FramebufferTexture2D(GLenum target, GLenum attachment,
                            GLenum texImageTarget,
                            const WebGLId<WebGLTexture>& tex, GLint level);

  void FrontFace(GLenum mode);

  GLenum GetError();

  void Hint(GLenum target, GLenum mode);

  void LineWidth(GLfloat width);

  void LinkProgram(const WebGLId<WebGLProgram>& progId);

  void PixelStorei(GLenum pname, GLint param);

  void PolygonOffset(GLfloat factor, GLfloat units);

  void SampleCoverage(GLclampf value, WebGLboolean invert);

  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);

  void ShaderSource(const WebGLId<WebGLShader>& shaderId,
                    const nsString& source);

  void StencilFunc(GLenum func, GLint ref, GLuint mask);

  void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);

  void StencilMask(GLuint mask);

  void StencilMaskSeparate(GLenum face, GLuint mask);

  void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);

  void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                         GLenum dppass);

  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);

  // ------------------------- Buffer Objects -------------------------
 public:
  already_AddRefed<ClientWebGLBuffer> CreateBuffer();

  void BindBuffer(GLenum target, const WebGLId<WebGLBuffer>& buffer);

  void BindBufferBase(GLenum target, GLuint index,
                      const WebGLId<WebGLBuffer>& buffer);

  void BindBufferRange(GLenum target, GLuint index,
                       const WebGLId<WebGLBuffer>& buffer, WebGLintptr offset,
                       WebGLsizeiptr size);

  void DeleteBuffer(const WebGLId<WebGLBuffer>& aBuf);

  void CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         GLintptr readOffset, GLintptr writeOffset,
                         GLsizeiptr size);

  void BufferData(GLenum target, WebGLsizeiptr size, GLenum usage);
  void BufferData(GLenum target,
                  const dom::Nullable<dom::ArrayBuffer>& maybeSrc,
                  GLenum usage);
  void BufferData(GLenum target, const dom::ArrayBufferView& srcData,
                  GLenum usage, GLuint srcElemOffset = 0,
                  GLuint srcElemCountOverride = 0);

  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const dom::ArrayBufferView& src, GLuint srcElemOffset = 0,
                     GLuint srcElemCountOverride = 0);
  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const dom::ArrayBuffer& src);

  void GetBufferSubData(GLenum target, GLintptr srcByteOffset,
                        const dom::ArrayBufferView& dstData,
                        GLuint dstElemOffset, GLuint dstElemCountOverride);

  // -------------------------- Framebuffer Objects --------------------------
  already_AddRefed<ClientWebGLFramebuffer> CreateFramebuffer();

  void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter);

  void FramebufferTextureLayer(GLenum target, GLenum attachment,
                               const WebGLId<WebGLTexture>& textureId,
                               GLint level, GLint layer);

  void InvalidateFramebuffer(GLenum target,
                             const dom::Sequence<GLenum>& attachments,
                             ErrorResult& unused);

  void InvalidateSubFramebuffer(GLenum target,
                                const dom::Sequence<GLenum>& attachments,
                                GLint x, GLint y, GLsizei width, GLsizei height,
                                ErrorResult& unused);

  void ReadBuffer(GLenum mode);

  // ----------------------- Renderbuffer objects -----------------------
  void GetInternalformatParameter(JSContext* cx, GLenum target,
                                  GLenum internalformat, GLenum pname,
                                  JS::MutableHandleValue retval,
                                  ErrorResult& rv);

  already_AddRefed<ClientWebGLRenderbuffer> CreateRenderbuffer();

  void RenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width,
                           GLsizei height);

  void RenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                      GLenum internalFormat, GLsizei width,
                                      GLsizei height);

  // --------------------------- Texture objects ---------------------------
  already_AddRefed<ClientWebGLTexture> CreateTexture();

  void ActiveTexture(GLenum texUnit);

  void BindTexture(GLenum texTarget, const WebGLId<WebGLTexture>& tex);

  void DeleteTexture(const WebGLId<WebGLTexture>& aTex);

  void GenerateMipmap(GLenum texTarget);

  void CopyTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                      GLint x, GLint y, GLsizei width, GLsizei height,
                      GLint border);

  void GetTexParameter(JSContext* cx, GLenum texTarget, GLenum pname,
                       JS::MutableHandle<JS::Value> retval);

  void TexParameterf(GLenum texTarget, GLenum pname, GLfloat param);

  void TexParameteri(GLenum texTarget, GLenum pname, GLint param);

  void TexStorage2D(GLenum target, GLsizei levels, GLenum internalFormat,
                    GLsizei width, GLsizei height);

  void TexStorage3D(GLenum target, GLsizei levels, GLenum internalFormat,
                    GLsizei width, GLsizei height, GLsizei depth);

  ////////////////////////////////////

  template <typename T>
  void TexImage2D(GLenum target, GLint level, GLenum internalFormat,
                  GLenum unpackFormat, GLenum unpackType, const T& src,
                  ErrorResult& out_error) {
    GLsizei width = 0;
    GLsizei height = 0;
    GLint border = 0;
    TexImage2D(target, level, internalFormat, width, height, border,
               unpackFormat, unpackType, src, out_error);
  }

  template <typename T>
  void TexImage2D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum unpackFormat, GLenum unpackType, const T& anySrc,
                  ErrorResult& out_error) {
    const TexImageSourceAdapter src(&anySrc, &out_error);
    TexImage2D(target, level, internalFormat, width, height, border,
               unpackFormat, unpackType, src);
  }

  void TexImage2D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum unpackFormat, GLenum unpackType,
                  const dom::ArrayBufferView& view, GLuint viewElemOffset,
                  ErrorResult&) {
    const TexImageSourceAdapter src(&view, viewElemOffset);
    TexImage2D(target, level, internalFormat, width, height, border,
               unpackFormat, unpackType, src);
  }

 protected:
  void TexImage2D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum unpackFormat, GLenum unpackType,
                  const TexImageSource& src);

  ////////////////////////////////////

 public:
  template <typename T>
  void TexSubImage2D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLenum unpackFormat, GLenum unpackType, const T& src,
                     ErrorResult& out_error) {
    GLsizei width = 0;
    GLsizei height = 0;
    TexSubImage2D(target, level, xOffset, yOffset, width, height, unpackFormat,
                  unpackType, src, out_error);
  }

  template <typename T>
  void TexSubImage2D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLsizei width, GLsizei height, GLenum unpackFormat,
                     GLenum unpackType, const T& anySrc,
                     ErrorResult& out_error) {
    const TexImageSourceAdapter src(&anySrc, &out_error);
    TexSubImage2D(target, level, xOffset, yOffset, width, height, unpackFormat,
                  unpackType, src);
  }

  void TexSubImage2D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLsizei width, GLsizei height, GLenum unpackFormat,
                     GLenum unpackType, const dom::ArrayBufferView& view,
                     GLuint viewElemOffset, ErrorResult&) {
    const TexImageSourceAdapter src(&view, viewElemOffset);
    TexSubImage2D(target, level, xOffset, yOffset, width, height, unpackFormat,
                  unpackType, src);
  }

 protected:
  void TexSubImage2D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLsizei width, GLsizei height, GLenum unpackFormat,
                     GLenum unpackType, const TexImageSource& src);

  ////////////////////////////////////

 public:
  template <typename T>
  void TexImage3D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum unpackFormat, GLenum unpackType, const T& anySrc,
                  ErrorResult& out_error) {
    const TexImageSourceAdapter src(&anySrc, &out_error);
    TexImage3D(target, level, internalFormat, width, height, depth, border,
               unpackFormat, unpackType, src);
  }

  void TexImage3D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum unpackFormat, GLenum unpackType,
                  const dom::ArrayBufferView& view, GLuint viewElemOffset,
                  ErrorResult&) {
    const TexImageSourceAdapter src(&view, viewElemOffset);
    TexImage3D(target, level, internalFormat, width, height, depth, border,
               unpackFormat, unpackType, src);
  }

 protected:
  void TexImage3D(GLenum target, GLint level, GLenum internalFormat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum unpackFormat, GLenum unpackType,
                  const TexImageSource& src);

  ////////////////////////////////////

 public:
  template <typename T>
  void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLint zOffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                     const T& anySrc, ErrorResult& out_error) {
    const TexImageSourceAdapter src(&anySrc, &out_error);
    TexSubImage3D(target, level, xOffset, yOffset, zOffset, width, height,
                  depth, unpackFormat, unpackType, src);
  }

  void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLint zOffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                     const dom::Nullable<dom::ArrayBufferView>& maybeSrcView,
                     GLuint srcElemOffset, ErrorResult&) {
    const TexImageSourceAdapter src(&maybeSrcView, srcElemOffset);
    TexSubImage3D(target, level, xOffset, yOffset, zOffset, width, height,
                  depth, unpackFormat, unpackType, src);
  }

 protected:
  void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                     GLint zOffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                     const TexImageSource& src);

  ////////////////////////////////////

 public:
  void CompressedTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei imageSize, WebGLsizeiptr offset) {
    const FuncScope scope(this, FuncScopeId::compressedTexImage2D);
    const uint8_t funcDims = 2;
    const GLsizei depth = 1;
    const TexImageSourceAdapter src(&offset, 0, 0);
    CompressedTexImage(funcDims, target, level, internalFormat, width, height,
                       depth, border, src, Some(imageSize), GetFuncScopeId());
  }

  template <typename T>
  void CompressedTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLint border,
                            const T& anySrc, GLuint viewElemOffset = 0,
                            GLuint viewElemLengthOverride = 0) {
    const FuncScope scope(this, FuncScopeId::compressedTexImage2D);
    const uint8_t funcDims = 2;
    const GLsizei depth = 1;
    const TexImageSourceAdapter src(&anySrc, viewElemOffset,
                                    viewElemLengthOverride);
    CompressedTexImage(funcDims, target, level, internalFormat, width, height,
                       depth, border, src, Nothing(), GetFuncScopeId());
  }

  void CompressedTexSubImage2D(GLenum target, GLint level, GLint xOffset,
                               GLint yOffset, GLsizei width, GLsizei height,
                               GLenum unpackFormat, GLsizei imageSize,
                               WebGLsizeiptr offset) {
    const FuncScope scope(this, FuncScopeId::compressedTexSubImage2D);
    const uint8_t funcDims = 2;
    const GLint zOffset = 0;
    const GLsizei depth = 1;
    const TexImageSourceAdapter src(&offset, 0, 0);
    CompressedTexSubImage(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, src,
                          Some(imageSize), GetFuncScopeId());
  }

  template <typename T>
  void CompressedTexSubImage2D(GLenum target, GLint level, GLint xOffset,
                               GLint yOffset, GLsizei width, GLsizei height,
                               GLenum unpackFormat, const T& anySrc,
                               GLuint viewElemOffset = 0,
                               GLuint viewElemLengthOverride = 0) {
    const FuncScope scope(this, FuncScopeId::compressedTexSubImage2D);
    const uint8_t funcDims = 2;
    const GLint zOffset = 0;
    const GLsizei depth = 1;
    const TexImageSourceAdapter src(&anySrc, viewElemOffset,
                                    viewElemLengthOverride);
    CompressedTexSubImage(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, src, Nothing(),
                          GetFuncScopeId());
  }

  ////////////////////////////////////

 public:
  void CompressedTexImage3D(GLenum target, GLint level, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth,
                            GLint border, GLsizei imageSize,
                            WebGLintptr offset) {
    const FuncScope scope(this, FuncScopeId::compressedTexImage3D);
    const uint8_t funcDims = 3;
    const TexImageSourceAdapter src(&offset, 0, 0);
    CompressedTexImage(funcDims, target, level, internalFormat, width, height,
                       depth, border, src, Some(imageSize), GetFuncScopeId());
  }

  template <typename T>
  void CompressedTexImage3D(GLenum target, GLint level, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth,
                            GLint border, const T& anySrc,
                            GLuint viewElemOffset = 0,
                            GLuint viewElemLengthOverride = 0) {
    const FuncScope scope(this, FuncScopeId::compressedTexImage3D);
    const uint8_t funcDims = 3;
    const TexImageSourceAdapter src(&anySrc, viewElemOffset,
                                    viewElemLengthOverride);
    CompressedTexImage(funcDims, target, level, internalFormat, width, height,
                       depth, border, src, Nothing(), GetFuncScopeId());
  }

  void CompressedTexSubImage3D(GLenum target, GLint level, GLint xOffset,
                               GLint yOffset, GLint zOffset, GLsizei width,
                               GLsizei height, GLsizei depth,
                               GLenum unpackFormat, GLsizei imageSize,
                               WebGLintptr offset) {
    const FuncScope scope(this, FuncScopeId::compressedTexSubImage3D);
    const uint8_t funcDims = 3;
    const TexImageSourceAdapter src(&offset, 0, 0);
    CompressedTexSubImage(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, src,
                          Some(imageSize), GetFuncScopeId());
  }

  template <typename T>
  void CompressedTexSubImage3D(GLenum target, GLint level, GLint xOffset,
                               GLint yOffset, GLint zOffset, GLsizei width,
                               GLsizei height, GLsizei depth,
                               GLenum unpackFormat, const T& anySrc,
                               GLuint viewElemOffset = 0,
                               GLuint viewElemLengthOverride = 0) {
    const FuncScope scope(this, FuncScopeId::compressedTexSubImage3D);
    const uint8_t funcDims = 3;
    const TexImageSourceAdapter src(&anySrc, viewElemOffset,
                                    viewElemLengthOverride);
    CompressedTexSubImage(funcDims, target, level, xOffset, yOffset, zOffset,
                          width, height, depth, unpackFormat, src, Nothing(),
                          GetFuncScopeId());
  }

 protected:
  void TexImage(uint8_t funcDims, GLenum target, GLint level,
                GLenum internalFormat, GLsizei width, GLsizei height,
                GLsizei depth, GLint border, GLenum unpackFormat,
                GLenum unpackType, const TexImageSource& src,
                FuncScopeId aFuncId);
  void TexSubImage(uint8_t funcDims, GLenum target, GLint level, GLint xOffset,
                   GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                   GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                   const TexImageSource& src, FuncScopeId aFuncId);
  void CompressedTexImage(uint8_t funcDims, GLenum target, GLint level,
                          GLenum internalFormat, GLsizei width, GLsizei height,
                          GLsizei depth, GLint border,
                          const TexImageSource& src,
                          const Maybe<GLsizei>& expectedImageSize,
                          FuncScopeId aFuncId);
  void CompressedTexSubImage(uint8_t funcDims, GLenum target, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat, const TexImageSource& src,
                             const Maybe<GLsizei>& expectedImageSize,
                             FuncScopeId aFuncId);

  ////////////////////////////////////

 public:
  void CopyTexSubImage2D(GLenum target, GLint level, GLint xOffset,
                         GLint yOffset, GLint x, GLint y, GLsizei width,
                         GLsizei height);

  ////////////////////////////////////

  void CopyTexSubImage3D(GLenum target, GLint level, GLint xOffset,
                         GLint yOffset, GLint zOffset, GLint x, GLint y,
                         GLsizei width, GLsizei height);

  // ------------------- Programs and shaders --------------------------------
 public:
  already_AddRefed<ClientWebGLProgram> CreateProgram();
  already_AddRefed<ClientWebGLShader> CreateShader(GLenum type);
  void GetAttachedShaders(
      const WebGLId<WebGLProgram>& prog,
      dom::Nullable<nsTArray<RefPtr<ClientWebGLShader>>>& retval);

  void UseProgram(const WebGLId<WebGLProgram>& prog);

  void ValidateProgram(const WebGLId<WebGLProgram>& prog);

  GLint GetFragDataLocation(const WebGLId<WebGLProgram>& prog,
                            const nsAString& name);

  // ------------------------ Uniforms and attributes ------------------------
 public:
  already_AddRefed<ClientWebGLActiveInfo> GetActiveAttrib(
      const WebGLId<WebGLProgram>& prog, GLuint index);

  already_AddRefed<ClientWebGLActiveInfo> GetActiveUniform(
      const WebGLId<WebGLProgram>& prog, GLuint index);

  void GetActiveUniforms(JSContext* cx, const WebGLId<WebGLProgram>& prog,
                         const dom::Sequence<GLuint>& uniformIndices,
                         GLenum pname, JS::MutableHandleValue retval);

  void GetUniformIndices(const WebGLId<WebGLProgram>& prog,
                         const dom::Sequence<nsString>& uniformNames,
                         dom::Nullable<nsTArray<GLuint>>& retval);

  void GetActiveUniformBlockParameter(JSContext* cx,
                                      const WebGLId<WebGLProgram>& prog,
                                      GLuint uniformBlockIndex, GLenum pname,
                                      JS::MutableHandleValue retval,
                                      ErrorResult& rv);

  void GetActiveUniformBlockName(const WebGLId<WebGLProgram>& prog,
                                 GLuint uniformBlockIndex, nsAString& retval);

  GLuint GetUniformBlockIndex(const WebGLId<WebGLProgram>& prog,
                              const nsAString& uniformBlockName);

  void GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                       JS::MutableHandle<JS::Value> retval, ErrorResult& rv);

  void Uniform1f(const WebGLId<WebGLUniformLocation>& aLoc, GLfloat x);

  void Uniform2f(const WebGLId<WebGLUniformLocation>& aLoc, GLfloat x,
                 GLfloat y);

  void Uniform3f(const WebGLId<WebGLUniformLocation>& aLoc, GLfloat x,
                 GLfloat y, GLfloat z);

  void Uniform4f(const WebGLId<WebGLUniformLocation>& aLoc, GLfloat x,
                 GLfloat y, GLfloat z, GLfloat w);

  void Uniform1i(const WebGLId<WebGLUniformLocation>& aLoc, GLint x);

  void Uniform2i(const WebGLId<WebGLUniformLocation>& aLoc, GLint x, GLint y);

  void Uniform3i(const WebGLId<WebGLUniformLocation>& aLoc, GLint x, GLint y,
                 GLint z);

  void Uniform4i(const WebGLId<WebGLUniformLocation>& aLoc, GLint x, GLint y,
                 GLint z, GLint w);

  void Uniform1ui(const WebGLId<WebGLUniformLocation>& aLoc, GLuint x);

  void Uniform2ui(const WebGLId<WebGLUniformLocation>& aLoc, GLuint x,
                  GLuint y);

  void Uniform3ui(const WebGLId<WebGLUniformLocation>& aLoc, GLuint x, GLuint y,
                  GLuint z);

  void Uniform4ui(const WebGLId<WebGLUniformLocation>& aLoc, GLuint x, GLuint y,
                  GLuint z, GLuint w);

#define FOO(N)                                                         \
  void Uniform##N##fv(WebGLId<WebGLUniformLocation> loc,               \
                      const Float32ListU& list, GLuint elemOffset = 0, \
                      GLuint elemCountOverride = 0);

  FOO(1)
  FOO(2)
  FOO(3)
  FOO(4)

#undef FOO

  //////

#define FOO(N)                                                       \
  void Uniform##N##iv(WebGLId<WebGLUniformLocation> loc,             \
                      const Int32ListU& list, GLuint elemOffset = 0, \
                      GLuint elemCountOverride = 0);

  FOO(1)
  FOO(2)
  FOO(3)
  FOO(4)

#undef FOO

  //////

#define FOO(N)                                                         \
  void Uniform##N##uiv(WebGLId<WebGLUniformLocation> loc,              \
                       const Uint32ListU& list, GLuint elemOffset = 0, \
                       GLuint elemCountOverride = 0);

  FOO(1)
  FOO(2)
  FOO(3)
  FOO(4)

#undef FOO

  //////

#define FOO(X, A, B)                                                           \
  void UniformMatrix##X##fv(WebGLId<WebGLUniformLocation> loc, bool transpose, \
                            const Float32ListU& list, GLuint elemOffset = 0,   \
                            GLuint elemCountOverride = 0);

  FOO(2, 2, 2)
  FOO(2x3, 2, 3)
  FOO(2x4, 2, 4)

  FOO(3x2, 3, 2)
  FOO(3, 3, 3)
  FOO(3x4, 3, 4)

  FOO(4x2, 4, 2)
  FOO(4x3, 4, 3)
  FOO(4, 4, 4)

#undef FOO

  void UniformBlockBinding(const WebGLId<WebGLProgram>& progId,
                           GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding);

  void EnableVertexAttribArray(GLuint index);

  void DisableVertexAttribArray(GLuint index);

  WebGLsizeiptr GetVertexAttribOffset(GLuint index, GLenum pname);

  void VertexAttrib1f(GLuint index, GLfloat x);
  void VertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
  void VertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);

  void VertexAttrib1fv(GLuint index, const Float32ListU& list);

  void VertexAttrib2fv(GLuint index, const Float32ListU& list);

  void VertexAttrib3fv(GLuint index, const Float32ListU& list);

  void VertexAttrib4fv(GLuint index, const Float32ListU& list);

  void VertexAttribIPointer(GLuint index, GLint size, GLenum type,
                            GLsizei stride, WebGLintptr byteOffset);

  void VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w,
                      FuncScopeId aFuncId = FuncScopeId::vertexAttrib4f);
  void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w,
                       FuncScopeId aFuncId = FuncScopeId::vertexAttribI4i);

  void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w,
                        FuncScopeId aFuncId = FuncScopeId::vertexAttribI4ui);

  void VertexAttribI4iv(GLuint index, const Int32ListU& list);

  void VertexAttribI4uiv(GLuint index, const Uint32ListU& list);

  void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                           WebGLboolean normalized, GLsizei stride,
                           WebGLintptr byteOffset);

  // -------------------------------- Drawing -------------------------------
 public:
  void DrawArrays(GLenum mode, GLint first, GLsizei count);

  void DrawElements(GLenum mode, GLsizei count, GLenum type,
                    WebGLintptr byteOffset);

  void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                         GLenum type, WebGLintptr byteOffset);

  // ------------------------------ Readback -------------------------------
 public:
  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type,
                  const dom::Nullable<dom::ArrayBufferView>& maybeView,
                  dom::CallerType aCallerType, ErrorResult& out_error) {
    const FuncScope funcScope(this, "readPixels");
    if (!ValidateNonNull("pixels", maybeView)) return;
    ReadPixels(x, y, width, height, format, type, maybeView.Value(), 0,
               aCallerType, out_error);
  }

  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, WebGLsizeiptr offset,
                  dom::CallerType aCallerType, ErrorResult& out_error);

  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type,
                  const dom::ArrayBufferView& dstData, GLuint dstElemOffset,
                  dom::CallerType aCallerType, ErrorResult& out_error);

 protected:
  bool ReadPixels_SharedPrecheck(dom::CallerType aCallerType,
                                 ErrorResult& out_error);

  // ------------------------------ Vertex Array ------------------------------
 public:
  already_AddRefed<ClientWebGLVertexArray> CreateVertexArray(
      bool aFromExtension = false);

  void DeleteVertexArray(const WebGLId<WebGLVertexArray>& array,
                         bool aFromExtension = false);

  void BindVertexArray(const WebGLId<WebGLVertexArray>& array,
                       bool aFromExtension = false);

  void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei primcount, bool aFromExtension = false);

  void DrawElementsInstanced(
      GLenum mode, GLsizei count, GLenum type, WebGLintptr offset,
      GLsizei primcount,
      FuncScopeId aFuncId = FuncScopeId::drawElementsInstanced,
      bool aFromExtension = false);

  void VertexAttribDivisor(GLuint index, GLuint divisor,
                           bool aFromExtension = false);

  // --------------------------------- GL Query
  // ---------------------------------
 public:
  already_AddRefed<ClientWebGLQuery> CreateQuery(bool aFromExtension = false);

  bool IsQuery(const WebGLId<WebGLQuery>& query,
               bool aFromExtension = false) const {
    return query != WebGLId<WebGLQuery>::Invalid();
  }

  void GetQuery(JSContext* cx, GLenum target, GLenum pname,
                JS::MutableHandleValue retval,
                bool aFromExtension = false) const;

  void GetQueryParameter(JSContext* cx, const WebGLId<WebGLQuery>& query,
                         GLenum pname, JS::MutableHandleValue retval,
                         bool aFromExtension = false) const;

  void DeleteQuery(const WebGLId<WebGLQuery>& query,
                   bool aFromExtension = false) const;

  void BeginQuery(GLenum target, const WebGLId<WebGLQuery>& query,
                  bool aFromExtension = false) const;

  void EndQuery(GLenum target, bool aFromExtension = false) const;

  void QueryCounter(const WebGLId<WebGLQuery>& query, GLenum target) const;

  // --------------------------- Buffer Operations --------------------------
 public:
  void ClearBufferfv(GLenum buffer, GLint drawBuffer, const Float32ListU& list,
                     GLuint srcElemOffset);
  void ClearBufferiv(GLenum buffer, GLint drawBuffer, const Int32ListU& list,
                     GLuint srcElemOffset);
  void ClearBufferuiv(GLenum buffer, GLint drawBuffer, const Uint32ListU& list,
                      GLuint srcElemOffset);
  void ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth,
                     GLint stencil);

  // -------------------------------- Sampler -------------------------------
  already_AddRefed<ClientWebGLSampler> CreateSampler();
  bool IsSampler(const WebGLId<WebGLSampler>& sampler) {
    return sampler != WebGLId<WebGLSampler>::Invalid();
  }

  void GetSamplerParameter(JSContext* cx, const WebGLId<WebGLSampler>& sampler,
                           GLenum pname, JS::MutableHandleValue retval);

  void DeleteSampler(const WebGLId<WebGLSampler>& aId);

  void BindSampler(GLuint unit, const WebGLId<WebGLSampler>& sampler);

  void SamplerParameteri(const WebGLId<WebGLSampler>& samplerId, GLenum pname,
                         GLint param);

  void SamplerParameterf(const WebGLId<WebGLSampler>& samplerId, GLenum pname,
                         GLfloat param);

  // ------------------------------- GL Sync ---------------------------------
  already_AddRefed<ClientWebGLSync> FenceSync(GLenum condition,
                                              GLbitfield flags);

  bool IsSync(const WebGLId<WebGLSync>& sync) {
    return sync != WebGLId<WebGLSync>::Invalid();
  }

  void GetSyncParameter(JSContext* cx, const WebGLId<WebGLSync>& sync,
                        GLenum pname, JS::MutableHandleValue retval);

  void DeleteSync(const WebGLId<WebGLSync>& sync);

  GLenum ClientWaitSync(const WebGLId<WebGLSync>& sync, GLbitfield flags,
                        GLuint64 timeout);

  void WaitSync(const WebGLId<WebGLSync>& sync, GLbitfield flags,
                GLint64 timeout);

  // -------------------------- Transform Feedback ---------------------------
  already_AddRefed<ClientWebGLTransformFeedback> CreateTransformFeedback();

  void DeleteTransformFeedback(const WebGLId<WebGLTransformFeedback>& tf);

  void BindTransformFeedback(GLenum target,
                             const WebGLId<WebGLTransformFeedback>& tf);

  void BeginTransformFeedback(GLenum primitiveMode);

  void EndTransformFeedback();

  void PauseTransformFeedback();

  void ResumeTransformFeedback();

  already_AddRefed<ClientWebGLActiveInfo> GetTransformFeedbackVarying(
      const WebGLId<WebGLProgram>& prog, GLuint index);

  bool IsTransformFeedback(const WebGLId<WebGLTransformFeedback>& tf) {
    return tf != WebGLId<WebGLTransformFeedback>::Invalid();
  }

  void TransformFeedbackVaryings(const WebGLId<WebGLProgram>& program,
                                 const dom::Sequence<nsString>& varyings,
                                 GLenum bufferMode);

  // ------------------------------ Extensions ------------------------------
 public:
  static const char* GetExtensionString(WebGLExtensionID ext);

  void GetSupportedExtensions(dom::Nullable<nsTArray<nsString>>& retval,
                              dom::CallerType callerType) {
    const Maybe<ExtensionSets>& exts = GetCachedExtensions();

    // DLP: TODO: Cache the value and return properly filtered string array
    if (exts) {
      nsTArray<nsString>& retarr = retval.SetValue();
      AddExtensionStrings(retarr, exts.ref().mNonSystem);
      if (callerType == dom::CallerType::System) {
        AddExtensionStrings(retarr, exts.ref().mSystem);
      }
    } else {
      retval.SetNull();
    }
  }

  void GetExtension(JSContext* cx, const nsAString& name,
                    JS::MutableHandle<JSObject*> retval,
                    dom::CallerType callerType, ErrorResult& rv);

 protected:
  void AddExtensionStrings(nsTArray<nsString>& retarr,
                           const nsTArray<WebGLExtensionID>& extarr) {
    for (auto& extension : extarr) {
      if (extension == WebGLExtensionID::MOZ_debug)
        continue;  // Hide MOZ_debug from this list.

      const char* extStr = GetExtensionString(extension);
      retarr.AppendElement(NS_ConvertUTF8toUTF16(extStr));
    }
  }

  const Maybe<ExtensionSets>& GetCachedExtensions();

  ClientWebGLExtensionBase* GetExtension(dom::CallerType callerType,
                                         WebGLExtensionID ext,
                                         bool toEnable = false);

  void EnableExtension(dom::CallerType callerType, WebGLExtensionID ext);

  ClientWebGLExtensionBase* UseExtension(WebGLExtensionID ext);

  Maybe<ExtensionSets> mExtensions;
  bool mEnabledExtension[static_cast<uint8_t>(WebGLExtensionID::Max)];

  // ---------------------------- Misc Extensions ----------------------------
 public:
  void DrawBuffers(const dom::Sequence<GLenum>& buffers,
                   bool aFromExtension = false);

  void GetASTCExtensionSupportedProfiles(
      dom::Nullable<nsTArray<nsString>>& retval) const;

  void GetTranslatedShaderSource(const WebGLId<WebGLShader>& shader,
                                 nsAString& retval) const;

  void LoseContext(bool isSimulated = true);

  void RestoreContext();

  void MOZDebugGetParameter(JSContext* cx, GLenum pname,
                            JS::MutableHandle<JS::Value> retval,
                            ErrorResult& rv) const;

  // -------------------------------------------------------------------------
  // Client-side methods.  Calls in the Host are forwarded to the client.
  // -------------------------------------------------------------------------
 public:
  void PostWarning(const nsCString& aWarning);
  void PostContextCreationError(const nsCString& msg);
  void OnLostContext();
  void OnRestoredContext();

 public:
  // The actor failed on the host side.  Make sure that we don't continue to try
  // to issue commands.
  void OnQueueFailed() {
    mContextLost = true;
    mWebGLChild = nullptr;
    DrainErrorQueue();
  }

  // -------------------------------------------------------------------------
  // The cross-process communication mechanism
  // -------------------------------------------------------------------------
 protected:
  template <size_t command, typename... Args>
  void DispatchAsync(Args&&... aArgs) const {
    if (mContextLost) {
      return;
    }

    // This method is const so that const client methods can call it but the
    // RPC mechanism needs non-const.
    auto nonConstThis = const_cast<ClientWebGLContext*>(this);
    PcqStatus status =
        nonConstThis->mCommandSource->RunAsyncCommand(command, aArgs...);
    if (!IsSuccess(status)) {
      if (status == PcqStatus::PcqOOMError) {
        nonConstThis->PostWarning(
            nsCString("Ran out-of-memory during WebGL IPC."));
      }
      // Not much to do but shut down.  Since this was a Pcq failure and
      // may have been catastrophic, we don't try to revive it.  Make sure to
      // post "webglcontextlost"
      MOZ_ASSERT_UNREACHABLE(
          "TODO: Make this shut down the context, actors, everything.");
    }
  }

  template <size_t command, typename ReturnType, typename... Args>
  ReturnType DispatchSync(Args&&... aArgs) const {
    if (mContextLost) {
      return ReturnType();  // TODO: ?? Is this right?
    }

    // This method is const so that const client methods can call it but the
    // RPC mechanism needs non-const.
    auto nonConstThis = const_cast<ClientWebGLContext*>(this);
    ReturnType returnValue;
    PcqStatus status = nonConstThis->mCommandSource->RunSyncCommand(
        command, returnValue, aArgs...);

    if (!IsSuccess(status)) {
      if (status == PcqStatus::PcqOOMError) {
        nonConstThis->PostWarning(
            nsCString("Ran out-of-memory during WebGL IPC."));
      }
      // Not much to do but shut down.  Since this was a Pcq failure and
      // may have been catastrophic, we don't try to revive it.  Make sure to
      // post "webglcontextlost"
      MOZ_ASSERT_UNREACHABLE(
          "TODO: Make this shut down the context, actors, everything.");
    }

    // TODO: Should I really do this here or require overloads (in this class)
    // of each function that wants it?
    nonConstThis->DrainErrorQueue();
    return returnValue;
  }

  template <size_t command, typename... Args>
  void DispatchVoidSync(Args&&... aArgs) const {
    if (mContextLost) {
      return;
    }

    // This method is const so that const client methods can call it but the
    // RPC mechanism needs non-const.
    auto nonConstThis = const_cast<ClientWebGLContext*>(this);
    PcqStatus status =
        nonConstThis->mCommandSource->RunVoidSyncCommand(command, aArgs...);

    if (!IsSuccess(status)) {
      if (status == PcqStatus::PcqOOMError) {
        nonConstThis->PostWarning(
            nsCString("Ran out-of-memory during WebGL IPC."));
      }
      // Not much to do but shut down.  Since this was a Pcq failure and
      // may have been catastrophic, we don't try to revive it.  Make sure to
      // post "webglcontextlost"
      MOZ_ASSERT_UNREACHABLE(
          "TODO: Make this shut down the context, actors, everything.");
    }

    // TODO: Should I really do this here or require overloads (in this class)
    // of each function that wants it?
    nonConstThis->DrainErrorQueue();
  }

  // Drains the error and warning queue completely.
  // This is called in a recurring fashion, starting with the constructor.
  // Other methods may call this from the main thread at any time but they
  // should not tell the drain task to reissue.
  void DrainErrorQueue(bool toReissue = false);

  friend void mozilla::DrainWebGLError(nsWeakPtr aWeakContext);

  template <typename ReturnType>
  friend struct WebGLClientDispatcher;

  // If we are running WebGL in this process then call the HostWebGLContext
  // method directly.  Otherwise, dispatch over IPC.
  template <typename MethodType, MethodType method, typename ReturnType,
            size_t Id, typename... Args>
  ReturnType Run(Args&&... aArgs) const;

  UniquePtr<ClientWebGLCommandSource> mCommandSource;

  UniquePtr<ClientWebGLErrorSink> mErrorSink;
  RefPtr<Runnable> mDrainErrorRunnable;

  // In the cross process case, the WebGL actor's ownership relationship looks
  // like this:
  // ---------------------------------------------------------------------
  // | ClientWebGLContext -> WebGLChild -> WebGLParent -> HostWebGLContext
  // ---------------------------------------------------------------------
  //
  // where 'A -> B' means "A owns B"
  mozilla::dom::WebGLChild* mWebGLChild;

  UniquePtr<HostWebGLContext> mHostContext;

  // -------------------------------------------------------------------------
  // Construction/Destruction
  // -------------------------------------------------------------------------
 protected:
  friend class WebGLContextUserData;

  // Cross-process client constructor
  ClientWebGLContext(dom::WebGLChild* aWebGLChild, WebGLVersion aVersion,
                     UniquePtr<ClientWebGLCommandSource>&& aCommandSource,
                     UniquePtr<ClientWebGLErrorSink>&& aErrorSink);

  // The single process constructor.  The host and client point directly at
  // one another.
  ClientWebGLContext(UniquePtr<HostWebGLContext>&& aHost);

  static RefPtr<ClientWebGLContext> MakeSingleProcessWebGLContext(
      WebGLVersion aVersion);

  static RefPtr<ClientWebGLContext> MakeCrossProcessWebGLContext(
      WebGLVersion aVersion);

  virtual ~ClientWebGLContext();

  // -------------------------------------------------------------------------
  // Helpers for DOM operations, composition, actors, etc
  // -------------------------------------------------------------------------
 public:
  WebGLPixelStore GetPixelStore() { return mPixelStore; }
  const WebGLPixelStore GetPixelStore() const { return mPixelStore; }

 protected:
  bool IsHostOOP() const { return !mHostContext; }

  void ForceLoseContext();
  void LoseOldestWebGLContextIfLimitExceeded();
  void UpdateLastUseIndex();

  // Prepare the context for capture before compositing
  void BeginComposition();

  // Clean up the context after captured for compositing
  void EndComposition();

  mozilla::dom::Document* GetOwnerDoc() const;

  bool mContextLost = false;
  uint64_t mLastUseIndex = 0;
  bool mResetLayer = true;
  bool mOptionsFrozen = false;
  WebGLContextOptions mOptions;
  WebGLPixelStore mPixelStore;
};

template <typename WebGLObjectType>
JS::Value ClientWebGLContext::WebGLObjectAsJSValue(
    JSContext* cx, RefPtr<WebGLObjectType>&& object, ErrorResult& rv) const {
  if (!object) return JS::NullValue();

  MOZ_ASSERT(this == object->GetParentObject());
  JS::Rooted<JS::Value> v(cx);
  JS::Rooted<JSObject*> wrapper(cx, GetWrapper());
  JSAutoRealm ar(cx, wrapper);
  if (!dom::GetOrCreateDOMReflector(cx, object, &v)) {
    rv.Throw(NS_ERROR_FAILURE);
    return JS::NullValue();
  }
  return v;
}

template <typename WebGLObjectType>
JS::Value ClientWebGLContext::WebGLObjectAsJSValue(
    JSContext* cx, const WebGLObjectType* object, ErrorResult& rv) const {
  if (!object) return JS::NullValue();

  MOZ_ASSERT(this == object->GetParentObject());
  JS::Rooted<JS::Value> v(cx);
  JS::Rooted<JSObject*> wrapper(cx, GetWrapper());
  JSAutoRealm ar(cx, wrapper);
  if (!dom::GetOrCreateDOMReflector(cx, const_cast<WebGLObjectType*>(object),
                                    &v)) {
    rv.Throw(NS_ERROR_FAILURE);
    return JS::NullValue();
  }
  return v;
}

template <typename WebGLObjectType>
JSObject* ClientWebGLContext::WebGLObjectAsJSObject(
    JSContext* cx, const WebGLObjectType* object, ErrorResult& rv) const {
  JS::Value v = WebGLObjectAsJSValue(cx, object, rv);
  if (v.isNull()) return nullptr;

  return &v.toObject();
}

// used by DOM bindings in conjunction with GetParentObject
inline nsISupports* ToSupports(ClientWebGLContext* webgl) {
  return static_cast<nsICanvasRenderingContextInternal*>(webgl);
}

}  // namespace mozilla

#endif  // CLIENTWEBGLCONTEXT_H_
