/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGFILTERPAINTCALLBACK_H_
#define LAYOUT_SVG_SVGFILTERPAINTCALLBACK_H_

#include "nsRect.h"

class nsIFrame;
class gfxContext;

namespace mozilla {

class SVGFilterPaintCallback {
 public:
  using imgDrawingParams = image::imgDrawingParams;

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
  virtual void Paint(gfxContext& aContext, nsIFrame* aTarget,
                     const gfxMatrix& aTransform, const nsIntRect* aDirtyRect,
                     imgDrawingParams& aImgParams) = 0;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGFILTERPAINTCALLBACK_H_
