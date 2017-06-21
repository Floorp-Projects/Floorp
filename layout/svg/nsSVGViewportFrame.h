/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGVIEWPORTFRAME_H__
#define __NS_SVGVIEWPORTFRAME_H__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsSVGContainerFrame.h"
#include "nsISVGSVGFrame.h"

class gfxContext;
/**
 * Superclass for inner SVG frames and symbol frames.
 */
class nsSVGViewportFrame
  : public nsSVGDisplayContainerFrame
  , public nsISVGSVGFrame
{
protected:
  nsSVGViewportFrame(nsStyleContext* aContext, nsIFrame::ClassID aID)
    : nsSVGDisplayContainerFrame(aContext, aID)
  {
  }
public:
  NS_DECL_ABSTRACT_FRAME(nsSVGViewportFrame)

  virtual nsresult  AttributeChanged(int32_t         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     int32_t         aModType) override;

  // nsSVGDisplayableFrame interface:
  virtual void PaintSVG(gfxContext& aContext,
                        const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  SVGBBox GetBBoxContribution(const Matrix &aToBBoxUserspace,
                              uint32_t aFlags) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;

  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;

  virtual bool HasChildrenOnlyTransform(Matrix *aTransform) const override;

  // nsISVGSVGFrame interface:
  virtual void NotifyViewportOrTransformChanged(uint32_t aFlags) override;

protected:

  nsAutoPtr<gfxMatrix> mCanvasTM;
};

#endif // __NS_SVGVIEWPORTFRAME_H__

