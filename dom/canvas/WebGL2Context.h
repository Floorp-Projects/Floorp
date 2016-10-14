/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL2CONTEXT_H_
#define WEBGL2CONTEXT_H_

#include "WebGLContext.h"

namespace mozilla {

class ErrorResult;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLVertexArrayObject;
namespace dom {
class OwningUnsignedLongOrUint32ArrayOrBoolean;
class OwningWebGLBufferOrLongLong;
} // namespace dom

class WebGL2Context
    : public WebGLContext
{
public:

    virtual ~WebGL2Context();

    static bool IsSupported();
    static WebGL2Context* Create();

    virtual bool IsWebGL2() const override
    {
        return true;
    }

    // -------------------------------------------------------------------------
    // IMPLEMENT nsWrapperCache

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    // -------------------------------------------------------------------------
    // Buffer objects - WebGL2ContextBuffers.cpp

    void CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                           GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);

private:
    template<typename BufferT>
    void GetBufferSubDataT(GLenum target, GLintptr offset, const BufferT& data);

public:
    void GetBufferSubData(GLenum target, GLintptr srcByteOffset,
                          const dom::ArrayBufferView& dstData, GLuint dstElemOffset,
                          GLuint dstElemCountOverride);

    // -------------------------------------------------------------------------
    // Framebuffer objects - WebGL2ContextFramebuffers.cpp

    void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask, GLenum filter);
    void FramebufferTextureLayer(GLenum target, GLenum attachment, WebGLTexture* texture, GLint level, GLint layer);

    virtual JS::Value GetFramebufferAttachmentParameter(JSContext* cx, GLenum target,
                                                        GLenum attachment, GLenum pname,
                                                        ErrorResult& rv) override;

    void InvalidateFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments,
                               ErrorResult& rv);
    void InvalidateSubFramebuffer (GLenum target, const dom::Sequence<GLenum>& attachments, GLint x, GLint y,
                                   GLsizei width, GLsizei height, ErrorResult& rv);
    void ReadBuffer(GLenum mode);


    // -------------------------------------------------------------------------
    // Renderbuffer objects - WebGL2ContextRenderbuffers.cpp

    void GetInternalformatParameter(JSContext*, GLenum target, GLenum internalformat,
                                    GLenum pname, JS::MutableHandleValue retval,
                                    ErrorResult& rv);
    void RenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                        GLsizei width, GLsizei height);


    // -------------------------------------------------------------------------
    // Texture objects - WebGL2ContextTextures.cpp

    void TexStorage2D(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                      GLsizei height);
    void TexStorage3D(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width,
                      GLsizei height, GLsizei depth);

    ////

private:
    void TexImage3D(GLenum target, GLint level, GLenum internalFormat, GLsizei width,
                    GLsizei height, GLsizei depth, GLint border, GLenum unpackFormat,
                    GLenum unpackType, const dom::ArrayBufferView* srcView,
                    GLuint srcElemOffset);

