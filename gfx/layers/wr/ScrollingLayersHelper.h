/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SCROLLINGLAYERSHELPER_H
#define GFX_SCROLLINGLAYERSHELPER_H

#include <unordered_map>

#include "mozilla/Attributes.h"

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

class ScrollingLayersHelper
{
public:
  ScrollingLayersHelper();

  void BeginBuild(wr::DisplayListBuilder& aBuilder);
  void EndBuild();

  void BeginItem(nsDisplayItem* aItem,
                 const StackingContextHelper& aStackingContext);
  void EndItem(nsDisplayItem* aItem);
  ~ScrollingLayersHelper();

private:
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

  // Note: two DisplayItemClipChain* A and B might actually be "equal" (as per
  // DisplayItemClipChain::Equal(A, B)) even though they are not the same pointer
  // (A != B). In this hopefully-rare case, they will get separate entries
  // in this map when in fact we could collapse them. However, to collapse
  // them involves writing a custom hash function for the pointer type such that
  // A and B hash to the same things whenever DisplayItemClipChain::Equal(A, B)
  // is true, and that will incur a performance penalty for all the hashmap
  // operations, so is probably not worth it. With the current code we might
  // end up creating multiple clips in WR that are effectively identical but
  // have separate clip ids. Hopefully this won't happen very often.
  typedef std::unordered_map<const DisplayItemClipChain*, wr::WrClipId> ClipIdMap;

  wr::DisplayListBuilder* mBuilder;
  ClipIdMap mCache;

  struct ItemClips {
    Maybe<FrameMetrics::ViewID> mScrollId;
    Maybe<wr::WrClipId> mClipId;
    Maybe<std::pair<FrameMetrics::ViewID, Maybe<wr::WrClipId>>> mClipAndScroll;

    void Apply(wr::DisplayListBuilder* aBuilder);
    void Unapply(wr::DisplayListBuilder* aBuilder);
  };

  std::vector<ItemClips> mItemClipStack;
};

} // namespace layers
} // namespace mozilla

#endif
