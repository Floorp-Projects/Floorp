/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGTEXTFRAME_H
#define NS_SVGTEXTFRAME_H

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsSVGTextContainerFrame.h"

class nsRenderingContext;

typedef nsSVGTextContainerFrame nsSVGTextFrameBase;

namespace mozilla {
namespace dom {
class SVGIRect;
}
}

class nsSVGTextFrame : public nsSVGTextFrameBase
{
  friend nsIFrame*
  NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGTextFrame(nsStyleContext* aContext) : nsSVGTextFrameBase(aContext)
  {
    AddStateBits(NS_STATE_SVG_POSITIONING_DIRTY);
  }

public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
#ifdef DEBUG
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;
#endif

  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType) MOZ_OVERRIDE;

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgTextFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGText"), aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(uint32_t aFlags) MOZ_OVERRIDE;
  // Override these four to ensure that UpdateGlyphPositioning is called
  // to bring glyph positions up to date
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect *aDirtyRect) MOZ_OVERRIDE;
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint & aPoint) MOZ_OVERRIDE;
  virtual void ReflowSVG() MOZ_OVERRIDE;
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      uint32_t aFlags) MOZ_OVERRIDE;
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor) MOZ_OVERRIDE;
  
  // nsSVGTextContainerFrame methods:
  virtual uint32_t GetNumberOfChars() MOZ_OVERRIDE;
  virtual float GetComputedTextLength() MOZ_OVERRIDE;
  virtual float GetSubStringLength(uint32_t charnum, uint32_t nchars) MOZ_OVERRIDE;
  virtual int32_t GetCharNumAtPosition(mozilla::nsISVGPoint *point) MOZ_OVERRIDE;

  NS_IMETHOD GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval) MOZ_OVERRIDE;
  NS_IMETHOD GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval) MOZ_OVERRIDE;
  NS_IMETHOD GetExtentOfChar(uint32_t charnum, mozilla::dom::SVGIRect **_retval) MOZ_OVERRIDE;
  NS_IMETHOD GetRotationOfChar(uint32_t charnum, float *_retval) MOZ_OVERRIDE;

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
};

#endif
