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
#include "nsContainerFrame.h"
#include "nsSVGPattern.h"
#include "nsWeakReference.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGAtoms.h"
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
#include "nsSVGNumber.h"
#include "nsLayoutAtoms.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGRendererSurface.h"
#include "nsSVGUtils.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGContainerFrame.h"
  
typedef nsContainerFrame  nsSVGPatternFrameBase;

#ifdef DEBUG_scooter
static void printCTM(char *msg, nsIDOMSVGMatrix *aCTM);
static void printRect(char *msg, nsIDOMSVGRect *aRect);
#endif

class nsSVGPatternFrame : public nsSVGPatternFrameBase,
                          public nsSVGValue,
                          public nsISVGContainerFrame,
                          public nsISVGValueObserver,
                          public nsSupportsWeakReference,
                          public nsISVGPattern
{
protected:
  friend nsresult NS_NewSVGPatternFrame(nsIPresShell* aPresShell, 
                                        nsIContent*   aContent, 
                                        nsIFrame**    aNewFrame);

  friend nsresult NS_GetSVGPatternFrame(nsIFrame**      result, 
                                        nsIURI*         aURI, 
                                        nsIContent*     aContent,
                                        nsIPresShell*   aPresShell);


public:
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

  // nsIFrame interface:
  NS_IMETHOD DidSetStyleContext(nsPresContext* aPresContext);
  
  // nsISVGContainerFrame interface:
  nsISVGOuterSVGFrame* GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();
  NS_IMETHOD GetBBox(nsIDOMSVGRect **aRect);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgPatternFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
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
  NS_IMETHOD PrivateGetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aEnum);
  NS_IMETHOD PrivateGetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aEnum);
  NS_IMETHOD PrivateGetX(nsIDOMSVGLength * *aValue);
  NS_IMETHOD PrivateGetY(nsIDOMSVGLength * *aValue);
  NS_IMETHOD PrivateGetWidth(nsIDOMSVGLength * *aValue);
  NS_IMETHOD PrivateGetHeight(nsIDOMSVGLength * *aValue);
  NS_IMETHOD PrivateGetViewBox(nsIDOMSVGAnimatedRect * *aMatrix);
  NS_IMETHOD PrivateGetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio **aPreserveAspectRatio);
  NS_IMETHOD GetPreserveAspectRatio(nsIDOMSVGPreserveAspectRatio **aPreserveAspectRatio);
  NS_IMETHOD GetPatternFirstChild(nsIFrame **kid);
  NS_IMETHOD GetViewBox(nsIDOMSVGRect * *aMatrix);
  nsresult GetPatternRect(nsIDOMSVGRect **patternRect, 
                          nsIDOMSVGRect *bbox, nsIContent *content);
  nsresult GetPatternMatrix(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *bbox, nsIDOMSVGRect *aViewRect);
  nsresult ConstructCTM(nsIDOMSVGMatrix **ctm, nsIDOMSVGMatrix *aPCTM, nsIDOMSVGRect *aViewRect);

  //

  nsSVGPatternFrame                     *mNextPattern;
  PRBool                                 mLoopFlag;

private:
  // Cached values
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration>  mPatternUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration>  mPatternContentUnits;
  nsCOMPtr<nsIDOMSVGLength>               mX;
  nsCOMPtr<nsIDOMSVGLength>               mY;
  nsCOMPtr<nsIDOMSVGLength>               mWidth;
  nsCOMPtr<nsIDOMSVGLength>               mHeight;
  nsCOMPtr<nsIDOMSVGRect>                 mBBox;
  nsCOMPtr<nsIDOMSVGAnimatedRect>                 mViewBox;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio>  mPreserveAspectRatio;

  nsCOMPtr<nsISVGGeometrySource>          mSource;
  nsCOMPtr<nsIDOMSVGMatrix>               mCTM;

  nsAutoString                            mNextPatternStr;
};

//----------------------------------------------------------------------
// Implementation

