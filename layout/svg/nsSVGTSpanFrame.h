/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGTSPANFRAME_H
#define NSSVGTSPANFRAME_H

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsFrame.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGTextContainerFrame.h"

class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;
class nsSVGGlyphFrame;

namespace mozilla {
class DOMSVGPoint;
}

typedef nsSVGTextContainerFrame nsSVGTSpanFrameBase;

class nsSVGTSpanFrame : public nsSVGTSpanFrameBase,
                        public nsISVGGlyphFragmentNode
{
  friend nsIFrame*
  NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGTSpanFrame(nsStyleContext* aContext) :
    nsSVGTSpanFrameBase(aContext) {}

public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame:
#ifdef DEBUG
  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
#endif

  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgTSpanFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGTSpan"), aResult);
  }
#endif
  // nsSVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM(uint32_t aFor);
  
  // nsISVGGlyphFragmentNode interface:
  virtual uint32_t GetNumberOfChars();
  virtual float GetComputedTextLength() MOZ_OVERRIDE;
  virtual float GetSubStringLength(uint32_t charnum, uint32_t fragmentChars) MOZ_OVERRIDE;
  virtual int32_t GetCharNumAtPosition(mozilla::DOMSVGPoint *point) MOZ_OVERRIDE;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetFirstGlyphFrame() MOZ_OVERRIDE;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetNextGlyphFrame() MOZ_OVERRIDE;
  NS_IMETHOD_(void) SetWhitespaceCompression(bool aCompressWhitespace) MOZ_OVERRIDE;
};

#endif
