/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncImagePipelineManager.h"

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

AsyncImagePipelineManager::PipelineUpdates::PipelineUpdates(
    RefPtr<wr::WebRenderPipelineInfo> aPipelineInfo,
    const uint64_t aUpdatesCount, const bool aRendered)
    : mPipelineInfo(aPipelineInfo),
      mUpdatesCount(aUpdatesCount),
      mRendered(aRendered) {}

AsyncImagePipelineManager::AsyncImagePipelineManager(
    nsTArray<RefPtr<wr::WebRenderAPI>>&& aApis)
    : mApis(aApis),
      mIdNamespace(mApis[0]->GetNamespace()),
      mUseTripleBuffering(mApis[0]->GetUseTripleBuffering()),
      mResourceId(0),
      mAsyncImageEpoch{0},
      mWillGenerateFrame{},
      mDestroyed(false),
      mUpdatesLock("UpdatesLock"),
      mUpdatesCount(0) {
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

wr::ExternalImageId AsyncImagePipelineManager::GetNextExternalImageId() {
  static uint32_t sNextId = 0;
  ++sNextId;
  MOZ_RELEASE_ASSERT(sNextId != UINT32_MAX);
  // gecko allocates external image id as (IdNamespace:32bit +
  // ResourceId:32bit). And AsyncImagePipelineManager uses IdNamespace = 0.
  return wr::ToExternalImageId((uint64_t)sNextId);
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
    AsyncImagePipeline* holder = entry.Data();
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

  TextureHost* texture = aPipeline->mImageHost->GetAsTextureHostForComposite();
  TextureHost* previousTexture = aPipeline->mCurrentTexture.get();

  if (texture == previousTexture) {
    // The texture has not changed, just reuse previous ImageKeys.
    aKeys = aPipeline->mKeys;
    if (aPipeline->mWrTextureWrapper) {
      HoldExternalImage(aPipelineId, aEpoch, aPipeline->mWrTextureWrapper);
    }
    return Nothing();
  }

  if (!texture) {
    // We don't have a new texture, there isn't much we can do.
    aKeys = aPipeline->mKeys;
    if (aPipeline->mWrTextureWrapper) {
      HoldExternalImage(aPipelineId, aEpoch, aPipeline->mWrTextureWrapper);
    }
    return Nothing();
  }

  aPipeline->mCurrentTexture = texture;

  WebRenderTextureHost* wrTexture = texture->AsWebRenderTextureHost();
  MOZ_ASSERT(wrTexture);
  if (!wrTexture) {
    gfxCriticalNote << "WebRenderTextureHost is not used";
  }

  bool useExternalImage = !gfxEnv::EnableWebRenderRecording() && wrTexture;
  aPipeline->mUseExternalImage = useExternalImage;

  // Use WebRenderTextureHostWrapper only for video.
  // And WebRenderTextureHostWrapper could be used only with
  // WebRenderTextureHost that supports NativeTexture
  bool useWrTextureWrapper =
      useExternalImage && wrTexture && wrTexture->SupportsWrNativeTexture();

  // The non-external image code path falls back to converting the texture into
  // an rgb image.
  auto numKeys = useExternalImage ? texture->NumSubTextures() : 1;

  // If we already had a texture and the format hasn't changed, better to reuse
  // the image keys than create new ones.
  bool canUpdate = !!previousTexture &&
                   previousTexture->GetSize() == texture->GetSize() &&
                   previousTexture->GetFormat() == texture->GetFormat() &&
                   previousTexture->NeedsYFlip() == texture->NeedsYFlip() &&
                   aPipeline->mKeys.Length() == numKeys;

  // Check if WebRenderTextureHostWrapper could be reused.
  if (aPipeline->mWrTextureWrapper && (!useWrTextureWrapper || !canUpdate)) {
    aPipeline->mWrTextureWrapper = nullptr;
    canUpdate = false;
  }

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

  wrTexture->MaybeNofityForUse(aMaybeFastTxn);

  if (useWrTextureWrapper && aPipeline->mWrTextureWrapper) {
    MOZ_ASSERT(canUpdate);
    // Reuse WebRenderTextureHostWrapper. With it, rendered frame could be
    // updated without batch re-creation.
    aPipeline->mWrTextureWrapper->UpdateWebRenderTextureHost(aMaybeFastTxn,
                                                             wrTexture);
    // Ensure frame generation.
    SetWillGenerateFrame(aPipeline->mRenderRoot);
  } else {
    if (useWrTextureWrapper) {
      aPipeline->mWrTextureWrapper = new WebRenderTextureHostWrapper(this);
      aPipeline->mWrTextureWrapper->UpdateWebRenderTextureHost(aMaybeFastTxn,
                                                               wrTexture);
    }
    Range<wr::ImageKey> keys(&aKeys[0], aKeys.Length());
    auto externalImageKey =
        aPipeline->mWrTextureWrapper
            ? aPipeline->mWrTextureWrapper->GetExternalImageKey()
            : wrTexture->GetExternalImageKey();
    wrTexture->PushResourceUpdates(aMaybeFastTxn, op, keys, externalImageKey);
  }

  if (aPipeline->mWrTextureWrapper) {
    // Force frame rendering, since WebRenderTextureHost update its data outside
    // of WebRender.
    aMaybeFastTxn.InvalidateRenderedFrame();
    HoldExternalImage(aPipelineId, aEpoch, aPipeline->mWrTextureWrapper);
  }

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
    AsyncImagePipeline* pipeline = iter.Data();
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
      params, wr::ToRoundedLayoutRect(aPipeline->mScBounds),
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
          builder, wr::ToRoundedLayoutRect(rect), wr::ToRoundedLayoutRect(rect),
          aPipeline->mFilter, range_keys);
      HoldExternalImage(aPipelineId, aEpoch, aPipeline->mCurrentTexture);
    } else {
      MOZ_ASSERT(keys.Length() == 1);
      builder.PushImage(wr::ToRoundedLayoutRect(rect),
                        wr::ToRoundedLayoutRect(rect), true, aPipeline->mFilter,
                        keys[0]);
    }
  }

  spaceAndClipChainHelper.reset();
  builder.PopStackingContext(referenceFrameId.isSome());

  wr::BuiltDisplayList dl;
  wr::LayoutSize builderContentSize;
  builder.Finalize(builderContentSize, dl);
  aSceneBuilderTxn.SetDisplayList(gfx::Color(0.f, 0.f, 0.f, 0.f), aEpoch,
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
  txn.SetDisplayList(gfx::Color(0.f, 0.f, 0.f, 0.f), epoch,
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
  // Hold WebRenderTextureHost until end of its usage on RenderThread
  holder->mTextureHosts.push(ForwardingTextureHost(aEpoch, aTexture));
}

void AsyncImagePipelineManager::HoldExternalImage(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    WebRenderTextureHostWrapper* aWrTextureWrapper) {
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aWrTextureWrapper);

  PipelineTexturesHolder* holder =
      mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  // Hold WebRenderTextureHostWrapper until end of its usage on RenderThread
  holder->mTextureHostWrappers.push(
      ForwardingTextureHostWrapper(aEpoch, aWrTextureWrapper));
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

  auto image = MakeUnique<ForwardingExternalImage>(aEpoch, aImageId);
  holder->mExternalImages.push(std::move(image));
}

