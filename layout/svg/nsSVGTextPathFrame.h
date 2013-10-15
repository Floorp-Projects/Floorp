/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSVGTEXTPATHFRAME_H
#define NSSVGTEXTPATHFRAME_H

#include "mozilla/Attributes.h"
#include "gfxTypes.h"
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsISVGChildFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"
#include "nsSVGTSpanFrame.h"

class gfxPath;
class nsIAtom;
class nsIContent;
class nsIFrame;
class nsIPresShell;
class nsStyleContext;

namespace mozilla {
class SVGNumberList;
}

typedef nsSVGTSpanFrame nsSVGTextPathFrameBase;

class nsSVGTextPathFrame : public nsSVGTextPathFrameBase
{
  typedef mozilla::SVGNumberList SVGNumberList;

  friend nsIFrame*
  NS_NewSVGTextPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  nsSVGTextPathFrame(nsStyleContext* aContext) : nsSVGTextPathFrameBase(aContext) {}

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
   * @see nsGkAtoms::svgGFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGTextPath"), aResult);
  }
#endif

  // nsSVGTextPathFrame methods:
  already_AddRefed<gfxPath> GetPath();
  nsIFrame *GetPathFrame();

  /**
   * Gets the scale by which offsets along this textPath must be scaled. This
   * scaling is due to the user provided 'pathLength' attribute on the <path>
   * element, which is a user provided estimate of the path length.
   */
  gfxFloat GetOffsetScale();

  /**
   * Gets the offset from the start of the path at which the first character
   * should be positioned. The value returned already takes GetOffsetScale
   * into account.
   */
  gfxFloat GetStartOffset();

protected:

  virtual void GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY) MOZ_OVERRIDE;
  virtual void GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy) MOZ_OVERRIDE;
  virtual const SVGNumberList *GetRotate() MOZ_OVERRIDE;
};

#endif
