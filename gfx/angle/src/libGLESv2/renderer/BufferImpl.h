//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl.h: Defines the abstract rx::BufferImpl class.

#ifndef LIBGLESV2_RENDERER_BUFFERIMPL_H_
#define LIBGLESV2_RENDERER_BUFFERIMPL_H_

#include "common/angleutils.h"
#include "libGLESv2/Buffer.h"

namespace rx
{

class BufferImpl
{
  public:
    virtual void setData(const void* data, size_t size, GLenum usage) = 0;
    virtual void *getData() = 0;
    virtual void setSubData(const void* data, size_t size, size_t offset) = 0;
    virtual void copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size) = 0;
    virtual GLvoid* map(size_t offset, size_t length, GLbitfield access) = 0;
    virtual void unmap() = 0;
    virtual void markTransformFeedbackUsage() = 0;
};

}

#endif // LIBGLESV2_RENDERER_BUFFERIMPL_H_
