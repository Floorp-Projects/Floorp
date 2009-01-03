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
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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

#include "nsGkAtoms.h"
#include "nsSVGLength.h"
#include "nsSVGAngle.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGMatrix.h"
#include "nsSVGPoint.h"
#include "nsSVGTransform.h"
#include "nsIDOMEventTarget.h"
#include "nsBindingManager.h"
#include "nsIFrame.h"
#include "nsISVGSVGFrame.h" //XXX
#include "nsSVGNumber.h"
#include "nsSVGRect.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include "nsISVGChildFrame.h"
#include "nsGUIEvent.h"
#include "nsSVGUtils.h"
#include "nsSVGSVGElement.h"

nsSVGElement::LengthInfo nsSVGSVGElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

nsSVGEnumMapping nsSVGSVGElement::sZoomAndPanMap[] = {
  {&nsGkAtoms::disable, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE},
  {&nsGkAtoms::magnify, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGSVGElement::sEnumInfo[1] =
{
  { &nsGkAtoms::zoomAndPan,
    sZoomAndPanMap,
    nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(SVG)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)

NS_INTERFACE_TABLE_HEAD(nsSVGSVGElement)
  NS_NODE_INTERFACE_TABLE7(nsSVGSVGElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGSVGElement,
                           nsIDOMSVGFitToViewBox, nsIDOMSVGLocatable,
                           nsIDOMSVGZoomAndPan)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGSVGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSVGElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSVGElement::nsSVGSVGElement(nsINodeInfo* aNodeInfo)
  : nsSVGSVGElementBase(aNodeInfo),
    mCoordCtx(nsnull),
    mViewportWidth(0),
    mViewportHeight(0),
    mCoordCtxMmPerPx(0),
    mPreviousTranslate_x(0),
    mPreviousTranslate_y(0),
    mPreviousScale(0),
    mRedrawSuspendCount(0),
    mDispatchEvent(PR_FALSE)
{
}

nsSVGSVGElement::~nsSVGSVGElement()
{
  if (mPreserveAspectRatio) {
    NS_REMOVE_SVGVALUE_OBSERVER(mPreserveAspectRatio);
  }
  if (mViewBox) {
    NS_REMOVE_SVGVALUE_OBSERVER(mViewBox);
  }
}

  
nsresult
nsSVGSVGElement::Init()
{
  nsresult rv = nsSVGSVGElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);
  
  // nsIDOMSVGFitToViewBox attributes ------:
  
  // DOM property: viewBox , #IMPLIED attrib: viewBox
  {
    nsCOMPtr<nsIDOMSVGRect> viewbox;
    rv = NS_NewSVGRect(getter_AddRefs(viewbox));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), viewbox);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: preserveAspectRatio , #IMPLIED attrib: preserveAspectRatio
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> preserveAspectRatio;
    rv = NS_NewSVGPreserveAspectRatio(getter_AddRefs(preserveAspectRatio));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedPreserveAspectRatio(
                                          getter_AddRefs(mPreserveAspectRatio),
                                          preserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: currentScale
  {
    rv = NS_NewSVGNumber(getter_AddRefs(mCurrentScale), 1.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    NS_ADD_SVGVALUE_OBSERVER(mCurrentScale);
  }

  // DOM property: currentTranslate
  {
    rv = NS_NewSVGPoint(getter_AddRefs(mCurrentTranslate));
    NS_ENSURE_SUCCESS(rv,rv);
    NS_ADD_SVGVALUE_OBSERVER(mCurrentTranslate);
  }

  // initialise "Previous" values
  RecordCurrentScaleTranslate();
  mDispatchEvent = PR_TRUE;

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSVGElement)


//----------------------------------------------------------------------
// nsIDOMSVGSVGElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP
nsSVGSVGElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP
nsSVGSVGElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP
nsSVGSVGElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP
nsSVGSVGElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

/* attribute DOMString contentScriptType; */
NS_IMETHODIMP
nsSVGSVGElement::GetContentScriptType(nsAString & aContentScriptType)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetContentScriptType");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetContentScriptType(const nsAString & aContentScriptType)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::SetContentScriptType");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute DOMString contentStyleType; */
NS_IMETHODIMP
nsSVGSVGElement::GetContentStyleType(nsAString & aContentStyleType)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetContentStyleType");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetContentStyleType(const nsAString & aContentStyleType)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::SetContentStyleType");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGRect viewport; */
NS_IMETHODIMP
nsSVGSVGElement::GetViewport(nsIDOMSVGRect * *aViewport)
{
  // XXX
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute float pixelUnitToMillimeterX; */
NS_IMETHODIMP
nsSVGSVGElement::GetPixelUnitToMillimeterX(float *aPixelUnitToMillimeterX)
{
  // to correctly determine this, the caller would need to pass in the
  // right PresContext...
  nsPresContext *context = nsContentUtils::GetContextForContent(this);
  if (!context) {
    *aPixelUnitToMillimeterX = 0.28f; // 90dpi
    return NS_OK;
  }

  *aPixelUnitToMillimeterX = 25.4f / nsPresContext::AppUnitsToIntCSSPixels(context->AppUnitsPerInch());
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
  nsPresContext *context = nsContentUtils::GetContextForContent(this);
  if (!context) {
    *aScreenPixelToMillimeterX = 0.28f; // 90dpi
    return NS_OK;
  }

  *aScreenPixelToMillimeterX = 25.4f /
      nsPresContext::AppUnitsToIntCSSPixels(context->AppUnitsPerInch());
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
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetUseCurrentView");
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsSVGSVGElement::SetUseCurrentView(PRBool aUseCurrentView)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::SetUseCurrentView");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGViewSpec currentView; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentView(nsIDOMSVGViewSpec * *aCurrentView)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetCurrentView");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute float currentScale; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentScale(float *aCurrentScale)
{
  return mCurrentScale->GetValue(aCurrentScale);
}

#define CURRENT_SCALE_MAX 16.0f
#define CURRENT_SCALE_MIN 0.0625f

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentScale(float aCurrentScale)
{
  NS_ENSURE_FINITE(aCurrentScale, NS_ERROR_ILLEGAL_VALUE);

  // Prevent bizarre behaviour and maxing out of CPU and memory by clamping
  if (aCurrentScale < CURRENT_SCALE_MIN)
    aCurrentScale = CURRENT_SCALE_MIN;
  else if (aCurrentScale > CURRENT_SCALE_MAX)
    aCurrentScale = CURRENT_SCALE_MAX;

  return mCurrentScale->SetValue(aCurrentScale);

  // We have to dispatch the required SVGZoom event from DidModifySVGObservable
  // since dispatching it here is too late (i.e. after repaint)
}

/* readonly attribute nsIDOMSVGPoint currentTranslate; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTranslate(nsIDOMSVGPoint * *aCurrentTranslate)
{
  *aCurrentTranslate = mCurrentTranslate;
  NS_ADDREF(*aCurrentTranslate);
  return NS_OK;
}

/* unsigned long suspendRedraw (in unsigned long max_wait_milliseconds); */
NS_IMETHODIMP
nsSVGSVGElement::SuspendRedraw(PRUint32 max_wait_milliseconds, PRUint32 *_retval)
{
  *_retval = 1;

  if (++mRedrawSuspendCount > 1) 
    return NS_OK;

  nsIFrame* frame = GetPrimaryFrame();
#ifdef DEBUG
  // XXX We sometimes hit this assertion when the svg:svg element is
  // in a binding and svg children are inserted underneath it using
  // <children/>. If the svg children then call suspendRedraw, the
  // above function call fails although the svg:svg's frame has been
  // build. Strange...
  
  NS_ASSERTION(frame, "suspending redraw w/o frame");
#endif
  if (frame) {
    nsISVGSVGFrame* svgframe;
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

  nsIFrame* frame = GetPrimaryFrame();
#ifdef DEBUG
  NS_ASSERTION(frame, "unsuspending redraw w/o frame");
#endif
  if (frame) {
    nsISVGSVGFrame* svgframe;
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
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_ERROR_FAILURE;

  doc->FlushPendingNotifications(Flush_Display);

  return NS_OK;
}

/* void pauseAnimations (); */
NS_IMETHODIMP
nsSVGSVGElement::PauseAnimations()
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::PauseAnimations");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unpauseAnimations (); */
NS_IMETHODIMP
nsSVGSVGElement::UnpauseAnimations()
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::UnpauseAnimations");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean animationsPaused (); */
NS_IMETHODIMP
nsSVGSVGElement::AnimationsPaused(PRBool *_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::AnimationsPaused");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* float getCurrentTime (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTime(float *_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetCurrentTime");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCurrentTime (in float seconds); */
NS_IMETHODIMP
nsSVGSVGElement::SetCurrentTime(float seconds)
{
  NS_ENSURE_FINITE(seconds, NS_ERROR_ILLEGAL_VALUE);
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::SetCurrentTime");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMNodeList getIntersectionList (in nsIDOMSVGRect rect, in nsIDOMSVGElement referenceElement); */
NS_IMETHODIMP
nsSVGSVGElement::GetIntersectionList(nsIDOMSVGRect *rect,
                                     nsIDOMSVGElement *referenceElement,
                                     nsIDOMNodeList **_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!rect || !referenceElement)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetIntersectionList");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMNodeList getEnclosureList (in nsIDOMSVGRect rect, in nsIDOMSVGElement referenceElement); */
NS_IMETHODIMP
nsSVGSVGElement::GetEnclosureList(nsIDOMSVGRect *rect,
                                  nsIDOMSVGElement *referenceElement,
                                  nsIDOMNodeList **_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!rect || !referenceElement)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetEnclosureList");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean checkIntersection (in nsIDOMSVGElement element, in nsIDOMSVGRect rect); */
NS_IMETHODIMP
nsSVGSVGElement::CheckIntersection(nsIDOMSVGElement *element,
                                   nsIDOMSVGRect *rect,
                                   PRBool *_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!element || !rect)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::CheckIntersection");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean checkEnclosure (in nsIDOMSVGElement element, in nsIDOMSVGRect rect); */
NS_IMETHODIMP
nsSVGSVGElement::CheckEnclosure(nsIDOMSVGElement *element,
                                nsIDOMSVGRect *rect,
                                PRBool *_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!element || !rect)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::CheckEnclosure");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deSelectAll (); */
NS_IMETHODIMP
nsSVGSVGElement::DeSelectAll()
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::DeSelectAll");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGNumber createSVGNumber (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGNumber(nsIDOMSVGNumber **_retval)
{
  return NS_NewSVGNumber(_retval);
}

/* nsIDOMSVGLength createSVGLength (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGLength(nsIDOMSVGLength **_retval)
{
  return NS_NewSVGLength(reinterpret_cast<nsISVGLength**>(_retval));
}

/* nsIDOMSVGAngle createSVGAngle (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGAngle(nsIDOMSVGAngle **_retval)
{
  return NS_NewDOMSVGAngle(_retval);
}

/* nsIDOMSVGPoint createSVGPoint (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGPoint(nsIDOMSVGPoint **_retval)
{
  return NS_NewSVGPoint(_retval);
}

/* nsIDOMSVGMatrix createSVGMatrix (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGMatrix(nsIDOMSVGMatrix **_retval)
{
  return NS_NewSVGMatrix(_retval);
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
nsSVGSVGElement::CreateSVGTransformFromMatrix(nsIDOMSVGMatrix *matrix, 
                                              nsIDOMSVGTransform **_retval)
{
  if (!matrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsresult rv = NS_NewSVGTransform(_retval);
  if (NS_FAILED(rv))
    return rv;

  (*_retval)->SetMatrix(matrix);
  return NS_OK;
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
  *aPreserveAspectRatio = mPreserveAspectRatio;
  NS_ADDREF(*aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGLocatable methods

/* readonly attribute nsIDOMSVGElement nearestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetNearestViewportElement(nsIDOMSVGElement * *aNearestViewportElement)
{
  return nsSVGUtils::GetNearestViewportElement(this, aNearestViewportElement);
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  return nsSVGUtils::GetFarthestViewportElement(this, aFarthestViewportElement);
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP
nsSVGSVGElement::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD))
    return NS_ERROR_FAILURE;

  nsISVGChildFrame* svgframe;
  CallQueryInterface(frame, &svgframe);
  if (svgframe) {
    svgframe->SetMatrixPropagation(PR_FALSE);
    svgframe->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                               nsISVGChildFrame::TRANSFORM_CHANGED);
    nsresult rv = svgframe->GetBBox(_retval);
    svgframe->SetMatrixPropagation(PR_TRUE);
    svgframe->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                               nsISVGChildFrame::TRANSFORM_CHANGED);
    return rv;
  } else {
    // XXX: outer svg
    return NS_ERROR_NOT_IMPLEMENTED;
  }
}

/* nsIDOMSVGMatrix getCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCTM(nsIDOMSVGMatrix **_retval)
{
  nsresult rv;
  *_retval = nsnull;

  nsIDocument* currentDoc = GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are uptodate
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }

  // first try to get the "screen" CTM of our nearest SVG ancestor

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the bindings that gives us our anonymous
  // ancestors. That's the binding manager for the document we actually belong
  // to, which is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsCOMPtr<nsIContent> element = this;
  nsCOMPtr<nsIContent> ancestor;
  unsigned short ancestorCount = 0;
  nsCOMPtr<nsIDOMSVGMatrix> ancestorCTM;

  while (1) {
    ancestor = nsnull;
    if (bindingManager) {
      // check for an anonymous ancestor first
      ancestor = bindingManager->GetInsertionParent(element);
    }
    if (!ancestor) {
      // if we didn't find an anonymous ancestor, use the explicit one
      ancestor = element->GetParent();
    }
    if (!ancestor) {
      // reached the top of our parent chain without finding an SVG ancestor
      break;
    }

    nsSVGSVGElement *viewportElement = QI_AND_CAST_TO_NSSVGSVGELEMENT(ancestor);
    if (viewportElement) {
      rv = viewportElement->GetViewboxToViewportTransform(getter_AddRefs(ancestorCTM));
      if (NS_FAILED(rv)) return rv;
      break;
    }

    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(ancestor);
    if (locatableElement) {
      rv = locatableElement->GetCTM(getter_AddRefs(ancestorCTM));
      if (NS_FAILED(rv)) return rv;
      break;
    }

    // ancestor was not SVG content. loop until we find an SVG ancestor
    element = ancestor;
    ancestorCount++;
  }

  // now account for our offset

  if (!ancestorCTM) {
    // we didn't find an SVG ancestor
    float s=1, x=0, y=0;
    if (IsRoot()) {
      // we're the root element. get our currentScale and currentTranslate vals
      mCurrentScale->GetValue(&s);
      mCurrentTranslate->GetX(&x);
      mCurrentTranslate->GetY(&y);
    }
    else {
      // we're inline in some non-SVG content. get our offset from the root
      GetOffsetToAncestor(nsnull, x, y);
    }
    rv = NS_NewSVGMatrix(getter_AddRefs(ancestorCTM), s, 0, 0, s, x, y);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    // we found an SVG ancestor
    float x=0, y=0;
    nsCOMPtr<nsIDOMSVGMatrix> tmp;
    if (ancestorCount == 0) {
      // our immediate parent is an SVG element. get our 'x' and 'y' attribs.
      // cast to nsSVGElement so we get our ancestor coord context.
      x = mLengthAttributes[X].GetAnimValue(static_cast<nsSVGElement*>
                                                       (this));
      y = mLengthAttributes[Y].GetAnimValue(static_cast<nsSVGElement*>
                                                       (this));
    }
    else {
      // We have an SVG ancestor, but with non-SVG content between us
#if 0
      nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject
                                              = do_QueryInterface(ancestor);
      if (!foreignObject) {
        NS_ERROR("the none-SVG content in the parent chain between us and our "
                 "SVG ancestor isn't rooted in a foreignObject element");
        return NS_ERROR_FAILURE;
      }
#endif
      // XXXjwatt: this isn't quite right since foreignObject can transform its
      // content, but it's close enough until we turn foreignObject back on
      GetOffsetToAncestor(ancestor, x, y);
    }
    rv = ancestorCTM->Translate(x, y, getter_AddRefs(tmp));
    if (NS_FAILED(rv)) return rv;
    ancestorCTM.swap(tmp);
  }

  // finally append our viewbox transform

  nsCOMPtr<nsIDOMSVGMatrix> tmp;
  rv = GetViewboxToViewportTransform(getter_AddRefs(tmp));
  if (NS_FAILED(rv)) return rv;
  return ancestorCTM->Multiply(tmp, _retval);  // addrefs, so we don't
}

/* nsIDOMSVGMatrix getScreenCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetScreenCTM(nsIDOMSVGMatrix **_retval)
{
  nsresult rv;
  *_retval = nsnull;

  nsIDocument* currentDoc = GetCurrentDoc();
  if (currentDoc) {
    // Flush all pending notifications so that our frames are uptodate
    currentDoc->FlushPendingNotifications(Flush_Layout);
  }

  // first try to get the "screen" CTM of our nearest SVG ancestor

  nsBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the bindings that gives us our anonymous
  // ancestors. That's the binding manager for the document we actually belong
  // to, which is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsCOMPtr<nsIContent> element = this;
  nsCOMPtr<nsIContent> ancestor;
  unsigned short ancestorCount = 0;
  nsCOMPtr<nsIDOMSVGMatrix> ancestorScreenCTM;

  while (1) {
    ancestor = nsnull;
    if (bindingManager) {
      // check for an anonymous ancestor first
      ancestor = bindingManager->GetInsertionParent(element);
    }
    if (!ancestor) {
      // if we didn't find an anonymous ancestor, use the explicit one
      ancestor = element->GetParent();
    }
    if (!ancestor) {
      // reached the top of our parent chain without finding an SVG ancestor
      break;
    }

    nsCOMPtr<nsIDOMSVGLocatable> locatableElement = do_QueryInterface(ancestor);
    if (locatableElement) {
      rv = locatableElement->GetScreenCTM(getter_AddRefs(ancestorScreenCTM));
      if (NS_FAILED(rv)) return rv;
      break;
    }

    // ancestor was not SVG content. loop until we find an SVG ancestor
    element = ancestor;
    ancestorCount++;
  }

  // now account for our offset

  if (!ancestorScreenCTM) {
    // we didn't find an SVG ancestor
    float s=1, x=0, y=0;
    if (IsRoot()) {
      // we're the root element. get our currentScale and currentTranslate vals
      mCurrentScale->GetValue(&s);
      mCurrentTranslate->GetX(&x);
      mCurrentTranslate->GetY(&y);
    }
    else {
      // we're inline in some non-SVG content. get our offset from the root
      GetOffsetToAncestor(nsnull, x, y);
    }
    rv = NS_NewSVGMatrix(getter_AddRefs(ancestorScreenCTM), s, 0, 0, s, x, y);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    // we found an SVG ancestor
    float x=0, y=0;
    nsCOMPtr<nsIDOMSVGMatrix> tmp;
    if (ancestorCount == 0) {
      // our immediate parent is an SVG element. get our 'x' and 'y' attribs
      // cast to nsSVGElement so we get our ancestor coord context.
      x = mLengthAttributes[X].GetAnimValue(static_cast<nsSVGElement*>
                                                       (this));
      y = mLengthAttributes[Y].GetAnimValue(static_cast<nsSVGElement*>
                                                       (this));
    }
    else {
      // We have an SVG ancestor, but with non-SVG content between us
#if 0
      nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject
                                              = do_QueryInterface(ancestor);
      if (!foreignObject) {
        NS_ERROR("the none-SVG content in the parent chain between us and our "
                 "SVG ancestor isn't rooted in a foreignObject element");
        return NS_ERROR_FAILURE;
      }
#endif
      // XXXjwatt: this isn't quite right since foreignObject can transform its
      // content, but it's close enough until we turn foreignObject back on
      GetOffsetToAncestor(ancestor, x, y);
    }
    rv = ancestorScreenCTM->Translate(x, y, getter_AddRefs(tmp));
    if (NS_FAILED(rv)) return rv;
    ancestorScreenCTM.swap(tmp);
  }

  // finally append our viewbox transform

  nsCOMPtr<nsIDOMSVGMatrix> tmp;
  rv = GetViewboxToViewportTransform(getter_AddRefs(tmp));
  if (NS_FAILED(rv)) return rv;
  return ancestorScreenCTM->Multiply(tmp, _retval);  // addrefs, so we don't
}

/* nsIDOMSVGMatrix getTransformToElement (in nsIDOMSVGElement element); */
NS_IMETHODIMP
nsSVGSVGElement::GetTransformToElement(nsIDOMSVGElement *element,
                                       nsIDOMSVGMatrix **_retval)
{
  if (!element)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsresult rv;
  *_retval = nsnull;
  nsCOMPtr<nsIDOMSVGMatrix> ourScreenCTM;
  nsCOMPtr<nsIDOMSVGMatrix> targetScreenCTM;
  nsCOMPtr<nsIDOMSVGMatrix> tmp;
  nsCOMPtr<nsIDOMSVGLocatable> target = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return rv;

  // the easiest way to do this (if likely to increase rounding error):
  rv = GetScreenCTM(getter_AddRefs(ourScreenCTM));
  if (NS_FAILED(rv)) return rv;
  rv = target->GetScreenCTM(getter_AddRefs(targetScreenCTM));
  if (NS_FAILED(rv)) return rv;
  rv = targetScreenCTM->Inverse(getter_AddRefs(tmp));
  if (NS_FAILED(rv)) return rv;
  return tmp->Multiply(ourScreenCTM, _retval);  // addrefs, so we don't
}

//----------------------------------------------------------------------
// nsIDOMSVGZoomAndPan methods

/* attribute unsigned short zoomAndPan; */
NS_IMETHODIMP
nsSVGSVGElement::GetZoomAndPan(PRUint16 *aZoomAndPan)
{
  *aZoomAndPan = mEnumAttributes[ZOOMANDPAN].GetAnimValue();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::SetZoomAndPan(PRUint16 aZoomAndPan)
{
  if (aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this, PR_TRUE);
    return NS_OK;
  }

  return NS_ERROR_DOM_SVG_INVALID_VALUE_ERR;
}

//----------------------------------------------------------------------
// helper methods for implementing SVGZoomEvent:

NS_IMETHODIMP
nsSVGSVGElement::GetCurrentScaleNumber(nsIDOMSVGNumber **aResult)
{
  *aResult = mCurrentScale;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentScaleTranslate(float s, float x, float y)
{
  RecordCurrentScaleTranslate();
  mDispatchEvent = PR_FALSE;
  SetCurrentScale(s);  // clamps! don't call mCurrentScale->SetValue() directly
  mCurrentTranslate->SetX(x);
  mCurrentTranslate->SetY(y);
  mDispatchEvent = PR_TRUE;

  // now dispatch an SVGZoom event if we are the root element
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
    NS_ASSERTION(presShell, "no presShell");
    if (presShell && IsRoot()) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsGUIEvent event(PR_TRUE, NS_SVG_ZOOM, 0);
      event.eventStructType = NS_SVGZOOM_EVENT;
      presShell->HandleDOMEventWithTarget(this, &event, &status);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentTranslate(float x, float y)
{
  RecordCurrentScaleTranslate();
  mDispatchEvent = PR_FALSE;
  mCurrentTranslate->SetX(x);
  mCurrentTranslate->SetY(y);
  mDispatchEvent = PR_TRUE;

  // now dispatch an SVGScroll event if we are the root element
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
    NS_ASSERTION(presShell, "no presShell");
    if (presShell && IsRoot()) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event(PR_TRUE, NS_SVG_SCROLL);
      event.eventStructType = NS_SVG_EVENT;
      presShell->HandleDOMEventWithTarget(this, &event, &status);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSVGSVGElement::RecordCurrentScaleTranslate()
{
  // IMPORTANT: If either mCurrentTranslate *or* mCurrentScale is changed then
  // mPreviousTranslate_x, mPreviousTranslate_y *and* mPreviousScale must all
  // be updated otherwise SVGZoomEvents will end up with invalid data. I.e. an
  // SVGZoomEvent's properties previousScale and previousTranslate must contain
  // the state of currentScale and currentTranslate immediately before the
  // change that caused the event's dispatch, which is *not* necessarily the
  // same thing as the values of currentScale and currentTranslate prior to
  // their own last change.
  mCurrentScale->GetValue(&mPreviousScale);
  mCurrentTranslate->GetX(&mPreviousTranslate_x);
  mCurrentTranslate->GetY(&mPreviousTranslate_y);
}

NS_IMETHODIMP_(float)
nsSVGSVGElement::GetPreviousTranslate_x()
{
  return mPreviousTranslate_x;
}

NS_IMETHODIMP_(float)
nsSVGSVGElement::GetPreviousTranslate_y()
{
  return mPreviousTranslate_y;
}

NS_IMETHODIMP_(float)
nsSVGSVGElement::GetPreviousScale()
{
  return mPreviousScale;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGSVGElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFEFloodMap,
    sFillStrokeMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sGraphicsMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGSVGElementBase::IsAttributeMapped(name);
}

nsresult
nsSVGSVGElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                              const nsAString* aValue, PRBool aNotify)
{
  nsSVGSVGElementBase::AfterSetAttr(aNameSpaceID, aName, aValue, aNotify);

  // We need to do this here because the calling
  // InvalidateTransformNotifyFrame in DidModifySVGObservable would
  // happen too early, before HasAttr(viewBox) returns true (important
  // in the case of adding a viewBox)
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::viewBox) {
    InvalidateTransformNotifyFrame();
  }

  return NS_OK;
}

nsresult
nsSVGSVGElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                           PRBool aNotify)
{
  nsSVGSVGElementBase::UnsetAttr(aNamespaceID, aName, aNotify);

  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::viewBox) {
    InvalidateTransformNotifyFrame();
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGSVGElement::WillModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  if (mDispatchEvent) {
    // Modification isn't due to calling SetCurrent[Scale]Translate, so if
    // currentScale or currentTranslate is about to change we must record their
    // current values.
    nsCOMPtr<nsIDOMSVGNumber> n = do_QueryInterface(observable);
    if (n && n==mCurrentScale) {
      RecordCurrentScaleTranslate();
    }
    else {
      nsCOMPtr<nsIDOMSVGPoint> p = do_QueryInterface(observable);
      if (p && p==mCurrentTranslate) {
        RecordCurrentScaleTranslate();
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::DidModifySVGObservable (nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
  NS_ASSERTION(presShell, "no presShell");
  if (!presShell) return NS_ERROR_FAILURE;

  // If currentScale or currentTranslate has changed, we are the root element,
  // and the changes wasn't caused by SetCurrent[Scale]Translate then we must
  // dispatch an SVGZoom or SVGScroll DOM event before repainting
  nsCOMPtr<nsIDOMSVGNumber> n = do_QueryInterface(observable);
  if (n && n==mCurrentScale) {
    if (mDispatchEvent && IsRoot()) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsGUIEvent event(PR_TRUE, NS_SVG_ZOOM, 0);
      event.eventStructType = NS_SVGZOOM_EVENT;
      presShell->HandleDOMEventWithTarget(this, &event, &status);
    }
    else {
      return NS_OK;  // we don't care about currentScale changes on non-root
    }
  }
  else {
    nsCOMPtr<nsIDOMSVGPoint> p = do_QueryInterface(observable);
    if (p && p==mCurrentTranslate) {
      if (mDispatchEvent && IsRoot()) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsEvent event(PR_TRUE, NS_SVG_SCROLL);
        event.eventStructType = NS_SVG_EVENT;
        presShell->HandleDOMEventWithTarget(this, &event, &status);
      }
      else {
        return NS_OK;  // we don't care about currentScale changes on non-root
      }
    }
  }

  // Deal with viewBox in AfterSetAttr (see comment there for reason)
  nsCOMPtr<nsIDOMSVGAnimatedRect> r = do_QueryInterface(observable);
  if (r != mViewBox) {
    InvalidateTransformNotifyFrame();
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement overrides

PRBool
nsSVGSVGElement::IsEventName(nsIAtom* aName)
{
  /* The events in EventNameType_SVGSVG are for events that are only
     applicable to outermost 'svg' elements. We don't check if we're an outer
     'svg' element in case we're not inserted into the document yet, but since
     the target of the events in question will always be the outermost 'svg'
     element, this shouldn't cause any real problems.
  */
  return nsContentUtils::IsEventAttributeName(aName,
         (EventNameType_SVGGraphic | EventNameType_SVGSVG));
}

//----------------------------------------------------------------------
// public helpers:

nsresult
nsSVGSVGElement::GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval)
{
  *_retval = nsnull;

  float viewportWidth, viewportHeight;
  nsSVGSVGElement *ctx = GetCtx();
  if (!ctx) {
    // outer svg
    viewportWidth = mViewportWidth;
    viewportHeight = mViewportHeight;
  } else {
    viewportWidth = mLengthAttributes[WIDTH].GetAnimValue(ctx);
    viewportHeight = mLengthAttributes[HEIGHT].GetAnimValue(ctx);
  }

  float viewboxX, viewboxY, viewboxWidth, viewboxHeight;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    nsCOMPtr<nsIDOMSVGRect> vb;
    mViewBox->GetAnimVal(getter_AddRefs(vb));
    NS_ASSERTION(vb, "could not get viewbox");
    vb->GetX(&viewboxX);
    vb->GetY(&viewboxY);
    vb->GetWidth(&viewboxWidth);
    vb->GetHeight(&viewboxHeight);
  } else {
    viewboxX = viewboxY = 0.0f;
    viewboxWidth = viewportWidth;
    viewboxHeight = viewportHeight;
  }

  if (viewboxWidth <= 0.0f || viewboxHeight <= 0.0f) {
    return NS_ERROR_FAILURE; // invalid - don't paint element
  }

  nsCOMPtr<nsIDOMSVGMatrix> xform =
    nsSVGUtils::GetViewBoxTransform(viewportWidth, viewportHeight,
                                    viewboxX, viewboxY,
                                    viewboxWidth, viewboxHeight,
                                    mPreserveAspectRatio);
  xform.swap(*_retval);

  return NS_OK;
}

//----------------------------------------------------------------------
// implementation helpers

// if an ancestor isn't specified, obtains offset from root frame
void nsSVGSVGElement::GetOffsetToAncestor(nsIContent* ancestor,
                                          float &x, float &y)
{
  x = 0.0f;
  y = 0.0f;

  nsIDocument *document = GetCurrentDoc();
  if (!document) return;

  // Flush all pending notifications so that our frames are uptodate
  // Make sure to do this before we start grabbing layout objects like
  // presshells.
  document->FlushPendingNotifications(Flush_Layout);
  
  nsIPresShell *presShell = document->GetPrimaryShell();
  if (!presShell) {
    return;
  }

  nsPresContext *context = presShell->GetPresContext();
  if (!context) {
    return;
  }

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
  nsIFrame* ancestorFrame = ancestor ?
                            presShell->GetPrimaryFrameFor(ancestor) :
                            presShell->GetRootFrame();

  if (frame && ancestorFrame) {
    nsPoint point = frame->GetOffsetTo(ancestorFrame);
    x = nsPresContext::AppUnitsToFloatCSSPixels(point.x);
    y = nsPresContext::AppUnitsToFloatCSSPixels(point.y);
  }
}

void
nsSVGSVGElement::InvalidateTransformNotifyFrame()
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return;
  nsIPresShell* presShell = doc->GetPrimaryShell();
  if (!presShell) return;

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
  if (frame) {
    nsISVGSVGFrame* svgframe;
    CallQueryInterface(frame, &svgframe);
    if (svgframe) {
      svgframe->NotifyViewportChange();
    }
#ifdef DEBUG
    else {
      // XXX we get here during nsSVGOuterSVGFrame::Init() since that
      // function is called before the presshell association between us
      // and our frame is established.
      NS_WARNING("wrong frame type");
    }
#endif
  }
}

//----------------------------------------------------------------------
// nsSVGSVGElement

already_AddRefed<nsIDOMSVGRect>
nsSVGSVGElement::GetCtxRect()
{
  float w, h;
  nsCOMPtr<nsIDOMSVGRect> vb;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    mViewBox->GetAnimVal(getter_AddRefs(vb));
    vb->GetWidth(&w);
    vb->GetHeight(&h);
  } else {
    nsSVGSVGElement *ctx = GetCtx();
    if (ctx) {
      w = mLengthAttributes[WIDTH].GetAnimValue(ctx);
      h = mLengthAttributes[HEIGHT].GetAnimValue(ctx);
    } else {
      w = mViewportWidth;
      h = mViewportHeight;
    }
  }

  if (!vb || w < 0.0f || h < 0.0f) {
    NS_NewSVGRect(getter_AddRefs(vb), 0, 0, PR_MAX(w, 0.0f), PR_MAX(h, 0.0f));
  }

  nsIDOMSVGRect *retval = nsnull;
  vb.swap(retval);
  return retval;
}

float
nsSVGSVGElement::GetLength(PRUint8 aCtxType)
{
  float h, w;

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    nsCOMPtr<nsIDOMSVGRect> vb;
    mViewBox->GetAnimVal(getter_AddRefs(vb));
    vb->GetHeight(&h);
    vb->GetWidth(&w);
  } else {
    nsSVGSVGElement *ctx = GetCtx();
    if (ctx) {
      w = mLengthAttributes[WIDTH].GetAnimValue(ctx);
      h = mLengthAttributes[HEIGHT].GetAnimValue(ctx);
    } else {
      w = mViewportWidth;
      h = mViewportHeight;
    }
  }

  if (w < 0.0f) {
    w = 0.0f;
  }
  if (h < 0.0f) {
    h = 0.0f;
  }

  switch (aCtxType) {
  case nsSVGUtils::X:
    return w;
  case nsSVGUtils::Y:
    return h;
  case nsSVGUtils::XY:
    return float(nsSVGUtils::ComputeNormalizedHypotenuse(w, h));
  }
  return 0;
}

float
nsSVGSVGElement::GetMMPerPx(PRUint8 aCtxType)
{
  if (mCoordCtxMmPerPx == 0.0f) {
    GetScreenPixelToMillimeterX(&mCoordCtxMmPerPx);
  }
  return mCoordCtxMmPerPx;
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGSVGElement::DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  nsSVGSVGElementBase::DidChangeLength(aAttrEnum, aDoSetAttr);

  InvalidateTransformNotifyFrame();
}

nsSVGElement::LengthAttributesInfo
nsSVGSVGElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

void
nsSVGSVGElement::DidChangeEnum(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  nsSVGSVGElementBase::DidChangeEnum(aAttrEnum, aDoSetAttr);

  InvalidateTransformNotifyFrame();
}

nsSVGElement::EnumAttributesInfo
nsSVGSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}
