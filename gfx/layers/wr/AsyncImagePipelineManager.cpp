/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncImagePipelineManager.h"

#include <algorithm>
#include <iterator>

#include "CompositableHost.h"
#include "gfxEnv.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace mozilla {
namespace layers {

AsyncImagePipelineManager::ForwardingExternalImage::~ForwardingExternalImage() {
  DebugOnly<bool> released = SharedSurfacesParent::Release(mImageId);
  MOZ_ASSERT(released);
}

AsyncImagePipelineManager::AsyncImagePipeline::AsyncImagePipeline()
    : mInitialised(false),
      mRenderRoot(wr::RenderRoot::Default),
      mIsChanged(false),
      mUseExternalImage(false),
      mFilter(wr::ImageRendering::Auto),
      mMixBlendMode(wr::MixBlendMode::Normal) {}

AsyncImagePipelineManager::AsyncImagePipelineManager(
    nsTArray<RefPtr<wr::WebRenderAPI>>&& aApis, bool aUseCompositorWnd)
    : mApis(aApis),
      mUseCompositorWnd(aUseCompositorWnd),
      mIdNamespace(mApis[0]->GetNamespace()),
      mUseTripleBuffering(mApis[0]->GetUseTripleBuffering()),
      mResourceId(0),
      mAsyncImageEpoch{0},
      mWillGenerateFrame{},
      mDestroyed(false),
      mRenderSubmittedUpdatesLock("SubmittedUpdatesLock"),
      mLastCompletedFrameId(0) {
  MOZ_COUNT_CTOR(AsyncImagePipelineManager);
}

AsyncImagePipelineManager::~AsyncImagePipelineManager() {
  MOZ_COUNT_DTOR(AsyncImagePipelineManager);
}

void AsyncImagePipelineManager::Destroy() {
  MOZ_ASSERT(!mDestroyed);
  mApis.Clear();
  mPipelineTexturesHolders.Clear();
  mDestroyed = true;
}

void AsyncImagePipelineManager::SetWillGenerateFrameAllRenderRoots() {
  for (auto renderRoot : wr::kRenderRoots) {
    SetWillGenerateFrame(renderRoot);
  }
}

void AsyncImagePipelineManager::SetWillGenerateFrame(
    wr::RenderRoot aRenderRoot) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mWillGenerateFrame[aRenderRoot] = true;
}

bool AsyncImagePipelineManager::GetAndResetWillGenerateFrame(
    wr::RenderRoot aRenderRoot) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  bool ret = mWillGenerateFrame[aRenderRoot];
  mWillGenerateFrame[aRenderRoot] = false;
  return ret;
}

void AsyncImagePipelineManager::AddPipeline(const wr::PipelineId& aPipelineId,
                                            WebRenderBridgeParent* aWrBridge) {
  if (mDestroyed) {
    return;
  }
  uint64_t id = wr::AsUint64(aPipelineId);

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  if (holder) {
    // This could happen during tab move between different windows.
    // Previously removed holder could be still alive for waiting destroyed.
    MOZ_ASSERT(holder->mDestroyedEpoch.isSome());
    holder->mDestroyedEpoch = Nothing();  // Revive holder
    holder->mWrBridge = aWrBridge;
    return;
  }
  holder = new PipelineTexturesHolder();
  holder->mWrBridge = aWrBridge;
  mPipelineTexturesHolders.Put(id, holder);
}

void AsyncImagePipelineManager::RemovePipeline(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch) {
  if (mDestroyed) {
    return;
  }

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  holder->mWrBridge = nullptr;
  holder->mDestroyedEpoch = Some(aEpoch);
}

WebRenderBridgeParent* AsyncImagePipelineManager::GetWrBridge(
    const wr::PipelineId& aPipelineId) {
  if (mDestroyed) {
    return nullptr;
  }

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  if (!holder) {
    return nullptr;
  }
  if (holder->mWrBridge) {
    MOZ_ASSERT(holder->mDestroyedEpoch.isNothing());
    return holder->mWrBridge;
  }

  return nullptr;
}

void AsyncImagePipelineManager::AddAsyncImagePipeline(
    const wr::PipelineId& aPipelineId, WebRenderImageHost* aImageHost,
    wr::RenderRoot aRenderRoot) {
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aImageHost);
  uint64_t id = wr::AsUint64(aPipelineId);

  MOZ_ASSERT(!mAsyncImagePipelines.Get(id));
  AsyncImagePipeline* holder = new AsyncImagePipeline();
  holder->mRenderRoot = aRenderRoot;
  holder->mImageHost = aImageHost;
  mAsyncImagePipelines.Put(id, holder);
  AddPipeline(aPipelineId, /* aWrBridge */ nullptr);
}

