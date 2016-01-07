/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11TextureSourceBasic.h"
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

X11TextureSourceBasic::X11TextureSourceBasic(BasicCompositor* aCompositor, gfxXlibSurface* aSurface)
  : mCompositor(aCompositor),
    mSurface(aSurface)
{
}

IntSize
X11TextureSourceBasic::GetSize() const
{
  return mSurface->GetSize();
}

SurfaceFormat
X11TextureSourceBasic::GetFormat() const
{
  gfxContentType type = mSurface->GetContentType();
  return X11TextureSourceBasic::ContentTypeToSurfaceFormat(type);
}

SourceSurface*
X11TextureSourceBasic::GetSurface(DrawTarget* aTarget)
{
  if (!mSourceSurface) {
    NativeSurface surf;
    surf.mFormat = GetFormat();
    surf.mType = NativeSurfaceType::CAIRO_CONTEXT;
    surf.mSurface = mSurface->CairoSurface();
    surf.mSize = GetSize();
    mSourceSurface = aTarget->CreateSourceSurfaceFromNativeSurface(surf);
  }
  return mSourceSurface;
}

void
X11TextureSourceBasic::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(aCompositor->GetBackendType() == LayersBackend::LAYERS_BASIC);
  BasicCompositor* compositor = static_cast<BasicCompositor*>(aCompositor);
  mCompositor = compositor;
}

SurfaceFormat
X11TextureSourceBasic::ContentTypeToSurfaceFormat(gfxContentType aType)
{
  switch (aType) {
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

}
}
