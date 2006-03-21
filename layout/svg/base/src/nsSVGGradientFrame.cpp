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
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
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

#include "nsSVGGenericContainerFrame.h"
#include "nsSVGGradient.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGStopElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsStyleContext.h"
#include "nsSVGValue.h"
#include "nsNetUtil.h"
#include "nsINameSpaceManager.h"
#include "nsISVGChildFrame.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsISVGGeometrySource.h"
#include "nsISVGGradient.h"
#include "nsIURI.h"
#include "nsIContent.h"
#include "nsSVGNumber.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGUtils.h"
#include "nsWeakReference.h"
#include "nsISVGValueObserver.h"
#include "nsContentUtils.h"
#include "nsSVGDefsFrame.h"
  
typedef nsSVGDefsFrame  nsSVGGradientFrameBase;

class nsSVGGradientFrame : public nsSVGGradientFrameBase,
                           public nsSVGValue,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference,
                           public nsISVGGradient
{
protected:
  friend nsIFrame* NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent);

  friend nsIFrame* NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent);

  friend nsIFrame* NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                                      nsIContent*   aContent, 
                                      nsIFrame*     aParentFrame);

  friend nsresult NS_GetSVGGradientFrame(nsIFrame**      result, 
                                         nsIURI*         aURI, 
                                         nsIContent*     aContent,
                                         nsIPresShell*   aPresShell);


public:
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGGradient interface:
  NS_DECL_NSISVGGRADIENT

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString &aValue) { return NS_OK; }
  NS_IMETHOD GetValueString(nsAString& aValue) { return NS_ERROR_NOT_IMPLEMENTED; }

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable, 
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable, 
                                    nsISVGValue::modificationType aModType);

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext();
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgGradientFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  // nsISVGChildFrame interface:
  // Override PaintSVG (our frames don't directly render)
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas) {return NS_OK;}
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGradient"), aResult);
  }
#endif // DEBUG

protected:
  virtual ~nsSVGGradientFrame();

  // Internal methods for handling referenced gradients
  PRBool checkURITarget(nsIAtom *);
  PRBool checkURITarget();

  nsCOMPtr<nsIDOMSVGAnimatedString> 	 mHref;

  nsSVGGradientFrame                    *mNextGrad;
  nsCOMPtr<nsIContent>                   mSourceContent;
  PRPackedBool                           mLoopFlag;
  
private:
  PRInt32 GetStopElement(PRInt32 aIndex, 
                         nsIDOMSVGStopElement * *aStopElement,
                         nsIFrame * *aStopFrame);

};

// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGLinearGradientFrameBase;

class nsSVGLinearGradientFrame : public nsSVGLinearGradientFrameBase,
                                 public nsISVGLinearGradient
{
public:
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGLinearGradient interface:
  NS_DECL_NSISVGLINEARGRADIENT

  // nsISVGGradient interface gets inherited from nsSVGGradientFrame

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgLinearGradientFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGLinearGradient"), aResult);
  }
#endif // DEBUG

};

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGRadialGradientFrameBase;

class nsSVGRadialGradientFrame : public nsSVGRadialGradientFrameBase,
                                 public nsISVGRadialGradient
{
public:
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGRadialGradient interface:
  NS_DECL_NSISVGRADIALGRADIENT

  // nsISVGGradient interface gets inherited from nsSVGGradientFrame

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgLinearGradientFrame
   */
  virtual nsIAtom* GetType() const;

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGRadialGradient"), aResult);
  }
#endif // DEBUG
};

//----------------------------------------------------------------------
// Implementation

nsSVGGradientFrame::~nsSVGGradientFrame()
{
  WillModify();
  // Notify the world that we're dying
  DidModify(mod_die);

  if (mNextGrad) 
    mNextGrad->RemoveObserver(this);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_INTERFACE_MAP_BEGIN(nsSVGGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGGradient)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGradientFrameBase)

