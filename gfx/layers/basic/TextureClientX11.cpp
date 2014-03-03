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
#include "gfxXlibSurface.h"
#include "gfx2DGlue.h"

#include "mozilla/X11Util.h"
#include <X11/Xlib.h>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

TextureClientX11::TextureClientX11(SurfaceFormat aFormat, TextureFlags aFlags)
  : TextureClient(aFlags),
    mFormat(aFormat),
    mLocked(false)
{
  MOZ_COUNT_CTOR(TextureClientX11);
}

TextureClientX11::~TextureClientX11()
{
  MOZ_COUNT_DTOR(TextureClientX11);
}

bool
TextureClientX11::IsAllocated() const
{
  return !!mSurface;
}

bool
TextureClientX11::Lock(OpenMode aMode)
{
  // XXX - Turn this into a fatal assertion as soon as Bug 952507 is fixed
  NS_WARN_IF_FALSE(!mLocked, "The TextureClient is already Locked!");
  mLocked = IsValid() && IsAllocated();
  return mLocked;
}

void
TextureClientX11::Unlock()
{
  // XXX - Turn this into a fatal assertion as soon as Bug 952507 is fixed
  NS_WARN_IF_FALSE(mLocked, "The TextureClient is already Unlocked!");
  mLocked = false;

  if (mSurface) {
    FinishX(DefaultXDisplay());
  }
}

bool
TextureClientX11::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!mSurface) {
    return false;
  }

  aOutDescriptor = SurfaceDescriptorX11(mSurface);
  return true;
}

TextureClientData*
TextureClientX11::DropTextureData()
{
  MOZ_ASSERT(!(mFlags & TEXTURE_DEALLOCATE_CLIENT));
  return nullptr;
}

bool
TextureClientX11::UpdateSurface(gfxASurface* aSurface)
{
  MOZ_ASSERT(IsValid());

  if (gfxPlatform::GetPlatform()->SupportsAzureContent()) {
    RefPtr<DrawTarget> dt = GetAsDrawTarget();
    if (!dt) {
      return false;
    }

    RefPtr<SourceSurface> source = gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aSurface);
    dt->CopySurface(source, IntRect(IntPoint(), GetSize()), IntPoint());
  } else {
    if (!mSurface) {
      return false;
    }

    nsRefPtr<gfxContext> ctx = new gfxContext(mSurface.get());
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->DrawSurface(aSurface, mSurface->GetSize());
  }

  return true;
}

already_AddRefed<gfxASurface>
TextureClientX11::GetAsSurface()
{
  MOZ_ASSERT(IsValid());
  if (!mSurface) {
    return nullptr;
  }

  nsRefPtr<gfxASurface> temp = mSurface.get();
  return temp.forget();
}

bool
TextureClientX11::AllocateForSurface(IntSize aSize, TextureAllocationFlags aTextureFlags)
{
  MOZ_ASSERT(IsValid());
  //MOZ_ASSERT(mFormat != gfx::FORMAT_YUV, "This TextureClient cannot use YCbCr data");

  gfxContentType contentType = ContentForFormat(mFormat);
  nsRefPtr<gfxASurface> surface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, contentType);
  if (!surface || surface->GetType() != gfxSurfaceType::Xlib) {
    NS_ERROR("creating Xlib surface failed!");
    return false;
  }

  mSize = aSize;
  mSurface = static_cast<gfxXlibSurface*>(surface.get());

  // The host is always responsible for freeing the pixmap.
  mSurface->ReleasePixmap();
  return true;
}

TemporaryRef<DrawTarget>
TextureClientX11::GetAsDrawTarget()
{
  MOZ_ASSERT(IsValid());
  if (!mSurface) {
    return nullptr;
  }

  IntSize size = ToIntSize(mSurface->GetSize());
  return Factory::CreateDrawTargetForCairoSurface(mSurface->CairoSurface(), size);
}
