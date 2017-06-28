/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERUSERDATA_H
#define GFX_WEBRENDERUSERDATA_H

#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {
class ImageClient;
class ImageContainer;
class WebRenderBridgeChild;
class WebRenderImageData;
class WebRenderLayerManager;

class WebRenderUserData
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebRenderUserData)

  explicit WebRenderUserData(WebRenderLayerManager* aWRManager)
    : mWRManager(aWRManager)
  { }

  virtual WebRenderImageData* AsImageData() { return nullptr; }

  enum class UserDataType {
    eImage,
  };

  virtual UserDataType GetType() = 0;

protected:
  virtual ~WebRenderUserData() {}

  WebRenderBridgeChild* WrBridge() const;

  WebRenderLayerManager* mWRManager;
};

class WebRenderImageData : public WebRenderUserData
{
public:
  explicit WebRenderImageData(WebRenderLayerManager* aWRManager);
  virtual ~WebRenderImageData();

  virtual WebRenderImageData* AsImageData() override { return this; }
  virtual UserDataType GetType() override { return UserDataType::eImage; }
  static UserDataType Type() { return UserDataType::eImage; }

  Maybe<wr::ImageKey> UpdateImageKey(ImageContainer* aContainer);

  void CreateAsyncImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         ImageContainer* aContainer,
                                         const StackingContextHelper& aSc,
                                         const LayerRect& aBounds,
                                         const LayerRect& aSCBounds,
                                         const gfx::Matrix4x4& aSCTransform,
                                         const gfx::MaybeIntSize& aScaleToSize,
                                         const WrImageRendering& aFilter,
                                         const WrMixBlendMode& aMixBlendMode);

protected:
  void CreateImageClientIfNeeded();
  void CreateExternalImageIfNeeded();

  wr::MaybeExternalImageId mExternalImageId;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  Maybe<wr::PipelineId> mPipelineId;
  RefPtr<ImageContainer> mContainer;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERUSERDATA_H */
