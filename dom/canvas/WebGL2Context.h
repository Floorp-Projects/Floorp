/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL2CONTEXT_H_
#define WEBGL2CONTEXT_H_

#include "WebGLContext.h"

namespace mozilla {

class ErrorResult;
class HostWebGLContext;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLVertexArrayObject;
namespace dom {
class OwningUnsignedLongOrUint32ArrayOrBoolean;
class OwningWebGLBufferOrLongLong;
}  // namespace dom

class WebGL2Context : public WebGLContext {
 public:
  virtual ~WebGL2Context(){};

  static bool IsSupported();
  static WebGL2Context* Create() { return new WebGL2Context(); }

  virtual bool IsWebGL2() const override { return true; }

  // -------------------------------------------------------------------------
  // Buffer objects - WebGL2ContextBuffers.cpp

  void CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         WebGLintptr readOffset, WebGLintptr writeOffset,
                         WebGLsizeiptr size);

 private:
  template <typename BufferT>
  void GetBufferSubDataT(GLenum target, WebGLintptr offset,
                         const BufferT& data);

 public:
  Maybe<UniquePtr<RawBuffer<>>> GetBufferSubData(GLenum target,
                                                 WebGLintptr srcByteOffset,
                                                 size_t byteLen);

  // -------------------------------------------------------------------------
  // Framebuffer objects - WebGL2ContextFramebuffers.cpp

