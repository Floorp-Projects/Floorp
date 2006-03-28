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
 *   Jonathan Watt <jwatt@jwatt.org>
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

class nsSVGLinearGradientFrame;
class nsSVGRadialGradientFrame;

typedef nsSVGDefsFrame  nsSVGGradientFrameBase;

class nsSVGGradientFrame : public nsSVGGradientFrameBase,
                           public nsSVGValue,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference,
                           public nsISVGGradient
{
public:
  friend nsIFrame* NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent,
                                                nsStyleContext* aContext);

  friend nsIFrame* NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                                                nsIContent*   aContent,
                                                nsStyleContext* aContext);

  friend nsIFrame* NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                                      nsIContent*   aContent, 
                                      nsIFrame*     aParentFrame,
                                      nsStyleContext* aContext);

  friend nsresult NS_GetSVGGradientFrame(nsIFrame**      result, 
                                         nsIURI*         aURI, 
                                         nsIContent*     aContent,
                                         nsIPresShell*   aPresShell);

  nsSVGGradientFrame(nsStyleContext* aContext) :
    nsSVGGradientFrameBase(aContext) {}

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

  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGGradient"), aResult);
  }
#endif // DEBUG

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas)
  {
    return NS_OK;  // override - our frames don't directly render
  }
  
private:

  // Helper methods to aid gradient implementation
  // ---------------------------------------------
  // The SVG specification allows gradient elements to reference another
  // gradient element to "inherit" its attributes or gradient stops. Reference
  // chains of arbitrary length are allowed, and loop checking is essential!
  // Use the following helpers to safely get attributes and stops.

  // Parse our xlink:href and set mNextGrad if we reference another gradient.
  void GetRefedGradientFromHref();

  // Helpers to look at our gradient and then along its reference chain (if any)
  // to find the first gradient with the specified attribute.
  nsIContent* GetGradientWithAttr(nsIAtom *aAttrName);

  // Some attributes are only valid on one type of gradient, and we *must* get
  // the right type or we won't have the data structures we require.
  nsIContent* GetGradientWithAttr(nsIAtom *aAttrName, nsIAtom *aGradType);

  // Get a stop element and (optionally) its frame (returns stop index/count)
  PRInt32 GetStopElement(PRInt32 aIndex, 
                         nsIDOMSVGStopElement * *aStopElement,
                         nsIFrame * *aStopFrame);

protected:

  // Use these inline methods instead of GetGradientWithAttr(..., aGradType)
  nsIContent* GetLinearGradientWithAttr(nsIAtom *aAttrName)
  {
    return GetGradientWithAttr(aAttrName, nsGkAtoms::svgLinearGradientFrame);
  }
  nsIContent* GetRadialGradientWithAttr(nsIAtom *aAttrName)
  {
    return GetGradientWithAttr(aAttrName, nsGkAtoms::svgRadialGradientFrame);
  }

  // We must loop check notifications too: see bug 330387 comment 18 + testcase
  // and comment 19. The mLoopFlag check is in Will/DidModifySVGObservable.
  void WillModify(modificationType aModType = mod_other)
  {
    mLoopFlag = PR_TRUE;
    nsSVGValue::WillModify(aModType);
    mLoopFlag = PR_FALSE;
  }
  void DidModify(modificationType aModType = mod_other)
  {
    mLoopFlag = PR_TRUE;
    nsSVGValue::DidModify(aModType);
    mLoopFlag = PR_FALSE;
  }

  // Get the value of our gradientUnits attribute
  PRUint16 GetGradientUnits();

  virtual ~nsSVGGradientFrame();

  // The graphic element our gradient is (currently) being applied to
  nsCOMPtr<nsIContent>                   mSourceContent;

private:

  // href of the other gradient we reference (if any)
  nsCOMPtr<nsIDOMSVGAnimatedString>      mHref;

  // Frame of the gradient we reference (if any). Do NOT use this directly.
  // Use Get[Xxx]GradientWithAttr instead to ensure proper loop checking.
  nsSVGGradientFrame                    *mNextGrad;

  // Flag to mark this frame as "in use" during recursive calls along our
  // gradient's reference chain so we can detect reference loops. See:
  // http://www.w3.org/TR/SVG11/pservers.html#LinearGradientElementHrefAttribute
  PRPackedBool                           mLoopFlag;

  // Ideally we'd set mNextGrad by implementing Init(), but the frame of the
  // gradient we reference isn't available at that stage. Our only option is to
  // set mNextGrad lazily in GetGradientWithAttr, and to make that efficient
  // we need this flag. Our class size is the same since it just fills padding.
  PRPackedBool                           mInitialized;
};


