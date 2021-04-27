/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CanvasContext.h"
#include "SwapChain.h"
#include "nsDisplayList.h"
#include "LayerUserData.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasContext)

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasContext, mSwapChain,
                                       mCanvasElement, mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CanvasContext::CanvasContext()
    : mExternalImageId(layers::CompositorManagerChild::GetInstance()
                           ->GetNextExternalImageId()) {}

CanvasContext::~CanvasContext() {
  Cleanup();
  RemovePostRefreshObserver();
}

void CanvasContext::Cleanup() {
  if (mSwapChain) {
    mSwapChain->Destroy(mExternalImageId);
    mSwapChain = nullptr;
  }
  if (mRenderRootStateManager && mImageKey) {
    mRenderRootStateManager->AddImageKeyForDiscard(mImageKey.value());
    mRenderRootStateManager = nullptr;
    mImageKey.reset();
  }
}

JSObject* CanvasContext::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::GPUCanvasContext_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<layers::Layer> CanvasContext::GetCanvasLayer(
    nsDisplayListBuilder* aBuilder, layers::Layer* aOldLayer,
    layers::LayerManager* aManager) {
  return nullptr;
}

bool CanvasContext::UpdateWebRenderCanvasData(
    nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  return true;
}

dom::GPUTextureFormat CanvasContext::GetSwapChainPreferredFormat(
    Adapter&) const {
  return dom::GPUTextureFormat::Bgra8unorm;
}

RefPtr<SwapChain> CanvasContext::ConfigureSwapChain(
    const dom::GPUSwapChainDescriptor& aDesc, ErrorResult& aRv) {
  Cleanup();

  gfx::SurfaceFormat format;
  switch (aDesc.mFormat) {
    case dom::GPUTextureFormat::Rgba8unorm:
      format = gfx::SurfaceFormat::R8G8B8A8;
      break;
    case dom::GPUTextureFormat::Bgra8unorm:
      format = gfx::SurfaceFormat::B8G8R8A8;
      break;
    default:
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return nullptr;
  }

  dom::GPUExtent3DDict extent;
  extent.mWidth = mWidth;
  extent.mHeight = mHeight;
  extent.mDepthOrArrayLayers = 1;
  mSwapChain = new SwapChain(aDesc, extent, mExternalImageId, format);

  // Force a new frame to be built, which will execute the
  // `CanvasContextType::WebGPU` switch case in `CreateWebRenderCommands` and
  // populate the WR user data.
  mCanvasElement->InvalidateCanvas();

  mSwapChain->GetCurrentTexture()->mTargetCanvasElement = mCanvasElement;
  return mSwapChain;
}

Maybe<wr::ImageKey> CanvasContext::GetImageKey() const { return mImageKey; }

wr::ImageKey CanvasContext::CreateImageKey(
    layers::RenderRootStateManager* aManager) {
  const auto key = aManager->WrBridge()->GetNextImageKey();
  mRenderRootStateManager = aManager;
  mImageKey = Some(key);
  return key;
}

bool CanvasContext::UpdateWebRenderLocalCanvasData(
    layers::WebRenderLocalCanvasData* aCanvasData) {
  if (!mSwapChain || !mSwapChain->GetParent()) {
    return false;
  }

  const auto size =
      nsIntSize(AssertedCast<int>(mWidth), AssertedCast<int>(mHeight));
  if (mSwapChain->mSize != size) {
    const auto gfxFormat = mSwapChain->mGfxFormat;
    dom::GPUSwapChainDescriptor desc;
    desc.mFormat = static_cast<dom::GPUTextureFormat>(mSwapChain->mFormat);
    desc.mUsage = mSwapChain->mUsage;
    desc.mDevice = mSwapChain->GetParent();

    mSwapChain->Destroy(mExternalImageId);
    mExternalImageId =
        layers::CompositorManagerChild::GetInstance()->GetNextExternalImageId();

    dom::GPUExtent3DDict extent;
    extent.mWidth = size.width;
    extent.mHeight = size.height;
    extent.mDepthOrArrayLayers = 1;
    mSwapChain = new SwapChain(desc, extent, mExternalImageId, gfxFormat);
  }

  aCanvasData->mGpuBridge = mSwapChain->GetParent()->GetBridge().get();
  aCanvasData->mGpuTextureId = mSwapChain->GetCurrentTexture()->mId;
  aCanvasData->mExternalImageId = mExternalImageId;
  aCanvasData->mFormat = mSwapChain->mGfxFormat;
  return true;
}

}  // namespace webgpu
}  // namespace mozilla