nsSVGPatternFrame::~nsSVGPatternFrame()
{
  WillModify();

  // Notify the world that we're dying
  DidModify(mod_die);

  // Remove observers on pattern attributes
  if (mPatternUnits) NS_REMOVE_SVGVALUE_OBSERVER(mPatternUnits);
  if (mPatternContentUnits) NS_REMOVE_SVGVALUE_OBSERVER(mPatternContentUnits);
  if (mX) NS_REMOVE_SVGVALUE_OBSERVER(mX);
  if (mY) NS_REMOVE_SVGVALUE_OBSERVER(mY);
  if (mWidth) NS_REMOVE_SVGVALUE_OBSERVER(mWidth);
  if (mHeight) NS_REMOVE_SVGVALUE_OBSERVER(mHeight);
  if (mViewBox) NS_REMOVE_SVGVALUE_OBSERVER(mViewBox);
  if (mPreserveAspectRatio) NS_REMOVE_SVGVALUE_OBSERVER(mPreserveAspectRatio);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_INTERFACE_MAP_BEGIN(nsSVGPatternFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGPattern)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
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
nsSVGPatternFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
  WillModify(mod_other);
  DidModify(mod_other);
  return NS_OK;
}

nsIAtom*
nsSVGPatternFrame::GetType() const
{
  return nsLayoutAtoms::svgPatternFrame;
}

//----------------------------------------------------------------------
// nsISVGContainerFrame interface:
nsISVGOuterSVGFrame*
nsSVGPatternFrame::GetOuterSVGFrame() {
  NS_ASSERTION(mParent, "null parent");

  nsISVGContainerFrame *aContainer;
  // Do we have a caller?
  if (mSource) {
    nsCOMPtr<nsISVGChildFrame> callerSVGFrame = do_QueryInterface(mSource);
    NS_ASSERTION(callerSVGFrame, "no SVG frame!");
    nsIFrame *callerFrame;
    CallQueryInterface(callerSVGFrame, &callerFrame);

    // Get the containerFrame from our geometric source
    CallQueryInterface(callerFrame, &aContainer);

    // Did we get one?
    if (!aContainer) {
      // No, try our caller's parent
      CallQueryInterface(callerFrame->GetParent(), &aContainer);
    }

    if (aContainer)
      return aContainer->GetOuterSVGFrame();
  }
  // No, use our parent
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), 
                          (void**)&aContainer);
  if (!aContainer) {
    NS_ERROR("invalid container");
    return nsnull;
  }
  return aContainer->GetOuterSVGFrame();
}

