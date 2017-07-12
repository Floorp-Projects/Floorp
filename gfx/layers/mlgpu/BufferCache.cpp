/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferCache.h"
#include "MLGDevice.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace layers {

BufferCache::BufferCache(MLGDevice* aDevice)
 : mDevice(aDevice)
{
}

BufferCache::~BufferCache()
{
}

RefPtr<MLGBuffer>
BufferCache::GetOrCreateBuffer(size_t aBytes)
{
  return mDevice->CreateBuffer(MLGBufferType::Constant, aBytes, MLGUsage::Dynamic, nullptr);
}

void
BufferCache::EndFrame()
{
}

} // namespace layers
} // namespace mozilla
