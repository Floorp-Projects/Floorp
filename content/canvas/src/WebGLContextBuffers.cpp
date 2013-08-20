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

    if (!ValidateBufferTarget(target, "bindBuffer")) {
        return;
    }

    if (buffer) {
        if ((buffer->Target() != LOCAL_GL_NONE) && (target != buffer->Target()))
            return ErrorInvalidOperation("bindBuffer: buffer already bound to a different target");
        buffer->SetTarget(target);
        buffer->SetHasEverBeenBound(true);
    }

    // we really want to do this AFTER all the validation is done, otherwise our bookkeeping could get confused.
    // see bug 656752
    if (target == LOCAL_GL_ARRAY_BUFFER) {
        mBoundArrayBuffer = buffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        mBoundVertexArray->mBoundElementArrayBuffer = buffer;
    }

    MakeContextCurrent();

    gl->fBindBuffer(target, buffer ? buffer->GLName() : 0);
}

void
WebGLContext::BufferData(WebGLenum target, WebGLsizeiptr size,
                         WebGLenum usage)
{
    if (!IsContextStable())
        return;

    WebGLBuffer *boundBuffer = nullptr;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundVertexArray->mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnumInfo("bufferData: target", target);
    }

    if (size < 0)
        return ErrorInvalidValue("bufferData: negative size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

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

    const ArrayBuffer& data = maybeData.Value();

    WebGLBuffer *boundBuffer = nullptr;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundVertexArray->mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnumInfo("bufferData: target", target);
    }

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

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

    WebGLBuffer *boundBuffer = nullptr;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundVertexArray->mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnumInfo("bufferData: target", target);
    }

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

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

    const ArrayBuffer& data = maybeData.Value();

    WebGLBuffer *boundBuffer = nullptr;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundVertexArray->mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnumInfo("bufferSubData: target", target);
    }

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

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

    WebGLBuffer *boundBuffer = nullptr;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundVertexArray->mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnumInfo("bufferSubData: target", target);
    }

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

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
WebGLContext::ValidateBufferTarget(WebGLenum target, const char* infos)
{
    switch(target)
    {
        case LOCAL_GL_ARRAY_BUFFER:
        case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
            return true;

        default:
            break;
    }

    ErrorInvalidEnum("%s: target: invalid enum value 0x%x", infos, target);
    return false;
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
