/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_layers_TouchActionHelper_h__
#define __mozilla_layers_TouchActionHelper_h__

#include "mozilla/layers/LayersTypes.h"  // for TouchBehaviorFlags
#include "RelativeTo.h"                  // for RelativeTo

class nsIWidget;
namespace mozilla {

namespace dom {
class Document;
}  // namespace dom

class WidgetTouchEvent;
}  // namespace mozilla

namespace mozilla::layers {

/*
 * Helper class to figure out the allowed touch behavior for frames, as per
 * the touch-action spec.
 */
class TouchActionHelper {
 public:
  /*
   * Performs hit testing on content, finds frame that corresponds to the touch
   * points of aEvent and retrieves touch-action CSS property value from it
   * according the rules specified in the spec:
   * http://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
   */
  static nsTArray<TouchBehaviorFlags> GetAllowedTouchBehavior(
      nsIWidget* aWidget, dom::Document* aDocument,
      const WidgetTouchEvent& aPoint);

  static TouchBehaviorFlags GetAllowedTouchBehaviorForFrame(nsIFrame* aFrame);
};

}  // namespace mozilla::layers

#endif /*__mozilla_layers_TouchActionHelper_h__ */
