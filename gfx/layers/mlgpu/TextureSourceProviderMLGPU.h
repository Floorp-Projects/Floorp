/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_TextureSourceProviderMLGPU_h
#define mozilla_gfx_layers_mlgpu_TextureSourceProviderMLGPU_h

#include "mozilla/layers/TextureSourceProvider.h"

namespace mozilla {
namespace layers {

class MLGDevice;
class LayerManagerMLGPU;

class TextureSourceProviderMLGPU final : public TextureSourceProvider {
 public:
  TextureSourceProviderMLGPU(LayerManagerMLGPU* aLayerManager,
                             MLGDevice* aDevice);
  virtual ~TextureSourceProviderMLGPU();

  already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags) override;

  already_AddRefed<DataTextureSource> CreateDataTextureSourceAround(
      gfx::DataSourceSurface* aSurface) override;

  void UnlockAfterComposition(TextureHost* aTexture) override;
  bool NotifyNotUsedAfterComposition(TextureHost* aTextureHost) override;

  int32_t GetMaxTextureSize() const override;
  TimeStamp GetLastCompositionEndTime() const override;
  bool SupportsEffect(EffectTypes aEffect) override;
  bool IsValid() const override;

#ifdef XP_WIN
  ID3D11Device* GetD3D11Device() const override;
#endif

  void ReadUnlockTextures() { TextureSourceProvider::ReadUnlockTextures(); }

  // Release references to the layer manager.
  void Destroy() override;

 private:
  // Using RefPtr<> here would be a circular reference.
  LayerManagerMLGPU* mLayerManager;
  RefPtr<MLGDevice> mDevice;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_mlgpu_TextureSourceProviderMLGPU_h
