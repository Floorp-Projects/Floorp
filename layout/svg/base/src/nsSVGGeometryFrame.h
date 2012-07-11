
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGGEOMETRYFRAME_H__
#define __NS_SVGGEOMETRYFRAME_H__

#include "gfxMatrix.h"
#include "gfxTypes.h"
#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsQueryFrame.h"
#include "nsRect.h"

class gfxContext;
class nsIContent;
class nsStyleContext;
class nsSVGPaintServerFrame;

struct nsStyleSVGPaint;

typedef nsFrame nsSVGGeometryFrameBase;

#define SVG_HIT_TEST_FILL        0x01
#define SVG_HIT_TEST_STROKE      0x02
#define SVG_HIT_TEST_CHECK_MRECT 0x04

/* nsSVGGeometryFrame is a base class for SVG objects that directly
 * have geometry (circle, ellipse, line, polyline, polygon, path, and
 * glyph frames).  It knows how to convert the style information into
 * cairo context information and stores the fill/stroke paint
 * servers. */

class nsSVGGeometryFrame : public nsSVGGeometryFrameBase
{
protected:
  NS_DECL_FRAMEARENA_HELPERS

  nsSVGGeometryFrame(nsStyleContext *aContext)
    : nsSVGGeometryFrameBase(aContext)
  {
    AddStateBits(NS_FRAME_SVG_LAYOUT);
  }

public:
  // nsIFrame interface:
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow);

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsSVGGeometryFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGGeometry));
  }

  // nsSVGGeometryFrame methods:
  virtual gfxMatrix GetCanvasTM(PRUint32 aFor) = 0;
  PRUint16 GetClipRule();

  float GetStrokeWidth();

  /*
   * Set up a cairo context for filling a path
   * @return false to skip rendering
   */
  bool SetupCairoFill(gfxContext *aContext);
  /*
   * @return false if there is no stroke
   */
  bool HasStroke();
  /*
   * Set up a cairo context for measuring a stroked path
   */
  void SetupCairoStrokeGeometry(gfxContext *aContext);
  /*
   * Set up a cairo context for hit testing a stroked path
   */
  void SetupCairoStrokeHitGeometry(gfxContext *aContext);
  /*
   * Set up a cairo context for stroking a path
   * @return false to skip rendering
   */
  bool SetupCairoStroke(gfxContext *aContext);

protected:
  nsSVGPaintServerFrame *GetPaintServer(const nsStyleSVGPaint *aPaint,
                                        const FramePropertyDescriptor *aProperty);

  /**
   * This function returns a set of bit flags indicating which parts of the
   * element (fill, stroke, bounds) should intercept pointer events. It takes
   * into account the type of element and the value of the 'pointer-events'
   * property on the element.
   */
  virtual PRUint16 GetHitTestFlags();

  /**
   * Returns the given 'fill-opacity' or 'stroke-opacity' value multiplied by
   * the value of the 'opacity' property if it's possible to avoid the expense
   * of creating and compositing an offscreen surface for 'opacity' by
   * combining 'opacity' with the 'fill-opacity'/'stroke-opacity'. If not, the
   * given 'fill-opacity'/'stroke-opacity' is returned unmodified.
   */
  float MaybeOptimizeOpacity(float aFillOrStrokeOpacity);

  nsRect mCoveredRegion;

private:
  bool GetStrokeDashData(FallibleTArray<gfxFloat>& dashes, gfxFloat *dashOffset);
};

#endif // __NS_SVGGEOMETRYFRAME_H__
