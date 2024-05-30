/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TouchActionHelper.h"

#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/TouchEvents.h"
#include "nsContainerFrame.h"
#include "nsIFrameInlines.h"
#include "nsLayoutUtils.h"

namespace mozilla::layers {

static void UpdateAllowedBehavior(StyleTouchAction aTouchActionValue,
                                  bool aConsiderPanning,
                                  TouchBehaviorFlags& aOutBehavior) {
  if (aTouchActionValue != StyleTouchAction::AUTO) {
    // Double-tap-zooming need property value AUTO
    aOutBehavior &= ~AllowedTouchBehavior::ANIMATING_ZOOM;
    if (aTouchActionValue != StyleTouchAction::MANIPULATION &&
        !(aTouchActionValue & StyleTouchAction::PINCH_ZOOM)) {
      // Pinch-zooming needs value AUTO or MANIPULATION, or the PINCH_ZOOM bit
      // set
      aOutBehavior &= ~AllowedTouchBehavior::PINCH_ZOOM;
    }
  }

  if (aConsiderPanning) {
    if (aTouchActionValue == StyleTouchAction::NONE) {
      aOutBehavior &= ~AllowedTouchBehavior::VERTICAL_PAN;
      aOutBehavior &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }

    // Values pan-x and pan-y set at the same time to the same element do not
    // affect panning constraints. Therefore we need to check whether pan-x is
    // set without pan-y and the same for pan-y.
    if ((aTouchActionValue & StyleTouchAction::PAN_X) &&
        !(aTouchActionValue & StyleTouchAction::PAN_Y)) {
      aOutBehavior &= ~AllowedTouchBehavior::VERTICAL_PAN;
    } else if ((aTouchActionValue & StyleTouchAction::PAN_Y) &&
               !(aTouchActionValue & StyleTouchAction::PAN_X)) {
      aOutBehavior &= ~AllowedTouchBehavior::HORIZONTAL_PAN;
    }
  }
}

static TouchBehaviorFlags GetAllowedTouchBehaviorForPoint(
    nsIWidget* aWidget, RelativeTo aRootFrame,
    const LayoutDeviceIntPoint& aPoint) {
  nsPoint relativePoint =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aWidget, aPoint, aRootFrame);

  nsIFrame* target = nsLayoutUtils::GetFrameForPoint(aRootFrame, relativePoint);

  return TouchActionHelper::GetAllowedTouchBehaviorForFrame(target);
}

nsTArray<TouchBehaviorFlags> TouchActionHelper::GetAllowedTouchBehavior(
    nsIWidget* aWidget, dom::Document* aDocument,
    const WidgetTouchEvent& aEvent) {
  nsTArray<TouchBehaviorFlags> flags;
  if (!aWidget || !aDocument) {
    return flags;
  }
  if (PresShell* presShell = aDocument->GetPresShell()) {
    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      for (const auto& touch : aEvent.mTouches) {
        flags.AppendElement(GetAllowedTouchBehaviorForPoint(
            aWidget, RelativeTo{rootFrame, ViewportType::Visual},
            touch->mRefPoint));
      }
    }
  }
  return flags;
}

TouchBehaviorFlags TouchActionHelper::GetAllowedTouchBehaviorForFrame(
    nsIFrame* aFrame) {
  TouchBehaviorFlags behavior = AllowedTouchBehavior::VERTICAL_PAN |
                                AllowedTouchBehavior::HORIZONTAL_PAN |
                                AllowedTouchBehavior::PINCH_ZOOM |
                                AllowedTouchBehavior::ANIMATING_ZOOM;

  if (!aFrame) {
    return behavior;
  }

  nsIFrame* nearestScrollContainerFrame =
      nsLayoutUtils::GetNearestScrollContainerFrame(aFrame, 0);

  // We're walking up the DOM tree until we meet the element with touch behavior
  // and accumulating touch-action restrictions of all elements in this chain.
  // The exact quote from the spec, that clarifies more:
  // To determine the effect of a touch, find the nearest ancestor (starting
  // from the element itself) that has a default touch behavior. Then examine
  // the touch-action property of each element between the hit tested element
  // and the element with the default touch behavior (including both the hit
  // tested element and the element with the default touch behavior). If the
  // touch-action property of any of those elements disallows the default touch
  // behavior, do nothing. Otherwise allow the element to start considering the
  // touch for the purposes of executing a default touch behavior.

  // Currently we support only two touch behaviors: panning and zooming.
  // For panning we walk up until we meet the first scrollable element (the
  // element that supports panning) or root element. For zooming we walk up
  // until the root element since Firefox currently supports only zooming of the
  // root frame but not the subframes.

  bool considerPanning = true;

  for (nsIFrame* frame = aFrame; frame && frame->GetContent() && behavior;
       frame = frame->GetInFlowParent()) {
    UpdateAllowedBehavior(frame->UsedTouchAction(), considerPanning, behavior);

    if (frame == nearestScrollContainerFrame) {
      // We met the scrollable element, after it we shouldn't consider
      // touch-action values for the purpose of panning but only for zooming.
      considerPanning = false;
    }
  }

  return behavior;
}

}  // namespace mozilla::layers