// If our GetCanvasTM is getting called, we
// need to return *our current* transformation
// matrix, which depends on our units parameters
// and X, Y, Width, and Height
already_AddRefed<nsIDOMSVGMatrix> 
nsSVGPatternFrame::GetCanvasTM() {
  nsIDOMSVGMatrix *aCTM;
  
  if (mCTM) {
    aCTM = mCTM;
    NS_IF_ADDREF(aCTM);
  } else {
    // Do we know our rendering parent?
    if (mSource) {
      // Yes, use it!
      mSource->GetCanvasTM(&aCTM);
    } else {
      // No, we'll use our content parent, then
      nsCOMPtr<nsISVGGeometrySource> aSource = do_QueryInterface(mParent); 
      if (aSource) {
        aSource->GetCanvasTM(&aCTM);
      } else {
        // OK, we have no content parent, which means that we're
        // not part of the document tree. Return an identity matrix?
        NS_NewSVGMatrix(&aCTM, 1.0f,0.0f,0.0f,1.0f,0.0f,0.0f);
      }
    }
  }
  return aCTM;  
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

  // Set our geometrical parent
  mSource = aSource;

  // Get the geometric parent CTM to get our scale
  nsCOMPtr<nsISVGChildFrame> callerSVGFrame = do_QueryInterface(mSource);
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

#ifdef DEBUG_scooter
  {
    nsAutoString name;
    nsIFrameDebug*  frameDebug;
    CallQueryInterface(callerFrame, &frameDebug);
    frameDebug->GetFrameName(name);
    char *aStr = ToNewCString(name);

    const char *aContentString;
    nsIAtom *aTag = callerContent->Tag();
    aTag->GetUTF8String(&aContentString);
    printf("PaintPattern -- caller frame is %s, content tag is %s\n",
            aStr, aContentString);
  }
#endif

  nsISVGContainerFrame *aContainer;
  CallQueryInterface(callerFrame, &aContainer);
  nsCOMPtr<nsIDOMSVGMatrix> pCTM;
  if (aContainer) {
    pCTM = aContainer->GetCanvasTM();
  } else {
    mSource->GetCanvasTM(getter_AddRefs(pCTM));
  }

#ifdef DEBUG_scooter
  printCTM("Parent's TM: ",pCTM);
#endif

  /*
   * OK, we've got the content geometry in general.  Now we need to get two
   * different transformation matricies.  First, we need to transform the
   * x,y,width,and heigth units.  This is done differently than the units of
   * the actual elements we are going to paint.  We also need to pass some
   * of this information back to our caller (to get the x,y offset of the
   * pattern within the painted area.  So, we need to generate two matrices
   */
  
  callerSVGFrame->SetMatrixPropagation(PR_FALSE);
  callerSVGFrame->NotifyCanvasTMChanged(PR_TRUE);

  // Get the parent scale values.  We need these to scale our width & height
  // for the surface.
  float xScale, yScale;
  pCTM->GetA(&xScale);
  pCTM->GetD(&yScale);

  // Get our Viewbox (if we have one)
  nsCOMPtr<nsIDOMSVGRect> aViewRect;
  GetViewBox(getter_AddRefs(aViewRect));

  // Construct a CTM for the parent that excludes the translation -- this
  // will become the CanvasTM we provide.  Note that mCTM must take into
  // account the viewBox/Viewport transformations since these can not be
  // handled by the renderer's pattern matrix
  if (NS_FAILED(ConstructCTM(getter_AddRefs(mCTM), pCTM, aViewRect)))
    return NS_ERROR_FAILURE;
#ifdef DEBUG_scooter
  printf("scales: %f %f\n", xScale, yScale);
#endif

  callerSVGFrame->SetMatrixPropagation(PR_FALSE);
  callerSVGFrame->NotifyCanvasTMChanged(PR_TRUE);

  nsCOMPtr<nsIDOMSVGRect> bbox;
  callerSVGFrame->GetBBox(getter_AddRefs(bbox));

#ifdef DEBUG_scooter
  printRect("Parent's bounding Rect: ",bbox);
#endif

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
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  nsCOMPtr<nsISVGRendererSurface> patternSurface;
#ifdef DEBUG_scooter
  printf("Creating %dX%d surface\n",(int)(xScale*width),(int)(yScale*height));
#endif
  renderer->CreateSurface((int)(xScale*width), (int)(yScale*height), 
                          getter_AddRefs(patternSurface));

  // Push the surface
  if (NS_FAILED(canvas->PushSurface(patternSurface)))
    return NS_ERROR_FAILURE; //?

  if (NS_FAILED(GetPatternMatrix(patternMatrix, bbox, aViewRect)))
    return NS_ERROR_FAILURE;

  // We really need to have the renderer deal with the pattern offsets, so
  // we sort of fudge things and add in our x and y offsets from our bounding 
  // box
  float xTrans, yTrans;
  (*patternMatrix)->GetE(&xTrans);
  (*patternMatrix)->GetF(&yTrans);
  xTrans += x;
  yTrans += y;
  (*patternMatrix)->SetE(xTrans*xScale);
  (*patternMatrix)->SetF(yTrans*yScale);

#ifdef DEBUG_scooter
  printRect("Bounding Rect: ",mBBox);
  printCTM("Child TM ",*patternMatrix);
#endif

  callerSVGFrame->SetMatrixPropagation(PR_TRUE);
  callerSVGFrame->NotifyCanvasTMChanged(PR_TRUE);

  // OK, now render -- note that we use "firstKid", which
  // we got at the beginning because it takes care of the
  // referenced pattern situation for us
  nsRect dummyRect;
  for (nsIFrame* kid = firstKid; kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->PaintSVG(canvas, dummyRect, PR_FALSE);
    }
  }

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
  if ((*kid = mFrames.FirstChild()) == nsnull) {
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
  if (!mPatternUnits) {
    PrivateGetPatternUnits(getter_AddRefs(mPatternUnits));
    if (!mPatternUnits)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mPatternUnits);
  }
  mPatternUnits->GetAnimVal(aUnits);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPatternContentUnits(PRUint16 *aUnits)
{
  if (!mPatternContentUnits) {
    PrivateGetPatternContentUnits(getter_AddRefs(mPatternContentUnits));
    if (!mPatternContentUnits)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mPatternContentUnits);
  }
  mPatternContentUnits->GetAnimVal(aUnits);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPatternTransform(nsIDOMSVGMatrix **aPatternTransform)
{
  *aPatternTransform = nsnull;
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> aTrans;
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }

  // See if we need to get the value from another pattern
  if (!checkURITarget(nsSVGAtoms::patternTransform)) {
    // No, return the values
    aPattern->GetPatternTransform(getter_AddRefs(aTrans));
    nsCOMPtr<nsIDOMSVGTransformList> lTrans;
    aTrans->GetAnimVal(getter_AddRefs(lTrans));
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
  if (!mViewBox) {
    PrivateGetViewBox(getter_AddRefs(mViewBox));
    if (!mViewBox)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mViewBox);
  }
