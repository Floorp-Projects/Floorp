//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl.h: Defines the abstract rx::BufferImpl class.

#ifndef LIBGLESV2_RENDERER_BUFFERD3D_H_
#define LIBGLESV2_RENDERER_BUFFERD3D_H_

#include "libGLESv2/renderer/BufferImpl.h"
#include "libGLESv2/angletypes.h"
#include "libGLESv2/renderer/IndexRangeCache.h"

namespace rx
{

class Renderer;
class StaticIndexBufferInterface;
class StaticVertexBufferInterface;

class BufferD3D : public BufferImpl
{
  public:
    BufferD3D();
    virtual ~BufferD3D();

    static BufferD3D *makeBufferD3D(BufferImpl *buffer);

    unsigned int getSerial() const { return mSerial; }

    virtual size_t getSize() const = 0;
    virtual void clear() = 0;
    virtual bool supportsDirectBinding() const = 0;
    virtual Renderer* getRenderer() = 0;

    rx::StaticVertexBufferInterface *getStaticVertexBuffer() { return mStaticVertexBuffer; }
    rx::StaticIndexBufferInterface *getStaticIndexBuffer() { return mStaticIndexBuffer; }
    rx::IndexRangeCache *getIndexRangeCache() { return &mIndexRangeCache; }

    void initializeStaticData();
    void invalidateStaticData();
    void promoteStaticUsage(int dataSize);

  protected:
    unsigned int mSerial;
    static unsigned int mNextSerial;

    void updateSerial();

    rx::StaticVertexBufferInterface *mStaticVertexBuffer;
    rx::StaticIndexBufferInterface *mStaticIndexBuffer;
    rx::IndexRangeCache mIndexRangeCache;
    unsigned int mUnmodifiedDataUse;
};

}

#endif // LIBGLESV2_RENDERER_BUFFERIMPLD3D_H_
