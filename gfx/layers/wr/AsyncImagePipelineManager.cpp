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

AsyncImagePipelineManager::AsyncImagePipeline::AsyncImagePipeline()
 : mInitialised(false)
 , mSentDL(false)
 , mIsChanged(false)
 , mUseExternalImage(false)
 , mFilter(wr::ImageRendering::Auto)
 , mMixBlendMode(wr::MixBlendMode::Normal)
{}

AsyncImagePipelineManager::AsyncImagePipelineManager(already_AddRefed<wr::WebRenderAPI>&& aApi)
 : mApi(aApi)
 , mIdNamespace(mApi->GetNamespace())
 , mResourceId(0)
 , mAsyncImageEpoch{0}
 , mWillGenerateFrame(false)
 , mDestroyed(false)
 , mUpdatesLock("UpdatesLock")
{
  MOZ_COUNT_CTOR(AsyncImagePipelineManager);
}

AsyncImagePipelineManager::~AsyncImagePipelineManager()
{
  MOZ_COUNT_DTOR(AsyncImagePipelineManager);
}

void
AsyncImagePipelineManager::Destroy()
{
  MOZ_ASSERT(!mDestroyed);
  mApi = nullptr;
  mDestroyed = true;
}

void
AsyncImagePipelineManager::SetWillGenerateFrame()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  mWillGenerateFrame = true;
}

bool
AsyncImagePipelineManager::GetAndResetWillGenerateFrame()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  bool ret = mWillGenerateFrame;
  mWillGenerateFrame = false;
  return ret;
}

void
AsyncImagePipelineManager::AddPipeline(const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return;
  }
  uint64_t id = wr::AsUint64(aPipelineId);

  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  if(holder) {
    // This could happen during tab move between different windows.
    // Previously removed holder could be still alive for waiting destroyed.
    MOZ_ASSERT(holder->mDestroyedEpoch.isSome());
    holder->mDestroyedEpoch = Nothing(); // Revive holder
    return;
  }
  holder = new PipelineTexturesHolder();
  mPipelineTexturesHolders.Put(id, holder);
}

void
AsyncImagePipelineManager::RemovePipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  if (mDestroyed) {
    return;
  }

  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  holder->mDestroyedEpoch = Some(aEpoch);
}

void
AsyncImagePipelineManager::AddAsyncImagePipeline(const wr::PipelineId& aPipelineId, WebRenderImageHost* aImageHost)
{
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aImageHost);
  uint64_t id = wr::AsUint64(aPipelineId);

  MOZ_ASSERT(!mAsyncImagePipelines.Get(id));
  AsyncImagePipeline* holder = new AsyncImagePipeline();
  holder->mImageHost = aImageHost;
  mAsyncImagePipelines.Put(id, holder);
  AddPipeline(aPipelineId);
}

void
AsyncImagePipelineManager::RemoveAsyncImagePipeline(const wr::PipelineId& aPipelineId, wr::TransactionBuilder& aTxn)
{
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

void
AsyncImagePipelineManager::UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                                    const LayoutDeviceRect& aScBounds,
                                                    const gfx::Matrix4x4& aScTransform,
                                                    const gfx::MaybeIntSize& aScaleToSize,
                                                    const wr::ImageRendering& aFilter,
                                                    const wr::MixBlendMode& aMixBlendMode)
{
  if (mDestroyed) {
    return;
  }
  AsyncImagePipeline* pipeline = mAsyncImagePipelines.Get(wr::AsUint64(aPipelineId));
  if (!pipeline) {
    return;
  }
  pipeline->mInitialised = true;
  pipeline->Update(aScBounds,
                   aScTransform,
                   aScaleToSize,
                   aFilter,
                   aMixBlendMode);
}

Maybe<TextureHost::ResourceUpdateOp>
AsyncImagePipelineManager::UpdateImageKeys(wr::TransactionBuilder& aResources,
                                           AsyncImagePipeline* aPipeline,
                                           nsTArray<wr::ImageKey>& aKeys)
{
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aPipeline);

  TextureHost* texture = aPipeline->mImageHost->GetAsTextureHostForComposite();
  TextureHost* previousTexture = aPipeline->mCurrentTexture.get();

  if (texture == previousTexture) {
    // The texture has not changed, just reuse previous ImageKeys.
    aKeys = aPipeline->mKeys;
    return Nothing();
  }

  if (!texture) {
    // We don't have a new texture, there isn't much we can do.
    aKeys = aPipeline->mKeys;
    return Nothing();
  }

  aPipeline->mCurrentTexture = texture;

  WebRenderTextureHost* wrTexture = texture->AsWebRenderTextureHost();

  bool useExternalImage = !gfxEnv::EnableWebRenderRecording() && wrTexture;
  aPipeline->mUseExternalImage = useExternalImage;

  // The non-external image code path falls back to converting the texture into
  // an rgb image.
  auto numKeys = useExternalImage ? texture->NumSubTextures() : 1;

  // If we already had a texture and the format hasn't changed, better to reuse the image keys
  // than create new ones.
  bool canUpdate = !!previousTexture
                   && previousTexture->GetSize() == texture->GetSize()
                   && previousTexture->GetFormat() == texture->GetFormat()
                   && aPipeline->mKeys.Length() == numKeys;

  if (!canUpdate) {
    for (auto key : aPipeline->mKeys) {
      aResources.DeleteImage(key);
    }
    aPipeline->mKeys.Clear();
    for (uint32_t i = 0; i < numKeys; ++i) {
      aPipeline->mKeys.AppendElement(GenerateImageKey());
    }
  }

  aKeys = aPipeline->mKeys;

  auto op = canUpdate ? TextureHost::UPDATE_IMAGE : TextureHost::ADD_IMAGE;

  if (!useExternalImage) {
    return UpdateWithoutExternalImage(aResources, texture, aKeys[0], op);
  }

  Range<wr::ImageKey> keys(&aKeys[0], aKeys.Length());
  wrTexture->PushResourceUpdates(aResources, op, keys, wrTexture->GetExternalImageKey());

  return Some(op);
}

