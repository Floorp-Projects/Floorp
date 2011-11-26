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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSVGTextPathFrame.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsSVGLength2.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGEffects.h"
#include "nsContentUtils.h"
#include "nsSVGPathElement.h"
#include "nsSVGTextPathElement.h"

using namespace mozilla;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTextPathFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGTextPathFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGTextPathFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsSVGTextPathFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  NS_ASSERTION(aParent, "null parent");

  nsIFrame* ancestorFrame = nsSVGUtils::GetFirstNonAAncestorFrame(aParent);
  NS_ASSERTION(ancestorFrame, "Must have ancestor");

  NS_ASSERTION(ancestorFrame->GetType() == nsGkAtoms::svgTextFrame,
               "trying to construct an SVGTextPathFrame for an invalid "
               "container");
  
  nsCOMPtr<nsIDOMSVGTextPathElement> textPath = do_QueryInterface(aContent);
  NS_ASSERTION(textPath, "Content is not an SVG textPath");


  return nsSVGTextPathFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGTextPathFrame::GetType() const
{
  return nsGkAtoms::svgTextPathFrame;
}

void
nsSVGTextPathFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  // 'x' and 'y' don't apply to 'textPath'
  aX->Clear();
  aY->Clear();
}

void
nsSVGTextPathFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  // 'dx' and 'dy' don't apply to 'textPath'
  aDx->Clear();
  aDy->Clear();
}

const SVGNumberList*
nsSVGTextPathFrame::GetRotate()
{
  return nsnull;
}

//----------------------------------------------------------------------
// nsSVGTextPathFrame methods:

nsIFrame *
nsSVGTextPathFrame::GetPathFrame()
{
  nsSVGTextPathProperty *property = static_cast<nsSVGTextPathProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    nsSVGTextPathElement *tp = static_cast<nsSVGTextPathElement*>(mContent);
    nsAutoString href;
    tp->mStringAttributes[nsSVGTextPathElement::HREF].GetAnimValue(href, tp);
    if (href.IsEmpty()) {
      return nsnull; // no URL
    }

    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetTextPathProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nsnull;
  }

  return property->GetReferencedFrame(nsGkAtoms::svgPathGeometryFrame, nsnull);
}

already_AddRefed<gfxFlattenedPath>
nsSVGTextPathFrame::GetFlattenedPath()
{
  nsIFrame *path = GetPathFrame();

  if (path) {
    nsSVGPathGeometryElement *element =
      static_cast<nsSVGPathGeometryElement*>(path->GetContent());

    return element->GetFlattenedPath(element->PrependLocalTransformTo(gfxMatrix()));
  }
  return nsnull;
}
 
gfxFloat
nsSVGTextPathFrame::GetStartOffset()
{
  nsSVGTextPathElement *tp = static_cast<nsSVGTextPathElement*>(mContent);
  nsSVGLength2 *length = &tp->mLengthAttributes[nsSVGTextPathElement::STARTOFFSET];
  float val = length->GetAnimValInSpecifiedUnits();

  if (val == 0.0f)
    return 0.0;

  if (length->IsPercentage()) {
    nsRefPtr<gfxFlattenedPath> data = GetFlattenedPath();
    return data ? (val * data->GetLength() / 100.0) : 0.0;
  }
  return val * GetOffsetScale();
}

gfxFloat
nsSVGTextPathFrame::GetOffsetScale()
{
  nsIFrame *pathFrame = GetPathFrame();
  if (!pathFrame)
    return 1.0;

  return static_cast<nsSVGPathElement*>(pathFrame->GetContent())->
    GetPathLengthScale(nsSVGPathElement::eForTextPath);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGTextPathFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::startOffset) {
    NotifyGlyphMetricsChange();
  } else if (aNameSpaceID == kNameSpaceID_XLink &&
             aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    NotifyGlyphMetricsChange();
  }

  return NS_OK;
}
