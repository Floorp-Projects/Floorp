/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"
#include "ImageContainer.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/TextureForwarder.h"
#include "TextureClientRecycleAllocator.h"

namespace mozilla {
namespace layers {

// Used to keep TextureClient's reference count stable as not to disrupt
// recycling.
class TextureClientHolder final {
  ~TextureClientHolder() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureClientHolder)

  explicit TextureClientHolder(TextureClient* aClient)
      : mTextureClient(aClient), mWillRecycle(true) {}

  TextureClient* GetTextureClient() { return mTextureClient; }

  bool WillRecycle() { return mWillRecycle; }

  void ClearWillRecycle() { mWillRecycle = false; }

  void ClearTextureClient() { mTextureClient = nullptr; }

 protected:
  RefPtr<TextureClient> mTextureClient;
  bool mWillRecycle;
};

class MOZ_RAII DefaultTextureClientAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  DefaultTextureClientAllocationHelper(
      TextureClientRecycleAllocator* aAllocator, gfx::SurfaceFormat aFormat,
      gfx::IntSize aSize, BackendSelector aSelector, TextureFlags aTextureFlags,
      TextureAllocationFlags aAllocationFlags)
      : ITextureClientAllocationHelper(aFormat, aSize, aSelector, aTextureFlags,
                                       aAllocationFlags),
        mAllocator(aAllocator) {}

  bool IsCompatible(TextureClient* aTextureClient) override {
    if (aTextureClient->GetFormat() != mFormat ||
        aTextureClient->GetSize() != mSize) {
      return false;
    }
    return true;
  }

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aKnowsCompositor) override {
    return mAllocator->Allocate(mFormat, mSize, mSelector, mTextureFlags,
                                mAllocationFlags);
  }

 protected:
  TextureClientRecycleAllocator* mAllocator;
};

YCbCrTextureClientAllocationHelper::YCbCrTextureClientAllocationHelper(
    const PlanarYCbCrData& aData, TextureFlags aTextureFlags)
    : ITextureClientAllocationHelper(gfx::SurfaceFormat::YUV, aData.mYSize,
                                     BackendSelector::Content, aTextureFlags,
                                     ALLOC_DEFAULT),
      mData(aData) {}

bool YCbCrTextureClientAllocationHelper::IsCompatible(
    TextureClient* aTextureClient) {
  MOZ_ASSERT(aTextureClient->GetFormat() == gfx::SurfaceFormat::YUV);

  BufferTextureData* bufferData =
      aTextureClient->GetInternalData()->AsBufferTextureData();
  if (!bufferData || aTextureClient->GetSize() != mData.mYSize ||
      bufferData->GetCbCrSize().isNothing() ||
      bufferData->GetCbCrSize().ref() != mData.mCbCrSize ||
      bufferData->GetYStride().isNothing() ||
      bufferData->GetYStride().ref() != mData.mYStride ||
      bufferData->GetCbCrStride().isNothing() ||
      bufferData->GetCbCrStride().ref() != mData.mCbCrStride ||
      bufferData->GetYUVColorSpace().isNothing() ||
      bufferData->GetYUVColorSpace().ref() != mData.mYUVColorSpace ||
      bufferData->GetColorDepth().isNothing() ||
      bufferData->GetColorDepth().ref() != mData.mColorDepth ||
      bufferData->GetStereoMode().isNothing() ||
      bufferData->GetStereoMode().ref() != mData.mStereoMode) {
    return false;
  }
  return true;
}

already_AddRefed<TextureClient> YCbCrTextureClientAllocationHelper::Allocate(
    KnowsCompositor* aKnowsCompositor) {
  return TextureClient::CreateForYCbCr(
      aKnowsCompositor, mData.GetPictureRect(), mData.mYSize, mData.mYStride,
      mData.mCbCrSize, mData.mCbCrStride, mData.mStereoMode, mData.mColorDepth,
      mData.mYUVColorSpace, mData.mColorRange, mTextureFlags);
}

TextureClientRecycleAllocator::TextureClientRecycleAllocator(
    KnowsCompositor* aKnowsCompositor)
    : mKnowsCompositor(aKnowsCompositor),
      mMaxPooledSize(kMaxPooledSized),
      mLock("TextureClientRecycleAllocatorImp.mLock"),
      mIsDestroyed(false) {}

TextureClientRecycleAllocator::~TextureClientRecycleAllocator() {
  MutexAutoLock lock(mLock);
  while (!mPooledClients.empty()) {
    mPooledClients.pop();
  }
  MOZ_ASSERT(mInUseClients.empty());
}

