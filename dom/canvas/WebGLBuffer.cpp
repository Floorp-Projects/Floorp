/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLBuffer.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLElementArrayCache.h"

namespace mozilla {

WebGLBuffer::WebGLBuffer(WebGLContext* webgl, GLuint buf)
    : WebGLRefCountedObject(webgl)
    , mGLName(buf)
    , mContent(Kind::Undefined)
    , mUsage(LOCAL_GL_STATIC_DRAW)
    , mByteLength(0)
{
    mContext->mBuffers.insertBack(this);
}

WebGLBuffer::~WebGLBuffer()
{
    DeleteOnce();
}

void
WebGLBuffer::SetContentAfterBind(GLenum target)
{
    if (mContent != Kind::Undefined)
        return;

    switch (target) {
    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        mContent = Kind::ElementArray;
        if (!mCache) {
            mCache.reset(new WebGLElementArrayCache);
        }
        break;

    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
    case LOCAL_GL_COPY_READ_BUFFER:
    case LOCAL_GL_COPY_WRITE_BUFFER:
        mContent = Kind::OtherData;
        break;

    default:
        MOZ_CRASH("GFX: invalid target");
    }
}

void
WebGLBuffer::Delete()
{
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteBuffers(1, &mGLName);
    mByteLength = 0;
    mCache = nullptr;
    LinkedListElement<WebGLBuffer>::remove(); // remove from mContext->mBuffers
}

////////////////////////////////////////

static bool
ValidateBufferUsageEnum(WebGLContext* webgl, const char* funcName, GLenum usage)
{
    switch (usage) {
    case LOCAL_GL_STREAM_DRAW:
    case LOCAL_GL_STATIC_DRAW:
    case LOCAL_GL_DYNAMIC_DRAW:
        return true;

    case LOCAL_GL_DYNAMIC_COPY:
    case LOCAL_GL_DYNAMIC_READ:
    case LOCAL_GL_STATIC_COPY:
    case LOCAL_GL_STATIC_READ:
    case LOCAL_GL_STREAM_COPY:
    case LOCAL_GL_STREAM_READ:
        if (MOZ_LIKELY(webgl->IsWebGL2()))
            return true;
        break;

    default:
        break;
    }

    webgl->ErrorInvalidEnum("%s: Invalid `usage`: 0x%04x", funcName, usage);
    return false;
}

void
WebGLBuffer::BufferData(GLenum target, size_t size, const void* data, GLenum usage)
{
    const char funcName[] = "bufferData";

    // Careful: data.Length() could conceivably be any uint32_t, but GLsizeiptr
    // is like intptr_t.
    if (!CheckedInt<GLsizeiptr>(size).isValid())
        return mContext->ErrorOutOfMemory("%s: bad size", funcName);

    if (!ValidateBufferUsageEnum(mContext, funcName, usage))
        return;

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    const ScopedLazyBind lazyBind(gl, target, this);
    mContext->InvalidateBufferFetching();

#ifdef XP_MACOSX
    // bug 790879
    if (gl->WorkAroundDriverBugs() &&
        size > INT32_MAX)
    {
        mContext->ErrorOutOfMemory("%s: Allocation size too large.", funcName);
        return;
    }
#endif

    const bool sizeChanges = (size != ByteLength());
    if (sizeChanges) {
        gl::GLContext::LocalErrorScope errorScope(*gl);
        gl->fBufferData(target, size, data, usage);
        const auto error = errorScope.GetError();

        if (error) {
            MOZ_ASSERT(error == LOCAL_GL_OUT_OF_MEMORY);
            mContext->ErrorOutOfMemory("%s: Error from driver: 0x%04x", funcName, error);
            return;
        }
    } else {
        gl->fBufferData(target, size, data, usage);
    }

    mUsage = usage;
    mByteLength = size;

    // Warning: Possibly shared memory.  See bug 1225033.
    if (!ElementArrayCacheBufferData(data, size)) {
        mByteLength = 0;
        mContext->ErrorOutOfMemory("%s: Failed update index buffer cache.", funcName);
    }
}

bool
WebGLBuffer::ValidateRange(const char* funcName, size_t byteOffset, size_t byteLen) const
{
    auto availLength = mByteLength;
    if (byteOffset > availLength) {
        mContext->ErrorInvalidValue("%s: Offset passes the end of the buffer.", funcName);
        return false;
    }
    availLength -= byteOffset;

    if (byteLen > availLength) {
        mContext->ErrorInvalidValue("%s: Offset+size passes the end of the buffer.",
                                    funcName);
        return false;
    }

    return true;
}

////////////////////////////////////////

bool
WebGLBuffer::ElementArrayCacheBufferData(const void* ptr,
                                         size_t bufferSizeInBytes)
{
    if (mContent == Kind::ElementArray)
        return mCache->BufferData(ptr, bufferSizeInBytes);

    return true;
}

void
WebGLBuffer::ElementArrayCacheBufferSubData(size_t pos, const void* ptr,
                                            size_t updateSizeInBytes)
{
    if (mContent == Kind::ElementArray)
        mCache->BufferSubData(pos, ptr, updateSizeInBytes);
}

size_t
WebGLBuffer::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t sizeOfCache = mCache ? mCache->SizeOfIncludingThis(mallocSizeOf)
                                : 0;
    return mallocSizeOf(this) + sizeOfCache;
}

bool
WebGLBuffer::Validate(GLenum type, uint32_t maxAllowed, size_t first, size_t count) const
{
    return mCache->Validate(type, maxAllowed, first, count);
}

bool
WebGLBuffer::IsElementArrayUsedWithMultipleTypes() const
{
    return mCache->BeenUsedWithMultipleTypes();
}

bool
WebGLBuffer::ValidateCanBindToTarget(const char* funcName, GLenum target)
{
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

    if (mContent == WebGLBuffer::Kind::Undefined)
        return true;

    switch (target) {
    case LOCAL_GL_COPY_READ_BUFFER:
    case LOCAL_GL_COPY_WRITE_BUFFER:
        return true;

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        if (mContent == WebGLBuffer::Kind::ElementArray)
            return true;
        break;

    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
        if (mContent == WebGLBuffer::Kind::OtherData)
            return true;
        break;

    default:
        MOZ_CRASH();
    }

    const auto dataType = (mContent == WebGLBuffer::Kind::OtherData) ? "other"
                                                                     : "element";
    mContext->ErrorInvalidOperation("%s: Buffer already contains %s data.", funcName,
                                    dataType);
    return false;
}

JSObject*
WebGLBuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLBufferBinding::Wrap(cx, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLBuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLBuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLBuffer, Release)

} // namespace mozilla
