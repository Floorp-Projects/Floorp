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
#include "nsISVGGeometrySource.h"
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
                                         nsIContent*   aContent);

  friend nsresult NS_GetSVGPatternFrame(nsIFrame**      result, 
                                        nsIURI*         aURI, 
                                        nsIContent*     aContent,
                                        nsIPresShell*   aPresShell);

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
  NS_IMETHOD GetX(nsIDOMSVGLength **aX);
  NS_IMETHOD GetY(nsIDOMSVGLength **aY);
  NS_IMETHOD GetHeight(nsIDOMSVGLength **aHeight);
  NS_IMETHOD GetWidth(nsIDOMSVGLength **aWidth);

  NS_IMETHOD GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio **aPreserveAspectRatio);
  NS_IMETHOD GetPatternFirstChild(nsIFrame **kid);
  NS_IMETHOD GetViewBox(nsIDOMSVGRect * *aMatrix);
  nsresult GetPatternRect(nsIDOMSVGRect **patternRect, 
                          nsIDOMSVGRect *bbox, nsIContent *content);
  nsresult GetPatternMatrix(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *bbox, nsIDOMSVGRect *aViewRect);
  nsresult ConstructCTM(nsIDOMSVGMatrix **ctm, nsIDOMSVGMatrix *aPCTM, nsIDOMSVGRect *aViewRect);