public:
    void TexImage3D(GLenum target, GLint level, GLenum internalFormat, GLsizei width,
                    GLsizei height, GLsizei depth, GLint border, GLenum unpackFormat,
                    GLenum unpackType,
                    const dom::Nullable<dom::ArrayBufferView>& maybeSrc)
    {
        const dom::ArrayBufferView* srcView = nullptr;
        if (!maybeSrc.IsNull()) {
            srcView = &maybeSrc.Value();
        }
        TexImage3D(target, level, internalFormat, width, height, depth, border,
                   unpackFormat, unpackType, srcView, 0);
    }

    void TexImage3D(GLenum target, GLint level, GLenum internalFormat, GLsizei width,
                    GLsizei height, GLsizei depth, GLint border, GLenum unpackFormat,
                    GLenum unpackType, const dom::ArrayBufferView& srcView,
                    GLuint srcElemOffset)
    {
        TexImage3D(target, level, internalFormat, width, height, depth, border,
                   unpackFormat, unpackType, &srcView, srcElemOffset);
    }

    ////

    void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                       GLint zOffset, GLsizei width, GLsizei height, GLsizei depth,
                       GLenum unpackFormat, GLenum unpackType,
                       const dom::ArrayBufferView& srcView, GLuint srcElemOffset,
                       ErrorResult&);
    void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                       GLint zOffset, GLenum unpackFormat, GLenum unpackType,
                       const dom::ImageData& data, ErrorResult&);
    void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                       GLint zOffset, GLenum unpackFormat, GLenum unpackType,
                       const dom::Element& elem, ErrorResult& out_error);

    ////

    void CopyTexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                           GLint zOffset, GLint x, GLint y, GLsizei width,
                           GLsizei height);

    ////

    void CompressedTexImage3D(GLenum target, GLint level, GLenum internalFormat,
                              GLsizei width, GLsizei height, GLsizei depth, GLint border,
                              const dom::ArrayBufferView& srcView, GLuint srcElemOffset);
    void CompressedTexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                                 GLint zOffset, GLsizei width, GLsizei height,
                                 GLsizei depth, GLenum sizedUnpackFormat,
                                 const dom::ArrayBufferView& srcView,
                                 GLuint srcElemOffset);

    ////////////////
    // Texture PBOs

    void TexImage3D(GLenum target, GLint level, GLenum internalFormat, GLsizei width,
                    GLsizei height, GLsizei depth, GLint border, GLenum unpackFormat,
                    GLenum unpackType, WebGLsizeiptr offset);

    void TexSubImage3D(GLenum target, GLint level, GLint xOffset, GLint yOffset,
                       GLint zOffset, GLsizei width, GLsizei height, GLsizei depth,
                       GLenum unpackFormat, GLenum unpackType, WebGLsizeiptr offset,
                       ErrorResult&);

    // -------------------------------------------------------------------------
    // Programs and shaders - WebGL2ContextPrograms.cpp
    GLint GetFragDataLocation(WebGLProgram* program, const nsAString& name);


    // -------------------------------------------------------------------------
    // Uniforms and attributes - WebGL2ContextUniforms.cpp
    void VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);

    ////////////////

    // GL 3.0 & ES 3.0
    void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
    void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);

    void VertexAttribI4iv(GLuint index, const Int32ListU& list) {
        const auto& arr = Int32Arr::From(list);
        if (!ValidateAttribArraySetter("vertexAttribI4iv", 4, arr.elemCount))
            return;

        const auto& itr = arr.elemBytes;
        VertexAttribI4i(index, itr[0], itr[1], itr[2], itr[3]);
    }

    void VertexAttribI4uiv(GLuint index, const Uint32ListU& list) {
        const auto& arr = Uint32Arr::From(list);
        if (!ValidateAttribArraySetter("vertexAttribI4uiv", 4, arr.elemCount))
            return;

        const auto& itr = arr.elemBytes;
        VertexAttribI4ui(index, itr[0], itr[1], itr[2], itr[3]);
    }

    // -------------------------------------------------------------------------
    // Writing to the drawing buffer

    /* Implemented in WebGLContext
    void VertexAttribDivisor(GLuint index, GLuint divisor);
    void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount);
    void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, GLintptr offset, GLsizei instanceCount);
    */
    void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLintptr offset);


    // ------------------------------------------------------------------------
    // Multiple Render Targets - WebGL2ContextMRTs.cpp
    /* Implemented in WebGLContext
    void DrawBuffers(const dom::Sequence<GLenum>& buffers);
    */

private:
    bool ValidateClearBuffer(const char* funcName, GLenum buffer, GLint drawBuffer,
                             size_t availElemCount, GLuint elemOffset);

    void ClearBufferfv(GLenum buffer, GLint drawBuffer, const Float32Arr& src,
                       GLuint srcElemOffset);
    void ClearBufferiv(GLenum buffer, GLint drawBuffer, const Int32Arr& src,
                       GLuint srcElemOffset);
    void ClearBufferuiv(GLenum buffer, GLint drawBuffer, const Uint32Arr& src,
                        GLuint srcElemOffset);

