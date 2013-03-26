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
  nsSVGTextFrame(nsStyleContext* aContext)
    : nsSVGTextFrameBase(aContext),
      mPositioningDirty(true) {}

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
                               int32_t         aModType);

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

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  // nsISVGChildFrame interface:
  virtual void NotifySVGChanged(uint32_t aFlags);
  // Override these four to ensure that UpdateGlyphPositioning is called
  // to bring glyph positions up to date
  NS_IMETHOD PaintSVG(nsRenderingContext* aContext,
                      const nsIntRect *aDirtyRect);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint & aPoint);
  virtual void ReflowSVG();
  virtual SVGBBox GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                      uint32_t aFlags);
  
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor);
  
  // nsSVGTextContainerFrame methods:
  virtual uint32_t GetNumberOfChars();
  virtual float GetComputedTextLength();
  virtual float GetSubStringLength(uint32_t charnum, uint32_t nchars);
  virtual int32_t GetCharNumAtPosition(mozilla::nsISVGPoint *point);

  NS_IMETHOD GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval);
  NS_IMETHOD GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval);
  NS_IMETHOD GetExtentOfChar(uint32_t charnum, mozilla::dom::SVGIRect **_retval);
  NS_IMETHOD GetRotationOfChar(uint32_t charnum, float *_retval);

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