private:
  nsCOMPtr<nsIDOMSVGRect>                 mBBox;

  // this is a *temporary* reference to the frame of the element currently
  // referencing our pattern.  This must be termporary because different
  // referencing frames will all reference this one fram
  nsISVGGeometrySource                   *mSource;
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
      // No, we'll use our content parent, then
      nsCOMPtr<nsISVGGeometrySource> aSource = do_QueryInterface(mParent); 
      if (aSource) {
        aSource->GetCanvasTM(&rCTM);
      } else {
        // OK, we have no content parent, which means that we're
        // not part of the document tree. Return an identity matrix?
        NS_NewSVGMatrix(&rCTM, 1.0f,0.0f,0.0f,1.0f,0.0f,0.0f);
      }
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
      nsCOMPtr<nsISVGChildFrame> callerSVGFrame = do_QueryInterface(mSource);
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
                                nsISVGGeometrySource *aSource)
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
   */

  // Get the geometric parent CTM to get our scale
  nsCOMPtr<nsISVGChildFrame> callerSVGFrame = do_QueryInterface(aSource);
  NS_ASSERTION(callerSVGFrame,"Caller is not an nsISVGChildFrame!");
  nsIFrame *callerFrame;
  CallQueryInterface(callerSVGFrame, &callerFrame);
  NS_ASSERTION(callerFrame,"Caller is not an nsIFrame!");

  // Make sure the callerContent is an SVG element.  If we are attempting
  // to paint a pattern for text, then the content will be the #text, so we
  // actually want the parent, which should be the <svg:text> or <svg:tspan>
  // element.
  nsIContent *callerContent;
  nsIAtom *callerType = callerFrame->GetType();
  if (callerType ==  nsLayoutAtoms::svgGlyphFrame) {
    callerContent = callerFrame->GetContent()->GetParent();
  } else {
    callerContent = callerFrame->GetContent();
  }
  NS_ASSERTION(callerContent,"Caller does not have any content!");

  nsISVGContainerFrame *container;
  CallQueryInterface(callerFrame, &container);
  nsCOMPtr<nsIDOMSVGMatrix> parentCTM;
  if (container) {
    parentCTM = container->GetCanvasTM();
  } else {
    aSource->GetCanvasTM(getter_AddRefs(parentCTM));
  }

  /*
   * OK, we've got the content geometry in general.  Now we need to get two
   * different transformation matrices.  First, we need to transform the
   * x,y,width,and height units.  This is done differently than the units of
   * the actual elements we are going to paint.  We also need to pass some
   * of this information back to our caller (to get the x,y offset of the
   * pattern within the painted area.  So, we need to generate two matrices
   */
  
  // Get the parent scale values.  We need these to scale our width & height
  // for the surface.
  float xScale, yScale, xTranslate, yTranslate;
  parentCTM->GetA(&xScale);
  parentCTM->GetD(&yScale);
  parentCTM->GetE(&xTranslate);
  parentCTM->GetF(&yTranslate);

  // Get our Viewbox (if we have one)
  nsCOMPtr<nsIDOMSVGRect> viewRect;
  GetViewBox(getter_AddRefs(viewRect));

  // Construct a CTM for the parent that excludes the translation -- this
  // will become the CanvasTM we provide.  Note that mCTM must take into
  // account the viewBox/Viewport transformations since these can not be
  // handled by the renderer's pattern matrix
  if (NS_FAILED(ConstructCTM(getter_AddRefs(mCTM), parentCTM, viewRect)))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMSVGRect> bbox;
  callerSVGFrame->GetBBox(getter_AddRefs(bbox));

  // We don't want to even try if we have a degenerate path
  {
    float width, height;
    bbox->GetWidth(&width);
    bbox->GetHeight(&height);
    if (width <= 0 || height <= 0) {
      return NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(GetPatternRect(getter_AddRefs(mBBox), bbox, callerContent)))
    return NS_ERROR_FAILURE;

  // Create the new surface
  float x, y, width, height;
  mBBox->GetWidth(&width);
  mBBox->GetHeight(&height);
  mBBox->GetX(&x);
  mBBox->GetY(&y);
  mBBox->SetX(0.0f);
  mBBox->SetY(0.0f);
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  nsCOMPtr<nsISVGRendererSurface> patternSurface;
#ifdef DEBUG_scooter
  printf("Creating %dX%d surface\n",(int)(xScale*width),(int)(yScale*height));
#endif
  renderer->CreateSurface((int)(xScale*width), (int)(yScale*height), 
                          getter_AddRefs(patternSurface));

  // Push the surface
  if (NS_FAILED(canvas->PushSurface(patternSurface, PR_FALSE)))
    return NS_ERROR_FAILURE; //?

  if (NS_FAILED(GetPatternMatrix(patternMatrix, bbox, viewRect)))
    return NS_ERROR_FAILURE;

  // We really need to have the renderer deal with the pattern offsets, so
  // we sort of fudge things and add in our x and y offsets from our bounding 
  // box
  float xTrans, yTrans;
  (*patternMatrix)->GetE(&xTrans);
  (*patternMatrix)->GetF(&yTrans);
  xTrans += xTranslate + x * xScale;
  yTrans += yTranslate + y * yScale;
  (*patternMatrix)->SetE(xTrans);
  (*patternMatrix)->SetF(yTrans);

#ifdef DEBUG_scooter
  printRect("Bounding Rect: ",mBBox);
  printCTM("Child TM ",*patternMatrix);
#endif

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
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
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
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
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
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsGkAtoms::patternTransform)) {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedTransformList> trans;
    patternElement->GetPatternTransform(getter_AddRefs(trans));
    nsCOMPtr<nsIDOMSVGTransformList> lTrans;
    trans->GetAnimVal(getter_AddRefs(lTrans));
    lTrans->GetConsolidationMatrix(aPatternTransform);
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
    nsCOMPtr<nsIDOMSVGFitToViewBox> patternElement = do_QueryInterface(mContent);
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
    nsCOMPtr<nsIDOMSVGFitToViewBox> patternElement = do_QueryInterface(mContent);
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
  nsCOMPtr<nsIDOMSVGLength> len;
  if (NS_FAILED(GetX(getter_AddRefs(len)))) {
    *aX = 0.0f;
    return NS_ERROR_FAILURE;
  }
  len->GetValue(aX);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetX(nsIDOMSVGLength **aX)
{
  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::x)) {
    // Yes, get it from the target
    mNextPattern->GetX(aX);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLen;
    patternElement->GetX(getter_AddRefs(animLen));
    animLen->GetAnimVal(aX);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetY(float *aY)
{
  nsCOMPtr<nsIDOMSVGLength> len;
  if (NS_FAILED(GetY(getter_AddRefs(len)))) {
    *aY = 0.0;
    return NS_ERROR_FAILURE;
  }
  len->GetValue(aY);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetY(nsIDOMSVGLength **aY)
{
  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::y)) {
    // Yes, get it from the target
    mNextPattern->GetY(aY);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLen;
    patternElement->GetY(getter_AddRefs(animLen));
    animLen->GetAnimVal(aY);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetWidth(float *aWidth)
{
  nsCOMPtr<nsIDOMSVGLength> len;
  if (NS_FAILED(GetWidth(getter_AddRefs(len)))) {
    *aWidth = 0.0f;
    return NS_ERROR_FAILURE;
  }
  len->GetValue(aWidth);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetWidth(nsIDOMSVGLength **aWidth)
{
  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::width)) {
    // Yes, get it from the target
    mNextPattern->GetWidth(aWidth);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLen;
    patternElement->GetWidth(getter_AddRefs(animLen));
    animLen->GetAnimVal(aWidth);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetHeight(float *aHeight)
{
  nsCOMPtr<nsIDOMSVGLength> len;
  if (NS_FAILED(GetHeight(getter_AddRefs(len)))) {
    *aHeight = 0.0f;
    return NS_ERROR_FAILURE;
  }
  len->GetValue(aHeight);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetHeight(nsIDOMSVGLength **aHeight)
{
  // See if we need to get the value from another pattern
  if (checkURITarget(nsGkAtoms::height)) {
    // Yes, get it from the target
    mNextPattern->GetHeight(aHeight);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLen;
    patternElement->GetHeight(getter_AddRefs(animLen));
    animLen->GetAnimVal(aHeight);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
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
  if (NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&nextPattern, targetURI, mContent, GetPresContext()->PresShell()))) {
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
nsSVGPatternFrame::GetPatternRect(nsIDOMSVGRect **patternRect, nsIDOMSVGRect *bbox, nsIContent *content)
{
  // Get our type
  PRUint16 type;
  GetPatternUnits(&type);

  // We need to initialize our box
  float x,y,width,height;

  // Get the pattern x,y,width, and height
  nsCOMPtr<nsIDOMSVGLength> tmpX;
  GetX(getter_AddRefs(tmpX));
  nsCOMPtr<nsIDOMSVGLength> tmpY;
  GetY(getter_AddRefs(tmpY));
  nsCOMPtr<nsIDOMSVGLength> tmpHeight;
  GetHeight(getter_AddRefs(tmpHeight));
  nsCOMPtr<nsIDOMSVGLength> tmpWidth;
  GetWidth(getter_AddRefs(tmpWidth));

  if (type == nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX) {
    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, tmpX, nsSVGUtils::X);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, tmpY, nsSVGUtils::Y);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth, nsSVGUtils::X);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight, nsSVGUtils::Y);
  } else {
    x = nsSVGUtils::UserSpace(content, tmpX, nsSVGUtils::X);
    y = nsSVGUtils::UserSpace(content, tmpY, nsSVGUtils::Y);
    width = nsSVGUtils::UserSpace(content, tmpWidth, nsSVGUtils::X);
    height = nsSVGUtils::UserSpace(content, tmpHeight, nsSVGUtils::Y);
  }

  return NS_NewSVGRect(patternRect, x, y, width, height);
}

nsresult
nsSVGPatternFrame::ConstructCTM(nsIDOMSVGMatrix **resultCTM, nsIDOMSVGMatrix *aPCTM, nsIDOMSVGRect *aViewRect)
{
  // Create a matrix from the parent without any translations
  nsCOMPtr<nsIDOMSVGMatrix> tCTM;
  nsCOMPtr<nsIDOMSVGMatrix>tempTM;
  float a, b, c, d, e, f;
  aPCTM->GetA(&a);
  aPCTM->GetB(&b);
  aPCTM->GetC(&c);
  aPCTM->GetD(&d);
  aPCTM->GetE(&e);
  aPCTM->GetF(&f);
  NS_NewSVGMatrix(getter_AddRefs(tCTM),
                  a,     b,
                  c,     d,
                  0.0f,  0.0f);

  // See if we really have something
  float viewBoxX, viewBoxY, viewBoxHeight, viewBoxWidth;
  aViewRect->GetX(&viewBoxX);
  aViewRect->GetY(&viewBoxY);
  aViewRect->GetHeight(&viewBoxHeight);
  aViewRect->GetWidth(&viewBoxWidth);
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

    tempTM->Multiply(tCTM, resultCTM);
  } else {
    // No viewBox, construct from the (modified) parent matrix
    NS_NewSVGMatrix(getter_AddRefs(tempTM),
                    1.0f,     0.0f,
                    0.0f,     1.0f,
                    0.0f,     0.0f);
    tempTM->Multiply(tCTM, resultCTM);
  }
  return NS_OK;
}

