/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLVertexArray.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLUniformInfo.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "GLContext.h"

using namespace mozilla;
using namespace dom;

// For a Tegra workaround.
static const int MAX_DRAW_CALLS_SINCE_FLUSH = 100;

void
WebGLContext::VertexAttrib1f(GLuint index, GLfloat x0)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib1f(index, x0);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = 0;
        mVertexAttrib0Vector[2] = 0;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES2())
            gl->fVertexAttrib1f(index, x0);
    }
}

void
WebGLContext::VertexAttrib2f(GLuint index, GLfloat x0, GLfloat x1)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib2f(index, x0, x1);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = 0;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES2())
            gl->fVertexAttrib2f(index, x0, x1);
    }
}

void
WebGLContext::VertexAttrib3f(GLuint index, GLfloat x0, GLfloat x1, GLfloat x2)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib3f(index, x0, x1, x2);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = x2;
        mVertexAttrib0Vector[3] = 1;
        if (gl->IsGLES2())
            gl->fVertexAttrib3f(index, x0, x1, x2);
    }
}

void
WebGLContext::VertexAttrib4f(GLuint index, GLfloat x0, GLfloat x1,
                             GLfloat x2, GLfloat x3)
{
    if (IsContextLost())
        return;

    MakeContextCurrent();

    if (index) {
        gl->fVertexAttrib4f(index, x0, x1, x2, x3);
    } else {
        mVertexAttrib0Vector[0] = x0;
        mVertexAttrib0Vector[1] = x1;
        mVertexAttrib0Vector[2] = x2;
        mVertexAttrib0Vector[3] = x3;
        if (gl->IsGLES2())
            gl->fVertexAttrib4f(index, x0, x1, x2, x3);
    }
}


void
WebGLContext::VertexAttrib1fv_base(GLuint idx, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib1fv", 1, arrayLength))
        return;

    MakeContextCurrent();
    if (idx) {
        gl->fVertexAttrib1fv(idx, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = GLfloat(0);
        mVertexAttrib0Vector[2] = GLfloat(0);
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES2())
            gl->fVertexAttrib1fv(idx, ptr);
    }
}

void
WebGLContext::VertexAttrib2fv_base(GLuint idx, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib2fv", 2, arrayLength))
        return;

    MakeContextCurrent();
    if (idx) {
        gl->fVertexAttrib2fv(idx, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = GLfloat(0);
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES2())
            gl->fVertexAttrib2fv(idx, ptr);
    }
}

