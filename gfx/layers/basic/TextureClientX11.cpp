/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/layers/TextureClientX11.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/ShadowLayerUtilsX11.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>

using namespace mozilla;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

already_AddRefed<TextureClient>
CreateX11TextureClient(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                       TextureFlags aFlags, ISurfaceAllocator* aAllocator)
{
  TextureData* data = X11TextureData::Create(aSize, aFormat, aFlags, aAllocator);
  if (!data) {
    return nullptr;
  }
  return MakeAndAddRef<TextureClient>(data, aFlags, aAllocator);
}

X11TextureData::X11TextureData(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                               bool aClientDeallocation, bool aIsCrossProcess,
                               gfxXlibSurface* aSurface)
: mSize(aSize)
, mFormat(aFormat)
, mSurface(aSurface)
, mClientDeallocation(aClientDeallocation)
, mIsCrossProcess(aIsCrossProcess)
{
  MOZ_ASSERT(mSurface);
}

bool
X11TextureData::Lock(OpenMode aMode, FenceHandle*)
{
  return true;
}

void
X11TextureData::Unlock()
{
  if (mSurface && mIsCrossProcess) {
    FinishX(DefaultXDisplay());
  }
}


bool
X11TextureData::Serialize(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(mSurface);
  if (!mSurface) {
    return false;
  }

  if (!mClientDeallocation) {
    // Pass to the host the responsibility of freeing the pixmap. ReleasePixmap means
    // the underlying pixmap will not be deallocated in mSurface's destructor.
    // ToSurfaceDescriptor is at most called once per TextureClient.
    mSurface->ReleasePixmap();
  }

  aOutDescriptor = SurfaceDescriptorX11(mSurface);
  return true;
}

already_AddRefed<gfx::DrawTarget>
X11TextureData::BorrowDrawTarget()
{
  MOZ_ASSERT(mSurface);
  if (!mSurface) {
    return nullptr;
  }

  IntSize size = mSurface->GetSize();
  RefPtr<gfx::DrawTarget> dt = Factory::CreateDrawTargetForCairoSurface(mSurface->CairoSurface(), size);

  return dt.forget();
}

bool
X11TextureData::UpdateFromSurface(gfx::SourceSurface* aSurface)
{
  RefPtr<DrawTarget> dt = BorrowDrawTarget();

  if (!dt) {
    return false;
  }

  dt->CopySurface(aSurface, IntRect(IntPoint(), aSurface->GetSize()), IntPoint());

  return true;
}

void
X11TextureData::Deallocate(ISurfaceAllocator*)
{
  mSurface = nullptr;
}

TextureData*
X11TextureData::CreateSimilar(ISurfaceAllocator* aAllocator,
                              TextureFlags aFlags,
                              TextureAllocationFlags aAllocFlags) const
{
  return X11TextureData::Create(mSize, mFormat, aFlags, aAllocator);
}

X11TextureData*
X11TextureData::Create(gfx::IntSize aSize, gfx::SurfaceFormat aFormat,
                       TextureFlags aFlags, ISurfaceAllocator* aAllocator)
{
  MOZ_ASSERT(aSize.width >= 0 && aSize.height >= 0);
  if (aSize.width <= 0 || aSize.height <= 0 ||
      aSize.width > XLIB_IMAGE_SIDE_SIZE_LIMIT ||
      aSize.height > XLIB_IMAGE_SIDE_SIZE_LIMIT) {
    gfxDebug() << "Asking for X11 surface of invalid size " << aSize.width << "x" << aSize.height;
    return nullptr;
  }
  gfxImageFormat imageFormat = SurfaceFormatToImageFormat(aFormat);
  RefPtr<gfxASurface> surface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, imageFormat);
  if (!surface || surface->GetType() != gfxSurfaceType::Xlib) {
    NS_ERROR("creating Xlib surface failed!");
    return nullptr;
  }

  gfxXlibSurface* xlibSurface = static_cast<gfxXlibSurface*>(surface.get());

  bool crossProcess = !aAllocator->IsSameProcess();
  X11TextureData* texture = new X11TextureData(aSize, aFormat,
                                               !!(aFlags & TextureFlags::DEALLOCATE_CLIENT),
                                               crossProcess,
                                               xlibSurface);
  if (crossProcess) {
    FinishX(DefaultXDisplay());
  }

  return texture;
}

} // namespace
} // namespace
