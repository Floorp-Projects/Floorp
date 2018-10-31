/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include <queue>

#include "CompositableHost.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/WebRenderTextureHostWrapper.h"
#include "mozilla/Maybe.h"
#include "mozilla/webrender/WebRenderAPI.h"
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
class WebRenderTextureHostWrapper;

class AsyncImagePipelineManager final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncImagePipelineManager)

  explicit AsyncImagePipelineManager(already_AddRefed<wr::WebRenderAPI>&& aApi);

protected:
  ~AsyncImagePipelineManager();

public:
  void Destroy();

  void AddPipeline(const wr::PipelineId& aPipelineId);
  void RemovePipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch);

  void HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture);
  void HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHostWrapper* aWrTextureWrapper);
  void HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, const wr::ExternalImageId& aImageId);

  // This is called from the Renderer thread to notify this class about the
  // pipelines in the most recently completed render. A copy of the update
  // information is put into mUpdatesQueue.
  void NotifyPipelinesUpdated(wr::WrPipelineInfo aInfo, bool aRender);

  // This is run on the compositor thread to process mUpdatesQueue. We make
  // this a public entry point because we need to invoke it from other places.
  void ProcessPipelineUpdates();

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
  void RemoveAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                wr::TransactionBuilder& aTxn);

  void UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                const LayoutDeviceRect& aScBounds,
                                const gfx::Matrix4x4& aScTransform,
                                const gfx::MaybeIntSize& aScaleToSize,
                                const wr::ImageRendering& aFilter,
                                const wr::MixBlendMode& aMixBlendMode);
  void ApplyAsyncImagesOfImageBridge(wr::TransactionBuilder& aSceneBuilderTxn, wr::TransactionBuilder& aFastTxn);
  void ApplyAsyncImageForPipeline(const wr::PipelineId& aPipelineId, wr::TransactionBuilder& aTxn, wr::TransactionBuilder& aTxnForImageBridge);

  void SetEmptyDisplayList(const wr::PipelineId& aPipelineId,
                           wr::TransactionBuilder& aTxn,
                           wr::TransactionBuilder& aTxnForImageBridge);

  void AppendImageCompositeNotification(const ImageCompositeNotificationInfo& aNotification)
  {
    mImageCompositeNotifications.AppendElement(aNotification);
  }

  void FlushImageNotifications(nsTArray<ImageCompositeNotificationInfo>* aNotifications)
  {
    aNotifications->AppendElements(std::move(mImageCompositeNotifications));
  }

  void SetWillGenerateFrame();
  bool GetAndResetWillGenerateFrame();

  wr::ExternalImageId GetNextExternalImageId();

