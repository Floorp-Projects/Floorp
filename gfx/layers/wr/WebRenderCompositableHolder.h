/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include <queue>

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Maybe.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsClassHashtable.h"

namespace mozilla {

namespace wr {
class DisplayListBuilder;
class WebRenderAPI;
}

namespace layers {

class CompositableHost;
class CompositorVsyncScheduler;
class WebRenderImageHost;
class WebRenderTextureHost;

class WebRenderCompositableHolder final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderCompositableHolder)

  explicit WebRenderCompositableHolder(uint32_t aIdNamespace);

protected:
  ~WebRenderCompositableHolder();

public:
  void Destroy(wr::WebRenderAPI* aApi);
  bool HasKeysToDelete();

  void AddPipeline(const wr::PipelineId& aPipelineId);
  void RemovePipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch);

  void HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture);
  void Update(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch);

  TimeStamp GetCompositionTime() const {
    return mCompositionTime;
  }
  void SetCompositionTime(TimeStamp aTimeStamp) {
    mCompositionTime = aTimeStamp;
    if (!mCompositionTime.IsNull() && !mCompositeUntilTime.IsNull() &&
        mCompositionTime >= mCompositeUntilTime) {
      mCompositeUntilTime = TimeStamp();
    }
  }
  void CompositeUntil(TimeStamp aTimeStamp) {
    if (mCompositeUntilTime.IsNull() ||
        mCompositeUntilTime < aTimeStamp) {
      mCompositeUntilTime = aTimeStamp;
    }
  }
  TimeStamp GetCompositeUntilTime() const {
    return mCompositeUntilTime;
  }

  void AddAsyncImagePipeline(const wr::PipelineId& aPipelineId, WebRenderImageHost* aImageHost);
  void RemoveAsyncImagePipeline(wr::WebRenderAPI* aApi, const wr::PipelineId& aPipelineId);

  void UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                const LayerRect& aScBounds,
                                const gfx::Matrix4x4& aScTransform,
                                const gfx::MaybeIntSize& aScaleToSize,
                                const MaybeLayerRect& aClipRect,
                                const MaybeImageMask& aMask,
                                const WrImageRendering& aFilter,
                                const WrMixBlendMode& aMixBlendMode);
  void ApplyAsyncImages(wr::WebRenderAPI* aApi);

private:
  void DeleteOldAsyncImages(wr::WebRenderAPI* aApi);

  uint32_t GetNextResourceId() { return ++mResourceId; }
  uint32_t GetNamespace() { return mIdNamespace; }
  wr::ImageKey GetImageKey()
  {
    wr::ImageKey key;
    key.mNamespace = GetNamespace();
    key.mHandle = GetNextResourceId();
    return key;
  }
  bool GetImageKeyForTextureHost(wr::WebRenderAPI* aApi, TextureHost* aTexture, nsTArray<wr::ImageKey>& aKeys);

  struct ForwardingTextureHost {
    ForwardingTextureHost(const wr::Epoch& aEpoch, TextureHost* aTexture)
      : mEpoch(aEpoch)
      , mTexture(aTexture)
    {}
    wr::Epoch mEpoch;
    CompositableTextureHostRef mTexture;
  };

  struct PipelineTexturesHolder {
    // Holds forwarding WebRenderTextureHosts.
    std::queue<ForwardingTextureHost> mTextureHosts;
    Maybe<wr::Epoch> mDestroyedEpoch;
  };

  struct AsyncImagePipelineHolder {
    AsyncImagePipelineHolder();

    bool mInitialised;
    bool mIsChanged;
    bool mUseExternalImage;
    LayerRect mScBounds;
    gfx::Matrix4x4 mScTransform;
    gfx::MaybeIntSize mScaleToSize;
    MaybeLayerRect mClipRect;
    MaybeImageMask mMask;
    WrImageRendering mFilter;
    WrMixBlendMode mMixBlendMode;
    RefPtr<WebRenderImageHost> mImageHost;
    CompositableTextureHostRef mCurrentTexture;
    nsTArray<wr::ImageKey> mKeys;
  };

  bool UpdateImageKeys(wr::WebRenderAPI* aApi,
                       bool& aUseExternalImage,
                       AsyncImagePipelineHolder* aHolder,
                       nsTArray<wr::ImageKey>& aKeys,
                       nsTArray<wr::ImageKey>& aKeysToDelete);

  uint32_t mIdNamespace;
  uint32_t mResourceId;

  nsClassHashtable<nsUint64HashKey, PipelineTexturesHolder> mPipelineTexturesHolders;
  nsClassHashtable<nsUint64HashKey, AsyncImagePipelineHolder> mAsyncImagePipelineHolders;
  uint32_t mAsyncImageEpoch;
  nsTArray<wr::ImageKey> mKeysToDelete;
  bool mDestroyed;

  // Render time for the current composition.
  TimeStamp mCompositionTime;

  // When nonnull, during rendering, some compositable indicated that it will
  // change its rendering at this time. In order not to miss it, we composite
  // on every vsync until this time occurs (this is the latest such time).
  TimeStamp mCompositeUntilTime;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
