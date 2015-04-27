/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"

namespace mozilla {

void
WebGLContext::UpdateBoundBuffer(GLenum target, WebGLBuffer* buffer)
{
    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);
    bufferSlot = buffer;

    if (!buffer)
        return;

    /* https://www.khronos.org/registry/webgl/specs/latest/2.0/#5.1
     *
     * In the WebGL 2 API, buffers have their WebGL buffer type
     * initially set to undefined. Calling bindBuffer, bindBufferRange
     * or bindBufferBase with the target argument set to any buffer
     * binding point except COPY_READ_BUFFER or COPY_WRITE_BUFFER will
     * then set the WebGL buffer type of the buffer being bound
     * according to the table above.
     *
     * Any call to one of these functions which attempts to bind a
     * WebGLBuffer that has the element array WebGL buffer type to a
     * binding point that falls under other data, or bind a
     * WebGLBuffer which has the other data WebGL buffer type to
     * ELEMENT_ARRAY_BUFFER will generate an INVALID_OPERATION error,
     * and the state of the binding point will remain untouched.
     */
    if (target != LOCAL_GL_COPY_READ_BUFFER && target != LOCAL_GL_COPY_WRITE_BUFFER)
        buffer->BindTo(target);
}

void
WebGLContext::UpdateBoundBufferIndexed(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    UpdateBoundBuffer(target, buffer);

    WebGLRefPtr<WebGLBuffer>& bufferIndexSlot =
        GetBufferSlotByTargetIndexed(target, index);
    bufferIndexSlot = buffer;
}

void
WebGLContext::BindBuffer(GLenum target, WebGLBuffer* buffer)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBuffer", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    if (!ValidateBufferTarget(target, "bindBuffer"))
        return;

    if (!ValidateBufferForTarget(target, buffer, "bindBuffer"))
        return;

    WebGLContextUnchecked::BindBuffer(target, buffer);

    UpdateBoundBuffer(target, buffer);
}

void
WebGLContext::BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindBufferBase", buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    // ValidateBufferTarget
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        if (index >= mGLMaxTransformFeedbackSeparateAttribs)
            return ErrorInvalidValue("bindBufferBase: index should be less than "
                                     "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");
        break;

    case LOCAL_GL_UNIFORM_BUFFER:
        if (index >= mGLMaxUniformBufferBindings)
            return ErrorInvalidValue("bindBufferBase: index should be less than "
                                     "MAX_UNIFORM_BUFFER_BINDINGS");
        break;

    default:
        return ErrorInvalidEnumInfo("bindBufferBase: target", target);
    }

    if (!ValidateBufferForTarget(target, buffer, "bindBufferBase"))
        return;

    WebGLContextUnchecked::BindBufferBase(target, index, buffer);

    UpdateBoundBufferIndexed(target, index, buffer);
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

    // ValidateBufferTarget
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        if (index >= mGLMaxTransformFeedbackSeparateAttribs)
            return ErrorInvalidValue("bindBufferRange: index should be less than "
                                     "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS");
        break;

    case LOCAL_GL_UNIFORM_BUFFER:
        if (index >= mGLMaxUniformBufferBindings)
            return ErrorInvalidValue("bindBufferRange: index should be less than "
                                     "MAX_UNIFORM_BUFFER_BINDINGS");
        break;

    default:
        return ErrorInvalidEnumInfo("bindBufferRange: target", target);
    }

    if (!ValidateBufferForTarget(target, buffer, "bindBufferRange"))
        return;

    WebGLContextUnchecked::BindBufferRange(target, index, buffer, offset, size);

    UpdateBoundBufferIndexed(target, index, buffer);
}