Maybe<TextureHost::ResourceUpdateOp>
AsyncImagePipelineManager::UpdateWithoutExternalImage(wr::TransactionBuilder& aResources,
                                                      TextureHost* aTexture,
                                                      wr::ImageKey aKey,
                                                      TextureHost::ResourceUpdateOp aOp)
{
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
    aResources.UpdateImageBuffer(aKey, descriptor, bytes);
  } else {
    aResources.AddImage(aKey, descriptor, bytes);
  }

  dSurf->Unmap();

  return Some(aOp);
}

void
AsyncImagePipelineManager::ApplyAsyncImages(wr::TransactionBuilder& aTxn)
{
  if (mDestroyed || mAsyncImagePipelines.Count() == 0) {
    return;
  }

  wr::Epoch epoch = GetNextImageEpoch();

  // We use a pipeline with a very small display list for each video element.
  // Update each of them if needed.
  for (auto iter = mAsyncImagePipelines.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    AsyncImagePipeline* pipeline = iter.Data();

    nsTArray<wr::ImageKey> keys;
    auto op = UpdateImageKeys(aTxn, pipeline, keys);

    bool updateDisplayList = pipeline->mInitialised &&
                             (pipeline->mIsChanged || op == Some(TextureHost::ADD_IMAGE)) &&
                             !!pipeline->mCurrentTexture;

    if (!pipeline->mSentDL) {
      // If we haven't sent a display list yet, do it anyway, even if it's just
      // an empty DL with a stacking context and no actual image. Otherwise WR
      // will assert about missing the pipeline
      updateDisplayList = true;
    }

    // We will schedule generating a frame after the scene
    // build is done or resource update is done, so we don't need to do it here.

    if (!updateDisplayList) {
      // We don't need to update the display list, either because we can't or because
      // the previous one is still up to date.
      // We may, however, have updated some resources.
      aTxn.UpdateEpoch(pipelineId, epoch);
      if (pipeline->mCurrentTexture) {
        HoldExternalImage(pipelineId, epoch, pipeline->mCurrentTexture->AsWebRenderTextureHost());
      }
      continue;
    }

    pipeline->mSentDL = true;
    pipeline->mIsChanged = false;

    wr::LayoutSize contentSize { pipeline->mScBounds.Width(), pipeline->mScBounds.Height() };
    wr::DisplayListBuilder builder(pipelineId, contentSize);

    float opacity = 1.0f;
    Maybe<wr::WrClipId> referenceFrameId = builder.PushStackingContext(
        wr::ToLayoutRect(pipeline->mScBounds),
        nullptr,
        nullptr,
        &opacity,
        pipeline->mScTransform.IsIdentity() ? nullptr : &pipeline->mScTransform,
        wr::TransformStyle::Flat,
        nullptr,
        pipeline->mMixBlendMode,
        nsTArray<wr::WrFilterOp>(),
        true,
        // This is fine to do unconditionally because we only push images here.
        wr::GlyphRasterSpace::Screen());

    if (pipeline->mCurrentTexture && !keys.IsEmpty()) {
      LayoutDeviceRect rect(0, 0, pipeline->mCurrentTexture->GetSize().width, pipeline->mCurrentTexture->GetSize().height);
      if (pipeline->mScaleToSize.isSome()) {
        rect = LayoutDeviceRect(0, 0, pipeline->mScaleToSize.value().width, pipeline->mScaleToSize.value().height);
      }

      if (pipeline->mUseExternalImage) {
        MOZ_ASSERT(pipeline->mCurrentTexture->AsWebRenderTextureHost());
        Range<wr::ImageKey> range_keys(&keys[0], keys.Length());
        pipeline->mCurrentTexture->PushDisplayItems(builder,
                                                    wr::ToLayoutRect(rect),
                                                    wr::ToLayoutRect(rect),
                                                    pipeline->mFilter,
                                                    range_keys);
        HoldExternalImage(pipelineId, epoch, pipeline->mCurrentTexture->AsWebRenderTextureHost());
      } else {
        MOZ_ASSERT(keys.Length() == 1);
        builder.PushImage(wr::ToLayoutRect(rect),
                          wr::ToLayoutRect(rect),
                          true,
                          pipeline->mFilter,
                          keys[0]);
      }
    }

    builder.PopStackingContext(referenceFrameId.isSome());

    wr::BuiltDisplayList dl;
    wr::LayoutSize builderContentSize;
    builder.Finalize(builderContentSize, dl);
    aTxn.SetDisplayList(gfx::Color(0.f, 0.f, 0.f, 0.f),
                        epoch,
                        LayerSize(pipeline->mScBounds.Width(), pipeline->mScBounds.Height()),
                        pipelineId, builderContentSize,
                        dl.dl_desc, dl.dl);
  }
}