void
WebGLContext::VertexAttrib3fv_base(GLuint idx, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib3fv", 3, arrayLength))
        return;

    MakeContextCurrent();
    if (idx) {
        gl->fVertexAttrib3fv(idx, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = ptr[2];
        mVertexAttrib0Vector[3] = GLfloat(1);
        if (gl->IsGLES2())
            gl->fVertexAttrib3fv(idx, ptr);
    }
}

void
WebGLContext::VertexAttrib4fv_base(GLuint idx, uint32_t arrayLength,
                                   const GLfloat* ptr)
{
    if (!ValidateAttribArraySetter("VertexAttrib4fv", 4, arrayLength))
        return;

    MakeContextCurrent();
    if (idx) {
        gl->fVertexAttrib4fv(idx, ptr);
    } else {
        mVertexAttrib0Vector[0] = ptr[0];
        mVertexAttrib0Vector[1] = ptr[1];
        mVertexAttrib0Vector[2] = ptr[2];
        mVertexAttrib0Vector[3] = ptr[3];
        if (gl->IsGLES2())
            gl->fVertexAttrib4fv(idx, ptr);
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
    MOZ_ASSERT(mBoundVertexArray->HasAttrib(index)); // should have been validated earlier
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

    if (index || gl->IsGLES2())
        gl->fDisableVertexAttribArray(index);

    MOZ_ASSERT(mBoundVertexArray->HasAttrib(index)); // should have been validated earlier
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
            if (!ValidateAttribIndex(index, "getVertexAttrib"))
                return JS::NullValue();

            if (!mBoundVertexArray->mAttribs[index].enabled)
                return JS::Int32Value(4);

            // Don't break; fall through.
        }
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE:
        {
            GLint i = 0;
            gl->fGetVertexAttribiv(index, pname, &i);
            if (pname == LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE)
                return JS::Int32Value(i);
            MOZ_ASSERT(pname == LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE);
            return JS::NumberValue(uint32_t(i));
        }

        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
        {
            if (IsExtensionEnabled(ANGLE_instanced_arrays))
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

    return mBoundVertexArray->mAttribs[index].byteOffset;
}

void
WebGLContext::VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                  WebGLboolean normalized, GLsizei stride,
                                  WebGLintptr byteOffset)
{
    if (IsContextLost())
        return;

    if (mBoundArrayBuffer == nullptr)
        return ErrorInvalidOperation("vertexAttribPointer: must have valid GL_ARRAY_BUFFER binding");

    GLsizei requiredAlignment = 1;
    switch (type) {
        case LOCAL_GL_BYTE:
        case LOCAL_GL_UNSIGNED_BYTE:
            requiredAlignment = 1;
            break;
        case LOCAL_GL_SHORT:
        case LOCAL_GL_UNSIGNED_SHORT:
            requiredAlignment = 2;
            break;
            // XXX case LOCAL_GL_FIXED:
        case LOCAL_GL_FLOAT:
            requiredAlignment = 4;
            break;
        default:
            return ErrorInvalidEnumInfo("vertexAttribPointer: type", type);
    }

    // requiredAlignment should always be a power of two.
    GLsizei requiredAlignmentMask = requiredAlignment - 1;

    if (!ValidateAttribIndex(index, "vertexAttribPointer")) {
        return;
    }

    if (size < 1 || size > 4)
        return ErrorInvalidValue("vertexAttribPointer: invalid element size");

    if (stride < 0 || stride > 255) // see WebGL spec section 6.6 "Vertex Attribute Data Stride"
        return ErrorInvalidValue("vertexAttribPointer: negative or too large stride");

    if (byteOffset < 0)
        return ErrorInvalidValue("vertexAttribPointer: negative offset");

    if (stride & requiredAlignmentMask) {
        return ErrorInvalidOperation("vertexAttribPointer: stride doesn't satisfy the alignment "
                                     "requirement of given type");
    }

    if (byteOffset & requiredAlignmentMask) {
        return ErrorInvalidOperation("vertexAttribPointer: byteOffset doesn't satisfy the alignment "
                                     "requirement of given type");

    }

    InvalidateBufferFetching();

    /* XXX make work with bufferSubData & heterogeneous types
     if (type != mBoundArrayBuffer->GLType())
     return ErrorInvalidOperation("vertexAttribPointer: type must match bound VBO type: %d != %d", type, mBoundArrayBuffer->GLType());
     */

    WebGLVertexAttribData &vd = mBoundVertexArray->mAttribs[index];

    vd.buf = mBoundArrayBuffer;
    vd.stride = stride;
    vd.size = size;
    vd.byteOffset = byteOffset;
    vd.type = type;
    vd.normalized = normalized;

    MakeContextCurrent();

    gl->fVertexAttribPointer(index, size, type, normalized,
                             stride,
                             reinterpret_cast<void*>(byteOffset));
}

void
WebGLContext::VertexAttribDivisor(GLuint index, GLuint divisor)
{
    if (IsContextLost())
        return;

    if (!ValidateAttribIndex(index, "vertexAttribDivisor")) {
        return;
    }

    WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];
    vd.divisor = divisor;

    InvalidateBufferFetching();

    MakeContextCurrent();

    gl->fVertexAttribDivisor(index, divisor);
}

bool
WebGLContext::DrawInstanced_check(const char* info)
{
    // This restriction was removed in GLES3, so WebGL2 shouldn't have it.
    if (!IsWebGL2() &&
        IsExtensionEnabled(ANGLE_instanced_arrays) &&
        !mBufferFetchingHasPerVertex)
    {
        /* http://www.khronos.org/registry/gles/extensions/ANGLE/ANGLE_instanced_arrays.txt
         *  If all of the enabled vertex attribute arrays that are bound to active
         *  generic attributes in the program have a non-zero divisor, the draw
         *  call should return INVALID_OPERATION.
         *
         * NB: This also appears to apply to NV_instanced_arrays, though the
         * INVALID_OPERATION emission is not explicitly stated.
         * ARB_instanced_arrays does not have this restriction.
         */
        ErrorInvalidOperation("%s: at least one vertex attribute divisor should be 0", info);
        return false;
    }

    return true;
}

