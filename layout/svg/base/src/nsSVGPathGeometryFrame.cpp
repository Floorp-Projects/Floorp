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
 * Portions created by the Initial Developer are Copyright (C) 2002
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
#include "nsIDOMSVGDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGValueUtils.h"
#include "nsISVGGeometrySource.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsISVGContainerFrame.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsSVGAtoms.h"
#include "nsCRT.h"
#include "prdtoa.h"
#include "nsSVGMarkerFrame.h"
#include "nsISVGMarkable.h"
#include "nsIViewManager.h"
#include "nsSVGMatrix.h"
#include "nsSVGClipPathFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIViewManager.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterFrame.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPathGeometryFrame

nsSVGPathGeometryFrame::nsSVGPathGeometryFrame()
  : mFilter(nsnull), mUpdateFlags(0), mPropagateTransform(PR_TRUE),
    mFillGradient(nsnull), mStrokeGradient(nsnull),
    mFillPattern(nsnull), mStrokePattern(nsnull)
{
#ifdef DEBUG
//  printf("nsSVGPathGeometryFrame %p CTOR\n", this);
#endif
}

nsSVGPathGeometryFrame::~nsSVGPathGeometryFrame()
{
#ifdef DEBUG
//  printf("~nsSVGPathGeometryFrame %p\n", this);
#endif
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  NS_REMOVE_SVGVALUE_OBSERVER(transforms);
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  if (mFilter) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFilter);
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
  }
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGPathGeometryFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISVGPathGeometrySource)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPathGeometryFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods
  
NS_IMETHODIMP
nsSVGPathGeometryFrame::Init(nsPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsStyleContext*  aContext,
                             nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  if (mContent) {
    mContent->SetMayHaveFrame(PR_TRUE);
  }
  
  InitSVG();
  
  SetStyleContext(aPresContext, aContext);
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType)
{
  // we don't use this notification mechanism
  
#ifdef DEBUG
//  printf("** nsSVGPathGeometryFrame::AttributeChanged(");
//  nsAutoString str;
//  aAttribute->ToString(str);
//  nsCAutoString cstr;
//  cstr.AssignWithConversion(str);
//  printf(cstr.get());
//  printf(")\n");
#endif
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
  // One of the styles that might have been changed are the urls that
  // point to gradients, etc.  Drop our cached values to those
  if (mFillGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillGradient);
    mFillGradient = nsnull;
  }
  if (mStrokeGradient) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokeGradient);
    mStrokeGradient = nsnull;
  }
  if (mFilter) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFilter);
    mFilter = nsnull;
  }
  if (mFillPattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mFillPattern);
    mFillPattern = nsnull;
  }
  if (mStrokePattern) {
    NS_REMOVE_SVGVALUE_OBSERVER(mStrokePattern);
    mStrokePattern = nsnull;
  }

  // XXX: we'd like to use the style_hint mechanism and the
  // ContentStateChanged/AttributeChanged functions for style changes
  // to get slightly finer granularity, but unfortunately the
  // style_hints don't map very well onto svg. Here seems to be the
  // best place to deal with style changes:

  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_ALL);

  return NS_OK;
}

