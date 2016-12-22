/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostWebRenderOGL.h"

#include "WebRenderCompositorOGL.h"

namespace mozilla {
namespace layers {

WebRenderCompositorOGL* AssertWebRenderCompositorOGL(Compositor* aCompositor)
{
  WebRenderCompositorOGL* compositor =
      aCompositor ? aCompositor->AsWebRenderCompositorOGL() : nullptr;
  MOZ_ASSERT(!!compositor);

  return compositor;
}

bool
TextureImageTextureSourceWebRenderOGL::Update(gfx::DataSourceSurface* aSurface,
                                              nsIntRegion* aDestRegion,
                                              gfx::IntPoint* aSrcOffset)
{
  GLContext *gl = mCompositor->gl();
  MOZ_ASSERT(gl);
  if (!gl || !gl->MakeCurrent()) {
    NS_WARNING("trying to update TextureImageTextureSourceWebRenderOGL without a GLContext");
    return false;
  }
  if (!aSurface) {
    gfxCriticalError() << "Invalid surface for WebRenderOGL update";
    return false;
  }
  MOZ_ASSERT(aSurface);

  IntSize size = aSurface->GetSize();
  if (!mTexImage ||
      (mTexImage->GetSize() != size && !aSrcOffset) ||
      mTexImage->GetContentType() != gfx::ContentForFormat(aSurface->GetFormat())) {
    if (mFlags & TextureFlags::DISALLOW_BIGIMAGE) {
      GLint maxTextureSize;
      gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &maxTextureSize);
      if (size.width > maxTextureSize || size.height > maxTextureSize) {
        NS_WARNING("Texture exceeds maximum texture size, refusing upload");
        return false;
      }
      // Explicitly use CreateBasicTextureImage instead of CreateTextureImage,
      // because CreateTextureImage might still choose to create a tiled
      // texture image.
      mTexImage = CreateBasicTextureImage(gl, size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          LOCAL_GL_CLAMP_TO_EDGE,
                                          FlagsToGLFlags(mFlags));
    } else {
      // XXX - clarify which size we want to use. IncrementalContentHost will
      // require the size of the destination surface to be different from
      // the size of aSurface.
      // See bug 893300 (tracks the implementation of ContentHost for new textures).
      mTexImage = CreateTextureImage(gl,
                                     size,
                                     gfx::ContentForFormat(aSurface->GetFormat()),
                                     LOCAL_GL_CLAMP_TO_EDGE,
                                     FlagsToGLFlags(mFlags),
                                     SurfaceFormatToImageFormat(aSurface->GetFormat()));
    }

    if (aDestRegion &&
        !aSrcOffset &&
        !aDestRegion->IsEqual(gfx::IntRect(0, 0, size.width, size.height))) {
      // UpdateFromDataSource will ignore our specified aDestRegion since the texture
      // hasn't been allocated with glTexImage2D yet. Call Resize() to force the
      // allocation (full size, but no upload), and then we'll only upload the pixels
      // we care about below.
      mTexImage->Resize(size);
    }
  }

  mTexImage->UpdateFromDataSource(aSurface, aDestRegion, aSrcOffset);

  return true;
}

void
TextureImageTextureSourceWebRenderOGL::SetCompositor(Compositor* aCompositor)
{
  WebRenderCompositorOGL* glCompositor = AssertWebRenderCompositorOGL(aCompositor);
  if (!glCompositor) {
    DeallocateDeviceData();
    return;
  }
  if (mCompositor != glCompositor) {
    DeallocateDeviceData();
    mCompositor = glCompositor;
  }
}

gfx::IntSize
TextureImageTextureSourceWebRenderOGL::GetSize() const
{
  if (mTexImage) {
    if (mIterating) {
      return mTexImage->GetTileRect().Size();
    }
    return mTexImage->GetSize();
  }
  NS_WARNING("Trying to query the size of an empty TextureSource.");
  return gfx::IntSize(0, 0);
}

gfx::SurfaceFormat
TextureImageTextureSourceWebRenderOGL::GetFormat() const
{
  if (mTexImage) {
    return mTexImage->GetTextureFormat();
  }
  NS_WARNING("Trying to query the format of an empty TextureSource.");
  return gfx::SurfaceFormat::UNKNOWN;
}

gfx::IntRect
TextureImageTextureSourceWebRenderOGL::GetTileRect()
{
  return mTexImage->GetTileRect();
}

} // namespace layers
} // namespace mozilla
