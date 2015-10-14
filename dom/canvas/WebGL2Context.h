/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL2CONTEXT_H_
#define WEBGL2CONTEXT_H_

#include "WebGLContext.h"

/*
 * Minimum value constants define in 6.2 State Tables of OpenGL ES - 3.0.4
 */
#define MINVALUE_GL_MAX_3D_TEXTURE_SIZE             256
#define MINVALUE_GL_MAX_ARRAY_TEXTURE_LAYERS        256

namespace mozilla {

class ErrorResult;
class WebGLSampler;
class WebGLSync;
class WebGLTransformFeedback;
class WebGLVertexArrayObject;
namespace dom {
class OwningUnsignedLongOrUint32ArrayOrBoolean;
class OwningWebGLBufferOrLongLong;
class ArrayBufferViewOrSharedArrayBufferView;
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
    void GetBufferSubData(GLenum target, GLintptr offset,
                          const dom::Nullable<dom::ArrayBuffer>& maybeData);
    void GetBufferSubData(GLenum target, GLintptr offset,
                          const dom::SharedArrayBuffer& data);


    // -------------------------------------------------------------------------
    // Framebuffer objects - WebGL2ContextFramebuffers.cpp

    void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask, GLenum filter);
    void FramebufferTextureLayer(GLenum target, GLenum attachment, WebGLTexture* texture, GLint level, GLint layer);
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

    void TexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
    void TexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                      GLsizei depth);
    void TexImage3D(GLenum target, GLint level, GLenum internalformat,
                    GLsizei width, GLsizei height, GLsizei depth,
                    GLint border, GLenum format, GLenum type,
                    const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& pixels,
                    ErrorResult& rv);
    void TexSubImage3D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLint zoffset,
                       GLsizei width, GLsizei height, GLsizei depth,
                       GLenum format, GLenum type, const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& pixels,
                       ErrorResult& rv);
    void TexSubImage3D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLint zoffset,
                       GLenum format, GLenum type, dom::ImageData* data,
                       ErrorResult& rv);
    template<class ElementType>
    void TexSubImage3D(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLint zoffset,
                       GLenum format, GLenum type, ElementType& elt, ErrorResult& rv)
    {}

    void CopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                           GLint x, GLint y, GLsizei width, GLsizei height);
    void CompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                              GLsizei width, GLsizei height, GLsizei depth,
                              GLint border, GLsizei imageSize, const dom::ArrayBufferViewOrSharedArrayBufferView& data);
    void CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                 GLsizei width, GLsizei height, GLsizei depth,
                                 GLenum format, GLsizei imageSize, const dom::ArrayBufferViewOrSharedArrayBufferView& data);


    // -------------------------------------------------------------------------
    // Programs and shaders - WebGL2ContextPrograms.cpp
    GLint GetFragDataLocation(WebGLProgram* program, const nsAString& name);


    // -------------------------------------------------------------------------
    // Uniforms and attributes - WebGL2ContextUniforms.cpp
    void VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset);

    // GL 3.0 & ES 3.0
    void Uniform1ui(WebGLUniformLocation* location, GLuint v0);
    void Uniform2ui(WebGLUniformLocation* location, GLuint v0, GLuint v1);
    void Uniform3ui(WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2);
    void Uniform4ui(WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);

private:
    void Uniform1uiv_base(WebGLUniformLocation* loc, size_t arrayLength, const GLuint* data);
    void Uniform2uiv_base(WebGLUniformLocation* loc, size_t arrayLength, const GLuint* data);
    void Uniform3uiv_base(WebGLUniformLocation* loc, size_t arrayLength, const GLuint* data);
    void Uniform4uiv_base(WebGLUniformLocation* loc, size_t arrayLength, const GLuint* data);

public:
    void Uniform1uiv(WebGLUniformLocation* loc, const dom::Sequence<GLuint>& arr) {
        Uniform1uiv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform2uiv(WebGLUniformLocation* loc, const dom::Sequence<GLuint>& arr) {
        Uniform2uiv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform3uiv(WebGLUniformLocation* loc, const dom::Sequence<GLuint>& arr) {
        Uniform3uiv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform4uiv(WebGLUniformLocation* loc, const dom::Sequence<GLuint>& arr) {
        Uniform4uiv_base(loc, arr.Length(), arr.Elements());
    }
    void Uniform1uiv(WebGLUniformLocation* loc, const dom::Uint32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform1uiv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform2uiv(WebGLUniformLocation* loc, const dom::Uint32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform2uiv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform3uiv(WebGLUniformLocation* loc, const dom::Uint32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform3uiv_base(loc, arr.Length(), arr.Data());
    }
    void Uniform4uiv(WebGLUniformLocation* loc, const dom::Uint32Array& arr) {
        arr.ComputeLengthAndData();
        Uniform4uiv_base(loc, arr.Length(), arr.Data());
    }

private:
    void UniformMatrix2x3fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);
    void UniformMatrix3x2fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);
    void UniformMatrix2x4fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);
    void UniformMatrix4x2fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);
    void UniformMatrix3x4fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);
    void UniformMatrix4x3fv_base(WebGLUniformLocation* loc, bool transpose,
                                 size_t arrayLength, const GLfloat* data);

