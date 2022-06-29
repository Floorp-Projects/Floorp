/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERIMAGEHOST_H
#define MOZILLA_GFX_WEBRENDERIMAGEHOST_H

#include <unordered_map>

#include "CompositableHost.h"               // for CompositableHost
#include "mozilla/layers/ImageComposite.h"  // for ImageComposite
#include "mozilla/WeakPtr.h"

namespace mozilla {
namespace layers {

class AsyncImagePipelineManager;
class WebRenderBridgeParent;
class WebRenderBridgeParentRef;

/**
 * ImageHost. Works with ImageClientSingle and ImageClientBuffered
 */
class WebRenderImageHost : public CompositableHost, public ImageComposite {
 public:
  explicit WebRenderImageHost(const TextureInfo& aTextureInfo);
  virtual ~WebRenderImageHost();

  void UseTextureHost(const nsTArray<TimedTexture>& aTextures) override;
  void RemoveTextureHost(TextureHost* aTexture) override;

  void Dump(std::stringstream& aStream, const char* aPrefix = "",
            bool aDumpHtml = false) override;

  void CleanupResources() override;

  uint32_t GetDroppedFrames() override { return GetDroppedFramesAndReset(); }

  WebRenderImageHost* AsWebRenderImageHost() override { return this; }

  TextureHost* GetAsTextureHostForComposite(
      AsyncImagePipelineManager* aAsyncImageManager);

  void SetWrBridge(const wr::PipelineId& aPipelineId,
                   WebRenderBridgeParent* aWrBridge);

  void ClearWrBridge(const wr::PipelineId& aPipelineId,
                     WebRenderBridgeParent* aWrBridge);

  TextureHost* GetCurrentTextureHost() { return mCurrentTextureHost; }

 protected:
  // ImageComposite
  TimeStamp GetCompositionTime() const override;
  CompositionOpportunityId GetCompositionOpportunityId() const override;
  void AppendImageCompositeNotification(
      const ImageCompositeNotificationInfo& aInfo) const override;

  void SetCurrentTextureHost(TextureHost* aTexture);

  std::unordered_map<uint64_t, RefPtr<WebRenderBridgeParentRef>> mWrBridges;

  AsyncImagePipelineManager* mCurrentAsyncImageManager;

  CompositableTextureHostRef mCurrentTextureHost;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_WEBRENDERIMAGEHOST_H
