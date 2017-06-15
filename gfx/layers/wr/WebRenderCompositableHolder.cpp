/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositableHolder.h"

#include "CompositableHost.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

WebRenderCompositableHolder::AsyncImagePipelineHolder::AsyncImagePipelineHolder()
 : mInitialised(false)
 , mIsChanged(false)
 , mUseExternalImage(false)
 , mFilter(WrImageRendering::Auto)
 , mMixBlendMode(WrMixBlendMode::Normal)
{}

WebRenderCompositableHolder::WebRenderCompositableHolder(uint32_t aIdNamespace)
 : mIdNamespace(aIdNamespace)
 , mResourceId(0)
 , mAsyncImageEpoch(0)
 , mDestroyed(false)
{
  MOZ_COUNT_CTOR(WebRenderCompositableHolder);
}

WebRenderCompositableHolder::~WebRenderCompositableHolder()
{
  MOZ_COUNT_DTOR(WebRenderCompositableHolder);
}

void
WebRenderCompositableHolder::Destroy(wr::WebRenderAPI* aApi)
{
  DeleteOldAsyncImages(aApi);
  mDestroyed = true;
}

bool
WebRenderCompositableHolder::HasKeysToDelete()
{
  return !mKeysToDelete.IsEmpty();
}

void
WebRenderCompositableHolder::DeleteOldAsyncImages(wr::WebRenderAPI* aApi)
{
  for (wr::ImageKey key : mKeysToDelete) {
    aApi->DeleteImage(key);
  }
  mKeysToDelete.Clear();
}

void
WebRenderCompositableHolder::AddPipeline(const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return;
  }
  uint64_t id = wr::AsUint64(aPipelineId);

  MOZ_ASSERT(!mPipelineTexturesHolders.Get(id));
  PipelineTexturesHolder* holder = new PipelineTexturesHolder();
  mPipelineTexturesHolders.Put(id, holder);
}

void
WebRenderCompositableHolder::RemovePipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  if (mDestroyed) {
    return;
  }

  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  MOZ_ASSERT(holder->mDestroyedEpoch.isNothing());
  holder->mDestroyedEpoch = Some(aEpoch);
}

void
WebRenderCompositableHolder::AddAsyncImagePipeline(const wr::PipelineId& aPipelineId, WebRenderImageHost* aImageHost)
{
  if (mDestroyed) {
    return;
  }
  MOZ_ASSERT(aImageHost);
  uint64_t id = wr::AsUint64(aPipelineId);

  MOZ_ASSERT(!mAsyncImagePipelineHolders.Get(id));
  AsyncImagePipelineHolder* holder = new AsyncImagePipelineHolder();
  holder->mImageHost = aImageHost;
  mAsyncImagePipelineHolders.Put(id, holder);
  AddPipeline(aPipelineId);
}

void
WebRenderCompositableHolder::RemoveAsyncImagePipeline(wr::WebRenderAPI* aApi, const wr::PipelineId& aPipelineId)
{
  if (mDestroyed) {
    return;
  }

  uint64_t id = wr::AsUint64(aPipelineId);
  AsyncImagePipelineHolder* holder = mAsyncImagePipelineHolders.Get(id);
  if (!holder) {
    return;
  }

  ++mAsyncImageEpoch; // Update webrender epoch
  aApi->ClearRootDisplayList(wr::NewEpoch(mAsyncImageEpoch), aPipelineId);
  for (wr::ImageKey key : holder->mKeys) {
    aApi->DeleteImage(key);
  }
  mAsyncImagePipelineHolders.Remove(id);
  RemovePipeline(aPipelineId, wr::NewEpoch(mAsyncImageEpoch));
}

void
WebRenderCompositableHolder::UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                                      const LayerRect& aScBounds,
                                                      const gfx::Matrix4x4& aScTransform,
                                                      const gfx::MaybeIntSize& aScaleToSize,
                                                      const WrImageRendering& aFilter,
                                                      const WrMixBlendMode& aMixBlendMode)
{
  if (mDestroyed) {
    return;
  }
  AsyncImagePipelineHolder* holder = mAsyncImagePipelineHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  holder->mInitialised = true;
  holder->mIsChanged = true;
  holder->mScBounds = aScBounds;
  holder->mScTransform = aScTransform;
  holder->mScaleToSize = aScaleToSize;
  holder->mFilter = aFilter;
  holder->mMixBlendMode = aMixBlendMode;
}

bool
WebRenderCompositableHolder::GetImageKeyForTextureHost(wr::WebRenderAPI* aApi, TextureHost* aTexture, nsTArray<wr::ImageKey>& aKeys)
{
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aTexture);

  WebRenderTextureHost* wrTexture = aTexture->AsWebRenderTextureHost();

  if (wrTexture) {
    wrTexture->GetWRImageKeys(aKeys, std::bind(&WebRenderCompositableHolder::GetImageKey, this));
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

    wr::ImageKey key = GetImageKey();
    aKeys.AppendElement(key);
    aApi->AddImage(key, descriptor, slice);
    dSurf->Unmap();
  }
  return false;
}

