/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERUSERDATA_H
#define GFX_WEBRENDERUSERDATA_H

#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/AnimationInfo.h"

class nsDisplayItemGeometry;

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
    eFallback,
    eAnimation,
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
  Maybe<wr::ImageKey> GetKey() { return mKey; }
  void SetKey(const wr::ImageKey& aKey) { mKey = Some(aKey); }
  already_AddRefed<ImageClient> GetImageClient();

  Maybe<wr::ImageKey> UpdateImageKey(ImageContainer* aContainer, bool aForceUpdate = false);

  void CreateAsyncImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         ImageContainer* aContainer,
                                         const StackingContextHelper& aSc,
                                         const LayerRect& aBounds,
                                         const LayerRect& aSCBounds,
                                         const gfx::Matrix4x4& aSCTransform,
                                         const gfx::MaybeIntSize& aScaleToSize,
                                         const wr::ImageRendering& aFilter,
                                         const wr::MixBlendMode& aMixBlendMode);

  void CreateImageClientIfNeeded();

protected:
  void CreateExternalImageIfNeeded();

  wr::MaybeExternalImageId mExternalImageId;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  Maybe<wr::PipelineId> mPipelineId;
  RefPtr<ImageContainer> mContainer;
};

class WebRenderFallbackData : public WebRenderImageData
{
public:
  explicit WebRenderFallbackData(WebRenderLayerManager* aWRManager);
  virtual ~WebRenderFallbackData();

  virtual UserDataType GetType() override { return UserDataType::eFallback; }
  static UserDataType Type() { return UserDataType::eFallback; }
  nsAutoPtr<nsDisplayItemGeometry> GetGeometry();
  void SetGeometry(nsAutoPtr<nsDisplayItemGeometry> aGeometry);
  nsRect GetBounds() { return mBounds; }
  void SetBounds(const nsRect& aRect) { mBounds = aRect; }

protected:
  nsAutoPtr<nsDisplayItemGeometry> mGeometry;
  nsRect mBounds;
};

class WebRenderAnimationData : public WebRenderUserData
{
public:
  explicit WebRenderAnimationData(WebRenderLayerManager* aWRManager);
  virtual ~WebRenderAnimationData() {}

  virtual UserDataType GetType() override { return UserDataType::eAnimation; }
  static UserDataType Type() { return UserDataType::eAnimation; }
  AnimationInfo& GetAnimationInfo() { return mAnimationInfo; }

protected:
  AnimationInfo mAnimationInfo;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERUSERDATA_H */
