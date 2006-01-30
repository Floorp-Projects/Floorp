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
#include "nsSVGGFrame.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererSurface.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIFrame.h"
#include "nsSVGMatrix.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsLayoutAtoms.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterFrame.h"
#include "nsISVGValueUtils.h"
#include "nsSVGMaskFrame.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsIContent* aContent)
{  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(aContent);
  if (!transformable) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGGFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGGFrame;
}

nsIAtom *
nsSVGGFrame::GetType() const
{
  return nsLayoutAtoms::svgGFrame;
}

nsSVGGFrame::~nsSVGGFrame()
{
  if (mFilter) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFilter);
  }
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGGFrame::PaintSVG(nsISVGRendererCanvas* canvas,
                      const nsRect& dirtyRectTwips,
                      PRBool ignoreFilter)
{
  const nsStyleDisplay *display = mStyleContext->GetStyleDisplay();
  if (display->mOpacity == 0.0)
    return NS_OK;

  nsIURI *aURI;

  /* check for filter */
  
  if (!ignoreFilter) {
    if (!mFilter) {
      aURI = GetStyleSVGReset()->mFilter;
      if (aURI)
        NS_GetSVGFilterFrame(&mFilter, aURI, mContent);
      if (mFilter)
        NS_ADD_SVGVALUE_OBSERVER(mFilter);
    }

    if (mFilter) {
      if (!mFilterRegion)
        mFilter->GetInvalidationRegion(this, getter_AddRefs(mFilterRegion));
      mFilter->FilterPaint(canvas, this);
      return NS_OK;
    }
  }

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();

  /* check for a clip path */

  PRBool trivialClip = PR_TRUE;
  nsISVGClipPathFrame *clip = NULL;
  nsCOMPtr<nsISVGRendererSurface> clipMaskSurface;

  aURI = GetStyleSVGReset()->mClipPath;
  if (aURI) {
    NS_GetSVGClipPathFrame(&clip, aURI, mContent);

    if (clip) {
      clip->IsTrivial(&trivialClip);

      if (trivialClip) {
        canvas->PushClip();
      } else {
        nsSVGUtils::GetSurface(outerSVGFrame, canvas,
                               getter_AddRefs(clipMaskSurface));
        if (!clipMaskSurface)
          clip = nsnull;
      }

      if (clip) {
        nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM();
        clip->ClipPaint(canvas, clipMaskSurface, this, matrix);
      }
    }
  }

  /* check for mask */

  nsISVGMaskFrame *mask = nsnull;
  nsCOMPtr<nsISVGRendererSurface> maskSurface, maskedSurface;

  aURI = GetStyleSVGReset()->mMask;
  if (aURI) {
    NS_GetSVGMaskFrame(&mask, aURI, mContent);

    if (mask) {
      nsSVGUtils::GetSurface(outerSVGFrame, canvas,
                             getter_AddRefs(maskSurface));

      if (maskSurface) {
        nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM();
        if (NS_FAILED(mask->MaskPaint(canvas, maskSurface, this, matrix,
                                      display->mOpacity)))
          maskSurface = nsnull;
      }
    }
  }

  if (maskSurface || clipMaskSurface || display->mOpacity != 1.0) {
    nsSVGUtils::GetSurface(outerSVGFrame, canvas,
                           getter_AddRefs(maskedSurface));
    if (maskedSurface) {
      canvas->PushSurface(maskedSurface);
    } else
      maskSurface = nsnull;
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->PaintSVG(canvas, dirtyRectTwips, PR_FALSE);
  }

  if (maskedSurface)
    canvas->PopSurface();

  if (clipMaskSurface) {
    if (!maskSurface && display->mOpacity == 1.0) {
      maskSurface = clipMaskSurface;
    } else {
      nsCOMPtr<nsISVGRendererSurface> clipped;
      nsSVGUtils::GetSurface(outerSVGFrame, canvas,
                             getter_AddRefs(clipped));
      
      canvas->PushSurface(clipped);
      canvas->CompositeSurfaceWithMask(maskedSurface, 0, 0, clipMaskSurface);
      canvas->PopSurface();
      maskedSurface = clipped;
    }
  }

  if (maskSurface)
    canvas->CompositeSurfaceWithMask(maskedSurface, 0, 0, maskSurface);
  else if (display->mOpacity != 1.0)
    canvas->CompositeSurface(maskedSurface, 0, 0, display->mOpacity);

  if (clip && trivialClip)
    canvas->PopClip();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  *hit = nsnull;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsIFrame* temp=nsnull;
      nsresult rv = SVGFrame->GetFrameForPointSVG(x, y, &temp);
      if (NS_SUCCEEDED(rv) && temp) {
        *hit = temp;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
  }

  if (*hit) {
    PRBool clipHit = PR_TRUE;;

    nsIURI *aURI;
    nsISVGClipPathFrame *clip = NULL;
    aURI = GetStyleSVGReset()->mClipPath;
    if (aURI)
      NS_GetSVGClipPathFrame(&clip, aURI, mContent);

    if (clip) {
      nsCOMPtr<nsIDOMSVGMatrix> matrix = GetCanvasTM();
      clip->ClipHitTest(this, matrix, x, y, &clipHit);
    }

    if (!clipHit)
      *hit = nsnull;
  }
  
  return *hit ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGGFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *accu_region=nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
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
  
  return accu_region;
}

NS_IMETHODIMP
nsSVGGFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  return nsSVGUtils::GetBBox(&mFrames, _retval);
}

NS_IMETHODIMP
nsSVGGFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGGFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    if (mOverrideCTM) {
      retval = mOverrideCTM;
      NS_ADDREF(retval);
    } else {
      NS_NewSVGMatrix(&retval);
    }
    return retval;
  }

  return nsSVGDefsFrame::GetCanvasTM();
}

NS_IMETHODIMP
nsSVGGFrame::WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType)
{
  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  // need to handle filters because we might be the topmost filtered frame and
  // the filter region could be changing.
  if (filter && mFilterRegion) {
    nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
    if (!outerSVGFrame)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISVGRendererRegion> region;
    nsSVGUtils::FindFilterInvalidation(this, getter_AddRefs(region));
    outerSVGFrame->InvalidateRegion(region, PR_TRUE);

    return NS_OK;
  }

  return nsSVGGFrameBase::WillModifySVGObservable(observable, aModType);
}

NS_IMETHODIMP
nsSVGGFrame::DidModifySVGObservable(nsISVGValue* observable,
                                    nsISVGValue::modificationType aModType)
{
  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  if (filter) {
    if (aModType == nsISVGValue::mod_die) {
      mFilter = nsnull;
      mFilterRegion = nsnull;
    }

    nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
    if (!outerSVGFrame)
      return NS_ERROR_FAILURE;

    if (mFilter)
      mFilter->GetInvalidationRegion(this, getter_AddRefs(mFilterRegion));
      
    nsCOMPtr<nsISVGRendererRegion> region;
    nsSVGUtils::FindFilterInvalidation(this, getter_AddRefs(region));
    
    if (region)
      outerSVGFrame->InvalidateRegion(region, PR_TRUE);
  }

  return nsSVGGFrameBase::DidModifySVGObservable(observable, aModType);
}
