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

#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGLocatable.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGLength.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsISVGViewportRect.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGMatrix.h"
#include "nsSVGPoint.h"
#include "nsSVGTransform.h"
#include "nsIDOMEventTarget.h"
#include "nsIViewManager.h"
#include "nsIBindingManager.h"
#include "nsIWidget.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsISVGSVGElement.h"
#include "nsISVGViewportAxis.h"
#include "nsISVGOuterSVGFrame.h" //XXX
#include "nsSVGNumber.h"
#include "nsSVGRect.h"
#include "nsISVGValueUtils.h"

typedef nsSVGStylableElement nsSVGSVGElementBase;

class nsSVGSVGElement : public nsSVGSVGElementBase,
                        public nsISVGSVGElement, // : nsIDOMSVGSVGElement
                        public nsIDOMSVGFitToViewBox,
                        public nsIDOMSVGLocatable
{
protected:
  friend nsresult NS_NewSVGSVGElement(nsIContent **aResult,
                                      nsINodeInfo *aNodeInfo);
  nsSVGSVGElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGSVGElement();
  nsresult Init();
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSVGELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX
  NS_DECL_NSIDOMSVGLOCATABLE
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSVGElementBase::)

  // nsISVGSVGElement interface:
  NS_IMETHOD GetParentViewportRect(nsISVGViewportRect **parentViewport);

  // nsIStyledContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
protected:
  
  // implementation helpers:
  void GetScreenPosition(PRInt32 &x, PRInt32 &y);
  
  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;
  nsCOMPtr<nsISVGViewportRect>      mParentViewport;
  nsCOMPtr<nsISVGViewportRect>      mViewport;
  nsCOMPtr<nsIDOMSVGAnimatedRect>   mViewBox;
  nsCOMPtr<nsIDOMSVGMatrix>         mViewBoxToViewportTransform;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;
  
  PRInt32 mRedrawSuspendCount;
};


NS_IMPL_NS_NEW_SVG_ELEMENT(SVG)


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFitToViewBox)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLocatable)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGSVGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSVGElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSVGElement::nsSVGSVGElement(nsINodeInfo* aNodeInfo)
  : nsSVGSVGElementBase(aNodeInfo), mRedrawSuspendCount(0)
{
}

nsSVGSVGElement::~nsSVGSVGElement()
{
  if (mViewBox) {
    NS_REMOVE_SVGVALUE_OBSERVER(mViewBox);
  }
  if (mViewport) {
    NS_REMOVE_SVGVALUE_OBSERVER(mViewport);
  }
}

  
nsresult
nsSVGSVGElement::Init()
{
  nsresult rv = nsSVGSVGElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // parent viewport. this will be initialized properly by our frame.
  {
    nsCOMPtr<nsIDOMSVGNumber> scalex;
    nsCOMPtr<nsIDOMSVGNumber> scaley;
    nsCOMPtr<nsIDOMSVGNumber> lengthx;
    nsCOMPtr<nsIDOMSVGNumber> lengthy;
    rv = NS_NewSVGNumber(getter_AddRefs(scalex));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(scaley));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(lengthx));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(lengthy));
    NS_ENSURE_SUCCESS(rv,rv);
    
    rv = NS_NewSVGViewportRect(getter_AddRefs(mParentViewport),
                               scalex, scaley, lengthx, lengthy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  
  // nsIDOMSVGSVGElement attributes ------:
  
  // DOM property: width ,  #IMPLIED attrib: width
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsISVGViewportAxis> ctx;
    mParentViewport->GetXAxis(getter_AddRefs(ctx));    
    length->SetContext(ctx);
    
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mWidth), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::width, mWidth);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  // DOM property: height , #IMPLIED attrib: height
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsISVGViewportAxis> ctx;
    mParentViewport->GetYAxis(getter_AddRefs(ctx));    
    length->SetContext(ctx);

    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  // readonly DOM property: viewport
  {
    nsCOMPtr<nsIDOMSVGNumber> scalex;
    nsCOMPtr<nsIDOMSVGNumber> scaley;
    nsCOMPtr<nsIDOMSVGNumber> lengthx;
    nsCOMPtr<nsIDOMSVGNumber> lengthy;
    rv = NS_NewSVGNumber(getter_AddRefs(scalex));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(scaley));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(lengthx));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGNumber(getter_AddRefs(lengthy));
    NS_ENSURE_SUCCESS(rv,rv);
    
    rv = NS_NewSVGViewportRect(getter_AddRefs(mViewport),
                               scalex, scaley, lengthx, lengthy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);

    // XXXXXXX
