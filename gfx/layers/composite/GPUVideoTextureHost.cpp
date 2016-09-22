/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureHost.h"
#include "mozilla/dom/VideoDecoderManagerParent.h"
#include "ImageContainer.h"

namespace mozilla {
namespace layers {

GPUVideoTextureHost::GPUVideoTextureHost(TextureFlags aFlags,
                                         const SurfaceDescriptorGPUVideo& aDescriptor)
  : TextureHost(aFlags)
{
  MOZ_COUNT_CTOR(GPUVideoTextureHost);
  mImage = dom::VideoDecoderManagerParent::LookupImage(aDescriptor);
}

GPUVideoTextureHost::~GPUVideoTextureHost()
{
  MOZ_COUNT_DTOR(GPUVideoTextureHost);
}

bool
GPUVideoTextureHost::Lock()
{
  if (!mImage || !mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = mCompositor->CreateTextureSourceForImage(mImage);
  }

  return !!mTextureSource;
}

void
GPUVideoTextureHost::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor != aCompositor) {
    mTextureSource = nullptr;
    mCompositor = aCompositor;
  }
}

gfx::IntSize
GPUVideoTextureHost::GetSize() const
{
  if (!mImage) {
    return gfx::IntSize();
  }
  return mImage->GetSize();
}

gfx::SurfaceFormat
GPUVideoTextureHost::GetFormat() const
{
  return mTextureSource ? mTextureSource->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
}

} // namespace layers
} // namespace mozilla
