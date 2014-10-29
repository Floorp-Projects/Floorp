//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureStorage.h: Defines the abstract rx::TextureStorage class.

#ifndef LIBGLESV2_RENDERER_TEXTURESTORAGE_H_
#define LIBGLESV2_RENDERER_TEXTURESTORAGE_H_

#include "common/debug.h"
#include "libGLESv2/Error.h"

#include <GLES2/gl2.h>
#include <cstdint>

namespace gl
{
struct ImageIndex;
struct Box;
struct PixelUnpackState;
}

namespace rx
{
class Renderer;
class SwapChain;
class RenderTarget;

class TextureStorage
{
  public:
    TextureStorage();
    virtual ~TextureStorage() {};

    virtual int getTopLevel() const = 0;
    virtual bool isRenderTarget() const = 0;
    virtual bool isManaged() const = 0;
    virtual int getLevelCount() const = 0;

    virtual RenderTarget *getRenderTarget(const gl::ImageIndex &index) = 0;
    virtual void generateMipmap(const gl::ImageIndex &sourceIndex, const gl::ImageIndex &destIndex) = 0;

    virtual gl::Error copyToStorage(TextureStorage *destStorage) = 0;
    virtual gl::Error setData(const gl::ImageIndex &index, const gl::Box &sourceBox, GLenum internalFormat, GLenum type,
                              const gl::PixelUnpackState &unpack, const uint8_t *pixelData) = 0;

    unsigned int getRenderTargetSerial(const gl::ImageIndex &index) const;
    unsigned int getTextureSerial() const;

  protected:
    void initializeSerials(unsigned int rtSerialsToReserve, unsigned int rtSerialsLayerStride);

  private:
    DISALLOW_COPY_AND_ASSIGN(TextureStorage);

    const unsigned int mTextureSerial;
    static unsigned int issueTextureSerial();

    static unsigned int mCurrentTextureSerial;

    unsigned int mFirstRenderTargetSerial;
    unsigned int mRenderTargetSerialsLayerStride;
};

}

#endif // LIBGLESV2_RENDERER_TEXTURESTORAGE_H_
