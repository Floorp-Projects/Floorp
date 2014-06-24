/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

using namespace mozilla;
using namespace mozilla::dom;

typedef union { GLint i; GLfloat f; GLuint u; } fi_t;

static inline
GLfloat PuntToFloat(GLint i)
{
   fi_t tmp;
   tmp.i = i;
   return tmp.f;
}

static inline
GLfloat PuntToFloat(GLuint u)
{
   fi_t tmp;
   tmp.u = u;
   return tmp.f;
}

bool
WebGL2Context::ValidateAttribPointerType(bool integerMode, GLenum type, GLsizei* alignment, const char* info)
{
    MOZ_ASSERT(alignment);

    switch (type) {
    case LOCAL_GL_BYTE:
    case LOCAL_GL_UNSIGNED_BYTE:
        *alignment = 1;
        return true;

    case LOCAL_GL_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT:
        *alignment = 2;
        return true;

    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
        *alignment = 4;
        return true;
    }

    if (!integerMode) {
        switch (type) {
        case LOCAL_GL_HALF_FLOAT:
            *alignment = 2;
            return true;

        case LOCAL_GL_FLOAT:
        case LOCAL_GL_FIXED:
        case LOCAL_GL_INT_2_10_10_10_REV:
        case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
            *alignment = 4;
            return true;
        }
    }

    ErrorInvalidEnum("%s: invalid enum value 0x%x", info, type);
    return false;
}

// -------------------------------------------------------------------------
// Uniforms and attributes

void
WebGL2Context::VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttribIPointer"))
        return;

    if (!ValidateAttribPointer(true, index, size, type, LOCAL_GL_FALSE, stride, offset, "vertexAttribIPointer"))
        return;

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);

    InvalidateBufferFetching();

    WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];

    vd.buf = mBoundArrayBuffer;
    vd.stride = stride;
    vd.size = size;
    vd.byteOffset = offset;
    vd.type = type;
    vd.normalized = false;
    vd.integer = true;

    MakeContextCurrent();
    gl->fVertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(offset));
}

void
WebGL2Context::Uniform1ui(WebGLUniformLocation* location, GLuint v0)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform2ui(WebGLUniformLocation* location, GLuint v0, GLuint v1)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform3ui(WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform4ui(WebGLUniformLocation* location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform1uiv(WebGLUniformLocation* location, const dom::Sequence<GLuint>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform2uiv(WebGLUniformLocation* location, const dom::Sequence<GLuint>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform3uiv(WebGLUniformLocation* location, const dom::Sequence<GLuint>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::Uniform4uiv(WebGLUniformLocation* location, const dom::Sequence<GLuint>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix2x3fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix2x3fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix3x2fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix3x2fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix2x4fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix2x4fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix4x2fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix4x2fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix3x4fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix3x4fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix4x3fv(WebGLUniformLocation* location, bool transpose, const dom::Float32Array& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformMatrix4x3fv(WebGLUniformLocation* location, bool transpose, const dom::Sequence<GLfloat>& value)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    if (IsContextLost())
        return;

    if (index || gl->IsGLES()) {
        MakeContextCurrent();
        gl->fVertexAttribI4i(index, x, y, z, w);
    } else {
        mVertexAttrib0Vector[0] = PuntToFloat(x);
        mVertexAttrib0Vector[1] = PuntToFloat(y);
        mVertexAttrib0Vector[2] = PuntToFloat(z);
        mVertexAttrib0Vector[3] = PuntToFloat(w);
    }
}

void
WebGL2Context::VertexAttribI4iv(GLuint index, size_t length, const GLint* v)
{
    if (!ValidateAttribArraySetter("vertexAttribI4iv", 4, length))
        return;

    if (index || gl->IsGLES()) {
        MakeContextCurrent();
        gl->fVertexAttribI4iv(index, v);
    } else {
        mVertexAttrib0Vector[0] = PuntToFloat(v[0]);
        mVertexAttrib0Vector[1] = PuntToFloat(v[1]);
        mVertexAttrib0Vector[2] = PuntToFloat(v[2]);
        mVertexAttrib0Vector[3] = PuntToFloat(v[3]);
    }
}

void
WebGL2Context::VertexAttribI4iv(GLuint index, const dom::Sequence<GLint>& v)
{
    VertexAttribI4iv(index, v.Length(), v.Elements());
}

void
WebGL2Context::VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    if (IsContextLost())
        return;

    if (index || gl->IsGLES()) {
        MakeContextCurrent();
        gl->fVertexAttribI4ui(index, x, y, z, w);
    } else {
        mVertexAttrib0Vector[0] = PuntToFloat(x);
        mVertexAttrib0Vector[1] = PuntToFloat(y);
        mVertexAttrib0Vector[2] = PuntToFloat(z);
        mVertexAttrib0Vector[3] = PuntToFloat(w);
    }
}

void
WebGL2Context::VertexAttribI4uiv(GLuint index, size_t length, const GLuint* v)
{
    if (IsContextLost())
        return;

    if (index || gl->IsGLES()) {
        MakeContextCurrent();
        gl->fVertexAttribI4uiv(index, v);
    } else {
        mVertexAttrib0Vector[0] = PuntToFloat(v[0]);
        mVertexAttrib0Vector[1] = PuntToFloat(v[1]);
        mVertexAttrib0Vector[2] = PuntToFloat(v[2]);
        mVertexAttrib0Vector[3] = PuntToFloat(v[3]);
    }
}

void
WebGL2Context::VertexAttribI4uiv(GLuint index, const dom::Sequence<GLuint>& v)
{
    VertexAttribI4uiv(index, v.Length(), v.Elements());
}

// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers
// TODO(djg): Implemented in WebGLContext
/*
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer, GLintptr offset, GLsizeiptr size);
*/

void
WebGL2Context::GetIndexedParameter(JSContext*, GLenum target, GLuint index, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetUniformIndices(WebGLProgram* program,
                                 const dom::Sequence<nsString>& uniformNames,
                                 dom::Nullable< nsTArray<GLuint> >& retval)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetActiveUniforms(WebGLProgram* program,
                                 const dom::Sequence<GLuint>& uniformIndices,
                                 GLenum pname,
                                 dom::Nullable< nsTArray<GLint> >& retval)
{
    MOZ_CRASH("Not Implemented.");
}

GLuint
WebGL2Context::GetUniformBlockIndex(WebGLProgram* program, const nsAString& uniformBlockName)
{
    MOZ_CRASH("Not Implemented.");
    return 0;
}

void
WebGL2Context::GetActiveUniformBlockParameter(JSContext*, WebGLProgram* program,
                                              GLuint uniformBlockIndex, GLenum pname,
                                              JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex, dom::DOMString& retval)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::UniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    MOZ_CRASH("Not Implemented.");
}
