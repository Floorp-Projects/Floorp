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
#include "nsISVGRenderer.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGValueUtils.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsISVGContainerFrame.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prdtoa.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPathGeometryFrame

nsSVGPathGeometryFrame::nsSVGPathGeometryFrame()
    : mUpdateFlags(0)
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
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
  NS_REMOVE_SVGVALUE_OBSERVER(transforms);
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
//  rv = nsSVGPathGeometryFrameBase::Init(aPresContext, aContent, aParent,
//                                        aContext, aPrevInFlow);

  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  Init();
  
  SetStyleContext(aPresContext, aContext);
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::AttributeChanged(nsPresContext* aPresContext,
                                         nsIContent*     aChild,
                                         PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType,
                                         PRInt32         aHint)
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
  // XXX: we'd like to use the style_hint mechanism and the
  // ContentStateChanged/AttributeChanged functions for style changes
  // to get slightly finer granularity, but unfortunately the
  // style_hints don't map very well onto svg. Here seems to be the
  // best place to deal with style changes:

  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_ALL);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGPathGeometryFrame::Paint(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)
{
#ifdef DEBUG
  //printf("nsSVGPathGeometryFrame(%p)::Paint\n", this);
#endif
  GetGeometry()->Render(canvas);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
  //printf("nsSVGPathGeometryFrame(%p)::GetFrameForPoint\n", this);
#endif

  // test for hit:
  *hit = nsnull;
  PRBool isHit;
  GetGeometry()->ContainsPoint(x, y, &isHit);
  if (isHit) 
    *hit = this;
  
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGPathGeometryFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *region = nsnull;
  GetGeometry()->GetCoveredRegion(&region);
  return region;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::InitialUpdate()
{
  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_ALL);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathGeometryFrame::NotifyCTMChanged()
{
  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_CTM);
  
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
nsSVGPathGeometryFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGPathGeometryFrame::DidModifySVGObservable (nsISVGValue* observable)
{
  // the observables we're listening in on affect the ctm by
  // default. We can specialize in the subclasses when needed.
  
  UpdateGraphic(nsISVGGeometrySource::UPDATEMASK_CTM);
  
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

/* readonly attribute nsIDOMSVGMatrix CTM; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetCTM(nsIDOMSVGMatrix * *aCTM)
{
  *aCTM = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content type");
  
  return transformable->GetCTM(aCTM);  
}

/* readonly attribute float strokeOpacity; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeOpacity(float *aStrokeOpacity)
{
  *aStrokeOpacity = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeOpacity;
  return NS_OK;
}

/* readonly attribute float strokeWidth; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeWidth(float *aStrokeWidth)
{
  *aStrokeWidth = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeWidth;
  return NS_OK;
}

/* void getStrokeDashArray ([array, size_is (count)] out float arr, out unsigned long count); */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeDashArray(float **arr, PRUint32 *count)
{
  *arr = nsnull;
  *count = 0;
  
  const nsString &dasharrayString = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeDasharray;
  if (dasharrayString.Length() == 0) return NS_OK;

  // XXX parsing of the dasharray string should be done elsewhere

  char *str = ToNewCString(dasharrayString);

  // array elements are separated by commas. count them to get our max
  // no of elems.

  int i=0;
  char* cp = str;
  while (*cp) {
    if (*cp == ',')
      ++i;
    ++cp;
  }
  ++i;

  // now get the elements
  
  *arr = (float*) nsMemory::Alloc(i * sizeof(float));

  cp = str;
  char *elem;
  while ((elem = nsCRT::strtok(cp, "',", &cp))) {
    char *end;
    (*arr)[(*count)++] = (float) PR_strtod(elem, &end);
#ifdef DEBUG
//    printf("[%f]",(*arr)[(*count)-1]);
#endif
  }
  
  nsMemory::Free(str);

  return NS_OK;
}

/* readonly attribute float strokeDashoffset; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeDashoffset(float *aStrokeDashoffset)
{
  *aStrokeDashoffset = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeDashoffset;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinecap; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeLinecap(PRUint16 *aStrokeLinecap)
{
  *aStrokeLinecap = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeLinecap;
  return NS_OK;
}

/* readonly attribute unsigned short strokeLinejoin; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeLinejoin(PRUint16 *aStrokeLinejoin)
{
  *aStrokeLinejoin = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeLinejoin;
  return NS_OK;
}

/* readonly attribute float strokeMiterlimit; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokeMiterlimit(float *aStrokeMiterlimit)
{
  *aStrokeMiterlimit = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStrokeMiterlimit;
  return NS_OK;
}

/* readonly attribute float fillOpacity; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillOpacity(float *aFillOpacity)
{
  *aFillOpacity = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFillOpacity;
  return NS_OK;
}

/* readonly attribute unsigned short fillRule; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillRule(PRUint16 *aFillRule)
{
  *aFillRule = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFillRule;
  return NS_OK;
}

/* readonly attribute unsigned short strokePaintType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePaintType(PRUint16 *aStrokePaintType)
{
  *aStrokePaintType = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mType;
  return NS_OK;
}

/* [noscript] readonly attribute nscolor strokePaint; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetStrokePaint(nscolor *aStrokePaint)
{
  *aStrokePaint = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mColor;
  return NS_OK;
}

/* readonly attribute unsigned short fillPaintType; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPaintType(PRUint16 *aFillPaintType)
{
  *aFillPaintType = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mType;
  return NS_OK;
}

/* [noscript] readonly attribute nscolor fillPaint; */
NS_IMETHODIMP
nsSVGPathGeometryFrame::GetFillPaint(nscolor *aFillPaint)
{
  *aFillPaint = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mColor;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

NS_IMETHODIMP
nsSVGPathGeometryFrame::GetHittestMask(PRUint16 *aHittestMask)
{
  *aHittestMask=0;

  switch(((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      // XXX inspect 'visible' property
      if (((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_FILL;
      if (((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
      // XXX inspect 'visible' property
      *aHittestMask |= HITTEST_MASK_FILL;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
      // XXX inspect 'visible' property
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      // XXX inspect 'visible' property
      *aHittestMask |= HITTEST_MASK_FILL;
      *aHittestMask |= HITTEST_MASK_STROKE;
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      if (((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mFill.mType != eStyleSVGPaintType_None)
        *aHittestMask |= HITTEST_MASK_FILL;
      if (((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mStroke.mType != eStyleSVGPaintType_None)
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
  *aShapeRendering = ((const nsStyleSVG*) mStyleContext->GetStyleData(eStyleStruct_SVG))->mShapeRendering;
  return NS_OK;
}

//---------------------------------------------------------------------- 

nsresult
nsSVGPathGeometryFrame::Init()
{
//  nsresult rv = nsSVGPathGeometryFrameBase::Init();
//  if (NS_FAILED(rv)) return rv;

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

void nsSVGPathGeometryFrame::UpdateGraphic(PRUint32 flags)
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
    GetGeometry()->Update(mUpdateFlags, getter_AddRefs(dirty_region));
    if (dirty_region)
      outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);
    mUpdateFlags = 0;
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

