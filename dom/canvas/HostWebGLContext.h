/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HOSTWEBGLCONTEXT_H_
#define HOSTWEBGLCONTEXT_H_

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/HashTable.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/HashTable.h"
#include "nsString.h"
#include "WebGLContext.h"
#include "WebGL2Context.h"
#include "WebGLContextEndpoint.h"
#include "mozilla/dom/WebGLTypes.h"
#include "WebGLActiveInfo.h"
#include "WebGLErrorQueue.h"
#include "WebGLShaderPrecisionFormat.h"

#ifndef WEBGL_BRIDGE_LOG_
#  define WEBGL_BRIDGE_LOG_(lvl, ...) \
    MOZ_LOG(mozilla::gWebGLBridgeLog, lvl, (__VA_ARGS__))
#  define WEBGL_BRIDGE_LOGD(...) WEBGL_BRIDGE_LOG_(LogLevel::Debug, __VA_ARGS__)
#  define WEBGL_BRIDGE_LOGE(...) WEBGL_BRIDGE_LOG_(LogLevel::Error, __VA_ARGS__)
#endif  // WEBGL_BRIDGE_LOG_

namespace mozilla {

class HostWebGLCommandSink;

extern LazyLogModule gWebGLBridgeLog;

namespace layers {
class CompositableHost;
}

/**
 * Host endpoint of a WebGLContext.  HostWebGLContext owns a WebGLContext
 * that it uses to execute commands sent from its ClientWebGLContext.
 *
 * The HostWebGLContext provides host implementions of methods from the
 * WebGLContextEndpoint.
 * A HostWebGLContext continuously issues a Task to the Compositor thread that
 * causes it to drain its queue of commands.  It also maintains a map of WebGL
 * objects (e.g. ObjectIdMap<WebGLShader>) that it uses associate them with
 * their cross-process IDs.
 *
 * This class is not an implementation of the
 * nsICanvasRenderingContextInternal DOM class.  That is the
 * ClientWebGLContext.
 */
class HostWebGLContext : public WebGLContextEndpoint {
 public:
  static UniquePtr<HostWebGLContext> Create(
      WebGLVersion aVersion,
      UniquePtr<HostWebGLCommandSink>&& aCommandSink = nullptr,
      UniquePtr<HostWebGLErrorSource>&& aErrorSource = nullptr);

  virtual ~HostWebGLContext();

  WebGLContext* GetWebGLContext() { return mContext; }

 protected:
  HostWebGLContext(WebGLVersion aVersion, RefPtr<WebGLContext> aContext,
                   UniquePtr<HostWebGLCommandSink>&& aCommandSink,
                   UniquePtr<HostWebGLErrorSource>&& aErrorSource);

  // These are null only if we are running single-process WebGL
  UniquePtr<HostWebGLCommandSink> mCommandSink;
  UniquePtr<HostWebGLErrorSource> mErrorSource;

  // The host-side of an object ID map for types for which the client
  // generates the IDs.  (This is the majority of the types of objects that
  // we maintain across processes.)
  template <typename ObjectType>
  class ObjectIdMap {
   public:
    using IdType = WebGLId<ObjectType>;
    using PtrType = ObjectType*;
    using RefType = RefPtr<ObjectType>;
    using AlreadyAddRefedType = already_AddRefed<ObjectType>;

    virtual IdType Insert(RefType&& aObj, const IdType& aId) {
      // asynchronous contructors must never fail
      MOZ_ASSERT(aId && aObj &&
                 ((aObj->Id() == aId.Id()) || (aObj->Id() == 0)));
      aObj->mId = aId.Id();
      Unused << mMap.put(aId, std::move(aObj));
      return aId;
    }

    PtrType Find(const IdType& aId) const {
      if (!aId) {
        return nullptr;
      }
      auto it = mMap.lookup(aId);
      return it ? it->value().get() : nullptr;
    }

    void Remove(const IdType& aId) {
      if (!aId) {
        return;
      }
      auto it = mMap.lookup(aId);
      if (!it) {
        return;
      }
      MOZ_ASSERT(it->value()->Id() == aId.Id());

      // NB: We leave it->value()->Id() as is because it may be resurrected
      // if the WebGLContext still holds a reference to it and later returns it.

      mMap.remove(it);
    }

