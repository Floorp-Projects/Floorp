/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"

namespace mozilla {

WebGLRefPtr<WebGLBuffer>*
WebGLContext::ValidateBufferSlot(const char* funcName, GLenum target)
{
    WebGLRefPtr<WebGLBuffer>* slot = nullptr;

    switch (target) {
    case LOCAL_GL_ARRAY_BUFFER:
        slot = &mBoundArrayBuffer;
        break;

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        slot = &(mBoundVertexArray->mElementArrayBuffer);
        break;
    }

    if (IsWebGL2()) {
        switch (target) {
        case LOCAL_GL_COPY_READ_BUFFER:
            slot = &mBoundCopyReadBuffer;
            break;

        case LOCAL_GL_COPY_WRITE_BUFFER:
            slot = &mBoundCopyWriteBuffer;
            break;

        case LOCAL_GL_PIXEL_PACK_BUFFER:
            slot = &mBoundPixelPackBuffer;
            break;

        case LOCAL_GL_PIXEL_UNPACK_BUFFER:
            slot = &mBoundPixelUnpackBuffer;
            break;

        case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
            slot = &(mBoundTransformFeedback->mGenericBufferBinding);
            break;

        case LOCAL_GL_UNIFORM_BUFFER:
            slot = &mBoundUniformBuffer;
            break;
        }
    }

    if (!slot) {
        ErrorInvalidEnum("%s: Bad `target`: 0x%04x", funcName, target);
        return nullptr;
    }

    return slot;
}

WebGLBuffer*
WebGLContext::ValidateBufferSelection(const char* funcName, GLenum target)
{
    const auto& slot = ValidateBufferSlot(funcName, target);
    if (!slot)
        return nullptr;
    const auto& buffer = *slot;

    if (!buffer) {
        ErrorInvalidOperation("%s: Buffer for `target` is null.", funcName);
        return nullptr;
    }

    return buffer.get();
}

IndexedBufferBinding*
WebGLContext::ValidateIndexedBufferSlot(const char* funcName, GLenum target, GLuint index)
{
    decltype(mIndexedUniformBufferBindings)* bindings;
    const char* maxIndexEnum;
    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        bindings = &(mBoundTransformFeedback->mIndexedBindings);
        maxIndexEnum = "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS";
        break;

    case LOCAL_GL_UNIFORM_BUFFER:
        bindings = &mIndexedUniformBufferBindings;
        maxIndexEnum = "MAX_UNIFORM_BUFFER_BINDINGS";
        break;

    default:
        ErrorInvalidEnum("%s: Bad `target`: 0x%04x", funcName, target);
        return nullptr;
    }

    if (index >= bindings->size()) {
        ErrorInvalidOperation("%s: `index` >= %s.", funcName, maxIndexEnum);
        return nullptr;
    }

    return &(*bindings)[index];
}

////////////////////////////////////////

void
WebGLContext::BindBuffer(GLenum target, WebGLBuffer* buffer)
{
    const char funcName[] = "bindBuffer";
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull(funcName, buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    const auto& slot = ValidateBufferSlot(funcName, target);
    if (!slot)
        return;

    if (buffer && !buffer->ValidateCanBindToTarget(funcName, target))
        return;

    gl->MakeCurrent();
    gl->fBindBuffer(target, buffer ? buffer->mGLName : 0);

    *slot = buffer;
    if (buffer) {
        buffer->SetContentAfterBind(target);
    }
}

////////////////////////////////////////

bool
WebGLContext::ValidateIndexedBufferBinding(const char* funcName, GLenum target,
                                           GLuint index,
                                           WebGLRefPtr<WebGLBuffer>** const out_genericBinding,
                                           IndexedBufferBinding** const out_indexedBinding)
{
    *out_genericBinding = ValidateBufferSlot(funcName, target);
    if (!*out_genericBinding)
        return false;

    *out_indexedBinding = ValidateIndexedBufferSlot(funcName, target, index);
    if (!*out_indexedBinding)
        return false;

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER &&
        mBoundTransformFeedback->mIsActive)
    {
        ErrorInvalidOperation("%s: Cannot update indexed buffer bindings on active"
                              " transform feedback objects.",
                              funcName);
        return false;
    }

    return true;
}

