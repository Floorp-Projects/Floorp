/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLINGLAYERSHELPER_H
#define GFX_SCROLLINGLAYERSHELPER_H

#include "mozilla/Attributes.h"
#include "mozilla/layers/WebRenderCommandBuilder.h"

namespace mozilla {

struct DisplayItemClipChain;

namespace wr {
class DisplayListBuilder;
}

namespace layers {

struct FrameMetrics;
class StackingContextHelper;

class MOZ_RAII ScrollingLayersHelper
{
public:
  ScrollingLayersHelper(nsDisplayItem* aItem,
                        wr::DisplayListBuilder& aBuilder,
                        const StackingContextHelper& aStackingContext,
                        WebRenderCommandBuilder::ClipIdMap& aCache,
                        bool aApzEnabled);
  ~ScrollingLayersHelper();

private:
  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  DefineClipChain(nsDisplayItem* aItem,
                  const ActiveScrolledRoot* aAsr,
                  const DisplayItemClipChain* aChain,
                  int32_t aAppUnitsPerDevPixel,
                  const StackingContextHelper& aStackingContext,
                  WebRenderCommandBuilder::ClipIdMap& aCache);

  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  RecurseAndDefineClip(nsDisplayItem* aItem,
                       const ActiveScrolledRoot* aAsr,
                       const DisplayItemClipChain* aChain,
                       int32_t aAppUnitsPerDevPixel,
                       const StackingContextHelper& aSc,
                       WebRenderCommandBuilder::ClipIdMap& aCache);

  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  RecurseAndDefineAsr(nsDisplayItem* aItem,
                      const ActiveScrolledRoot* aAsr,
                      const DisplayItemClipChain* aChain,
                      int32_t aAppUnitsPerDevPixel,
                      const StackingContextHelper& aSc,
                      WebRenderCommandBuilder::ClipIdMap& aCache);

  void DefineAndPushScrollLayers(nsDisplayItem* aItem,
                                 const ActiveScrolledRoot* aAsr,
                                 const DisplayItemClipChain* aChain,
                                 wr::DisplayListBuilder& aBuilder,
                                 int32_t aAppUnitsPerDevPixel,
                                 const StackingContextHelper& aStackingContext,
                                 WebRenderCommandBuilder::ClipIdMap& aCache);
  void DefineAndPushChain(const DisplayItemClipChain* aChain,
                          wr::DisplayListBuilder& aBuilder,
                          const StackingContextHelper& aStackingContext,
                          int32_t aAppUnitsPerDevPixel,
                          WebRenderCommandBuilder::ClipIdMap& aCache);
  bool DefineAndPushScrollLayer(const FrameMetrics& aMetrics,
                                const StackingContextHelper& aStackingContext);

  wr::DisplayListBuilder* mBuilder;
  bool mPushedClipAndScroll;
  std::vector<wr::ScrollOrClipId> mPushedClips;
};

} // namespace layers
} // namespace mozilla

#endif
