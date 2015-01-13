/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "mozilla/CheckedInt.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

using namespace mozilla;
using namespace dom;

void
WebGLContext::VertexAttrib1f(GLuint index, GLfloat x0)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib1f"))
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib1f(index, x0);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = 0;
        mVertexAttrib0Vector[2] = 0;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES())
            gl->fVertexAttrib1f(index, x0);
    }
}

void
WebGLContext::VertexAttrib2f(GLuint index, GLfloat x0, GLfloat x1)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib2f"))
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib2f(index, x0, x1);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = 0;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES())
            gl->fVertexAttrib2f(index, x0, x1);
    }
}

void
WebGLContext::VertexAttrib3f(GLuint index, GLfloat x0, GLfloat x1, GLfloat x2)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib3f"))
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib3f(index, x0, x1, x2);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = x2;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES())
            gl->fVertexAttrib3f(index, x0, x1, x2);
    }
}

void
WebGLContext::VertexAttrib4f(GLuint index, GLfloat x0, GLfloat x1,
                             GLfloat x2, GLfloat x3)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib4f"))
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib4f(index, x0, x1, x2, x3);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = x2;
        mVertexAttrib0Vector[3] = x3;
        if (gl->IsGLES())
            gl->fVertexAttrib4f(index, x0, x1, x2, x3);
    }
}


void
WebGLContext::VertexAttrib1fv_base(GLuint index, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib1fv", 1, arrayLength))
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib1fv"))
        return;

    MakeContextCurrent();
    if (index) {
        gl->fVertexAttrib1fv(index, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = GLfloat(0);
        mVertexAttrib0Vector[2] = GLfloat(0);
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES())
            gl->fVertexAttrib1fv(index, ptr);
    }
}

void
WebGLContext::VertexAttrib2fv_base(GLuint index, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib2fv", 2, arrayLength))
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib2fv"))
        return;

    MakeContextCurrent();
    if (index) {
        gl->fVertexAttrib2fv(index, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = GLfloat(0);
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES())
            gl->fVertexAttrib2fv(index, ptr);
    }
}

void
WebGLContext::VertexAttrib3fv_base(GLuint index, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib3fv", 3, arrayLength))
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib3fv"))
        return;

    MakeContextCurrent();
    if (index) {
        gl->fVertexAttrib3fv(index, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = ptr[2];
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES())
            gl->fVertexAttrib3fv(index, ptr);
    }
}

void
WebGLContext::VertexAttrib4fv_base(GLuint index, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib4fv", 4, arrayLength))
        return;

    if (!ValidateAttribIndex(index, "vertexAttrib4fv"))
        return;

    MakeContextCurrent();
    if (index) {
        gl->fVertexAttrib4fv(index, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = ptr[2];
        mVertexAttrib0Vector[3] = ptr[3];
        if (gl->IsGLES())
            gl->fVertexAttrib4fv(index, ptr);
    }
}

void
WebGLContext::EnableVertexAttribArray(GLuint index)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "enableVertexAttribArray"))
        return;

    MakeContextCurrent();
    InvalidateBufferFetching();

    gl->fEnableVertexAttribArray(index);

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);
    mBoundVertexArray->mAttribs[index].enabled = true;
}

void
WebGLContext::DisableVertexAttribArray(GLuint index)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "disableVertexAttribArray"))
        return;

    MakeContextCurrent();
    InvalidateBufferFetching();

    if (index || gl->IsGLES())
        gl->fDisableVertexAttribArray(index);

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);
    mBoundVertexArray->mAttribs[index].enabled = false;
}

JS::Value
WebGLContext::GetVertexAttrib(JSContext* cx, GLuint index, GLenum pname,
                              ErrorResult& rv)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateAttribIndex(index, "getVertexAttrib"))
        return JS::NullValue();

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        {
            return WebGLObjectAsJSValue(cx, mBoundVertexArray->mAttribs[index].buf.get(), rv);
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE:
        {
            return JS::Int32Value(mBoundVertexArray->mAttribs[index].stride);
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE:
        {
            if (!mBoundVertexArray->mAttribs[index].enabled)
                return JS::Int32Value(4);

            return JS::Int32Value(mBoundVertexArray->mAttribs[index].size);
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE:
        {
            if (!mBoundVertexArray->mAttribs[index].enabled)
                return JS::NumberValue(uint32_t(LOCAL_GL_FLOAT));

            return JS::NumberValue(uint32_t(mBoundVertexArray->mAttribs[index].type));
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
        {
            if (IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays))
            {
                return JS::Int32Value(mBoundVertexArray->mAttribs[index].divisor);
            }
            break;
        }

        case LOCAL_GL_CURRENT_VERTEX_ATTRIB:
        {
            GLfloat vec[4] = {0, 0, 0, 1};
            if (index) {
                gl->fGetVertexAttribfv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, &vec[0]);
            } else {
                vec[0] = mVertexAttrib0Vector[0];
                vec[1] = mVertexAttrib0Vector[1];
                vec[2] = mVertexAttrib0Vector[2];
                vec[3] = mVertexAttrib0Vector[3];
            }
            JSObject* obj = Float32Array::Create(cx, this, 4, vec);
            if (!obj) {
                rv.Throw(NS_ERROR_OUT_OF_MEMORY);
            }
            return JS::ObjectOrNullValue(obj);
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        {
            return JS::BooleanValue(mBoundVertexArray->mAttribs[index].enabled);
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        {
            return JS::BooleanValue(mBoundVertexArray->mAttribs[index].normalized);
        }

        default:
            break;
    }

    ErrorInvalidEnumInfo("getVertexAttrib: parameter", pname);
    return JS::NullValue();
}

WebGLsizeiptr
WebGLContext::GetVertexAttribOffset(GLuint index, GLenum pname)
{
    if (IsContextLost())
        return 0;

    if (!ValidateAttribIndex(index, "getVertexAttribOffset"))
        return 0;

    if (pname != LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER) {
        ErrorInvalidEnum("getVertexAttribOffset: bad parameter");
        return 0;
    }

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);
    return mBoundVertexArray->mAttribs[index].byteOffset;
}

void
WebGLContext::VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  WebGLboolean normalized, GLsizei stride,
                                  WebGLintptr byteOffset)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttribPointer"))
        return;

    if (!ValidateAttribPointer(false, index, size, type, normalized, stride, byteOffset, "vertexAttribPointer"))
        return;

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);

    InvalidateBufferFetching();

    /* XXX make work with bufferSubData & heterogeneous types
     if (type != mBoundArrayBuffer->GLType())
     return ErrorInvalidOperation("vertexAttribPointer: type must match bound VBO type: %d != %d", type, mBoundArrayBuffer->GLType());
     */

    WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];

    vd.buf = mBoundArrayBuffer;
    vd.stride = stride;
    vd.size = size;
    vd.byteOffset = byteOffset;
    vd.type = type;
    vd.normalized = normalized;
    vd.integer = false;

    MakeContextCurrent();
    gl->fVertexAttribPointer(index, size, type, normalized, stride,
                             reinterpret_cast<void*>(byteOffset));
}

void
WebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttribDivisor"))
        return;

    MOZ_ASSERT(mBoundVertexArray);
    mBoundVertexArray->EnsureAttrib(index);

    WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];
    vd.divisor = divisor;

    InvalidateBufferFetching();

    MakeContextCurrent();

    gl->fVertexAttribDivisor(index, divisor);
}