//     nsCOMPtr<nsISVGLengthContext> ctx;
//     GetLengthContextX(getter_AddRefs(ctx));    
//     length->SetContext(ctx);

    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x, mX);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y ,  #IMPLIED attrib: y
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);

    // XXXXXXX
//     nsCOMPtr<nsISVGLengthContext> ctx;
//     GetLengthContextY(getter_AddRefs(ctx));    
//     length->SetContext(ctx);

    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // nsIDOMSVGFitToViewBox attributes ------:
  
  // DOM property: viewBox , #IMPLIED attrib: viewBox
  {
    //      -----------------
    //     | SVGAnimatedRect |
    //      -----------------
    //             < >
    //              |
    //  -------------------------
    // | SVGRectPrototypeWrapper |
    //  -------------------------
    //             < >
    //              |
    //              | prototype
    //         -----------
    //        | mViewport |
    //         -----------
    
    nsCOMPtr<nsIDOMSVGRect> wrapperRect;
    rv = NS_NewSVGRectPrototypeWrapper(getter_AddRefs(wrapperRect), mViewport);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), wrapperRect);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
  }


  // add observers -------------------------- :
  NS_ADD_SVGVALUE_OBSERVER(mViewport);
  NS_ADD_SVGVALUE_OBSERVER(mViewBox);

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_SVG_DOM_CLONENODE(SVG)


//----------------------------------------------------------------------
// nsIDOMSVGSVGElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP
nsSVGSVGElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = mX;
  NS_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP
nsSVGSVGElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = mY;
  NS_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP
nsSVGSVGElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = mWidth;
  NS_ADDREF(*aWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP
nsSVGSVGElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = mHeight;
  NS_ADDREF(*aHeight);
  return NS_OK;
}

/* attribute DOMString contentScriptType; */
NS_IMETHODIMP
nsSVGSVGElement::GetContentScriptType(nsAString & aContentScriptType)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetContentScriptType(const nsAString & aContentScriptType)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute DOMString contentStyleType; */
NS_IMETHODIMP
nsSVGSVGElement::GetContentStyleType(nsAString & aContentStyleType)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetContentStyleType(const nsAString & aContentStyleType)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGRect viewport; */
NS_IMETHODIMP
nsSVGSVGElement::GetViewport(nsIDOMSVGRect * *aViewport)
{
  *aViewport = mViewport;
  NS_ADDREF(*aViewport);
  return NS_OK;
}

/* readonly attribute float pixelUnitToMillimeterX; */
NS_IMETHODIMP
nsSVGSVGElement::GetPixelUnitToMillimeterX(float *aPixelUnitToMillimeterX)
{
  // to correctly determine this, the caller would need to pass in the
  // right PresContext...

  *aPixelUnitToMillimeterX = 0.28f; // 90dpi

  if (!mDocument) return NS_OK;
  // Get Presentation shell 0
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  if (!presShell) return NS_OK;
  
  // Get the Presentation Context from the Shell
  nsCOMPtr<nsIPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));
  if (!context) return NS_OK;

  float TwipsPerPx;
  context->GetScaledPixelsToTwips(&TwipsPerPx);
  *aPixelUnitToMillimeterX = TwipsPerPx / TWIPS_PER_POINT_FLOAT / (72.0f * 0.03937f);
  return NS_OK;
}

/* readonly attribute float pixelUnitToMillimeterY; */
NS_IMETHODIMP
nsSVGSVGElement::GetPixelUnitToMillimeterY(float *aPixelUnitToMillimeterY)
{
  return GetPixelUnitToMillimeterX(aPixelUnitToMillimeterY);
}