// -------------------------------------------------------------------------
// Linear Gradients
// -------------------------------------------------------------------------

typedef nsSVGGradientFrame nsSVGLinearGradientFrameBase;

class nsSVGLinearGradientFrame : public nsSVGLinearGradientFrameBase,
                                 public nsISVGLinearGradient
{
public:
  nsSVGLinearGradientFrame(nsStyleContext* aContext) :
    nsSVGLinearGradientFrameBase(aContext) {}

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGLinearGradient interface:
  NS_DECL_NSISVGLINEARGRADIENT

  // nsISVGGradient interface gets inherited from nsSVGGradientFrame

  // nsIFrame interface:
  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgLinearGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
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
  nsSVGRadialGradientFrame(nsStyleContext* aContext) :
    nsSVGRadialGradientFrameBase(aContext) {}

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGRadialGradient interface:
  NS_DECL_NSISVGRADIALGRADIENT

  // nsISVGGradient interface gets inherited from nsSVGGradientFrame

  // nsIFrame interface:
  virtual nsIAtom* GetType() const;  // frame type: nsGkAtoms::svgRadialGradientFrame

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  // nsIFrameDebug interface:
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
  WillModify(mod_die);
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
  // return if we have an mObservers loop
  if (mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("gradient reference loop detected while notifying observers!");
    return NS_OK;
  }

  // Don't pass on mod_die - our gradient observers would stop observing us!
  if (aModType == mod_die)
    aModType = mod_other;

  WillModify(aModType);
  return NS_OK;
}
                                                                                
NS_IMETHODIMP
nsSVGGradientFrame::DidModifySVGObservable(nsISVGValue* observable, 
                                           nsISVGValue::modificationType aModType)
{
  // return if we have an mObservers loop
  if (mLoopFlag) {
    // XXXjwatt: we should really send an error to the JavaScript Console here:
    NS_WARNING("gradient reference loop detected while notifying observers!");
    return NS_OK;
  }

  // If we reference another gradient and it's going away, null out mNextGrad
  nsCOMPtr<nsISVGGradient> gradient;
  if (mNextGrad && aModType == nsISVGValue::mod_die &&
      (gradient = do_QueryInterface(observable))) {
    nsISVGGradient *grad;
    CallQueryInterface(mNextGrad, &grad);
    if (grad == gradient) {
      mNextGrad = nsnull;
    }
  }

  // Don't pass on mod_die - our gradient observers would stop observing us!
  if (aModType == mod_die)
    aModType = mod_other;

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
    WillModify();
    GetRefedGradientFromHref();
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
  nsIDOMSVGStopElement *stopElement = nsnull;
  *aStopCount = GetStopElement(-1, &stopElement, nsnull);
  return NS_OK;  // no stops is valid - paint as for 'none'
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOffset(PRInt32 aIndex, float *aOffset)
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, nsnull);

  if (stopElement) {
    nsCOMPtr<nsIDOMSVGAnimatedNumber> aNum;
    stopElement->GetOffset(getter_AddRefs(aNum));
    aNum->GetAnimVal(aOffset);
    if (*aOffset < 0.0f)
      *aOffset = 0.0f;
    else if (*aOffset > 1.0f)
      *aOffset = 1.0f;

    return NS_OK;
  }

  NS_ASSERTION(stopCount == 0, "Don't call me with an invalid stop index!");
  *aOffset = 0.0f;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopColor(PRInt32 aIndex, nscolor *aStopColor) 
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsIFrame *stopFrame = nsnull;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, &stopFrame);
  if (stopFrame) {
    *aStopColor = stopFrame->GetStyleSVGReset()->mStopColor.mPaint.mColor;
    return NS_OK;
  }

  // One way or another we have an implementation problem if we get here