#ifdef DEBUG_scooter
  printf("Getting viewbox from stored value\n");
#endif
  mViewBox->GetAnimVal(aViewBox);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetPreserveAspectRatio(nsIDOMSVGPreserveAspectRatio 
                                                   **aPreserveAspectRatio)
{
  if (!mPreserveAspectRatio) {
    PrivateGetPreserveAspectRatio(getter_AddRefs(mPreserveAspectRatio));
    if (!mPreserveAspectRatio)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mPreserveAspectRatio);
  }
  mPreserveAspectRatio->GetAnimVal(aPreserveAspectRatio);
  return NS_OK;
}


NS_IMETHODIMP
nsSVGPatternFrame::GetX(float *aX)
{
  if (!mX) {
    PrivateGetX(getter_AddRefs(mX));
    if (!mX)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mX);
  }
  mX->GetValue(aX);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetY(float *aY)
{
  if (!mY) {
    PrivateGetY(getter_AddRefs(mY));
    if (!mY)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mY);
  }
  mY->GetValue(aY);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetWidth(float *aWidth)
{
  if (!mWidth) {
    PrivateGetWidth(getter_AddRefs(mWidth));
    if (!mWidth)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mWidth);
  }
  mWidth->GetValue(aWidth);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetHeight(float *aHeight)
{
  if (!mHeight) {
    PrivateGetHeight(getter_AddRefs(mHeight));
    if (!mHeight)
      return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mHeight);
  }
  mHeight->GetValue(aHeight);
  return NS_OK;
}