void
WebGLContext::BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer)
{
    const char funcName[] = "bindBufferBase";
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull(funcName, buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    WebGLRefPtr<WebGLBuffer>* genericBinding;
    IndexedBufferBinding* indexedBinding;
    if (!ValidateIndexedBufferBinding(funcName, target, index, &genericBinding,
                                      &indexedBinding))
    {
        return;
    }

    if (buffer && !buffer->ValidateCanBindToTarget(funcName, target))
        return;

    ////

    gl->MakeCurrent();
    gl->fBindBufferBase(target, index, buffer ? buffer->mGLName : 0);

    ////

    *genericBinding = buffer;
    indexedBinding->mBufferBinding = buffer;
    indexedBinding->mRangeStart = 0;
    indexedBinding->mRangeSize = 0;

    if (buffer) {
        buffer->SetContentAfterBind(target);
    }
}

void
WebGLContext::BindBufferRange(GLenum target, GLuint index, WebGLBuffer* buffer,
                              WebGLintptr offset, WebGLsizeiptr size)
{
    const char funcName[] = "bindBufferRange";
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull(funcName, buffer))
        return;

    // silently ignore a deleted buffer
    if (buffer && buffer->IsDeleted())
        return;

    if (!ValidateNonNegative(funcName, "offset", offset) ||
        !ValidateNonNegative(funcName, "size", size))
    {
        return;
    }

    WebGLRefPtr<WebGLBuffer>* genericBinding;
    IndexedBufferBinding* indexedBinding;
    if (!ValidateIndexedBufferBinding(funcName, target, index, &genericBinding,
                                      &indexedBinding))
    {
        return;
    }

    if (buffer && !buffer->ValidateCanBindToTarget(funcName, target))
        return;

    ////

    gl->MakeCurrent();

    switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        if (offset % 4 != 0 || size % 4 != 0) {
            ErrorInvalidValue("%s: For %s, `offset` and `size` must be multiples of 4.",
                              funcName, "TRANSFORM_FEEDBACK_BUFFER");
            return;
        }
        break;

    case LOCAL_GL_UNIFORM_BUFFER:
        {
            GLuint offsetAlignment = 0;
            gl->GetUIntegerv(LOCAL_GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offsetAlignment);
            if (offset % offsetAlignment != 0) {
                ErrorInvalidValue("%s: For %s, `offset` must be a multiple of %s.",
                                  funcName, "UNIFORM_BUFFER",
                                  "UNIFORM_BUFFER_OFFSET_ALIGNMENT");
                return;
            }
        }
        break;
    }

    ////

#ifdef XP_MACOSX
    if (buffer && buffer->Content() == WebGLBuffer::Kind::Undefined &&
        gl->WorkAroundDriverBugs())
    {
        // BindBufferRange will fail if the buffer's contents is undefined.
        // Bind so driver initializes the buffer.
        gl->fBindBuffer(target, buffer->mGLName);
    }
#endif

    gl->fBindBufferRange(target, index, buffer ? buffer->mGLName : 0, offset, size);

    ////

    *genericBinding = buffer;
    indexedBinding->mBufferBinding = buffer;
    indexedBinding->mRangeStart = offset;
    indexedBinding->mRangeSize = size;

    if (buffer) {
        buffer->SetContentAfterBind(target);
    }
}

////////////////////////////////////////

void
WebGLContext::BufferData(GLenum target, WebGLsizeiptr size, GLenum usage)
{
    const char funcName[] = "bufferData";
    if (IsContextLost())
        return;

    if (!ValidateNonNegative(funcName, "size", size))
        return;

    // careful: WebGLsizeiptr is always 64-bit, but GLsizeiptr is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(size).isValid())
        return ErrorOutOfMemory("%s: bad size", funcName);

    const auto& buffer = ValidateBufferSelection(funcName, target);
    if (!buffer)
        return;

    ////

    UniquePtr<uint8_t> zeroBuffer((uint8_t*)calloc(size, 1));
    if (!zeroBuffer)
        return ErrorOutOfMemory("%s: Failed to allocate zeros.", funcName);

    buffer->BufferData(target, size_t(size), zeroBuffer.get(), usage);
}

// BufferT may be one of
// const dom::ArrayBuffer&
// const dom::SharedArrayBuffer&
// const dom::ArrayBufferView&
template<typename BufferT>
void
WebGLContext::BufferDataT(GLenum target,
                          const BufferT& data,
                          GLenum usage)
{
    const char funcName[] = "bufferData";
    if (IsContextLost())
        return;

    data.ComputeLengthAndData();

    // Careful: data.Length() could conceivably be any uint32_t, but GLsizeiptr
    // is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(data.LengthAllowShared()).isValid())
        return ErrorOutOfMemory("bufferData: bad size");

    const auto& buffer = ValidateBufferSelection(funcName, target);
    if (!buffer)
        return;

    // Warning: Possibly shared memory.  See bug 1225033.
    buffer->BufferData(target, data.LengthAllowShared(), data.DataAllowShared(), usage);
}

void
WebGLContext::BufferData(GLenum target,
                         const dom::SharedArrayBuffer& data,
                         GLenum usage)
{
    BufferDataT(target, data, usage);
}

void
WebGLContext::BufferData(GLenum target,
                         const dom::Nullable<dom::ArrayBuffer>& maybeData,
                         GLenum usage)
{
    if (maybeData.IsNull()) {
        // see http://www.khronos.org/bugzilla/show_bug.cgi?id=386
        return ErrorInvalidValue("bufferData: null object passed");
    }
    BufferDataT(target, maybeData.Value(), usage);
}