nsresult
nsSVGPatternFrame::GetPatternMatrix(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *bbox, nsIDOMSVGRect *aViewRect)
{
  // Determine the new CTM and set it
  // this depends on the patternContentUnits & viewBox
  nsCOMPtr<nsIDOMSVGMatrix> tCTM;

  // Get the pattern transform
  nsCOMPtr<nsIDOMSVGMatrix> patternTransform;
  GetPatternTransform(getter_AddRefs(patternTransform));
  // OK, now that we're dealing with the content units, we want to
  // get rid of translations (since we're a pattern).  The pattern
  // matrix will translate the entire frame for us when we render
  // the actual pattern

  // See if we really have something
  float viewBoxHeight, viewBoxWidth;
  aViewRect->GetHeight(&viewBoxHeight);
  aViewRect->GetWidth(&viewBoxWidth);
  if (viewBoxHeight != 0.0f && viewBoxWidth != 0.0f) {
    NS_NewSVGMatrix(getter_AddRefs(tCTM), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
  } else {
    // Use whatever patternContentUnits tells us
    PRUint16 type;
    GetPatternContentUnits(&type);

    if (type == nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX) {
      // Use the bounding box
      float bX,bY,bWidth,bHeight;
      bbox->GetX(&bX);
      bbox->GetY(&bY);
      bbox->GetWidth(&bWidth);
      bbox->GetHeight(&bHeight);
      NS_NewSVGMatrix(getter_AddRefs(tCTM), bWidth, 0.0f, 0.0f, bHeight, bX, bY);
    } else {
      NS_NewSVGMatrix(getter_AddRefs(tCTM), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    }
  }
  tCTM->Multiply(patternTransform, ctm);
  return NS_OK;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(nsIPresShell* aPresShell, 
                                nsIContent*   aContent)
{
  nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(aContent);
  NS_ASSERTION(patternElement, 
               "NS_NewSVGPatternFrame -- Content doesn't support nsIDOMSVGPattern");
  if (!patternElement)
    return nsnull;
  
  nsSVGPatternFrame* it = new (aPresShell) nsSVGPatternFrame;
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
