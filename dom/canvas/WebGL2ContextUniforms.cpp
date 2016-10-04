/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/RefPtr.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

bool
WebGL2Context::ValidateUniformMatrixTranspose(bool /*transpose*/, const char* /*info*/)
{
    return true;
}

// -------------------------------------------------------------------------
// Uniforms

void
WebGL2Context::Uniform1ui(WebGLUniformLocation* loc, GLuint v0)
{
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_UNSIGNED_INT, "uniform1ui"))
        return;

    MakeContextCurrent();
    gl->fUniform1ui(loc->mLoc, v0);
}

void
WebGL2Context::Uniform2ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1)
{
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_UNSIGNED_INT, "uniform2ui"))
        return;

    MakeContextCurrent();
    gl->fUniform2ui(loc->mLoc, v0, v1);
}

void
WebGL2Context::Uniform3ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2)
{
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_UNSIGNED_INT, "uniform3ui"))
        return;

    MakeContextCurrent();
    gl->fUniform3ui(loc->mLoc, v0, v1, v2);
}

void
WebGL2Context::Uniform4ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2,
                          GLuint v3)
{
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_UNSIGNED_INT, "uniform4ui"))
        return;

    MakeContextCurrent();
    gl->fUniform4ui(loc->mLoc, v0, v1, v2, v3);
}


// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers
// TODO(djg): Implemented in WebGLContext
/*
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                         GLintptr offset, GLsizeiptr size);
*/

/* This doesn't belong here. It's part of state querying */
void
WebGL2Context::GetIndexedParameter(GLenum target, GLuint index,
                                   dom::Nullable<dom::OwningWebGLBufferOrLongLong>& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    GLint64 data = 0;

    MakeContextCurrent();

    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index >= mGLMaxTransformFeedbackSeparateAttribs)
            return ErrorInvalidValue("getIndexedParameter: index should be less than "
                                     "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");

        if (mBoundTransformFeedbackBuffers[index].get()) {
            retval.SetValue().SetAsWebGLBuffer() =
                mBoundTransformFeedbackBuffers[index].get();
        }
        return;

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
        if (index >= mGLMaxUniformBufferBindings)
            return ErrorInvalidValue("getIndexedParameter: index should be than "
                                     "MAX_UNIFORM_BUFFER_BINDINGS");

        if (mBoundUniformBuffers[index].get())
            retval.SetValue().SetAsWebGLBuffer() = mBoundUniformBuffers[index].get();
        return;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case LOCAL_GL_UNIFORM_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
        gl->fGetInteger64i_v(target, index, &data);
        retval.SetValue().SetAsLongLong() = data;
        return;
    }

    ErrorInvalidEnumInfo("getIndexedParameter: target", target);
}

void
WebGL2Context::GetUniformIndices(WebGLProgram* program,
                                 const dom::Sequence<nsString>& uniformNames,
                                 dom::Nullable< nsTArray<GLuint> >& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("getUniformIndices: program", program))
        return;

    if (!uniformNames.Length())
        return;

    program->GetUniformIndices(uniformNames, retval);
}

static bool
ValidateUniformEnum(WebGLContext* webgl, GLenum pname, const char* info)
{
    switch (pname) {
    case LOCAL_GL_UNIFORM_TYPE:
    case LOCAL_GL_UNIFORM_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_INDEX:
    case LOCAL_GL_UNIFORM_OFFSET:
    case LOCAL_GL_UNIFORM_ARRAY_STRIDE:
    case LOCAL_GL_UNIFORM_MATRIX_STRIDE:
    case LOCAL_GL_UNIFORM_IS_ROW_MAJOR:
        return true;

    default:
        webgl->ErrorInvalidEnum("%s: invalid pname: %s", info, webgl->EnumName(pname));
        return false;
    }
}

void
WebGL2Context::GetActiveUniforms(JSContext* cx,
                                 WebGLProgram* program,
                                 const dom::Sequence<GLuint>& uniformIndices,
                                 GLenum pname,
                                 JS::MutableHandleValue retval)
{
    retval.set(JS::NullValue());
    if (IsContextLost())
        return;

    if (!ValidateUniformEnum(this, pname, "getActiveUniforms"))
        return;

    if (!ValidateObject("getActiveUniforms: program", program))
        return;

    size_t count = uniformIndices.Length();
    if (!count)
        return;

    GLuint progname = program->mGLName;
    Vector<GLint> samples;
    if (!samples.resize(count)) {
        return;
    }

    MakeContextCurrent();
    gl->fGetActiveUniformsiv(progname, count, uniformIndices.Elements(), pname,
                             samples.begin());

    JS::Rooted<JSObject*> array(cx, JS_NewArrayObject(cx, count));
    if (!array) {
        return;
    }

    switch (pname) {
    case LOCAL_GL_UNIFORM_TYPE:
    case LOCAL_GL_UNIFORM_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_INDEX:
    case LOCAL_GL_UNIFORM_OFFSET:
    case LOCAL_GL_UNIFORM_ARRAY_STRIDE:
    case LOCAL_GL_UNIFORM_MATRIX_STRIDE:
        for (uint32_t i = 0; i < count; ++i) {
            JS::RootedValue value(cx);
            value = JS::Int32Value(samples[i]);
            if (!JS_DefineElement(cx, array, i, value, JSPROP_ENUMERATE)) {
                return;
            }
        }
        break;
    case LOCAL_GL_UNIFORM_IS_ROW_MAJOR:
        for (uint32_t i = 0; i < count; ++i) {
            JS::RootedValue value(cx);
            value = JS::BooleanValue(samples[i]);
            if (!JS_DefineElement(cx, array, i, value, JSPROP_ENUMERATE)) {
                return;
            }
        }
        break;

    default:
        return;
    }

    retval.setObjectOrNull(array);
}

GLuint
WebGL2Context::GetUniformBlockIndex(WebGLProgram* program,
                                    const nsAString& uniformBlockName)
{
    if (IsContextLost())
        return 0;

    if (!ValidateObject("getUniformBlockIndex: program", program))
        return 0;

    return program->GetUniformBlockIndex(uniformBlockName);
}

void
WebGL2Context::GetActiveUniformBlockParameter(JSContext* cx, WebGLProgram* program,
                                              GLuint uniformBlockIndex, GLenum pname,
                                              dom::Nullable<dom::OwningUnsignedLongOrUint32ArrayOrBoolean>& retval,
                                              ErrorResult& rv)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniformBlockParameter: program", program))
        return;

    MakeContextCurrent();

    switch(pname) {
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
    case LOCAL_GL_UNIFORM_BLOCK_BINDING:
    case LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        program->GetActiveUniformBlockParam(uniformBlockIndex, pname, retval);
        return;

    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        program->GetActiveUniformBlockActiveUniforms(cx, uniformBlockIndex, retval, rv);
        return;
    }

    ErrorInvalidEnumInfo("getActiveUniformBlockParameter: parameter", pname);
}

void
WebGL2Context::GetActiveUniformBlockName(WebGLProgram* program, GLuint uniformBlockIndex,
                                         nsAString& retval)
{
    retval.SetIsVoid(true);
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniformBlockName: program", program))
        return;

    program->GetActiveUniformBlockName(uniformBlockIndex, retval);
}

void
WebGL2Context::UniformBlockBinding(WebGLProgram* program, GLuint uniformBlockIndex,
                                   GLuint uniformBlockBinding)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("uniformBlockBinding: program", program))
        return;

    program->UniformBlockBinding(uniformBlockIndex, uniformBlockBinding);
}

} // namespace mozilla
