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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGPattern.h"
#include "nsWeakReference.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGStopElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsIDOMSVGNumber.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueUtils.h"
#include "nsStyleContext.h"
#include "nsISVGValueObserver.h"
#include "nsSVGValue.h"
#include "nsNetUtil.h"
#include "nsINameSpaceManager.h"
#include "nsISVGChildFrame.h"
#include "nsIDOMSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsSVGRect.h"
#include "nsISVGPattern.h"
#include "nsIURI.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsSVGNumber.h"
#include "nsLayoutAtoms.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGRendererSurface.h"
#include "nsSVGUtils.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsSVGDefsFrame.h"
#include "nsSVGPatternElement.h"
#include "nsSVGGeometryFrame.h"

typedef nsSVGDefsFrame  nsSVGPatternFrameBase;

#ifdef DEBUG_scooter
static void printCTM(char *msg, nsIDOMSVGMatrix *aCTM);
static void printRect(char *msg, nsIDOMSVGRect *aRect);
#endif

class nsSVGPatternFrame : public nsSVGPatternFrameBase,
                          public nsSVGValue,
                          public nsISVGPattern,
                          public nsISVGValueObserver,
                          public nsSupportsWeakReference
{
public:
  friend nsIFrame* NS_NewSVGPatternFrame(nsIPresShell* aPresShell, 
                                         nsIContent*   aContent,
                                         nsStyleContext* aContext);

  friend nsresult NS_GetSVGPatternFrame(nsIFrame**      result, 
                                        nsIURI*         aURI, 
                                        nsIContent*     aContent,
                                        nsIPresShell*   aPresShell);

  nsSVGPatternFrame(nsStyleContext* aContext) : nsSVGPatternFrameBase(aContext) {}

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGPattern interface:
  NS_DECL_NSISVGPATTERN

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString &aValue) { return NS_OK; }
  NS_IMETHOD GetValueString(nsAString& aValue) { 
    return NS_ERROR_NOT_IMPLEMENTED; 
  }

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable, 
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable, 
                                    nsISVGValue::modificationType aModType);
  
  // nsISVGContainerFrame interface:
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();
  NS_IMETHOD GetBBox(nsIDOMSVGRect **aRect);

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext();

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgPatternFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  // nsIFrameDebug interface:
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGPattern"), aResult);
  }
#endif // DEBUG

protected:
  virtual ~nsSVGPatternFrame();

  // Internal methods for handling referenced patterns
  PRBool checkURITarget(nsIAtom *);
  PRBool checkURITarget();
  //
  nsSVGLength2 *GetX();
  nsSVGLength2 *GetY();
  nsSVGLength2 *GetWidth();
  nsSVGLength2 *GetHeight();

  NS_IMETHOD GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio 
                                                     **aPreserveAspectRatio);
  NS_IMETHOD GetPatternFirstChild(nsIFrame **kid);
  NS_IMETHOD GetViewBox(nsIDOMSVGRect * *aMatrix);
  nsresult   GetPatternRect(nsIDOMSVGRect **patternRect, nsIDOMSVGRect *bbox, 
                            nsSVGElement *content);
  nsresult   GetPatternMatrix(nsIDOMSVGMatrix **aCTM, 
                              nsIDOMSVGRect *bbox,
                              nsIDOMSVGMatrix *callerCTM);
  nsresult   ConstructCTM(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *callerBBox);
  nsresult   CreateSurface(nsISVGRendererSurface **aSurface);
  nsresult   GetCallerGeometry(nsIDOMSVGMatrix **aCTM, 
                               nsIDOMSVGRect **aBBox,
                               nsSVGElement **aContent, 
                               nsSVGGeometryFrame *aSource);

  //

private:
  nsCOMPtr<nsIDOMSVGRect>                 mBBox;

  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be termporary because different
  // referencing frames will all reference this one fram
  nsSVGGeometryFrame                     *mSource;
  nsCOMPtr<nsIDOMSVGMatrix>               mCTM;

protected:
  nsSVGPatternFrame                      *mNextPattern;
  nsCOMPtr<nsIDOMSVGAnimatedString> 	  mHref;
  PRPackedBool                            mLoopFlag;
};

//----------------------------------------------------------------------
// Implementation

nsSVGPatternFrame::~nsSVGPatternFrame()
{
  WillModify(mod_die);
  if (mNextPattern)
    mNextPattern->RemoveObserver(this);

  // Notify the world that we're dying
  DidModify(mod_die);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_INTERFACE_MAP_BEGIN(nsSVGPatternFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGPattern)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPatternFrameBase)

//----------------------------------------------------------------------
// nsISVGValueObserver methods:
NS_IMETHODIMP
nsSVGPatternFrame::WillModifySVGObservable(nsISVGValue* observable,
                                            modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}
                                                                                
NS_IMETHODIMP
nsSVGPatternFrame::DidModifySVGObservable(nsISVGValue* observable, 
                                          nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsISVGPattern> pattern = do_QueryInterface(observable);
  // Is this a pattern we are observing that is going away?
  if (mNextPattern && aModType == nsISVGValue::mod_die && pattern) {
    // Yes, we need to handle this differently
    nsISVGPattern *patt;
    CallQueryInterface(mNextPattern, &patt);
    if (patt == pattern) {
      mNextPattern = nsnull;
    }
  }
  // Something we depend on was modified -- pass it on!
  DidModify(aModType);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGPatternFrame::DidSetStyleContext()
{
  WillModify();
  DidModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::patternUnits ||
       aAttribute == nsGkAtoms::patternContentUnits ||
       aAttribute == nsGkAtoms::patternTransform ||
       aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox)) {
    WillModify();
    DidModify();
    return NS_OK;
  } 

  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    if (mNextPattern)
      mNextPattern->RemoveObserver(this);
    mNextPattern = nsnull;
    WillModify();
    DidModify();
    return NS_OK;
  }

  return nsSVGPatternFrameBase::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}

nsIAtom*
nsSVGPatternFrame::GetType() const
{
  return nsLayoutAtoms::svgPatternFrame;
}

PRBool
nsSVGPatternFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

//----------------------------------------------------------------------
// nsISVGContainerFrame interface:

// If our GetCanvasTM is getting called, we
// need to return *our current* transformation
// matrix, which depends on our units parameters
// and X, Y, Width, and Height
already_AddRefed<nsIDOMSVGMatrix> 
nsSVGPatternFrame::GetCanvasTM() {
  nsIDOMSVGMatrix *rCTM;
  
  if (mCTM) {
    rCTM = mCTM;
    NS_IF_ADDREF(rCTM);
  } else {
    // Do we know our rendering parent?
    if (mSource) {
      // Yes, use it!
      mSource->GetCanvasTM(&rCTM);
    } else {
      // No, return an identity
      // We get here when geometry in the <pattern> container is updated
      NS_NewSVGMatrix(&rCTM);
    }
  }
  return rCTM;  
}

already_AddRefed<nsSVGCoordCtxProvider> 
nsSVGPatternFrame::GetCoordContextProvider() {
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), 
                          (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCoordContextProvider();  
}

