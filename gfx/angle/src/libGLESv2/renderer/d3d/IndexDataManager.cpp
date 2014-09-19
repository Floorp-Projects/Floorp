//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexDataManager.cpp: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#include "libGLESv2/renderer/d3d/IndexDataManager.h"
#include "libGLESv2/renderer/d3d/BufferD3D.h"
#include "libGLESv2/renderer/d3d/IndexBuffer.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/main.h"
#include "libGLESv2/formatutils.h"

namespace rx
{

static void ConvertIndices(GLenum sourceType, GLenum destinationType, const void *input, GLsizei count, void *output)
{
    if (sourceType == GL_UNSIGNED_BYTE)
    {
        ASSERT(destinationType == GL_UNSIGNED_SHORT);
        const GLubyte *in = static_cast<const GLubyte*>(input);
        GLushort *out = static_cast<GLushort*>(output);

        for (GLsizei i = 0; i < count; i++)
        {
            out[i] = in[i];
        }
    }
    else if (sourceType == GL_UNSIGNED_INT)
    {
        ASSERT(destinationType == GL_UNSIGNED_INT);
        memcpy(output, input, count * sizeof(GLuint));
    }
    else if (sourceType == GL_UNSIGNED_SHORT)
    {
        if (destinationType == GL_UNSIGNED_SHORT)
        {
            memcpy(output, input, count * sizeof(GLushort));
        }
        else if (destinationType == GL_UNSIGNED_INT)
        {
            const GLushort *in = static_cast<const GLushort*>(input);
            GLuint *out = static_cast<GLuint*>(output);

            for (GLsizei i = 0; i < count; i++)
            {
                out[i] = in[i];
            }
        }
        else UNREACHABLE();
    }
    else UNREACHABLE();
}

IndexDataManager::IndexDataManager(Renderer *renderer)
    : mRenderer(renderer)
{
    mStreamingBufferShort = new StreamingIndexBufferInterface(mRenderer);
    if (!mStreamingBufferShort->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_SHORT))
    {
        SafeDelete(mStreamingBufferShort);
    }

    mStreamingBufferInt = new StreamingIndexBufferInterface(mRenderer);
    if (!mStreamingBufferInt->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_INT))
    {
        SafeDelete(mStreamingBufferInt);
    }

    if (!mStreamingBufferShort)
    {
        // Make sure both buffers are deleted.
        SafeDelete(mStreamingBufferInt);
        ERR("Failed to allocate the streaming index buffer(s).");
    }

    mCountingBuffer = NULL;
}

IndexDataManager::~IndexDataManager()
{
    SafeDelete(mStreamingBufferShort);
    SafeDelete(mStreamingBufferInt);
    SafeDelete(mCountingBuffer);
}

