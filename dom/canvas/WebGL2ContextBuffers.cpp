/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLTransformFeedback.h"

namespace mozilla {

bool
WebGL2Context::ValidateBufferTarget(GLenum target, const char* funcName)
{
    switch (target) {
    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_COPY_READ_BUFFER:
    case LOCAL_GL_COPY_WRITE_BUFFER:
    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
        return true;

    default:
        ErrorInvalidEnumInfo(funcName, target);
        return false;
    }
}

bool
WebGL2Context::ValidateBufferIndexedTarget(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
        return true;

    default:
        ErrorInvalidEnumInfo(info, target);
        return false;
    }
}

bool
WebGL2Context::ValidateBufferUsageEnum(GLenum usage, const char* info)
{
    switch (usage) {
    case LOCAL_GL_DYNAMIC_COPY:
    case LOCAL_GL_DYNAMIC_DRAW:
    case LOCAL_GL_DYNAMIC_READ:
    case LOCAL_GL_STATIC_COPY:
    case LOCAL_GL_STATIC_DRAW:
    case LOCAL_GL_STATIC_READ:
    case LOCAL_GL_STREAM_COPY:
    case LOCAL_GL_STREAM_DRAW:
    case LOCAL_GL_STREAM_READ:
        return true;
    default:
        break;
    }

    ErrorInvalidEnumInfo(info, usage);
    return false;
}

// -------------------------------------------------------------------------
// Buffer objects

void
WebGL2Context::CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                                 GLintptr readOffset, GLintptr writeOffset,
                                 GLsizeiptr size)
{
    const char funcName[] = "copyBufferSubData";
    if (IsContextLost())
        return;

    if (!ValidateBufferTarget(readTarget, funcName) ||
        !ValidateBufferTarget(writeTarget, funcName))
    {
        return;
    }

    const WebGLRefPtr<WebGLBuffer>& readBufferSlot = GetBufferSlotByTarget(readTarget);
    const WebGLRefPtr<WebGLBuffer>& writeBufferSlot = GetBufferSlotByTarget(writeTarget);
    if (!readBufferSlot || !writeBufferSlot)
        return;

    const WebGLBuffer* readBuffer = readBufferSlot.get();
    if (!readBuffer) {
        ErrorInvalidOperation("%s: No buffer bound to readTarget.", funcName);
        return;
    }

    WebGLBuffer* writeBuffer = writeBufferSlot.get();
    if (!writeBuffer) {
        ErrorInvalidOperation("%s: No buffer bound to writeTarget.", funcName);
        return;
    }

    if (!ValidateDataOffsetSize(readOffset, size, readBuffer->ByteLength(), funcName))
        return;

    if (!ValidateDataOffsetSize(writeOffset, size, writeBuffer->ByteLength(), funcName))
        return;

    if (readTarget == writeTarget &&
        !ValidateDataRanges(readOffset, writeOffset, size, funcName))
    {
        return;
    }

    WebGLBuffer::Kind readType = readBuffer->Content();
    WebGLBuffer::Kind writeType = writeBuffer->Content();

    if (readType != WebGLBuffer::Kind::Undefined &&
        writeType != WebGLBuffer::Kind::Undefined &&
        writeType != readType)
    {
        ErrorInvalidOperation("%s: Can't copy %s data to %s data.",
                              funcName,
                              (readType == WebGLBuffer::Kind::OtherData) ? "other"
                                                                         : "element",
                              (writeType == WebGLBuffer::Kind::OtherData) ? "other"
                                                                          : "element");
        return;
    }

    WebGLContextUnchecked::CopyBufferSubData(readTarget, writeTarget, readOffset,
                                             writeOffset, size);

    if (writeType == WebGLBuffer::Kind::Undefined) {
        writeBuffer->BindTo(
            (readType == WebGLBuffer::Kind::OtherData) ? LOCAL_GL_ARRAY_BUFFER
                                                       : LOCAL_GL_ELEMENT_ARRAY_BUFFER);
    }
}

