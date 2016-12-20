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
WebGLContext::Uniform1ui(WebGLUniformLocation* loc, GLuint v0)
{
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_UNSIGNED_INT, "uniform1ui"))
        return;

    MakeContextCurrent();
    gl->fUniform1ui(loc->mLoc, v0);
}

void
WebGLContext::Uniform2ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1)
{
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_UNSIGNED_INT, "uniform2ui"))
        return;

    MakeContextCurrent();
    gl->fUniform2ui(loc->mLoc, v0, v1);
}

void
WebGLContext::Uniform3ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2)
{
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_UNSIGNED_INT, "uniform3ui"))
        return;

    MakeContextCurrent();
    gl->fUniform3ui(loc->mLoc, v0, v1, v2);
}

void
WebGLContext::Uniform4ui(WebGLUniformLocation* loc, GLuint v0, GLuint v1, GLuint v2,
                         GLuint v3)
{
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_UNSIGNED_INT, "uniform4ui"))
        return;

    MakeContextCurrent();
    gl->fUniform4ui(loc->mLoc, v0, v1, v2, v3);
}

// -------------------------------------------------------------------------
// Uniform Buffer Objects and Transform Feedback Buffers

void
WebGL2Context::GetIndexedParameter(JSContext* cx, GLenum target, GLuint index,
                                   JS::MutableHandleValue retval, ErrorResult& out_error)
{
    const char funcName[] = "getIndexedParameter";
    retval.set(JS::NullValue());
    if (IsContextLost())
        return;

    const std::vector<IndexedBufferBinding>* bindings;
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        bindings = &(mBoundTransformFeedback->mIndexedBindings);
        break;

    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
    case LOCAL_GL_UNIFORM_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
        bindings = &mIndexedUniformBufferBindings;
        break;

    default:
        ErrorInvalidEnumInfo("getIndexedParameter: target", target);
        return;
    }

    if (index >= bindings->size()) {
        ErrorInvalidValue("%s: `index` must be < %s.", funcName,
                          "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");
        return;
    }
    const auto& binding = (*bindings)[index];

    JS::Value ret = JS::NullValue();

    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case LOCAL_GL_UNIFORM_BUFFER_BINDING:
        if (binding.mBufferBinding) {
            ret = WebGLObjectAsJSValue(cx, binding.mBufferBinding.get(), out_error);
        }
        break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case LOCAL_GL_UNIFORM_BUFFER_START:
        ret = JS::NumberValue(binding.mRangeStart);
        break;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case LOCAL_GL_UNIFORM_BUFFER_SIZE:
        ret = JS::NumberValue(binding.mRangeSize);
        break;
    }

    retval.set(ret);
}

void
WebGL2Context::GetUniformIndices(const WebGLProgram& program,
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

    program.GetUniformIndices(uniformNames, retval);
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
WebGL2Context::GetActiveUniforms(JSContext* cx, const WebGLProgram& program,
                                 const dom::Sequence<GLuint>& uniformIndices,
                                 GLenum pname, JS::MutableHandleValue retval)
{
    const char funcName[] = "getActiveUniforms";
    retval.setNull();
    if (IsContextLost())
        return;

    if (!ValidateUniformEnum(this, pname, funcName))
        return;

    if (!ValidateObject("getActiveUniforms: program", program))
        return;

    const auto& numActiveUniforms = program.LinkInfo()->uniforms.size();
    for (const auto& curIndex : uniformIndices) {
        if (curIndex >= numActiveUniforms) {
            ErrorInvalidValue("%s: Too-large active uniform index queried.", funcName);
            return;
        }
    }

    const auto& count = uniformIndices.Length();

    JS::Rooted<JSObject*> array(cx, JS_NewArrayObject(cx, count));
    UniquePtr<GLint[]> samples(new GLint[count]);
    if (!array || !samples) {
        ErrorOutOfMemory("%s: Failed to allocate buffers.", funcName);
        return;
    }
    retval.setObject(*array);

    MakeContextCurrent();
    gl->fGetActiveUniformsiv(program.mGLName, count, uniformIndices.Elements(), pname,
                             samples.get());

    switch (pname) {
    case LOCAL_GL_UNIFORM_TYPE:
    case LOCAL_GL_UNIFORM_SIZE:
    case LOCAL_GL_UNIFORM_BLOCK_INDEX:
    case LOCAL_GL_UNIFORM_OFFSET:
    case LOCAL_GL_UNIFORM_ARRAY_STRIDE:
    case LOCAL_GL_UNIFORM_MATRIX_STRIDE:
        for (size_t i = 0; i < count; ++i) {
            JS::RootedValue value(cx);
            value = JS::Int32Value(samples[i]);
            if (!JS_DefineElement(cx, array, i, value, JSPROP_ENUMERATE))
                return;
        }
        break;
    case LOCAL_GL_UNIFORM_IS_ROW_MAJOR:
        for (size_t i = 0; i < count; ++i) {
            JS::RootedValue value(cx);
            value = JS::BooleanValue(samples[i]);
            if (!JS_DefineElement(cx, array, i, value, JSPROP_ENUMERATE))
                return;
        }
        break;

    default:
        MOZ_CRASH("Invalid pname");
    }
}

GLuint
WebGL2Context::GetUniformBlockIndex(const WebGLProgram& program,
                                    const nsAString& uniformBlockName)
{
    if (IsContextLost())
        return 0;

    if (!ValidateObject("getUniformBlockIndex: program", program))
        return 0;

    return program.GetUniformBlockIndex(uniformBlockName);
}

void
WebGL2Context::GetActiveUniformBlockParameter(JSContext* cx, const WebGLProgram& program,
                                              GLuint uniformBlockIndex, GLenum pname,
                                              JS::MutableHandleValue out_retval,
                                              ErrorResult& out_error)
{
    out_retval.setNull();
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
        out_retval.set(program.GetActiveUniformBlockParam(uniformBlockIndex, pname));
        return;

    case LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        out_retval.set(program.GetActiveUniformBlockActiveUniforms(cx, uniformBlockIndex,
                                                                   &out_error));
        return;
    }

    ErrorInvalidEnumInfo("getActiveUniformBlockParameter: parameter", pname);
}

void
WebGL2Context::GetActiveUniformBlockName(const WebGLProgram& program,
                                         GLuint uniformBlockIndex, nsAString& retval)
{
    retval.SetIsVoid(true);
    if (IsContextLost())
        return;

    if (!ValidateObject("getActiveUniformBlockName: program", program))
        return;

    program.GetActiveUniformBlockName(uniformBlockIndex, retval);
}

void
WebGL2Context::UniformBlockBinding(WebGLProgram& program, GLuint uniformBlockIndex,
                                   GLuint uniformBlockBinding)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("uniformBlockBinding: program", program))
        return;

    program.UniformBlockBinding(uniformBlockIndex, uniformBlockBinding);
}

} // namespace mozilla
