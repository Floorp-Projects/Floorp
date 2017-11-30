/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORHITTESTINFO_H_
#define MOZILLA_GFX_COMPOSITORHITTESTINFO_H_

#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace gfx {

// This set of flags is used to figure out what information a frame has
// that is relevant to hit-testing in the compositor. The flags are
// intentionally set up so that if all of them are 0 the item is effectively
// invisible to hit-testing, and no information for this frame needs to be
// sent to the compositor.
enum class CompositorHitTestInfo : uint16_t {
  // Shortcut for checking that none of the flags are set
  eInvisibleToHitTest = 0,

  // The frame participates in hit-testing
  eVisibleToHitTest = 1 << 0,
  // The frame requires main-thread handling for events
  eDispatchToContent = 1 << 1,

  // The touch action flags are set up so that the default of
  // touch-action:auto on an element leaves all the flags as 0.
  eTouchActionPanXDisabled = 1 << 2,
  eTouchActionPanYDisabled = 1 << 3,
  eTouchActionPinchZoomDisabled = 1 << 4,
  eTouchActionDoubleTapZoomDisabled = 1 << 5,
  // Mask to check for all the touch-action flags at once
  eTouchActionMask = (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5),

  // The frame is a scrollbar or a subframe inside a scrollbar (including
  // scroll thumbs)
  eScrollbar = 1 << 6,
  // The frame is a scrollthumb. If this is set then eScrollbar will also be
  // set, unless gecko somehow generates a scroll thumb without a containing
  // scrollbar.
  eScrollbarThumb = 1 << 7,
  // If eScrollbar is set, this flag indicates if the scrollbar is a vertical
  // one (if set) or a horizontal one (if not set)
  eScrollbarVertical = 1 << 8,

  // Used for IPDL serialization. This bitmask should include all the bits
  // that are defined in the enum.
  ALL_BITS = (1 << 9) - 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CompositorHitTestInfo)

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITORHITTESTINFO_H_ */