   protected:
    using IdMap = HashMap<WebGLId<ObjectType>, RefPtr<ObjectType>>;
    IdMap mMap;
  };

  // The host-side of an object ID map for types for which the host
  // generates the IDs.
  template <typename ObjectType>
  class HostGenObjectIdMap : public ObjectIdMap<ObjectType> {
   public:
    using BaseType = ObjectIdMap<ObjectType>;
    using IdType = typename BaseType::IdType;
    using RefType = typename BaseType::RefType;
    using AlreadyAddRefedType = typename BaseType::AlreadyAddRefedType;

    HostGenObjectIdMap() : mNextId(1) {}

    // Generate or resurrect aObj.
    IdType Insert(RefType&& aObj, const IdType& aId) override {
      MOZ_ASSERT((!aId) && aObj);
      uint64_t curId;
      if (aObj->Id()) {
        curId = aObj->Id();
      } else {
        curId = mNextId;
        ++mNextId;
        aObj->mId = curId;
      }
      IdType ret(curId);
      Unused << BaseType::mMap.put(ret, std::move(aObj));
      return ret;
    }

   private:
    uint64_t mNextId;
  };

#define DECLARE_OBJECT_ID_MAP_FUNCS(_WebGLType)                                \
  WebGLId<WebGL##_WebGLType> Insert(RefPtr<WebGL##_WebGLType>&& aObj,          \
                                    const WebGLId<WebGL##_WebGLType>& aId =    \
                                        WebGLId<WebGL##_WebGLType>::Invalid()) \
      const;                                                                   \
  WebGL##_WebGLType* Find(const WebGLId<WebGL##_WebGLType>& aId) const;        \
  void Remove(const WebGLId<WebGL##_WebGLType>& aId) const;

  // Use this when failure to find an object by ID is a fatal error.
  template <typename WebGLType>
  WebGLType* MustFind(const WebGLId<WebGLType>& aId) const {
    auto ret = Find(aId);
    MOZ_RELEASE_ASSERT(ret);
    return ret;
  }

  // Defines a host-side Object ID Map (client generates the IDs) and a few
  // convenient methods that forward to them.  If _WebGLType needs IDs to be
  // generated on the host side then use DEFINE_HOST_GEN_OBJECT_ID_MAP.
#define DEFINE_OBJECT_ID_MAP(_WebGLType)                     \
  mutable ObjectIdMap<WebGL##_WebGLType> m##_WebGLType##Map; \
  DECLARE_OBJECT_ID_MAP_FUNCS(_WebGLType)

  DEFINE_OBJECT_ID_MAP(Framebuffer);
  DEFINE_OBJECT_ID_MAP(Program);
  DEFINE_OBJECT_ID_MAP(Query);
  DEFINE_OBJECT_ID_MAP(Renderbuffer);
  DEFINE_OBJECT_ID_MAP(Sampler);
  DEFINE_OBJECT_ID_MAP(Shader);
  DEFINE_OBJECT_ID_MAP(Sync);
  DEFINE_OBJECT_ID_MAP(TransformFeedback);
  DEFINE_OBJECT_ID_MAP(VertexArray);

#undef DEFINE_OBJECT_ID_MAP

  // Defines a host-side Object ID Map (host generates the IDs) and a few
  // convenient methods that forward to them.
#define DEFINE_HOST_GEN_OBJECT_ID_MAP(_WebGLType)                   \
  mutable HostGenObjectIdMap<WebGL##_WebGLType> m##_WebGLType##Map; \
  DECLARE_OBJECT_ID_MAP_FUNCS(_WebGLType)

  DEFINE_HOST_GEN_OBJECT_ID_MAP(Buffer);
  DEFINE_HOST_GEN_OBJECT_ID_MAP(Texture);
  DEFINE_HOST_GEN_OBJECT_ID_MAP(UniformLocation);

#undef DEFINE_HOST_GEN_OBJECT_ID_MAP
#undef DECLARE_OBJECT_ID_MAP_FUNCS

  template <typename T>
  nsTArray<WebGLId<T>> MapArrayOfObjectsToIds(
      const nsTArray<RefPtr<T>>& objects) {
    nsTArray<WebGLId<T>> ret;
    for (auto& object : objects) {
      ret.AppendElement(object ? object->GetWebGLId() : WebGLId<T>::Invalid());
    }
    return ret;
  }

  // -------------------------------------------------------------------------
  // RPC Framework
  // -------------------------------------------------------------------------

 public:
  CommandResult RunCommandsForDuration(TimeDuration aDuration);

  // This must be called if this is a Host for a single-process context
  void SetClientContext(ClientWebGLContext* aClientContext) {
    MOZ_ASSERT(!mCommandSink);
    mClientContext = aClientContext;
  }

  // -------------------------------------------------------------------------
  // Host-side methods.  Calls in the client are forwarded to the host.
  // -------------------------------------------------------------------------

 public:
  // ------------------------- Creation/Destruction -------------------------
  // When the client releases its handle to this, we release ours.  Note that
  // we may later ressurrect the object if the WebGLContext or some other
  // internal class still holds a reference to it.  In that case, we will still
  // keep the old ID.
  template <typename WebGLType>
  void ReleaseWebGLObject(const WebGLId<WebGLType>& o) {
    Remove(o);
  }

  // ------------------------- Composition -------------------------
  void Present();

  Maybe<ICRData> InitializeCanvasRenderer(layers::LayersBackend backend);

  void SetContextOptions(const WebGLContextOptions& options);

  SetDimensionsData SetDimensions(int32_t signedWidth, int32_t signedHeight);

  gfx::IntSize DrawingBufferSize();

  void SetCompositableHost(RefPtr<layers::CompositableHost>& aCompositableHost);

  void OnMemoryPressure();

  void AllowContextRestore();

  void DidRefresh();

  // ------------------------- GL State -------------------------
  bool IsContextLost() const;

  void Disable(GLenum cap);

  void Enable(GLenum cap);

  bool IsEnabled(GLenum cap);

  MaybeWebGLVariant GetParameter(GLenum pname);

  void AttachShader(const WebGLId<WebGLProgram>& progId,
                    const WebGLId<WebGLShader>& shaderId);

  void BindAttribLocation(const WebGLId<WebGLProgram>& progId, GLuint location,
                          const nsString& name);

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

  void CreateFramebuffer(const WebGLId<WebGLFramebuffer>& aId);

  void CreateProgram(const WebGLId<WebGLProgram>& aId);

  void CreateRenderbuffer(const WebGLId<WebGLRenderbuffer>& aId);

  void CreateShader(GLenum aType, const WebGLId<WebGLShader>& aId);

  void CullFace(GLenum face);

  void DeleteFramebuffer(const WebGLId<WebGLFramebuffer>& fb);

  void DeleteProgram(const WebGLId<WebGLProgram>& prog);

  void DeleteRenderbuffer(const WebGLId<WebGLRenderbuffer>& rb);

  void DeleteShader(const WebGLId<WebGLShader>& shader);

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

  Maybe<WebGLActiveInfo> GetActiveAttrib(const WebGLId<WebGLProgram>& progId,
                                         GLuint index);

  Maybe<WebGLActiveInfo> GetActiveUniform(const WebGLId<WebGLProgram>& progId,
                                          GLuint index);

  MaybeAttachedShaders GetAttachedShaders(const WebGLId<WebGLProgram>& progId);

  GLint GetAttribLocation(const WebGLId<WebGLProgram>& progId,
                          const nsString& name);

  MaybeWebGLVariant GetBufferParameter(GLenum target, GLenum pname);

  GLenum GetError();

  MaybeWebGLVariant GetFramebufferAttachmentParameter(GLenum target,
                                                      GLenum attachment,
                                                      GLenum pname);

  MaybeWebGLVariant GetProgramParameter(const WebGLId<WebGLProgram>& progId,
                                        GLenum pname);

  nsString GetProgramInfoLog(const WebGLId<WebGLProgram>& progId);

  MaybeWebGLVariant GetRenderbufferParameter(GLenum target, GLenum pname);

  MaybeWebGLVariant GetShaderParameter(const WebGLId<WebGLShader>& shaderId,
                                       GLenum pname);

  MaybeWebGLVariant GetShaderPrecisionFormat(GLenum shadertype,
                                             GLenum precisiontype);

  nsString GetShaderInfoLog(const WebGLId<WebGLShader>& shaderId);

  nsString GetShaderSource(const WebGLId<WebGLShader>& shaderId);

  MaybeWebGLVariant GetUniform(const WebGLId<WebGLProgram>& progId,
                               const WebGLId<WebGLUniformLocation>& locId);

  WebGLId<WebGLUniformLocation> GetUniformLocation(
      const WebGLId<WebGLProgram>& progId, const nsString& name);

  void Hint(GLenum target, GLenum mode);

  void LineWidth(GLfloat width);

  void LinkProgram(const WebGLId<WebGLProgram>& progId);

  WebGLPixelStore PixelStorei(GLenum pname, GLint param);

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
  void BindBuffer(GLenum target, const WebGLId<WebGLBuffer>& buffer);

  void BindBufferBase(GLenum target, GLuint index,
                      const WebGLId<WebGLBuffer>& buffer);

  void BindBufferRange(GLenum target, GLuint index,
                       const WebGLId<WebGLBuffer>& buffer, WebGLintptr offset,
                       WebGLsizeiptr size);

  WebGLId<WebGLBuffer> CreateBuffer();

  void DeleteBuffer(const WebGLId<WebGLBuffer>& buf);

  void CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         GLintptr readOffset, GLintptr writeOffset,
                         GLsizeiptr size);

