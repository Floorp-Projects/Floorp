
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
  virtual void Init(nsIContent* aContent,
		    nsIFrame* aParent,
		    nsIFrame* aPrevInFlow) MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
    return nsSVGGeometryFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGGeometry));
  }

  // nsSVGGeometryFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor) = 0;
  uint16_t GetClipRule();

protected:
  /**
   * This function returns a set of bit flags indicating which parts of the
   * element (fill, stroke, bounds) should intercept pointer events. It takes
   * into account the type of element and the value of the 'pointer-events'
   * property on the element.
   */
  virtual uint16_t GetHitTestFlags();
};

#endif // __NS_SVGGEOMETRYFRAME_H__
