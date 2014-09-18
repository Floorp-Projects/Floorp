/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Uniforms and attributes

void
WebGL2Context::VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset)
{
    MOZ_CRASH("Not Implemented.");
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
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::VertexAttribI4iv(GLuint index, const dom::Sequence<GLint>& v)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::VertexAttribI4uiv(GLuint index, const dom::Sequence<GLuint>& v)
{
    MOZ_CRASH("Not Implemented.");
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