  Maybe<UniquePtr<RawBuffer<>>> GetBufferSubData(GLenum target,
                                                 GLintptr srcByteOffset,
                                                 size_t byteLen);

  void BufferData(GLenum target, const RawBuffer<>& data, GLenum usage);

  void BufferSubData(GLenum target, WebGLsizeiptr dstByteOffset,
                     const RawBuffer<>& srcData);

  // -------------------------- Framebuffer Objects --------------------------
  void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter);

  void FramebufferTextureLayer(GLenum target, GLenum attachment,
                               const WebGLId<WebGLTexture>& textureId,
                               GLint level, GLint layer);

  void InvalidateFramebuffer(GLenum target,
                             const nsTArray<GLenum>& attachments);

  void InvalidateSubFramebuffer(GLenum target,
                                const nsTArray<GLenum>& attachments, GLint x,
                                GLint y, GLsizei width, GLsizei height);

  void ReadBuffer(GLenum mode);

  // ----------------------- Renderbuffer objects -----------------------
  Maybe<nsTArray<int32_t>> GetInternalformatParameter(GLenum target,
                                                      GLenum internalformat,
                                                      GLenum pname);

  void RenderbufferStorage_base(GLenum target, GLsizei samples,
                                GLenum internalFormat, GLsizei width,
                                GLsizei height, FuncScopeId aFuncId);

