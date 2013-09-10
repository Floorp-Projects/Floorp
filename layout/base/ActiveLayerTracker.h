/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACTIVELAYERTRACKER_H_
#define ACTIVELAYERTRACKER_H_

#include "nsCSSProperty.h"

class nsIFrame;

namespace mozilla {

/**
 * This class receives various notifications about style changes and content
 * changes that affect layerization decisions, and implements the heuristics
 * that drive those decisions. It manages per-frame state to support those
 * heuristics.
 */
class ActiveLayerTracker {
public:
  static void Shutdown();

  /*
   * We track style changes to selected styles:
   *   eCSSProperty_transform
   *   eCSSProperty_opacity
   *   eCSSProperty_left, eCSSProperty_top, eCSSProperty_right, eCSSProperty_bottom
   * and use that information to guess whether style changes are animated.
   */

  /**
   * Notify aFrame's style property as having changed due to a restyle,
   * and therefore possibly wanting an active layer to render that style.
   * Any such marking will time out after a short period.
   * @param aProperty the property that has changed
   */
  static void NotifyRestyle(nsIFrame* aFrame, nsCSSProperty aProperty);
  /**
   * Notify aFrame's left/top/right/bottom properties as having (maybe)
   * changed due to a restyle, and therefore possibly wanting an active layer
   * to render that style. Any such marking will time out after a short period.
   */
  static void NotifyOffsetRestyle(nsIFrame* aFrame);
  /**
   * Mark aFrame as being known to have an animation of aProperty.
   * Any such marking will time out after a short period.
   */
  static void NotifyAnimated(nsIFrame* aFrame, nsCSSProperty aProperty);
  /**
   * Notify that a property in the inline style rule of aFrame's element
   * has been modified.
   * This notification is incomplete --- not all modifications to inline
   * style will trigger this.
   */
  static void NotifyInlineStyleRuleModified(nsIFrame* aFrame, nsCSSProperty aProperty);
  /**
   * Return true if aFrame's aProperty style should be considered as being animated
   * for constructing active layers.
   */
  static bool IsStyleAnimated(nsIFrame* aFrame, nsCSSProperty aProperty);
  /**
   * Return true if any of aFrame's offset property styles should be considered
   * as being animated for constructing active layers.
   */
  static bool IsOffsetOrMarginStyleAnimated(nsIFrame* aFrame);

  /*
   * We track modifications to the content of certain frames (i.e. canvas frames)
   * and use that to make layering decisions.
   */

  /**
   * Mark aFrame's content as being active. This marking will time out after
   * a short period.
   */
  static void NotifyContentChange(nsIFrame* aFrame);
  /**
   * Return true if this frame's content is still marked as active.
   */
  static bool IsContentActive(nsIFrame* aFrame);
};

}

#endif /* ACTIVELAYERTRACKER_H_ */