#ifdef DEBUG
  if (stopElement) {
    NS_WARNING("We *do* have a stop but can't use it because it doesn't have "
               "a frame - we need frame free gradients and stops!");
  }
  else {
    NS_ERROR("Don't call me with an invalid stop index!");
  }
#endif

  *aStopColor = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOpacity(PRInt32 aIndex, float *aStopOpacity) 
{
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsIFrame *stopFrame = nsnull;
  PRInt32 stopCount = GetStopElement(aIndex, &stopElement, &stopFrame);
  if (stopFrame) {
    *aStopOpacity = stopFrame->GetStyleSVGReset()->mStopOpacity;
    return NS_OK;
  }

  // One way or another we have an implementation problem if we get here
#ifdef DEBUG
  if (stopElement) {
    NS_WARNING("We *do* have a stop but can't use it because it doesn't have "
               "a frame - we need frame free gradients and stops!");
  }
  else {
    NS_ERROR("Don't call me with an invalid stop index!");
  }
#endif

  *aStopOpacity = 1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientType(PRUint32 *aType)
{
  if (GetType() == nsGkAtoms::svgLinearGradientFrame) {
    *aType = nsISVGGradient::SVG_LINEAR_GRADIENT;
    return NS_OK;
  }
  if (GetType() == nsGkAtoms::svgRadialGradientFrame) {
    *aType = nsISVGGradient::SVG_RADIAL_GRADIENT;
    return NS_OK;
  }
  NS_NOTREACHED("Unknown gradient type!");
  *aType = nsISVGGradient::SVG_UNKNOWN_GRADIENT;
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientTransform(nsIDOMSVGMatrix **aGradientTransform,
                                         nsISVGGeometrySource *aSource)
{
  nsCOMPtr<nsIDOMSVGMatrix> bboxTransform;
  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    nsIFrame *frame = nsnull;
    CallQueryInterface(aSource, &frame);

    // If this gradient is applied to text, our caller
    // will be the glyph, which is not a container, so we
    // need to get the parent
    nsIAtom *callerType = frame->GetType();
    if (callerType ==  nsGkAtoms::svgGlyphFrame)
      mSourceContent = frame->GetContent()->GetParent();
    else
      mSourceContent = frame->GetContent();
    NS_ASSERTION(mSourceContent, "Can't get content for gradient");
  }
  else {
    NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
                 "Unknown gradientUnits type");
    // objectBoundingBox is the default anyway

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
  }

  if (!bboxTransform)
    NS_NewSVGMatrix(getter_AddRefs(bboxTransform));

  nsIContent *gradient = GetGradientWithAttr(nsGkAtoms::gradientTransform);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> animTrans;
  gradElement->GetGradientTransform(getter_AddRefs(animTrans));
  nsCOMPtr<nsIDOMSVGTransformList> trans;
  animTrans->GetAnimVal(getter_AddRefs(trans));
  nsCOMPtr<nsIDOMSVGMatrix> gradientTransform;
  trans->GetConsolidationMatrix(getter_AddRefs(gradientTransform));

  return bboxTransform->Multiply(gradientTransform, aGradientTransform);
}

NS_IMETHODIMP
nsSVGGradientFrame::GetSpreadMethod(PRUint16 *aSpreadMethod)
{
  nsIContent *gradient = GetGradientWithAttr(nsGkAtoms::spreadMethod);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> method;
  gradElement->GetSpreadMethod(getter_AddRefs(method));
  return method->GetAnimVal(aSpreadMethod);
}


// Private (helper) methods

void
nsSVGGradientFrame::GetRefedGradientFromHref()
{
  mNextGrad = nsnull;
  mInitialized = PR_TRUE;

  // Fetch our gradient element's xlink:href attribute
  nsAutoString href;
  mHref->GetAnimVal(href);
  if (href.IsEmpty()) {
    return; // no URL
  }

  // Convert href to an nsIURI
  nsCOMPtr<nsIURI> targetURI;
  nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
  nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                            mContent->GetCurrentDoc(), base);

  // Fetch and store a pointer to the referenced gradient element's frame.
  // Note that we are using *our* frame tree for this call, otherwise we're
  // going to have to get the PresShell in each call
  nsIFrame *nextGrad;
  if (NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&nextGrad, targetURI, mContent,
                                                  GetPresContext()->PresShell()))) {
    nsIAtom* frameType = nextGrad->GetType();
    if (frameType != nsGkAtoms::svgLinearGradientFrame && 
        frameType != nsGkAtoms::svgRadialGradientFrame)
      return;

    mNextGrad = NS_REINTERPRET_CAST(nsSVGGradientFrame*, nextGrad);

    // Add ourselves to the observer list
    if (mNextGrad) {
      // Can't use the NS_ADD macro here because of nsISupports ambiguity
      mNextGrad->AddObserver(this);
    }
  }
}