// -------------------------------------------------------------
// Protected versions of the various "Get" routines.  These need
// to be used to allow for the ability to delegate to referenced
// patterns
// -------------------------------------------------------------
NS_IMETHODIMP
nsSVGPatternFrame::PrivateGetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aEnum)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsSVGAtoms::patternUnits)) {
    // No, return the values
    aPattern->GetPatternUnits(aEnum);
  } else {
    // Yes, get it from the target
    mNextPattern->PrivateGetPatternUnits(aEnum);
  }  
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPatternFrame::PrivateGetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aEnum)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (!checkURITarget(nsSVGAtoms::patternContentUnits)) {
    // No, return the values
    aPattern->GetPatternContentUnits(aEnum);
  } else {
    // Yes, get it from the target
    mNextPattern->PrivateGetPatternContentUnits(aEnum);
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
  nsIFrame *aNextPattern;
  mLoopFlag = PR_TRUE; // Set our loop detection flag
  // Have we already figured out the next Pattern?
  if (mNextPattern != nsnull) {
    return PR_TRUE;
  }

  // Do we have URI?
  if (mNextPatternStr.Length() == 0) {
    return PR_FALSE; // No, return the default
  }

  // Get the Pattern
  nsCAutoString aPatternStr;
  CopyUTF16toUTF8(mNextPatternStr, aPatternStr);
  // Note that we are using *our* frame tree for this call, 
  // otherwise we're going to have to get the PresShell in each call
  if (nsSVGUtils::GetReferencedFrame(&aNextPattern, aPatternStr, mContent, GetPresContext()->PresShell()) == NS_OK) {
    nsIAtom* frameType = aNextPattern->GetType();
    if (frameType != nsLayoutAtoms::svgPatternFrame)
      return PR_FALSE;
    mNextPattern = (nsSVGPatternFrame *)aNextPattern;
    // Are we looping?
    if (mNextPattern->mLoopFlag) {
      // Yes, remove the reference and return an error
      NS_WARNING("Pattern loop detected!");
      CopyUTF8toUTF16("", mNextPatternStr);
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

nsresult
nsSVGPatternFrame::PrivateGetX(nsIDOMSVGLength * *aX)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (checkURITarget(nsSVGAtoms::x)) {
    // Yes, get it from the target
    mNextPattern->PrivateGetX(aX);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
    aPattern->GetX(getter_AddRefs(aLen));
    aLen->GetAnimVal(aX);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

nsresult
nsSVGPatternFrame::PrivateGetY(nsIDOMSVGLength * *aY)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (checkURITarget(nsSVGAtoms::y)) {
    // Yes, get it from the target
    mNextPattern->PrivateGetY(aY);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
    aPattern->GetY(getter_AddRefs(aLen));
    aLen->GetAnimVal(aY);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

nsresult
nsSVGPatternFrame::PrivateGetHeight(nsIDOMSVGLength * *aHeight)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (checkURITarget(nsSVGAtoms::height)) {
    // Yes, get it from the target
    mNextPattern->PrivateGetHeight(aHeight);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
    aPattern->GetHeight(getter_AddRefs(aLen));
    aLen->GetAnimVal(aHeight);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

nsresult
nsSVGPatternFrame::PrivateGetWidth(nsIDOMSVGLength * *aWidth)
{
  nsCOMPtr<nsIDOMSVGPatternElement> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }
  // See if we need to get the value from another pattern
  if (checkURITarget(nsSVGAtoms::width)) {
    // Yes, get it from the target
    mNextPattern->PrivateGetWidth(aWidth);
  } else {
    // No, return the values
    nsCOMPtr<nsIDOMSVGAnimatedLength> aLen;
    aPattern->GetWidth(getter_AddRefs(aLen));
    aLen->GetAnimVal(aWidth);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

nsresult
nsSVGPatternFrame::PrivateGetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = nsnull;
  nsCOMPtr<nsIDOMSVGFitToViewBox> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }

  // See if we need to get the value from another pattern
  if (!checkURITarget(nsSVGAtoms::viewBox)) {
#ifdef DEBUG_scooter
    printf("Getting viewbox from content\n");
#endif
    // No, return the values
    aPattern->GetViewBox(aViewBox);
  } else {
#ifdef DEBUG_scooter
    printf("Getting viewbox from referenced pattern");
#endif
    // Yes, get it from the target
    mNextPattern->PrivateGetViewBox(aViewBox);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
}

nsresult
nsSVGPatternFrame::PrivateGetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio * *aPreserveAspectRatio)
{
  *aPreserveAspectRatio = nsnull;
  nsCOMPtr<nsIDOMSVGFitToViewBox> aPattern = do_QueryInterface(mContent);
  NS_ASSERTION(aPattern, "Wrong content element (not pattern)");
  if (aPattern == nsnull) {
    return NS_ERROR_FAILURE;
  }

  // See if we need to get the value from another pattern
  if (!checkURITarget(nsSVGAtoms::preserveAspectRatio)) {
#ifdef DEBUG_scooter
    printf("Getting preserveAspectRatio from content\n");
#endif
    // No, return the values
    aPattern->GetPreserveAspectRatio(aPreserveAspectRatio);
  } else {
#ifdef DEBUG_scooter
    printf("Getting preserveAspectRatio from referenced pattern");
#endif
    // Yes, get it from the target
    mNextPattern->PrivateGetPreserveAspectRatio(aPreserveAspectRatio);
  }
  mLoopFlag = PR_FALSE;
  return NS_OK;
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
  float x,y,width,height,tmp;
  if (!mX)
    GetX(&tmp);
  if (!mY)
    GetY(&tmp);
  if (!mWidth)
    GetWidth(&tmp);
  if (!mHeight)
    GetHeight(&tmp);
  if (type == nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX) {
    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, mX, nsSVGUtils::X);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, mY, nsSVGUtils::Y);
    width = nsSVGUtils::ObjectSpace(bbox, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::ObjectSpace(bbox, mHeight, nsSVGUtils::Y);
  } else {
    x = nsSVGUtils::UserSpace(content, mX, nsSVGUtils::X);
    y = nsSVGUtils::UserSpace(content, mY, nsSVGUtils::Y);
    width = nsSVGUtils::UserSpace(content, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::UserSpace(content, mHeight, nsSVGUtils::Y);
  }

  return NS_NewSVGRect(patternRect, x, y, width, height);
}

nsresult
nsSVGPatternFrame::ConstructCTM(nsIDOMSVGMatrix **ctm, nsIDOMSVGMatrix *aPCTM, nsIDOMSVGRect *aViewRect)
{
  // Create a matrix for the parent without any translations
  nsCOMPtr<nsIDOMSVGMatrix> aCTM;
  nsCOMPtr<nsIDOMSVGMatrix>aTempTM;
  float a, b, c, d;
  aPCTM->GetA(&a);
  aPCTM->GetB(&b);
  aPCTM->GetC(&c);
  aPCTM->GetD(&d);
  NS_NewSVGMatrix(getter_AddRefs(aCTM),
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
    // Use the viewbox to calculate the new matrix
    PRUint16 align, meetOrSlice;
    {
      nsCOMPtr<nsIDOMSVGPreserveAspectRatio> par;
      GetPreserveAspectRatio(getter_AddRefs(par));
      NS_ASSERTION(par, "could not get preserveAspectRatio");
      par->GetAlign(&align);
      par->GetMeetOrSlice(&meetOrSlice);
    }

    // default to the defaults
    if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
    if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;

    float viewportWidth, viewportHeight;
    GetWidth(&viewportWidth);
    GetHeight(&viewportHeight);

    float a, d, e, f;
    a = viewportWidth/viewBoxWidth;
    d = viewportHeight/viewBoxHeight;
    e = 0.0f;
    f = 0.0f;

    if (align != nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
        a != d) {
      if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
          a < d ||
          meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
          d < a) {
        d = a;
      }
      else if (
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
        d < a ||
        meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
        a < d) {
        a = d;
      }
      else NS_NOTREACHED("Unknown value for meetOrSlice");
    }

    if (viewBoxX) e += -a * viewBoxX;
    if (viewBoxY) f += -d * viewBoxY;

    float refX, refY;
    GetX(&refX);
    GetY(&refY);

    e -= refX * a;
    f -= refY * d;

#ifdef DEBUG
    printf("Pattern Viewport=(0?,0?,%f,%f)\n", viewportWidth, viewportHeight);
    printf("Pattern Viewbox=(%f,%f,%f,%f)\n", viewBoxX, viewBoxY, viewBoxWidth, viewBoxHeight);
    printf("Pattern Viewbox->Viewport xform [a c e] = [%f,   0, %f]\n", a, e);
    printf("                            [b d f] = [   0,  %f, %f]\n", d, f);
#endif

    NS_NewSVGMatrix(getter_AddRefs(aTempTM),
                    a,     0.0f,
                    0.0f,  d,
                    e,     f);
    aTempTM->Multiply(aCTM, ctm);
  } else {
    // No viewBox, construct from the (modified) parent matrix
    NS_NewSVGMatrix(getter_AddRefs(aTempTM),
                    1.0f,     0.0f,
                    0.0f,     1.0f,
                    0.0f,     0.0f);
    aTempTM->Multiply(aCTM, ctm);
  }
  return NS_OK;
}

nsresult
nsSVGPatternFrame::GetPatternMatrix(nsIDOMSVGMatrix **ctm, nsIDOMSVGRect *bbox, nsIDOMSVGRect *aViewRect)
{
  // Determine the new CTM and set it
  // this depends on the patternContentUnits & viewBox
  nsCOMPtr<nsIDOMSVGMatrix> aCTM;

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
    NS_NewSVGMatrix(getter_AddRefs(aCTM), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
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
#ifdef DEBUG_scooter
      printf("PaintPattern -- constructing CTM from bounding box\n");
#endif
      NS_NewSVGMatrix(getter_AddRefs(aCTM), bWidth, 0.0f, 0.0f, bHeight, bX, bY);
    } else {
#ifdef DEBUG_scooter
      printf("PaintPattern -- constructing CTM from parent CTM\n");
#endif
      NS_NewSVGMatrix(getter_AddRefs(aCTM), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    }
  }
  aCTM->Multiply(patternTransform, ctm);
  return NS_OK;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsresult NS_NewSVGPatternFrame(nsIPresShell* aPresShell, 
                               nsIContent*   aContent, 
                               nsIFrame**    aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGPatternElement> pattern = do_QueryInterface(aContent);
  NS_ASSERTION(pattern, 
               "NS_NewSVGPatternFrame -- Content doesn't support nsIDOMSVGPattern");
  if (!pattern)
    return NS_ERROR_FAILURE;
  
  nsSVGPatternFrame* it = new (aPresShell) nsSVGPatternFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMSVGURIReference> aRef = do_QueryInterface(aContent);
  NS_ASSERTION(aRef, 
               "NS_NewSVGPatternFrame -- Content doesn't support nsIDOMSVGURIReference");
  if (!aRef) {
    it->mNextPattern = nsnull;
  } else {
    // Get the hRef
    nsCOMPtr<nsIDOMSVGAnimatedString> aHref;
    aRef->GetHref(getter_AddRefs(aHref));

    nsAutoString aStr;
    aHref->GetAnimVal(aStr);
    it->mNextPatternStr = aStr;
    it->mNextPattern = nsnull;
#ifdef DEBUG_scooter
    printf("NS_NewSVGPatternFrame\n");
#endif
  }

  *aNewFrame = it;

  return NS_OK;
}

// Public function to locate the SVGPatternFrame structure pointed to by a URI
// and return a nsISVGPattern
nsresult NS_GetSVGPattern(nsISVGPattern **aPattern, nsIURI *aURI, 
                          nsIContent *aContent, 
                          nsIPresShell *aPresShell)
{
  *aPattern = nsnull;

  // Get the spec from the URI
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);
#ifdef scooter_DEBUG
  printf("NS_GetSVGGradient: uri = %s\n",uriSpec.get());
#endif
  nsIFrame *result;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&result, 
                                                   uriSpec, 
                                                   aContent, aPresShell)))
    return NS_ERROR_FAILURE;
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