void
WebGLContext::BufferData(GLenum target, WebGLsizeiptr size, GLenum usage)
{
    if (IsContextLost())
        return;

    if (!ValidateBufferTarget(target, "bufferData"))
        return;

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);

    if (size < 0)
        return ErrorInvalidValue("bufferData: negative size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    // careful: WebGLsizeiptr is always 64-bit, but GLsizeiptr is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(size).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

    WebGLBuffer* boundBuffer = bufferSlot.get();

    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    UniquePtr<uint8_t> zeroBuffer((uint8_t*)calloc(size, 1));
    if (!zeroBuffer)
        return ErrorOutOfMemory("bufferData: out of memory");

    MakeContextCurrent();
    InvalidateBufferFetching();

    GLenum error = CheckedBufferData(target, size, zeroBuffer.get(), usage);

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
                         const dom::Nullable<dom::ArrayBuffer>& maybeData,
                         GLenum usage)
{
    if (IsContextLost())
        return;

    if (maybeData.IsNull()) {
        // see http://www.khronos.org/bugzilla/show_bug.cgi?id=386
        return ErrorInvalidValue("bufferData: null object passed");
    }

    if (!ValidateBufferTarget(target, "bufferData"))
        return;

    const WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);

    const dom::ArrayBuffer& data = maybeData.Value();
    data.ComputeLengthAndData();

    // Careful: data.Length() could conceivably be any uint32_t, but GLsizeiptr
    // is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(data.Length()).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    WebGLBuffer* boundBuffer = bufferSlot.get();

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
    if (!boundBuffer->ElementArrayCacheBufferData(data.Data(), data.Length()))
        return ErrorOutOfMemory("bufferData: out of memory");
}

void
WebGLContext::BufferData(GLenum target, const dom::ArrayBufferView& data,
                         GLenum usage)
{
    if (IsContextLost())
        return;

    if (!ValidateBufferTarget(target, "bufferData"))
        return;

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return;

    WebGLBuffer* boundBuffer = bufferSlot.get();
    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    data.ComputeLengthAndData();

    // Careful: data.Length() could conceivably be any uint32_t, but GLsizeiptr
    // is like intptr_t.
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
    if (!boundBuffer->ElementArrayCacheBufferData(data.Data(), data.Length()))
        return ErrorOutOfMemory("bufferData: out of memory");
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const dom::Nullable<dom::ArrayBuffer>& maybeData)
{
    if (IsContextLost())
        return;

    if (maybeData.IsNull()) {
        // see http://www.khronos.org/bugzilla/show_bug.cgi?id=386
        return;
    }

    if (!ValidateBufferTarget(target, "bufferSubData"))
        return;

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

    WebGLBuffer* boundBuffer = bufferSlot.get();
    if (!boundBuffer)
        return ErrorInvalidOperation("bufferData: no buffer bound!");

    const dom::ArrayBuffer& data = maybeData.Value();
    data.ComputeLengthAndData();

    CheckedInt<WebGLsizeiptr> checked_neededByteLength =
        CheckedInt<WebGLsizeiptr>(byteOffset) + data.Length();

    if (!checked_neededByteLength.isValid()) {
        ErrorInvalidValue("bufferSubData: Integer overflow computing the needed"
                          " byte length.");
        return;
    }

    if (checked_neededByteLength.value() > boundBuffer->ByteLength()) {
        ErrorInvalidValue("bufferSubData: Not enough data. Operation requires"
                          " %d bytes, but buffer only has %d bytes.",
                          checked_neededByteLength.value(),
                          boundBuffer->ByteLength());
        return;
    }

    boundBuffer->ElementArrayCacheBufferSubData(byteOffset, data.Data(),
                                                data.Length());

    MakeContextCurrent();
    gl->fBufferSubData(target, byteOffset, data.Length(), data.Data());
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const dom::ArrayBufferView& data)
{
    if (IsContextLost())
        return;

    if (!ValidateBufferTarget(target, "bufferSubData"))
        return;

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);

    if (byteOffset < 0)
        return ErrorInvalidValue("bufferSubData: negative offset");

    WebGLBuffer* boundBuffer = bufferSlot.get();
    if (!boundBuffer)
        return ErrorInvalidOperation("bufferSubData: no buffer bound!");

    data.ComputeLengthAndData();

    CheckedInt<WebGLsizeiptr> checked_neededByteLength =
        CheckedInt<WebGLsizeiptr>(byteOffset) + data.Length();

    if (!checked_neededByteLength.isValid()) {
        ErrorInvalidValue("bufferSubData: Integer overflow computing the needed"
                          " byte length.");
        return;
    }

    if (checked_neededByteLength.value() > boundBuffer->ByteLength()) {
        ErrorInvalidValue("bufferSubData: Not enough data. Operation requires"
                          " %d bytes, but buffer only has %d bytes.",
                          checked_neededByteLength.value(),
                          boundBuffer->ByteLength());
        return;
    }

    boundBuffer->ElementArrayCacheBufferSubData(byteOffset, data.Data(),
                                                data.Length());

    MakeContextCurrent();
    gl->fBufferSubData(target, byteOffset, data.Length(), data.Data());
}

