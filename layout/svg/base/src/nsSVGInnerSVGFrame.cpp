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

#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsSVGMatrix.h"
#include "nsSVGSVGElement.h"
#include "nsSVGContainerFrame.h"
#include "gfxContext.h"

typedef nsSVGDisplayContainerFrame nsSVGInnerSVGFrameBase;

class nsSVGInnerSVGFrame : public nsSVGInnerSVGFrameBase,
                           public nsISVGValueObserver,
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
protected:
  nsSVGInnerSVGFrame(nsStyleContext* aContext);
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  

public:
  // nsIFrame:
  NS_IMETHOD DidSetStyleContext();

  // We don't define an AttributeChanged method since changes to the
  // 'x', 'y', 'width' and 'height' attributes of our content object
  // are handled in nsSVGSVGElement::DidModifySVGObservable

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgInnerSVGFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGInnerSVG"), aResult);
  }
#endif

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsSVGRenderState *aContext, nsRect *aDirtyRect);
  NS_IMETHOD NotifyCanvasTMChanged(PRBool suppressInvalidation);
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate);
  NS_IMETHOD SetOverrideCTM(nsIDOMSVGMatrix *aCTM);
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit);

  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

protected:

  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
  nsCOMPtr<nsIDOMSVGMatrix> mOverrideCTM;

  PRPackedBool mPropagateTransform;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGInnerSVGFrame(aContext);
}

nsSVGInnerSVGFrame::nsSVGInnerSVGFrame(nsStyleContext* aContext) :
  nsSVGInnerSVGFrameBase(aContext), mPropagateTransform(PR_TRUE)
{
#ifdef DEBUG
//  printf("nsSVGInnerSVGFrame CTOR\n");
#endif
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGInnerSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGInnerSVGFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods

nsIAtom *
nsSVGInnerSVGFrame::GetType() const
{
  return nsGkAtoms::svgInnerSVGFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::PaintSVG(nsSVGRenderState *aContext, nsRect *aDirtyRect)
{
  nsresult rv = NS_OK;

  gfxContext *gfx = aContext->GetGfxContext();

  gfx->Save();

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    nsSVGSVGElement *svg = NS_STATIC_CAST(nsSVGSVGElement*, mContent);

    float x, y, width, height;
    svg->GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

    nsCOMPtr<nsIDOMSVGMatrix> clipTransform;
    if (!mPropagateTransform) {
      NS_NewSVGMatrix(getter_AddRefs(clipTransform));
    } else {
      nsSVGContainerFrame *parent = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                   mParent);
      clipTransform = parent->GetCanvasTM();
    }

    if (clipTransform)
      nsSVGUtils::SetClipRect(gfx, clipTransform, x, y, width, height);
  }

  rv = nsSVGInnerSVGFrameBase::PaintSVG(aContext, aDirtyRect);

  gfx->Restore();

  return rv;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  // make sure our cached transform matrix gets (lazily) updated
  mCanvasTM = nsnull;

  return nsSVGInnerSVGFrameBase::NotifyCanvasTMChanged(suppressInvalidation);
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  if (GetStyleDisplay()->IsScrollableOverflow()) {
    float clipX, clipY, clipWidth, clipHeight;
    nsCOMPtr<nsIDOMSVGMatrix> clipTransform;

    nsSVGElement *svg = NS_STATIC_CAST(nsSVGElement*, mContent);
    svg->GetAnimatedLengthValues(&clipX, &clipY, &clipWidth, &clipHeight, nsnull);

    nsSVGContainerFrame *parent = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                 mParent);
    clipTransform = parent->GetCanvasTM();

    if (!nsSVGUtils::HitTestRect(clipTransform,
                                 clipX, clipY, clipWidth, clipHeight,
                                 x, y)) {
      *hit = nsnull;
      return NS_OK;
    }
  }

  return nsSVGInnerSVGFrameBase::GetFrameForPointSVG(x, y, hit);
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:

NS_IMETHODIMP
nsSVGInnerSVGFrame::SuspendRedraw()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->SuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::UnsuspendRedraw()
{
  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("no outer svg frame");
    return NS_ERROR_FAILURE;
  }
  return outerSVGFrame->UnsuspendRedraw();
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::NotifyViewportChange()
{
  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame = nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(PR_FALSE); 
    kid = kid->GetNextSibling();
  }
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGInnerSVGFrame::GetCanvasTM()
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

  // parentTM * Translate(x,y) * viewboxToViewportTM

  if (!mCanvasTM) {
    // get the transform from our parent's coordinate system to ours:
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                         mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // append the transform due to the 'x' and 'y' attributes:
    float x, y;
    nsSVGSVGElement *svg = NS_STATIC_CAST(nsSVGSVGElement*, mContent);
    svg->GetAnimatedLengthValues(&x, &y, nsnull);

    nsCOMPtr<nsIDOMSVGMatrix> xyTM;
    parentTM->Translate(x, y, getter_AddRefs(xyTM));

    // append the viewbox to viewport transform:
    nsCOMPtr<nsIDOMSVGMatrix> viewBoxToViewportTM;
    nsSVGSVGElement *svgElement = NS_STATIC_CAST(nsSVGSVGElement*, mContent);
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(viewBoxToViewportTM));
    xyTM->Multiply(viewBoxToViewportTM, getter_AddRefs(mCanvasTM));
  }    

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGInnerSVGFrame::WillModifySVGObservable(nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
  return NS_OK;
}
	
NS_IMETHODIMP
nsSVGInnerSVGFrame::DidModifySVGObservable (nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
  NotifyViewportChange();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGInnerSVGFrame::DidSetStyleContext()
{
  nsSVGUtils::StyleEffects(this);
  return NS_OK;
}