NS_IMETHODIMP
nsSVGPatternFrame::GetBBox(nsIDOMSVGRect **aRect) {
  *aRect = nsnull;

  if (mBBox) {
    *aRect = mBBox;
    return NS_OK;
  } else {
    if (mSource) {
      // Yes, use it!
      nsCOMPtr<nsISVGChildFrame> callerSVGFrame =
        do_QueryInterface(NS_STATIC_CAST(nsIFrame*, mSource));
      if (callerSVGFrame) {
        return callerSVGFrame->GetBBox(aRect);
      }
    } else {
      // No, we'll use our content parent, then
      nsCOMPtr<nsISVGChildFrame> aFrame = do_QueryInterface(mParent); 
      if (aFrame) {
        return aFrame->GetBBox(aRect);
      }
    }
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
//  nsISVGPattern implementation
//----------------------------------------------------------------------
NS_IMETHODIMP
nsSVGPatternFrame::PaintPattern(nsISVGRendererCanvas* canvas, 
                                nsISVGRendererSurface** surface,
                                nsIDOMSVGMatrix** patternMatrix,
                                nsSVGGeometryFrame *aSource)
{

  /*
   * General approach:
   *    Set the content geometry stuff
   *    Calculate our bbox (using x,y,width,height & patternUnits & 
   *                        patternTransform)
   *    Create the surface
   *    Calculate the content transformation matrix
   *    Get our children (we may need to get them from another Pattern)
   *    Call SVGPaint on all of our children
   *    Return
   */
  *surface = nsnull;
  *patternMatrix = nsnull;

  // Get our child
  nsIFrame *firstKid;
  if (NS_FAILED(GetPatternFirstChild(&firstKid)))
    return NS_ERROR_FAILURE; // Either no kids or a bad reference

  /*
   * Get the content geometry information.  This is a little tricky --
   * our parent is probably a <defs>, but we are rendering in the context
   * of some geometry source.  Our content geometry information needs to
   * come from our rendering parent as opposed to our content parent.  We
   * get that information from aSource, which is passed to us from the
   * backend renderer.
   *
   * There are three "geometries" that we need:
   *   1) The bounding box for the pattern.  We use this to get the
   *      width and height for the surface, and as the return to
   *      GetBBox.
   *   2) The transformation matrix for the pattern.  This is not *quite*
   *      the same as the canvas transformation matrix that we will
   *      provide to our rendering children since we "fudge" it a little
   *      to get the renderer to handle the translations correctly for us.
   *   3) The CTM that we return to our children who make up the pattern.
   */

  // Get all of the information we need from our "caller" -- i.e.
  // the geometry that is being rendered with a pattern
  nsSVGElement *callerContent;
  nsCOMPtr<nsIDOMSVGRect> callerBBox;
  nsCOMPtr<nsIDOMSVGMatrix> callerCTM;
  if (NS_FAILED(GetCallerGeometry(getter_AddRefs(callerCTM),
                                  getter_AddRefs(callerBBox),
                                  &callerContent, aSource)))
    return NS_ERROR_FAILURE;

  // Construct the CTM that we will provide to our children when we
  // render them into the tile.
  if (NS_FAILED(ConstructCTM(getter_AddRefs(mCTM), callerBBox)))
    return NS_ERROR_FAILURE;

  // Get the bounding box of the pattern.  This will be used to determine
  // the size of the surface, and will also be used to define the bounding
  // box for the pattern tile.
  if (NS_FAILED(GetPatternRect(getter_AddRefs(mBBox), callerBBox, 
                               callerContent)))
    return NS_ERROR_FAILURE;

  // Get the transformation matrix that we will hand to the renderer's pattern
  // routine.
  if (NS_FAILED(GetPatternMatrix(patternMatrix, mBBox, callerCTM)))
     return NS_ERROR_FAILURE;

#ifdef DEBUG_scooter
  printRect("Bounding Rect: ",mBBox);
  printCTM("Pattern TM ",*patternMatrix);
  printCTM("Child TM ",mCTM);
#endif

  // Now that we have all of the necessary geometries, we can
  // create our surface.  Note that this uses mBBox for the size
  nsCOMPtr<nsISVGRendererSurface> patternSurface;
  if (NS_FAILED(CreateSurface(getter_AddRefs(patternSurface))))
    return NS_ERROR_FAILURE;

  // Push the surface
  if (NS_FAILED(canvas->PushSurface(patternSurface, PR_FALSE)))
    return NS_ERROR_FAILURE; //?

  // OK, now render -- note that we use "firstKid", which
  // we got at the beginning because it takes care of the
  // referenced pattern situation for us

  // Set our geometrical parent
  mSource = aSource;

  nsRect dummyRect;
  for (nsIFrame* kid = firstKid; kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(canvas, kid);
  }
  mSource = nsnull;

  // The surface on the top of the stack is the surface
  // that was passed to us, and has the pattern cell.  We
  // pop it off of the stack, but we don't destroy it so
  // that the renderer can do what needs to be done to
  // use it as a pattern
  canvas->PopSurface();
  *surface = patternSurface;
  NS_IF_ADDREF(*surface);
  return NS_OK;
}

/* Will probably need something like this... */
// How do we handle the insertion of a new frame?
// We really don't want to rerender this every time,
// do we?
NS_IMETHODIMP
nsSVGPatternFrame::GetPatternFirstChild(nsIFrame **kid)
{
  nsresult rv = NS_OK;

  // Do we have any children ourselves?
  if (!(*kid = mFrames.FirstChild())) {
    // No, see if we chain to someone who does
    if (checkURITarget())
      rv = mNextPattern->GetPatternFirstChild(kid);
    else
      rv = NS_ERROR_FAILURE; // No children = error
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPatternUnits(PRUint16 *aUnits)
{
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::patternUnits)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = 
                                            do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedEnumeration> units;
    patternElement->GetPatternUnits(getter_AddRefs(units));
    units->GetAnimVal(aUnits);
  } else {
    // Yes, get it from the target
    mNextPattern->GetPatternUnits(aUnits);
  }  
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPatternContentUnits(PRUint16 *aUnits)
{
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::patternContentUnits)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = 
                                            do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedEnumeration> units;
    patternElement->GetPatternContentUnits(getter_AddRefs(units));
    units->GetAnimVal(aUnits);
  } else {
    // Yes, get it from the target
    mNextPattern->GetPatternContentUnits(aUnits);
  }  
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPatternTransform(nsIDOMSVGMatrix **aPatternTransform)
{
  *aPatternTransform = nsnull;

  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::patternTransform)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = 
                                            do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedTransformList> trans;
    patternElement->GetPatternTransform(getter_AddRefs(trans));
    nsCOMPtr<nsIDOMSVGTransformList> lTrans;
    trans->GetAnimVal(getter_AddRefs(lTrans));
    lTrans->GetConsolidationMatrix(aPatternTransform);
    if (!*aPatternTransform) {
      nsresult rv = NS_NewSVGMatrix(aPatternTransform);
      if (NS_FAILED(rv)) {
        mLoopFlag = PR_FALSE;
        return rv;
      }
    }
  } else {
    // Yes, get it from the target
    mNextPattern->GetPatternTransform(aPatternTransform);
  }
  mLoopFlag = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetViewBox(nsIDOMSVGRect **aViewBox)
{
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::viewBox)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGFitToViewBox> patternElement = 
                                            do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedRect> viewBox;
    patternElement->GetViewBox(getter_AddRefs(viewBox));
    viewBox->GetAnimVal(aViewBox);
  } else {
    // Yes, get it from the target
    mNextPattern->GetViewBox(aViewBox);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio 
                                          **aPreserveAspectRatio)
{
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::preserveAspectRatio)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGFitToViewBox> patternElement = 
                                            do_QueryInterface(mContent);
    patternElement->GetPreserveAspectRatio(aPreserveAspectRatio);
  } else {
    // Yes, get it from the target
    mNextPattern->GetPreserveAspectRatio(aPreserveAspectRatio);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetX(float *aX)
{
  nsSVGLength2 *len;
  if ((len = GetX()) == nsnull) {
    *aX = 0.0f;
    return NS_ERROR_FAILURE;
  }
  *aX = len->GetAnimValue(NS_STATIC_CAST(nsSVGCoordCtxProvider*, nsnull));
  return NS_OK;
}

nsSVGLength2 *
nsSVGPatternFrame::GetX()
{
  nsSVGLength2 *rv = nsnull;

  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::x)) {
    // Yes, get it from the target
    rv = mNextPattern->GetX();
  } else {
    // No, return the values
    nsSVGPatternElement *pattern =
      NS_STATIC_CAST(nsSVGPatternElement*, mContent);
    rv = &pattern->mLengthAttributes[nsSVGPatternElement::X];
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetY(float *aY)
{
  nsSVGLength2 *len;
  if ((len = GetY()) == nsnull) {
    *aY = 0.0;
    return NS_ERROR_FAILURE;
  }
  *aY = len->GetAnimValue(NS_STATIC_CAST(nsSVGCoordCtxProvider*, nsnull));
  return NS_OK;
}

nsSVGLength2 *
nsSVGPatternFrame::GetY()
{
  nsSVGLength2 *rv = nsnull;

  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::y)) {
    // Yes, get it from the target
    rv = mNextPattern->GetY();
  } else {
    // No, return the values
    nsSVGPatternElement *pattern =
      NS_STATIC_CAST(nsSVGPatternElement*, mContent);
    rv = &pattern->mLengthAttributes[nsSVGPatternElement::Y];
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetWidth(float *aWidth)
{
  nsSVGLength2 *len;
  if ((len = GetWidth()) == nsnull) {
    *aWidth = 0.0f;
    return NS_ERROR_FAILURE;
  }
 *aWidth = len->GetAnimValue(NS_STATIC_CAST(nsSVGCoordCtxProvider*, nsnull));
 return NS_OK;
}

nsSVGLength2 *
nsSVGPatternFrame::GetWidth()
{
  nsSVGLength2 *rv = nsnull;

  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::width)) {
    // Yes, get it from the target
    rv = mNextPattern->GetWidth();
  } else {
    // No, return the values
    nsSVGPatternElement *pattern =
      NS_STATIC_CAST(nsSVGPatternElement*, mContent);
    rv = &pattern->mLengthAttributes[nsSVGPatternElement::WIDTH];
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetHeight(float *aHeight)
{
  nsSVGLength2 *len;
  if ((len = GetHeight()) = nsnull) {
    *aHeight = 0.0f;
    return NS_ERROR_FAILURE;
  }
 *aHeight = len->GetAnimValue(NS_STATIC_CAST(nsSVGCoordCtxProvider*, nsnull));
  return NS_OK;
}

nsSVGLength2 *
nsSVGPatternFrame::GetHeight()
{
  nsSVGLength2 *rv = nsnull;

  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::height)) {
    // Yes, get it from the target
    rv = mNextPattern->GetHeight();
  } else {
    // No, return the values
    nsSVGPatternElement *pattern =
      NS_STATIC_CAST(nsSVGPatternElement*, mContent);
    rv = &pattern->mLengthAttributes[nsSVGPatternElement::HEIGHT];
  }
  mLoopFlag = PR_FALSE;
  return rv;
}

