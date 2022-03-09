/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CanvasContext.h"
#include "nsDisplayList.h"
#include "LayerUserData.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/CompositableInProcessManager.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "ipc/WebGPUChild.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasContext)

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(CanvasContext, mTexture,
                                                mBridge, mCanvasElement,
                                                mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CanvasContext::CanvasContext() = default;

CanvasContext::~CanvasContext() {
  Cleanup();
  RemovePostRefreshObserver();
}

void CanvasContext::Cleanup() { Unconfigure(); }

JSObject* CanvasContext::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::GPUCanvasContext_Binding::Wrap(aCx, this, aGivenProto);
}

void CanvasContext::Configure(const dom::GPUCanvasConfiguration& aDesc) {
  Unconfigure();

  // these formats are guaranteed by the spec
  switch (aDesc.mFormat) {
    case dom::GPUTextureFormat::Rgba8unorm:
    case dom::GPUTextureFormat::Rgba8unorm_srgb:
      mGfxFormat = gfx::SurfaceFormat::R8G8B8A8;
      break;
    case dom::GPUTextureFormat::Bgra8unorm:
    case dom::GPUTextureFormat::Bgra8unorm_srgb:
      mGfxFormat = gfx::SurfaceFormat::B8G8R8A8;
      break;
    default:
      NS_WARNING("Specified swap chain format is not supported");
      return;
  }

  gfx::IntSize actualSize(mWidth, mHeight);
  mHandle = layers::CompositableInProcessManager::GetNextHandle();
  mTexture =
      aDesc.mDevice->InitSwapChain(aDesc, mHandle, mGfxFormat, &actualSize);
  mTexture->mTargetContext = this;
  mBridge = aDesc.mDevice->GetBridge();
  mGfxSize = actualSize;

  // Force a new frame to be built, which will execute the
  // `CanvasContextType::WebGPU` switch case in `CreateWebRenderCommands` and
  // populate the WR user data.
  mCanvasElement->InvalidateCanvas();
}

void CanvasContext::Unconfigure() {
  if (mBridge && mBridge->IsOpen() && mHandle) {
    mBridge->SendSwapChainDestroy(mHandle);
  }
  mHandle = layers::CompositableHandle();
  mBridge = nullptr;
  mTexture = nullptr;
  mGfxFormat = gfx::SurfaceFormat::UNKNOWN;
}

dom::GPUTextureFormat CanvasContext::GetPreferredFormat(Adapter&) const {
  return dom::GPUTextureFormat::Bgra8unorm;
}

RefPtr<Texture> CanvasContext::GetCurrentTexture(ErrorResult& aRv) {
  if (!mTexture) {
    aRv.ThrowOperationError("Canvas not configured");
    return nullptr;
  }
  return mTexture;
}

void CanvasContext::MaybeQueueSwapChainPresent() {
  if (mPendingSwapChainPresent) {
    return;
  }

  mPendingSwapChainPresent = true;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(
      NewCancelableRunnableMethod("CanvasContext::SwapChainPresent", this,
                                  &CanvasContext::SwapChainPresent)));
}

void CanvasContext::SwapChainPresent() {
  mPendingSwapChainPresent = false;
  if (mBridge && mBridge->IsOpen() && mHandle && mTexture) {
    mBridge->SwapChainPresent(mHandle, mTexture->mId);
  }
}

}  // namespace webgpu
}  // namespace mozilla
