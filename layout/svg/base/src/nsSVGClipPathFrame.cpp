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

#include "nsIDOMSVGTransformable.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGClipPathElement.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIDOMSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGUtils.h"

NS_IMETHODIMP_(nsrefcnt)
nsSVGClipPathFrame::AddRef()
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsSVGClipPathFrame::Release()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGClipPathFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsSVGClipPathFrame::GetCID())) {
    *aInstancePtr = (void*)(nsSVGClipPathFrame*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return (nsSVGDefsFrame::QueryInterface(aIID, aInstancePtr));
}

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGClipPathFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(aContent);
  if (!transformable) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGClipPathFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGClipPathFrame* it = new (aPresShell) nsSVGClipPathFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsresult
NS_GetSVGClipPathFrame(nsSVGClipPathFrame **aResult, nsIURI *aURI, nsIContent *aContent)
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
  nsIFrame *cpframe;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&cpframe, 
                                                   uriSpec, aContent, aPresShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = cpframe->GetType();
  if (frameType != nsLayoutAtoms::svgClipPathFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGClipPathFrame *)cpframe;
  return NS_OK;
}

nsSVGClipPathFrame::~nsSVGClipPathFrame()
{
}

NS_IMETHODIMP
nsSVGClipPathFrame::InitSVG()
{
  nsresult rv = nsSVGDefsFrame::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  mClipParentMatrix = NULL;

  return NS_OK;
}

NS_IMETHODIMP
nsSVGClipPathFrame::ClipPaint(nsISVGRendererCanvas* canvas,
                              nsISVGChildFrame* aParent,
                              nsCOMPtr<nsIDOMSVGMatrix> aMatrix)
{
  nsRect dirty;
  nsresult rv;

  mClipParent = aParent,
  mClipParentMatrix = aMatrix;

  NotifyCanvasTMChanged(PR_TRUE);

  rv = canvas->SetRenderMode(nsISVGRendererCanvas::SVG_RENDER_MODE_CLIP);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->PaintSVG(canvas, dirty, PR_TRUE);
    }
  }

  canvas->SetRenderMode(nsISVGRendererCanvas::SVG_RENDER_MODE_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGClipPathFrame::ClipHitTest(nsISVGChildFrame* aParent,
                                nsCOMPtr<nsIDOMSVGMatrix> aMatrix,
                                float aX, float aY, PRBool *aHit)
{
  nsRect dirty;
  mClipParent = aParent,
  mClipParentMatrix = aMatrix;
  *aHit = PR_FALSE;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsIFrame *temp = nsnull;
      nsresult rv = SVGFrame->GetFrameForPointSVG(aX, aY, &temp);
      if (NS_SUCCEEDED(rv) && temp) {
        *aHit = PR_TRUE;
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

nsIAtom *
nsSVGClipPathFrame::GetType() const
{
  return nsLayoutAtoms::svgClipPathFrame;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGClipPathFrame::GetCanvasTM()
{
  // startup cycle
  if (!mClipParentMatrix) {
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    mClipParentMatrix = containerFrame->GetCanvasTM();
  }

  nsCOMPtr<nsIDOMSVGMatrix> localTM;
  {
    nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
    NS_ASSERTION(transformable, "wrong content element");
    nsCOMPtr<nsIDOMSVGAnimatedTransformList> atl;
    transformable->GetTransform(getter_AddRefs(atl));
    NS_ASSERTION(atl, "null animated transform list");
    nsCOMPtr<nsIDOMSVGTransformList> transforms;
    atl->GetAnimVal(getter_AddRefs(transforms));
    NS_ASSERTION(transforms, "null transform list");
    PRUint32 numberOfItems;
    transforms->GetNumberOfItems(&numberOfItems);
    if (numberOfItems>0)
      transforms->GetConsolidationMatrix(getter_AddRefs(localTM));
  }
  
  nsCOMPtr<nsIDOMSVGMatrix> canvasTM;

  if (localTM)
    mClipParentMatrix->Multiply(localTM, getter_AddRefs(canvasTM));
  else
    canvasTM = mClipParentMatrix;

  /* object bounding box? */
  PRUint16 units;
  nsCOMPtr<nsIDOMSVGClipPathElement> path = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> aEnum;
  path->GetClipPathUnits(getter_AddRefs(aEnum));
  aEnum->GetAnimVal(&units);
  
  if (mClipParent &&
      units == nsIDOMSVGClipPathElement::SVG_CPUNITS_OBJECTBOUNDINGBOX) {
    nsCOMPtr<nsIDOMSVGRect> rect;
    nsresult rv = mClipParent->GetBBox(getter_AddRefs(rect));

    if (NS_SUCCEEDED(rv)) {
      float minx, miny, width, height;
      rect->GetX(&minx);
      rect->GetY(&miny);
      rect->GetWidth(&width);
      rect->GetHeight(&height);

      nsCOMPtr<nsIDOMSVGMatrix> tmp, fini;
      canvasTM->Translate(minx, miny, getter_AddRefs(tmp));
      tmp->ScaleNonUniform(width, height, getter_AddRefs(fini));
      canvasTM = fini;
    }
  }

  nsIDOMSVGMatrix* retval = canvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}
