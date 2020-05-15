/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GeckoContentControllerTypes_h
#define mozilla_layers_GeckoContentControllerTypes_h

#include "mozilla/DefineEnum.h"

namespace mozilla {
namespace layers {

// clang-format off
MOZ_DEFINE_ENUM_CLASS(GeckoContentController_APZStateChange, (
  /**
   * APZ started modifying the view (including panning, zooming, and fling).
   */
  eTransformBegin,
  /**
   * APZ finished modifying the view.
   */
  eTransformEnd,
  /**
   * APZ started a touch.
   * |aArg| is 1 if touch can be a pan, 0 otherwise.
   */
  eStartTouch,
  /**
   * APZ started a pan.
   */
  eStartPanning,
  /**
   * APZ finished processing a touch.
   * |aArg| is 1 if touch was a click, 0 otherwise.
   */
  eEndTouch
));
// clang-format on

/**
 * Different types of tap-related events that can be sent in
 * the HandleTap function. The names should be relatively self-explanatory.
 * Note that the eLongTapUp will always be preceded by an eLongTap, but not
 * all eLongTap notifications will be followed by an eLongTapUp (for instance,
 * if the user moves their finger after triggering the long-tap but before
 * lifting it).
 * The difference between eDoubleTap and eSecondTap is subtle - the eDoubleTap
 * is for an actual double-tap "gesture" while eSecondTap is for the same user
 * input but where a double-tap gesture is not allowed. This is used to fire
 * a click event with detail=2 to web content (similar to what a mouse double-
 * click would do).
 */
// clang-format off
MOZ_DEFINE_ENUM_CLASS(GeckoContentController_TapType, (
  eSingleTap,
  eDoubleTap,
  eSecondTap,
  eLongTap,
  eLongTapUp
));
// clang-format on

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_GeckoContentControllerTypes_h