void AsyncImagePipelineManager::NotifyPipelinesUpdated(
    RefPtr<wr::WebRenderPipelineInfo> aInfo, bool aRender) {
  // This is called on the render thread, so we just stash the data into
  // UpdatesQueue and process it later on the compositor thread.
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());

  // Increment the count when render happens.
  uint64_t currCount = aRender ? ++mUpdatesCount : mUpdatesCount;
  auto updates = MakeUnique<PipelineUpdates>(aInfo, currCount, aRender);

  {
    // Scope lock to push UpdatesQueue to mUpdatesQueues.
    MutexAutoLock lock(mUpdatesLock);
    mUpdatesQueues.push(std::move(updates));
  }

  if (!aRender) {
    // Do not post ProcessPipelineUpdate when rendering did not happen.
    return;
  }

  // Queue a runnable on the compositor thread to process the queue
  layers::CompositorThreadHolder::Loop()->PostTask(
      NewRunnableMethod("ProcessPipelineUpdates", this,
                        &AsyncImagePipelineManager::ProcessPipelineUpdates));
}

void AsyncImagePipelineManager::ProcessPipelineUpdates() {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (mDestroyed) {
    return;
  }

  UniquePtr<PipelineUpdates> updates;

  while (true) {
    {
      // Scope lock to extract UpdatesQueue from mUpdatesQueues.
      MutexAutoLock lock(mUpdatesLock);
      if (mUpdatesQueues.empty()) {
        // No more PipelineUpdates to process for now.
        break;
      }
      // Check if PipelineUpdates is ready to process.
      uint64_t currCount = mUpdatesCount;
      if (mUpdatesQueues.front()->NeedsToWait(currCount)) {
        // PipelineUpdates is not ready for processing for now.
        break;
      }
      updates = std::move(mUpdatesQueues.front());
      mUpdatesQueues.pop();
    }
    MOZ_ASSERT(updates);

    auto& info = updates->mPipelineInfo->Raw();

    for (uintptr_t i = 0; i < info.epochs.length; i++) {
      ProcessPipelineRendered(info.epochs.data[i].pipeline_id,
                              info.epochs.data[i].epoch,
                              updates->mUpdatesCount);
    }
    for (uintptr_t i = 0; i < info.removed_pipelines.length; i++) {
      ProcessPipelineRemoved(info.removed_pipelines.data[i],
                             updates->mUpdatesCount);
    }
  }
  CheckForTextureHostsNotUsedByGPU();
}

