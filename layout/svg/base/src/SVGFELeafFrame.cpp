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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsFrame.h"
#include "nsSVGFilters.h"
#include "nsSVGEffects.h"

typedef nsFrame SVGFELeafFrameBase;

/*
 * This frame is used by filter primitive elements that don't
 * have special child elements that provide parameters.
 */
class SVGFELeafFrame : public SVGFELeafFrameBase
{
  friend nsIFrame*
  NS_NewSVGFELeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
protected:
  SVGFELeafFrame(nsStyleContext* aContext) : SVGFELeafFrameBase(aContext) {}

public:
  NS_DECL_FRAMEARENA_HELPERS

#ifdef DEBUG
  NS_IMETHOD Init(nsIContent* aContent,
                  nsIFrame*   aParent,
                  nsIFrame*   aPrevInFlow);
#endif

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return SVGFELeafFrameBase::IsFrameOfType(aFlags & ~(nsIFrame::eSVG));
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGFELeaf"), aResult);
  }
#endif

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFELeafFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32  aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32  aModType);
};

nsIFrame*
NS_NewSVGFELeafFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) SVGFELeafFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(SVGFELeafFrame)

/* virtual */ void
SVGFELeafFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  SVGFELeafFrameBase::DidSetStyleContext(aOldStyleContext);
  nsSVGEffects::InvalidateRenderingObservers(this);
}

#ifdef DEBUG
NS_IMETHODIMP
SVGFELeafFrame::Init(nsIContent* aContent,
                     nsIFrame* aParent,
                     nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGFilterPrimitiveStandardAttributes> elem = do_QueryInterface(aContent);
  NS_ASSERTION(elem,
               "Trying to construct an SVGFELeafFrame for a "
               "content element that doesn't support the right interfaces");

  return SVGFELeafFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
SVGFELeafFrame::GetType() const
{
  return nsGkAtoms::svgFELeafFrame;
}

NS_IMETHODIMP
SVGFELeafFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32  aModType)
{
  nsSVGFE *element = static_cast<nsSVGFE*>(mContent);
  if (element->AttributeAffectsRendering(aNameSpaceID, aAttribute)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return SVGFELeafFrameBase::AttributeChanged(aNameSpaceID,
                                                aAttribute, aModType);
}
