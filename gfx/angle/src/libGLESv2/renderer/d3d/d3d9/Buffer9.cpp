//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer9.cpp Defines the Buffer9 class.

#include "libGLESv2/renderer/d3d/d3d9/Buffer9.h"
#include "libGLESv2/renderer/d3d/d3d9/Renderer9.h"
#include "libGLESv2/main.h"

namespace rx
{

Buffer9::Buffer9(rx::Renderer9 *renderer)
    : BufferD3D(),
      mRenderer(renderer),
      mSize(0)
{}

Buffer9::~Buffer9()
{
    mSize = 0;
}

Buffer9 *Buffer9::makeBuffer9(BufferImpl *buffer)
{
    ASSERT(HAS_DYNAMIC_TYPE(Buffer9*, buffer));
    return static_cast<Buffer9*>(buffer);
}

void Buffer9::setData(const void* data, size_t size, GLenum usage)
{
    if (size > mMemory.size())
    {
        if (!mMemory.resize(size))
        {
            return gl::error(GL_OUT_OF_MEMORY);
        }
    }

    mSize = size;
    if (data)
    {
        memcpy(mMemory.data(), data, size);
    }

    invalidateStaticData();

    if (usage == GL_STATIC_DRAW)
    {
        initializeStaticData();
    }
}

void *Buffer9::getData()
{
    return mMemory.data();
}

void Buffer9::setSubData(const void* data, size_t size, size_t offset)
{
    if (offset + size > mMemory.size())
    {
        if (!mMemory.resize(offset + size))
        {
            return gl::error(GL_OUT_OF_MEMORY);
        }
    }

    mSize = std::max(mSize, offset + size);
    if (data)
    {
        memcpy(mMemory.data() + offset, data, size);
    }

    invalidateStaticData();
}

void Buffer9::copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    // Note: this method is currently unreachable
    Buffer9* sourceBuffer = makeBuffer9(source);
    if (sourceBuffer)
    {
        memcpy(mMemory.data() + destOffset, sourceBuffer->mMemory.data() + sourceOffset, size);
    }

    invalidateStaticData();
}

// We do not suppot buffer mapping in D3D9
GLvoid* Buffer9::map(size_t offset, size_t length, GLbitfield access)
{
    UNREACHABLE();
    return NULL;
}

void Buffer9::unmap()
{
    UNREACHABLE();
}

void Buffer9::markTransformFeedbackUsage()
{
    UNREACHABLE();
}

Renderer* Buffer9::getRenderer()
{
    return mRenderer;
}

}