void
WebGLContext::BufferData(GLenum target, const dom::ArrayBufferView& data,
                         GLenum usage)
{
    BufferDataT(target, data, usage);
}

////////////////////////////////////////

// BufferT may be one of
// const dom::ArrayBuffer&
// const dom::SharedArrayBuffer&
// const dom::ArrayBufferView&
template<typename BufferT>
void
WebGLContext::BufferSubDataT(GLenum target,
                             WebGLsizeiptr byteOffset,
                             const BufferT& data)
{
    const char funcName[] = "bufferSubData";
    if (IsContextLost())
        return;

    if (!ValidateNonNegative(funcName, "byteOffset", byteOffset))
        return;

    const auto& buffer = ValidateBufferSelection(funcName, target);
    if (!buffer)
        return;

    if (buffer->mNumActiveTFOs) {
        ErrorInvalidOperation("%s: Buffer is bound to an active transform feedback"
                              " object.",
                              "bufferSubData");
        return;
    }

    data.ComputeLengthAndData();

    const auto checked_neededByteLength =
        CheckedInt<size_t>(byteOffset) + data.LengthAllowShared();

    if (!checked_neededByteLength.isValid()) {
        ErrorInvalidValue("bufferSubData: Integer overflow computing the needed"
                          " byte length.");
        return;
    }

    if (checked_neededByteLength.value() > buffer->ByteLength()) {
        ErrorInvalidValue("bufferSubData: Not enough data. Operation requires"
                          " %d bytes, but buffer only has %d bytes.",
                          checked_neededByteLength.value(),
                          buffer->ByteLength());
        return;
    }

    MakeContextCurrent();
    // Warning: Possibly shared memory.  See bug 1225033.
    gl->fBufferSubData(target, byteOffset, data.LengthAllowShared(),
                       data.DataAllowShared());

    // Warning: Possibly shared memory.  See bug 1225033.
    buffer->ElementArrayCacheBufferSubData(byteOffset, data.DataAllowShared(),
                                           data.LengthAllowShared());
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const dom::Nullable<dom::ArrayBuffer>& maybeData)
{
    if (maybeData.IsNull()) {
        ErrorInvalidValue("BufferSubData: returnedData is null.");
        return;
    }
    BufferSubDataT(target, byteOffset, maybeData.Value());
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const dom::SharedArrayBuffer& data)
{
    BufferSubDataT(target, byteOffset, data);
}

void
WebGLContext::BufferSubData(GLenum target, WebGLsizeiptr byteOffset,
                            const dom::ArrayBufferView& data)
{
    BufferSubDataT(target, byteOffset, data);
}

////////////////////////////////////////

already_AddRefed<WebGLBuffer>
WebGLContext::CreateBuffer()
{
    if (IsContextLost())
        return nullptr;

    GLuint buf = 0;
    MakeContextCurrent();
    gl->fGenBuffers(1, &buf);

    RefPtr<WebGLBuffer> globj = new WebGLBuffer(this, buf);
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

    ////

    const auto fnClearIfBuffer = [&](WebGLRefPtr<WebGLBuffer>& bindPoint) {
        if (bindPoint == buffer) {
            bindPoint = nullptr;
        }
    };

    fnClearIfBuffer(mBoundArrayBuffer);
    fnClearIfBuffer(mBoundVertexArray->mElementArrayBuffer);

    // WebGL binding points
    if (IsWebGL2()) {
        fnClearIfBuffer(mBoundCopyReadBuffer);
        fnClearIfBuffer(mBoundCopyWriteBuffer);
        fnClearIfBuffer(mBoundPixelPackBuffer);
        fnClearIfBuffer(mBoundPixelUnpackBuffer);
        fnClearIfBuffer(mBoundUniformBuffer);
        fnClearIfBuffer(mBoundTransformFeedback->mGenericBufferBinding);

        for (auto& binding : mBoundTransformFeedback->mIndexedBindings) {
            fnClearIfBuffer(binding.mBufferBinding);
        }

        for (auto& binding : mIndexedUniformBufferBindings) {
            fnClearIfBuffer(binding.mBufferBinding);
        }
    }

    for (int32_t i = 0; i < mGLMaxVertexAttribs; i++) {
        if (mBoundVertexArray->HasAttrib(i)) {
            fnClearIfBuffer(mBoundVertexArray->mAttribs[i].buf);
        }
    }

    ////

    buffer->RequestDelete();
}

bool
WebGLContext::IsBuffer(WebGLBuffer* buffer)
{
    if (IsContextLost())
        return false;

    if (!ValidateObjectAllowDeleted("isBuffer", buffer))
        return false;

    if (buffer->IsDeleted())
        return false;

    MakeContextCurrent();
    return gl->fIsBuffer(buffer->mGLName);
}

} // namespace mozilla
