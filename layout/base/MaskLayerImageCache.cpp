/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MaskLayerImageCache.h"
#include "ImageContainer.h"

using namespace mozilla::layers;

namespace mozilla {

MaskLayerImageCache::MaskLayerImageCache()
{
  MOZ_COUNT_CTOR(MaskLayerImageCache);
}
MaskLayerImageCache::~MaskLayerImageCache()
{
  MOZ_COUNT_DTOR(MaskLayerImageCache);
}


/* static */ PLDHashOperator
MaskLayerImageCache::SweepFunc(MaskLayerImageEntry* aEntry,
                               void* aUserArg)
{
  const MaskLayerImageCache::MaskLayerImageKey* key = aEntry->mKey;

  if (key->HasZeroLayerCount()) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

void
MaskLayerImageCache::Sweep() 
{
  mMaskImageContainers.EnumerateEntries(SweepFunc, nullptr);
}

ImageContainer*
MaskLayerImageCache::FindImageFor(const MaskLayerImageKey** aKey)
{
  if (MaskLayerImageEntry* entry = mMaskImageContainers.GetEntry(**aKey)) {
    *aKey = entry->mKey.get();
    return entry->mContainer;
  }

  return nullptr;
}

void
MaskLayerImageCache::PutImage(const MaskLayerImageKey* aKey, ImageContainer* aContainer)
{
  MaskLayerImageEntry* entry = mMaskImageContainers.PutEntry(*aKey);
  entry->mContainer = aContainer;
}

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey()
  : mRoundedClipRects()
  , mLayerCount(0)
{
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey(const MaskLayerImageKey& aKey)
  : mRoundedClipRects(aKey.mRoundedClipRects)
  , mLayerCount(aKey.mLayerCount)
{
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::~MaskLayerImageKey()
{
  MOZ_COUNT_DTOR(MaskLayerImageKey);
}

} // namespace mozilla
