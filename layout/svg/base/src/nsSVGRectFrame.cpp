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
 *   William Cook <william.cook@crocodile-clips.com> (original author)
 *   Håkan Waara <hwaara@chello.se>
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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
#include "nsISVGRendererPathBuilder.h"
#include "nsIDOMSVGRectElement.h"
#include "nsSVGElement.h"

class nsSVGRectFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGRectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  nsSVGRectFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}
public:
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgRectFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGRect"), aResult);
  }
#endif

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(nsISVGRendererPathBuilder *pathBuilder);
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGRectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGRectElement> Rect = do_QueryInterface(aContent);
  if (!Rect) {
    NS_ASSERTION(Rect != nsnull, "wrong content element");
    return nsnull;
  }

  return new (aPresShell) nsSVGRectFrame(aContext);
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGRectFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::rx ||
       aAttribute == nsGkAtoms::ry)) {
    UpdateGraphic(nsISVGPathGeometrySource::UPDATEMASK_PATH);
    return NS_OK;
  }

  return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* void constructPath (in nsISVGRendererPathBuilder pathBuilder); */
NS_IMETHODIMP
nsSVGRectFrame::ConstructPath(nsISVGRendererPathBuilder* pathBuilder)
{
  float x, y, width, height, rx, ry;

  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x, &y, &width, &height, &rx, &ry, nsnull);

  /* In a perfect world, this would be handled by the DOM, and 
     return a DOM exception. */
  if (width <= 0 || height <= 0 || ry < 0 || rx < 0)
    return NS_OK;

  /* Clamp rx and ry to half the rect's width and height respectively. */
  float halfWidth  = width/2;
  float halfHeight = height/2;
  if (rx > halfWidth)
    rx = halfWidth;
  if (ry > halfHeight)
    ry = halfHeight;

  /* If either the 'rx' or the 'ry' attribute isn't set in the markup, then we
     have to set it to the value of the other. We do this after clamping rx and
     ry since omitting one of the attributes implicitly means they should both
     be the same. */
  PRBool hasRx = mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::rx);
  PRBool hasRy = mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::ry);
  if (hasRx && !hasRy)
    ry = rx;
  else if (hasRy && !hasRx)
    rx = ry;

  /* However, we may now have made rx > width/2 or else ry > height/2. (If this
     is the case, we know we must be giving rx and ry the same value.) */
  if (rx > halfWidth)
    rx = ry = halfWidth;
  else if (ry > halfHeight)
    rx = ry = halfHeight;

  pathBuilder->Moveto(x+rx, y);
  pathBuilder->Lineto(x+width-rx, y);
  pathBuilder->Arcto(x+width, y+ry , rx, ry, 0.0f, PR_FALSE, PR_TRUE);
  pathBuilder->Lineto(x+width, y+height-ry);
  pathBuilder->Arcto(x+width-rx, y+height , rx, ry, 0.0f, PR_FALSE, PR_TRUE);
  pathBuilder->Lineto(x+rx,y+height);
  pathBuilder->Arcto(x, y+height-ry , rx, ry, 0.0f, PR_FALSE, PR_TRUE);
  pathBuilder->Lineto(x, y+ry);
  pathBuilder->Arcto(x+rx, y, rx, ry, 0.0f, PR_FALSE, PR_TRUE);
  pathBuilder->ClosePath(&x, &y);

  return NS_OK;
}

nsIAtom *
nsSVGRectFrame::GetType() const
{
  return nsLayoutAtoms::svgRectFrame;
}
