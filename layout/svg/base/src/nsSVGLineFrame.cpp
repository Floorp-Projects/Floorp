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
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
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
#include "nsISVGMarkable.h"
#include "nsIDOMSVGLineElement.h"
#include "nsSVGElement.h"

class nsSVGLineFrame : public nsSVGPathGeometryFrame,
                       public nsISVGMarkable
{
  friend nsIFrame*
  NS_NewSVGLineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  nsSVGLineFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgLineFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGLine"), aResult);
  }
#endif

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(cairo_t *aCtx);

  // nsISVGMarkable interface
  void GetMarkPoints(nsVoidArray *aMarks);

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
};

NS_INTERFACE_MAP_BEGIN(nsSVGLineFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGMarkable)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathGeometryFrame)

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGLineFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGLineElement> line = do_QueryInterface(aContent);
  if (!line) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGLineFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGLineFrame(aContext);
}

//----------------------------------------------------------------------
// nsIFrame methods:

nsIAtom *
nsSVGLineFrame::GetType() const
{
  return nsLayoutAtoms::svgLineFrame;
}

NS_IMETHODIMP
nsSVGLineFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x1 ||
       aAttribute == nsGkAtoms::y1 ||
       aAttribute == nsGkAtoms::x2 ||
       aAttribute == nsGkAtoms::y2)) {
    UpdateGraphic();
    return NS_OK;
  }

  return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

NS_IMETHODIMP nsSVGLineFrame::ConstructPath(cairo_t *aCtx)
{
  float x1, y1, x2, y2;

  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nsnull);

  // move to start coordinates then draw line to end coordinates
  cairo_move_to(aCtx, x1, y1);
  cairo_line_to(aCtx, x2, y2);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGMarkable methods:

void
nsSVGLineFrame::GetMarkPoints(nsVoidArray *aMarks) {
  float x1, y1, x2, y2;

  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x1, &y1, &x2, &y2, nsnull);

  nsSVGMark *m1, *m2;
  m1 = new nsSVGMark();
  m2 = new nsSVGMark();

  m1->x = x1;
  m1->y = y1;
  m2->x = x2;
  m2->y = y2;
  m1->angle = m2->angle = atan2(y2-y1, x2-x1);

  aMarks->AppendElement(m1);
  aMarks->AppendElement(m2);
}
