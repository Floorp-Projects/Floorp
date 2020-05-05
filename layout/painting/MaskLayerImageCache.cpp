/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MaskLayerImageCache.h"

#include "ImageContainer.h"
#include "mozilla/layers/KnowsCompositor.h"

using namespace mozilla::layers;

namespace mozilla {

MaskLayerImageCache::MaskLayerImageCache() {
  MOZ_COUNT_CTOR(MaskLayerImageCache);
}
MaskLayerImageCache::~MaskLayerImageCache() {
  MOZ_COUNT_DTOR(MaskLayerImageCache);
}

void MaskLayerImageCache::Sweep() {
  for (auto iter = mMaskImageContainers.Iter(); !iter.Done(); iter.Next()) {
    const auto& key = iter.Get()->mKey;
    if (key->HasZeroLayerCount()) {
      iter.Remove();
    }
  }
}

ImageContainer* MaskLayerImageCache::FindImageFor(
    const MaskLayerImageKey** aKey) {
  if (MaskLayerImageEntry* entry = mMaskImageContainers.GetEntry(**aKey)) {
    *aKey = entry->mKey.get();
    return entry->mContainer;
  }

  return nullptr;
}

void MaskLayerImageCache::PutImage(const MaskLayerImageKey* aKey,
                                   ImageContainer* aContainer) {
  MaskLayerImageEntry* entry = mMaskImageContainers.PutEntry(*aKey);
  entry->mContainer = aContainer;
}

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey()
    : mRoundedClipRects(), mLayerCount(0) {
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::MaskLayerImageKey(
    const MaskLayerImageKey& aKey)
    : mRoundedClipRects(aKey.mRoundedClipRects.Clone()),
      mLayerCount(aKey.mLayerCount) {
  MOZ_COUNT_CTOR(MaskLayerImageKey);
}

MaskLayerImageCache::MaskLayerImageKey::~MaskLayerImageKey() {
  MOZ_COUNT_DTOR(MaskLayerImageKey);
}

}  // namespace mozilla
