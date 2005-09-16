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

#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedAngle.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAngle.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGRectElement.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsISVGRendererPathBuilder.h"
#include "nsSVGDefsFrame.h"
#include "nsISVGValue.h"
#include "nsIDOMSVGMarkerElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsSVGMarkerFrame.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsSVGUtils.h"
#include "nsSVGMatrix.h"

NS_IMETHODIMP_(nsrefcnt)
  nsSVGMarkerFrame::AddRef()
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
  nsSVGMarkerFrame::Release()
{
  return NS_OK;
}

// Trying to implement a QueryInterfacable class without a nsIFoo class.
// Couldn't find a macro incantation for this situation, so roll by hand.
NS_IMETHODIMP
nsSVGMarkerFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsSVGMarkerFrame::GetCID())) {
    *aInstancePtr = (void*)(nsSVGMarkerFrame*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (nsSVGDefsFrame::QueryInterface(aIID, aInstancePtr));
}

nsresult
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsSVGMarkerFrame* it = new (aPresShell) nsSVGMarkerFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsresult
NS_GetSVGMarkerFrame(nsSVGMarkerFrame **aResult, nsIURI *aURI, nsIContent *aContent)
{
  *aResult = nsnull;

  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return NS_ERROR_FAILURE;
  }
  nsIPresShell *aPresShell = myDoc->GetShellAt(0);

  // Get the URI Spec
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);

  // Find the referenced frame
  nsIFrame *marker;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&marker, 
                                                   uriSpec, aContent, aPresShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = marker->GetType();
  if (frameType != nsLayoutAtoms::svgMarkerFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGMarkerFrame *)marker;
  return NS_OK;
}

nsSVGMarkerFrame::~nsSVGMarkerFrame()
{
  nsCOMPtr<nsISVGValue> value;
  if (mRefX && (value = do_QueryInterface(mRefX)))
    value->RemoveObserver(this);
  if (mRefY && (value = do_QueryInterface(mRefY)))
    value->RemoveObserver(this);
  if (mMarkerWidth && (value = do_QueryInterface(mMarkerWidth)))
    value->RemoveObserver(this);
  if (mMarkerHeight && (value = do_QueryInterface(mMarkerHeight)))
    value->RemoveObserver(this);
  if (mOrientAngle && (value = do_QueryInterface(mOrientAngle)))
    value->RemoveObserver(this);
  if (mViewBox && (value = do_QueryInterface(mViewBox)))
    value->RemoveObserver(this);
}

NS_IMETHODIMP
nsSVGMarkerFrame::InitSVG()
{
  nsresult rv = nsSVGDefsFrame::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMSVGMarkerElement> marker = do_QueryInterface(mContent);
  NS_ASSERTION(marker, "wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    marker->GetRefX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mRefX));
    NS_ASSERTION(mRefX, "no RefX");
    if (!mRefX) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRefX);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    marker->GetRefY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mRefY));
    NS_ASSERTION(mRefY, "no RefY");
    if (!mRefY) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRefY);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    marker->GetMarkerWidth(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mMarkerWidth));
    NS_ASSERTION(mMarkerWidth, "no markerWidth");
    if (!mMarkerWidth) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mMarkerWidth);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    marker->GetMarkerHeight(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mMarkerHeight));
    NS_ASSERTION(mMarkerHeight, "no markerHeight");
    if (!mMarkerHeight) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mMarkerHeight);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedAngle> angle;
    marker->GetOrientAngle(getter_AddRefs(angle));
    angle->GetAnimVal(getter_AddRefs(mOrientAngle));
    NS_ASSERTION(mOrientAngle, "no orientAngle");
    if (!mOrientAngle) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mOrientAngle);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
    nsCOMPtr<nsIDOMSVGFitToViewBox> box = do_QueryInterface(marker);
    box->GetViewBox(getter_AddRefs(rect));

    if (rect) {
      rect->GetAnimVal(getter_AddRefs(mViewBox));
      NS_ASSERTION(mRefY, "no viewBox");
      if (!mRefY) return NS_ERROR_FAILURE;
      nsCOMPtr<nsISVGValue> value = do_QueryInterface(mRefY);
      if (value)
        value->AddObserver(this);
    }
  }

  marker->GetMarkerUnits(getter_AddRefs(mMarkerUnits));
  marker->GetOrientType(getter_AddRefs(mOrientType));

  mMarkerParent = nsnull;
  mInUse = mInUse2 = PR_FALSE;

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGMarkerFrame::DidModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  return nsSVGDefsFrame::DidModifySVGObservable(observable, aModType);
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:
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
  nsCOMPtr<nsIDOMSVGMatrix> parentTM;
  if (mMarkerParent) {
    nsISVGGeometrySource *geometrySource;
    mMarkerParent->QueryInterface(NS_GET_IID(nsISVGGeometrySource),
                                  (void**)&geometrySource);
    if (!geometrySource) {
      NS_ERROR("invalid parent");
      mInUse2 = PR_FALSE;
      return nsnull;
    }
    geometrySource->GetCanvasTM(getter_AddRefs(parentTM));
  } else {
    // <svg:defs> 
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame),
                            (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      mInUse2 = PR_FALSE;
      return nsnull;
    }
    parentTM = containerFrame->GetCanvasTM();
  }
  NS_ASSERTION(parentTM, "null TM");

  // get element
  nsCOMPtr<nsIDOMSVGMarkerElement> element = do_QueryInterface(mContent);

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


