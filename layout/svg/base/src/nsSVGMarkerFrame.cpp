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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDocument.h"
#include "nsSVGMarkerFrame.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsSVGMatrix.h"
#include "nsSVGMarkerElement.h"
#include "nsSVGPathGeometryElement.h"

NS_INTERFACE_MAP_BEGIN(nsSVGMarkerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMarkerFrameBase)

nsIFrame*
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMarkerFrame(aContext);
}

nsresult
NS_GetSVGMarkerFrame(nsSVGMarkerFrame **aResult,
                     nsIURI *aURI, nsIContent *aContent)
{
  *aResult = nsnull;

  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return NS_ERROR_FAILURE;
  }
  nsIPresShell *presShell = myDoc->GetShellAt(0);
  if (!presShell) {
    NS_WARNING("no presshell");
    return NS_ERROR_FAILURE;
  }

  // Find the referenced frame
  nsIFrame *marker;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&marker, aURI, aContent, presShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = marker->GetType();
  if (frameType != nsLayoutAtoms::svgMarkerFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGMarkerFrame *)marker;
  return NS_OK;
}

nsSVGMarkerFrame::~nsSVGMarkerFrame()
{
  WillModify();
  // Notify the world that we're dying
  DidModify(mod_die);
}

NS_IMETHODIMP
nsSVGMarkerFrame::InitSVG()
{
  nsCOMPtr<nsIDOMSVGMarkerElement> marker = do_QueryInterface(mContent);
  NS_ASSERTION(marker, "wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedAngle> angle;
    marker->GetOrientAngle(getter_AddRefs(angle));
    angle->GetAnimVal(getter_AddRefs(mOrientAngle));
    NS_ASSERTION(mOrientAngle, "no orientAngle");
    if (!mOrientAngle) return NS_ERROR_FAILURE;
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
    nsCOMPtr<nsIDOMSVGFitToViewBox> box = do_QueryInterface(marker);
    box->GetViewBox(getter_AddRefs(rect));

    if (rect) {
      rect->GetAnimVal(getter_AddRefs(mViewBox));
      NS_ASSERTION(mViewBox, "no viewBox");
      if (!mViewBox) return NS_ERROR_FAILURE;
    }
  }

  marker->GetMarkerUnits(getter_AddRefs(mMarkerUnits));
  marker->GetOrientType(getter_AddRefs(mOrientType));

  mMarkerParent = nsnull;
  mInUse = mInUse2 = PR_FALSE;

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGMarkerFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                   nsIAtom*        aAttribute,
                                   PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::refX ||
       aAttribute == nsGkAtoms::refY ||
       aAttribute == nsGkAtoms::markerWidth ||
       aAttribute == nsGkAtoms::markerHeight ||
       aAttribute == nsGkAtoms::orient ||
       aAttribute == nsGkAtoms::viewBox)) {
    WillModify();
    DidModify();
    return NS_OK;
  }

  return nsSVGMarkerFrameBase::AttributeChanged(aNameSpaceID,
                                                aAttribute, aModType);
}
  

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGMarkerFrame::GetCanvasTM()
{
  if (mInUse2) {
    // really we should return null, but the rest of the SVG code
    // isn't set up for that.  We're going to be bailing drawing the
    // marker anyway, so return an identity.
    nsCOMPtr<nsIDOMSVGMatrix> ident;
    NS_NewSVGMatrix(getter_AddRefs(ident));

    nsIDOMSVGMatrix *retval = ident.get();
    NS_IF_ADDREF(retval);
    return retval;
  }

  mInUse2 = PR_TRUE;

  // get our parent's tm and append local transform

  NS_ASSERTION(mMarkerParent, "null marker parent");
  nsCOMPtr<nsIDOMSVGMatrix> parentTM;
  mMarkerParent->GetCanvasTM(getter_AddRefs(parentTM));
  NS_ASSERTION(parentTM, "null TM");

  // get element
  nsSVGMarkerElement *element = NS_STATIC_CAST(nsSVGMarkerElement*, mContent);

  // scale/move marker
  nsCOMPtr<nsIDOMSVGMatrix> markerTM;
  element->GetMarkerTransform(mStrokeWidth, mX, mY, mAngle, getter_AddRefs(markerTM));

  // viewport marker
  nsCOMPtr<nsIDOMSVGMatrix> viewTM;
  element->GetViewboxToViewportTransform(getter_AddRefs(viewTM));

  nsCOMPtr<nsIDOMSVGMatrix> tmpTM;
  nsCOMPtr<nsIDOMSVGMatrix> resultTM;

  parentTM->Multiply(markerTM, getter_AddRefs(tmpTM));
  tmpTM->Multiply(viewTM, getter_AddRefs(resultTM));

  nsIDOMSVGMatrix *retval = resultTM.get();
  NS_IF_ADDREF(retval);

  mInUse2 = PR_FALSE;

  return retval;
}