GLenum IndexDataManager::prepareIndexData(GLenum type, GLsizei count, gl::Buffer *buffer, const GLvoid *indices, TranslatedIndexData *translated)
{
    if (!mStreamingBufferShort)
    {
        return GL_OUT_OF_MEMORY;
    }

    const gl::Type &typeInfo = gl::GetTypeInfo(type);

    GLenum destinationIndexType = (type == GL_UNSIGNED_INT) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

    unsigned int offset = 0;
    bool alignedOffset = false;

    BufferD3D *storage = NULL;

    if (buffer != NULL)
    {
        if (reinterpret_cast<uintptr_t>(indices) > std::numeric_limits<unsigned int>::max())
        {
            return GL_OUT_OF_MEMORY;
        }
        offset = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(indices));

        storage = BufferD3D::makeBufferD3D(buffer->getImplementation());

        switch (type)
        {
          case GL_UNSIGNED_BYTE:  alignedOffset = (offset % sizeof(GLubyte) == 0);  break;
          case GL_UNSIGNED_SHORT: alignedOffset = (offset % sizeof(GLushort) == 0); break;
          case GL_UNSIGNED_INT:   alignedOffset = (offset % sizeof(GLuint) == 0);   break;
          default: UNREACHABLE(); alignedOffset = false;
        }

        ASSERT(typeInfo.bytes * static_cast<unsigned int>(count) + offset <= storage->getSize());

        indices = static_cast<const GLubyte*>(storage->getData()) + offset;
    }

    StaticIndexBufferInterface *staticBuffer = storage ? storage->getStaticIndexBuffer() : NULL;
    IndexBufferInterface *indexBuffer = NULL;
    bool directStorage = alignedOffset && storage && storage->supportsDirectBinding() &&
                         destinationIndexType == type;
    unsigned int streamOffset = 0;

    if (directStorage)
    {
        streamOffset = offset;

        if (!buffer->getIndexRangeCache()->findRange(type, offset, count, NULL, NULL))
        {
            buffer->getIndexRangeCache()->addRange(type, offset, count, translated->indexRange, offset);
        }
    }
    else if (staticBuffer && staticBuffer->getBufferSize() != 0 && staticBuffer->getIndexType() == type && alignedOffset)
    {
        indexBuffer = staticBuffer;

        if (!staticBuffer->getIndexRangeCache()->findRange(type, offset, count, NULL, &streamOffset))
        {
            streamOffset = (offset / typeInfo.bytes) * gl::GetTypeInfo(destinationIndexType).bytes;
            staticBuffer->getIndexRangeCache()->addRange(type, offset, count, translated->indexRange, streamOffset);
        }
    }

    // Avoid D3D11's primitive restart index value
    // see http://msdn.microsoft.com/en-us/library/windows/desktop/bb205124(v=vs.85).aspx
    if (translated->indexRange.end == 0xFFFF && type == GL_UNSIGNED_SHORT && mRenderer->getMajorShaderModel() > 3)
    {
        destinationIndexType = GL_UNSIGNED_INT;
        directStorage = false;
        indexBuffer = NULL;
    }

    const gl::Type &destTypeInfo = gl::GetTypeInfo(destinationIndexType);

    if (!directStorage && !indexBuffer)
    {
        indexBuffer = (destinationIndexType == GL_UNSIGNED_INT) ? mStreamingBufferInt : mStreamingBufferShort;

        unsigned int convertCount = count;

        if (staticBuffer)
        {
            if (staticBuffer->getBufferSize() == 0 && alignedOffset)
            {
                indexBuffer = staticBuffer;
                convertCount = storage->getSize() / typeInfo.bytes;
            }
            else
            {
                storage->invalidateStaticData();
                staticBuffer = NULL;
            }
        }

        ASSERT(indexBuffer);

        if (convertCount > std::numeric_limits<unsigned int>::max() / destTypeInfo.bytes)
        {
            ERR("Reserving %u indicies of %u bytes each exceeds the maximum buffer size.", convertCount, destTypeInfo.bytes);
            return GL_OUT_OF_MEMORY;
        }

        unsigned int bufferSizeRequired = convertCount * destTypeInfo.bytes;
        if (!indexBuffer->reserveBufferSpace(bufferSizeRequired, type))
        {
            ERR("Failed to reserve %u bytes in an index buffer.", bufferSizeRequired);
            return GL_OUT_OF_MEMORY;
        }

        void* output = NULL;
        if (!indexBuffer->mapBuffer(bufferSizeRequired, &output, &streamOffset))
        {
            ERR("Failed to map index buffer.");
            return GL_OUT_OF_MEMORY;
        }

        ConvertIndices(type, destinationIndexType, staticBuffer ? storage->getData() : indices, convertCount, output);

        if (!indexBuffer->unmapBuffer())
        {
            ERR("Failed to unmap index buffer.");
            return GL_OUT_OF_MEMORY;
        }

        if (staticBuffer)
        {
            streamOffset = (offset / typeInfo.bytes) * destTypeInfo.bytes;
            staticBuffer->getIndexRangeCache()->addRange(type, offset, count, translated->indexRange, streamOffset);
        }
    }

    translated->storage = directStorage ? storage : NULL;
    translated->indexBuffer = indexBuffer ? indexBuffer->getIndexBuffer() : NULL;
    translated->serial = directStorage ? storage->getSerial() : indexBuffer->getSerial();
    translated->startIndex = streamOffset / destTypeInfo.bytes;
    translated->startOffset = streamOffset;
    translated->indexType = destinationIndexType;

    if (storage)
    {
        storage->promoteStaticUsage(count * typeInfo.bytes);
    }

    return GL_NO_ERROR;
}

StaticIndexBufferInterface *IndexDataManager::getCountingIndices(GLsizei count)
{
    if (count <= 65536)   // 16-bit indices
    {
        const unsigned int spaceNeeded = count * sizeof(unsigned short);

        if (!mCountingBuffer || mCountingBuffer->getBufferSize() < spaceNeeded)
        {
            delete mCountingBuffer;
            mCountingBuffer = new StaticIndexBufferInterface(mRenderer);
            mCountingBuffer->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_SHORT);

            void* mappedMemory = NULL;
            if (!mCountingBuffer->mapBuffer(spaceNeeded, &mappedMemory, NULL))
            {
                ERR("Failed to map counting buffer.");
                return NULL;
            }

            unsigned short *data = reinterpret_cast<unsigned short*>(mappedMemory);
            for(int i = 0; i < count; i++)
            {
                data[i] = i;
            }

            if (!mCountingBuffer->unmapBuffer())
            {
                ERR("Failed to unmap counting buffer.");
                return NULL;
            }
        }
    }
    else if (mStreamingBufferInt)   // 32-bit indices supported
    {
        const unsigned int spaceNeeded = count * sizeof(unsigned int);

        if (!mCountingBuffer || mCountingBuffer->getBufferSize() < spaceNeeded)
        {
            delete mCountingBuffer;
            mCountingBuffer = new StaticIndexBufferInterface(mRenderer);
            mCountingBuffer->reserveBufferSpace(spaceNeeded, GL_UNSIGNED_INT);

            void* mappedMemory = NULL;
            if (!mCountingBuffer->mapBuffer(spaceNeeded, &mappedMemory, NULL))
            {
                ERR("Failed to map counting buffer.");
                return NULL;
            }

            unsigned int *data = reinterpret_cast<unsigned int*>(mappedMemory);
            for(int i = 0; i < count; i++)
            {
                data[i] = i;
            }

            if (!mCountingBuffer->unmapBuffer())
            {
                ERR("Failed to unmap counting buffer.");
                return NULL;
            }
        }
    }
    else
    {
        return NULL;
    }

    return mCountingBuffer;
}

}
