/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_layers_TouchActionHelper_h__
#define __mozilla_layers_TouchActionHelper_h__

#include "mozilla/layers/LayersTypes.h"  // for TouchBehaviorFlags
#include "nsLayoutUtils.h"               // for RelativeTo

class nsIFrame;
class nsIWidget;

namespace mozilla {
namespace layers {

/*
 * Helper class to figure out the allowed touch behavior for frames, as per
 * the touch-action spec.
 */
class TouchActionHelper {
 public:
  /*
   * Performs hit testing on content, finds frame that corresponds to the aPoint
   * and retrieves touch-action css property value from it according the rules
   * specified in the spec:
   * http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
   */
  static TouchBehaviorFlags GetAllowedTouchBehavior(
      nsIWidget* aWidget, RelativeTo aRootFrame,
      const LayoutDeviceIntPoint& aPoint);
};

}  // namespace layers
}  // namespace mozilla

#endif /*__mozilla_layers_TouchActionHelper_h__ */
