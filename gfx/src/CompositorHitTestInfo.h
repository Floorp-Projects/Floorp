/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITORHITTESTINFO_H_
#define MOZILLA_GFX_COMPOSITORHITTESTINFO_H_

#include "mozilla/EnumSet.h"
#include "mozilla/EnumTypeTraits.h"

namespace mozilla {
namespace gfx {

// This set of flags is used to figure out what information a frame has
// that is relevant to hit-testing in the compositor. The flags are
// intentionally set up so that if all of them are 0 the item is effectively
// invisible to hit-testing, and no information for this frame needs to be
// sent to the compositor.
enum class CompositorHitTestFlags : uint8_t {
  // The frame participates in hit-testing
  eVisibleToHitTest = 0,
  // The frame requires main-thread handling for events
  eDispatchToContent,

  // The touch action flags are set up so that the default of
  // touch-action:auto on an element leaves all the flags as 0.
  eTouchActionPanXDisabled,
  eTouchActionPanYDisabled,
  eTouchActionPinchZoomDisabled,
  eTouchActionDoubleTapZoomDisabled,

  // The frame is a scrollbar or a subframe inside a scrollbar (including
  // scroll thumbs)
  eScrollbar,
  // The frame is a scrollthumb. If this is set then eScrollbar will also be
  // set, unless gecko somehow generates a scroll thumb without a containing
  // scrollbar.
  eScrollbarThumb,
  // If eScrollbar is set, this flag indicates if the scrollbar is a vertical
  // one (if set) or a horizontal one (if not set)
  eScrollbarVertical,

  // Events targeting this frame should only be processed if a target
  // confirmation is received from the main thread. If no such confirmation
  // is received within a timeout period, the event may be dropped.
  // Only meaningful in combination with eDispatchToContent.
  eRequiresTargetConfirmation,
};

using CompositorHitTestInfo = EnumSet<CompositorHitTestFlags, uint32_t>;

// A CompositorHitTestInfo with none of the flags set
constexpr CompositorHitTestInfo CompositorHitTestInvisibleToHit;

// Mask to check for all the touch-action flags at once
constexpr CompositorHitTestInfo CompositorHitTestTouchActionMask(
  CompositorHitTestFlags::eTouchActionPanXDisabled,
  CompositorHitTestFlags::eTouchActionPanYDisabled,
  CompositorHitTestFlags::eTouchActionPinchZoomDisabled,
  CompositorHitTestFlags::eTouchActionDoubleTapZoomDisabled);

} // namespace gfx


// Used for IPDL serialization. The 'value' have to be the biggest enum from CompositorHitTestFlags.
template <>
struct MaxEnumValue<::mozilla::gfx::CompositorHitTestFlags>
{
  static constexpr unsigned int value = static_cast<unsigned int>(gfx::CompositorHitTestFlags::eRequiresTargetConfirmation);
};

namespace gfx {

// Checks if the CompositorHitTestFlags max enum value is less than N.
template <int N>
static constexpr bool DoesCompositorHitTestInfoFitIntoBits()
{
    if (MaxEnumValue<CompositorHitTestInfo::valueType>::value < N)
    {
        return true;
    }

    return false;
}
} // namespace gfx

} // namespace mozilla

#endif /* MOZILLA_GFX_COMPOSITORHITTESTINFO_H_ */