public:
    // GL 2.1 & ES 3.0
    void UniformMatrix2x3fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix2x3fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix2x4fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix2x4fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix3x2fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix3x2fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix3x4fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix3x4fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix4x2fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix4x2fv_base(loc, transpose, value.Length(), value.Elements());
    }
    void UniformMatrix4x3fv(WebGLUniformLocation* loc, bool transpose, const dom::Sequence<GLfloat>& value){
        UniformMatrix4x3fv_base(loc, transpose, value.Length(), value.Elements());
    }

    void UniformMatrix2x3fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix2x3fv_base(loc, transpose, value.Length(), value.Data());
    }

    void UniformMatrix2x4fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix2x4fv_base(loc, transpose, value.Length(), value.Data());
    }

    void UniformMatrix3x2fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix3x2fv_base(loc, transpose, value.Length(), value.Data());
    }

    void UniformMatrix3x4fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix3x4fv_base(loc, transpose, value.Length(), value.Data());
    }

    void UniformMatrix4x2fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix4x2fv_base(loc, transpose, value.Length(), value.Data());
    }

    void UniformMatrix4x3fv(WebGLUniformLocation* loc, bool transpose, const dom::Float32Array& value){
        value.ComputeLengthAndData();
        UniformMatrix4x3fv_base(loc, transpose, value.Length(), value.Data());
    }

private:
    void VertexAttribI4iv(GLuint index, size_t length, const GLint* v);
    void VertexAttribI4uiv(GLuint index, size_t length, const GLuint* v);

public:
    // GL 3.0 & ES 3.0
    void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
    void VertexAttribI4iv(GLuint index, const dom::Sequence<GLint>& v);
    void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
    void VertexAttribI4uiv(GLuint index, const dom::Sequence<GLuint>& v);


    // -------------------------------------------------------------------------
    // Writing to the drawing buffer
    // TODO(djg): Implemented in WebGLContext
/*
    void VertexAttribDivisor(GLuint index, GLuint divisor);
    void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount);
    void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, GLintptr offset, GLsizei instanceCount);
*/
    void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLintptr offset);


    // ------------------------------------------------------------------------
    // Multiple Render Targets - WebGL2ContextMRTs.cpp
    // TODO(djg): Implemented in WebGLContext
/*
    void DrawBuffers(const dom::Sequence<GLenum>& buffers);
*/

    void ClearBufferiv_base(GLenum buffer, GLint drawbuffer, const GLint* value);
    void ClearBufferuiv_base(GLenum buffer, GLint drawbuffer, const GLuint* value);
    void ClearBufferfv_base(GLenum buffer, GLint drawbuffer, const GLfloat* value);

    void ClearBufferiv(GLenum buffer, GLint drawbuffer, const dom::Int32Array& value);
    void ClearBufferiv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLint>& value);
    void ClearBufferuiv(GLenum buffer, GLint drawbuffer, const dom::Uint32Array& value);
    void ClearBufferuiv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLuint>& value);
    void ClearBufferfv(GLenum buffer, GLint drawbuffer, const dom::Float32Array& value);
    void ClearBufferfv(GLenum buffer, GLint drawbuffer, const dom::Sequence<GLfloat>& value);
    void ClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

    bool ValidateClearBuffer(const char* info, GLenum buffer, GLint drawbuffer, size_t elemCount);


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
    void GetActiveUniforms(WebGLProgram* program,
                           const dom::Sequence<GLuint>& uniformIndices, GLenum pname,
                           dom::Nullable< nsTArray<GLint> >& retval);
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
    virtual UniquePtr<webgl::FormatUsageAuthority> CreateFormatUsage() const override;

    virtual bool IsTexParamValid(GLenum pname) const override;

    void UpdateBoundQuery(GLenum target, WebGLQuery* query);

    // CreateVertexArrayImpl is assumed to be infallible.
    virtual WebGLVertexArray* CreateVertexArrayImpl() override;
    virtual bool ValidateAttribPointerType(bool integerMode, GLenum type, GLsizei* alignment, const char* info) override;
    virtual bool ValidateBufferTarget(GLenum target, const char* info) override;
    virtual bool ValidateBufferIndexedTarget(GLenum target, const char* info) override;
    virtual bool ValidateBufferUsageEnum(GLenum usage, const char* info) override;
    virtual bool ValidateQueryTarget(GLenum target, const char* info) override;
    virtual bool ValidateUniformMatrixTranspose(bool transpose, const char* info) override;
};

} // namespace mozilla

#endif
