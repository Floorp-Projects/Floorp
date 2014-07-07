#include "precompiled.h"
//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferStorage9.cpp Defines the BufferStorage9 class.

#include "libGLESv2/renderer/d3d9/BufferStorage9.h"
#include "common/debug.h"
#include "libGLESv2/main.h"

namespace rx
{

BufferStorage9::BufferStorage9()
    : mSize(0)
{
}

BufferStorage9::~BufferStorage9()
{
}

BufferStorage9 *BufferStorage9::makeBufferStorage9(BufferStorage *bufferStorage)
{
    ASSERT(HAS_DYNAMIC_TYPE(BufferStorage9*, bufferStorage));
    return static_cast<BufferStorage9*>(bufferStorage);
}

void *BufferStorage9::getData()
{
    return mMemory.data();
}

void BufferStorage9::setData(const void* data, size_t size, size_t offset)
{
    if (offset + size > mMemory.size())
    {
        mMemory.resize(offset + size);
    }

    mSize = std::max(mSize, offset + size);
    if (data)
    {
        memcpy(mMemory.data() + offset, data, size);
    }
}

void BufferStorage9::copyData(BufferStorage* sourceStorage, size_t size, size_t sourceOffset, size_t destOffset)
{
    BufferStorage9* source = makeBufferStorage9(sourceStorage);
    if (source)
    {
        memcpy(mMemory.data() + destOffset, source->mMemory.data() + sourceOffset, size);
    }
}

void BufferStorage9::clear()
{
    mSize = 0;
}

void BufferStorage9::markTransformFeedbackUsage()
{
    UNREACHABLE();
}

size_t BufferStorage9::getSize() const
{
    return mSize;
}

bool BufferStorage9::supportsDirectBinding() const
{
    return false;
}

// We do not suppot buffer mapping facility in D3D9
bool BufferStorage9::isMapped() const
{
    UNREACHABLE();
    return false;
}

void *BufferStorage9::map(GLbitfield access)
{
    UNREACHABLE();
    return NULL;
}

void BufferStorage9::unmap()
{
    UNREACHABLE();
}

}
