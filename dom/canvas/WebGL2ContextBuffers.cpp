/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLTransformFeedback.h"

namespace mozilla {

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

    const auto& readBuffer = ValidateBufferSelection(funcName, readTarget);
    if (!readBuffer)
        return;

    const auto& writeBuffer = ValidateBufferSelection(funcName, writeTarget);
    if (!writeBuffer)
        return;

    if (readBuffer->mNumActiveTFOs ||
        writeBuffer->mNumActiveTFOs)
    {
        ErrorInvalidOperation("%s: Buffer is bound to an active transform feedback"
                              " object.",
                              funcName);
        return;
    }

    if (!ValidateNonNegative(funcName, "readOffset", readOffset) ||
        !ValidateNonNegative(funcName, "writeOffset", writeOffset) ||
        !ValidateNonNegative(funcName, "size", size))
    {
        return;
    }

    const auto fnValidateOffsetSize = [&](const char* info, GLintptr offset,
                                          const WebGLBuffer* buffer)
    {
        const auto neededBytes = CheckedInt<size_t>(offset) + size;
        if (!neededBytes.isValid() || neededBytes.value() > buffer->ByteLength()) {
            ErrorInvalidValue("%s: Invalid %s range.", funcName, info);
            return false;
        }
        return true;
    };

    if (!fnValidateOffsetSize("read", readOffset, readBuffer) ||
        !fnValidateOffsetSize("write", writeOffset, writeBuffer))
    {
        return;
    }

    if (readBuffer == writeBuffer &&
        !ValidateDataRanges(readOffset, writeOffset, size, funcName))
    {
        return;
    }

    const auto& readType = readBuffer->Content();
    const auto& writeType = writeBuffer->Content();
    MOZ_ASSERT(readType != WebGLBuffer::Kind::Undefined);
    MOZ_ASSERT(writeType != WebGLBuffer::Kind::Undefined);
    if (writeType != readType) {
        ErrorInvalidOperation("%s: Can't copy %s data to %s data.",
                              funcName,
                              (readType == WebGLBuffer::Kind::OtherData) ? "other"
                                                                         : "element",
                              (writeType == WebGLBuffer::Kind::OtherData) ? "other"
                                                                          : "element");
        return;
    }

    gl->MakeCurrent();
    const ScopedLazyBind readBind(gl, readTarget, readBuffer);
    const ScopedLazyBind writeBind(gl, writeTarget, writeBuffer);
    gl->fCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset,
                                const dom::ArrayBufferView& data)
{
    const char funcName[] = "getBufferSubData";
    if (IsContextLost())
        return;

    if (!ValidateNonNegative(funcName, "offset", offset))
        return;

    const auto& buffer = ValidateBufferSelection(funcName, target);
    if (!buffer)
        return;

    ////

    // If offset + returnedData.byteLength would extend beyond the end
    // of the buffer an INVALID_VALUE error is generated.
    data.ComputeLengthAndData();

    const auto neededByteLength = CheckedInt<size_t>(offset) + data.LengthAllowShared();
    if (!neededByteLength.isValid()) {
        ErrorInvalidValue("%s: Integer overflow computing the needed byte length.",
                          funcName);
        return;
    }

    if (neededByteLength.value() > buffer->ByteLength()) {
        ErrorInvalidValue("%s: Not enough data. Operation requires %d bytes, but buffer"
                          " only has %d bytes.",
                          funcName, neededByteLength.value(), buffer->ByteLength());
        return;
    }

    ////

    if (buffer->mNumActiveTFOs) {
        ErrorInvalidOperation("%s: Buffer is bound to an active transform feedback"
                              " object.",
                              funcName);
        return;
    }

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER &&
        mBoundTransformFeedback->mIsActive)
    {
        ErrorInvalidOperation("%s: Currently bound transform feedback is active.",
                              funcName);
        return;
    }

    ////

    gl->MakeCurrent();
    const ScopedLazyBind readBind(gl, target, buffer);

    const auto ptr = gl->fMapBufferRange(target, offset, data.LengthAllowShared(),
                                         LOCAL_GL_MAP_READ_BIT);
    // Warning: Possibly shared memory.  See bug 1225033.
    memcpy(data.DataAllowShared(), ptr, data.LengthAllowShared());
    gl->fUnmapBuffer(target);
}

} // namespace mozilla
