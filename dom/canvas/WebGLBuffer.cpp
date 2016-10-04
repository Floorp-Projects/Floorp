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
    : WebGLContextBoundObject(webgl)
    , mGLName(buf)
    , mContent(Kind::Undefined)
    , mByteLength(0)
{
    mContext->mBuffers.insertBack(this);
}

WebGLBuffer::~WebGLBuffer()
{
    DeleteOnce();
}

void
WebGLBuffer::BindTo(GLenum target)
{
    switch (target) {
    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
        mContent = Kind::ElementArray;
        if (!mCache)
            mCache = new WebGLElementArrayCache;
        break;

    case LOCAL_GL_ARRAY_BUFFER:
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
    case LOCAL_GL_UNIFORM_BUFFER:
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        mContent = Kind::OtherData;
        break;

    case LOCAL_GL_COPY_READ_BUFFER:
    case LOCAL_GL_COPY_WRITE_BUFFER:
        if (mContent == Kind::Undefined) {
          mContent = Kind::OtherData;
        }
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
WebGLBuffer::Validate(GLenum type, uint32_t maxAllowed, size_t first,
                      size_t count, uint32_t* const out_upperBound)
{
    return mCache->Validate(type, maxAllowed, first, count, out_upperBound);
}

bool
WebGLBuffer::IsElementArrayUsedWithMultipleTypes() const
{
    return mCache->BeenUsedWithMultipleTypes();
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
