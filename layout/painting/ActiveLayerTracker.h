/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACTIVELAYERTRACKER_H_
#define ACTIVELAYERTRACKER_H_

#include "nsCSSPropertyID.h"

class nsIFrame;
class nsIContent;
class nsDisplayListBuilder;
class nsDOMCSSDeclaration;

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
  static void NotifyRestyle(nsIFrame* aFrame, nsCSSPropertyID aProperty);
  /**
   * Notify aFrame's left/top/right/bottom properties as having (maybe)
   * changed due to a restyle, and therefore possibly wanting an active layer
   * to render that style. Any such marking will time out after a short period.
   */
  static void NotifyOffsetRestyle(nsIFrame* aFrame);
  /**
   * Mark aFrame as being known to have an animation of aProperty.
   * Any such marking will time out after a short period.
   * aNewValue and aDOMCSSDecl are used to determine whether the property's
   * value has changed.
   */
  static void NotifyAnimated(nsIFrame* aFrame, nsCSSPropertyID aProperty,
                             const nsAString& aNewValue,
                             nsDOMCSSDeclaration* aDOMCSSDecl);
  /**
   * Notify aFrame as being known to have an animation of aProperty through an
   * inline style modification during aScrollFrame's scroll event handler.
   */
  static void NotifyAnimatedFromScrollHandler(nsIFrame* aFrame, nsCSSPropertyID aProperty,
                                              nsIFrame* aScrollFrame);
  /**
   * Notify that a property in the inline style rule of aFrame's element
   * has been modified.
   * This notification is incomplete --- not all modifications to inline
   * style will trigger this.
   * aNewValue and aDOMCSSDecl are used to determine whether the property's
   * value has changed.
   */
  static void NotifyInlineStyleRuleModified(nsIFrame* aFrame, nsCSSPropertyID aProperty,
                                            const nsAString& aNewValue,
                                            nsDOMCSSDeclaration* aDOMCSSDecl);
  /**
   * Notify that a frame needs to be repainted. This is important for layering
   * decisions where, say, aFrame's transform is updated from JS, but we need
   * to repaint aFrame anyway, so we get no benefit from giving it its own
   * layer.
   */
  static void NotifyNeedsRepaint(nsIFrame* aFrame);
  /**
   * Return true if aFrame's aProperty style should be considered as being animated
   * for pre-rendering.
   */
  static bool IsStyleMaybeAnimated(nsIFrame* aFrame, nsCSSPropertyID aProperty);
  /**
   * Return true if aFrame's aProperty style should be considered as being animated
   * for constructing active layers.
   */
  static bool IsStyleAnimated(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsCSSPropertyID aProperty);
  /**
   * Return true if any of aFrame's offset property styles should be considered
   * as being animated for constructing active layers.
   */
  static bool IsOffsetStyleAnimated(nsIFrame* aFrame);
  /**
   * Return true if aFrame's background-position-x or background-position-y
   * property is animated.
   */
  static bool IsBackgroundPositionAnimated(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame);
  /**
   * Return true if aFrame either has an animated scale now, or is likely to
   * have one in the future because it has a CSS animation or transition
   * (which may not be playing right now) that affects its scale.
   */
  static bool IsScaleSubjectToAnimation(nsIFrame* aFrame);

  /**
   * Transfer the LayerActivity property to the frame's content node when the
   * frame is about to be destroyed so that layer activity can be tracked
   * throughout reframes of an element. Only call this when aFrame is the
   * primary frame of aContent.
   */
  static void TransferActivityToContent(nsIFrame* aFrame, nsIContent* aContent);
  /**
   * Transfer the LayerActivity property back to the content node's primary
   * frame after the frame has been created.
   */
  static void TransferActivityToFrame(nsIContent* aContent, nsIFrame* aFrame);

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

  /**
   * Called before and after a scroll event handler is executed, with the
   * scrollframe or nullptr, respectively. This acts as a hint to treat
   * inline style changes during the handler differently.
   */
  static void SetCurrentScrollHandlerFrame(nsIFrame* aFrame);
};

} // namespace mozilla

#endif /* ACTIVELAYERTRACKER_H_ */
