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
  nsresult rv;
  *aResult = nsnull;

  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);

  // Get ID from spec
  PRInt32 pos = uriSpec.FindChar('#');
  if (pos == -1) {
    NS_ASSERTION(pos != -1, "URI Spec not a reference");
    return NS_ERROR_FAILURE;
  }

  // Strip off hash and get name
  nsCAutoString idC;
  uriSpec.Right(idC, uriSpec.Length() - (pos + 1));

  // Convert to unicode
  nsAutoString id;
  CopyUTF8toUTF16(idC, id);

  // Get document
  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aContent->GetCurrentDoc());
  NS_ASSERTION(doc, "Content doesn't reference a dom Document");
  if (!doc)
    return NS_ERROR_FAILURE;

  // Get element
  nsCOMPtr<nsIDOMElement> element;
  nsCOMPtr<nsIPresShell> ps = do_QueryInterface(aContent->GetCurrentDoc()->GetShellAt(0));
  rv = doc->GetElementById(id, getter_AddRefs(element));
  if (!NS_SUCCEEDED(rv) || element == nsnull)
    return rv;

  nsIFrame *frame;
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  rv = ps->GetPrimaryFrameFor(content, &frame);
  if (!frame)
    return NS_ERROR_FAILURE;

//  see comment preceeding nsSVGMarkerFrame::QueryInterface
//  nsCOMPtr<nsSVGMarkerFrame> marker = do_QueryInterface(frame);
  nsSVGMarkerFrame *marker;
  CallQueryInterface(frame, &marker);
  *aResult = marker;
  return rv;
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
  mInUse = PR_FALSE;

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
  // get our parent's tm and append local transform
  nsCOMPtr<nsIDOMSVGMatrix> parentTM;
  if (mMarkerParent) {
    nsISVGGeometrySource *geometrySource;
    mMarkerParent->QueryInterface(NS_GET_IID(nsISVGGeometrySource),
                                  (void**)&geometrySource);
    if (!geometrySource) {
      NS_ERROR("invalid parent");
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

  nsRect dirtyRectTwips;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged();
      SVGFrame->PaintSVG(aCanvas, dirtyRectTwips);
    }
  }
  mMarkerParent = nsnull;
  mInUse = PR_FALSE;
}


NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
  nsSVGMarkerFrame::RegionMark(nsSVGPathGeometryFrame *aParent,
                               nsSVGMark *aMark, float aStrokeWidth)
{
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
      SVGFrame->NotifyCanvasTMChanged();

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