//----------------------------------------------------------------------
// nsISVGValueObserver methods:
NS_IMETHODIMP
nsSVGGradientFrame::WillModifySVGObservable(nsISVGValue* observable,
                                            modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}
                                                                                
NS_IMETHODIMP
nsSVGGradientFrame::DidModifySVGObservable(nsISVGValue* observable, 
                                           nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsISVGGradient> gradient = do_QueryInterface(observable);
  // Is this a gradient we are observing that is going away?
  if (mNextGrad && aModType == nsISVGValue::mod_die && gradient) {
    // Yes, we need to handle this differently
    nsISVGGradient *grad;
    CallQueryInterface(mNextGrad, &grad);
    if (grad == gradient) {
      mNextGrad = nsnull;
    }
  }
  // Something we depend on was modified -- pass it on!
  DidModify(aModType);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGGradientFrame::DidSetStyleContext()
{
  WillModify();
  DidModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::RemoveFrame(nsIAtom*        aListName,
                                nsIFrame*       aOldFrame)
{
  WillModify();
  PRBool result = mFrames.DestroyFrame(GetPresContext(), aOldFrame);
  DidModify();
  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsIAtom*
nsSVGGradientFrame::GetType() const
{
  return nsLayoutAtoms::svgGradientFrame;
}

NS_IMETHODIMP
nsSVGGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::gradientUnits ||
       aAttribute == nsGkAtoms::gradientTransform ||
       aAttribute == nsGkAtoms::spreadMethod)) {
    WillModify();
    DidModify();
    return NS_OK;
  } 

  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    if (mNextGrad)
      mNextGrad->RemoveObserver(this);
    mNextGrad = nsnull;
    WillModify();
    DidModify();
    return NS_OK;
  }

  return nsSVGGradientFrameBase::AttributeChanged(aNameSpaceID,
                                                  aAttribute, aModType);
}

//----------------------------------------------------------------------
//  nsISVGGradient implementation
//----------------------------------------------------------------------

