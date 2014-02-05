/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostX11.h"
#include "mozilla/layers/BasicCompositor.h"
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

static inline SurfaceFormat
ContentTypeToSurfaceFormat(gfxContentType type)
{
  switch (type) {
    case gfxContentType::COLOR:
      return SurfaceFormat::B8G8R8X8;
    case gfxContentType::ALPHA:
      return SurfaceFormat::A8;
    case gfxContentType::COLOR_ALPHA:
      return SurfaceFormat::B8G8R8A8;
    default:
      return SurfaceFormat::UNKNOWN;
  }
}

TextureSourceX11::TextureSourceX11(BasicCompositor* aCompositor, gfxXlibSurface* aSurface)
  : mCompositor(aCompositor),
    mSurface(aSurface)
{
}

IntSize
TextureSourceX11::GetSize() const
{
  return ToIntSize(mSurface->GetSize());
}

SurfaceFormat
TextureSourceX11::GetFormat() const
{
  return ContentTypeToSurfaceFormat(mSurface->GetContentType());
}

SourceSurface*
TextureSourceX11::GetSurface()
{
  if (!mSourceSurface) {
    mSourceSurface =
      Factory::CreateSourceSurfaceForCairoSurface(mSurface->CairoSurface(), GetFormat());
  }
  return mSourceSurface;
}

void
TextureSourceX11::SetCompositor(Compositor* aCompositor)
{
  BasicCompositor* compositor = static_cast<BasicCompositor*>(aCompositor);
  mCompositor = compositor;
}

TextureHostX11::TextureHostX11(TextureFlags aFlags,
                               const SurfaceDescriptorX11& aDescriptor)
 : TextureHost(aFlags)
{
  nsRefPtr<gfxXlibSurface> surface = aDescriptor.OpenForeign();
  mSurface = surface.get();

  // The host always frees the pixmap.
  MOZ_ASSERT(!(aFlags & TEXTURE_DEALLOCATE_CLIENT));
  mSurface->TakePixmap();
}

bool
TextureHostX11::Lock()
{
  if (!mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = new TextureSourceX11(mCompositor, mSurface);
  }

  return true;
}

void
TextureHostX11::SetCompositor(Compositor* aCompositor)
{
  BasicCompositor* compositor = static_cast<BasicCompositor*>(aCompositor);
  mCompositor = compositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(compositor);
  }
}

SurfaceFormat
TextureHostX11::GetFormat() const
{
  return ContentTypeToSurfaceFormat(mSurface->GetContentType());
}

IntSize
TextureHostX11::GetSize() const
{
  return ToIntSize(mSurface->GetSize());
}
