/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CanvasContext.h"
#include "gfxUtils.h"
#include "LayerUserData.h"
#include "nsDisplayList.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/layers/CanvasRenderer.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/SVGObserverUtils.h"
#include "ipc/WebGPUChild.h"
#include "Utility.h"

namespace mozilla {

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    dom::GPUCanvasConfiguration& aField, const char* aName, uint32_t aFlags) {
  aField.TraverseForCC(aCallback, aFlags);
}

inline void ImplCycleCollectionUnlink(dom::GPUCanvasConfiguration& aField) {
  aField.UnlinkForCC();
}

// -

template <class T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    const std::unique_ptr<T>& aField, const char* aName, uint32_t aFlags) {
  if (aField) {
    ImplCycleCollectionTraverse(aCallback, *aField, aName, aFlags);
  }
}

template <class T>
inline void ImplCycleCollectionUnlink(std::unique_ptr<T>& aField) {
  aField = nullptr;
}

}  // namespace mozilla

// -

namespace mozilla::webgpu {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CanvasContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CanvasContext)

GPU_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(CanvasContext, mConfig,
                                                mTexture, mBridge,
                                                mCanvasElement,
                                                mOffscreenCanvas)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanvasContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// -

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

// -

void CanvasContext::GetCanvas(
    dom::OwningHTMLCanvasElementOrOffscreenCanvas& aRetVal) const {
  if (mCanvasElement) {
    aRetVal.SetAsHTMLCanvasElement() = mCanvasElement;
  } else if (mOffscreenCanvas) {
    aRetVal.SetAsOffscreenCanvas() = mOffscreenCanvas;
  } else {
    MOZ_CRASH(
        "This should only happen briefly during CC Unlink, and no JS should "
        "happen then.");
  }
}

