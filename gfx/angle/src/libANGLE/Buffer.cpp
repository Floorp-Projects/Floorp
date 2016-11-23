//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libANGLE/Buffer.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{

BufferState::BufferState()
    : mLabel(),
      mUsage(GL_STATIC_DRAW),
      mSize(0),
      mAccessFlags(0),
      mAccess(GL_WRITE_ONLY_OES),
      mMapped(GL_FALSE),
      mMapPointer(nullptr),
      mMapOffset(0),
      mMapLength(0)
{
}

BufferState::~BufferState()
{
}

Buffer::Buffer(rx::GLImplFactory *factory, GLuint id)
    : RefCountObject(id), mImpl(factory->createBuffer(mState))
{
}

Buffer::~Buffer()
{
    SafeDelete(mImpl);
}

void Buffer::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Buffer::getLabel() const
{
    return mState.mLabel;
}

Error Buffer::bufferData(GLenum target, const void *data, GLsizeiptr size, GLenum usage)
{
    ANGLE_TRY(mImpl->setData(target, data, size, usage));

    mIndexRangeCache.clear();
    mState.mUsage = usage;
    mState.mSize  = size;

    return NoError();
}

Error Buffer::bufferSubData(GLenum target, const void *data, GLsizeiptr size, GLintptr offset)
{
    ANGLE_TRY(mImpl->setSubData(target, data, size, offset));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset), static_cast<unsigned int>(size));

    return NoError();
}

Error Buffer::copyBufferSubData(Buffer* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    ANGLE_TRY(mImpl->copySubData(source->getImplementation(), sourceOffset, destOffset, size));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(destOffset), static_cast<unsigned int>(size));

    return NoError();
}

Error Buffer::map(GLenum access)
{
    ASSERT(!mState.mMapped);

    Error error = mImpl->map(access, &mState.mMapPointer);
    if (error.isError())
    {
        mState.mMapPointer = nullptr;
        return error;
    }

    ASSERT(access == GL_WRITE_ONLY_OES);

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = 0;
    mState.mMapLength   = mState.mSize;
    mState.mAccess      = access;
    mState.mAccessFlags = GL_MAP_WRITE_BIT;
    mIndexRangeCache.clear();

    return error;
}

Error Buffer::mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    ASSERT(!mState.mMapped);
    ASSERT(offset + length <= mState.mSize);

    Error error = mImpl->mapRange(offset, length, access, &mState.mMapPointer);
    if (error.isError())
    {
        mState.mMapPointer = nullptr;
        return error;
    }

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = static_cast<GLint64>(offset);
    mState.mMapLength   = static_cast<GLint64>(length);
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = access;

    // The OES_mapbuffer extension states that GL_WRITE_ONLY_OES is the only valid
    // value for GL_BUFFER_ACCESS_OES because it was written against ES2.  Since there is
    // no update for ES3 and the GL_READ_ONLY and GL_READ_WRITE enums don't exist for ES,
    // we cannot properly set GL_BUFFER_ACCESS_OES when glMapBufferRange is called.

    if ((access & GL_MAP_WRITE_BIT) > 0)
    {
        mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset), static_cast<unsigned int>(length));
    }

    return error;
}

Error Buffer::unmap(GLboolean *result)
{
    ASSERT(mState.mMapped);

    Error error = mImpl->unmap(result);
    if (error.isError())
    {
        *result = GL_FALSE;
        return error;
    }

    mState.mMapped      = GL_FALSE;
    mState.mMapPointer  = nullptr;
    mState.mMapOffset   = 0;
    mState.mMapLength   = 0;
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = 0;

    return error;
}

void Buffer::onTransformFeedback()
{
    mIndexRangeCache.clear();
}

void Buffer::onPixelUnpack()
{
    mIndexRangeCache.clear();
}

Error Buffer::getIndexRange(GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            IndexRange *outRange) const
{
    if (mIndexRangeCache.findRange(type, offset, count, primitiveRestartEnabled, outRange))
    {
        return NoError();
    }

    ANGLE_TRY(mImpl->getIndexRange(type, offset, count, primitiveRestartEnabled, outRange));

    mIndexRangeCache.addRange(type, offset, count, primitiveRestartEnabled, *outRange);

    return NoError();
}

}  // namespace gl