void AsyncImagePipelineManager::RemoveAsyncImagePipeline(
    const wr::PipelineId& aPipelineId, wr::TransactionBuilder& aTxn) {
  if (mDestroyed) {
    return;
  }

  uint64_t id = wr::AsUint64(aPipelineId);
  if (auto entry = mAsyncImagePipelines.Lookup(id)) {
    const auto& holder = entry.Data();
    wr::Epoch epoch = GetNextImageEpoch();
    aTxn.ClearDisplayList(epoch, aPipelineId);
    for (wr::ImageKey key : holder->mKeys) {
      aTxn.DeleteImage(key);
    }
    entry.Remove();
    RemovePipeline(aPipelineId, epoch);
  }
}

void AsyncImagePipelineManager::UpdateAsyncImagePipeline(
    const wr::PipelineId& aPipelineId, const LayoutDeviceRect& aScBounds,
    const gfx::Matrix4x4& aScTransform, const gfx::MaybeIntSize& aScaleToSize,
    const wr::ImageRendering& aFilter, const wr::MixBlendMode& aMixBlendMode) {
  if (mDestroyed) {
    return;
  }
  AsyncImagePipeline* pipeline =
      mAsyncImagePipelines.Get(wr::AsUint64(aPipelineId));
  if (!pipeline) {
    return;
  }
  pipeline->mInitialised = true;
  pipeline->Update(aScBounds, aScTransform, aScaleToSize, aFilter,
                   aMixBlendMode);
}

Maybe<TextureHost::ResourceUpdateOp> AsyncImagePipelineManager::UpdateImageKeys(
    const wr::Epoch& aEpoch, const wr::PipelineId& aPipelineId,
    AsyncImagePipeline* aPipeline, nsTArray<wr::ImageKey>& aKeys,
    wr::TransactionBuilder& aSceneBuilderTxn,
    wr::TransactionBuilder& aMaybeFastTxn) {
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aPipeline);

  TextureHost* texture =
      aPipeline->mImageHost->GetAsTextureHostForComposite(this);
  TextureHost* previousTexture = aPipeline->mCurrentTexture.get();

  if (texture == previousTexture) {
    // The texture has not changed, just reuse previous ImageKeys.
    aKeys = aPipeline->mKeys;
    return Nothing();
  }

  if (!texture || texture->NumSubTextures() == 0) {
    // We don't have a new texture or texture does not have SubTextures, there
    // isn't much we can do.
    aKeys = aPipeline->mKeys;
    return Nothing();
  }

  aPipeline->mCurrentTexture = texture;

  WebRenderTextureHost* wrTexture = texture->AsWebRenderTextureHost();
  MOZ_ASSERT(wrTexture);
  if (!wrTexture) {
    gfxCriticalNote << "WebRenderTextureHost is not used";
  }

  bool useExternalImage = !!wrTexture;
  aPipeline->mUseExternalImage = useExternalImage;

  // The non-external image code path falls back to converting the texture into
  // an rgb image.
  auto numKeys = useExternalImage ? texture->NumSubTextures() : 1;
  MOZ_ASSERT(numKeys > 0);

  // If we already had a texture and the format hasn't changed, better to reuse
  // the image keys than create new ones.
  bool canUpdate = !!previousTexture &&
                   previousTexture->GetSize() == texture->GetSize() &&
                   previousTexture->GetFormat() == texture->GetFormat() &&
                   previousTexture->NeedsYFlip() == texture->NeedsYFlip() &&
                   aPipeline->mKeys.Length() == numKeys;

  if (!canUpdate) {
    for (auto key : aPipeline->mKeys) {
      // Destroy ImageKeys on transaction of scene builder thread, since
      // DisplayList is updated on SceneBuilder thread. It prevents too early
      // ImageKey deletion.
      aSceneBuilderTxn.DeleteImage(key);
    }
    aPipeline->mKeys.Clear();
    for (uint32_t i = 0; i < numKeys; ++i) {
      aPipeline->mKeys.AppendElement(GenerateImageKey());
    }
  }

  aKeys = aPipeline->mKeys;

  auto op = canUpdate ? TextureHost::UPDATE_IMAGE : TextureHost::ADD_IMAGE;

  if (!useExternalImage) {
    return UpdateWithoutExternalImage(texture, aKeys[0], op, aMaybeFastTxn);
  }

  Range<wr::ImageKey> keys(&aKeys[0], aKeys.Length());
  auto externalImageKey = wrTexture->GetExternalImageKey();
  wrTexture->PushResourceUpdates(aMaybeFastTxn, op, keys, externalImageKey);

  return Some(op);
}