/* readonly attribute float screenPixelToMillimeterX; */
NS_IMETHODIMP
nsSVGSVGElement::GetScreenPixelToMillimeterX(float *aScreenPixelToMillimeterX)
{
  // to correctly determine this, the caller would need to pass in the
  // right PresContext...

  *aScreenPixelToMillimeterX = 0.28f; // 90dpi

  if (!mDocument) return NS_OK;
    // Get Presentation shell 0
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  if (!presShell) return NS_OK;
  
  // Get the Presentation Context from the Shell
  nsCOMPtr<nsIPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));
  if (!context) return NS_OK;

  float TwipsPerPx;
  TwipsPerPx = context->PixelsToTwips();
  *aScreenPixelToMillimeterX = TwipsPerPx / TWIPS_PER_POINT_FLOAT / (72.0f * 0.03937f);
  return NS_OK;
}

/* readonly attribute float screenPixelToMillimeterY; */
NS_IMETHODIMP
nsSVGSVGElement::GetScreenPixelToMillimeterY(float *aScreenPixelToMillimeterY)
{
  return GetScreenPixelToMillimeterX(aScreenPixelToMillimeterY);
}

/* attribute boolean useCurrentView; */
NS_IMETHODIMP
nsSVGSVGElement::GetUseCurrentView(PRBool *aUseCurrentView)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetUseCurrentView(PRBool aUseCurrentView)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGViewSpec currentView; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentView(nsIDOMSVGViewSpec * *aCurrentView)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute float currentScale; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentScale(float *aCurrentScale)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetCurrentScale(float aCurrentScale)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGPoint currentTranslate; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTranslate(nsIDOMSVGPoint * *aCurrentTranslate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long suspendRedraw (in unsigned long max_wait_milliseconds); */
NS_IMETHODIMP
nsSVGSVGElement::SuspendRedraw(PRUint32 max_wait_milliseconds, PRUint32 *_retval)
{
  *_retval = 1;

  if (++mRedrawSuspendCount > 1) 
    return NS_OK;
  
  if (!mDocument) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  NS_ASSERTION(presShell, "need presShell to suspend redraw");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);
#ifdef DEBUG
  // XXX We sometimes hit this assertion when the svg:svg element is
  // in a binding and svg children are inserted underneath it using
  // <children/>. If the svg children then call suspendRedraw, the
  // above function call fails although the svg:svg's frame has been
  // build. Strange...
  
  NS_ASSERTION(frame, "suspending redraw w/o frame");
#endif
  if (frame) {
    nsISVGOuterSVGFrame* svgframe;
    CallQueryInterface(frame, &svgframe);
    NS_ASSERTION(svgframe, "wrong frame type");
    if (svgframe) {
      svgframe->SuspendRedraw();
    }
  }
  
  return NS_OK;
}

/* void unsuspendRedraw (in unsigned long suspend_handle_id); */
NS_IMETHODIMP
nsSVGSVGElement::UnsuspendRedraw(PRUint32 suspend_handle_id)
{
  if (mRedrawSuspendCount == 0) {
    NS_ASSERTION(1==0, "unbalanced suspend/unsuspend calls");
    return NS_ERROR_FAILURE;
  }
                 
  if (mRedrawSuspendCount > 1) {
    --mRedrawSuspendCount;
    return NS_OK;
  }
  
  return UnsuspendRedrawAll();
}

/* void unsuspendRedrawAll (); */
NS_IMETHODIMP
nsSVGSVGElement::UnsuspendRedrawAll()
{
  mRedrawSuspendCount = 0;
  
  if (!mDocument) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = mDocument->GetShellAt(0);
  NS_ASSERTION(presShell, "need presShell to unsuspend redraw");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);
#ifdef DEBUG
  NS_ASSERTION(frame, "unsuspending redraw w/o frame");
#endif
  if (frame) {
    nsISVGOuterSVGFrame* svgframe;
    CallQueryInterface(frame, &svgframe);
    NS_ASSERTION(svgframe, "wrong frame type");
    if (svgframe) {
      svgframe->UnsuspendRedraw();
    }
  }  
  return NS_OK;
}

/* void forceRedraw (); */
NS_IMETHODIMP
nsSVGSVGElement::ForceRedraw()
{
  if (!mDocument) return NS_ERROR_FAILURE;

  nsIPresShell *presShell = mDocument->GetShellAt(0);
  NS_ASSERTION(presShell, "need presShell to unsuspend redraw");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIViewManager* vm = presShell->GetViewManager();
  NS_ASSERTION(vm, "need viewmanager to unsuspend redraw");
  if (!vm) return NS_ERROR_FAILURE;

  vm->UpdateAllViews(NS_VMREFRESH_IMMEDIATE);

  return NS_OK;
}

