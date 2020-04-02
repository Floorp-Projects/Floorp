/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CanvasContext_H_
#define GPU_CanvasContext_H_

#include "nsICanvasRenderingContextInternal.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "SwapChain.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace dom {
class Promise;
}  // namespace dom
namespace layers {
class WebRenderLocalCanvasData;
};
namespace webgpu {
class Device;
class SwapChain;
class Texture;

class CanvasContext final : public nsICanvasRenderingContextInternal,
                            public nsWrapperCache {
 private:
  virtual ~CanvasContext();
  void Cleanup();

 public:
  // nsISupports interface + CC
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CanvasContext)

  CanvasContext();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void RemoveSwapChain();

  Maybe<wr::ImageKey> GetImageKey() const;
  wr::ImageKey CreateImageKey(layers::RenderRootStateManager* aManager);
  bool UpdateWebRenderLocalCanvasData(
      layers::WebRenderLocalCanvasData* aCanvasData);

  const wr::ExternalImageId mExternalImageId;

 public:  // nsICanvasRenderingContextInternal
  int32_t GetWidth() override { return mWidth; }
  int32_t GetHeight() override { return mHeight; }

  NS_IMETHOD SetDimensions(int32_t aWidth, int32_t aHeight) override {
    mWidth = aWidth;
    mHeight = aHeight;
    return NS_OK;
  }
  NS_IMETHOD InitializeWithDrawTarget(
      nsIDocShell* aShell, NotNull<gfx::DrawTarget*> aTarget) override {
    return NS_OK;
  }

  mozilla::UniquePtr<uint8_t[]> GetImageBuffer(int32_t* aFormat) override {
    MOZ_CRASH("todo");
  }
  NS_IMETHOD GetInputStream(const char* aMimeType,
                            const nsAString& aEncoderOptions,
                            nsIInputStream** aStream) override {
    *aStream = nullptr;
    return NS_OK;
  }

  already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* aOutAlphaType) override {
    return nullptr;
  }

  void SetOpaqueValueFromOpaqueAttr(bool aOpaqueAttrValue) override {}
  bool GetIsOpaque() override { return true; }
  NS_IMETHOD Reset() override { return NS_OK; }
  already_AddRefed<Layer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                         Layer* aOldLayer,
                                         LayerManager* aManager) override;
  bool UpdateWebRenderCanvasData(nsDisplayListBuilder* aBuilder,
                                 WebRenderCanvasData* aCanvasData) override;
  void MarkContextClean() override {}

  NS_IMETHOD Redraw(const gfxRect& aDirty) override { return NS_OK; }
  NS_IMETHOD SetIsIPC(bool aIsIPC) override { return NS_OK; }

  void DidRefresh() override {}

  void MarkContextCleanForFrameCapture() override {}
  bool IsContextCleanForFrameCapture() override { return false; }

 public:
  RefPtr<SwapChain> ConfigureSwapChain(const dom::GPUSwapChainDescriptor& aDesc,
                                       ErrorResult& aRv);

 private:
  uint32_t mWidth = 0, mHeight = 0;

  RefPtr<SwapChain> mSwapChain;
  RefPtr<layers::RenderRootStateManager> mRenderRootStateManager;
  Maybe<wr::ImageKey> mImageKey;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CanvasContext_H_
