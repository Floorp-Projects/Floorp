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
 * Scooter Morris
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
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

#include "nsContainerFrame.h"
#include "nsSVGGradientFrame.h"
#include "nsWeakReference.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsStyleContext.h"
#include "nsNetUtil.h"

// class nsSVGLinearGradient;
// class nsSVGRadialGradient;

nsresult NS_NewSVGGenericContainerFrame(nsIPresShell *aPresShell, nsIContent *aContent, nsIFrame **aNewFrame);
static nsresult GetSVGGradient(nsISVGGradient **result, nsCAutoString& aSpec, 
                               nsIContent *aContent, nsIPresShell *aPresShell);

typedef nsContainerFrame nsSVGGradientFrameBase;

class nsSVGGradientFrame : public nsSVGGradientFrameBase,
                           public nsISVGGradient
{
protected:
  friend nsresult NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                                               nsIContent*   aContent, 
                                               nsIFrame**    aNewFrame);

  friend nsresult NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                                               nsIContent*   aContent, 
                                               nsIFrame**    aNewFrame);

  friend nsresult NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                                     nsIContent*   aContent, 
                                     nsIFrame*     aParentFrame, 
                                     nsIFrame**    aNewFrame);

  friend nsresult NS_GetSVGGradientFrame(nsIFrame**      result, 
                                         nsIURI*         aURI, 
                                         nsIContent*     aContent,
                                         nsIPresShell*   aPresShell);


public:
  // nsISupports interface:
  // NS_DECL_ISUPPORTS

   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGGradient interface:
  NS_DECL_NSISVGGRADIENT

  // Add destructor that releases all stop elements
  PRBool checkURITarget(const nsAString &);
  PRBool checkURITarget();


  nsISVGGradient                      *mNextGrad;
  nsAutoString                         mNextGradStr;
};

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGradient)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGradientFrameBase)

//----------------------------------------------------------------------
// Implementation