/* void pauseAnimations (); */
NS_IMETHODIMP
nsSVGSVGElement::PauseAnimations()
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unpauseAnimations (); */
NS_IMETHODIMP
nsSVGSVGElement::UnpauseAnimations()
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean animationsPaused (); */
NS_IMETHODIMP
nsSVGSVGElement::AnimationsPaused(PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* float getCurrentTime (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTime(float *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCurrentTime (in float seconds); */
NS_IMETHODIMP
nsSVGSVGElement::SetCurrentTime(float seconds)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMNodeList getIntersectionList (in nsIDOMSVGRect rect, in nsIDOMSVGElement referenceElement); */
NS_IMETHODIMP
nsSVGSVGElement::GetIntersectionList(nsIDOMSVGRect *rect, nsIDOMSVGElement *referenceElement, nsIDOMNodeList **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMNodeList getEnclosureList (in nsIDOMSVGRect rect, in nsIDOMSVGElement referenceElement); */
NS_IMETHODIMP
nsSVGSVGElement::GetEnclosureList(nsIDOMSVGRect *rect, nsIDOMSVGElement *referenceElement, nsIDOMNodeList **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean checkIntersection (in nsIDOMSVGElement element, in nsIDOMSVGRect rect); */
NS_IMETHODIMP
nsSVGSVGElement::CheckIntersection(nsIDOMSVGElement *element, nsIDOMSVGRect *rect, PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean checkEnclosure (in nsIDOMSVGElement element, in nsIDOMSVGRect rect); */
NS_IMETHODIMP
nsSVGSVGElement::CheckEnclosure(nsIDOMSVGElement *element, nsIDOMSVGRect *rect, PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deSelectAll (); */
NS_IMETHODIMP
nsSVGSVGElement::DeSelectAll()
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGNumber createSVGNumber (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGNumber(nsIDOMSVGNumber **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGLength createSVGLength (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGLength(nsIDOMSVGLength **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGAngle createSVGAngle (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGAngle(nsIDOMSVGAngle **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPoint createSVGPoint (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGPoint(nsIDOMSVGPoint **_retval)
{
  return nsSVGPoint::Create(0.0f, 0.0f, _retval);
}

/* nsIDOMSVGMatrix createSVGMatrix (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGMatrix(nsIDOMSVGMatrix **_retval)
{
  return nsSVGMatrix::Create(_retval);
}

/* nsIDOMSVGRect createSVGRect (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGRect(nsIDOMSVGRect **_retval)
{
  return NS_NewSVGRect(_retval);
}

/* nsIDOMSVGTransform createSVGTransform (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGTransform(nsIDOMSVGTransform **_retval)
{
  return NS_NewSVGTransform(_retval);
}

/* nsIDOMSVGTransform createSVGTransformFromMatrix (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGTransformFromMatrix(nsIDOMSVGMatrix *matrix, nsIDOMSVGTransform **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString createSVGString (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGString(nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMElement getElementById (in DOMString elementId); */
NS_IMETHODIMP
nsSVGSVGElement::GetElementById(const nsAString & elementId, nsIDOMElement **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGMatrix getViewboxToViewportTransform (); */
NS_IMETHODIMP
nsSVGSVGElement::GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval)
{
  if (!mViewBoxToViewportTransform) {
    float viewportWidth, viewportHeight;
    mViewport->GetWidth(&viewportWidth);
    mViewport->GetHeight(&viewportHeight);
    
    float viewboxWidth, viewboxHeight, viewboxX, viewboxY;
    {
      nsCOMPtr<nsIDOMSVGRect> vb;
      mViewBox->GetAnimVal(getter_AddRefs(vb));
      NS_ASSERTION(vb, "could not get viewbox");
      vb->GetWidth(&viewboxWidth);
      vb->GetHeight(&viewboxHeight);
      vb->GetX(&viewboxX);
      vb->GetY(&viewboxY);
    }
    if (viewboxWidth==0.0f || viewboxHeight==0.0f) {
      NS_ERROR("XXX. We shouldn't get here. Viewbox width/height is set to 0. Need to disable display of element as per specs.");
      viewboxWidth = 1.0f;
      viewboxHeight = 1.0f;
    }
    
    // case with preserveAspectRatio=none:
    float a, e, d, f;
    
    a = viewportWidth/viewboxWidth;
    e = -a*viewboxX;
    
    d = viewportHeight/viewboxHeight;
    f = -d*viewboxY;
    
#ifdef DEBUG
    printf("SVG Viewport=(0?,0?,%f,%f)\n", viewportWidth, viewportHeight);
    printf("SVG Viewbox=(%f,%f,%f,%f)\n", viewboxX, viewboxY, viewboxWidth, viewboxHeight);
    printf("SVG Viewbox->Viewport xform [a c e] = [%f,   0, %f]\n", a, e);
    printf("                            [b d f] = [   0,  %f, %f]\n", d, f);
#endif
    
    nsSVGMatrix::Create(getter_AddRefs(mViewBoxToViewportTransform),
                                       a, 0, 0, d, e, f);
  }

  *_retval = mViewBoxToViewportTransform;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP
nsSVGSVGElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = mViewBox;
  NS_ADDREF(*aViewBox);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGSVGElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio * *aPreserveAspectRatio)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIDOMSVGLocatable methods

/* readonly attribute nsIDOMSVGElement nearestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetNearestViewportElement(nsIDOMSVGElement * *aNearestViewportElement)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP
nsSVGSVGElement::GetBBox(nsIDOMSVGRect **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGMatrix getCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCTM(nsIDOMSVGMatrix **_retval)
{
  nsCOMPtr<nsIDOMSVGMatrix> CTM;

  nsIBindingManager *bindingManager = nsnull;
  if (mDocument) {
    bindingManager = mDocument->GetBindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }
  
  while (parent) {
    nsCOMPtr<nsIDOMSVGSVGElement> viewportElement = do_QueryInterface(parent);
    if (viewportElement) {
      // Our nearest SVG parent is a viewport element.
      viewportElement->GetViewboxToViewportTransform(getter_AddRefs(CTM));
      break; 
    }
    
    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
    if (locatableElement) {
      // Our nearest SVG parent is a locatable object that is not a
      // viewport. Its GetCTM function will give us a ctm from the
      // viewport to itself:
      locatableElement->GetCTM(getter_AddRefs(CTM));
      break;
    }

    // Our parent was not svg content. We allow interdispersed non-SVG
    // content to coexist with XBL. Loop until we find the first SVG
    // parent.
    
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }

    parent = next;
  }

  if (!CTM) {
    // We either didn't find an SVG parent, or our parent failed in
    // giving us a CTM. In either case:
    nsSVGMatrix::Create(getter_AddRefs(CTM));
  }
  
  // XXX do we have to append our viewboxToViewport  transformation?
  // if we do we have to change nsSVGGraphicElement::GetCTM()
  
  *_retval = CTM;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGMatrix getScreenCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetScreenCTM(nsIDOMSVGMatrix **_retval)
{
  nsCOMPtr<nsIDOMSVGMatrix> screenCTM;

  nsIBindingManager *bindingManager = nsnull;
  if (mDocument) {
    bindingManager = mDocument->GetBindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }
  
  while (parent) {
    
    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(parent);
    if (locatableElement) {
      nsCOMPtr<nsIDOMSVGMatrix> ctm;
      locatableElement->GetScreenCTM(getter_AddRefs(ctm));
      if (!ctm) {
        NS_ERROR("couldn't get CTM");
        break;
      }
      
      nsCOMPtr<nsIDOMSVGSVGElement> viewportElement = do_QueryInterface(parent);
      if (viewportElement) {
        // It is a viewport element. we need to append the viewbox xform:
        nsCOMPtr<nsIDOMSVGMatrix> matrix;
        viewportElement->GetViewboxToViewportTransform(getter_AddRefs(matrix));
        ctm->Multiply(matrix, getter_AddRefs(screenCTM));
      }
      else
        screenCTM = ctm;

      break;
    }

    // Our parent was not svg content. We allow interdispersed non-SVG
    // content to coexist with XBL. Loop until we find the first SVG
    // parent.
    
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }

    parent = next;
  }

  if (!screenCTM) {
    // We either didn't find an SVG parent, or our parent failed in
    // giving us a CTM.
    // In either case, we'll just assume that we are the outermost element:
    nsCOMPtr<nsIDOMSVGMatrix> matrix;
    nsSVGMatrix::Create(getter_AddRefs(matrix));
    PRInt32 x, y;
    GetScreenPosition(x, y);
    matrix->Translate((float)x, (float)y, getter_AddRefs(screenCTM));
  }

  // XXX do we have to append our viewboxToViewport  transformation?
  // if we do we have to change nsSVGGraphicElement::GetScreenCTM()
  
  *_retval = screenCTM;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGMatrix getTransformToElement (in nsIDOMSVGElement element); */
NS_IMETHODIMP
nsSVGSVGElement::GetTransformToElement(nsIDOMSVGElement *element, nsIDOMSVGMatrix **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
// nsISVGSVGElement methods:

NS_IMETHODIMP
nsSVGSVGElement::GetParentViewportRect(nsISVGViewportRect **parentViewport)
{
  *parentViewport = mParentViewport;
  NS_IF_ADDREF(*parentViewport);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP_(PRBool)
nsSVGSVGElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFillStrokeMap,
    sGraphicsMap,
    sTextContentElementsMap,
    sFontSpecificationMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGSVGElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGSVGElement::WillModifySVGObservable(nsISVGValue* observable)
{
#ifdef DEBUG
  printf("viewport/viewbox will be changed\n");
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsSVGSVGElement::DidModifySVGObservable (nsISVGValue* observable)
{
  // either viewport or viewbox have changed
  // invalidate viewbox -> viewport xform & inform frames
  
  mViewBoxToViewportTransform = nsnull;
  
  if (!mDocument) return NS_ERROR_FAILURE;
  nsIPresShell* presShell = mDocument->GetShellAt(0);
  NS_ASSERTION(presShell, "no presShell");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);
  if (frame) {
    nsISVGOuterSVGFrame* svgframe;
    CallQueryInterface(frame, &svgframe);
    NS_ASSERTION(svgframe, "wrong frame type");
    if (svgframe) {
      svgframe->NotifyViewportChange();
    }
  }
  
  
#ifdef DEBUG
  printf("viewport/viewbox have been changed\n");
#endif
  return NS_OK;
}

//----------------------------------------------------------------------
// implementation helpers
void nsSVGSVGElement::GetScreenPosition(PRInt32 &x, PRInt32 &y)
{
  x = 0;
  y = 0;

  if (!mDocument) return;

  nsIPresShell *presShell = mDocument->GetShellAt(0);
  if (!presShell) {
    NS_ERROR("couldn't get presshell");
    return;
  }

  nsCOMPtr<nsIPresContext> context;
  presShell->GetPresContext(getter_AddRefs(context));
  if (!context) {
    NS_ERROR("couldn't get prescontext");
    return;
  }
   
  // Flush all pending notifications so that our frames are uptodate
  presShell->FlushPendingNotifications(PR_FALSE);
    
  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(this, &frame);

  float t2p;
  t2p = context->TwipsToPixels();

  
  nsIWidget* widget = nsnull;
        
  while (frame) {
    // Look for a widget so we can get screen coordinates
    nsIView* view = frame->GetView();
    if (view) {
      // handle scrolled views along the way:
      nsIScrollableView* scrollableView = nsnull;
      CallQueryInterface(view, &scrollableView);
      if (scrollableView) {
        nscoord scrollX, scrollY;
        scrollableView->GetScrollPosition(scrollX, scrollY);
        x -= scrollX;
        y -= scrollY;
      }

      // if this is a widget we break and get screen coords from it:
      widget = view->GetWidget();
      if (widget)
        break;
    }
          
    // No widget yet, so count up the coordinates of the frame 
    nsPoint origin = frame->GetPosition();
    x += origin.x;
    y += origin.y;
      
    frame = frame->GetParent();
  }
        
  
  // Convert to pixels using that scale
  x = NSTwipsToIntPixels(x, t2p);
  y = NSTwipsToIntPixels(y, t2p);
  
  if (widget) {
    // Add the widget's screen coordinates to the offset we've counted
    nsRect client(0,0,0,0);
    nsRect screen;
    widget->WidgetToScreen(client, screen);
    x += screen.x;
    y += screen.y;
  }
}