NS_IMETHODIMP
nsSVGGradientFrame::GetStopCount(PRUint32 *aStopCount)
{
  nsresult rv = NS_OK;
  nsIDOMSVGStopElement *stopElement = nsnull;
  *aStopCount = GetStopElement(-1, &stopElement, nsnull);
  if (*aStopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      rv = mNextGrad->GetStopCount(aStopCount);
    else
      rv = NS_ERROR_FAILURE;
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOffset(PRInt32 aIndex, float *aOffset)
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsresult rv = NS_OK;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, nsnull);

  if (stopElement) {
    nsCOMPtr<nsIDOMSVGAnimatedNumber> aNum;
    stopElement->GetOffset(getter_AddRefs(aNum));
    aNum->GetAnimVal(aOffset);
    if (*aOffset < 0.0f)
      *aOffset = 0.0f;
    if (*aOffset > 1.0f)
      *aOffset = 1.0f;

    return NS_OK;
  }

  // if our gradient doesn't have its own stops we must check if it references
  // another gradient in which case we must attempt to "inherit" its stops
  if (stopCount == 0 && checkURITarget()) {
    rv = mNextGrad->GetStopOffset(aIndex, aOffset);
  }
  else {
    *aOffset = 0.0f;
    rv = NS_ERROR_FAILURE;
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopColor(PRInt32 aIndex, nscolor *aStopColor) 
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsIFrame *stopFrame = nsnull;
  nsresult rv = NS_OK;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, &stopFrame);

  if (stopElement) {
    if (!stopFrame) {
      NS_ERROR("No stop frame found!");
      *aStopColor = 0;
      return NS_ERROR_FAILURE;
    }
    *aStopColor = stopFrame->GetStyleSVGReset()->mStopColor.mPaint.mColor;

    return NS_OK;
  }

  // if our gradient doesn't have its own stops we must check if it references
  // another gradient in which case we must attempt to "inherit" its stops
  if (stopCount == 0 && checkURITarget()) {
    rv = mNextGrad->GetStopColor(aIndex, aStopColor);
  }
  else {
    *aStopColor = 0;
    rv = NS_ERROR_FAILURE;
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOpacity(PRInt32 aIndex, float *aStopOpacity) 
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsIFrame *stopFrame = nsnull;
  nsresult rv = NS_OK;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, &stopFrame);

  if (stopElement) {
    if (!stopFrame) {
      NS_ERROR("No stop frame found!");
      *aStopOpacity = 1.0f;
      return NS_ERROR_FAILURE;
    }
    *aStopOpacity = stopFrame->GetStyleSVGReset()->mStopOpacity;

    return NS_OK;
  }

  // if our gradient doesn't have its own stops we must check if it references
  // another gradient in which case we must attempt to "inherit" its stops
  if (stopCount == 0 && checkURITarget()) {
    rv = mNextGrad->GetStopOpacity(aIndex, aStopOpacity);
  }
  else {
    *aStopOpacity = 0;
    rv = NS_ERROR_FAILURE;
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientType(PRUint32 *aType)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGradElement = do_QueryInterface(mContent);
  if (lGradElement) {
    *aType = nsISVGGradient::SVG_LINEAR_GRADIENT;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  if (rGradElement) {
    *aType = nsISVGGradient::SVG_RADIAL_GRADIENT;
    return NS_OK;
  }
  *aType = nsISVGGradient::SVG_UNKNOWN_GRADIENT;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientUnits(PRUint16 *aUnits)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(mContent);
  NS_ASSERTION(gradElement, "Wrong content element (not gradient)");
  if (!gradElement) {
    return NS_ERROR_FAILURE;
  }

  // See if we need to get the value from another gradient
  if (!checkURITarget(nsGkAtoms::gradientUnits)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedEnumeration> units;
    gradElement->GetGradientUnits(getter_AddRefs(units));
    units->GetAnimVal(aUnits);
  } else {
    // Yes, get it from the target
    rv = mNextGrad->GetGradientUnits(aUnits);
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientTransform(nsIDOMSVGMatrix **aGradientTransform,
                                         nsISVGGeometrySource *aSource)
{
  *aGradientTransform = nsnull;
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> animTrans;
  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(mContent);
  NS_ASSERTION(gradElement, "Wrong content element (not gradient)");
  if (!gradElement) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMSVGMatrix> bboxTransform;
  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    nsISVGChildFrame *frame = nsnull;
    if (aSource)
      CallQueryInterface(aSource, &frame);
    nsCOMPtr<nsIDOMSVGRect> rect;
    if (frame) {
      frame->SetMatrixPropagation(PR_FALSE);
      frame->NotifyCanvasTMChanged(PR_TRUE);
      frame->GetBBox(getter_AddRefs(rect));
      frame->SetMatrixPropagation(PR_TRUE);
      frame->NotifyCanvasTMChanged(PR_TRUE);
    }
    if (rect) {
      float x, y, width, height;
      rect->GetX(&x);
      rect->GetY(&y);
      rect->GetWidth(&width);
      rect->GetHeight(&height);
      NS_NewSVGMatrix(getter_AddRefs(bboxTransform),
                      width, 0, 0, height, x, y);
    }
  } else {
    nsIFrame *frame = nsnull;
    CallQueryInterface(aSource, &frame);

    // If this gradient is applied to text, our caller
    // will be the glyph, which is not a container, so we
    // need to get the parent
    nsIAtom *callerType = frame->GetType();
    if (callerType ==  nsLayoutAtoms::svgGlyphFrame)
      mSourceContent = frame->GetContent()->GetParent();
    else
      mSourceContent = frame->GetContent();
    NS_ASSERTION(mSourceContent, "Can't get content for gradient");
  }

  if (!bboxTransform)
    NS_NewSVGMatrix(getter_AddRefs(bboxTransform));

  nsCOMPtr<nsIDOMSVGMatrix> gradientTransform;
  // See if we need to get the value from another gradient
  if (!checkURITarget(nsGkAtoms::gradientTransform)) {
    // No, return the values
    gradElement->GetGradientTransform(getter_AddRefs(animTrans));
    nsCOMPtr<nsIDOMSVGTransformList> lTrans;
    animTrans->GetAnimVal(getter_AddRefs(lTrans));
    lTrans->GetConsolidationMatrix(getter_AddRefs(gradientTransform));
  } else {
    // Yes, get it from the target
    mNextGrad->GetGradientTransform(getter_AddRefs(gradientTransform), nsnull);
  }

  bboxTransform->Multiply(gradientTransform, aGradientTransform);
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetSpreadMethod(PRUint16 *aSpreadMethod)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(mContent);
  NS_ASSERTION(gradElement, "Wrong content element (not gradient)");
  if (!gradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (!checkURITarget(nsGkAtoms::spreadMethod)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedEnumeration> method;
    gradElement->GetSpreadMethod(getter_AddRefs(method));
    method->GetAnimVal(aSpreadMethod);
  } else {
    // Yes, get it from the target
    rv = mNextGrad->GetSpreadMethod(aSpreadMethod);
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetNextGradient(nsISVGGradient * *aNextGrad, PRUint32 aType) {
  PRUint32 nextType;
  if (!mNextGrad) {
    *aNextGrad = nsnull;
    return NS_ERROR_FAILURE;
  }
  mNextGrad->GetGradientType(&nextType);
  if (nextType == aType) {
    *aNextGrad = mNextGrad;
    return NS_OK;
  } else {
    return mNextGrad->GetNextGradient(aNextGrad, aType);
  }
}

// Private (helper) methods
PRBool 
nsSVGGradientFrame::checkURITarget(nsIAtom *attr) {
  // Was the attribute explicitly set?
  if (mContent->HasAttr(kNameSpaceID_None, attr)) {
    // Yes, just return
    return PR_FALSE;
  }
  return checkURITarget();
}

PRBool
nsSVGGradientFrame::checkURITarget(void) {
  mLoopFlag = PR_TRUE; // Set our loop detection flag
  // Have we already figured out the next Gradient?
  if (mNextGrad != nsnull) {
    return PR_TRUE;
  }

  // check if we reference another gradient to "inherit" 
  // its stops or attributes
  nsAutoString href;
  mHref->GetAnimVal(href);
  // Do we have URI?
  if (href.IsEmpty()) {
    return PR_FALSE; // No, return the default
  }

  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI),
    href, mContent->GetCurrentDoc(), base);

  // Note that we are using *our* frame tree for this call, otherwise we're going to have
  // to get the PresShell in each call
  nsIFrame *nextGrad;
  if (NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&nextGrad, targetURI, mContent, GetPresContext()->PresShell()))) {
    nsIAtom* frameType = nextGrad->GetType();
    if ((frameType != nsLayoutAtoms::svgLinearGradientFrame) && 
        (frameType != nsLayoutAtoms::svgRadialGradientFrame))
      return PR_FALSE;

    mNextGrad = (nsSVGGradientFrame *)nextGrad;
    if (mNextGrad->mLoopFlag) {
      // Yes, remove the reference and return an error
      NS_WARNING("Gradient loop detected!");
      mNextGrad = nsnull;
      return PR_FALSE;
    }
    // Add ourselves to the observer list
    if (mNextGrad) {
      // Can't use the NS_ADD macro here because of nsISupports ambiguity
      mNextGrad->AddObserver(this);
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

// -------------------------------------------------------------------------
// Private helper method to simplify getting stop elements
// returns non-addrefed stop element
// -------------------------------------------------------------------------
PRInt32 
nsSVGGradientFrame::GetStopElement(PRInt32 aIndex, nsIDOMSVGStopElement * *aStopElement,
                                   nsIFrame * *aStopFrame)
{
  PRInt32 stopCount = 0;
  nsIFrame *stopFrame;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    nsCOMPtr<nsIDOMSVGStopElement>stopElement = do_QueryInterface(stopFrame->GetContent());
    if (stopElement) {
      // Is this the one we're looking for?
      if (stopCount++ == aIndex) {
        *aStopElement = stopElement;
        break; // Yes, break out of the loop
      }
    }
  }
  if (aStopFrame)
    *aStopFrame = stopFrame;
  return stopCount;
}

// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

nsIAtom*
nsSVGLinearGradientFrame::GetType() const
{
  return nsLayoutAtoms::svgLinearGradientFrame;
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x1 ||
       aAttribute == nsGkAtoms::y1 ||
       aAttribute == nsGkAtoms::x2 ||
       aAttribute == nsGkAtoms::y2)) {
    WillModify();
    DidModify();
    return NS_OK;
  }

  return nsSVGGradientFrame::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGLinearGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGLinearGradient)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLinearGradientFrameBase)

// nsISVGLinearGradient
NS_IMETHODIMP
nsSVGLinearGradientFrame::GetX1(float *aX1)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(mContent);
  NS_ASSERTION(lGrad, "Wrong content element (not linear gradient)");
  if (!lGrad) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::x1)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsSVGLinearGradientFrame *lNgrad =
        (nsSVGLinearGradientFrame *)nextGrad;
      lNgrad->GetX1(aX1);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetX1(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aX1);
  } else {
    *aX1 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}
  