void TextureClientRecycleAllocator::SetMaxPoolSize(uint32_t aMax) {
  mMaxPooledSize = aMax;
}

already_AddRefed<TextureClient> TextureClientRecycleAllocator::CreateOrRecycle(
    gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
    TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) {
  MOZ_ASSERT(!(aTextureFlags & TextureFlags::RECYCLE));
  DefaultTextureClientAllocationHelper helper(this, aFormat, aSize, aSelector,
                                              aTextureFlags, aAllocFlags);
  return CreateOrRecycle(helper);
}

already_AddRefed<TextureClient> TextureClientRecycleAllocator::CreateOrRecycle(
    ITextureClientAllocationHelper& aHelper) {
  MOZ_ASSERT(aHelper.mTextureFlags & TextureFlags::RECYCLE);

  RefPtr<TextureClientHolder> textureHolder;

  {
    MutexAutoLock lock(mLock);
    if (mIsDestroyed || !mKnowsCompositor->GetTextureForwarder()) {
      return nullptr;
    }
    if (!mPooledClients.empty()) {
      textureHolder = mPooledClients.top();
      mPooledClients.pop();
      // If the texture's allocator is not open or a pooled TextureClient is
      // not compatible, release it.
      if (!textureHolder->GetTextureClient()->GetAllocator()->IPCOpen() ||
          !aHelper.IsCompatible(textureHolder->GetTextureClient())) {
        // Release TextureClient.
        RefPtr<Runnable> task =
            new TextureClientReleaseTask(textureHolder->GetTextureClient());
        textureHolder->ClearTextureClient();
        textureHolder = nullptr;
        mKnowsCompositor->GetTextureForwarder()->GetThread()->Dispatch(
            task.forget());
      } else {
        textureHolder->GetTextureClient()->RecycleTexture(
            aHelper.mTextureFlags);
      }
    }
  }

  if (!textureHolder) {
    // Allocate new TextureClient
    RefPtr<TextureClient> texture = aHelper.Allocate(mKnowsCompositor);
    if (!texture) {
      return nullptr;
    }
    textureHolder = new TextureClientHolder(texture);
  }

  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(mInUseClients.find(textureHolder->GetTextureClient()) ==
               mInUseClients.end());
    // Register TextureClient
    mInUseClients[textureHolder->GetTextureClient()] = textureHolder;
  }
  RefPtr<TextureClient> client(textureHolder->GetTextureClient());

  // Make sure the texture holds a reference to us, and ask it to call
  // RecycleTextureClient when its ref count drops to 1.
  client->SetRecycleAllocator(this);
  return client.forget();
}

already_AddRefed<TextureClient> TextureClientRecycleAllocator::Allocate(
    gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
    TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) {
  return TextureClient::CreateForDrawing(mKnowsCompositor, aFormat, aSize,
                                         aSelector, aTextureFlags, aAllocFlags);
}

void TextureClientRecycleAllocator::ShrinkToMinimumSize() {
  MutexAutoLock lock(mLock);
  while (!mPooledClients.empty()) {
    mPooledClients.pop();
  }
  // We can not clear using TextureClients safely.
  // Just clear WillRecycle here.
  std::map<TextureClient*, RefPtr<TextureClientHolder> >::iterator it;
  for (it = mInUseClients.begin(); it != mInUseClients.end(); it++) {
    RefPtr<TextureClientHolder> holder = it->second;
    holder->ClearWillRecycle();
  }
}

void TextureClientRecycleAllocator::Destroy() {
  MutexAutoLock lock(mLock);
  while (!mPooledClients.empty()) {
    mPooledClients.pop();
  }
  mIsDestroyed = true;
}

void TextureClientRecycleAllocator::RecycleTextureClient(
    TextureClient* aClient) {
  // Clearing the recycle allocator drops a reference, so make sure we stay
  // alive for the duration of this function.
  RefPtr<TextureClientRecycleAllocator> kungFuDeathGrip(this);
  aClient->SetRecycleAllocator(nullptr);

  RefPtr<TextureClientHolder> textureHolder;
  {
    MutexAutoLock lock(mLock);
    if (mInUseClients.find(aClient) != mInUseClients.end()) {
      textureHolder =
          mInUseClients[aClient];  // Keep reference count of
                                   // TextureClientHolder within lock.
      if (textureHolder->WillRecycle() && !mIsDestroyed &&
          mPooledClients.size() < mMaxPooledSize) {
        mPooledClients.push(textureHolder);
      }
      mInUseClients.erase(aClient);
    }
  }
}

}  // namespace layers
}  // namespace mozilla
