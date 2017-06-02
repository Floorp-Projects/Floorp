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

using namespace gfx;

namespace layers {

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
  if (!mAsyncImagePipelineHolders.Get(id)) {
    return;
  }

  ++mAsyncImageEpoch; // Update webrender epoch
  aApi->ClearRootDisplayList(wr::NewEpoch(mAsyncImageEpoch), aPipelineId);
  mAsyncImagePipelineHolders.Remove(id);
  RemovePipeline(aPipelineId, wr::NewEpoch(mAsyncImageEpoch));
}

void
WebRenderCompositableHolder::UpdateAsyncImagePipeline(const wr::PipelineId& aPipelineId,
                                                      const LayerRect& aScBounds,
                                                      const gfx::Matrix4x4& aScTransform,
                                                      const gfx::MaybeIntSize& aScaleToSize,
                                                      const MaybeLayerRect& aClipRect,
                                                      const MaybeImageMask& aMask,
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
  holder->mScBounds = aScBounds;
  holder->mScTransform = aScTransform;
  holder->mScaleToSize = aScaleToSize;
  holder->mClipRect = aClipRect;
  holder->mMask = aMask;
  holder->mFilter = aFilter;
  holder->mMixBlendMode = aMixBlendMode;
}

void
WebRenderCompositableHolder::GetImageKeys(nsTArray<wr::ImageKey>& aKeys, size_t aChannelNumber)
{
  MOZ_ASSERT(aChannelNumber > 0);
  for (size_t i = 0; i < aChannelNumber; ++i) {
    wr::ImageKey key = GetImageKey();
    aKeys.AppendElement(key);
  }
}

void
WebRenderCompositableHolder::GetImageKeysForExternalImage(nsTArray<wr::ImageKey>& aKeys)
{
  MOZ_ASSERT(aKeys.IsEmpty());

  // XXX (Jerry): Remove the hardcode image format setting.
#if defined(XP_WIN)
  // Use libyuv to convert the buffer to rgba format. So, use 1 image key here.
  GetImageKeys(aKeys, 1);
#elif defined(XP_MACOSX)
  if (gfxVars::CanUseHardwareVideoDecoding()) {
    // Use the hardware MacIOSurface with YCbCr interleaved format. It uses 1
    // image key.
    GetImageKeys(aKeys, 1);
  } else {
    // Use libyuv.
    GetImageKeys(aKeys, 1);
  }
#elif defined(MOZ_WIDGET_GTK)
  // Use libyuv.
  GetImageKeys(aKeys, 1);
#elif defined(ANDROID)
  // Use libyuv.
  GetImageKeys(aKeys, 1);
#endif

  MOZ_ASSERT(!aKeys.IsEmpty());
}

bool
WebRenderCompositableHolder::GetImageKeyForTextureHost(wr::WebRenderAPI* aApi, TextureHost* aTexture, nsTArray<wr::ImageKey>& aKeys)
{
  MOZ_ASSERT(aKeys.IsEmpty());

  if (!aTexture) {
    return false;
  }

  WebRenderTextureHost* wrTexture = aTexture->AsWebRenderTextureHost();

  if (wrTexture) {
    GetImageKeysForExternalImage(aKeys);
    MOZ_ASSERT(!aKeys.IsEmpty());
    Range<const wr::ImageKey> keys(&aKeys[0], aKeys.Length());
    wrTexture->AddWRImage(aApi, keys, wrTexture->GetExternalImageKey());
    return true;
  } else {
    RefPtr<DataSourceSurface> dSurf = aTexture->GetAsSurface();
    if (!dSurf) {
      NS_ERROR("TextureHost does not return DataSourceSurface");
      return false;
    }
    DataSourceSurface::MappedSurface map;
    if (!dSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
      NS_ERROR("DataSourceSurface failed to map");
      return false;
    }
    IntSize size = dSurf->GetSize();
    wr::ImageDescriptor descriptor(size, map.mStride, dSurf->GetFormat());
    auto slice = Range<uint8_t>(map.mData, size.height * map.mStride);

    wr::ImageKey key = GetImageKey();
    aKeys.AppendElement(key);
    aApi->AddImage(key, descriptor, slice);
    dSurf->Unmap();
  }
  return false;
}

