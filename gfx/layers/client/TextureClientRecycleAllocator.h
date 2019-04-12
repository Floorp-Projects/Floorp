/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H
#define MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H

#include <map>
#include <stack>
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/RefPtr.h"
#include "TextureClient.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

class TextureClientHolder;
struct PlanarYCbCrData;

class ITextureClientRecycleAllocator {
 protected:
  virtual ~ITextureClientRecycleAllocator() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ITextureClientRecycleAllocator)

 protected:
  friend class TextureClient;
  virtual void RecycleTextureClient(TextureClient* aClient) = 0;
};

class ITextureClientAllocationHelper {
 public:
  ITextureClientAllocationHelper(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                                 BackendSelector aSelector,
                                 TextureFlags aTextureFlags,
                                 TextureAllocationFlags aAllocationFlags)
      : mFormat(aFormat),
        mSize(aSize),
        mSelector(aSelector),
        mTextureFlags(aTextureFlags |
                      TextureFlags::RECYCLE)  // Set recycle flag
        ,
        mAllocationFlags(aAllocationFlags) {}

  virtual already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) = 0;
  virtual bool IsCompatible(TextureClient* aTextureClient) = 0;

  const gfx::SurfaceFormat mFormat;
  const gfx::IntSize mSize;
  const BackendSelector mSelector;
  const TextureFlags mTextureFlags;
  const TextureAllocationFlags mAllocationFlags;
};

class MOZ_RAII YCbCrTextureClientAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  YCbCrTextureClientAllocationHelper(const PlanarYCbCrData& aData,
                                     TextureFlags aTextureFlags);

  bool IsCompatible(TextureClient* aTextureClient) override;

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override;

 protected:
  const PlanarYCbCrData& mData;
};

/**
 * TextureClientRecycleAllocator provides TextureClients allocation and
 * recycling capabilities. It expects allocations of same sizes and
 * attributres. If a recycled TextureClient is different from
 * requested one, the recycled one is dropped and new TextureClient is
 * allocated.
 *
 * By default this uses TextureClient::CreateForDrawing to allocate new texture
 * clients.
 */
class TextureClientRecycleAllocator : public ITextureClientRecycleAllocator {
 protected:
  virtual ~TextureClientRecycleAllocator();

 public:
  explicit TextureClientRecycleAllocator(KnowsCompositor* aAllocator);

  void SetMaxPoolSize(uint32_t aMax);

  // Creates and allocates a TextureClient.
  already_AddRefed<TextureClient> CreateOrRecycle(
      gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
      TextureFlags aTextureFlags, TextureAllocationFlags flags = ALLOC_DEFAULT);

  already_AddRefed<TextureClient> CreateOrRecycle(
      ITextureClientAllocationHelper& aHelper);

  void ShrinkToMinimumSize();

  void Destroy();

 protected:
  virtual already_AddRefed<TextureClient> Allocate(
      gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
      TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags);

  RefPtr<KnowsCompositor> mSurfaceAllocator;

  friend class DefaultTextureClientAllocationHelper;
  void RecycleTextureClient(TextureClient* aClient) override;

  static const uint32_t kMaxPooledSized = 2;
  uint32_t mMaxPooledSize;

  std::map<TextureClient*, RefPtr<TextureClientHolder> > mInUseClients;

  // stack is good from Graphics cache usage point of view.
  std::stack<RefPtr<TextureClientHolder> > mPooledClients;
  Mutex mLock;
  bool mIsDestroyed;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H */