public:
    void ClearBufferfv(GLenum buffer, GLint drawBuffer, const Float32ListU& list,
                       GLuint srcElemOffset)
    {
        ClearBufferfv(buffer, drawBuffer, Float32Arr::From(list), srcElemOffset);
    }
    void ClearBufferiv(GLenum buffer, GLint drawBuffer, const Int32ListU& list,
                       GLuint srcElemOffset)
    {
        ClearBufferiv(buffer, drawBuffer, Int32Arr::From(list), srcElemOffset);
    }
    void ClearBufferuiv(GLenum buffer, GLint drawBuffer, const Uint32ListU& list,
                        GLuint srcElemOffset)
    {
        ClearBufferuiv(buffer, drawBuffer, Uint32Arr::From(list), srcElemOffset);
    }

    void ClearBufferfi(GLenum buffer, GLint drawBuffer, GLfloat depth, GLint stencil);

    // -------------------------------------------------------------------------
    // Query Objects - WebGL2ContextQueries.cpp

    already_AddRefed<WebGLQuery> CreateQuery();
    void DeleteQuery(WebGLQuery* query);
    bool IsQuery(WebGLQuery* query);
    void BeginQuery(GLenum target, WebGLQuery* query);
    void EndQuery(GLenum target);
    already_AddRefed<WebGLQuery> GetQuery(GLenum target, GLenum pname);
    void GetQueryParameter(JSContext*, WebGLQuery* query, GLenum pname, JS::MutableHandleValue retval);


    // -------------------------------------------------------------------------
    // Sampler Objects - WebGL2ContextSamplers.cpp

    already_AddRefed<WebGLSampler> CreateSampler();
    void DeleteSampler(WebGLSampler* sampler);
    bool IsSampler(WebGLSampler* sampler);
    void BindSampler(GLuint unit, WebGLSampler* sampler);
    void SamplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param);
    void SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Int32Array& param);
    void SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLint>& param);
    void SamplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param);
    void SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Float32Array& param);
    void SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLfloat>& param);
    void GetSamplerParameter(JSContext*, WebGLSampler* sampler, GLenum pname, JS::MutableHandleValue retval);


    // -------------------------------------------------------------------------
    // Sync objects - WebGL2ContextSync.cpp

    already_AddRefed<WebGLSync> FenceSync(GLenum condition, GLbitfield flags);
    bool IsSync(WebGLSync* sync);
    void DeleteSync(WebGLSync* sync);
    GLenum ClientWaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout);
    void WaitSync(WebGLSync* sync, GLbitfield flags, GLuint64 timeout);
    void GetSyncParameter(JSContext*, WebGLSync* sync, GLenum pname, JS::MutableHandleValue retval);


    // -------------------------------------------------------------------------
    // Transform Feedback - WebGL2ContextTransformFeedback.cpp

    already_AddRefed<WebGLTransformFeedback> CreateTransformFeedback();
    void DeleteTransformFeedback(WebGLTransformFeedback* tf);
    bool IsTransformFeedback(WebGLTransformFeedback* tf);
    void BindTransformFeedback(GLenum target, WebGLTransformFeedback* tf);
    void BeginTransformFeedback(GLenum primitiveMode);
    void EndTransformFeedback();
    void PauseTransformFeedback();
    void ResumeTransformFeedback();
    void TransformFeedbackVaryings(WebGLProgram* program, const dom::Sequence<nsString>& varyings, GLenum bufferMode);
    already_AddRefed<WebGLActiveInfo> GetTransformFeedbackVarying(WebGLProgram* program, GLuint index);


    // -------------------------------------------------------------------------
    // Uniform Buffer Objects and Transform Feedback Buffers - WebGL2ContextUniforms.cpp
    // TODO(djg): Implemented in WebGLContext
/*
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer, GLintptr offset, GLsizeiptr size);
*/
    virtual JS::Value GetParameter(JSContext* cx, GLenum pname, ErrorResult& rv) override;
    void GetIndexedParameter(GLenum target, GLuint index,
                             dom::Nullable<dom::OwningWebGLBufferOrLongLong>& retval);
    void GetUniformIndices(WebGLProgram* program,
                           const dom::Sequence<nsString>& uniformNames,
                           dom::Nullable< nsTArray<GLuint> >& retval);
    void GetActiveUniforms(JSContext* cx,
                           WebGLProgram* program,
                           const dom::Sequence<GLuint>& uniformIndices,
                           GLenum pname,
                           JS::MutableHandleValue retval);

    GLuint GetUniformBlockIndex(WebGLProgram* program, const nsAString& uniformBlockName);
    void GetActiveUniformBlockParameter(JSContext*, WebGLProgram* program,
                                        GLuint uniformBlockIndex, GLenum pname,
                                        dom::Nullable<dom::OwningUnsignedLongOrUint32ArrayOrBoolean>& retval,
                                        ErrorResult& rv);
    void GetActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex,
                                   nsAString& retval);
    void UniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex,
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
    WebGL2Context();
    virtual UniquePtr<webgl::FormatUsageAuthority>
    CreateFormatUsage(gl::GLContext* gl) const override;

    virtual bool IsTexParamValid(GLenum pname) const override;

    void UpdateBoundQuery(GLenum target, WebGLQuery* query);

    // CreateVertexArrayImpl is assumed to be infallible.
    virtual WebGLVertexArray* CreateVertexArrayImpl() override;
    virtual bool ValidateAttribPointerType(bool integerMode, GLenum type,
                                           uint32_t* alignment,
                                           const char* info) override;
    virtual bool ValidateQueryTarget(GLenum target, const char* info) override;
    virtual bool ValidateUniformMatrixTranspose(bool transpose, const char* info) override;
};

} // namespace mozilla

#endif