void
WebRenderCompositableHolder::PushExternalImage(wr::DisplayListBuilder& aBuilder,
                                               const WrRect& aBounds,
                                               const WrClipRegionToken aClip,
                                               wr::ImageRendering aFilter,
                                               nsTArray<wr::ImageKey>& aKeys)
{
  // XXX (Jerry): Remove the hardcode image format setting. The format of
  // textureClient could change from time to time. So, we just set the most
  // usable format here.
#if defined(XP_WIN)
  // Use libyuv to convert the buffer to rgba format.
  MOZ_ASSERT(aKeys.Length() == 1);
  aBuilder.PushImage(aBounds, aClip, aFilter, aKeys[0]);
#elif defined(XP_MACOSX)
  if (gfx::gfxVars::CanUseHardwareVideoDecoding()) {
    // Use the hardware MacIOSurface with YCbCr interleaved format.
    MOZ_ASSERT(aKeys.Length() == 1);
    aBuilder.PushYCbCrInterleavedImage(aBounds, aClip, aKeys[0], WrYuvColorSpace::Rec601, aFilter);
  } else {
    // Use libyuv to convert the buffer to rgba format.
    MOZ_ASSERT(aKeys.Length() == 1);
    aBuilder.PushImage(aBounds, aClip, aFilter, aKeys[0]);
  }
#elif defined(MOZ_WIDGET_GTK)
  // Use libyuv to convert the buffer to rgba format.
  MOZ_ASSERT(aKeys.Length() == 1);
  aBuilder.PushImage(aBounds, aClip, aFilter, aKeys[0]);
#elif defined(ANDROID)
  // Use libyuv to convert the buffer to rgba format.
  MOZ_ASSERT(aKeys.Length() == 1);
  aBuilder.PushImage(aBounds, aClip, aFilter, aKeys[0]);
#endif
}

void
WebRenderCompositableHolder::ApplyAsyncImages(wr::WebRenderAPI* aApi)
{
  if (mDestroyed) {
    return;
  }
  ++mAsyncImageEpoch; // Update webrender epoch
  wr::Epoch epoch = wr::NewEpoch(mAsyncImageEpoch);
  nsTArray<wr::ImageKey> keysToDelete;

  for (auto iter = mAsyncImagePipelineHolders.Iter(); !iter.Done(); iter.Next()) {
    wr::PipelineId pipelineId = wr::AsPipelineId(iter.Key());
    AsyncImagePipelineHolder* holder = iter.Data();

    WrSize contentSize { holder->mScBounds.width, holder->mScBounds.height };
    wr::DisplayListBuilder builder(pipelineId, contentSize);
    float opacity = 1.0f;
    builder.PushStackingContext(wr::ToWrRect(holder->mScBounds),
                                0,
                                &opacity,
                                holder->mScTransform.IsIdentity() ? nullptr : &holder->mScTransform,
                                holder->mMixBlendMode);

    TextureHost* texture = holder->mImageHost->GetAsTextureHostForComposite();
    nsTArray<wr::ImageKey> keys;
    bool useExternalImage = GetImageKeyForTextureHost(aApi, texture, keys);

    if (!keys.IsEmpty()) {
      LayerRect rect(0, 0, texture->GetSize().width, texture->GetSize().height);
      if (holder->mScaleToSize.isSome()) {
        rect = LayerRect(0, 0, holder->mScaleToSize.value().width, holder->mScaleToSize.value().height);
      }
      LayerRect clipRect = holder->mClipRect.valueOr(rect);
      WrClipRegionToken clip = builder.PushClipRegion(
        wr::ToWrRect(clipRect),
        holder->mMask.ptrOr(nullptr));

      if (useExternalImage) {
        MOZ_ASSERT(texture->AsWebRenderTextureHost());
        PushExternalImage(builder,
                          wr::ToWrRect(rect),
                          clip,
                          holder->mFilter,
                          keys);
        HoldExternalImage(pipelineId, epoch, texture->AsWebRenderTextureHost());
      } else {
        MOZ_ASSERT(keys.Length() == 1);
        builder.PushImage(wr::ToWrRect(rect),
                          clip,
                          holder->mFilter,
                          keys[0]);
      }
      // XXX do not delete ImageKey for every rendering.
      keysToDelete.AppendElements(keys);
    }
    builder.PopStackingContext();

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