Maybe<TextureHost::ResourceUpdateOp>
AsyncImagePipelineManager::UpdateWithoutExternalImage(
    TextureHost* aTexture, wr::ImageKey aKey, TextureHost::ResourceUpdateOp aOp,
    wr::TransactionBuilder& aTxn) {
  MOZ_ASSERT(aTexture);

  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    NS_ERROR("TextureHost does not return DataSourceSurface");
    return Nothing();
  }
  gfx::DataSourceSurface::MappedSurface map;
  if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
    NS_ERROR("DataSourceSurface failed to map");
    return Nothing();
  }

  gfx::IntSize size = dSurf->GetSize();
  wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());

  // Costly copy right here...
  wr::Vec<uint8_t> bytes;
  bytes.PushBytes(Range<uint8_t>(map.mData, size.height * map.mStride));

  if (aOp == TextureHost::UPDATE_IMAGE) {
    aTxn.UpdateImageBuffer(aKey, descriptor, bytes);
  } else {
    aTxn.AddImage(aKey, descriptor, bytes);
  }

  dSurf->Unmap();

  return Some(aOp);
}

void AsyncImagePipelineManager::ApplyAsyncImagesOfImageBridge(
    wr::RenderRootArray<Maybe<wr::TransactionBuilder>>& aSceneBuilderTxns,
    wr::RenderRootArray<Maybe<wr::TransactionBuilder>>& aFastTxns) {
  if (mDestroyed || mAsyncImagePipelines.Count() == 0) {
    return;
  }

  wr::Epoch epoch = GetNextImageEpoch();

  // We use a pipeline with a very small display list for each video element.
  // Update each of them if needed.
  for (auto iter = mAsyncImagePipelines.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    AsyncImagePipeline* pipeline = iter.UserData();
    // If aync image pipeline does not use ImageBridge, do not need to apply.
    if (!pipeline->mImageHost->GetAsyncRef()) {
      continue;
    }
    ApplyAsyncImageForPipeline(epoch, pipelineId, pipeline,
                               *aSceneBuilderTxns[pipeline->mRenderRoot],
                               *aFastTxns[pipeline->mRenderRoot]);
  }
}

