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
WebGLContext::BindBuffer(GLenum target, WebGLBuffer *buffer)
{
    if (IsContextLost())
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
WebGLContext::BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBufferBase", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted()) {
        return;
    }

    WebGLRefPtr<WebGLBuffer>* indexedBufferSlot = GetBufferSlotByTargetIndexed(target, index, "bindBufferBase");

    if (!indexedBufferSlot) {
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

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bindBuffer");

    MOZ_ASSERT(bufferSlot, "GetBufferSlotByTarget(Indexed) mismatch");

    *indexedBufferSlot = buffer;
    *bufferSlot = buffer;

    MakeContextCurrent();

    gl->fBindBufferBase(target, index, buffer ? buffer->GLName() : 0);
}

void
WebGLContext::BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                              WebGLintptr offset, WebGLsizeiptr size)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBufferRange", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    WebGLRefPtr<WebGLBuffer>* indexedBufferSlot = GetBufferSlotByTargetIndexed(target, index, "bindBufferBase");

    if (!indexedBufferSlot) {
        return;
    }

    if (buffer) {
        if (!buffer->Target()) {
            buffer->SetTarget(target);
            buffer->SetHasEverBeenBound(true);
        } else if (target != buffer->Target()) {
            return ErrorInvalidOperation("bindBuffer: buffer already bound to a different target");
        }
        CheckedInt<WebGLsizeiptr> checked_neededByteLength = CheckedInt<WebGLsizeiptr>(offset) + size;
        if (!checked_neededByteLength.isValid() ||
            checked_neededByteLength.value() > buffer->ByteLength())
        {
            return ErrorInvalidValue("bindBufferRange: invalid range");
        }
    }

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bindBuffer");

    MOZ_ASSERT(bufferSlot, "GetBufferSlotByTarget(Indexed) mismatch");

    *indexedBufferSlot = buffer;
    *bufferSlot = buffer;

    MakeContextCurrent();

    gl->fBindBufferRange(target, index, buffer ? buffer->GLName() : 0, offset, size);
}

void
WebGLContext::BufferData(GLenum target, WebGLsizeiptr size,
                         GLenum usage)
{
    if (IsContextLost())
        return;

    WebGLRefPtr<WebGLBuffer>* bufferSlot = GetBufferSlotByTarget(target, "bufferData");

    if (!bufferSlot) {
        return;
    }

    if (size < 0)
        return ErrorInvalidValue("bufferData: negative size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    // careful: WebGLsizeiptr is always 64-bit, but GLsizeiptr is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(size).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

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
WebGLContext::BufferData(GLenum target,
                         const Nullable<ArrayBuffer> &maybeData,
                         GLenum usage)
{
    if (IsContextLost())
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

    // careful: data.Length() could conceivably be any size_t, but GLsizeiptr is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(data.Length()).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

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
WebGLContext::BufferData(GLenum target, const ArrayBufferView& data,
                         GLenum usage)
{
    if (IsContextLost())
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

    // careful: data.Length() could conceivably be any size_t, but GLsizeiptr is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(data.Length()).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

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
    if (IsContextLost())
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

    CheckedInt<WebGLsizeiptr> checked_neededByteLength = CheckedInt<WebGLsizeiptr>(byteOffset) + data.Length();
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
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const ArrayBufferView& data)
{
    if (IsContextLost())
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

    CheckedInt<WebGLsizeiptr> checked_neededByteLength = CheckedInt<WebGLsizeiptr>(byteOffset) + data.Length();
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
    if (IsContextLost())
        return nullptr;

    nsRefPtr<WebGLBuffer> globj = new WebGLBuffer(this);
    return globj.forget();
}

void
WebGLContext::DeleteBuffer(WebGLBuffer *buffer)
{
    if (IsContextLost())
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
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isBuffer", buffer) &&
           !buffer->IsDeleted() &&
           buffer->HasEverBeenBound();
}

bool
WebGLContext::ValidateBufferUsageEnum(GLenum target, const char *infos)
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