bool WebGLContext::DrawArrays_check(GLint first, GLsizei count, GLsizei primcount, const char* info)
{
    if (first < 0 || count < 0) {
        ErrorInvalidValue("%s: negative first or count", info);
        return false;
    }

    if (primcount < 0) {
        ErrorInvalidValue("%s: negative primcount", info);
        return false;
    }

    if (!ValidateStencilParamsForDrawCall()) {
        return false;
    }

    // If count is 0, there's nothing to do.
    if (count == 0 || primcount == 0) {
        return false;
    }

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        return false;
    }

    if (!ValidateBufferFetching(info)) {
        return false;
    }

    CheckedInt<GLsizei> checked_firstPlusCount = CheckedInt<GLsizei>(first) + count;

    if (!checked_firstPlusCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in first+count", info);
        return false;
    }

    if (uint32_t(checked_firstPlusCount.value()) > mMaxFetchedVertices) {
        ErrorInvalidOperation("%s: bound vertex attribute buffers do not have sufficient size for given first and count", info);
        return false;
    }

    if (uint32_t(primcount) > mMaxFetchedInstances) {
        ErrorInvalidOperation("%s: bound instance attribute buffers do not have sufficient size for given primcount", info);
        return false;
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    }

    if (!DoFakeVertexAttrib0(checked_firstPlusCount.value())) {
        return false;
    }
    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArrays: mode"))
        return;

    if (!DrawArrays_check(first, count, 1, "drawArrays"))
        return;

    SetupContextLossTimer();
    gl->fDrawArrays(mode, first, count);

    Draw_cleanup();
}

void
WebGLContext::DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArraysInstanced: mode"))
        return;

    if (!DrawArrays_check(first, count, primcount, "drawArraysInstanced"))
        return;

    if (!DrawInstanced_check("drawArraysInstanced"))
        return;

    SetupContextLossTimer();
    gl->fDrawArraysInstanced(mode, first, count, primcount);

    Draw_cleanup();
}

bool
WebGLContext::DrawElements_check(GLsizei count, GLenum type, WebGLintptr byteOffset, GLsizei primcount, const char* info)
{
    if (count < 0 || byteOffset < 0) {
        ErrorInvalidValue("%s: negative count or offset", info);
        return false;
    }

    if (primcount < 0) {
        ErrorInvalidValue("%s: negative primcount", info);
        return false;
    }

    if (!ValidateStencilParamsForDrawCall()) {
        return false;
    }

    // If count is 0, there's nothing to do.
    if (count == 0 || primcount == 0) {
        return false;
    }

    CheckedUint32 checked_byteCount;

    GLsizei first = 0;

    if (type == LOCAL_GL_UNSIGNED_SHORT) {
        checked_byteCount = 2 * CheckedUint32(count);
        if (byteOffset % 2 != 0) {
            ErrorInvalidOperation("%s: invalid byteOffset for UNSIGNED_SHORT (must be a multiple of 2)", info);
            return false;
        }
        first = byteOffset / 2;
    }
    else if (type == LOCAL_GL_UNSIGNED_BYTE) {
        checked_byteCount = count;
        first = byteOffset;
    }
    else if (type == LOCAL_GL_UNSIGNED_INT && IsExtensionEnabled(OES_element_index_uint)) {
        checked_byteCount = 4 * CheckedUint32(count);
        if (byteOffset % 4 != 0) {
            ErrorInvalidOperation("%s: invalid byteOffset for UNSIGNED_INT (must be a multiple of 4)", info);
            return false;
        }
        first = byteOffset / 4;
    }
    else {
        ErrorInvalidEnum("%s: type must be UNSIGNED_SHORT or UNSIGNED_BYTE", info);
        return false;
    }

    if (!checked_byteCount.isValid()) {
        ErrorInvalidValue("%s: overflow in byteCount", info);
        return false;
    }

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        return false;
    }

    if (!mBoundVertexArray->mBoundElementArrayBuffer) {
        ErrorInvalidOperation("%s: must have element array buffer binding", info);
        return false;
    }

    if (!mBoundVertexArray->mBoundElementArrayBuffer->ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer doesn't have any data", info);
        return false;
    }

    CheckedInt<GLsizei> checked_neededByteCount = checked_byteCount.toChecked<GLsizei>() + byteOffset;

    if (!checked_neededByteCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in byteOffset+byteCount", info);
        return false;
    }

    if (uint32_t(checked_neededByteCount.value()) > mBoundVertexArray->mBoundElementArrayBuffer->ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer is too small for given count and offset", info);
        return false;
    }

    if (!ValidateBufferFetching(info))
        return false;

    if (!mMaxFetchedVertices ||
        !mBoundVertexArray->mBoundElementArrayBuffer->Validate(type, mMaxFetchedVertices - 1, first, count))
    {
        ErrorInvalidOperation(
                              "%s: bound vertex attribute buffers do not have sufficient "
                              "size for given indices from the bound element array", info);
        return false;
    }

    if (uint32_t(primcount) > mMaxFetchedInstances) {
        ErrorInvalidOperation("%s: bound instance attribute buffers do not have sufficient size for given primcount", info);
        return false;
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    }

    if (!DoFakeVertexAttrib0(mMaxFetchedVertices)) {
        return false;
    }
    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawElements(GLenum mode, GLsizei count, GLenum type,
                               WebGLintptr byteOffset)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElements: mode"))
        return;

    if (!DrawElements_check(count, type, byteOffset, 1, "drawElements"))
        return;

    SetupContextLossTimer();
    gl->fDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(byteOffset));

    Draw_cleanup();
}

