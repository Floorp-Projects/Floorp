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
                           public nsISVGSVGFrame
{
  friend nsIFrame*
  NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);
protected:
  nsSVGInnerSVGFrame(nsStyleContext* aContext) :
    nsSVGInnerSVGFrameBase(aContext) {}
  
public:
  NS_DECL_QUERYFRAME

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
  NS_IMETHOD PaintSVG(nsSVGRenderState *aContext, const nsIntRect *aDirtyRect);
  virtual void NotifySVGChanged(PRUint32 aFlags);
  NS_IMETHOD_(nsIFrame*) GetFrameForPoint(const nsPoint &aPoint);

  // nsSVGContainerFrame methods:
  virtual already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();

protected:

  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGInnerSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGSVGElement> svg = do_QueryInterface(aContent);
  if (!svg) {
    NS_ERROR("Can't create frame! Content is not an SVG 'svg' element!");
    return nsnull;
  }

  return new (aPresShell) nsSVGInnerSVGFrame(aContext);
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_QUERYFRAME_HEAD(nsSVGInnerSVGFrame)
  NS_QUERYFRAME_ENTRY(nsISVGSVGFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGInnerSVGFrameBase)

nsIAtom *
nsSVGInnerSVGFrame::GetType() const
{
  return nsGkAtoms::svgInnerSVGFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGInnerSVGFrame::PaintSVG(nsSVGRenderState *aContext,
                             const nsIntRect *aDirtyRect)
{
  gfxContextAutoSaveRestore autoSR;

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    float x, y, width, height;
    static_cast<nsSVGSVGElement*>(mContent)->
      GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

    if (width <= 0 || height <= 0) {
      return NS_OK;
    }

    nsCOMPtr<nsIDOMSVGMatrix> clipTransform;
    if (!GetMatrixPropagation()) {
      NS_NewSVGMatrix(getter_AddRefs(clipTransform));
    } else {
      clipTransform = static_cast<nsSVGContainerFrame*>(mParent)->GetCanvasTM();
    }

    if (clipTransform) {
      gfxContext *gfx = aContext->GetGfxContext();
      autoSR.SetContext(gfx);
      nsSVGUtils::SetClipRect(gfx, clipTransform, x, y, width, height);
    }
  }

  return nsSVGInnerSVGFrameBase::PaintSVG(aContext, aDirtyRect);
}

void
nsSVGInnerSVGFrame::NotifySVGChanged(PRUint32 aFlags)
{
  if (aFlags & COORD_CONTEXT_CHANGED) {

    nsSVGSVGElement *svg = static_cast<nsSVGSVGElement*>(mContent);

    // Coordinate context changes affect mCanvasTM if we have a
    // percentage 'x' or 'y', or if we have a percentage 'width' or 'height' AND
    // a 'viewBox'.

    if (!(aFlags & TRANSFORM_CHANGED) &&
        (svg->mLengthAttributes[nsSVGSVGElement::X].IsPercentage() ||
         svg->mLengthAttributes[nsSVGSVGElement::Y].IsPercentage() ||
         (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox) &&
          (svg->mLengthAttributes[nsSVGSVGElement::WIDTH].IsPercentage() ||
           svg->mLengthAttributes[nsSVGSVGElement::HEIGHT].IsPercentage())))) {
    
      aFlags |= TRANSFORM_CHANGED;
    }

    // XXX We could clear the COORD_CONTEXT_CHANGED flag in some circumstances
    // if we have a non-percentage 'width' AND 'height, or if we have a 'viewBox'
    // rect. This is because, when we have a viewBox rect, the viewBox rect
    // is the coordinate context for our children, and it isn't changing.
    // Percentage lengths on our children will continue to resolve to the
    // same number of user units because they're relative to our viewBox rect. The
    // same is true if we have a non-percentage width and height and don't have a
    // viewBox. We (the <svg>) establish the coordinate context for our children. Our
    // children don't care about changes to our parent coordinate context unless that
    // change results in a change to the coordinate context that _we_ establish. Hence
    // we can (should, really) stop propagating COORD_CONTEXT_CHANGED in these cases.
    // We'd actually need to check that we have a viewBox rect and not just
    // that viewBox is set, since it could be set to none.
    // Take care not to break the testcase for bug 394463 when implementing this
  }

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
  }

  nsSVGInnerSVGFrameBase::NotifySVGChanged(aFlags);
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGInnerSVGFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  if (GetStyleDisplay()->IsScrollableOverflow()) {
    float clipX, clipY, clipWidth, clipHeight;
    nsCOMPtr<nsIDOMSVGMatrix> clipTransform;

    nsSVGElement *svg = static_cast<nsSVGElement*>(mContent);
    svg->GetAnimatedLengthValues(&clipX, &clipY, &clipWidth, &clipHeight, nsnull);

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>
                                             (mParent);
    clipTransform = parent->GetCanvasTM();

    if (!nsSVGUtils::HitTestRect(clipTransform,
                                 clipX, clipY, clipWidth, clipHeight,
                                 PresContext()->AppUnitsToDevPixels(aPoint.x),
                                 PresContext()->AppUnitsToDevPixels(aPoint.y))) {
      return nsnull;
    }
  }

  return nsSVGInnerSVGFrameBase::GetFrameForPoint(aPoint);
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
  PRUint32 flags = COORD_CONTEXT_CHANGED;

#if 1
  // XXX nsSVGSVGElement::InvalidateTransformNotifyFrame calls us for changes
  // to 'x' and 'y'. Until this is fixed, add TRANSFORM_CHANGED to flags
  // unconditionally.

  flags |= TRANSFORM_CHANGED;

  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
#else
  // viewport changes only affect our transform if we have a viewBox attribute
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    // make sure canvas transform matrix gets (lazily) recalculated:
    mCanvasTM = nsnull;

    flags |= TRANSFORM_CHANGED;
  }
#endif
  
  // inform children
  SuspendRedraw();
  nsSVGUtils::NotifyChildrenOfSVGChange(this, flags);
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGInnerSVGFrame::GetCanvasTM()
{
  if (!GetMatrixPropagation()) {
    nsIDOMSVGMatrix *retval;
    NS_NewSVGMatrix(&retval);
    return retval;
  }

  // parentTM * Translate(x,y) * viewBoxTM

  if (!mCanvasTM) {
    // get the transform from our parent's coordinate system to ours:
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = static_cast<nsSVGContainerFrame*>
                                                     (mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // append the transform due to the 'x' and 'y' attributes:
    float x, y;
    nsSVGSVGElement *svg = static_cast<nsSVGSVGElement*>(mContent);
    svg->GetAnimatedLengthValues(&x, &y, nsnull);

    nsCOMPtr<nsIDOMSVGMatrix> xyTM;
    parentTM->Translate(x, y, getter_AddRefs(xyTM));

    // append the viewbox to viewport transform:
    nsCOMPtr<nsIDOMSVGMatrix> viewBoxTM;
    nsSVGSVGElement *svgElement = static_cast<nsSVGSVGElement*>(mContent);
    nsresult res =
      svgElement->GetViewboxToViewportTransform(getter_AddRefs(viewBoxTM));
    if (NS_SUCCEEDED(res) && viewBoxTM) {
      xyTM->Multiply(viewBoxTM, getter_AddRefs(mCanvasTM));
    } else {
      NS_WARNING("We should propagate the fact that the viewBox is invalid.");
      mCanvasTM = xyTM;
    }
  }    

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