  void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter);

  virtual MaybeWebGLVariant GetFramebufferAttachmentParameter(
      GLenum target, GLenum attachment, GLenum pname) override;

  // Make the inline version from the superclass visible here.
  using WebGLContext::GetFramebufferAttachmentParameter;

  void InvalidateFramebuffer(GLenum target,
                             const nsTArray<GLenum>& attachments);
  void InvalidateSubFramebuffer(GLenum target,
                                const nsTArray<GLenum>& attachments, GLint x,
                                GLint y, GLsizei width, GLsizei height);
  void ReadBuffer(GLenum mode);

  // -------------------------------------------------------------------------
  // Renderbuffer objects - WebGL2ContextRenderbuffers.cpp

  Maybe<nsTArray<int32_t>> GetInternalformatParameter(GLenum target,
                                                      GLenum internalformat,
                                                      GLenum pname);

  // -------------------------------------------------------------------------
  // Texture objects - WebGL2ContextTextures.cpp

  void TexStorage(uint8_t funcDims, GLenum target, GLsizei levels,
                  GLenum internalFormat, GLsizei width, GLsizei height,
                  GLsizei depth);

  GLint GetFragDataLocation(const WebGLProgram& prog, const nsAString& name);

  // GL 3.0 & ES 3.0
  void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
  void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);

  // -------------------------------------------------------------------------
  // Writing to the drawing buffer

  /* Implemented in WebGLContext
  void VertexAttribDivisor(GLuint index, GLuint divisor);
  void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instanceCount);
  void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                             WebGLintptr offset, GLsizei instanceCount);
  */

  void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                         GLenum type, WebGLintptr byteOffset) {
    const FuncScope funcScope(*this, "drawRangeElements");
    if (IsContextLost()) return;

    if (end < start) {
      ErrorInvalidValue("end must be >= start.");
      return;
    }

    DrawElementsInstanced(mode, count, type, byteOffset, 1);
  }

  // ------------------------------------------------------------------------
  // Multiple Render Targets - WebGL2ContextMRTs.cpp
  /* Implemented in WebGLContext
  void DrawBuffers(const nsTArray<GLenum>& buffers);
  */

 private:
  bool ValidateClearBuffer(GLenum buffer, GLint drawBuffer,
                           size_t availElemCount, GLuint elemOffset,
                           GLenum funcType);

 public:
  void ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth,
                     GLint stencil);
  void ClearBufferfv(GLenum buffer, GLint drawBuffer,
                     const RawBuffer<const float>& src, GLuint srcElemOffset);
  void ClearBufferiv(GLenum buffer, GLint drawBuffer,
                     const RawBuffer<const int32_t>& src, GLuint srcElemOffset);
  void ClearBufferuiv(GLenum buffer, GLint drawBuffer,
                      const RawBuffer<const uint32_t>& src,
                      GLuint srcElemOffset);

  // -------------------------------------------------------------------------
  // Sampler Objects - WebGL2ContextSamplers.cpp

  already_AddRefed<WebGLSampler> CreateSampler();
  void DeleteSampler(WebGLSampler* sampler);
  bool IsSampler(const WebGLSampler* sampler);
  void BindSampler(GLuint unit, WebGLSampler* sampler);
  void SamplerParameteri(WebGLSampler& sampler, GLenum pname, GLint param);
  void SamplerParameterf(WebGLSampler& sampler, GLenum pname, GLfloat param);
  MaybeWebGLVariant GetSamplerParameter(const WebGLSampler& sampler,
                                        GLenum pname);

  // -------------------------------------------------------------------------
  // Sync objects - WebGL2ContextSync.cpp

  const GLuint64 kMaxClientWaitSyncTimeoutNS =
      1000 * 1000 * 1000;  // 1000ms in ns.

  already_AddRefed<WebGLSync> FenceSync(GLenum condition, GLbitfield flags);
  bool IsSync(const WebGLSync* sync);
  void DeleteSync(WebGLSync* sync);
  GLenum ClientWaitSync(const WebGLSync& sync, GLbitfield flags,
                        GLuint64 timeout);
  void WaitSync(const WebGLSync& sync, GLbitfield flags, GLint64 timeout);
  MaybeWebGLVariant GetSyncParameter(const WebGLSync& sync, GLenum pname);

  // -------------------------------------------------------------------------
  // Transform Feedback - WebGL2ContextTransformFeedback.cpp

  already_AddRefed<WebGLTransformFeedback> CreateTransformFeedback();
  void DeleteTransformFeedback(WebGLTransformFeedback* tf);
  bool IsTransformFeedback(const WebGLTransformFeedback* tf);
  void BindTransformFeedback(GLenum target, WebGLTransformFeedback* tf);
  void BeginTransformFeedback(GLenum primitiveMode);
  void EndTransformFeedback();
  void PauseTransformFeedback();
  void ResumeTransformFeedback();
  void TransformFeedbackVaryings(WebGLProgram& program,
                                 const nsTArray<nsString>& varyings,
                                 GLenum bufferMode);
  Maybe<WebGLActiveInfo> GetTransformFeedbackVarying(
      const WebGLProgram& program, GLuint index);

  // -------------------------------------------------------------------------
  // Uniform Buffer Objects and Transform Feedback Buffers -
  // WebGL2ContextUniforms.cpp
  // TODO(djg): Implemented in WebGLContext
  /*
      void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
      void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                           WebGLintptr offset, WebGLsizeiptr size);
  */
  MaybeWebGLVariant GetParameter(GLenum pname) override;

  // Make the inline version from the superclass visible here.
  using WebGLContext::GetParameter;
  MaybeWebGLVariant GetIndexedParameter(GLenum target, GLuint index);
  MaybeWebGLVariant GetUniformIndices(const WebGLProgram& program,
                                      const nsTArray<nsString>& uniformNames);
  MaybeWebGLVariant GetActiveUniforms(const WebGLProgram& program,
                                      const nsTArray<GLuint>& uniformIndices,
                                      GLenum pname);

  GLuint GetUniformBlockIndex(const WebGLProgram& program,
                              const nsAString& uniformBlockName);
  MaybeWebGLVariant GetActiveUniformBlockParameter(const WebGLProgram& program,
                                                   GLuint uniformBlockIndex,
                                                   GLenum pname);
  nsString GetActiveUniformBlockName(const WebGLProgram& program,
                                     GLuint uniformBlockIndex);
  void UniformBlockBinding(WebGLProgram& program, GLuint uniformBlockIndex,
                           GLuint uniformBlockBinding);

  // -------------------------------------------------------------------------
  // Vertex Array Object - WebGL2ContextVAOs.cpp
  // TODO(djg): Implemented in WebGLContext
  /*
      already_AddRefed<WebGLVertexArrayObject> CreateVertexArray();
      void DeleteVertexArray(WebGLVertexArrayObject* vertexArray);
      bool IsVertexArray(WebGLVertexArrayObject* vertexArray);
      void BindVertexArray(WebGLVertexArrayObject* vertexArray);
  */

 private:
  WebGL2Context() {
    MOZ_ASSERT(IsSupported(),
               "not supposed to create a WebGL2Context"
               "context when not supported");
  }

  virtual UniquePtr<webgl::FormatUsageAuthority> CreateFormatUsage(
      gl::GLContext* gl) const override;

  virtual bool IsTexParamValid(GLenum pname) const override;

  void UpdateBoundQuery(GLenum target, WebGLQuery* query);

  // CreateVertexArrayImpl is assumed to be infallible.
  virtual WebGLVertexArray* CreateVertexArrayImpl() override;
};

}  // namespace mozilla

#endif
