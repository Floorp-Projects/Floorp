/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H
#define MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H

#include <vector>

#include "CompositableHost.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/Maybe.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsClassHashtable.h"

namespace mozilla {

namespace wr {
class DisplayListBuilder;
class WebRenderAPI;
class WebRenderPipelineInfo;
}  // namespace wr

namespace layers {

class CompositableHost;
class CompositorVsyncScheduler;
class WebRenderImageHost;
class WebRenderTextureHost;

class AsyncImagePipelineManager final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncImagePipelineManager)

  explicit AsyncImagePipelineManager(RefPtr<wr::WebRenderAPI>&& aApi,
                                     bool aUseCompositorWnd);

 protected:
  ~AsyncImagePipelineManager();

 public:
  void Destroy();

  bool UseCompositorWnd() const { return mUseCompositorWnd; }

  void AddPipeline(const wr::PipelineId& aPipelineId,
                   WebRenderBridgeParent* aWrBridge);
  void RemovePipeline(const wr::PipelineId& aPipelineId,
                      const wr::Epoch& aEpoch);
  WebRenderBridgeParent* GetWrBridge(const wr::PipelineId& aPipelineId);

  void HoldExternalImage(const wr::PipelineId& aPipelineId,
                         const wr::Epoch& aEpoch, TextureHost* aTexture);
  void HoldExternalImage(const wr::PipelineId& aPipelineId,
                         const wr::Epoch& aEpoch,
                         const wr::ExternalImageId& aImageId);

  // This is called from the Renderer thread to notify this class about the
  // pipelines in the most recently completed update.
  // @param aInfo PipelineInfo for the update
  // @param aLatestFrameId RenderedFrameId if a frame has been submitted for
  //                       rendering, invalid if not
  // @param aLastCompletedFrameId RenderedFrameId for the last completed frame
  void NotifyPipelinesUpdated(RefPtr<const wr::WebRenderPipelineInfo> aInfo,
                              wr::RenderedFrameId aLatestFrameId,
                              wr::RenderedFrameId aLastCompletedFrameId,
                              ipc::FileDescriptor&& aFenceFd);

  // This is run on the compositor thread to process mRenderSubmittedUpdates. We
  // make this public because we need to invoke it from other places.
  void ProcessPipelineUpdates();

  TimeStamp GetCompositionTime() const { return mCompositionTime; }
  CompositionOpportunityId GetCompositionOpportunityId() const {
    return mCompositionOpportunityId;
  }

  void SetCompositionInfo(TimeStamp aTimeStamp,
                          CompositionOpportunityId aCompositionOpportunityId) {
    mCompositionTime = aTimeStamp;
    mCompositionOpportunityId = aCompositionOpportunityId;
    if (!mCompositionTime.IsNull() && !mCompositeUntilTime.IsNull() &&
        mCompositionTime >= mCompositeUntilTime) {
      mCompositeUntilTime = TimeStamp();
    }
  }
  void CompositeUntil(TimeStamp aTimeStamp) {
    if (mCompositeUntilTime.IsNull() || mCompositeUntilTime < aTimeStamp) {
      mCompositeUntilTime = aTimeStamp;
    }
  }
  TimeStamp GetCompositeUntilTime() const { return mCompositeUntilTime; }

