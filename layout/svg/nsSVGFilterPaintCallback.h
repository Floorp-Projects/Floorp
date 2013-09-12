/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGFILTERPAINTCALLBACK_H__
#define __NS_SVGFILTERPAINTCALLBACK_H__

class nsIFrame;
class nsRenderingContext;

struct nsIntRect;

class nsSVGFilterPaintCallback {
public:
  /**
   * Paint the frame contents.
   * SVG frames will have had matrix propagation set to false already.
   * Non-SVG frames have to do their own thing.
   * The caller will do a Save()/Restore() as necessary so feel free
   * to mess with context state.
   * The context will be configured to use the "user space" coordinate
   * system.
   * @param aDirtyRect the dirty rect *in user space pixels*
   * @param aTransformRoot the outermost frame whose transform should be taken
   *                       into account when painting an SVG glyph
   */
  virtual void Paint(nsRenderingContext *aContext, nsIFrame *aTarget,
                     const nsIntRect *aDirtyRect,
                     nsIFrame* aTransformRoot) = 0;
};

#endif