bool
WebRenderCompositableHolder::UpdateImageKeys(wr::WebRenderAPI* aApi,
                                             bool& aUseExternalImage,
                                             AsyncImagePipelineHolder* aHolder,
                                             nsTArray<wr::ImageKey>& aKeys,
                                             nsTArray<wr::ImageKey>& aKeysToDelete)
{
  MOZ_ASSERT(aKeys.IsEmpty());
  MOZ_ASSERT(aHolder);
  TextureHost* texture = aHolder->mImageHost->GetAsTextureHostForComposite();

  if (!aHolder->mInitialised) {
    return false;
  }

  // No change
  if (!aHolder->mIsChanged && texture == aHolder->mCurrentTexture) {
    // No need to update DisplayList.
    return false;
  }

  aHolder->mIsChanged = false;

  if (texture == aHolder->mCurrentTexture) {
    // Reuse previous ImageKeys.
    aKeys.AppendElements(aHolder->mKeys);
    aUseExternalImage = aHolder->mUseExternalImage;
    return true;
  }

  // Delete old ImageKeys
  aKeysToDelete.AppendElements(aHolder->mKeys);
  aHolder->mKeys.Clear();
  aHolder->mCurrentTexture = nullptr;

  // No txture to render
  if (!texture) {
    return true;
  }

  aUseExternalImage = aHolder->mUseExternalImage = GetImageKeyForTextureHost(aApi, texture, aKeys);
  MOZ_ASSERT(!aKeys.IsEmpty());
  aHolder->mKeys.AppendElements(aKeys);
  aHolder->mCurrentTexture = texture;
  return true;
}

void
WebRenderCompositableHolder::ApplyAsyncImages(wr::WebRenderAPI* aApi)
{
  if (mDestroyed || mAsyncImagePipelineHolders.Count() == 0) {
    return;
  }

  ++mAsyncImageEpoch; // Update webrender epoch
  wr::Epoch epoch = wr::NewEpoch(mAsyncImageEpoch);
  nsTArray<wr::ImageKey> keysToDelete;

  for (auto iter = mAsyncImagePipelineHolders.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    AsyncImagePipelineHolder* holder = iter.Data();

    nsTArray<wr::ImageKey> keys;
    bool useExternalImage = false;
    bool updateDisplayList = UpdateImageKeys(aApi,
                                             useExternalImage,
                                             holder,
                                             keys,
                                             keysToDelete);
    if (!updateDisplayList) {
      continue;
    }

    WrSize contentSize { holder->mScBounds.width, holder->mScBounds.height };
    wr::DisplayListBuilder builder(pipelineId, contentSize);

    if (!keys.IsEmpty()) {
      MOZ_ASSERT(holder->mCurrentTexture.get());

      float opacity = 1.0f;
      builder.PushStackingContext(wr::ToWrRect(holder->mScBounds),
                                  0,
                                  &opacity,
                                  holder->mScTransform.IsIdentity() ? nullptr : &holder->mScTransform,
                                  holder->mMixBlendMode,
                                  nsTArray<WrFilterOp>());

      LayerRect rect(0, 0, holder->mCurrentTexture->GetSize().width, holder->mCurrentTexture->GetSize().height);
      if (holder->mScaleToSize.isSome()) {
        rect = LayerRect(0, 0, holder->mScaleToSize.value().width, holder->mScaleToSize.value().height);
      }
      WrClipRegionToken clip = builder.PushClipRegion(
        wr::ToWrRect(rect), nullptr);

      if (useExternalImage) {
        MOZ_ASSERT(holder->mCurrentTexture->AsWebRenderTextureHost());
        Range<const wr::ImageKey> range_keys(&keys[0], keys.Length());
        holder->mCurrentTexture->PushExternalImage(builder,
                                                   wr::ToWrRect(rect),
                                                   clip,
                                                   holder->mFilter,
                                                   range_keys);
        HoldExternalImage(pipelineId, epoch, holder->mCurrentTexture->AsWebRenderTextureHost());
      } else {
        MOZ_ASSERT(keys.Length() == 1);
        builder.PushImage(wr::ToWrRect(rect),
                          clip,
                          holder->mFilter,
                          keys[0]);
      }
      builder.PopStackingContext();
    }

    wr::BuiltDisplayList dl;
    WrSize builderContentSize;
    builder.Finalize(builderContentSize, dl);
    aApi->SetRootDisplayList(gfx::Color(0.f, 0.f, 0.f, 0.f), epoch, LayerSize(holder->mScBounds.width, holder->mScBounds.height),
                             pipelineId, builderContentSize,
                             dl.dl_desc, dl.dl.inner.data, dl.dl.inner.length);
  }
  DeleteOldAsyncImages(aApi);
  mKeysToDelete.SwapElements(keysToDelete);
}

void
WebRenderCompositableHolder::HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture)
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
WebRenderCompositableHolder::Update(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  if (mDestroyed) {
    return;
  }
  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  if (!holder) {
    return;
  }

  // Remove Pipeline
  if (holder->mDestroyedEpoch.isSome() && holder->mDestroyedEpoch.ref() <= aEpoch) {

    mPipelineTexturesHolders.Remove(wr::AsUint64(aPipelineId));
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

} // namespace layers
} // namespace mozilla