private:
  void ProcessPipelineRendered(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch);
  void ProcessPipelineRemoved(const wr::PipelineId& aPipelineId);

  wr::Epoch GetNextImageEpoch();
  uint32_t GetNextResourceId() { return ++mResourceId; }
  wr::IdNamespace GetNamespace() { return mIdNamespace; }
  wr::ImageKey GenerateImageKey()
  {
    wr::ImageKey key;
    key.mNamespace = GetNamespace();
    key.mHandle = GetNextResourceId();
    return key;
  }

  struct ForwardingTextureHost {
    ForwardingTextureHost(const wr::Epoch& aEpoch, TextureHost* aTexture)
      : mEpoch(aEpoch)
      , mTexture(aTexture)
    {}
    wr::Epoch mEpoch;
    CompositableTextureHostRef mTexture;
  };

  struct ForwardingTextureHostWrapper {
    ForwardingTextureHostWrapper(const wr::Epoch& aEpoch, WebRenderTextureHostWrapper* aWrTextureWrapper)
      : mEpoch(aEpoch)
      , mWrTextureWrapper(aWrTextureWrapper)
    {}
    wr::Epoch mEpoch;
    RefPtr<WebRenderTextureHostWrapper> mWrTextureWrapper;
  };

  struct ForwardingExternalImage {
    ForwardingExternalImage(const wr::Epoch& aEpoch, const wr::ExternalImageId& aImageId)
      : mEpoch(aEpoch)
      , mImageId(aImageId)
    {}
    wr::Epoch mEpoch;
    wr::ExternalImageId mImageId;
  };

  struct PipelineTexturesHolder {
    // Holds forwarding WebRenderTextureHosts.
    std::queue<ForwardingTextureHost> mTextureHosts;
    std::queue<ForwardingTextureHostWrapper> mTextureHostWrappers;
    std::queue<ForwardingExternalImage> mExternalImages;
    Maybe<wr::Epoch> mDestroyedEpoch;
  };

  struct AsyncImagePipeline {
    AsyncImagePipeline();
    void Update(const LayoutDeviceRect& aScBounds,
                const gfx::Matrix4x4& aScTransform,
                const gfx::MaybeIntSize& aScaleToSize,
                const wr::ImageRendering& aFilter,
                const wr::MixBlendMode& aMixBlendMode)
    {
      mIsChanged |= !mScBounds.IsEqualEdges(aScBounds) ||
                    mScTransform != aScTransform ||
                    mScaleToSize != aScaleToSize ||
                    mFilter != aFilter ||
                    mMixBlendMode != aMixBlendMode;
      mScBounds = aScBounds;
      mScTransform = aScTransform;
      mScaleToSize = aScaleToSize;
      mFilter = aFilter;
      mMixBlendMode = aMixBlendMode;
    }

    bool mInitialised;
    bool mIsChanged;
    bool mUseExternalImage;
    LayoutDeviceRect mScBounds;
    gfx::Matrix4x4 mScTransform;
    gfx::MaybeIntSize mScaleToSize;
    wr::ImageRendering mFilter;
    wr::MixBlendMode mMixBlendMode;
    RefPtr<WebRenderImageHost> mImageHost;
    CompositableTextureHostRef mCurrentTexture;
    RefPtr<WebRenderTextureHostWrapper> mWrTextureWrapper;
    nsTArray<wr::ImageKey> mKeys;
  };

  void ApplyAsyncImageForPipeline(const wr::Epoch& aEpoch,
                                  const wr::PipelineId& aPipelineId,
                                  AsyncImagePipeline* aPipeline,
                                  wr::TransactionBuilder& aSceneBuilderTxn,
                                  wr::TransactionBuilder& aMaybeFastTxn);
  Maybe<TextureHost::ResourceUpdateOp>
  UpdateImageKeys(const wr::Epoch& aEpoch,
                  const wr::PipelineId& aPipelineId,
                  AsyncImagePipeline* aPipeline,
                  nsTArray<wr::ImageKey>& aKeys,
                  wr::TransactionBuilder& aSceneBuilderTxn,
                  wr::TransactionBuilder& aMaybeFastTxn);
  Maybe<TextureHost::ResourceUpdateOp>
  UpdateWithoutExternalImage(TextureHost* aTexture,
                             wr::ImageKey aKey,
                             TextureHost::ResourceUpdateOp,
                             wr::TransactionBuilder& aTxn);

  RefPtr<wr::WebRenderAPI> mApi;
  wr::IdNamespace mIdNamespace;
  uint32_t mResourceId;

  nsClassHashtable<nsUint64HashKey, PipelineTexturesHolder> mPipelineTexturesHolders;
  nsClassHashtable<nsUint64HashKey, AsyncImagePipeline> mAsyncImagePipelines;
  wr::Epoch mAsyncImageEpoch;
  bool mWillGenerateFrame;
  bool mDestroyed;

  // Render time for the current composition.
  TimeStamp mCompositionTime;

  // When nonnull, during rendering, some compositable indicated that it will
  // change its rendering at this time. In order not to miss it, we composite
  // on every vsync until this time occurs (this is the latest such time).
  TimeStamp mCompositeUntilTime;

  nsTArray<ImageCompositeNotificationInfo> mImageCompositeNotifications;

  // The lock that protects mUpdatesQueue
  Mutex mUpdatesLock;
  // Used for checking if PipelineUpdates could be processed.
  Atomic<uint64_t> mUpdatesCount;
  struct PipelineUpdates {
    PipelineUpdates(const uint64_t aUpdatesCount, const bool aRendered)
      : mUpdatesCount(aUpdatesCount)
      , mRendered(aRendered)
    {}
    bool NeedsToWait(const uint64_t aUpdatesCount) {
      MOZ_ASSERT(mUpdatesCount <= aUpdatesCount);
      // XXX Add support of delaying releasing TextureHosts for multiple rendering.
      // See Bug 1500017.
      if (mUpdatesCount == aUpdatesCount && !mRendered) {
        // RenderTextureHosts related to this might be still used by GPU.
        return true;
      }
      return false;
    }
    const uint64_t mUpdatesCount;
    const bool mRendered;
    // Queue to store rendered pipeline epoch information. This is populated from
    // the Renderer thread after a render, and is read from the compositor thread
    // to free resources (e.g. textures) that are no longer needed. Each entry
    // in the queue is a pair that holds the pipeline id and Some(x) for
    // a render of epoch x, or Nothing() for a removed pipeline.
    std::queue<std::pair<wr::PipelineId, Maybe<wr::Epoch>>> mQueue;
  };
  std::queue<UniquePtr<PipelineUpdates>> mUpdatesQueues;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
