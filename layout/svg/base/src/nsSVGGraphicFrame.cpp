/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGGraphicFrame.h"
#include "nsIPresContext.h"
#include "nsISVGFrame.h"
#include "nsSVGRenderingContext.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGPoint.h"

////////////////////////////////////////////////////////////////////////
// nsSVGGraphicFrame

nsSVGGraphicFrame::nsSVGGraphicFrame()
    : mUpdateFlags(0)
{
#ifdef DEBUG
//  printf("nsSVGGraphicFrame %p CTOR\n", this);
#endif
}

nsSVGGraphicFrame::~nsSVGGraphicFrame()
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
  NS_ASSERTION(transformable, "interface not found");
  if (value)
    value->RemoveObserver(this);

#ifdef DEBUG
//  printf("~nsSVGGraphicFrame %p\n", this);
#endif
}



//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGraphicFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGraphicFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGGraphicFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsStyleContext*  aContext,
                     nsIFrame*        aPrevInFlow)
{
//  rv = nsSVGGraphicFrameBase::Init(aPresContext, aContent, aParent,
//                                aContext, aPrevInFlow);

  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mParent = aParent;

  Init();
  
  SetStyleContext(aPresContext, aContext);
    
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::AttributeChanged(nsIPresContext* aPresContext,
                                    nsIContent*     aChild,
                                    PRInt32         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    PRInt32         aModType)
{
  // we don't use this notification mechanism
  
#ifdef DEBUG
//  nsAutoString str;
//  aAttribute->ToString(str);
//  printf("** nsSVGGraphicFrame::AttributeChanged(%s)\n",
//         NS_LossyConvertUCS2toASCII(str).get());
#endif
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
  // XXX: we'd like to use the style_hint mechanism and the
  // ContentStateChanged/AttributeChanged functions for style changes
  // to get slightly finer granularity, but unfortunately the
  // style_hints don't map very well onto svg. Here seems to be the
  // best place to deal with style changes:

  UpdateGraphic(NS_SVGGRAPHIC_UPDATE_FLAGS_STYLECHANGE);

  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGGraphicFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGGraphicFrame::DidModifySVGObservable (nsISVGValue* observable)
{
  // the observables we're listening in on affect the path by default.
  // We can specialize in the subclasses when needed.
  
  UpdateGraphic(NS_SVGGRAPHIC_UPDATE_FLAGS_PATHCHANGE);
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGFrame methods

NS_IMETHODIMP
nsSVGGraphicFrame::Paint(nsSVGRenderingContext* renderingContext)
{
  mGraphic.Paint(renderingContext);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
  *hit = nsnull;
  if (mGraphic.IsMouseHit(x, y)) {
    *hit = this;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::NotifyCTMChanged()
{
  UpdateGraphic(NS_SVGGRAPHIC_UPDATE_FLAGS_CTMCHANGE);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::NotifyRedrawSuspended()
{
  // XXX should we cache the fact that redraw is suspended?
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::NotifyRedrawUnsuspended()
{
  if (mUpdateFlags != 0) {
    ArtUta* uta = nsnull;
    uta = mGraphic.Update(mUpdateFlags, this);
    if (uta)
      InvalidateRegion(uta, PR_TRUE);
    mUpdateFlags = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGraphicFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  if (!SVGFrame) {
    NS_ASSERTION(SVGFrame, "no parent frame");
    *isSuspended = PR_FALSE;
    return NS_OK;
  }

  return SVGFrame->IsRedrawSuspended(isSuspended);  
}

NS_IMETHODIMP
nsSVGGraphicFrame::InvalidateRegion(ArtUta* uta, PRBool bRedraw)
{
  if (!uta && !bRedraw) return NS_OK;

  NS_ASSERTION(mParent, "need parent to invalidate!");
  if (!mParent) {
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  nsCOMPtr<nsISVGFrame> SVGFrame = do_QueryInterface(mParent);
  NS_ASSERTION(SVGFrame, "wrong frame type!");
  if (!SVGFrame) {
    if (uta)
      art_uta_free(uta);
    return NS_OK;
  }

  return SVGFrame->InvalidateRegion(uta, bRedraw);
}

//----------------------------------------------------------------------
// nsASVGGraphicSource methods:

void nsSVGGraphicFrame::GetCTM(nsIDOMSVGMatrix** ctm)
{
  *ctm = nsnull;
  
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content type");
  
  transformable->GetCTM(ctm);  
}


const nsStyleSVG* nsSVGGraphicFrame::GetStyle()
{
  return GetStyleSVG();
}


//----------------------------------------------------------------------
// 

nsresult nsSVGGraphicFrame::Init()
{
  nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
  NS_ASSERTION(transformable, "wrong content element");
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
  transformable->GetTransform(getter_AddRefs(transforms));
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
  NS_ASSERTION(transformable, "interface not found");
  if (value)
    value->AddObserver(this);
    
  return NS_OK;
}

void nsSVGGraphicFrame::UpdateGraphic(nsSVGGraphicUpdateFlags flags)
{
  mUpdateFlags |= flags;
  
  PRBool suspended;
  IsRedrawSuspended(&suspended);
  if (!suspended) {
    ArtUta* uta = nsnull;
    uta = mGraphic.Update(mUpdateFlags, this);
    if (uta)
      InvalidateRegion(uta, PR_TRUE);
    mUpdateFlags = 0;
  }  
}