// Private (helper) methods
PRBool 
nsSVGPatternFrame::checkURITarget(nsIAtom *attr) {
  // Was the attribute explicitly set?
  if (mContent->HasAttr(kNameSpaceID_None, attr)) {
    // Yes, just return
    return PR_FALSE;
  }
  return checkURITarget();
}

PRBool
nsSVGPatternFrame::checkURITarget(void) {
  nsIFrame *nextPattern;
  mLoopFlag = PR_TRUE; // Set our loop detection flag
  // Have we already figured out the next Pattern?
  if (mNextPattern != nsnull) {
    return PR_TRUE;
  }

  // check if we reference another pattern to "inherit" its children
  // or attributes
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

  // Note that we are using *our* frame tree for this call, 
  // otherwise we're going to have to get the PresShell in each call
  if (NS_SUCCEEDED(
          nsSVGUtils::GetReferencedFrame(&nextPattern, targetURI, 
                                         mContent, 
                                         GetPresContext()->PresShell()))) {
    nsIAtom* frameType = nextPattern->GetType();
    if (frameType != nsLayoutAtoms::svgPatternFrame)
      return PR_FALSE;
    mNextPattern = (nsSVGPatternFrame *)nextPattern;
    // Are we looping?
    if (mNextPattern->mLoopFlag) {
      // Yes, remove the reference and return an error
      NS_WARNING("Pattern loop detected!");
      mNextPattern = nsnull;
      return PR_FALSE;
    }
    // Add ourselves to the observer list
    if (mNextPattern) {
      // Can't use the NS_ADD macro here because of nsISupports ambiguity
      mNextPattern->AddObserver(this);
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

// -------------------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------------------

nsresult 
nsSVGPatternFrame::GetPatternRect(nsIDOMSVGRect **patternRect, 
                                  nsIDOMSVGRect *bbox, 
                                  nsSVGElement *content)
{
  // Get our type
  PRUint16 type;
  GetPatternUnits(&type);

  // We need to initialize our box
  float x,y,width,height;

  // Get the pattern x,y,width, and height
  nsSVGLength2 *tmpX, *tmpY, *tmpHeight, *tmpWidth;
  tmpX = GetX();
  tmpY = GetY();
  tmpHeight = GetHeight();
  tmpWidth = GetWidth();

  if (type == nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX) {
    x = nsSVGUtils::ObjectSpace(bbox, tmpX);
    y = nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    x = nsSVGUtils::UserSpace(content, tmpX);
    y = nsSVGUtils::UserSpace(content, tmpY);
    width = nsSVGUtils::UserSpace(content, tmpWidth);
    height = nsSVGUtils::UserSpace(content, tmpHeight);
  }

  return NS_NewSVGRect(patternRect, x, y, width, height);
}

nsresult
nsSVGPatternFrame::ConstructCTM(nsIDOMSVGMatrix **aCTM,
                                nsIDOMSVGRect *callerBBox)
{
  nsCOMPtr<nsIDOMSVGMatrix> tCTM, tempTM;

  // Begin by handling the objectBoundingBox conversion since
  // this must be handled in the CTM
  PRUint16 type;
  GetPatternContentUnits(&type);

  if (type == nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX) {
    // Use the bounding box
    float minx,miny,width,height;
    callerBBox->GetX(&minx);
    callerBBox->GetY(&miny);
    callerBBox->GetWidth(&width);
    callerBBox->GetHeight(&height);
    NS_NewSVGMatrix(getter_AddRefs(tCTM), width, 0.0f, 0.0f, 
                    height, 0.0f, 0.0f);
  } else {
    NS_NewSVGMatrix(getter_AddRefs(tCTM));
  }

  // Do we have a viewbox?
  nsCOMPtr<nsIDOMSVGRect> viewRect;
  GetViewBox(getter_AddRefs(viewRect));

  // See if we really have something
  float viewBoxX, viewBoxY, viewBoxHeight, viewBoxWidth;
  viewRect->GetX(&viewBoxX);
  viewRect->GetY(&viewBoxY);
  viewRect->GetHeight(&viewBoxHeight);
  viewRect->GetWidth(&viewBoxWidth);
  if (viewBoxHeight != 0.0f && viewBoxWidth != 0.0f) {

    float viewportWidth, viewportHeight;
    GetWidth(&viewportWidth);
    GetHeight(&viewportHeight);

    float refX, refY;
    GetX(&refX);
    GetY(&refY);

    nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> par;
    GetPreserveAspectRatio(getter_AddRefs(par));

    tempTM = nsSVGUtils::GetViewBoxTransform(viewportWidth, viewportHeight,
                                             viewBoxX + refX, viewBoxY + refY,
                                             viewBoxWidth, viewBoxHeight,
                                             par,
                                             PR_TRUE);

  } else {
    // No viewBox, construct from the (modified) parent matrix
    NS_NewSVGMatrix(getter_AddRefs(tempTM),
                    1.0f,     0.0f,
                    0.0f,     1.0f,
                    0.0f,     0.0f);
  }
  tempTM->Multiply(tCTM, aCTM);
  return NS_OK;
}

nsresult
nsSVGPatternFrame::GetPatternMatrix(nsIDOMSVGMatrix **aCTM, 
                                    nsIDOMSVGRect *bbox,
                                    nsIDOMSVGMatrix *callerCTM)
{
  // Get the pattern transform
  nsCOMPtr<nsIDOMSVGMatrix> patternTransform;
  GetPatternTransform(getter_AddRefs(patternTransform));

  // We really want the pattern matrix to handle translations
  float minx,miny;
  bbox->GetX(&minx);
  bbox->GetY(&miny);
  nsCOMPtr<nsIDOMSVGMatrix> tCTM;
  patternTransform->Translate(minx, miny, getter_AddRefs(tCTM));

  // Multiply in the caller's CTM to handle device coordinates
  callerCTM->Multiply(tCTM, aCTM);
  return NS_OK;
}

nsresult
nsSVGPatternFrame::GetCallerGeometry(nsIDOMSVGMatrix **aCTM, 
                                     nsIDOMSVGRect **aBBox,
                                     nsSVGElement **aContent,
                                     nsSVGGeometryFrame *aSource)
{
  *aCTM = nsnull;
  *aBBox = nsnull;
  *aContent = nsnull;

  // Make sure the callerContent is an SVG element.  If we are attempting
  // to paint a pattern for text, then the content will be the #text, so we
  // actually want the parent, which should be the <svg:text> or <svg:tspan>
  // element.
  nsIAtom *callerType = aSource->GetType();
  if (callerType ==  nsLayoutAtoms::svgGlyphFrame) {
    *aContent = NS_STATIC_CAST(nsSVGElement*,
                               aSource->GetContent()->GetParent());
  } else {
    *aContent = NS_STATIC_CAST(nsSVGElement*, aSource->GetContent());
  }
  NS_ASSERTION(aContent,"Caller does not have any content!");
  if (!aContent)
    return NS_ERROR_FAILURE;

  // Get the calling geometry's bounding box.  This
  // will be in *device coordinates*
  nsISVGChildFrame *callerSVGFrame;
  CallQueryInterface(aSource, &callerSVGFrame);
  callerSVGFrame->GetBBox(aBBox);
  // Sanity check
  {
    float width, height;
    (*aBBox)->GetWidth(&width);
    (*aBBox)->GetHeight(&height);
    if (width <= 0 || height <= 0) {
      return NS_ERROR_FAILURE;
    }
  }

  // Get the transformation matrix from our calling geometry
  aSource->GetCanvasTM(aCTM);

  // OK, now fix up the bounding box to reflect user coordinates
  // We handle device unit scaling in pattern matrix
  {
    float x,y,width,height,xscale,yscale;
    (*aBBox)->GetX(&x);
    (*aBBox)->GetY(&y);
    (*aBBox)->GetWidth(&width);
    (*aBBox)->GetHeight(&height);
    (*aCTM)->GetA(&xscale);
    (*aCTM)->GetD(&yscale);
    x = x / xscale;
    y = y / yscale;
    width = width / xscale;
    height = height / yscale;
    (*aBBox)->SetX(x);
    (*aBBox)->SetY(y);
    (*aBBox)->SetWidth(width);
    (*aBBox)->SetHeight(height);
  }
  return NS_OK;
}

nsresult
nsSVGPatternFrame::CreateSurface(nsISVGRendererSurface **aSurface)
{
  *aSurface = nsnull;

  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    return NS_ERROR_FAILURE;
  }

  float width, height;
  mBBox->GetWidth(&width);
  mBBox->GetHeight(&height);

  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
#ifdef DEBUG_scooter
  printf("Creating %dX%d surface\n",(int)(width),(int)(height));
#endif
  renderer->CreateSurface((int)(width), (int)(height), aSurface);
  return NS_OK;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(nsIPresShell*   aPresShell,
                                nsIContent*     aContent,
                                nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(aContent);
  NS_ASSERTION(patternElement, 
               "NS_NewSVGPatternFrame -- Content doesn't support nsIDOMSVGPattern");
  if (!patternElement)
    return nsnull;

  nsSVGPatternFrame* it = new (aPresShell) nsSVGPatternFrame(aContext);
  if (!it)
    return nsnull;

  nsCOMPtr<nsIDOMSVGURIReference> ref = do_QueryInterface(aContent);
  NS_ASSERTION(ref, 
               "NS_NewSVGPatternFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (ref) {
    // Get the hRef
    ref->GetHref(getter_AddRefs(it->mHref));
  }

  it->mNextPattern = nsnull;
  it->mLoopFlag = PR_FALSE;
#ifdef DEBUG_scooter
  printf("NS_NewSVGPatternFrame\n");
#endif
  return it;
}

// Public function to locate the SVGPatternFrame structure pointed to by a URI
// and return a nsISVGPattern
nsresult NS_GetSVGPattern(nsISVGPattern **aPattern, nsIURI *aURI, 
                          nsIContent *aContent, 
                          nsIPresShell *aPresShell)
{
  *aPattern = nsnull;

#ifdef DEBUG_scooter
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);
  printf("NS_GetSVGPattern: uri = %s\n",uriSpec.get());
#endif
  nsIFrame *result;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&result, aURI,
                                                   aContent, aPresShell)))
    return NS_ERROR_FAILURE;
