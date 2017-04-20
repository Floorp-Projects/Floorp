/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

WebRenderLayerScrollData::WebRenderLayerScrollData()
  : mDescendantCount(-1)
{
}

WebRenderLayerScrollData::~WebRenderLayerScrollData()
{
}

void
WebRenderLayerScrollData::Initialize(WebRenderScrollData& aOwner,
                                     Layer* aLayer,
                                     int32_t aDescendantCount)
{
  MOZ_ASSERT(aDescendantCount >= 0); // Ensure value is valid
  MOZ_ASSERT(mDescendantCount == -1); // Don't allow re-setting an already set value
  mDescendantCount = aDescendantCount;

  MOZ_ASSERT(aLayer);
  for (uint32_t i = 0; i < aLayer->GetScrollMetadataCount(); i++) {
    mScrollIds.AppendElement(aOwner.AddMetadata(aLayer->GetScrollMetadata(i)));
  }
}

WebRenderScrollData::WebRenderScrollData()
{
}

WebRenderScrollData::~WebRenderScrollData()
{
}

size_t
WebRenderScrollData::AddMetadata(const ScrollMetadata& aMetadata)
{
  FrameMetrics::ViewID scrollId = aMetadata.GetMetrics().GetScrollId();
  auto insertResult = mScrollIdMap.insert(std::make_pair(scrollId, 0));
  if (insertResult.second) {
    // Insertion took place, therefore it's a scrollId we hadn't seen before
    insertResult.first->second = mScrollMetadatas.Length();
    mScrollMetadatas.AppendElement(aMetadata);
  } // else we didn't insert, because it already existed
  return insertResult.first->second;
}

size_t
WebRenderScrollData::AddNewLayerData()
{
  size_t len = mLayerScrollData.Length();
  Unused << mLayerScrollData.AppendElement();
  return len;
}

WebRenderLayerScrollData*
WebRenderScrollData::GetLayerDataMutable(size_t aIndex)
{
  if (aIndex >= mLayerScrollData.Length()) {
    return nullptr;
  }
  return &(mLayerScrollData.ElementAt(aIndex));
}

} // namespace layers
} // namespace mozilla