void AsyncImagePipelineManager::ApplyAsyncImageForPipeline(
    const wr::Epoch& aEpoch, const wr::PipelineId& aPipelineId,
    AsyncImagePipeline* aPipeline, wr::TransactionBuilder& aSceneBuilderTxn,
    wr::TransactionBuilder& aMaybeFastTxn) {
  nsTArray<wr::ImageKey> keys;
  auto op = UpdateImageKeys(aEpoch, aPipelineId, aPipeline, keys,
                            aSceneBuilderTxn, aMaybeFastTxn);

  bool updateDisplayList =
      aPipeline->mInitialised &&
      (aPipeline->mIsChanged || op == Some(TextureHost::ADD_IMAGE)) &&
      !!aPipeline->mCurrentTexture;

  if (!updateDisplayList) {
    // We don't need to update the display list, either because we can't or
    // because the previous one is still up to date. We may, however, have
    // updated some resources.

    // Use transaction of scene builder thread to notify epoch.
    // It is for making epoch update consistent.
    aSceneBuilderTxn.UpdateEpoch(aPipelineId, aEpoch);
    if (aPipeline->mCurrentTexture) {
      HoldExternalImage(aPipelineId, aEpoch, aPipeline->mCurrentTexture);
    }
    return;
  }

  aPipeline->mIsChanged = false;

  gfx::Matrix4x4 scTransform = aPipeline->mScTransform;
  if (aPipeline->mCurrentTexture && aPipeline->mCurrentTexture->NeedsYFlip()) {
    scTransform.PreTranslate(0, aPipeline->mCurrentTexture->GetSize().height, 0)
        .PreScale(1, -1, 1);
  }

  wr::LayoutSize contentSize{aPipeline->mScBounds.Width(),
                             aPipeline->mScBounds.Height()};
  wr::DisplayListBuilder builder(aPipelineId, contentSize);

  float opacity = 1.0f;
  wr::StackingContextParams params;
  params.opacity = &opacity;
  params.mTransformPtr = scTransform.IsIdentity() ? nullptr : &scTransform;
  params.mix_blend_mode = aPipeline->mMixBlendMode;

  Maybe<wr::WrSpatialId> referenceFrameId = builder.PushStackingContext(
      params, wr::ToLayoutRect(aPipeline->mScBounds),
      // This is fine to do unconditionally because we only push images here.
      wr::RasterSpace::Screen());

  Maybe<wr::SpaceAndClipChainHelper> spaceAndClipChainHelper;
  if (referenceFrameId) {
    spaceAndClipChainHelper.emplace(builder, referenceFrameId.ref());
  }

  if (aPipeline->mCurrentTexture && !keys.IsEmpty()) {
    LayoutDeviceRect rect(0, 0, aPipeline->mCurrentTexture->GetSize().width,
                          aPipeline->mCurrentTexture->GetSize().height);
    if (aPipeline->mScaleToSize.isSome()) {
      rect = LayoutDeviceRect(0, 0, aPipeline->mScaleToSize.value().width,
                              aPipeline->mScaleToSize.value().height);
    }

    if (aPipeline->mUseExternalImage) {
      MOZ_ASSERT(aPipeline->mCurrentTexture->AsWebRenderTextureHost());
      Range<wr::ImageKey> range_keys(&keys[0], keys.Length());
      aPipeline->mCurrentTexture->PushDisplayItems(
          builder, wr::ToLayoutRect(rect), wr::ToLayoutRect(rect),
          aPipeline->mFilter, range_keys, /* aPreferCompositorSurface */ true);
      HoldExternalImage(aPipelineId, aEpoch, aPipeline->mCurrentTexture);
    } else {
      MOZ_ASSERT(keys.Length() == 1);
      builder.PushImage(wr::ToLayoutRect(rect), wr::ToLayoutRect(rect), true,
                        aPipeline->mFilter, keys[0]);
    }
  }

  spaceAndClipChainHelper.reset();
  builder.PopStackingContext(referenceFrameId.isSome());

  wr::BuiltDisplayList dl;
  wr::LayoutSize builderContentSize;
  builder.Finalize(builderContentSize, dl);
  aSceneBuilderTxn.SetDisplayList(gfx::DeviceColor(0.f, 0.f, 0.f, 0.f), aEpoch,
                                  wr::ToLayoutSize(aPipeline->mScBounds.Size()),
                                  aPipelineId, builderContentSize, dl.dl_desc,
                                  dl.dl);
}

void AsyncImagePipelineManager::ApplyAsyncImageForPipeline(
    const wr::PipelineId& aPipelineId, wr::TransactionBuilder& aTxn,
    wr::TransactionBuilder& aTxnForImageBridge, wr::RenderRoot aRenderRoot) {
  AsyncImagePipeline* pipeline =
      mAsyncImagePipelines.Get(wr::AsUint64(aPipelineId));
  if (!pipeline) {
    return;
  }
  wr::WebRenderAPI* api = mApis[(size_t)pipeline->mRenderRoot];
  wr::TransactionBuilder fastTxn(/* aUseSceneBuilderThread */ false);
  wr::AutoTransactionSender sender(api, &fastTxn);

  // Transaction for async image pipeline that uses ImageBridge always need to
  // be non low priority.
  auto& sceneBuilderTxn =
      pipeline->mImageHost->GetAsyncRef() ? aTxnForImageBridge : aTxn;

  // Use transaction of using non scene builder thread when ImageHost uses
  // ImageBridge. ApplyAsyncImagesOfImageBridge() handles transaction of adding
  // and updating wr::ImageKeys of ImageHosts that uses ImageBridge. Then
  // AsyncImagePipelineManager always needs to use non scene builder thread
  // transaction for adding and updating wr::ImageKeys of ImageHosts that uses
  // ImageBridge. Otherwise, ordering of wr::ImageKeys updating in webrender
  // becomes inconsistent.
  auto& maybeFastTxn = pipeline->mImageHost->GetAsyncRef() ? fastTxn : aTxn;

  wr::Epoch epoch = GetNextImageEpoch();

  ApplyAsyncImageForPipeline(epoch, aPipelineId, pipeline, sceneBuilderTxn,
                             maybeFastTxn);
}

