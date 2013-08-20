/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"

using namespace mozilla;
using namespace mozilla::dom;

void
WebGLContext::BindBuffer(WebGLenum target, WebGLBuffer *buffer)
{
    if (!IsContextStable())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBuffer", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bindBuffer");

    if (!bufferSlot) {
        return;
    }

    if (buffer) {
        if (!buffer->Target()) {
            buffer->SetTarget(target);
            buffer->SetHasEverBeenBound(true);
        } else if (target != buffer->Target()) {
            return ErrorInvalidOperation("bindBuffer: buffer already bound to a different target");
        }
    }

    *bufferSlot = buffer;

    MakeContextCurrent();

    gl->fBindBuffer(target, buffer ? buffer->GLName() : 0);
}

void
WebGLContext::BindBufferBase(WebGLenum target, WebGLuint index, WebGLBuffer* buffer)
{
    if (!IsContextStable())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBufferBase", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted()) {
        return;
    }

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTargetIndexed(target, index, "bindBufferBase");

    if (!bufferSlot) {
        return;
    }

    if (buffer) {
        if (!buffer->Target()) {
            buffer->SetTarget(target);
            buffer->SetHasEverBeenBound(true);
        } else if (target != buffer->Target()) {
            return ErrorInvalidOperation("bindBuffer: buffer already bound to a different target");
        }
    }

    *bufferSlot = buffer;

    MakeContextCurrent();

    gl->fBindBufferBase(target, index, buffer ? buffer->GLName() : 0);
}

void
WebGLContext::BindBufferRange(WebGLenum target, WebGLuint index, WebGLBuffer* buffer,
                              WebGLintptr offset, WebGLsizeiptr size)
{
    if (!IsContextStable())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBufferRange", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTargetIndexed(target, index, "bindBufferBase");

    if (!bufferSlot) {
        return;
    }

    if (buffer) {
        if (!buffer->Target()) {
            buffer->SetTarget(target);
            buffer->SetHasEverBeenBound(true);
        } else if (target != buffer->Target()) {
            return ErrorInvalidOperation("bindBuffer: buffer already bound to a different target");
        }
    }

    *bufferSlot = buffer;

    MakeContextCurrent();

    gl->fBindBufferRange(target, index, buffer ? buffer->GLName() : 0, offset, size);
}