nsresult
nsSVGLinearGradientFrame::GetY1(float *aY1)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(mContent);
  NS_ASSERTION(lGrad, "Wrong content element (not linear gradient)");
  if (!lGrad) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::y1)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsSVGLinearGradientFrame *lNgrad =
        (nsSVGLinearGradientFrame *)nextGrad;
      lNgrad->GetY1(aY1);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetY1(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aY1);
  } else {
    *aY1 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::GetX2(float *aX2)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(mContent);
  NS_ASSERTION(lGrad, "Wrong content element (not linear gradient)");
  if (!lGrad) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::x2)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsSVGLinearGradientFrame *lNgrad =
        (nsSVGLinearGradientFrame *)nextGrad;
      lNgrad->GetX2(aX2);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetX2(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aX2);
  } else {
    *aX2 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::GetY2(float *aY2)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(mContent);
  NS_ASSERTION(lGrad, "Wrong content element (not linear gradient)");
  if (!lGrad) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::y2)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsSVGLinearGradientFrame *lNgrad =
        (nsSVGLinearGradientFrame *)nextGrad;
      lNgrad->GetY2(aY2);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetY2(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aY2);
  } else {
    *aY2 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

// -------------------------------------------------------------------------
// Radial Gradients
// -------------------------------------------------------------------------

nsIAtom*
nsSVGRadialGradientFrame::GetType() const
{
  return nsLayoutAtoms::svgRadialGradientFrame;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::r ||
       aAttribute == nsGkAtoms::cx ||
       aAttribute == nsGkAtoms::cy ||
       aAttribute == nsGkAtoms::fx ||
       aAttribute == nsGkAtoms::fy)) {
    WillModify();
    DidModify();
    return NS_OK;
  }

  return nsSVGGradientFrame::AttributeChanged(aNameSpaceID,
                                              aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGRadialGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGRadialGradient)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRadialGradientFrameBase)