nsresult
nsSVGMarkerFrame::PaintMark(nsISVGRendererCanvas *aCanvas,
                            nsSVGPathGeometryFrame *aParent,
                            nsSVGMark *aMark, float aStrokeWidth)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used painting the current mark, and the document
  // has a marker reference loop.
  if (mInUse)
    return NS_OK;

  mInUse = PR_TRUE;
  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAngle = aMark->angle;
  mMarkerParent = aParent;

  nsSVGMarkerElement *marker = NS_STATIC_CAST(nsSVGMarkerElement*, mContent);

  nsRefPtr<nsSVGCoordCtxProvider> ctx;
  ctx =
    nsSVGUtils::GetCoordContextProvider(NS_STATIC_CAST(nsSVGElement*,
                                                       aParent->GetContent()));
  marker->SetParentCoordCtxProvider(ctx);

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    aCanvas->PushClip();

    nsCOMPtr<nsIDOMSVGMatrix> parentTransform, markerTransform, clipTransform;
    nsCOMPtr<nsIDOMSVGMatrix> viewTransform;

    mMarkerParent->GetCanvasTM(getter_AddRefs(parentTransform));

    nsSVGMarkerElement *element = NS_STATIC_CAST(nsSVGMarkerElement*, mContent);
    element->GetMarkerTransform(mStrokeWidth, mX, mY, mAngle,
                                getter_AddRefs(markerTransform));

    element->GetViewboxToViewportTransform(getter_AddRefs(viewTransform));

    if (parentTransform && markerTransform)
      parentTransform->Multiply(markerTransform,
                                getter_AddRefs(clipTransform));

    if (clipTransform && viewTransform) {
      float x, y, width, height;

      viewTransform->GetE(&x);
      viewTransform->GetF(&y);
      width = marker->mLengthAttributes[nsSVGMarkerElement::MARKERWIDTH].GetAnimValue(ctx);
      height = marker->mLengthAttributes[nsSVGMarkerElement::MARKERHEIGHT].GetAnimValue(ctx);
      aCanvas->SetClipRect(clipTransform, x, y, width, height);
    }
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged(PR_TRUE);
      nsSVGUtils::PaintChildWithEffects(aCanvas, nsnull, kid);
    }
  }

  if (GetStyleDisplay()->IsScrollableOverflow())
    aCanvas->PopClip();

  mMarkerParent = nsnull;
  mInUse = PR_FALSE;
  marker->SetParentCoordCtxProvider(nsnull);

  return NS_OK;
}


nsRect
nsSVGMarkerFrame::RegionMark(nsSVGPathGeometryFrame *aParent,
                             nsSVGMark *aMark, float aStrokeWidth)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used in calculating the current mark region, and
  // the document has a marker reference loop.
  if (mInUse)
    return nsRect();

  mInUse = PR_TRUE;

  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAngle = aMark->angle;
  mMarkerParent = aParent;

  nsSVGMarkerElement *marker = NS_STATIC_CAST(nsSVGMarkerElement*, mContent);

  nsRefPtr<nsSVGCoordCtxProvider> ctx;
  ctx =
    nsSVGUtils::GetCoordContextProvider(NS_STATIC_CAST(nsSVGElement*,
                                                       aParent->GetContent()));
  marker->SetParentCoordCtxProvider(ctx);

  // Force children to update their covered region
  for (nsIFrame* kid = mFrames.FirstChild();
       kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* child = nsnull;
    CallQueryInterface(kid, &child);
    if (child)
      child->UpdateCoveredRegion();
  }

  // Now get the combined covered region
  nsRect rect = nsSVGUtils::GetCoveredRegion(mFrames);

  mMarkerParent = nsnull;

  mInUse = PR_FALSE;
  marker->SetParentCoordCtxProvider(nsnull);

  return rect;
}


nsIAtom *
nsSVGMarkerFrame::GetType() const
{
  return nsLayoutAtoms::svgMarkerFrame;
}