void AsyncImagePipelineManager::ProcessPipelineRendered(
    const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch,
    const uint64_t aUpdatesCount) {
  if (auto entry = mPipelineTexturesHolders.Lookup(wr::AsUint64(aPipelineId))) {
    PipelineTexturesHolder* holder = entry.Data();
    // Release TextureHosts based on Epoch
    while (!holder->mTextureHosts.empty()) {
      if (aEpoch <= holder->mTextureHosts.front().mEpoch) {
        break;
      }
      // Need to extend holding TextureHost if it is direct bounded texture.
      HoldUntilNotUsedByGPU(holder->mTextureHosts.front().mTexture,
                            aUpdatesCount);
      holder->mTextureHosts.pop();
    }
    while (!holder->mTextureHostWrappers.empty()) {
      if (aEpoch <= holder->mTextureHostWrappers.front().mEpoch) {
        break;
      }
      holder->mTextureHostWrappers.pop();
    }
    while (!holder->mExternalImages.empty()) {
      if (aEpoch <= holder->mExternalImages.front()->mEpoch) {
        break;
      }
      holder->mExternalImages.pop();
    }
  }
}

void AsyncImagePipelineManager::ProcessPipelineRemoved(
    const wr::RemovedPipeline& aRemovedPipeline, const uint64_t aUpdatesCount) {
  if (mDestroyed) {
    return;
  }
  if (auto entry = mPipelineTexturesHolders.Lookup(
          wr::AsUint64(aRemovedPipeline.pipeline_id))) {
    PipelineTexturesHolder* holder = entry.Data();
    if (holder->mDestroyedEpoch.isSome()) {
      while (!holder->mTextureHosts.empty()) {
        // Need to extend holding TextureHost if it is direct bounded texture.
        HoldUntilNotUsedByGPU(holder->mTextureHosts.front().mTexture,
                              aUpdatesCount);
        holder->mTextureHosts.pop();
      }
      // Remove Pipeline
      entry.Remove();
    }
    // If mDestroyedEpoch contains nothing it means we reused the same pipeline
    // id (probably because we moved the tab to another window). In this case we
    // need to keep the holder.
  }
}

void AsyncImagePipelineManager::HoldUntilNotUsedByGPU(
    const CompositableTextureHostRef& aTextureHost, uint64_t aUpdatesCount) {
  MOZ_ASSERT(aTextureHost);

  if (aTextureHost->HasIntermediateBuffer()) {
    // If texutre is not direct binding texture, gpu has already finished using
    // it. We could release it now.
    return;
  }

  // When Triple buffer is used, we need wait one more WebRender rendering,
  if (mUseTripleBuffering) {
    ++aUpdatesCount;
  }

  mTexturesInUseByGPU.emplace(std::make_pair(aUpdatesCount, aTextureHost));
}

void AsyncImagePipelineManager::CheckForTextureHostsNotUsedByGPU() {
  uint64_t currCount = mUpdatesCount;

  while (!mTexturesInUseByGPU.empty()) {
    if (currCount <= mTexturesInUseByGPU.front().first) {
      break;
    }
    mTexturesInUseByGPU.pop();
  }
}

wr::Epoch AsyncImagePipelineManager::GetNextImageEpoch() {
  mAsyncImageEpoch.mHandle++;
  return mAsyncImageEpoch;
}

}  // namespace layers
}  // namespace mozilla