#ifdef DEBUG_scooter
  const char *contentString;
  nsIAtom *tag = aContent->Tag();
  tag->GetUTF8String(&contentString);
  printf("NS_GetSVGPattern: checking content -- caller content tag is %s, ",
          contentString);
  tag = result->GetContent()->Tag();
  tag->GetUTF8String(&contentString);
  printf("target content tag is %s\n",
          contentString);
#endif
  // Check to see if we are referencing an ancestor (*not* a good thing)
  if (result && nsContentUtils::ContentIsDescendantOf(aContent, result->GetContent())) {
    NS_WARNING("Pattern child references pattern!");
    return NS_ERROR_FAILURE;
  }
  return result->QueryInterface(NS_GET_IID(nsISVGPattern), (void **)aPattern);
}

#ifdef DEBUG_scooter
static void printCTM(char *msg, nsIDOMSVGMatrix *aCTM)
{
  float a,b,c,d,e,f;
  aCTM->GetA(&a); 
  aCTM->GetB(&b); 
  aCTM->GetC(&c);
  aCTM->GetD(&d); 
  aCTM->GetE(&e); 
  aCTM->GetF(&f);
  printf("%s {%f,%f,%f,%f,%f,%f}\n",msg,a,b,c,d,e,f);
}

static void printRect(char *msg, nsIDOMSVGRect *aRect)
{
  float x,y,width,height;
  aRect->GetX(&x); 
  aRect->GetY(&y); 
  aRect->GetWidth(&width); 
  aRect->GetHeight(&height); 
  printf("%s {%f,%f,%f,%f}\n",msg,x,y,width,height);
}
#endif