  // --------------------------- Texture objects ---------------------------
  void ActiveTexture(GLenum texUnit);

  void BindTexture(GLenum texTarget, const WebGLId<WebGLTexture>& tex);

  WebGLId<WebGLTexture> CreateTexture();

  void DeleteTexture(const WebGLId<WebGLTexture>& tex);

  void GenerateMipmap(GLenum texTarget);

  void CopyTexImage2D(GLenum target, GLint level, GLenum internalFormat,
                      GLint x, GLint y, uint32_t width, uint32_t height,
                      uint32_t depth);

  void TexStorage(uint8_t funcDims, GLenum target, GLsizei levels,
                  GLenum internalFormat, GLsizei width, GLsizei height,
                  GLsizei depth, FuncScopeId aFuncId);

  void TexImage(uint8_t funcDims, GLenum target, GLint level,
                GLenum internalFormat, GLsizei width, GLsizei height,
                GLsizei depth, GLint border, GLenum unpackFormat,
                GLenum unpackType, MaybeWebGLTexUnpackVariant&& src,
                FuncScopeId aFuncId);

  void TexSubImage(uint8_t funcDims, GLenum target, GLint level, GLint xOffset,
                   GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                   GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                   MaybeWebGLTexUnpackVariant&& src, FuncScopeId aFuncId);

