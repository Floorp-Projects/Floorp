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
class nsCSSPropertyIDSet;
class nsDOMCSSDeclaration;

namespace mozilla {

class nsDisplayListBuilder;

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
   *   eCSSProperty_transform, eCSSProperty_translate,
   *   eCSSProperty_rotate, eCSSProperty_scale
   *   eCSSProperty_offset_path, eCSSProperty_offset_distance,
   *   eCSSProperty_offset_rotate, eCSSProperty_offset_anchor,
   *   eCSSProperty_offset_position, eCSSProperty_opacity
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
   * Notify that a property in the inline style rule of aFrame's element
   * has been modified.
   */
  static void NotifyInlineStyleRuleModified(nsIFrame* aFrame,
                                            nsCSSPropertyID aProperty);
  /**
   * Notify that a frame needs to be repainted. This is important for layering
   * decisions where, say, aFrame's transform is updated from JS, but we need
   * to repaint aFrame anyway, so we get no benefit from giving it its own
   * layer.
   */
  static void NotifyNeedsRepaint(nsIFrame* aFrame);
  /**
   * Return true if aFrame's property style in |aPropertySet| should be
   * considered as being animated for constructing active layers.
   */
  static bool IsStyleAnimated(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              const nsCSSPropertyIDSet& aPropertySet);
  /**
   * Return true if aFrame's transform-like property,
   * i.e. transform/translate/rotate/scale, is animated.
   */
  static bool IsTransformAnimated(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame);
  /**
   * Return true if aFrame's transform style should be considered as being
   * animated for pre-rendering.
   */
  static bool IsTransformMaybeAnimated(nsIFrame* aFrame);
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
};

}  // namespace mozilla

#endif /* ACTIVELAYERTRACKER_H_ */
