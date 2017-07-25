/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncImagePipelineManager.h"

#include "CompositableHost.h"
#include "gfxEnv.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

AsyncImagePipelineManager::AsyncImagePipeline::AsyncImagePipeline()
 : mInitialised(false)
 , mIsChanged(false)
 , mUseExternalImage(false)
 , mFilter(wr::ImageRendering::Auto)
 , mMixBlendMode(wr::MixBlendMode::Normal)
{}

AsyncImagePipelineManager::AsyncImagePipelineManager(uint32_t aIdNamespace)
 : mIdNamespace(aIdNamespace)
 , mResourceId(0)
 , mAsyncImageEpoch(0)
 , mDestroyed(false)
{
  MOZ_COUNT_CTOR(AsyncImagePipelineManager);
}

AsyncImagePipelineManager::~AsyncImagePipelineManager()
{
  MOZ_COUNT_DTOR(AsyncImagePipelineManager);
}

void
AsyncImagePipelineManager::Destroy(wr::WebRenderAPI* aApi)
{
  DeleteOldAsyncImages(aApi);
  mDestroyed = true;
}

bool
AsyncImagePipelineManager::HasKeysToDelete()
{
  return !mKeysToDelete.IsEmpty();
}

void
AsyncImagePipelineManager::DeleteOldAsyncImages(wr::WebRenderAPI* aApi)
{
  for (wr::ImageKey key : mKeysToDelete) {
    aApi->DeleteImage(key);
  }
  mKeysToDelete.Clear();
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
AsyncImagePipelineManager::RemoveAsyncImagePipeline(wr::WebRenderAPI* aApi, const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return;
  }

  uint64_t id = wr::AsUint64(aPipelineId);
  if (auto entry = mAsyncImagePipelines.Lookup(id)) {
    AsyncImagePipeline* holder = entry.Data();
    ++mAsyncImageEpoch; // Update webrender epoch
    aApi->ClearRootDisplayList(wr::NewEpoch(mAsyncImageEpoch), aPipelineId);
    for (wr::ImageKey key : holder->mKeys) {
      aApi->DeleteImage(key);
    }
    entry.Remove();
    RemovePipeline(aPipelineId, wr::NewEpoch(mAsyncImageEpoch));
  }
}

void
AsyncImagePipelineManager::UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                                    const LayerRect& aScBounds,
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
  pipeline->mIsChanged = true;
  pipeline->mScBounds = aScBounds;
  pipeline->mScTransform = aScTransform;
  pipeline->mScaleToSize = aScaleToSize;
  pipeline->mFilter = aFilter;
  pipeline->mMixBlendMode = aMixBlendMode;
}

bool
AsyncImagePipelineManager::GenerateImageKeyForTextureHost(wr::WebRenderAPI* aApi, TextureHost* aTexture, nsTArray<wr::ImageKey>& aKeys)
{
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aTexture);

  WebRenderTextureHost* wrTexture = aTexture->AsWebRenderTextureHost();

  if (!gfxEnv::EnableWebRenderRecording() && wrTexture) {
    wrTexture->GetWRImageKeys(aKeys, std::bind(&AsyncImagePipelineManager::GenerateImageKey, this));
    MOZ_ASSERT(!aKeys.IsEmpty());
    Range<const wr::ImageKey> keys(&aKeys[0], aKeys.Length());
    wrTexture->AddWRImage(aApi, keys, wrTexture->GetExternalImageKey());
    return true;
  } else {
    RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
    if (!dSurf) {
      NS_ERROR("TextureHost does not return DataSourceSurface");
      return false;
    }
    gfx::DataSourceSurface::MappedSurface map;
    if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
      NS_ERROR("DataSourceSurface failed to map");
      return false;
    }
    gfx::IntSize size = dSurf->GetSize();
    wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());
    auto slice = Range<uint8_t>(map.mData, size.height * map.mStride);

    wr::ImageKey key = GenerateImageKey();
    aKeys.AppendElement(key);
    aApi->AddImage(key, descriptor, slice);
    dSurf->Unmap();
  }
  return false;
}

