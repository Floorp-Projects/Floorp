/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexAttribData.h"

#include "GLContext.h"
#include "WebGLBuffer.h"

namespace mozilla {

static uint8_t
CalcBytesPerVertex(GLenum type, uint8_t size)
{
    uint8_t bytesPerType;
    switch (type) {
    case LOCAL_GL_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
        return 4;

    case LOCAL_GL_BYTE:
    case LOCAL_GL_UNSIGNED_BYTE:
        bytesPerType = 1;
        break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT:
        bytesPerType = 2;
        break;

    case LOCAL_GL_FIXED: // GLES 3.0.4 p9: 32-bit signed, with 16 fractional bits.
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
        bytesPerType = 4;
        break;

    default:
        MOZ_CRASH("Bad `type`.");
    }

    return bytesPerType * size;
}

static GLenum
AttribPointerBaseType(bool integerFunc, GLenum type)
{
    if (!integerFunc)
        return LOCAL_GL_FLOAT;

    switch (type) {
    case LOCAL_GL_BYTE:
    case LOCAL_GL_SHORT:
    case LOCAL_GL_INT:
        return LOCAL_GL_INT;

    case LOCAL_GL_UNSIGNED_BYTE:
    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_INT:
        return LOCAL_GL_UNSIGNED_INT;

    default:
        MOZ_CRASH();
    }
}

void
WebGLVertexAttribData::VertexAttribPointer(bool integerFunc, WebGLBuffer* buf,
                                           uint8_t size, GLenum type, bool normalized,
                                           uint32_t stride, uint64_t byteOffset)
{
    mIntegerFunc = integerFunc;
    WebGLBuffer::SetSlot(0, buf, &mBuf);
    mType = type;
    mBaseType = AttribPointerBaseType(integerFunc, type);
    mSize = size;
    mBytesPerVertex = CalcBytesPerVertex(mType, mSize);
    mNormalized = normalized;
    mStride = stride;
    mExplicitStride = (mStride ? mStride : mBytesPerVertex);
    mByteOffset = byteOffset;
}

void
WebGLVertexAttribData::DoVertexAttribPointer(gl::GLContext* gl, GLuint index) const
{
    if (mIntegerFunc) {
        gl->fVertexAttribIPointer(index, mSize, mType, mStride,
                                  (const void*)mByteOffset);
    } else {
        gl->fVertexAttribPointer(index, mSize, mType, mNormalized, mStride,
                                 (const void*)mByteOffset);
    }
}

} // namespace mozilla