// This is implemented to return nsnull if the attribute is not set so that
// GetFx and GetFy can use the values of cx and cy instead of the defaults.
nsIContent*
nsSVGGradientFrame::GetGradientWithAttr(nsIAtom *aAttrName)
{
  if (mContent->HasAttr(kNameSpaceID_None, aAttrName))
    return mContent;

  if (!mInitialized)  // make sure mNextGrad has been initialized
    GetRefedGradientFromHref();

  if (!mNextGrad)
    return nsnull;

  nsIContent *grad = nsnull;

  // Set mLoopFlag before checking mNextGrad->mLoopFlag in case we are mNextGrad
  mLoopFlag = PR_TRUE;
  // XXXjwatt: we should really send an error to the JavaScript Console here:
  NS_WARN_IF_FALSE(!mNextGrad->mLoopFlag, "gradient reference loop detected "
                                          "while inheriting attribute!");
  if (!mNextGrad->mLoopFlag)
    grad = mNextGrad->GetGradientWithAttr(aAttrName);
  mLoopFlag = PR_FALSE;

  return grad;
}

nsIContent*
nsSVGGradientFrame::GetGradientWithAttr(nsIAtom *aAttrName, nsIAtom *aGradType)
{
  if (GetType() == aGradType && mContent->HasAttr(kNameSpaceID_None, aAttrName))
    return mContent;

  if (!mInitialized)
    GetRefedGradientFromHref();  // make sure mNextGrad has been initialized

  if (!mNextGrad)
    return nsnull;

  nsIContent *grad = nsnull;

  // Set mLoopFlag before checking mNextGrad->mLoopFlag in case we are mNextGrad
  mLoopFlag = PR_TRUE;
  // XXXjwatt: we should really send an error to the JavaScript Console here:
  NS_WARN_IF_FALSE(!mNextGrad->mLoopFlag, "gradient reference loop detected "
                                          "while inheriting attribute!");
  if (!mNextGrad->mLoopFlag)
    grad = mNextGrad->GetGradientWithAttr(aAttrName, aGradType);
  mLoopFlag = PR_FALSE;

  return grad;
}

PRInt32 
nsSVGGradientFrame::GetStopElement(PRInt32 aIndex, nsIDOMSVGStopElement * *aStopElement,
                                   nsIFrame * *aStopFrame)
{
  PRInt32 stopCount = 0;
  nsIFrame *stopFrame = nsnull;
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
  if (stopCount > 0) {
    if (aStopFrame)
      *aStopFrame = stopFrame;
    return stopCount;
  }

  // Our gradient element doesn't have stops - try to "inherit" them

  if (!mInitialized)
    GetRefedGradientFromHref();  // make sure mNextGrad has been initialized

  if (!mNextGrad) {
    *aStopElement = nsnull;
    if (aStopFrame)
      *aStopFrame = nsnull;
    return 0;
  }

  // Set mLoopFlag before checking mNextGrad->mLoopFlag in case we are mNextGrad
  mLoopFlag = PR_TRUE;
  // XXXjwatt: we should really send an error to the JavaScript Console here:
  NS_WARN_IF_FALSE(!mNextGrad->mLoopFlag, "gradient reference loop detected "
                                          "while inheriting stop!");
  if (!mNextGrad->mLoopFlag)
    stopCount = mNextGrad->GetStopElement(aIndex, aStopElement, aStopFrame);
  mLoopFlag = PR_FALSE;

  return stopCount;
}

