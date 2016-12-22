/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOSTWEBRENDEROGL_H
#define MOZILLA_GFX_TEXTUREHOSTWEBRENDEROGL_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t
#include "GLContextTypes.h"             // for GLContext
#include "GLTextureImage.h"             // for TextureImage
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Point.h"          // for IntSize, IntPoint
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "nsRegionFwd.h"                // for nsIntRegion

namespace mozilla {
namespace layers {

class Compositor;
class TextureImageTextureSourceWebRenderOGL;

class TextureSourceWebRenderOGL
{
public:
  TextureSourceWebRenderOGL() {}
  virtual ~TextureSourceWebRenderOGL() {}

  virtual gl::TextureImage* GetTextureImage() = 0;

  virtual TextureImageTextureSourceWebRenderOGL* AsTextureImageTextureSource() { return nullptr; }
};

/**
 * A TextureSourceWebRenderOGL backed by a TextureImage.
 */
class TextureImageTextureSourceWebRenderOGL final : public DataTextureSource
                                                  , public TextureSourceWebRenderOGL
                                                  , public BigImageIterator
{
public:
  explicit TextureImageTextureSourceWebRenderOGL(WebRenderCompositorOGL *aCompositor,
                                                 TextureFlags aFlags = TextureFlags::DEFAULT)
    : mCompositor(aCompositor)
    , mFlags(aFlags)
    , mIterating(false)
  {
  }

  virtual const char* Name() const override
  {
    return "TextureImageTextureSourceWebRenderOGL";
  }

  virtual TextureImageTextureSourceWebRenderOGL* AsTextureImageTextureSource() override
  {
    return this;
  }

  virtual gl::TextureImage* GetTextureImage() override
  {
    return mTexImage.get();
  }

  // DataTextureSource
  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) override;
  //

  // TextureSource
  virtual void DeallocateDeviceData() override
  {
    mTexImage = nullptr;
    SetUpdateSerial(0);
  }

  virtual TextureSourceWebRenderOGL* AsSourceWebRenderOGL() override
  {
    return this;
  }

  virtual gfx::IntSize GetSize() const override;

  virtual gfx::SurfaceFormat GetFormat() const override;

  virtual void SetCompositor(Compositor* aCompositor) override;
  //

  // BigImageIterator
  virtual BigImageIterator* AsBigImageIterator() override { return this; }

  virtual void BeginBigImageIteration() override
  {
    mTexImage->BeginBigImageIteration();
    mIterating = true;
  }

  virtual void EndBigImageIteration() override
  {
    mIterating = false;
  }

  virtual gfx::IntRect GetTileRect() override;

  virtual size_t GetTileCount() override
  {
    return mTexImage->GetTileCount();
  }

  virtual bool NextTile() override
  {
    return mTexImage->NextTile();
  }
  //

private:
  RefPtr<gl::TextureImage> mTexImage;
  RefPtr<WebRenderCompositorOGL> mCompositor;
  TextureFlags mFlags;
  bool mIterating;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