// BufferT may be one of
// const dom::ArrayBuffer&
// const dom::SharedArrayBuffer&
template<typename BufferT>
void
WebGL2Context::GetBufferSubDataT(GLenum target, GLintptr offset, const BufferT& data)
{
    const char funcName[] = "getBufferSubData";
    if (IsContextLost())
        return;

    // For the WebGLBuffer bound to the passed target, read
    // returnedData.byteLength bytes from the buffer starting at byte
    // offset offset and write them to returnedData.

    // If zero is bound to target, an INVALID_OPERATION error is
    // generated.
    if (!ValidateBufferTarget(target, funcName))
        return;

    // If offset is less than zero, an INVALID_VALUE error is
    // generated.
    if (offset < 0) {
        ErrorInvalidValue("%s: Offset must be non-negative.", funcName);
        return;
    }

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);
    WebGLBuffer* boundBuffer = bufferSlot.get();
    if (!boundBuffer) {
        ErrorInvalidOperation("%s: No buffer bound.", funcName);
        return;
    }

    // If offset + returnedData.byteLength would extend beyond the end
    // of the buffer an INVALID_VALUE error is generated.
    data.ComputeLengthAndData();

    CheckedInt<WebGLsizeiptr> neededByteLength = CheckedInt<WebGLsizeiptr>(offset) + data.LengthAllowShared();
    if (!neededByteLength.isValid()) {
        ErrorInvalidValue("%s: Integer overflow computing the needed byte length.",
                          funcName);
        return;
    }

    if (neededByteLength.value() > boundBuffer->ByteLength()) {
        ErrorInvalidValue("%s: Not enough data. Operation requires %d bytes, but buffer"
                          " only has %d bytes.",
                          funcName, neededByteLength.value(), boundBuffer->ByteLength());
        return;
    }

    // If target is TRANSFORM_FEEDBACK_BUFFER, and any transform
    // feedback object is currently active, an INVALID_OPERATION error
    // is generated.
    WebGLTransformFeedback* currentTF = mBoundTransformFeedback;
    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER && currentTF) {
        if (currentTF->mIsActive) {
            ErrorInvalidOperation("%s: Currently bound transform feedback is active.",
                                  funcName);
            return;
        }

        // https://github.com/NVIDIA/WebGL/commit/63aff5e58c1d79825a596f0f4aa46174b9a5f72c
        // Performing reads and writes on a buffer that is currently
        // bound for transform feedback causes undefined results in
        // GLES3.0 and OpenGL 4.5. In practice results of reads and
        // writes might be consistent as long as transform feedback
        // objects are not active, but neither GLES3.0 nor OpenGL 4.5
        // spec guarantees this - just being bound for transform
        // feedback is sufficient to cause undefined results.

        BindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, nullptr);
    }

    /* If the buffer is written and read sequentially by other
     * operations and getBufferSubData, it is the responsibility of
     * the WebGL API to ensure that data are access
     * consistently. This applies even if the buffer is currently
     * bound to a transform feedback binding point.
     */

    void* ptr = gl->fMapBufferRange(target, offset, data.LengthAllowShared(),
                                    LOCAL_GL_MAP_READ_BIT);
    // Warning: Possibly shared memory.  See bug 1225033.
    memcpy(data.DataAllowShared(), ptr, data.LengthAllowShared());
    gl->fUnmapBuffer(target);

    ////

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER && currentTF) {
        BindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, currentTF);
    }
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset,
                                const dom::Nullable<dom::ArrayBuffer>& maybeData)
{
    // If returnedData is null then an INVALID_VALUE error is
    // generated.
    if (maybeData.IsNull()) {
        ErrorInvalidValue("getBufferSubData: returnedData is null");
        return;
    }

    const dom::ArrayBuffer& data = maybeData.Value();
    GetBufferSubDataT(target, offset, data);
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset,
                                const dom::SharedArrayBuffer& data)
{
    GetBufferSubDataT(target, offset, data);
}

} // namespace mozilla
