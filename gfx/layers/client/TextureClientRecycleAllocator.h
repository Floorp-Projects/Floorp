/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H
#define MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H

#include "mozilla/gfx/Types.h"
#include "mozilla/RefPtr.h"
#include "TextureClient.h"

namespace mozilla {
namespace layers {

class ISurfaceAllocator;
class TextureClientRecycleAllocatorImp;


/**
 * TextureClientRecycleAllocator provides TextureClients allocation and
 * recycling capabilities. It expects allocations of same sizes and
 * attributres. If a recycled TextureClient is different from
 * requested one, the recycled one is dropped and new TextureClient is allocated.
 */
class TextureClientRecycleAllocator
{
  ~TextureClientRecycleAllocator();

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureClientRecycleAllocator)

  explicit TextureClientRecycleAllocator(ISurfaceAllocator* aAllocator);

  void SetMaxPoolSize(uint32_t aMax);

  // Creates and allocates a TextureClient.
  already_AddRefed<TextureClient>
  CreateOrRecycleForDrawing(gfx::SurfaceFormat aFormat,
                            gfx::IntSize aSize,
                            BackendSelector aSelector,
                            TextureFlags aTextureFlags,
                            TextureAllocationFlags flags = ALLOC_DEFAULT);

private:
  RefPtr<TextureClientRecycleAllocatorImp> mAllocator;
};

} // namespace layers
} // namespace mozilla

#endif /* MOZILLA_GFX_TEXTURECLIENT_RECYCLE_ALLOCATOR_H */