already_AddRefed<WebGLBuffer>
WebGLContext::CreateBuffer()
{
    if (IsContextLost())
        return nullptr;

    GLuint buf = 0;
    MakeContextCurrent();
    gl->fGenBuffers(1, &buf);

    nsRefPtr<WebGLBuffer> globj = new WebGLBuffer(this, buf);
    return globj.forget();
}

void
WebGLContext::DeleteBuffer(WebGLBuffer* buffer)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteBuffer", buffer))
        return;

    if (!buffer || buffer->IsDeleted())
        return;

    if (mBoundArrayBuffer == buffer)
        BindBuffer(LOCAL_GL_ARRAY_BUFFER, static_cast<WebGLBuffer*>(nullptr));

    if (mBoundVertexArray->mElementArrayBuffer == buffer) {
        BindBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<WebGLBuffer*>(nullptr));
    }

    for (int32_t i = 0; i < mGLMaxVertexAttribs; i++) {
        if (mBoundVertexArray->HasAttrib(i) &&
            mBoundVertexArray->mAttribs[i].buf == buffer)
        {
            mBoundVertexArray->mAttribs[i].buf = nullptr;
        }
    }

    buffer->RequestDelete();
}

bool
WebGLContext::IsBuffer(WebGLBuffer* buffer)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isBuffer", buffer) &&
           !buffer->IsDeleted() &&
           buffer->HasEverBeenBound();
}

bool
WebGLContext::ValidateBufferUsageEnum(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_STREAM_DRAW:
    case LOCAL_GL_STATIC_DRAW:
    case LOCAL_GL_DYNAMIC_DRAW:
        return true;
    default:
        break;
    }

    ErrorInvalidEnumInfo(info, target);
    return false;
}

WebGLRefPtr<WebGLBuffer>&
WebGLContext::GetBufferSlotByTarget(GLenum target)
{
    /* This function assumes that target has been validated for either
     * WebGL1 or WebGL2.
     */
    switch (target) {
    case LOCAL_GL_ARRAY_BUFFER:
        return mBoundArrayBuffer;

    case LOCAL_GL_COPY_READ_BUFFER:
        return mBoundCopyReadBuffer;

    case LOCAL_GL_COPY_WRITE_BUFFER:
        return mBoundCopyWriteBuffer;

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        return mBoundVertexArray->mElementArrayBuffer;

    case LOCAL_GL_PIXEL_PACK_BUFFER:
        return mBoundPixelPackBuffer;

    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
        return mBoundPixelUnpackBuffer;

    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        return mBoundTransformFeedbackBuffer;

    case LOCAL_GL_UNIFORM_BUFFER:
        return mBoundUniformBuffer;

    default:
        MOZ_CRASH("Should not get here.");
    }
}

WebGLRefPtr<WebGLBuffer>&
WebGLContext::GetBufferSlotByTargetIndexed(GLenum target, GLuint index)
{
    /* This function assumes that target has been validated for either WebGL1 or WebGL. */
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        MOZ_ASSERT(index < mGLMaxTransformFeedbackSeparateAttribs);
        return mBoundTransformFeedbackBuffers[index];

    case LOCAL_GL_UNIFORM_BUFFER:
        MOZ_ASSERT(index < mGLMaxUniformBufferBindings);
        return mBoundUniformBuffers[index];

    default:
        MOZ_CRASH("Should not get here.");
    }
}

GLenum
WebGLContext::CheckedBufferData(GLenum target, GLsizeiptr size,
                                const GLvoid* data, GLenum usage)
{
#ifdef XP_MACOSX
    // bug 790879
    if (gl->WorkAroundDriverBugs() &&
        int64_t(size) > INT32_MAX) // cast avoids a potential always-true warning on 32bit
    {
        GenerateWarning("Rejecting valid bufferData call with size %lu to avoid"
                        " a Mac bug", size);
        return LOCAL_GL_INVALID_VALUE;
    }
#endif

    WebGLRefPtr<WebGLBuffer>& bufferSlot = GetBufferSlotByTarget(target);
    WebGLBuffer* boundBuffer = bufferSlot.get();
    MOZ_ASSERT(boundBuffer, "No buffer bound for this target.");

    bool sizeChanges = uint32_t(size) != boundBuffer->ByteLength();
    if (sizeChanges) {
        GetAndFlushUnderlyingGLErrors();
        gl->fBufferData(target, size, data, usage);
        GLenum error = GetAndFlushUnderlyingGLErrors();
        return error;
    } else {
        gl->fBufferData(target, size, data, usage);
        return LOCAL_GL_NO_ERROR;
    }
}

} // namespace mozilla
