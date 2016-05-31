/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_layers_TouchActionHelper_h__
#define __mozilla_layers_TouchActionHelper_h__

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZUtils.h"  

namespace mozilla {
namespace widget {

/*
 * Allow different platform widgets to access Content/DOM stuff.
 */
class TouchActionHelper
{
  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;

private:
  static void UpdateAllowedBehavior(uint32_t aTouchActionValue, bool aConsiderPanning, mozilla::layers::TouchBehaviorFlags& aOutBehavior);

public:
  /*
   * Performs hit testing on content, finds frame that corresponds to the aPoint and retrieves
   * touch-action css property value from it according the rules specified in the spec:
   * http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
   */
  static mozilla::layers::TouchBehaviorFlags GetAllowedTouchBehavior(nsIWidget* aWidget, const LayoutDeviceIntPoint& aPoint);
};

} // namespace widget
} // namespace mozilla

#endif /*__mozilla_layers_TouchActionHelper_h__ */
