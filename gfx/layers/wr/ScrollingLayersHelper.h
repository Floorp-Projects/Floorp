/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLINGLAYERSHELPER_H
#define GFX_SCROLLINGLAYERSHELPER_H

#include "mozilla/Attributes.h"
#include "mozilla/layers/WebRenderLayerManager.h"

namespace mozilla {

struct DisplayItemClipChain;

namespace wr {
class DisplayListBuilder;
}

namespace layers {

struct FrameMetrics;
struct LayerClip;
class StackingContextHelper;
class WebRenderLayer;

class MOZ_RAII ScrollingLayersHelper
{
public:
  ScrollingLayersHelper(WebRenderLayer* aLayer,
                        wr::DisplayListBuilder& aBuilder,
                        wr::IpcResourceUpdateQueue& aResources,
                        const StackingContextHelper& aSc);
  ScrollingLayersHelper(nsDisplayItem* aItem,
                        wr::DisplayListBuilder& aBuilder,
                        const StackingContextHelper& aStackingContext,
                        WebRenderLayerManager::ClipIdMap& aCache,
                        bool aApzEnabled);
  ~ScrollingLayersHelper();

private:
  void DefineAndPushScrollLayers(nsDisplayItem* aItem,
                                 const ActiveScrolledRoot* aAsr,
                                 const DisplayItemClipChain* aChain,
                                 wr::DisplayListBuilder& aBuilder,
                                 int32_t aAppUnitsPerDevPixel,
                                 const StackingContextHelper& aStackingContext,
                                 WebRenderLayerManager::ClipIdMap& aCache);
  void DefineAndPushChain(const DisplayItemClipChain* aChain,
                          wr::DisplayListBuilder& aBuilder,
                          const StackingContextHelper& aStackingContext,
                          int32_t aAppUnitsPerDevPixel,
                          WebRenderLayerManager::ClipIdMap& aCache);
  bool DefineAndPushScrollLayer(const FrameMetrics& aMetrics,
                                const StackingContextHelper& aStackingContext);
  void PushLayerLocalClip(const StackingContextHelper& aStackingContext,
                          wr::IpcResourceUpdateQueue& aResources);
  void PushLayerClip(const LayerClip& aClip,
                     const StackingContextHelper& aSc,
                     wr::IpcResourceUpdateQueue& aResources);

  WebRenderLayer* mLayer;
  wr::DisplayListBuilder* mBuilder;
  bool mPushedLayerLocalClip;
  bool mPushedClipAndScroll;
  std::vector<wr::ScrollOrClipId> mPushedClips;
};

} // namespace layers
} // namespace mozilla

#endif