void AsyncImagePipelineManager::SetEmptyDisplayList(
    const wr::PipelineId& aPipelineId, wr::TransactionBuilder& aTxn,
    wr::TransactionBuilder& aTxnForImageBridge) {
  AsyncImagePipeline* pipeline =
      mAsyncImagePipelines.Get(wr::AsUint64(aPipelineId));
  if (!pipeline) {
    return;
  }

  // Transaction for async image pipeline that uses ImageBridge always need to
  // be non low priority.
  auto& txn = pipeline->mImageHost->GetAsyncRef() ? aTxnForImageBridge : aTxn;

  wr::Epoch epoch = GetNextImageEpoch();
  wr::LayoutSize contentSize{pipeline->mScBounds.Width(),
                             pipeline->mScBounds.Height()};
  wr::DisplayListBuilder builder(aPipelineId, contentSize);

  wr::BuiltDisplayList dl;
  wr::LayoutSize builderContentSize;
  builder.Finalize(builderContentSize, dl);
  txn.SetDisplayList(gfx::DeviceColor(0.f, 0.f, 0.f, 0.f), epoch,
                     wr::ToLayoutSize(pipeline->mScBounds.Size()), aPipelineId,
                     builderContentSize, dl.dl_desc, dl.dl);
}

void AsyncImagePipelineManager::HoldExternalImage(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    TextureHost* aTexture) {
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aTexture);

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  if (aTexture->HasIntermediateBuffer()) {
    // Hold WebRenderTextureHost until submitted for rendering if it has an
    // intermediate buffer.
    holder->mTextureHostsUntilRenderSubmitted.emplace_back(aEpoch, aTexture);
  } else {
    // Hold WebRenderTextureHost until rendering completed if not.
    holder->mTextureHostsUntilRenderCompleted.emplace_back(
        MakeUnique<ForwardingTextureHost>(aEpoch, aTexture));
  }
}

void AsyncImagePipelineManager::HoldExternalImage(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    const wr::ExternalImageId& aImageId) {
  if (mDestroyed) {
    SharedSurfacesParent::Release(aImageId);
    return;
  }

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    SharedSurfacesParent::Release(aImageId);
    return;
  }

  holder->mExternalImages.emplace_back(
      MakeUnique<ForwardingExternalImage>(aEpoch, aImageId));
}

void AsyncImagePipelineManager::NotifyPipelinesUpdated(
    RefPtr<const wr::WebRenderPipelineInfo> aInfo,
    wr::RenderedFrameId aLatestFrameId,
    wr::RenderedFrameId aLastCompletedFrameId) {
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());
  MOZ_ASSERT(mLastCompletedFrameId <= aLastCompletedFrameId.mId);
  MOZ_ASSERT(aLatestFrameId.IsValid());

  // This is called on the render thread, so we just stash the data into
  // mPendingUpdates and process it later on the compositor thread.
  mPendingUpdates.push_back(std::move(aInfo));
  mLastCompletedFrameId = aLastCompletedFrameId.mId;

  {
    // We need to lock for mRenderSubmittedUpdates because it can be accessed
    // on the compositor thread.
    MutexAutoLock lock(mRenderSubmittedUpdatesLock);

    // Move the pending updates into the submitted ones. Note that this clears
    // mPendingUpdates.
    mRenderSubmittedUpdates.emplace_back(aLatestFrameId,
                                         std::move(mPendingUpdates));
  }

  // Queue a runnable on the compositor thread to process the updates.
  // This will also call CheckForTextureHostsNotUsedByGPU.
  layers::CompositorThreadHolder::Loop()->PostTask(
      NewRunnableMethod("ProcessPipelineUpdates", this,
                        &AsyncImagePipelineManager::ProcessPipelineUpdates));
}

void AsyncImagePipelineManager::ProcessPipelineUpdates() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (mDestroyed) {
    return;
  }

  std::vector<std::pair<wr::RenderedFrameId, PipelineInfoVector>>
      submittedUpdates;
  {
    // We need to lock for mRenderSubmittedUpdates because it can be accessed on
    // the compositor thread.
    MutexAutoLock lock(mRenderSubmittedUpdatesLock);
    mRenderSubmittedUpdates.swap(submittedUpdates);
  }

  // submittedUpdates is a vector of RenderedFrameIds paired with vectors of
  // WebRenderPipelineInfo.
  for (auto update : submittedUpdates) {
    for (auto pipelineInfo : update.second) {
      auto& info = pipelineInfo->Raw();

      for (auto& epoch : info.epochs) {
        ProcessPipelineRendered(epoch.pipeline_id, epoch.epoch, update.first);
      }
      for (auto& removedPipeline : info.removed_pipelines) {
        ProcessPipelineRemoved(removedPipeline, update.first);
      }
    }
  }
  CheckForTextureHostsNotUsedByGPU();
}