  void CompressedTexImage(uint8_t funcDims, GLenum target, GLint level,
                          GLenum internalFormat, GLsizei width, GLsizei height,
                          GLsizei depth, GLint border,
                          MaybeWebGLTexUnpackVariant&& src,
                          const Maybe<GLsizei>& expectedImageSize,
                          FuncScopeId aFuncId);

  void CompressedTexSubImage(uint8_t funcDims, GLenum target, GLint level,
                             GLint xOffset, GLint yOffset, GLint zOffset,
                             GLsizei width, GLsizei height, GLsizei depth,
                             GLenum unpackFormat,
                             MaybeWebGLTexUnpackVariant&& src,
                             const Maybe<GLsizei>& expectedImageSize,
                             FuncScopeId aFuncId);

  void CopyTexSubImage(uint8_t funcDims, GLenum target, GLint level,
                       GLint xOffset, GLint yOffset, GLint zOffset, GLint x,
                       GLint y, uint32_t width, uint32_t height, uint32_t depth,
                       FuncScopeId aFuncId);

  MaybeWebGLVariant GetTexParameter(GLenum texTarget, GLenum pname);

  void TexParameter_base(GLenum texTarget, GLenum pname,
                         const FloatOrInt& param);

  // ------------------- Programs and shaders --------------------------------
  void UseProgram(const WebGLId<WebGLProgram>& prog);

  void ValidateProgram(const WebGLId<WebGLProgram>& progId);

  GLint GetFragDataLocation(const WebGLId<WebGLProgram>& progId,
                            const nsString& name);

  // ------------------------ Uniforms and attributes ------------------------
  void UniformNfv(const nsCString& funcName, uint8_t N,
                  const WebGLId<WebGLUniformLocation>& loc,
                  const RawBuffer<const float>& arr, GLuint elemOffset,
                  GLuint elemCountOverride);

  void UniformNiv(const nsCString& funcName, uint8_t N,
                  const WebGLId<WebGLUniformLocation>& loc,
                  const RawBuffer<const int32_t>& arr, GLuint elemOffset,
                  GLuint elemCountOverride);

  void UniformNuiv(const nsCString& funcName, uint8_t N,
                   const WebGLId<WebGLUniformLocation>& loc,
                   const RawBuffer<const uint32_t>& arr, GLuint elemOffset,
                   GLuint elemCountOverride);

  void UniformMatrixAxBfv(const nsCString& funcName, uint8_t A, uint8_t B,
                          const WebGLId<WebGLUniformLocation>& loc,
                          bool transpose, const RawBuffer<const float>& arr,
                          GLuint elemOffset, GLuint elemCountOverride);

  void UniformFVec(const WebGLId<WebGLUniformLocation>& aLoc,
                   const nsTArray<float>& vec);

  void UniformIVec(const WebGLId<WebGLUniformLocation>& aLoc,
                   const nsTArray<int32_t>& vec);