nsIAtom *
nsSVGPathGeometryFrame::GetType() const
{
  return nsLayoutAtoms::svgPathGeometryFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

// marker helper
void
nsSVGPathGeometryFrame::GetMarkerFrames(nsSVGMarkerFrame **markerStart,
                                        nsSVGMarkerFrame **markerMid,
                                        nsSVGMarkerFrame **markerEnd)
{
  nsIURI *aURI;

  *markerStart = *markerMid = *markerEnd = NULL;
  
  aURI = GetStyleSVG()->mMarkerEnd;
  if (aURI)
    NS_GetSVGMarkerFrame(markerEnd, aURI, mContent);
    
  aURI = GetStyleSVG()->mMarkerMid;
  if (aURI)
    NS_GetSVGMarkerFrame(markerMid, aURI, mContent);
  
  aURI = GetStyleSVG()->mMarkerStart;
  if (aURI)
    NS_GetSVGMarkerFrame(markerStart, aURI, mContent);
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::PaintSVG(nsISVGRendererCanvas* canvas, 
                                 const nsRect& dirtyRectTwips,
                                 PRBool ignoreFilter)
{
  if (!GetStyleVisibility()->IsVisible())
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

  /* check for a clip path */

  nsSVGClipPathFrame *clip = nsnull;
  aURI = GetStyleSVGReset()->mClipPath;
  if (aURI) {
    NS_GetSVGClipPathFrame(&clip, aURI, mContent);

    if (clip) {
      nsCOMPtr<nsIDOMSVGMatrix> matrix;
      GetCanvasTM(getter_AddRefs(matrix));
      canvas->PushClip();
      clip->ClipPaint(canvas, this, matrix);
    }

#ifdef DEBUG_tor    
    nsCAutoString spec;
    aURI->GetAsciiSpec(spec);
    fprintf(stderr, "CLIPPATH %s %p\n", spec.get(), clip);
#endif
  }

  /* render */
  GetGeometry()->Render(canvas);
  
  nsISVGMarkable *markable;
  CallQueryInterface(this, &markable);

  if (markable) {
    nsSVGMarkerFrame *markerEnd, *markerMid, *markerStart;
    GetMarkerFrames(&markerStart, &markerMid, &markerEnd);

    if (markerEnd || markerMid || markerStart) {
      // need to set this up with the first draw
      if (!mMarkerRegion)
        mMarkerRegion = GetCoveredRegion();

      float strokeWidth;
      GetStrokeWidth(&strokeWidth);
      
      nsVoidArray marks;
      markable->GetMarkPoints(&marks);
    
      PRUint32 num = marks.Count();
      
      if (num && markerStart)
        markerStart->PaintMark(canvas, this, (nsSVGMark *)marks[0], strokeWidth);
      
      if (num && markerMid)
        for (PRUint32 i = 1; i < num - 1; i++)
          markerMid->PaintMark(canvas, this, (nsSVGMark *)marks[i], strokeWidth);

      if (num && markerEnd)
        markerEnd->PaintMark(canvas, this, (nsSVGMark *)marks[num-1], strokeWidth);
    }
  }

  if (clip)
    canvas->PopClip();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGPathGeometryFrame(%p)::GetFrameForPoint\n", this);
#endif

  // test for hit:
  *hit = nsnull;
  PRBool isHit;
  GetGeometry()->ContainsPoint(x, y, &isHit);

  if (isHit) {
    PRBool clipHit = PR_TRUE;;

    nsIURI *aURI;
    nsSVGClipPathFrame *clip = NULL;
    aURI = GetStyleSVGReset()->mClipPath;
    if (aURI)
      NS_GetSVGClipPathFrame(&clip, aURI, mContent);

    if (clip) {
      nsCOMPtr<nsIDOMSVGMatrix> matrix;
      GetCanvasTM(getter_AddRefs(matrix));
      clip->ClipHitTest(this, matrix, x, y, &clipHit);
    }

    if (clipHit)
      *hit = this;
  }
  
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGPathGeometryFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *region = nsnull;
  if (!GetGeometry())
    return region;

  GetGeometry()->GetCoveredRegion(&region);

  nsISVGMarkable *markable;
  CallQueryInterface(this, &markable);

  if (markable) {
    nsSVGMarkerFrame *markerEnd, *markerMid, *markerStart;
    GetMarkerFrames(&markerStart, &markerMid, &markerEnd);

    if (!markerEnd && !markerMid && !markerStart)
      return region;

    float strokeWidth;
    GetStrokeWidth(&strokeWidth);

    nsVoidArray marks;
    markable->GetMarkPoints(&marks);

    PRUint32 num = marks.Count();

    if (markerStart) {
      nsCOMPtr<nsISVGRendererRegion> mark;
      mark = markerStart->RegionMark(this, (nsSVGMark *)marks[0], strokeWidth);

      if (mark) {
        nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
        mark->Combine(tmp, &region);
      }
    }

    if (markerMid)
      for (PRUint32 i = 1; i < num - 1; i++) {
        nsCOMPtr<nsISVGRendererRegion> mark;
        mark = markerMid->RegionMark(this, (nsSVGMark *)marks[i], strokeWidth);

        if (mark) {
          nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
          mark->Combine(tmp, &region);
        }
      }

    if (markerEnd) {
      nsCOMPtr<nsISVGRendererRegion> mark;
      mark = markerEnd->RegionMark(this, (nsSVGMark *)marks[num-1], strokeWidth);

      if (mark) {
        nsCOMPtr<nsISVGRendererRegion> tmp = dont_AddRef(region);
        mark->Combine(tmp, &region);
      }
    }
  }

  return region;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::InitialUpdate()
{
  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_ALL);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_CANVAS_TM,
                suppressInvalidation);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyRedrawUnsuspended()
{
  if (mUpdateFlags != 0) {
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    if (GetGeometry())
      GetGeometry()->Update(mUpdateFlags, getter_AddRefs(dirty_region));
    if (dirty_region) {
      nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
      if (outerSVGFrame)
        outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    }
    mUpdateFlags = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  if (GetGeometry())
    return GetGeometry()->GetBoundingBox(_retval);
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::WillModifySVGObservable(nsISVGValue* observable,
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
  }

  return NS_OK;
}


NS_IMETHODIMP
nsSVGPathGeometryFrame::DidModifySVGObservable (nsISVGValue* observable,
                                                nsISVGValue::modificationType aModType)
{
  // Is this a gradient?
  nsCOMPtr<nsISVGGradient> gradient = do_QueryInterface(observable);
  nsCOMPtr<nsISVGPattern> pval = do_QueryInterface(observable);

  nsISVGFilterFrame *filter;
  CallQueryInterface(observable, &filter);

  if (gradient) {
    // Yes, we need to handle this differently
    nsCOMPtr<nsISVGGradient>fill = do_QueryInterface(mFillGradient);
    if (fill == gradient) {
      if (aModType == nsISVGValue::mod_die) {
        mFillGradient = nsnull;
      }
      UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_FILL_PAINT);
    } else {
      // No real harm in assuming a stroke gradient at this point
      if (aModType == nsISVGValue::mod_die) {
        mStrokeGradient = nsnull;
      }
      UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT);
    }
  } else if (filter) {
    if (aModType == nsISVGValue::mod_die) {
      mFilter = nsnull;
      mFilterRegion = nsnull;
    }
    UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT |
                  nsISVGGeometrySource::UPDATEMASK_FILL_PAINT);
  } else if (pval) {
    // Handle Patterns
    nsCOMPtr<nsISVGPattern>fill = do_QueryInterface(mFillPattern);
    if (fill == pval) {
      if (aModType == nsISVGValue::mod_die) {
        mFillPattern = nsnull;
      }
      UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_FILL_PAINT);
    } else {
      // Assume stroke pattern
      if (aModType == nsISVGValue::mod_die) {
        mStrokePattern = nsnull;
      }
      UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT);
    }
  } else {
    // No, all of our other observables update the canvastm by default
    UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_CANVAS_TM);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGGeometrySource methods:

/* [noscript] readonly attribute nsPresContext presContext; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetPresContext(nsPresContext * *aPresContext)
{
  // XXX gcc 3.2.2 requires the explicit 'nsSVGPathGeometryFrameBase::' qualification
  *aPresContext = nsSVGPathGeometryFrameBase::GetPresContext();
  NS_ADDREF(*aPresContext);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGMatrix canvasTM; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetCanvasTM(nsIDOMSVGMatrix * *aCTM)
{
  *aCTM = nsnull;

  if (!mPropagateTransform) {
    if (mOverrideCTM) {
      *aCTM = mOverrideCTM;
      NS_ADDREF(*aCTM);
      return NS_OK;
    }
    return NS_NewSVGMatrix(aCTM);
  }

  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
  NS_ASSERTION(parentTM, "null TM");

  // append our local transformations if we have any:
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
  if (localTM) {
    return parentTM->Multiply(localTM, aCTM);
  }
  *aCTM = parentTM;
  NS_ADDREF(*aCTM);
  return NS_OK;
}

/* readonly attribute float strokeOpacity; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeOpacity(float *aStrokeOpacity)
{
  *aStrokeOpacity =
    GetStyleSVG()->mStrokeOpacity * GetStyleDisplay()->mOpacity;
  return NS_OK;
}

/* readonly attribute float strokeWidth; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeWidth(float *aStrokeWidth)
{
  *aStrokeWidth =
    nsSVGUtils::CoordToFloat(nsSVGPathGeometryFrameBase::GetPresContext(),
                             mContent, GetStyleSVG()->mStrokeWidth);
  return NS_OK;
}

/* void getStrokeDashArray ([array, size_is (count)] out float arr, out unsigned long count); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeDashArray(float **arr, PRUint32 *count)
{
  const nsStyleCoord *dasharray = GetStyleSVG()->mStrokeDasharray;
  nsPresContext *presContext = nsSVGPathGeometryFrameBase::GetPresContext();
  float totalLength = 0.0f;

  *count = GetStyleSVG()->mStrokeDasharrayLength;
  *arr = nsnull;

  if (*count) {
    *arr = (float *) nsMemory::Alloc(*count * sizeof(float));
    if (*arr) {
      for (PRUint32 i = 0; i < *count; i++) {
        (*arr)[i] = nsSVGUtils::CoordToFloat(presContext, mContent, dasharray[i]);
        if ((*arr)[i] < 0.0f) {
          nsMemory::Free(*arr);
          *count = 0;
          *arr = nsnull;
          return NS_OK;
        }
        totalLength += (*arr)[i];
      }
    } else {
      *count = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (totalLength == 0.0f) {
      nsMemory::Free(*arr);
      *count = 0;
      *arr = nsnull;
    }
  }

  return NS_OK;
}

/* readonly attribute float strokeDashoffset; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeDashoffset(float *aStrokeDashoffset)
{
  *aStrokeDashoffset = 
    nsSVGUtils::CoordToFloat(nsSVGPathGeometryFrameBase::GetPresContext(),
                             mContent, GetStyleSVG()->mStrokeDashoffset);
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinecap; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeLinecap(PRUint16 *aStrokeLinecap)
{
  *aStrokeLinecap = GetStyleSVG()->mStrokeLinecap;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinejoin; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeLinejoin(PRUint16 *aStrokeLinejoin)
{
  *aStrokeLinejoin = GetStyleSVG()->mStrokeLinejoin;
  return NS_OK;
}

/* readonly attribute float strokeMiterlimit; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeMiterlimit(float *aStrokeMiterlimit)
{
  *aStrokeMiterlimit = GetStyleSVG()->mStrokeMiterlimit;
  return NS_OK;
}

/* readonly attribute float fillOpacity; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillOpacity(float *aFillOpacity)
{
  *aFillOpacity =
    GetStyleSVG()->mFillOpacity * GetStyleDisplay()->mOpacity;
  return NS_OK;
}

/* readonly attribute unsigned short fillRule; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillRule(PRUint16 *aFillRule)
{
  *aFillRule = GetStyleSVG()->mFillRule;
  return NS_OK;
}

/* readonly attribute unsigned short clipRule; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetClipRule(PRUint16 *aClipRule)
{
  *aClipRule = GetStyleSVG()->mClipRule;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePaintType(PRUint16 *aStrokePaintType)
{
  *aStrokePaintType = GetStyleSVG()->mStroke.mType;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintServerType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePaintServerType(PRUint16 *aStrokePaintServerType) {
  return nsSVGUtils::GetPaintType(aStrokePaintServerType, GetStyleSVG()->mStroke, mContent,
                                  nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
}

/* [noscript] readonly attribute nscolor strokePaint; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePaint(nscolor *aStrokePaint)
{
  *aStrokePaint = GetStyleSVG()->mStroke.mPaint.mColor;
  return NS_OK;
}

/* [noscript] void GetStrokeGradient(nsISVGGradient **aGrad); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mStrokeGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(getter_AddRefs(mStrokeGradient), aServer, mContent, 
                           nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mStrokeGradient);
  }
  *aGrad = mStrokeGradient;
  return rv;
}

/* [noscript] void GetStrokePattern(nsISVGPattern **aPat); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mStrokePattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mStroke.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGPattern(getter_AddRefs(mStrokePattern), aServer, mContent, 
                          nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mStrokePattern);
  }
  *aPat = mStrokePattern;
  return rv;
}

/* readonly attribute unsigned short fillPaintType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPaintType(PRUint16 *aFillPaintType)
{
  *aFillPaintType = GetStyleSVG()->mFill.mType;
  return NS_OK;
}

/* readonly attribute unsigned short fillPaintServerType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPaintServerType(PRUint16 *aFillPaintServerType)
{
  return nsSVGUtils::GetPaintType(aFillPaintServerType, GetStyleSVG()->mFill, mContent,
                                  nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
}

/* [noscript] readonly attribute nscolor fillPaint; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPaint(nscolor *aFillPaint)
{
  *aFillPaint = GetStyleSVG()->mFill.mPaint.mColor;
  return NS_OK;
}

/* [noscript] void GetFillGradient(nsISVGGradient **aGrad); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillGradient(nsISVGGradient **aGrad)
{
  nsresult rv = NS_OK;
  *aGrad = nsnull;
  if (!mFillGradient) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the gradient 
    rv = NS_GetSVGGradient(getter_AddRefs(mFillGradient), aServer, mContent, 
                           nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mFillGradient);
  }
  *aGrad = mFillGradient;
  return rv;
}

/* [noscript] void GetFillPattern(nsISVGPattern **aPat); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPattern(nsISVGPattern **aPat)
{
  nsresult rv = NS_OK;
  *aPat = nsnull;
  if (!mFillPattern) {
    nsIURI *aServer;
    aServer = GetStyleSVG()->mFill.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;
    // Now have the URI.  Get the pattern 
    rv = NS_GetSVGPattern(getter_AddRefs(mFillPattern), aServer, mContent, 
                          nsSVGPathGeometryFrameBase::GetPresContext()->PresShell());
    NS_ADD_SVGVALUE_OBSERVER(mFillPattern);
  }
  *aPat = mFillPattern;
  return rv;
}

/* [noscript] boolean isClipChild; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::IsClipChild(PRBool *_retval)
{
  *_retval = PR_FALSE;
  nsCOMPtr<nsIContent> node(mContent);

  do {
    if (node->Tag() == nsSVGAtoms::clipPath) {
      *_retval = PR_TRUE;
      break;
    }
    node = node->GetParent();
  } while (node);
    
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetHittestMask(PRUint16 *aHittestMask)
{
  *aHittestMask=0;

  switch(GetStyleSVG()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      if (GetStyleVisibility()->IsVisible()) {
        if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
          *aHittestMask |= HITTEST_MASK_FILL;
        if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
          *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_FILL;
        *aHittestMask |= HITTEST_MASK_STROKE;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (GetStyleSVG()->mFill.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_FILL;
      if (GetStyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
      *aHittestMask |= HITTEST_MASK_FILL;
      break;
    case NS_STYLE_POINTER_EVENTS_STROKE:
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_ALL:
      *aHittestMask |= HITTEST_MASK_FILL;
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }
  
  return NS_OK;
}

/* readonly attribute unsigned short shapeRendering; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetShapeRendering(PRUint16 *aShapeRendering)
{
  *aShapeRendering = GetStyleSVG()->mShapeRendering;
  return NS_OK;
}

//---------------------------------------------------------------------- 

NS_IMETHODIMP
nsSVGPathGeometryFrame::InitSVG()
{
  // all path geometry frames listen in on changes to their
  // corresponding content element's transform attribute:
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  NS_ADD_SVGVALUE_OBSERVER(transforms);

  // construct a pathgeometry object:
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("Null outerSVGFrame");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (!renderer) return NS_ERROR_FAILURE;

  renderer->CreatePathGeometry(this, getter_AddRefs(mGeometry));
  
  if (!mGeometry) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsISVGRendererPathGeometry *
nsSVGPathGeometryFrame::GetGeometry()
{
#ifdef DEBUG
  NS_ASSERTION(mGeometry, "invalid geometry object");
#endif
  return mGeometry;
}

void nsSVGPathGeometryFrame::UpdateGraphic(PRUint32 flags,
                                           PRBool suppressInvalidation)
{
  mUpdateFlags |= flags;
  
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }

  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    nsCOMPtr<nsISVGRendererRegion> dirty_region;
    if (GetGeometry())
      GetGeometry()->Update(mUpdateFlags, getter_AddRefs(dirty_region));

    mUpdateFlags = 0;

    if (suppressInvalidation)
      return;

    nsCOMPtr<nsISVGRendererRegion> filter_region;
    nsSVGUtils::FindFilterInvalidation(this,
                                       getter_AddRefs(filter_region));
    if (filter_region) {
      outerSVGFrame->InvalidateRegion(filter_region, PR_TRUE);
    } else {
      if (mMarkerRegion) {
        outerSVGFrame->InvalidateRegion(mMarkerRegion, PR_TRUE);
        mMarkerRegion = nsnull;
      }

      nsISVGMarkable *markable;
      CallQueryInterface(this, &markable);
      
      if (markable) {
        nsSVGMarkerFrame *markerEnd, *markerMid, *markerStart;
        GetMarkerFrames(&markerStart, &markerMid, &markerEnd);

        if (markerEnd || markerMid || markerStart) {
          mMarkerRegion = GetCoveredRegion();
          if (mMarkerRegion)
            outerSVGFrame->InvalidateRegion(mMarkerRegion, PR_TRUE);
          return;
        }
      }

      if (dirty_region)
        outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    }
  }
}

nsISVGOuterSVGFrame *
nsSVGPathGeometryFrame::GetOuterSVGFrame()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetOuterSVGFrame();  
}

