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

  if (key->mLayerCount == 0) {
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

// This case is particularly 'clever' because it uses AddRef/Release to track the use
// not to release the object.
template<>
struct HasDangerousPublicDestructor<MaskLayerImageCache::MaskLayerImageKey>
{
  static const bool value = true;
};

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey()
  : mLayerCount(0)
  , mRoundedClipRects()
{
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey(const MaskLayerImageKey& aKey)
  : mLayerCount(aKey.mLayerCount)
  , mRoundedClipRects(aKey.mRoundedClipRects)
{
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::~MaskLayerImageKey()
{
  MOZ_COUNT_DTOR(MaskLayerImageKey);
}

}