void
nsSVGMarkerFrame::PaintMark(nsISVGRendererCanvas *aCanvas,
                            nsSVGPathGeometryFrame *aParent,
                            nsSVGMark *aMark, float aStrokeWidth)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used painting the current mark, and the document
  // has a marker reference loop.
  if (mInUse)
    return;

  mInUse = PR_TRUE;
  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAngle = aMark->angle;
  mMarkerParent = aParent;

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    aCanvas->PushClip();

    nsCOMPtr<nsIDOMSVGMatrix> parentTransform, markerTransform, clipTransform;
    nsCOMPtr<nsIDOMSVGMatrix> viewTransform;

    nsISVGGeometrySource *parent;
    CallQueryInterface(mMarkerParent, &parent);
    if (parent)
      parent->GetCanvasTM(getter_AddRefs(parentTransform));

    nsCOMPtr<nsIDOMSVGMarkerElement> element = do_QueryInterface(mContent);
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
      mMarkerWidth->GetValue(&width);
      mMarkerHeight->GetValue(&height);
      aCanvas->SetClipRect(clipTransform, x, y, width, height);
    }
  }

  nsRect dirtyRectTwips;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged(PR_TRUE);
      SVGFrame->PaintSVG(aCanvas, dirtyRectTwips, PR_FALSE);
    }
  }

  if (GetStyleDisplay()->IsScrollableOverflow())
    aCanvas->PopClip();

  mMarkerParent = nsnull;
  mInUse = PR_FALSE;
}


NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
  nsSVGMarkerFrame::RegionMark(nsSVGPathGeometryFrame *aParent,
                               nsSVGMark *aMark, float aStrokeWidth)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used in calculating the current mark region, and
  // the document has a marker reference loop.
  if (mInUse)
    return nsnull;

  mInUse = PR_TRUE;

  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAngle = aMark->angle;
  mMarkerParent = aParent;

  nsISVGRendererRegion *accu_region=nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged(PR_TRUE);

      nsCOMPtr<nsISVGRendererRegion> dirty_region = SVGFrame->GetCoveredRegion();
      if (dirty_region) {
        if (accu_region) {
          nsCOMPtr<nsISVGRendererRegion> temp = dont_AddRef(accu_region);
          dirty_region->Combine(temp, &accu_region);
        }
        else {
          accu_region = dirty_region;
          NS_IF_ADDREF(accu_region);
        }
      }
    }
    kid = kid->GetNextSibling();
  }
  mMarkerParent = nsnull;

  mInUse = PR_FALSE;

  return accu_region;
}


float
nsSVGMarkerFrame::bisect(float a1, float a2)
{
  if (a2 - a1 < M_PI)
    return (a1+a2)/2;
  else
    return M_PI + (a1+a2)/2;
}

nsIAtom *
nsSVGMarkerFrame::GetType() const
{
  return nsLayoutAtoms::svgMarkerFrame;
}