bool
AsyncImagePipelineManager::UpdateImageKeys(wr::WebRenderAPI* aApi,
                                           bool& aUseExternalImage,
                                           AsyncImagePipeline* aImageMgr,
                                           nsTArray<wr::ImageKey>& aKeys,
                                           nsTArray<wr::ImageKey>& aKeysToDelete)
{
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aImageMgr);
  TextureHost* texture = aImageMgr->mImageHost->GetAsTextureHostForComposite();

  if (!aImageMgr->mInitialised) {
    return false;
  }

  // No change
  if (!aImageMgr->mIsChanged && texture == aImageMgr->mCurrentTexture) {
    // No need to update DisplayList.
    return false;
  }

  aImageMgr->mIsChanged = false;

  if (texture == aImageMgr->mCurrentTexture) {
    // Reuse previous ImageKeys.
    aKeys.AppendElements(aImageMgr->mKeys);
    aUseExternalImage = aImageMgr->mUseExternalImage;
    return true;
  }

  // Delete old ImageKeys
  aKeysToDelete.AppendElements(aImageMgr->mKeys);
  aImageMgr->mKeys.Clear();
  aImageMgr->mCurrentTexture = nullptr;

  // No txture to render
  if (!texture) {
    return true;
  }

  aUseExternalImage = aImageMgr->mUseExternalImage = GenerateImageKeyForTextureHost(aApi, texture, aKeys);
  MOZ_ASSERT(!aKeys.IsEmpty());
  aImageMgr->mKeys.AppendElements(aKeys);
  aImageMgr->mCurrentTexture = texture;
  return true;
}

void
AsyncImagePipelineManager::ApplyAsyncImages(wr::WebRenderAPI* aApi)
{
  if (mDestroyed || mAsyncImagePipelines.Count() == 0) {
    return;
  }

  ++mAsyncImageEpoch; // Update webrender epoch
  wr::Epoch epoch = wr::NewEpoch(mAsyncImageEpoch);
  nsTArray<wr::ImageKey> keysToDelete;

  for (auto iter = mAsyncImagePipelines.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    AsyncImagePipeline* pipeline = iter.Data();

    nsTArray<wr::ImageKey> keys;
    bool useExternalImage = false;
    bool updateDisplayList = UpdateImageKeys(aApi,
                                             useExternalImage,
                                             pipeline,
                                             keys,
                                             keysToDelete);
    if (!updateDisplayList) {
      continue;
    }

    wr::LayoutSize contentSize { pipeline->mScBounds.width, pipeline->mScBounds.height };
    wr::DisplayListBuilder builder(pipelineId, contentSize);

    if (!keys.IsEmpty()) {
      MOZ_ASSERT(pipeline->mCurrentTexture.get());

      float opacity = 1.0f;
      builder.PushStackingContext(wr::ToLayoutRect(pipeline->mScBounds),
                                  0,
                                  &opacity,
                                  pipeline->mScTransform.IsIdentity() ? nullptr : &pipeline->mScTransform,
                                  wr::TransformStyle::Flat,
                                  pipeline->mMixBlendMode,
                                  nsTArray<wr::WrFilterOp>());

      LayerRect rect(0, 0, pipeline->mCurrentTexture->GetSize().width, pipeline->mCurrentTexture->GetSize().height);
      if (pipeline->mScaleToSize.isSome()) {
        rect = LayerRect(0, 0, pipeline->mScaleToSize.value().width, pipeline->mScaleToSize.value().height);
      }

      if (useExternalImage) {
        MOZ_ASSERT(pipeline->mCurrentTexture->AsWebRenderTextureHost());
        Range<const wr::ImageKey> range_keys(&keys[0], keys.Length());
        pipeline->mCurrentTexture->PushExternalImage(builder,
                                                     wr::ToLayoutRect(rect),
                                                     wr::ToLayoutRect(rect),
                                                     pipeline->mFilter,
                                                     range_keys);
        HoldExternalImage(pipelineId, epoch, pipeline->mCurrentTexture->AsWebRenderTextureHost());
      } else {
        MOZ_ASSERT(keys.Length() == 1);
        builder.PushImage(wr::ToLayoutRect(rect),
                          wr::ToLayoutRect(rect),
                          pipeline->mFilter,
                          keys[0]);
      }
      builder.PopStackingContext();
    }

    wr::BuiltDisplayList dl;
    wr::LayoutSize builderContentSize;
    builder.Finalize(builderContentSize, dl);
    aApi->SetRootDisplayList(gfx::Color(0.f, 0.f, 0.f, 0.f), epoch, LayerSize(pipeline->mScBounds.width, pipeline->mScBounds.height),
                             pipelineId, builderContentSize,
                             dl.dl_desc, dl.dl.inner.data, dl.dl.inner.length);
  }
  DeleteOldAsyncImages(aApi);
  mKeysToDelete.SwapElements(keysToDelete);
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
AsyncImagePipelineManager::Update(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  if (mDestroyed) {
    return;
  }
  if (auto entry = mPipelineTexturesHolders.Lookup(wr::AsUint64(aPipelineId))) {
    PipelineTexturesHolder* holder = entry.Data();
    // Remove Pipeline
    if (holder->mDestroyedEpoch.isSome() && holder->mDestroyedEpoch.ref() <= aEpoch) {
      entry.Remove();
      return;
    }

    // Release TextureHosts based on Epoch
    while (!holder->mTextureHosts.empty()) {
      if (aEpoch <= holder->mTextureHosts.front().mEpoch) {
        break;
      }
      holder->mTextureHosts.pop();
    }
  }
}

} // namespace layers
} // namespace mozilla