void CanvasContext::Configure(const dom::GPUCanvasConfiguration& aConfig) {
  Unconfigure();

  // Bug 1864904: Failures in validation should throw a TypeError, per spec.

  // these formats are guaranteed by the spec
  switch (aConfig.mFormat) {
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

  mConfig.reset(new dom::GPUCanvasConfiguration(aConfig));
  mRemoteTextureOwnerId = Some(layers::RemoteTextureOwnerId::GetNext());
  mUseExternalTextureInSwapChain =
      wgpu_client_use_external_texture_in_swapChain(
          aConfig.mDevice->mId, ConvertTextureFormat(aConfig.mFormat));
  mTexture = aConfig.mDevice->InitSwapChain(
      mConfig.get(), mRemoteTextureOwnerId.ref(),
      mUseExternalTextureInSwapChain, mGfxFormat, mCanvasSize);
  if (!mTexture) {
    Unconfigure();
    return;
  }

  mTexture->mTargetContext = this;
  mBridge = aConfig.mDevice->GetBridge();
  if (mCanvasElement) {
    mWaitingCanvasRendererInitialized = true;
  }

  ForceNewFrame();
}

void CanvasContext::Unconfigure() {
  if (mBridge && mBridge->IsOpen() && mRemoteTextureOwnerId.isSome()) {
    mBridge->SendSwapChainDrop(*mRemoteTextureOwnerId);
  }
  mRemoteTextureOwnerId = Nothing();
  mBridge = nullptr;
  mConfig = nullptr;
  mTexture = nullptr;
  mGfxFormat = gfx::SurfaceFormat::UNKNOWN;
}

NS_IMETHODIMP CanvasContext::SetDimensions(int32_t aWidth, int32_t aHeight) {
  aWidth = std::max(1, aWidth);
  aHeight = std::max(1, aHeight);
  const auto newSize = gfx::IntSize{aWidth, aHeight};
  if (newSize == mCanvasSize) return NS_OK;  // No-op no-change resizes.

  mCanvasSize = newSize;
  if (mConfig) {
    const auto copy = dom::GPUCanvasConfiguration{
        *mConfig};  // So we can't null it out on ourselves.
    Configure(copy);
  }
  return NS_OK;
}

RefPtr<Texture> CanvasContext::GetCurrentTexture(ErrorResult& aRv) {
  if (!mTexture) {
    aRv.ThrowOperationError("Canvas not configured");
    return nullptr;
  }

  MOZ_ASSERT(mConfig);
  MOZ_ASSERT(mRemoteTextureOwnerId.isSome());

  if (mNewTextureRequested) {
    mNewTextureRequested = false;

    mTexture = mConfig->mDevice->CreateTextureForSwapChain(
        mConfig.get(), mCanvasSize, mRemoteTextureOwnerId.ref());
    mTexture->mTargetContext = this;
  }
  return mTexture;
}

void CanvasContext::MaybeQueueSwapChainPresent() {
  MOZ_ASSERT(mTexture);

  if (mTexture) {
    mBridge->NotifyWaitForSubmit(mTexture->mId);
  }

  if (mPendingSwapChainPresent) {
    return;
  }

  mPendingSwapChainPresent = true;

  if (mWaitingCanvasRendererInitialized) {
    return;
  }

  InvalidateCanvasContent();
}

Maybe<layers::SurfaceDescriptor> CanvasContext::SwapChainPresent() {
  mPendingSwapChainPresent = false;
  if (!mBridge || !mBridge->IsOpen() || mRemoteTextureOwnerId.isNothing() ||
      !mTexture) {
    return Nothing();
  }
  mLastRemoteTextureId = Some(layers::RemoteTextureId::GetNext());
  mBridge->SwapChainPresent(mTexture->mId, *mLastRemoteTextureId,
                            *mRemoteTextureOwnerId);
  if (mUseExternalTextureInSwapChain) {
    mTexture->Destroy();
    mNewTextureRequested = true;
  }
  return Some(layers::SurfaceDescriptorRemoteTexture(*mLastRemoteTextureId,
                                                     *mRemoteTextureOwnerId));
}

bool CanvasContext::UpdateWebRenderCanvasData(
    mozilla::nsDisplayListBuilder* aBuilder, WebRenderCanvasData* aCanvasData) {
  auto* renderer = aCanvasData->GetCanvasRenderer();

  if (renderer && mRemoteTextureOwnerId.isSome() &&
      renderer->GetRemoteTextureOwnerId() == mRemoteTextureOwnerId) {
    return true;
  }

  renderer = aCanvasData->CreateCanvasRenderer();
  if (!InitializeCanvasRenderer(aBuilder, renderer)) {
    // Clear CanvasRenderer of WebRenderCanvasData
    aCanvasData->ClearCanvasRenderer();
    return false;
  }
  return true;
}

bool CanvasContext::InitializeCanvasRenderer(
    nsDisplayListBuilder* aBuilder, layers::CanvasRenderer* aRenderer) {
  if (mRemoteTextureOwnerId.isNothing()) {
    return false;
  }

  layers::CanvasRendererData data;
  data.mContext = this;
  data.mSize = mCanvasSize;
  data.mIsOpaque = false;
  data.mRemoteTextureOwnerId = mRemoteTextureOwnerId;

  aRenderer->Initialize(data);
  aRenderer->SetDirty();

  if (mWaitingCanvasRendererInitialized) {
    InvalidateCanvasContent();
  }
  mWaitingCanvasRendererInitialized = false;

  return true;
}

mozilla::UniquePtr<uint8_t[]> CanvasContext::GetImageBuffer(
    int32_t* out_format, gfx::IntSize* out_imageSize) {
  *out_format = 0;
  *out_imageSize = {};

  gfxAlphaType any;
  RefPtr<gfx::SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) {
    return nullptr;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = snapshot->GetDataSurface();
  *out_imageSize = dataSurface->GetSize();

  if (ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    gfxUtils::GetImageBufferWithRandomNoise(
        dataSurface,
        /* aIsAlphaPremultiplied */ true, GetCookieJarSettings(), &*out_format);
  }

  return gfxUtils::GetImageBuffer(dataSurface, /* aIsAlphaPremultiplied */ true,
                                  &*out_format);
}

NS_IMETHODIMP CanvasContext::GetInputStream(const char* aMimeType,
                                            const nsAString& aEncoderOptions,
                                            nsIInputStream** aStream) {
  gfxAlphaType any;
  RefPtr<gfx::SourceSurface> snapshot = GetSurfaceSnapshot(&any);
  if (!snapshot) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = snapshot->GetDataSurface();

  if (ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    gfxUtils::GetInputStreamWithRandomNoise(
        dataSurface, /* aIsAlphaPremultiplied */ true, aMimeType,
        aEncoderOptions, GetCookieJarSettings(), aStream);
  }

  return gfxUtils::GetInputStream(dataSurface, /* aIsAlphaPremultiplied */ true,
                                  aMimeType, aEncoderOptions, aStream);
}

already_AddRefed<mozilla::gfx::SourceSurface> CanvasContext::GetSurfaceSnapshot(
    gfxAlphaType* aOutAlphaType) {
  if (aOutAlphaType) {
    *aOutAlphaType = gfxAlphaType::Premult;
  }

  auto* const cm = gfx::CanvasManagerChild::Get();
  if (!cm) {
    return nullptr;
  }

  if (!mBridge || !mBridge->IsOpen() || mRemoteTextureOwnerId.isNothing()) {
    return nullptr;
  }

  MOZ_ASSERT(mRemoteTextureOwnerId.isSome());
  return cm->GetSnapshot(cm->Id(), mBridge->Id(), mRemoteTextureOwnerId,
                         mGfxFormat, /* aPremultiply */ false,
                         /* aYFlip */ false);
}

Maybe<layers::SurfaceDescriptor> CanvasContext::GetFrontBuffer(
    WebGLFramebufferJS*, const bool) {
  if (mPendingSwapChainPresent) {
    auto desc = SwapChainPresent();
    MOZ_ASSERT(!mPendingSwapChainPresent);
    return desc;
  }
  return Nothing();
}

void CanvasContext::ForceNewFrame() {
  if (!mCanvasElement && !mOffscreenCanvas) {
    return;
  }

  // Force a new frame to be built, which will execute the
  // `CanvasContextType::WebGPU` switch case in `CreateWebRenderCommands` and
  // populate the WR user data.
  if (mCanvasElement) {
    mCanvasElement->InvalidateCanvas();
  } else if (mOffscreenCanvas) {
    dom::OffscreenCanvasDisplayData data;
    data.mSize = mCanvasSize;
    data.mIsOpaque = false;
    data.mOwnerId = mRemoteTextureOwnerId;
    mOffscreenCanvas->UpdateDisplayData(data);
  }
}

void CanvasContext::InvalidateCanvasContent() {
  if (!mCanvasElement && !mOffscreenCanvas) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  if (mCanvasElement) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);
    mCanvasElement->InvalidateCanvasContent(nullptr);
  } else if (mOffscreenCanvas) {
    mOffscreenCanvas->QueueCommitToCompositor();
  }
}

}  // namespace mozilla::webgpu
