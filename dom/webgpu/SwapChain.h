/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SwapChain_H_
#define GPU_SwapChain_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace dom {
struct GPUExtent3DDict;
struct GPUSwapChainDescriptor;
}  // namespace dom
namespace webgpu {

class Device;
class Texture;

class SwapChain final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(SwapChain)
  GPU_DECL_JS_WRAP(SwapChain)

  SwapChain(const dom::GPUSwapChainDescriptor& aDesc,
            const dom::GPUExtent3DDict& aExtent3D,
            wr::ExternalImageId aExternalImageId, gfx::SurfaceFormat aFormat);

  RefPtr<Device> GetParent() const;
  void Destroy(wr::ExternalImageId aExternalImageId);

  const gfx::SurfaceFormat mGfxFormat;
  const uint8_t
      mFormat;  // This is `dom::GPUTextureFormat` but without the includes
  const uint32_t mUsage;
  const nsIntSize mSize;

 private:
  virtual ~SwapChain();
  void Cleanup();

  RefPtr<Texture> mTexture;

 public:
  RefPtr<Texture> GetCurrentTexture();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_SwapChain_H_