// nsISVGRadialGradient
NS_IMETHODIMP
nsSVGRadialGradientFrame::GetFx(float *aFx)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  NS_ASSERTION(rGradElement, "Wrong content element (not linear gradient)");
  if (!rGradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::fx)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsSVGRadialGradientFrame *rNgrad =
        (nsSVGRadialGradientFrame *)nextGrad;
      rNgrad->GetFx(aFx);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // See if the value was explicitly set --  the spec
  // states that if there is no explicit fx value, we return the cx value
  // see http://www.w3.org/TR/SVG11/pservers.html#RadialGradients
  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::fx))
    return GetCx(aFx);

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetFx(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aFx);
  } else {
    *aFx = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetFy(float *aFy)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  NS_ASSERTION(rGradElement, "Wrong content element (not linear gradient)");
  if (!rGradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::fy)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsSVGRadialGradientFrame *rNgrad =
        (nsSVGRadialGradientFrame *)nextGrad;
      rNgrad->GetFy(aFy);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // See if the value was explicitly set --  the spec
  // states that if there is no explicit fx value, we return the cx value
  // see http://www.w3.org/TR/SVG11/pservers.html#RadialGradients
  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::fy))
    return GetCy(aFy);

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetFy(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aFy);
  } else {
    *aFy = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetCx(float *aCx)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  NS_ASSERTION(rGradElement, "Wrong content element (not linear gradient)");
  if (!rGradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::cx)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsSVGRadialGradientFrame *rNgrad =
        (nsSVGRadialGradientFrame *)nextGrad;
      rNgrad->GetCx(aCx);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }

  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetCx(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aCx);
  } else {
    *aCx = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetCy(float *aCy)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  NS_ASSERTION(rGradElement, "Wrong content element (not linear gradient)");
  if (!rGradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::cy)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsSVGRadialGradientFrame *rNgrad =
        (nsSVGRadialGradientFrame *)nextGrad;
      rNgrad->GetCy(aCy);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetCy(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aCy);
  } else {
    *aCy = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetR(float *aR)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(mContent);
  NS_ASSERTION(rGradElement, "Wrong content element (not linear gradient)");
  if (!rGradElement) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(nsGkAtoms::r)) {
    // Yes, get it from the target
    nsISVGGradient *nextGrad;
    if (GetNextGradient(&nextGrad,
                        nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsSVGRadialGradientFrame *rNgrad =
        (nsSVGRadialGradientFrame *)nextGrad;
      rNgrad->GetR(aR);
      mLoopFlag = PR_FALSE;
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetR(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  PRUint16 bbox;
  GetGradientUnits(&bbox);
  if (bbox == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX) {
    length->GetValue(aR);
  } else {
    *aR = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::XY);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* 
NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                             nsIContent*   aContent)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGLinearGradient");
  if (!grad)
    return nsnull;
  
  nsSVGLinearGradientFrame* it = new (aPresShell) nsSVGLinearGradientFrame;
  if (nsnull == it)
    return nsnull;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (aRef) {
    // Get the hRef
    aRef->GetHref(getter_AddRefs(it->mHref));
  }
  it->mNextGrad = nsnull;
  it->mLoopFlag = PR_FALSE;
  return it;
}

nsIFrame*
NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                             nsIContent*   aContent)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGRadialGradient");
  if (!grad)
    return nsnull;
  
  nsSVGRadialGradientFrame* it = new (aPresShell) nsSVGRadialGradientFrame;
  if (nsnull == it)
    return nsnull;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (aRef) {
    // Get the hRef
    aRef->GetHref(getter_AddRefs(it->mHref));
  }
  it->mNextGrad = nsnull;
  it->mLoopFlag = PR_FALSE;
  return it;
}

// Public function to locate the SVGGradientFrame structure pointed to by a URI
// and return a nsISVGGradient
nsresult NS_GetSVGGradient(nsISVGGradient **aGrad, nsIURI *aURI, nsIContent *aContent, 
                           nsIPresShell *aPresShell)
{
  *aGrad = nsnull;

#ifdef DEBUG_scooter
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);
  printf("NS_GetSVGGradient: uri = %s\n",uriSpec.get());
#endif
  nsIFrame *result;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&result, aURI, aContent, aPresShell))) {
    return NS_ERROR_FAILURE;
  }
  return result->QueryInterface(NS_GET_IID(nsISVGGradient), (void **)aGrad);
}
