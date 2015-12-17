//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferD3D.cpp Defines common functionality between the Buffer9 and Buffer11 classes.

#include "libANGLE/renderer/d3d/BufferD3D.h"

#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/renderer/d3d/IndexBuffer.h"
#include "libANGLE/renderer/d3d/VertexBuffer.h"

namespace rx
{

unsigned int BufferD3D::mNextSerial = 1;

BufferD3D::BufferD3D(BufferFactoryD3D *factory)
    : BufferImpl(),
      mFactory(factory),
      mStaticVertexBuffer(nullptr),
      mStaticIndexBuffer(nullptr),
      mStaticBufferCache(nullptr),
      mStaticBufferCacheTotalSize(0),
      mStaticVertexBufferOutOfDate(false),
      mUnmodifiedDataUse(0),
      mUsage(D3D_BUFFER_USAGE_STATIC)
{
    updateSerial();
}

BufferD3D::~BufferD3D()
{
    SafeDelete(mStaticVertexBuffer);
    SafeDelete(mStaticIndexBuffer);

    emptyStaticBufferCache();
}

void BufferD3D::emptyStaticBufferCache()
{
    if (mStaticBufferCache != nullptr)
    {
        SafeDeleteContainer(*mStaticBufferCache);
        SafeDelete(mStaticBufferCache);
    }

    mStaticBufferCacheTotalSize = 0;
}

void BufferD3D::updateSerial()
{
    mSerial = mNextSerial++;
}

void BufferD3D::updateD3DBufferUsage(GLenum usage)
{
    switch (usage)
    {
        case GL_STATIC_DRAW:
        case GL_STATIC_READ:
        case GL_STATIC_COPY:
            mUsage = D3D_BUFFER_USAGE_STATIC;
            initializeStaticData();
            break;

        case GL_STREAM_DRAW:
        case GL_STREAM_READ:
        case GL_STREAM_COPY:
        case GL_DYNAMIC_READ:
        case GL_DYNAMIC_COPY:
        case GL_DYNAMIC_DRAW:
            mUsage = D3D_BUFFER_USAGE_DYNAMIC;
            break;
        default:
            UNREACHABLE();
    }
}

void BufferD3D::initializeStaticData()
{
    if (!mStaticVertexBuffer)
    {
        mStaticVertexBuffer = new StaticVertexBufferInterface(mFactory);
    }
    if (!mStaticIndexBuffer)
    {
        mStaticIndexBuffer = new StaticIndexBufferInterface(mFactory);
    }
}

StaticIndexBufferInterface *BufferD3D::getStaticIndexBuffer()
{
    return mStaticIndexBuffer;
}

StaticVertexBufferInterface *BufferD3D::getStaticVertexBuffer(
    const gl::VertexAttribute &attribute,
    D3DStaticBufferCreationType creationType)
{
    if (!mStaticVertexBuffer)
    {
        // Early out if there aren't any static buffers at all
        ASSERT(mStaticBufferCache == nullptr);
        return nullptr;
    }

    if (mStaticBufferCache == nullptr && !mStaticVertexBuffer->isCommitted())
    {
        // Early out, the attribute can be added to mStaticVertexBuffer or is already in there
        return mStaticVertexBuffer;
    }

    // At this point, see if any of the existing static buffers contains the attribute data

    // If the default static vertex buffer contains the attribute, then return it
    if (mStaticVertexBuffer->lookupAttribute(attribute, nullptr))
    {
        return mStaticVertexBuffer;
    }

    if (mStaticBufferCache != nullptr)
    {
        // If there is a cached static buffer that already contains the attribute, then return it
        for (StaticVertexBufferInterface *staticBuffer : *mStaticBufferCache)
        {
            if (staticBuffer->lookupAttribute(attribute, nullptr))
            {
                return staticBuffer;
            }
        }
    }

    if (!mStaticVertexBuffer->isCommitted())
    {
        // None of the existing static buffers contain the attribute data and we are able to add
        // the data to mStaticVertexBuffer, so we should just do so
        return mStaticVertexBuffer;
    }

    // At this point, we must create a new static buffer for the attribute data
    if (creationType != D3D_BUFFER_CREATE_IF_NECESSARY)
    {
        return nullptr;
    }

    ASSERT(mStaticVertexBuffer);
    ASSERT(mStaticVertexBuffer->isCommitted());
    unsigned int staticVertexBufferSize = mStaticVertexBuffer->getBufferSize();
    if (IsUnsignedAdditionSafe(staticVertexBufferSize, mStaticBufferCacheTotalSize))
    {
        // Ensure that the total size of the static buffer cache remains less than 4x the
        // size of the original buffer
        unsigned int maxStaticCacheSize =
            IsUnsignedMultiplicationSafe(static_cast<unsigned int>(getSize()), 4u)
                ? 4u * static_cast<unsigned int>(getSize())
                : std::numeric_limits<unsigned int>::max();

        // We can't reuse the default static vertex buffer, so we add it to the cache
        if (staticVertexBufferSize + mStaticBufferCacheTotalSize <= maxStaticCacheSize)
        {
            if (mStaticBufferCache == nullptr)
            {
                mStaticBufferCache = new std::vector<StaticVertexBufferInterface *>();
            }

            mStaticBufferCacheTotalSize += staticVertexBufferSize;
            (*mStaticBufferCache).push_back(mStaticVertexBuffer);
            mStaticVertexBuffer = nullptr;

            // Then reinitialize the static buffers to create a new static vertex buffer
            initializeStaticData();

            // Return the default static vertex buffer
            return mStaticVertexBuffer;
        }
    }

    // At this point:
    //   - mStaticVertexBuffer is committed and can't be altered
    //   - mStaticBufferCache is full (or nearly overflowing)
    // The inputted attribute should be put in some static buffer at some point, but it can't
    // go in one right now, since mStaticBufferCache is full and we can't delete mStaticVertexBuffer
    // in case another attribute is relying upon it for the current draw.
    // We therefore mark mStaticVertexBuffer for deletion at the next possible time.
    mStaticVertexBufferOutOfDate = true;
    return nullptr;
}

void BufferD3D::reinitOutOfDateStaticData()
{
    if (mStaticVertexBufferOutOfDate)
    {
        // During the last draw the caller tried to use some attribute with static data, but neither
        // the static buffer cache nor mStaticVertexBuffer contained that data.
        // Therefore, invalidate mStaticVertexBuffer so that if the caller tries to use that
        // attribute in the next draw, it'll successfully get put into mStaticVertexBuffer.
        invalidateStaticData(D3D_BUFFER_INVALIDATE_DEFAULT_BUFFER_ONLY);
        mStaticVertexBufferOutOfDate = false;
    }
}

void BufferD3D::invalidateStaticData(D3DBufferInvalidationType invalidationType)
{
    if (invalidationType == D3D_BUFFER_INVALIDATE_WHOLE_CACHE && mStaticBufferCache != nullptr)
    {
        emptyStaticBufferCache();
    }

    if ((mStaticVertexBuffer && mStaticVertexBuffer->getBufferSize() != 0) || (mStaticIndexBuffer && mStaticIndexBuffer->getBufferSize() != 0))
    {
        SafeDelete(mStaticVertexBuffer);
        SafeDelete(mStaticIndexBuffer);

        // If the buffer was created with a static usage then we recreate the static
        // buffers so that they are populated the next time we use this buffer.
        if (mUsage == D3D_BUFFER_USAGE_STATIC)
        {
            initializeStaticData();
        }
    }

    mUnmodifiedDataUse = 0;
}

// Creates static buffers if sufficient used data has been left unmodified
void BufferD3D::promoteStaticUsage(int dataSize)
{
    if (!mStaticVertexBuffer && !mStaticIndexBuffer)
    {
        // There isn't any scenario that involves promoting static usage and the static buffer cache
        // being non-empty
        ASSERT(mStaticBufferCache == nullptr);

        mUnmodifiedDataUse += dataSize;

        if (mUnmodifiedDataUse > 3 * getSize())
        {
            initializeStaticData();
        }
    }
}

gl::Error BufferD3D::getIndexRange(GLenum type,
                                   size_t offset,
                                   size_t count,
                                   bool primitiveRestartEnabled,
                                   gl::IndexRange *outRange)
{
    const uint8_t *data = nullptr;
    gl::Error error = getData(&data);
    if (error.isError())
    {
        return error;
    }

    *outRange = gl::ComputeIndexRange(type, data + offset, count, primitiveRestartEnabled);
    return gl::Error(GL_NO_ERROR);
}

}