NS_IMETHODIMP
nsSVGGradientFrame::GetStopCount(PRUint32 *aStopCount)
{
  PRInt32 stopCount = 0;
  nsIDOMSVGStopElement *stopElement = nsnull;
  for (nsIFrame* stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    stopFrame->GetContent()->QueryInterface(NS_GET_IID(nsIDOMSVGStopElement),(void**)&stopElement);
    if (stopElement) stopCount++;
  }
  if (stopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      return mNextGrad->GetStopCount(aStopCount);
  }
  *aStopCount = stopCount;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOffset(PRInt32 aIndex, float *aOffset)
{
  PRInt32 stopCount = 0;
  nsIDOMSVGStopElement *stopElement = nsnull;
  for (nsIFrame* stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    stopFrame->GetContent()->QueryInterface(NS_GET_IID(nsIDOMSVGStopElement),(void**)&stopElement);
    if (stopElement) {
      // Is this the one we're looking for?
      if (stopCount == aIndex)
        break; // Yes, break out of the loop
      stopCount++;
    }
  }
  if (stopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      return mNextGrad->GetStopOffset(aIndex, aOffset);
  }

  if (!stopElement) {
    *aOffset = nsnull;
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIDOMSVGAnimatedNumber> aNum;
  stopElement->GetOffset(getter_AddRefs(aNum));
  aNum->GetBaseVal(aOffset);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopColorType(PRInt32 aIndex, PRUint16 *aStopColorType) {
  PRInt32 stopCount = 0;
  nsIDOMSVGStopElement *stopElement = nsnull;
  nsIFrame* stopFrame = nsnull;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    nsIDOMSVGStopElement *stopElement;
    stopFrame->GetContent()->QueryInterface(NS_GET_IID(nsIDOMSVGStopElement),(void**)&stopElement);
    if (stopElement) {
      // Is this the one we're looking for?
      if (stopCount == aIndex)
        break; // Yes, break out of the loop
      stopCount++;
    }
  }
  if (stopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      return mNextGrad->GetStopColorType(aIndex, aStopColorType);
  }
  if (!stopElement) {
    *aStopColorType = 0;
    return NS_ERROR_FAILURE;
  }

  *aStopColorType = ((const nsStyleSVG*) stopFrame->GetStyleContext()->GetStyleData(eStyleStruct_SVG))->mStopColor.mType;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopColor(PRInt32 aIndex, nscolor *aStopColor) {
  PRInt32 stopCount = 0;
  nsIFrame *stopFrame = nsnull;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    nsIDOMSVGStopElement *stopElement;
    stopFrame->GetContent()->QueryInterface(NS_GET_IID(nsIDOMSVGStopElement),(void**)&stopElement);
    if (stopElement) {
      // Is this the one we're looking for?
      if (stopCount == aIndex) {
        break; // Yes, break out of the loop
      }
      stopCount++;
    }
  }
  if (stopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      return mNextGrad->GetStopColor(aIndex, aStopColor);
  }
  NS_ASSERTION(stopFrame, "No stop frame found!");
  if (!stopFrame) {
    *aStopColor = 0;
    return NS_ERROR_FAILURE;
  }
  *aStopColor = ((const nsStyleSVG*) stopFrame->GetStyleContext()->GetStyleData(eStyleStruct_SVG))->mStopColor.mPaint.mColor;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetStopOpacity(PRInt32 aIndex, float *aStopOpacity) {
  PRInt32 stopCount = 0;
  nsIFrame *stopFrame = nsnull;
  for (stopFrame = mFrames.FirstChild(); stopFrame;
       stopFrame = stopFrame->GetNextSibling()) {
    nsIDOMSVGStopElement *stopElement;
    stopFrame->GetContent()->QueryInterface(NS_GET_IID(nsIDOMSVGStopElement),(void**)&stopElement);
    if (stopElement) {
      // Is this the one we're looking for?
      if (stopCount == aIndex)
        break; // Yes, break out of the loop
      stopCount++;
    }
  }
  if (stopCount == 0) {
    // No stops?  check for URI target
    if (checkURITarget())
      return mNextGrad->GetStopOpacity(aIndex, aStopOpacity);
  }
  if (!stopFrame) {
    *aStopOpacity = 1.0f;
    return NS_ERROR_FAILURE;
  }

  *aStopOpacity = ((const nsStyleSVG*) stopFrame->GetStyleContext()->GetStyleData(eStyleStruct_SVG))->mStopOpacity;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientType(PRUint32 *aType)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> aLe = do_QueryInterface(mContent);
  if (aLe) {
    *aType = nsISVGGradient::SVG_LINEAR_GRADIENT;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRe = do_QueryInterface(mContent);
  if (aRe) {
    *aType = nsISVGGradient::SVG_RADIAL_GRADIENT;
    return NS_OK;
  }
  *aType = nsISVGGradient::SVG_UNKNOWN_GRADIENT;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientUnits(PRUint16 *aUnits)
{
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> aEnum;
  nsCOMPtr<nsIDOMSVGGradientElement> aGrad = do_QueryInterface(mContent);
  NS_ASSERTION(aGrad, "Wrong content element (not gradient)");
  if (aGrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (!checkURITarget(NS_LITERAL_STRING("gradientUnits"))) {
    // No, return the values
    aGrad->GetGradientUnits(getter_AddRefs(aEnum));
    aEnum->GetBaseVal(aUnits);
  } else {
    // Yes, get it from the target
    mNextGrad->GetGradientUnits(aUnits);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetGradientTransform(nsIDOMSVGMatrix **aGradientTransform)
{
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> aTrans;
  nsCOMPtr<nsIDOMSVGGradientElement> aGrad = do_QueryInterface(mContent);
  NS_ASSERTION(aGrad, "Wrong content element (not gradient)");
  if (aGrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (!checkURITarget(NS_LITERAL_STRING("gradientTransform"))) {
    // No, return the values
    aGrad->GetGradientTransform(getter_AddRefs(aTrans));
    nsCOMPtr<nsIDOMSVGTransformList> lTrans;
    aTrans->GetBaseVal(getter_AddRefs(lTrans));
    lTrans->GetConsolidationMatrix(aGradientTransform);
  } else {
    // Yes, get it from the target
    mNextGrad->GetGradientTransform(aGradientTransform);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetSpreadMethod(PRUint16 *aSpreadMethod)
{
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> aEnum;
  nsCOMPtr<nsIDOMSVGGradientElement> aGrad = do_QueryInterface(mContent);
  NS_ASSERTION(aGrad, "Wrong content element (not gradient)");
  if (aGrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (!checkURITarget(NS_LITERAL_STRING("spreadMethod"))) {
    // No, return the values
    aGrad->GetSpreadMethod(getter_AddRefs(aEnum));
    aEnum->GetBaseVal(aSpreadMethod);
  } else {
    // Yes, get it from the target
    mNextGrad->GetSpreadMethod(aSpreadMethod);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGGradientFrame::GetNextGradient(nsISVGGradient * *aNextGrad, PRUint32 aType) {
  if (checkURITarget()) {
    // OK, we have a reference.  We need to find the next reference that points to
    // our type
    PRUint32 nextType;
    mNextGrad->GetGradientType(&nextType);
    if (nextType == aType) {
      *aNextGrad = mNextGrad;
      return NS_OK;
    } else {
      return mNextGrad->GetNextGradient(aNextGrad, aType);
    }
  } else {
    *aNextGrad = nsnull;
    return NS_ERROR_FAILURE;
  }
}

// Private (helper) methods
PRBool 
nsSVGGradientFrame::checkURITarget(const nsAString& attrStr) {
  nsCOMPtr<nsIDOMSVGGradientElement> aGrad = do_QueryInterface(mContent);

  // Was the attribute explicitly set?
  PRBool attr;
  aGrad->HasAttribute(attrStr, &attr);
  if (attr) {
    // Yes, just return
    return false;
  }
  return checkURITarget();
}

PRBool
nsSVGGradientFrame::checkURITarget(void) {
  // Have we already figured out the next Gradient?
  if (mNextGrad != nsnull) {
    return true;
  }

  // Do we have URI?
  if (mNextGradStr.Length() == 0) {
    return false; // No, return the default
  }

  // Get the Gradient
  nsCAutoString aGradStr;
  CopyUTF16toUTF8(mNextGradStr, aGradStr);
  // Note that we are using *our* frame tree for this call, otherwise we're going to have
  // to get the PresShell in each call
  if (GetSVGGradient(&mNextGrad, aGradStr, mContent, GetPresContext()->PresShell()) == NS_OK) {
    return true;
  }
  return false;
}

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
};

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGLinearGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGLinearGradient)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLinearGradientFrameBase)

// Implementation
nsresult
nsSVGLinearGradientFrame::GetX1(float *aX1)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> aLgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aLgrad, "Wrong content element (not linear gradient)");
  if (aLgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("x1"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGLinearGradient> aLNgrad = do_QueryInterface(aNextGrad);
      aLNgrad->GetX1(aX1);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aLgrad->GetX1(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aX1);
  return NS_OK;
}

nsresult
nsSVGLinearGradientFrame::GetY1(float *aY1)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> aLgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aLgrad, "Wrong content element (not linear gradient)");
  if (aLgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("y1"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGLinearGradient> aLNgrad = do_QueryInterface(aNextGrad);
      aLNgrad->GetY1(aY1);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aLgrad->GetY1(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aY1);
  return NS_OK;
}

nsresult
nsSVGLinearGradientFrame::GetX2(float *aX2)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> aLgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aLgrad, "Wrong content element (not linear gradient)");
  if (aLgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("x2"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGLinearGradient> aLNgrad = do_QueryInterface(aNextGrad);
      aLNgrad->GetX2(aX2);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aLgrad->GetX2(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aX2);
  return NS_OK;
}

nsresult
nsSVGLinearGradientFrame::GetY2(float *aY2)
{
  nsCOMPtr<nsIDOMSVGLinearGradientElement> aLgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aLgrad, "Wrong content element (not linear gradient)");
  if (aLgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("y2"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_LINEAR_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGLinearGradient> aLNgrad = do_QueryInterface(aNextGrad);
      aLNgrad->GetX1(aY2);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aLgrad->GetY2(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aY2);
  return NS_OK;
}


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
};

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGRadialGradientFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGRadialGradient)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRadialGradientFrameBase)

// Implementation
nsresult
nsSVGRadialGradientFrame::GetCx(float *aCx)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aRgrad, "Wrong content element (not radial gradient)");
  if (aRgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("cx"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGRadialGradient> aRNgrad = do_QueryInterface(aNextGrad);
      aRNgrad->GetCx(aCx);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aRgrad->GetCx(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aCx);
  return NS_OK;
}

nsresult
nsSVGRadialGradientFrame::GetCy(float *aCy)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aRgrad, "Wrong content element (not radial gradient)");
  if (aRgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("cy"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGRadialGradient> aRNgrad = do_QueryInterface(aNextGrad);
      aRNgrad->GetCy(aCy);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aRgrad->GetCy(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aCy);
  return NS_OK;
}

nsresult
nsSVGRadialGradientFrame::GetR(float *aR)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aRgrad, "Wrong content element (not radial gradient)");
  if (aRgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("r"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGRadialGradient> aRNgrad = do_QueryInterface(aNextGrad);
      aRNgrad->GetR(aR);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aRgrad->GetR(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aR);
  return NS_OK;
}

nsresult
nsSVGRadialGradientFrame::GetFx(float *aFx)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aRgrad, "Wrong content element (not radial gradient)");
  if (aRgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("fx"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGRadialGradient> aRNgrad = do_QueryInterface(aNextGrad);
      aRNgrad->GetFx(aFx);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aRgrad->GetCx(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aFx);
  return NS_OK;
}

nsresult
nsSVGRadialGradientFrame::GetFy(float *aFy)
{
  nsCOMPtr<nsIDOMSVGRadialGradientElement> aRgrad = do_QueryInterface(mContent);
  NS_ASSERTION(aRgrad, "Wrong content element (not radial gradient)");
  if (aRgrad == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another gradient
  if (checkURITarget(NS_LITERAL_STRING("fy"))) {
    // Yes, get it from the target
    nsISVGGradient *aNextGrad;
    if (nsSVGGradientFrame::GetNextGradient(&aNextGrad, nsISVGGradient::SVG_RADIAL_GRADIENT) == NS_OK) {
      nsCOMPtr<nsISVGRadialGradient> aRNgrad = do_QueryInterface(aNextGrad);
      aRNgrad->GetFy(aFy);
      return NS_OK;
    }
    // There are no gradients in the list with our type -- fall through
    // and return our default value
  }
  // No, return the values
  nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
  aRgrad->GetFy(getter_AddRefs(aLen));
  nsCOMPtr<nsIDOMSVGLength> sLen;
  aLen->GetBaseVal(getter_AddRefs(sLen));
  sLen->GetValue(aFy);
  return NS_OK;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsresult NS_NewSVGLinearGradientFrame(nsIPresShell* aPresShell, 
                                      nsIContent*   aContent, 
                                      nsIFrame**    aNewFrame)
{

#ifdef DEBUG_scooter
  printf("NS_NewSVGLinearGradientFrame called\n");
#endif
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGLinearGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGLinearGradient");
  if (!grad)
    return NS_ERROR_FAILURE;
  
  nsSVGLinearGradientFrame* it = new (aPresShell) nsSVGLinearGradientFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGLinearGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (!aRef) {
    it->mNextGrad = nsnull;
  } else {
    // Get the hRef
    nsCOMPtr<nsIDOMSVGAnimatedString> aHref;
    aRef->GetHref(getter_AddRefs(aHref));
    // We *really* want an observer on this
    nsAutoString aStr;
    aHref->GetBaseVal(aStr);
    it->mNextGradStr = aStr;
    it->mNextGrad = nsnull;
  }

  *aNewFrame = it;

  return NS_OK;
}

nsresult NS_NewSVGRadialGradientFrame(nsIPresShell* aPresShell, 
                                      nsIContent*   aContent, 
                                      nsIFrame**    aNewFrame)
{
#ifdef DEBUG_scooter
  printf("NS_NewSVGRadialGradientFrame called\n");
#endif
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGRadialGradientElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGRadialGradient");
  if (!grad)
    return NS_ERROR_FAILURE;
  
  nsSVGRadialGradientFrame* it = new (aPresShell) nsSVGRadialGradientFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, "NS_NewSVGRadialGradientFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (!aRef) {
    it->mNextGrad = nsnull;
  } else {
    // Get the hRef
    nsCOMPtr<nsIDOMSVGAnimatedString> aHref;
    aRef->GetHref(getter_AddRefs(aHref));
    // We *really* want an observer on this
    nsAutoString aStr;
    aHref->GetBaseVal(aStr);
    it->mNextGradStr = aStr;
    it->mNextGrad = nsnull;
  }

  *aNewFrame = it;

  return NS_OK;
}

nsresult NS_NewSVGStopFrame(nsIPresShell* aPresShell, 
                            nsIContent*   aContent, 
                            nsIFrame*     aParentFrame, 
                            nsIFrame**    aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGStopElement> grad = do_QueryInterface(aContent);
  NS_ASSERTION(grad, "NS_NewSVGStopFrame -- Content doesn't support nsIDOMSVGStopElement");
  if (!grad)
    return NS_ERROR_FAILURE;

  // Create an anonymous frame for ourselves (we only use it for style information)
  nsresult rv = NS_NewSVGGenericContainerFrame(aPresShell, aContent, aNewFrame);
  if (!NS_SUCCEEDED(rv) || *aNewFrame == nsnull)
    return rv;
  
  return NS_OK;
}

// Public function to locate the SVGGradientFrame structure pointed to by a URI
// and return a nsISVGGradient
nsresult NS_GetSVGGradient(nsISVGGradient **result, nsIURI *aURI, nsIContent *aContent, 
                           nsIPresShell *aPresShell)
{
  *result = nsnull;

  // Get the spec from the URI
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);
  return GetSVGGradient(result, uriSpec, aContent, aPresShell);
}

// Static (helper) function to get a gradient from URI spec
static nsresult GetSVGGradient(nsISVGGradient **result, nsCAutoString& aSpec, nsIContent *aContent,
                               nsIPresShell *aPresShell)
{
  nsresult rv = NS_OK;
  *result = nsnull;

  // Get the ID from the spec (no ID = an error)
  PRInt32 pos = aSpec.FindChar('#');
  if (pos == -1) {
    NS_ASSERTION(pos != -1, "URI Spec not a reference");
    return NS_ERROR_FAILURE;
  }

  // Strip off the hash and get the name
  nsCAutoString aURICName;
  aSpec.Right(aURICName, aSpec.Length()-(pos + 1));

  // Get a unicode string
  nsAutoString  aURIName;
  CopyUTF8toUTF16(aURICName, aURIName);

  // Get the domDocument
  nsCOMPtr<nsIDOMDocument>domDoc = do_QueryInterface(aContent->GetDocument());
  NS_ASSERTION(domDoc, "Content doesn't reference a dom Document");
  if (domDoc == nsnull) {
    return NS_ERROR_FAILURE;
  }

  // Get the gradient element
  nsCOMPtr<nsIDOMElement> element;
  nsIFrame *grad;
  rv = domDoc->GetElementById(aURIName, getter_AddRefs(element));
  if (!NS_SUCCEEDED(rv) || element == nsnull) {
    return rv;  
  }

  // Note: the following queries are not really required, but we want to make sure
  // we're pointing to gradient elements.
  nsCOMPtr<nsIDOMSVGLinearGradientElement> lGradient = do_QueryInterface(element);
  if (!lGradient) {
    nsCOMPtr<nsIDOMSVGRadialGradientElement> rGradient = do_QueryInterface(element);
    if (!rGradient) {
      NS_ASSERTION(PR_FALSE, "Not a gradient element!");
      return NS_ERROR_FAILURE;
    } 
  } 

  // Get the Primary Frame
  nsCOMPtr<nsIContent> aGContent = do_QueryInterface(element);
  NS_ASSERTION(aPresShell, "svg get gradient -- no pres shell provided");
  if (!aPresShell)
    return NS_ERROR_FAILURE;
  rv = aPresShell->GetPrimaryFrameFor(aGContent, &grad);

  nsCOMPtr<nsISVGGradient> aGrad = do_QueryInterface(grad);
  *result = aGrad;
  return rv;
}

