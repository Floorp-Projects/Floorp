/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Robert Longson.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsContainerFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGFilters.h"

typedef nsContainerFrame SVGFEContainerFrameBase;

/*
 * This frame is used by filter primitive elements that
 * have special child elements that provide parameters.
 */
class SVGFEContainerFrame : public SVGFEContainerFrameBase
{
  friend nsIFrame*
  NS_NewSVGFEContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  SVGFEContainerFrame(nsStyleContext* aContext) : SVGFEContainerFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return SVGFEContainerFrameBase::IsFrameOfType(
            aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGContainer));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFEContainer"), aResult);
  }
#endif

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);
#endif
  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFEContainerFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32  aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32  aModType);
};

nsIFrame*
NS_NewSVGFEContainerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFEContainerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFEContainerFrame)

/* virtual */ void
SVGFEContainerFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  SVGFEContainerFrameBase::DidSetStyleContext(aOldStyleContext);
  nsSVGEffects::InvalidateRenderingObservers(this);
}

#ifdef DEBUG
NS_IMETHODIMP
SVGFEContainerFrame::Init(nsIContent* aContent,
                          nsIFrame* aParent,
                          nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGFilterPrimitiveStandardAttributes> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem,
               "Trying to construct an SVGFEContainerFrame for a "
               "content element that doesn't support the right interfaces");

  return SVGFEContainerFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
SVGFEContainerFrame::GetType() const
{
  return nsGkAtoms::svgFEContainerFrame;
}

NS_IMETHODIMP
SVGFEContainerFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      PRInt32  aModType)
{
  nsSVGFE *element = static_cast<nsSVGFE*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return SVGFEContainerFrameBase::AttributeChanged(aNameSpaceID,
                                                     aAttribute, aModType);
}