void
AsyncImagePipelineManager::HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture)
{
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aTexture);

  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  // Hold WebRenderTextureHost until end of its usage on RenderThread
  holder->mTextureHosts.push(ForwardingTextureHost(aEpoch, aTexture));
}

void
AsyncImagePipelineManager::HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, const wr::ExternalImageId& aImageId)
{
  if (mDestroyed) {
    SharedSurfacesParent::Release(aImageId);
    return;
  }

  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    SharedSurfacesParent::Release(aImageId);
    return;
  }

  holder->mExternalImages.push(ForwardingExternalImage(aEpoch, aImageId));
}

void
AsyncImagePipelineManager::NotifyPipelinesUpdated(wr::WrPipelineInfo aInfo)
{
  // This is called on the render thread, so we just stash the data into
  // mUpdatesQueue and process it later on the compositor thread.
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());

  MutexAutoLock lock(mUpdatesLock);
  for (uintptr_t i = 0; i < aInfo.epochs.length; i++) {
    mUpdatesQueue.push(std::make_pair(
        aInfo.epochs.data[i].pipeline_id,
        Some(aInfo.epochs.data[i].epoch)));
  }
  for (uintptr_t i = 0; i < aInfo.removed_pipelines.length; i++) {
    mUpdatesQueue.push(std::make_pair(
        aInfo.removed_pipelines.data[i],
        Nothing()));
  }
  // Queue a runnable on the compositor thread to process the queue
  layers::CompositorThreadHolder::Loop()->PostTask(
      NewRunnableMethod("ProcessPipelineUpdates",
                        this,
                        &AsyncImagePipelineManager::ProcessPipelineUpdates));
}

void
AsyncImagePipelineManager::ProcessPipelineUpdates()
{
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());

  if (mDestroyed) {
    return;
  }

  while (true) {
    wr::PipelineId pipelineId;
    Maybe<wr::Epoch> epoch;

    { // scope lock to extract one item from the queue
      MutexAutoLock lock(mUpdatesLock);
      if (mUpdatesQueue.empty()) {
        break;
      }
      pipelineId = mUpdatesQueue.front().first;
      epoch = mUpdatesQueue.front().second;
      mUpdatesQueue.pop();
    }

    if (epoch.isSome()) {
      ProcessPipelineRendered(pipelineId, *epoch);
    } else {
      ProcessPipelineRemoved(pipelineId);
    }
  }
}

void
AsyncImagePipelineManager::ProcessPipelineRendered(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  if (auto entry = mPipelineTexturesHolders.Lookup(wr::AsUint64(aPipelineId))) {
    PipelineTexturesHolder* holder = entry.Data();
    // Release TextureHosts based on Epoch
    while (!holder->mTextureHosts.empty()) {
      if (aEpoch <= holder->mTextureHosts.front().mEpoch) {
        break;
      }
      holder->mTextureHosts.pop();
    }
    while (!holder->mExternalImages.empty()) {
      if (aEpoch <= holder->mExternalImages.front().mEpoch) {
        break;
      }
      DebugOnly<bool> released =
        SharedSurfacesParent::Release(holder->mExternalImages.front().mImageId);
      MOZ_ASSERT(released);
      holder->mExternalImages.pop();
    }
  }
}

void
AsyncImagePipelineManager::ProcessPipelineRemoved(const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return;
  }
  if (auto entry = mPipelineTexturesHolders.Lookup(wr::AsUint64(aPipelineId))) {
    if (entry.Data()->mDestroyedEpoch.isSome()) {
      // Remove Pipeline
      entry.Remove();
    }
    // If mDestroyedEpoch contains nothing it means we reused the same pipeline id (probably because
    // we moved the tab to another window). In this case we need to keep the holder.
  }
}

wr::Epoch
AsyncImagePipelineManager::GetNextImageEpoch()
{
  mAsyncImageEpoch.mHandle++;
  return mAsyncImageEpoch;
}

} // namespace layers
} // namespace mozilla
