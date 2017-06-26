/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureSourceProviderMLGPU.h"
#include "LayerManagerMLGPU.h"
#include "MLGDevice.h"
#ifdef XP_WIN
# include "mozilla/layers/MLGDeviceD3D11.h"
#endif

namespace mozilla {
namespace layers {

TextureSourceProviderMLGPU::TextureSourceProviderMLGPU(LayerManagerMLGPU* aLayerManager, MLGDevice* aDevice)
 : mLayerManager(aLayerManager),
   mDevice(aDevice)
{
}

TextureSourceProviderMLGPU::~TextureSourceProviderMLGPU()
{
}

int32_t
TextureSourceProviderMLGPU::GetMaxTextureSize() const
{
  if (!mDevice) {
    return 0;
  }
  return mDevice->GetMaxTextureSize();
}

bool
TextureSourceProviderMLGPU::SupportsEffect(EffectTypes aEffect)
{
  switch (aEffect) {
  case EffectTypes::YCBCR:
    return true;
  default:
    MOZ_ASSERT_UNREACHABLE("NYI");
  }
  return false;
}

bool
TextureSourceProviderMLGPU::IsValid() const
{
  return !!mLayerManager;
}

void
TextureSourceProviderMLGPU::Destroy()
{
  mLayerManager = nullptr;
  mDevice = nullptr;
  TextureSourceProvider::Destroy();
}

#ifdef XP_WIN
ID3D11Device*
TextureSourceProviderMLGPU::GetD3D11Device() const
{
  if (!mDevice) {
    return nullptr;
  }
  return mDevice->AsD3D11()->GetD3D11Device();
}
#endif

TimeStamp
TextureSourceProviderMLGPU::GetLastCompositionEndTime() const
{
  if (!mLayerManager) {
    return TimeStamp();
  }
  return mLayerManager->GetLastCompositionEndTime();
}

already_AddRefed<DataTextureSource>
TextureSourceProviderMLGPU::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSource> texture = mDevice->CreateDataTextureSource(aFlags);
  return texture.forget();
}

already_AddRefed<DataTextureSource>
TextureSourceProviderMLGPU::CreateDataTextureSourceAround(gfx::DataSourceSurface* aSurface)
{
  MOZ_ASSERT_UNREACHABLE("NYI");
  return nullptr;
}

bool
TextureSourceProviderMLGPU::NotifyNotUsedAfterComposition(TextureHost* aTextureHost)
{
  if (!mDevice) {
    return false;
  }
  return TextureSourceProvider::NotifyNotUsedAfterComposition(aTextureHost);
}

} // namespace layers
} // namespace mozilla
