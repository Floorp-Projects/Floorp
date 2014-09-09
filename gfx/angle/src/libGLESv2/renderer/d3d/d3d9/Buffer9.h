//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer9.h: Defines the rx::Buffer9 class which implements rx::BufferImpl via rx::BufferD3D.

#ifndef LIBGLESV2_RENDERER_BUFFER9_H_
#define LIBGLESV2_RENDERER_BUFFER9_H_

#include "libGLESv2/renderer/d3d/BufferD3D.h"
#include "libGLESv2/angletypes.h"

namespace rx
{
class Renderer9;

class Buffer9 : public BufferD3D
{
  public:
    Buffer9(rx::Renderer9 *renderer);
    virtual ~Buffer9();

    static Buffer9 *makeBuffer9(BufferImpl *buffer);

    // BufferD3D implementation
    virtual size_t getSize() const { return mSize; }
    virtual void clear();
    virtual bool supportsDirectBinding() const { return false; }
    virtual Renderer* getRenderer();

    // BufferImpl implementation
    virtual void setData(const void* data, size_t size, GLenum usage);
    virtual void *getData();
    virtual void setSubData(const void* data, size_t size, size_t offset);
    virtual void copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size);
    virtual GLvoid* map(size_t offset, size_t length, GLbitfield access);
    virtual void unmap();
    virtual void markTransformFeedbackUsage();

  private:
    DISALLOW_COPY_AND_ASSIGN(Buffer9);

    rx::Renderer9 *mRenderer;
    std::vector<char> mMemory;
    size_t mSize;
};

}

#endif // LIBGLESV2_RENDERER_BUFFER9_H_