  void UniformUIVec(const WebGLId<WebGLUniformLocation>& aLoc,
                    const nsTArray<uint32_t>& vec);
  void VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w,
                      FuncScopeId aFuncId);

  void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w,
                       FuncScopeId aFuncId = FuncScopeId::vertexAttribI4i);

  void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w,
                        FuncScopeId aFuncId = FuncScopeId::vertexAttribI4ui);

  void VertexAttribDivisor(GLuint index, GLuint divisor,
                           bool aFromExtension = false);

  MaybeWebGLVariant GetIndexedParameter(GLenum target, GLuint index);

  MaybeWebGLVariant GetUniformIndices(const WebGLId<WebGLProgram>& progId,
                                      const nsTArray<nsString>& uniformNames);

  MaybeWebGLVariant GetActiveUniforms(const WebGLId<WebGLProgram>& progId,
                                      const nsTArray<GLuint>& uniformIndices,
                                      GLenum pname);

  GLuint GetUniformBlockIndex(const WebGLId<WebGLProgram>& progId,
                              const nsString& uniformBlockName);

  MaybeWebGLVariant GetActiveUniformBlockParameter(
      const WebGLId<WebGLProgram>& progId, GLuint uniformBlockIndex,
      GLenum pname);

  nsString GetActiveUniformBlockName(const WebGLId<WebGLProgram>& progId,
                                     GLuint uniformBlockIndex);

  void UniformBlockBinding(const WebGLId<WebGLProgram>& progId,
                           GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding);

  void EnableVertexAttribArray(GLuint index);

  void DisableVertexAttribArray(GLuint index);

  MaybeWebGLVariant GetVertexAttrib(GLuint index, GLenum pname);

  WebGLsizeiptr GetVertexAttribOffset(GLuint index, GLenum pname);

  void VertexAttribAnyPointer(bool isFuncInt, GLuint index, GLint size,
                              GLenum type, bool normalized, GLsizei stride,
                              WebGLintptr byteOffset, FuncScopeId aFuncId);

  // --------------------------- Buffer Operations --------------------------
  void ClearBufferfv(GLenum buffer, GLint drawBuffer,
                     const RawBuffer<const float>& src, GLuint srcElemOffset);
  void ClearBufferiv(GLenum buffer, GLint drawBuffer,
                     const RawBuffer<const int32_t>& src, GLuint srcElemOffset);
  void ClearBufferuiv(GLenum buffer, GLint drawBuffer,
                      const RawBuffer<const uint32_t>& src,
                      GLuint srcElemOffset);
  void ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth,
                     GLint stencil);

  // ------------------------------ Readback -------------------------------
  void ReadPixels1(GLint x, GLint y, GLsizei width, GLsizei height,
                   GLenum format, GLenum type, WebGLsizeiptr offset);

  Maybe<UniquePtr<RawBuffer<>>> ReadPixels2(GLint x, GLint y, GLsizei width,
                                            GLsizei height, GLenum format,
                                            GLenum type, size_t byteLen);

  // ----------------------------- Sampler -----------------------------------
  void CreateSampler(const WebGLId<WebGLSampler>& aId);

  void DeleteSampler(const WebGLId<WebGLSampler>& aId);

  void BindSampler(GLuint unit, const WebGLId<WebGLSampler>& sampler);

  void SamplerParameteri(const WebGLId<WebGLSampler>& samplerId, GLenum pname,
                         GLint param);

  void SamplerParameterf(const WebGLId<WebGLSampler>& samplerId, GLenum pname,
                         GLfloat param);

  MaybeWebGLVariant GetSamplerParameter(const WebGLId<WebGLSampler>& samplerId,
                                        GLenum pname);

  // ------------------------------- GL Sync ---------------------------------
  WebGLId<WebGLSync> FenceSync(const WebGLId<WebGLSync>& aId, GLenum condition,
                               GLbitfield flags);

  void DeleteSync(const WebGLId<WebGLSync>& sync);

  GLenum ClientWaitSync(const WebGLId<WebGLSync>& sync, GLbitfield flags,
                        GLuint64 timeout);

  void WaitSync(const WebGLId<WebGLSync>& sync, GLbitfield flags,
                GLint64 timeout);

  MaybeWebGLVariant GetSyncParameter(const WebGLId<WebGLSync>& sync,
                                     GLenum pname);

  // -------------------------- Transform Feedback ---------------------------
  void CreateTransformFeedback(const WebGLId<WebGLTransformFeedback>& aId);

  void DeleteTransformFeedback(const WebGLId<WebGLTransformFeedback>& tf);

  void BindTransformFeedback(GLenum target,
                             const WebGLId<WebGLTransformFeedback>& tf);

  void BeginTransformFeedback(GLenum primitiveMode);

  void EndTransformFeedback();

  void PauseTransformFeedback();

  void ResumeTransformFeedback();

  void TransformFeedbackVaryings(const WebGLId<WebGLProgram>& prog,
                                 const nsTArray<nsString>& varyings,
                                 GLenum bufferMode);

  Maybe<WebGLActiveInfo> GetTransformFeedbackVarying(
      const WebGLId<WebGLProgram>& prog, GLuint index);

  // ------------------------------ WebGL Debug
  // ------------------------------------
  void EnqueueError(GLenum aGLError, const nsCString& aMsg);

  void EnqueueWarning(const nsCString& aMsg);

  void ReportOOMAndLoseContext();

  // -------------------------------------------------------------------------
  // Host-side extension methods.  Calls in the client are forwarded to the
  // host. Some extension methods are also available in WebGL2 Contexts.  For
  // them, the final parameter is a boolean indicating if the call originated
  // from an extension.
  // -------------------------------------------------------------------------

  // Misc. Extensions
  void EnableExtension(dom::CallerType callerType, WebGLExtensionID ext);

  const Maybe<ExtensionSets> GetSupportedExtensions();

  void DrawBuffers(const nsTArray<GLenum>& buffers,
                   bool aFromExtension = false);

  Maybe<nsTArray<nsString>> GetASTCExtensionSupportedProfiles() const;

  nsString GetTranslatedShaderSource(const WebGLId<WebGLShader>& shader) const;

  void LoseContext(bool isSimulated = true);

  void RestoreContext();

  MaybeWebGLVariant MOZDebugGetParameter(GLenum pname) const;

  // VertexArrayObjectEXT
  void BindVertexArray(const WebGLId<WebGLVertexArray>& array,
                       bool aFromExtension = false);

  void CreateVertexArray(const WebGLId<WebGLVertexArray>& aId,
                         bool aFromExtension = false);

  void DeleteVertexArray(const WebGLId<WebGLVertexArray>& array,
                         bool aFromExtension = false);

  // InstancedElementsEXT
  void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei primcount, bool aFromExtension = false);

  void DrawElementsInstanced(
      GLenum mode, GLsizei count, GLenum type, WebGLintptr offset,
      GLsizei primcount,
      FuncScopeId aFuncId = FuncScopeId::drawElementsInstanced,
      bool aFromExtension = false);

  void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                         GLenum type, WebGLintptr byteOffset);

  // GLQueryEXT
  void CreateQuery(const WebGLId<WebGLQuery>& aId,
                   bool aFromExtension = false) const;

  void DeleteQuery(const WebGLId<WebGLQuery>& query,
                   bool aFromExtension = false) const;

  void BeginQuery(GLenum target, const WebGLId<WebGLQuery>& query,
                  bool aFromExtension = false) const;

  void EndQuery(GLenum target, bool aFromExtension = false) const;

  void QueryCounter(const WebGLId<WebGLQuery>& query, GLenum target) const;

  MaybeWebGLVariant GetQuery(GLenum target, GLenum pname,
                             bool aFromExtension = false) const;

  MaybeWebGLVariant GetQueryParameter(const WebGLId<WebGLQuery>& query,
                                      GLenum pname,
                                      bool aFromExtension = false) const;

  // -------------------------------------------------------------------------
  // Client-side methods.  Calls in the Host are forwarded to the client.
  // -------------------------------------------------------------------------
 public:
  void PostWarning(const nsCString& aWarningMsg);

  void PostContextCreationError(const nsCString& aMsg);

  void OnLostContext();

  void OnRestoredContext();

  // Etc
 public:
  already_AddRefed<layers::SharedSurfaceTextureClient> GetVRFrame();

 protected:
  static WebGLContext* MakeWebGLContext(WebGLVersion aVersion);

  const WebGL2Context* GetWebGL2Context() const {
    MOZ_RELEASE_ASSERT(mContext->IsWebGL2(), "Requires WebGL2 context");
    return static_cast<WebGL2Context*>(mContext.get());
  }

  WebGL2Context* GetWebGL2Context() {
    const auto* constThis = this;
    return const_cast<WebGL2Context*>(constThis->GetWebGL2Context());
  }

  mozilla::ipc::Shmem PopShmem() { return mShmemStack.PopLastElement(); }

  RefPtr<WebGLContext> mContext;
  nsTArray<mozilla::ipc::Shmem> mShmemStack;
  ClientWebGLContext* mClientContext;
};

}  // namespace mozilla

#endif  // HOSTWEBGLCONTEXT_H_