void
WebGLContext::DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                        WebGLintptr byteOffset, GLsizei primcount)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElementsInstanced: mode"))
        return;

    if (!DrawElements_check(count, type, byteOffset, primcount, "drawElementsInstanced"))
        return;

    if (!DrawInstanced_check("drawElementsInstanced"))
        return;

    SetupContextLossTimer();
    gl->fDrawElementsInstanced(mode, count, type, reinterpret_cast<GLvoid*>(byteOffset), primcount);

    Draw_cleanup();
}

void WebGLContext::Draw_cleanup()
{
    UndoFakeVertexAttrib0();
    UnbindFakeBlackTextures();

    if (!mBoundFramebuffer) {
        Invalidate();
        mShouldPresent = true;
        mIsScreenCleared = false;
    }

    if (gl->WorkAroundDriverBugs()) {
        if (gl->Renderer() == gl::GLContext::RendererTegra) {
            mDrawCallsSinceLastFlush++;

            if (mDrawCallsSinceLastFlush >= MAX_DRAW_CALLS_SINCE_FLUSH) {
                gl->fFlush();
                mDrawCallsSinceLastFlush = 0;
            }
        }
    }
}

/*
 * Verify that state is consistent for drawing, and compute max number of elements (maxAllowedCount)
 * that will be legal to be read from bound VBOs.
 */

bool
WebGLContext::ValidateBufferFetching(const char *info)
{
#ifdef DEBUG
    GLint currentProgram = 0;
    MakeContextCurrent();
    gl->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram);
    MOZ_ASSERT(GLuint(currentProgram) == mCurrentProgram->GLName(),
               "WebGL: current program doesn't agree with GL state");
#endif

    if (mBufferFetchingIsVerified) {
        return true;
    }

    bool hasPerVertex = false;
    uint32_t maxVertices = UINT32_MAX;
    uint32_t maxInstances = UINT32_MAX;
    uint32_t attribs = mBoundVertexArray->mAttribs.Length();

    for (uint32_t i = 0; i < attribs; ++i) {
        const WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[i];

        // If the attrib array isn't enabled, there's nothing to check;
        // it's a static value.
        if (!vd.enabled)
            continue;

        if (vd.buf == nullptr) {
            ErrorInvalidOperation("%s: no VBO bound to enabled vertex attrib index %d!", info, i);
            return false;
        }

        // If the attrib is not in use, then we don't have to validate
        // it, just need to make sure that the binding is non-null.
        if (!mCurrentProgram->IsAttribInUse(i))
            continue;

        // the base offset
        CheckedUint32 checked_byteLength = CheckedUint32(vd.buf->ByteLength()) - vd.byteOffset;
        CheckedUint32 checked_sizeOfLastElement = CheckedUint32(vd.componentSize()) * vd.size;

        if (!checked_byteLength.isValid() ||
            !checked_sizeOfLastElement.isValid())
        {
            ErrorInvalidOperation("%s: integer overflow occured while checking vertex attrib %d", info, i);
            return false;
        }

        if (checked_byteLength.value() < checked_sizeOfLastElement.value()) {
            maxVertices = 0;
            maxInstances = 0;
            break;
        }

        CheckedUint32 checked_maxAllowedCount = ((checked_byteLength - checked_sizeOfLastElement) / vd.actualStride()) + 1;

        if (!checked_maxAllowedCount.isValid()) {
            ErrorInvalidOperation("%s: integer overflow occured while checking vertex attrib %d", info, i);
            return false;
        }

        if (vd.divisor == 0) {
            maxVertices = std::min(maxVertices, checked_maxAllowedCount.value());
            hasPerVertex = true;
        } else {
            maxInstances = std::min(maxInstances, checked_maxAllowedCount.value() / vd.divisor);
        }
    }

    mBufferFetchingIsVerified = true;
    mBufferFetchingHasPerVertex = hasPerVertex;
    mMaxFetchedVertices = maxVertices;
    mMaxFetchedInstances = maxInstances;

    return true;
}