void AsyncImagePipelineManager::ProcessPipelineRendered(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    wr::RenderedFrameId aRenderedFrameId) {
  if (auto entry = mPipelineTexturesHolders.Lookup(wr::AsUint64(aPipelineId))) {
    const auto& holder = entry.Data();
    // For TextureHosts that can be released on render submission, using aEpoch
    // find the first that we can't release and then release all prior to that.
    auto firstSubmittedHostToKeep = std::find_if(
        holder->mTextureHostsUntilRenderSubmitted.begin(),
        holder->mTextureHostsUntilRenderSubmitted.end(),
        [&aEpoch](const auto& entry) { return aEpoch <= entry.mEpoch; });
    holder->mTextureHostsUntilRenderSubmitted.erase(
        holder->mTextureHostsUntilRenderSubmitted.begin(),
        firstSubmittedHostToKeep);

    // For TextureHosts that need to wait until render completed, find the first
    // that is later than aEpoch and then move all prior to that to
    // mTexturesInUseByGPU paired with aRenderedFrameId.  These will be released
    // once rendering has completed for aRenderedFrameId.
    auto firstCompletedHostToKeep = std::find_if(
        holder->mTextureHostsUntilRenderCompleted.begin(),
        holder->mTextureHostsUntilRenderCompleted.end(),
        [&aEpoch](const auto& entry) { return aEpoch <= entry->mEpoch; });
    if (firstCompletedHostToKeep !=
        holder->mTextureHostsUntilRenderCompleted.begin()) {
      std::vector<UniquePtr<ForwardingTextureHost>> hostsUntilCompleted(
          std::make_move_iterator(
              holder->mTextureHostsUntilRenderCompleted.begin()),
          std::make_move_iterator(firstCompletedHostToKeep));
      mTexturesInUseByGPU.emplace_back(aRenderedFrameId,
                                       std::move(hostsUntilCompleted));
      holder->mTextureHostsUntilRenderCompleted.erase(
          holder->mTextureHostsUntilRenderCompleted.begin(),
          firstCompletedHostToKeep);
    }

    // Using aEpoch, find the first external image that we can't release and
    // then release all prior to that.
    auto firstImageToKeep = std::find_if(
        holder->mExternalImages.begin(), holder->mExternalImages.end(),
        [&aEpoch](const auto& entry) { return aEpoch <= entry->mEpoch; });
    holder->mExternalImages.erase(holder->mExternalImages.begin(),
                                  firstImageToKeep);
  }
}

void AsyncImagePipelineManager::ProcessPipelineRemoved(
    const wr::RemovedPipeline& aRemovedPipeline,
    wr::RenderedFrameId aRenderedFrameId) {
  if (mDestroyed) {
    return;
  }
  if (auto entry = mPipelineTexturesHolders.Lookup(
          wr::AsUint64(aRemovedPipeline.pipeline_id))) {
    const auto& holder = entry.Data();
    if (holder->mDestroyedEpoch.isSome()) {
      if (!holder->mTextureHostsUntilRenderCompleted.empty()) {
        // Move all TextureHosts that must be held until render completed to
        // mTexturesInUseByGPU paired with aRenderedFrameId.
        mTexturesInUseByGPU.emplace_back(
            aRenderedFrameId,
            std::move(holder->mTextureHostsUntilRenderCompleted));
      }

      // Remove Pipeline releasing all remaining TextureHosts and external
      // images.
      entry.Remove();
    }

    // If mDestroyedEpoch contains nothing it means we reused the same pipeline
    // id (probably because we moved the tab to another window). In this case we
    // need to keep the holder.
  }
}

void AsyncImagePipelineManager::CheckForTextureHostsNotUsedByGPU() {
  uint64_t lastCompletedFrameId = mLastCompletedFrameId;

  // Find first entry after mLastCompletedFrameId and release all prior ones.
  auto firstTexturesToKeep =
      std::find_if(mTexturesInUseByGPU.begin(), mTexturesInUseByGPU.end(),
                   [lastCompletedFrameId](const auto& entry) {
                     return lastCompletedFrameId < entry.first.mId;
                   });
  mTexturesInUseByGPU.erase(mTexturesInUseByGPU.begin(), firstTexturesToKeep);
}

wr::Epoch AsyncImagePipelineManager::GetNextImageEpoch() {
  mAsyncImageEpoch.mHandle++;
  return mAsyncImageEpoch;
}

}  // namespace layers
}  // namespace mozilla
