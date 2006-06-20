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
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGPathGeometryFrame.h"
#include "nsIDOMSVGCircleElement.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"

class nsSVGCircleFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGCircleFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  nsSVGCircleFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}

public:
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgCircleFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGCircle"), aResult);
  }
#endif

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsSVGPathGeometryFrame methods:
  virtual void ConstructPath(cairo_t *aCtx);
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGCircleFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGCircleElement> circle = do_QueryInterface(aContent);
  if (!circle) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGCircleFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGCircleFrame(aContext);
}

nsIAtom *
nsSVGCircleFrame::GetType() const
{
  return nsLayoutAtoms::svgCircleFrame;
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGCircleFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::cx ||
       aAttribute == nsGkAtoms::cy ||
       aAttribute == nsGkAtoms::r)) {
    UpdateGraphic();
    return NS_OK;
  }

  return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}


//----------------------------------------------------------------------
// nsSVGPathGeometryFrame methods:

void
nsSVGCircleFrame::ConstructPath(cairo_t *aCtx)
{
  float x, y, r;

  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x, &y, &r, nsnull);

  cairo_arc(aCtx, x, y, r, 0, 2*M_PI);
}