  void AddAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                             WebRenderImageHost* aImageHost);
  void RemoveAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                wr::TransactionBuilder& aTxn);

  void UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                const LayoutDeviceRect& aScBounds,
                                VideoInfo::Rotation aRotation,
                                const wr::ImageRendering& aFilter,
                                const wr::MixBlendMode& aMixBlendMode);
  void ApplyAsyncImagesOfImageBridge(wr::TransactionBuilder& aSceneBuilderTxn,
                                     wr::TransactionBuilder& aFastTxn);
  void ApplyAsyncImageForPipeline(const wr::PipelineId& aPipelineId,
                                  wr::TransactionBuilder& aTxn,
                                  wr::TransactionBuilder& aTxnForImageBridge);

  void SetEmptyDisplayList(const wr::PipelineId& aPipelineId,
                           wr::TransactionBuilder& aTxn,
                           wr::TransactionBuilder& aTxnForImageBridge);

  void AppendImageCompositeNotification(
      const ImageCompositeNotificationInfo& aNotification) {
    mImageCompositeNotifications.AppendElement(aNotification);
  }

  void FlushImageNotifications(
      nsTArray<ImageCompositeNotificationInfo>* aNotifications) {
    aNotifications->AppendElements(std::move(mImageCompositeNotifications));
  }

  void SetWillGenerateFrame();
  bool GetAndResetWillGenerateFrame();

  static wr::ExternalImageId GetNextExternalImageId();

 private:
  void ProcessPipelineRendered(const wr::PipelineId& aPipelineId,
                               const wr::Epoch& aEpoch,
                               wr::RenderedFrameId aRenderedFrameId);
  void ProcessPipelineRemoved(const wr::RemovedPipeline& aRemovedPipeline,
                              wr::RenderedFrameId aRenderedFrameId);

  wr::Epoch GetNextImageEpoch();
  uint32_t GetNextResourceId() { return ++mResourceId; }
  wr::IdNamespace GetNamespace() { return mIdNamespace; }
  wr::ImageKey GenerateImageKey() {
    wr::ImageKey key;
    key.mNamespace = GetNamespace();
    key.mHandle = GetNextResourceId();
    return key;
  }

  struct ForwardingTextureHost {
    ForwardingTextureHost(const wr::Epoch& aEpoch, TextureHost* aTexture)
        : mEpoch(aEpoch), mTexture(aTexture) {}
    wr::Epoch mEpoch;
    CompositableTextureHostRef mTexture;
  };

  struct ForwardingExternalImage {
    ForwardingExternalImage(const wr::Epoch& aEpoch,
                            const wr::ExternalImageId& aImageId)
        : mEpoch(aEpoch), mImageId(aImageId) {}
    ~ForwardingExternalImage();
    wr::Epoch mEpoch;
    wr::ExternalImageId mImageId;
  };

  struct PipelineTexturesHolder {
    // Holds forwarding WebRenderTextureHosts.
    std::vector<ForwardingTextureHost> mTextureHostsUntilRenderSubmitted;
    // TextureHosts that must be held until rendering has completed. UniquePtr
    // is used to make the entries movable, ideally ForwardingTextureHost would
    // be fully movable.
    std::vector<UniquePtr<ForwardingTextureHost>>
        mTextureHostsUntilRenderCompleted;
    std::vector<UniquePtr<ForwardingExternalImage>> mExternalImages;
    Maybe<wr::Epoch> mDestroyedEpoch;
    WebRenderBridgeParent* MOZ_NON_OWNING_REF mWrBridge = nullptr;
  };

  struct AsyncImagePipeline {
    AsyncImagePipeline(wr::PipelineId aPipelineId,
                       layers::WebRenderBackend aBackend);
    void Update(const LayoutDeviceRect& aScBounds,
                VideoInfo::Rotation aRotation,
                const wr::ImageRendering& aFilter,
                const wr::MixBlendMode& aMixBlendMode) {
      mIsChanged |= !mScBounds.IsEqualEdges(aScBounds) ||
                    mRotation != aRotation || mFilter != aFilter ||
                    mMixBlendMode != aMixBlendMode;
      mScBounds = aScBounds;
      mRotation = aRotation;
      mFilter = aFilter;
      mMixBlendMode = aMixBlendMode;
    }

    bool mInitialised;
    bool mIsChanged;
    bool mUseExternalImage;
    LayoutDeviceRect mScBounds;
    VideoInfo::Rotation mRotation;
    wr::ImageRendering mFilter;
    wr::MixBlendMode mMixBlendMode;
    RefPtr<WebRenderImageHost> mImageHost;
    CompositableTextureHostRef mCurrentTexture;
    nsTArray<wr::ImageKey> mKeys;
    wr::DisplayListBuilder mDLBuilder;
  };

  void ApplyAsyncImageForPipeline(const wr::Epoch& aEpoch,
                                  const wr::PipelineId& aPipelineId,
                                  AsyncImagePipeline* aPipeline,
                                  wr::TransactionBuilder& aSceneBuilderTxn,
                                  wr::TransactionBuilder& aMaybeFastTxn);
  Maybe<TextureHost::ResourceUpdateOp> UpdateImageKeys(
      const wr::Epoch& aEpoch, const wr::PipelineId& aPipelineId,
      AsyncImagePipeline* aPipeline, nsTArray<wr::ImageKey>& aKeys,
      wr::TransactionBuilder& aSceneBuilderTxn,
      wr::TransactionBuilder& aMaybeFastTxn);
  Maybe<TextureHost::ResourceUpdateOp> UpdateWithoutExternalImage(
      TextureHost* aTexture, wr::ImageKey aKey, TextureHost::ResourceUpdateOp,
      wr::TransactionBuilder& aTxn);

  void CheckForTextureHostsNotUsedByGPU();

  RefPtr<wr::WebRenderAPI> mApi;
  bool mUseCompositorWnd;

  const wr::IdNamespace mIdNamespace;
  const bool mUseTripleBuffering;
  uint32_t mResourceId;

  nsClassHashtable<nsUint64HashKey, PipelineTexturesHolder>
      mPipelineTexturesHolders;
  nsClassHashtable<nsUint64HashKey, AsyncImagePipeline> mAsyncImagePipelines;
  wr::Epoch mAsyncImageEpoch;
  bool mWillGenerateFrame;
  bool mDestroyed;

#ifdef XP_WIN
  bool mUseWebRenderDCompVideoOverlayWin;
#endif

  // Render time for the current composition.
  TimeStamp mCompositionTime;

  // CompositionOpportunityId of the current composition.
  CompositionOpportunityId mCompositionOpportunityId;

  // When nonnull, during rendering, some compositable indicated that it will
  // change its rendering at this time. In order not to miss it, we composite
  // on every vsync until this time occurs (this is the latest such time).
  TimeStamp mCompositeUntilTime;

  nsTArray<ImageCompositeNotificationInfo> mImageCompositeNotifications;

  struct WebRenderPipelineInfoHolder {
    WebRenderPipelineInfoHolder(RefPtr<const wr::WebRenderPipelineInfo>&& aInfo,
                                ipc::FileDescriptor&& aFenceFd);
    ~WebRenderPipelineInfoHolder();
    RefPtr<const wr::WebRenderPipelineInfo> mInfo;
    ipc::FileDescriptor mFenceFd;
  };

  std::vector<std::pair<wr::RenderedFrameId, WebRenderPipelineInfoHolder>>
      mRenderSubmittedUpdates;
  Mutex mRenderSubmittedUpdatesLock;

  Atomic<uint64_t> mLastCompletedFrameId;
  std::vector<std::pair<wr::RenderedFrameId,
                        std::vector<UniquePtr<ForwardingTextureHost>>>>
      mTexturesInUseByGPU;
  ipc::FileDescriptor mReleaseFenceFd;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_WEBRENDERCOMPOSITABLE_HOLDER_H */
