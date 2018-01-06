/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLINGLAYERSHELPER_H
#define GFX_SCROLLINGLAYERSHELPER_H

#include <unordered_map>

#include "mozilla/Attributes.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "FrameMetrics.h"

class nsDisplayItem;

namespace mozilla {

struct ActiveScrolledRoot;
struct DisplayItemClipChain;

namespace wr {
class DisplayListBuilder;
}

namespace layers {

struct FrameMetrics;
class StackingContextHelper;
class WebRenderLayerManager;

class ScrollingLayersHelper
{
public:
  ScrollingLayersHelper();

  void BeginBuild(WebRenderLayerManager* aManager,
                  wr::DisplayListBuilder& aBuilder);
  void EndBuild();

  void BeginList(const StackingContextHelper& aStackingContext);
  void EndList(const StackingContextHelper& aStackingContext);

  void BeginItem(nsDisplayItem* aItem,
                 const StackingContextHelper& aStackingContext);
  ~ScrollingLayersHelper();

private:
  typedef std::pair<FrameMetrics::ViewID, Maybe<wr::WrClipId>> ClipAndScroll;

  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  DefineClipChain(nsDisplayItem* aItem,
                  const ActiveScrolledRoot* aAsr,
                  const DisplayItemClipChain* aChain,
                  int32_t aAppUnitsPerDevPixel,
                  const StackingContextHelper& aStackingContext);

  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  RecurseAndDefineClip(nsDisplayItem* aItem,
                       const ActiveScrolledRoot* aAsr,
                       const DisplayItemClipChain* aChain,
                       int32_t aAppUnitsPerDevPixel,
                       const StackingContextHelper& aSc);

  std::pair<Maybe<FrameMetrics::ViewID>, Maybe<wr::WrClipId>>
  RecurseAndDefineAsr(nsDisplayItem* aItem,
                      const ActiveScrolledRoot* aAsr,
                      const DisplayItemClipChain* aChain,
                      int32_t aAppUnitsPerDevPixel,
                      const StackingContextHelper& aSc);

  const DisplayItemClipChain* ExtendChain(const DisplayItemClipChain* aClip);
  Maybe<ClipAndScroll> EnclosingClipAndScroll() const;

  typedef std::unordered_map<const DisplayItemClipChain*, wr::WrClipId> ClipIdMap;

  WebRenderLayerManager* MOZ_NON_OWNING_REF mManager;
  wr::DisplayListBuilder* mBuilder;
  // Stack of clip caches. There is one entry in the stack for each reference
  // frame that is currently pushed in WR (a reference frame is a stacking
  // context with a non-identity transform). Each entry contains a map that
  // maps gecko DisplayItemClipChain objects to webrender WrClipIds, which
  // allows us to avoid redefining identical clips in WR. We need to keep a
  // separate cache per reference frame because the DisplayItemClipChain items
  // themselves get deduplicated without regard to reference frames, but on the
  // WR side we need to create different clips if they are in different
  // reference frames.
  std::vector<ClipIdMap> mCacheStack;

  struct ItemClips {
    ItemClips(const ActiveScrolledRoot* aAsr,
              const DisplayItemClipChain* aChain);

    const ActiveScrolledRoot* mAsr;
    const DisplayItemClipChain* mChain;

    Maybe<FrameMetrics::ViewID> mScrollId;
    Maybe<wr::WrClipId> mClipId;
    Maybe<ClipAndScroll> mClipAndScroll;

    void Apply(wr::DisplayListBuilder* aBuilder);
    void Unapply(wr::DisplayListBuilder* aBuilder);
    bool HasSameInputs(const ItemClips& aOther);
  };

  std::vector<ItemClips> mItemClipStack;
};

} // namespace layers
} // namespace mozilla

#endif
