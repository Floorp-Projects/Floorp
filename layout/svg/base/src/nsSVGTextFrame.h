/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGTEXTFRAME_H
#define NS_SVGTEXTFRAME_H

#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsSVGTextContainerFrame.h"

class nsRenderingContext;

typedef nsSVGTextContainerFrame nsSVGTextFrameBase;

class nsSVGTextFrame : public nsSVGTextFrameBase
{
  friend nsIFrame*
  NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGTextFrame(nsStyleContext* aContext)
    : nsSVGTextFrameBase(aContext),
      mPositioningDirty(true) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgTextFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGText"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(PRUint32 aFlags);
  // Override these four to ensure that UpdateGlyphPositioning is called
  // to bring glyph positions up to date
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint & aPoint);
  virtual void UpdateBounds();
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      PRUint32 aFlags);
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM();
  
  // nsSVGTextContainerFrame methods:
  virtual PRUint32 GetNumberOfChars();
  virtual float GetComputedTextLength();
  virtual float GetSubStringLength(PRUint32 charnum, PRUint32 nchars);
  virtual PRInt32 GetCharNumAtPosition(nsIDOMSVGPoint *point);

  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);

  // nsSVGTextFrame
  void NotifyGlyphMetricsChange();

private:
  /**
   * @param aForceGlobalTransform passed down to nsSVGGlyphFrames to
   * control whether they should use the global transform even when
   * NS_STATE_NONDISPLAY_CHILD
   */
  void UpdateGlyphPositioning(bool aForceGlobalTransform);

  void SetWhitespaceHandling(nsSVGGlyphFrame *aFrame);

  nsAutoPtr<gfxMatrix> mCanvasTM;

  bool mPositioningDirty;
};

#endif
