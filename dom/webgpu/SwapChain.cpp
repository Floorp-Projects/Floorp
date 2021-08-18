/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SwapChain.h"
#include "Texture.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(SwapChain, mParent, mTexture)
GPU_IMPL_JS_WRAP(SwapChain)

SwapChain::SwapChain(const dom::GPUSwapChainDescriptor& aDesc,
                     const dom::GPUExtent3DDict& aExtent3D,
                     wr::ExternalImageId aExternalImageId,
                     gfx::SurfaceFormat aFormat)
    : ChildOf(aDesc.mDevice),
      mGfxFormat(aFormat),
      mFormat(static_cast<uint8_t>(aDesc.mFormat)),
      mUsage(aDesc.mUsage),
      mSize(aExtent3D.mWidth, aExtent3D.mHeight),
      mTexture(aDesc.mDevice->InitSwapChain(aDesc, aExtent3D, aExternalImageId,
                                            aFormat)) {}

SwapChain::~SwapChain() { Cleanup(); }

void SwapChain::Cleanup() {
  if (mValid) {
    mValid = false;
  }
}

RefPtr<Device> SwapChain::GetParent() const { return mParent; }

void SwapChain::Destroy(wr::ExternalImageId aExternalImageId) {
  if (mValid && mParent && mParent->GetBridge()) {
    mValid = false;
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendSwapChainDestroy(aExternalImageId);
    }
  }
}

RefPtr<Texture> SwapChain::GetCurrentTexture() { return mTexture; }

}  // namespace webgpu
}  // namespace mozilla