void
WebGLContext::BufferData(WebGLenum target, WebGLsizeiptr size,
                         WebGLenum usage)
{
    if (!IsContextStable())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferData");

    if (!bufferSlot) {
        return;
    }

    if (size < 0)
        return ErrorInvalidValue("bufferData: negative size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    WebGLBuffer* boundBuffer = bufferSlot->get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    void* zeroBuffer = calloc(size, 1);
    if (!zeroBuffer)
        return ErrorOutOfMemory("bufferData: out of memory");

    MakeContextCurrent();
    InvalidateBufferFetching();

    GLenum error = CheckedBufferData(target, size, zeroBuffer, usage);
    free(zeroBuffer);

    if (error) {
        GenerateWarning("bufferData generated error %s", ErrorName(error));
        return;
    }

    boundBuffer->SetByteLength(size);
    if (!boundBuffer->ElementArrayCacheBufferData(nullptr, size)) {
        return ErrorOutOfMemory("bufferData: out of memory");
    }
}

void
WebGLContext::BufferData(WebGLenum target,
                         const Nullable<ArrayBuffer> &maybeData,
                         WebGLenum usage)
{
    if (!IsContextStable())
        return;

    if (maybeData.IsNull()) {
        // see http://www.khronos.org/bugzilla/show_bug.cgi?id=386
        return ErrorInvalidValue("bufferData: null object passed");
    }

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferData");

    if (!bufferSlot) {
        return;
    }

    const ArrayBuffer& data = maybeData.Value();

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    WebGLBuffer* boundBuffer = bufferSlot->get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    MakeContextCurrent();
    InvalidateBufferFetching();

    GLenum error = CheckedBufferData(target, data.Length(), data.Data(), usage);

    if (error) {
        GenerateWarning("bufferData generated error %s", ErrorName(error));
        return;
    }

    boundBuffer->SetByteLength(data.Length());
    if (!boundBuffer->ElementArrayCacheBufferData(data.Data(), data.Length())) {
        return ErrorOutOfMemory("bufferData: out of memory");
    }
}

void
WebGLContext::BufferData(WebGLenum target, const ArrayBufferView& data,
                         WebGLenum usage)
{
    if (!IsContextStable())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferSubData");

    if (!bufferSlot) {
        return;
    }

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    WebGLBuffer* boundBuffer = bufferSlot->get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    InvalidateBufferFetching();
    MakeContextCurrent();

    GLenum error = CheckedBufferData(target, data.Length(), data.Data(), usage);
    if (error) {
        GenerateWarning("bufferData generated error %s", ErrorName(error));
        return;
    }

    boundBuffer->SetByteLength(data.Length());
    if (!boundBuffer->ElementArrayCacheBufferData(data.Data(), data.Length())) {
        return ErrorOutOfMemory("bufferData: out of memory");
    }
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const Nullable<ArrayBuffer> &maybeData)
{
    if (!IsContextStable())
        return;

    if (maybeData.IsNull()) {
        // see http://www.khronos.org/bugzilla/show_bug.cgi?id=386
        return;
    }

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferSubData");

    if (!bufferSlot) {
        return;
    }

    const ArrayBuffer& data = maybeData.Value();

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

    WebGLBuffer* boundBuffer = bufferSlot->get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    CheckedUint32 checked_neededByteLength = CheckedUint32(byteOffset) + data.Length();
    if (!checked_neededByteLength.isValid())
        return ErrorInvalidValue("bufferSubData: integer overflow computing the needed byte length");

    if (checked_neededByteLength.value() > boundBuffer->ByteLength())
        return ErrorInvalidValue("bufferSubData: not enough data - operation requires %d bytes, but buffer only has %d bytes",
                                 checked_neededByteLength.value(), boundBuffer->ByteLength());

    MakeContextCurrent();

    boundBuffer->ElementArrayCacheBufferSubData(byteOffset, data.Data(), data.Length());

    gl->fBufferSubData(target, byteOffset, data.Length(), data.Data());
}

void
WebGLContext::BufferSubData(WebGLenum target, WebGLsizeiptr byteOffset,
                            const ArrayBufferView& data)
{
    if (!IsContextStable())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferSubData");

    if (!bufferSlot) {
        return;
    }

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

    WebGLBuffer* boundBuffer = bufferSlot->get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferSubData: no buffer bound!");

    CheckedUint32 checked_neededByteLength = CheckedUint32(byteOffset) + data.Length();
    if (!checked_neededByteLength.isValid())
        return ErrorInvalidValue("bufferSubData: integer overflow computing the needed byte length");

    if (checked_neededByteLength.value() > boundBuffer->ByteLength())
        return ErrorInvalidValue("bufferSubData: not enough data -- operation requires %d bytes, but buffer only has %d bytes",
                                 checked_neededByteLength.value(), boundBuffer->ByteLength());

    boundBuffer->ElementArrayCacheBufferSubData(byteOffset, data.Data(), data.Length());

    MakeContextCurrent();
    gl->fBufferSubData(target, byteOffset, data.Length(), data.Data());
}

already_AddRefed<WebGLBuffer>
WebGLContext::CreateBuffer()
{
    if (!IsContextStable())
        return nullptr;

    nsRefPtr<WebGLBuffer> globj = new WebGLBuffer(this);
    return globj.forget();
}

void
WebGLContext::DeleteBuffer(WebGLBuffer *buffer)
{
    if (!IsContextStable())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteBuffer", buffer))
        return;

    if (!buffer || buffer->IsDeleted())
        return;

    if (mBoundArrayBuffer == buffer) {
        BindBuffer(LOCAL_GL_ARRAY_BUFFER,
                   static_cast<WebGLBuffer*>(nullptr));
    }

    if (mBoundVertexArray->mBoundElementArrayBuffer == buffer) {
        BindBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<WebGLBuffer*>(nullptr));
    }

    for (int32_t i = 0; i < mGLMaxVertexAttribs; i++) {
        if (mBoundVertexArray->mAttribBuffers[i].buf == buffer)
            mBoundVertexArray->mAttribBuffers[i].buf = nullptr;
    }

    buffer->RequestDelete();
}

bool
WebGLContext::IsBuffer(WebGLBuffer *buffer)
{
    if (!IsContextStable())
        return false;

    return ValidateObjectAllowDeleted("isBuffer", buffer) &&
           !buffer->IsDeleted() &&
           buffer->HasEverBeenBound();
}

bool
WebGLContext::ValidateBufferUsageEnum(WebGLenum target, const char *infos)
{
    switch (target) {
        case LOCAL_GL_STREAM_DRAW:
        case LOCAL_GL_STATIC_DRAW:
        case LOCAL_GL_DYNAMIC_DRAW:
            return true;
        default:
            break;
    }

    ErrorInvalidEnumInfo(infos, target);
    return false;
}

WebGLRefPtr<WebGLBuffer>*
WebGLContext::GetBufferSlotByTarget(GLenum target, const char* infos)
{
    switch (target) {
        case LOCAL_GL_ARRAY_BUFFER:
            return &mBoundArrayBuffer;

        case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
            return &mBoundVertexArray->mBoundElementArrayBuffer;

        case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
            if (!IsWebGL2()) {
                break;
            }
            return &mBoundTransformFeedbackBuffer;

        default:
            break;
    }

    ErrorInvalidEnum("%s: target: invalid enum value 0x%x", infos, target);
    return nullptr;
}

WebGLRefPtr<WebGLBuffer>*
WebGLContext::GetBufferSlotByTargetIndexed(GLenum target, GLuint index, const char* infos)
{
    switch (target) {
        case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
            if (index >= mGLMaxTransformFeedbackSeparateAttribs) {
                ErrorInvalidValue("%s: index should be less than MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS", infos, index);
                return nullptr;
            }
            return nullptr; // See bug 903594

        default:
            break;
    }

    ErrorInvalidEnum("%s: target: invalid enum value 0x%x", infos, target);
    return nullptr;
}
