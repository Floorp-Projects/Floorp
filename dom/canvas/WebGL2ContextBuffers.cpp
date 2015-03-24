/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLBuffer.h"

using namespace mozilla;
using namespace mozilla::dom;

bool
WebGL2Context::ValidateBufferTarget(GLenum target, const char* info)
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
        ErrorInvalidEnumInfo(info, target);
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
WebGL2Context::ValidateBufferForTarget(GLenum target, WebGLBuffer* buffer,
                                       const char* info)
{
    if (!buffer)
        return true;

    switch (target) {
    case LOCAL_GL_COPY_READ_BUFFER:
    case LOCAL_GL_COPY_WRITE_BUFFER:
        return true;

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        return !buffer->HasEverBeenBound() ||
            buffer->Target() == LOCAL_GL_ELEMENT_ARRAY_BUFFER;

    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
        return !buffer->HasEverBeenBound() ||
            buffer->Target() != LOCAL_GL_ELEMENT_ARRAY_BUFFER;
    }

    ErrorInvalidOperation("%s: buffer already bound to a incompatible target %s",
                          info, EnumName(buffer->Target().get()));
    return false;
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
    if (IsContextLost())
        return;

    if (!ValidateBufferTarget(readTarget, "copyBufferSubData") ||
        !ValidateBufferTarget(writeTarget, "copyBufferSubData"))
    {
        return;
    }

    const WebGLRefPtr<WebGLBuffer>& readBufferSlot = GetBufferSlotByTarget(readTarget);
    const WebGLRefPtr<WebGLBuffer>& writeBufferSlot = GetBufferSlotByTarget(writeTarget);
    if (!readBufferSlot || !writeBufferSlot)
        return;

    const WebGLBuffer* readBuffer = readBufferSlot.get();
    if (!readBuffer)
        return ErrorInvalidOperation("copyBufferSubData: No buffer bound to readTarget");

    const WebGLBuffer* writeBuffer = writeBufferSlot.get();
    if (!writeBuffer)
        return ErrorInvalidOperation("copyBufferSubData: No buffer bound to writeTarget");

    if (!ValidateDataOffsetSize(readOffset, size, readBuffer->ByteLength(),
        "copyBufferSubData"))
    {
        return;
    }

    if (!ValidateDataOffsetSize(writeOffset, size, writeBuffer->ByteLength(),
        "copyBufferSubData"))
    {
        return;
    }

    if (readTarget == writeTarget &&
        !ValidateDataRanges(readOffset, writeOffset, size, "copyBufferSubData"))
    {
        return;
    }

    WebGLContextUnchecked::CopyBufferSubData(readTarget, writeTarget, readOffset,
                                             writeOffset, size);
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset,
                                const dom::ArrayBuffer& returnedData)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset,
                                const dom::ArrayBufferView& returnedData)
{
    MOZ_CRASH("Not Implemented.");
}
