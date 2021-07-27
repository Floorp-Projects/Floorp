/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CanvasContext.h"
#include "nsDisplayList.h"
#include "LayerUserData.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasContext)

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CanvasContext, mTexture, mBridge,
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
  Unconfigure();
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

void CanvasContext::Configure(const dom::GPUCanvasConfiguration& aDesc) {
  Unconfigure();

  switch (aDesc.mFormat) {
    case dom::GPUTextureFormat::Rgba8unorm:
      mGfxFormat = gfx::SurfaceFormat::R8G8B8A8;
      break;
    case dom::GPUTextureFormat::Bgra8unorm:
      mGfxFormat = gfx::SurfaceFormat::B8G8R8A8;
      break;
    default:
      NS_WARNING("Specified swap chain format is not supported");
      return;
  }

  gfx::IntSize actualSize(mWidth, mHeight);
  mTexture = aDesc.mDevice->InitSwapChain(aDesc, mExternalImageId, mGfxFormat,
                                          &actualSize);
  mTexture->mTargetCanvasElement = mCanvasElement;
  mBridge = aDesc.mDevice->GetBridge();
  mGfxSize = actualSize;

  // Force a new frame to be built, which will execute the
  // `CanvasContextType::WebGPU` switch case in `CreateWebRenderCommands` and
  // populate the WR user data.
  mCanvasElement->InvalidateCanvas();
}

Maybe<wr::ImageKey> CanvasContext::GetImageKey() const { return mImageKey; }

wr::ImageKey CanvasContext::CreateImageKey(
    layers::RenderRootStateManager* aManager) {
  const auto key = aManager->WrBridge()->GetNextImageKey();
  mRenderRootStateManager = aManager;
  mImageKey = Some(key);
  return key;
}

void CanvasContext::Unconfigure() {
  if (mBridge && mBridge->IsOpen()) {
    mBridge->SendSwapChainDestroy(mExternalImageId);
  }
  mBridge = nullptr;
  mTexture = nullptr;
}

dom::GPUTextureFormat CanvasContext::GetPreferredFormat(Adapter&) const {
  return dom::GPUTextureFormat::Bgra8unorm;
}

RefPtr<Texture> CanvasContext::GetCurrentTexture() { return mTexture; }

bool CanvasContext::UpdateWebRenderLocalCanvasData(
    layers::WebRenderLocalCanvasData* aCanvasData) {
  if (!mTexture) {
    return false;
  }

  aCanvasData->mGpuBridge = mBridge.get();
  aCanvasData->mGpuTextureId = mTexture->mId;
  aCanvasData->mExternalImageId = mExternalImageId;
  aCanvasData->mFormat = mGfxFormat;
  return true;
}

wr::ImageDescriptor CanvasContext::MakeImageDescriptor() const {
  const layers::RGBDescriptor rgbDesc(mGfxSize, mGfxFormat, false);
  const auto targetStride = layers::ImageDataSerializer::GetRGBStride(rgbDesc);
  const bool preferCompositorSurface = true;
  return wr::ImageDescriptor(mGfxSize, targetStride, mGfxFormat,
                             wr::OpacityType::HasAlphaChannel,
                             preferCompositorSurface);
}

}  // namespace webgpu
}  // namespace mozilla