PRUint16
nsSVGGradientFrame::GetGradientUnits()
{
  // This getter is called every time the others are called - maybe cache it?

  nsIContent *gradient = GetGradientWithAttr(nsGkAtoms::gradientUnits);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGGradientElement> gradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> animUnits;
  gradElement->GetGradientUnits(getter_AddRefs(animUnits));
  PRUint16 units;
  animUnits->GetAnimVal(&units);
  return units;
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
  nsIContent *gradient = GetLinearGradientWithAttr(nsGkAtoms::x1);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetX1(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aX1 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aX1);  // objectBoundingBox is the default anyway
}

nsresult
nsSVGLinearGradientFrame::GetY1(float *aY1)
{
  nsIContent *gradient = GetLinearGradientWithAttr(nsGkAtoms::y1);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetY1(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aY1 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aY1);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::GetX2(float *aX2)
{
  nsIContent *gradient = GetLinearGradientWithAttr(nsGkAtoms::x2);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetX2(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aX2 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aX2);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGLinearGradientFrame::GetY2(float *aY2)
{
  nsIContent *gradient = GetLinearGradientWithAttr(nsGkAtoms::y2);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGrad = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  lGrad->GetY2(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aY2 = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aY2);  // objectBoundingBox is the default anyway
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
  nsIContent *gradient = GetRadialGradientWithAttr(nsGkAtoms::fx);
  if (!gradient)
    return GetCx(aFx);  // if fx isn't set, we must use cx

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetFx(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aFx = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aFx);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetFy(float *aFy)
{
  nsIContent *gradient = GetRadialGradientWithAttr(nsGkAtoms::fy);
  if (!gradient)
    return GetCy(aFy);  // if fy isn't set, we must use cy

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetFy(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aFy = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aFy);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetCx(float *aCx)
{
  nsIContent *gradient = GetRadialGradientWithAttr(nsGkAtoms::cx);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetCx(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aCx = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::X);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aCx);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetCy(float *aCy)
{
  nsIContent *gradient = GetRadialGradientWithAttr(nsGkAtoms::cy);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetCy(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aCy = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::Y);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aCy);  // objectBoundingBox is the default anyway
}

NS_IMETHODIMP
nsSVGRadialGradientFrame::GetR(float *aR)
{
  nsIContent *gradient = GetRadialGradientWithAttr(nsGkAtoms::r);
  if (!gradient)
    gradient = mContent;  // use our gradient to get the correct default value

  nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradElement = do_QueryInterface(gradient);
  nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
  rGradElement->GetR(getter_AddRefs(animLength));
  nsCOMPtr<nsIDOMSVGLength> length;
  animLength->GetAnimVal(getter_AddRefs(length));

  // Object bounding box units are handled by setting the appropriate
  // transform in GetGradientTransfrom, but we need to handle user
  // space units as part of the individual Get* routines.  Fixes 323669.

  PRUint16 gradientUnits = GetGradientUnits();
  if (gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE) {
    *aR = nsSVGUtils::UserSpace(mSourceContent, length, nsSVGUtils::XY);
    return NS_OK;
  }

  NS_ASSERTION(gradientUnits == nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX,
               "Unknown gradientUnits type");

  return length->GetValue(aR);  // objectBoundingBox is the default anyway
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* 
NS_NewSVGLinearGradientFrame(nsIPresShell*   aPresShell,
                             nsIContent*     aContent,
                             nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGLinearGradient");
  if (!grad)
    return nsnull;
  
  nsSVGLinearGradientFrame* it = new (aPresShell) nsSVGLinearGradientFrame(aContext);
  if (nsnull == it)
    return nsnull;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (aRef) {
    // Get the href
    aRef->GetHref(getter_AddRefs(it->mHref));
  }
  it->mNextGrad = nsnull;
  it->mLoopFlag = PR_FALSE;
  it->mInitialized = PR_FALSE;
  return it;
}

nsIFrame*
NS_NewSVGRadialGradientFrame(nsIPresShell*   aPresShell,
                             nsIContent*     aContent,
                             nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGRadialGradient");
  if (!grad)
    return nsnull;
  
  nsSVGRadialGradientFrame* it = new (aPresShell) nsSVGRadialGradientFrame(aContext);
  if (nsnull == it)
    return nsnull;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (aRef) {
    // Get the href
    aRef->GetHref(getter_AddRefs(it->mHref));
  }
  it->mNextGrad = nsnull;
  it->mLoopFlag = PR_FALSE;
  it->mInitialized = PR_FALSE;
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
