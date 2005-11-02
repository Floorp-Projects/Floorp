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

#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGLocatable.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGLength.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
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
#include "nsISVGSVGFrame.h" //XXX
#include "nsSVGNumber.h"
#include "nsSVGRect.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsSVGEnum.h"
#include "nsISVGChildFrame.h"
#include "nsGUIEvent.h"

typedef nsSVGStylableElement nsSVGSVGElementBase;

class nsSVGSVGElement : public nsSVGSVGElementBase,
                        public nsISVGSVGElement, // : nsIDOMSVGSVGElement
                        public nsIDOMSVGFitToViewBox,
                        public nsIDOMSVGLocatable,
                        public nsIDOMSVGZoomAndPan,
                        public nsSVGCoordCtxProvider
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
  NS_DECL_NSIDOMSVGZOOMANDPAN
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSVGElementBase::)

  // nsISVGSVGElement interface:
  NS_IMETHOD SetParentCoordCtxProvider(nsSVGCoordCtxProvider *parentCtx);
  NS_IMETHOD GetCurrentScaleNumber(nsIDOMSVGNumber **aResult);
  NS_IMETHOD GetZoomAndPanEnum(nsISVGEnum **aResult);
  NS_IMETHOD SetCurrentScaleTranslate(float s, float x, float y);
  NS_IMETHOD SetCurrentTranslate(float x, float y);
  NS_IMETHOD_(void) RecordCurrentScaleTranslate();
  NS_IMETHOD_(float) GetPreviousTranslate_x();
  NS_IMETHOD_(float) GetPreviousTranslate_y();
  NS_IMETHOD_(float) GetPreviousScale();

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

protected:
  // nsSVGElement overrides
  PRBool IsEventName(nsIAtom* aName);

  // implementation helpers:
  void GetOffsetToAncestor(nsIContent* ancestor, float &x, float &y);

  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;
  nsCOMPtr<nsIDOMSVGAnimatedRect>   mViewBox;
  nsCOMPtr<nsIDOMSVGMatrix>         mViewBoxToViewportTransform;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> mPreserveAspectRatio;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;

  // zoom and pan
  // IMPORTANT: only RecordCurrentScaleTranslate should change the "mPreviousX"
  // members below - see the comment in RecordCurrentScaleTranslate
  nsCOMPtr<nsISVGEnum>              mZoomAndPan;
  nsCOMPtr<nsIDOMSVGPoint>          mCurrentTranslate;
  nsCOMPtr<nsIDOMSVGNumber>         mCurrentScale;
  float                             mPreviousTranslate_x;
  float                             mPreviousTranslate_y;
  float                             mPreviousScale;
  PRBool                            mDispatchEvent;

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
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGZoomAndPan)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsSVGCoordCtxProvider)
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

  
  // nsIDOMSVGSVGElement attributes ------:
  
  // DOM property: width ,  #IMPLIED attrib: width
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);

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

    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0f);
    NS_ENSURE_SUCCESS(rv,rv);

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

    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // nsIDOMSVGFitToViewBox attributes ------:
  
  // DOM property: viewBox , #IMPLIED attrib: viewBox
  {
    nsCOMPtr<nsIDOMSVGRect> viewbox;
    nsCOMPtr<nsIDOMSVGLength> animWidth, animHeight;
    mWidth->GetAnimVal(getter_AddRefs(animWidth));
    mHeight->GetAnimVal(getter_AddRefs(animHeight));
    rv = NS_NewSVGViewBox(getter_AddRefs(viewbox), animWidth, animHeight);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), viewbox);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
    // initialize coordinate context with viewbox:
    SetCoordCtxRect(viewbox);
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
    rv = AddMappedSVGValue(nsSVGAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // nsIDOMSVGZoomAndPan attribute ------:

  // Define enumeration mappings
  static struct nsSVGEnumMapping zoomMap[] = {
        {&nsSVGAtoms::disable, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE},
        {&nsSVGAtoms::magnify, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY},
        {nsnull, 0}
  };

  // DOM property: zoomAndPan ,  #IMPLIED attrib: zoomAndPan
  {
    rv = NS_NewSVGEnum(getter_AddRefs(mZoomAndPan),
                       nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY, zoomMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::zoomAndPan, mZoomAndPan);
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


NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGSVGElement)


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

  *aPixelUnitToMillimeterX = 0.28f; // 90dpi

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_OK;
  // Get Presentation shell 0
  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell) return NS_OK;
  
  // Get the Presentation Context from the Shell
  nsPresContext *context = presShell->GetPresContext();
  if (!context) return NS_OK;

  *aPixelUnitToMillimeterX = context->ScaledPixelsToTwips() / TWIPS_PER_POINT_FLOAT / (72.0f * 0.03937f);
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

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_OK;
    // Get Presentation shell 0
  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell) return NS_OK;
  
  // Get the Presentation Context from the Shell
  nsPresContext *context = presShell->GetPresContext();
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

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = doc->GetShellAt(0);
  NS_ASSERTION(presShell, "need presShell to suspend redraw");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
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

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = doc->GetShellAt(0);
  NS_ASSERTION(presShell, "need presShell to unsuspend redraw");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
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
  return NS_NewSVGLength(NS_REINTERPRET_CAST(nsISVGLength**, _retval));
}

/* nsIDOMSVGAngle createSVGAngle (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGAngle(nsIDOMSVGAngle **_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::CreateSVGAngle");
  return NS_ERROR_NOT_IMPLEMENTED;
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

/* nsIDOMSVGMatrix getViewboxToViewportTransform (); */
NS_IMETHODIMP
nsSVGSVGElement::GetViewboxToViewportTransform(nsIDOMSVGMatrix **_retval)
{
  nsresult rv = NS_OK;

  if (!mViewBoxToViewportTransform) {
    float viewportWidth;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mWidth->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&viewportWidth);
    }
    float viewportHeight;
    {
      nsCOMPtr<nsIDOMSVGLength> l;
      mHeight->GetAnimVal(getter_AddRefs(l));
      l->GetValue(&viewportHeight);
    }
    
    float viewboxX, viewboxY, viewboxWidth, viewboxHeight;
    {
      nsCOMPtr<nsIDOMSVGRect> vb;
      mViewBox->GetAnimVal(getter_AddRefs(vb));
      NS_ASSERTION(vb, "could not get viewbox");
      vb->GetX(&viewboxX);
      vb->GetY(&viewboxY);
      vb->GetWidth(&viewboxWidth);
      vb->GetHeight(&viewboxHeight);
    }
    if (viewboxWidth==0.0f || viewboxHeight==0.0f) {
      NS_ERROR("XXX. We shouldn't get here. Viewbox width/height is set to 0. Need to disable display of element as per specs.");
      viewboxWidth = 1.0f;
      viewboxHeight = 1.0f;
    }
    
    PRUint16 align, meetOrSlice;
    {
      nsCOMPtr<nsIDOMSVGPreserveAspectRatio> par;
      mPreserveAspectRatio->GetAnimVal(getter_AddRefs(par));
      NS_ASSERTION(par, "could not get preserveAspectRatio");
      par->GetAlign(&align);
      par->GetMeetOrSlice(&meetOrSlice);
    }

    // default to the defaults
    if (align == nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
    if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN)
      align = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;
    
    float a, d, e, f;
    a = viewportWidth/viewboxWidth;
    d = viewportHeight/viewboxHeight;
    e = 0.0f;
    f = 0.0f;

    if (align != nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
        a != d) {
      if (meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
          a < d ||
          meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
          d < a) {
        d = a;
        switch (align) {
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
            break;
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
            f = (viewportHeight - a * viewboxHeight) / 2.0f;
            break;
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
            f = viewportHeight - a * viewboxHeight;
            break;
          default:
            NS_NOTREACHED("Unknown value for align");
        }
      }
      else if (
          meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
          d < a ||
          meetOrSlice == nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE &&
          a < d) {
        a = d;
        switch (align) {
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMIN:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMID:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMINYMAX:
            break;
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMIN:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMAX:
            e = (viewportWidth - a * viewboxWidth) / 2.0f;
            break;
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMIN:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMID:
          case nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX:
            e = viewportWidth - a * viewboxWidth;
            break;
          default:
            NS_NOTREACHED("Unknown value for align");
        }
      }
      else NS_NOTREACHED("Unknown value for meetOrSlice");
    }

    if (viewboxX) e += -a * viewboxX;
    if (viewboxY) f += -d * viewboxY;
    
#ifdef DEBUG
    printf("SVG Viewport=(0?,0?,%f,%f)\n", viewportWidth, viewportHeight);
    printf("SVG Viewbox=(%f,%f,%f,%f)\n", viewboxX, viewboxY, viewboxWidth, viewboxHeight);
    printf("SVG Viewbox->Viewport xform [a c e] = [%f,   0, %f]\n", a, e);
    printf("                            [b d f] = [   0,  %f, %f]\n", d, f);
#endif
    
    rv = NS_NewSVGMatrix(getter_AddRefs(mViewBoxToViewportTransform),
                         a, 0.0f, 0.0f, d, e, f);
  }

  *_retval = mViewBoxToViewportTransform;
  NS_IF_ADDREF(*_retval);
  return rv;
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
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetNearestViewportElement");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetFarthestViewportElement");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP
nsSVGSVGElement::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) return NS_ERROR_FAILURE;
  nsIPresShell *presShell = doc->GetShellAt(0);
  NS_ASSERTION(presShell, "no presShell");
  if (!presShell) return NS_ERROR_FAILURE;

  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);

  NS_ASSERTION(frame, "can't get bounding box for element without frame");

  if (frame) {
    nsISVGChildFrame* svgframe;
    frame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&svgframe);
    if (svgframe) {
      svgframe->SetMatrixPropagation(PR_FALSE);
      svgframe->NotifyCanvasTMChanged(PR_TRUE);
      nsresult rv = svgframe->GetBBox(_retval);
      svgframe->SetMatrixPropagation(PR_TRUE);
      svgframe->NotifyCanvasTMChanged(PR_TRUE);
      return rv;
    } else {
      // XXX: outer svg
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }
  return NS_ERROR_FAILURE;
}

/* nsIDOMSVGMatrix getCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCTM(nsIDOMSVGMatrix **_retval)
{
  nsresult rv;
  *_retval = nsnull;

  // first try to get the "screen" CTM of our nearest SVG ancestor

  nsIBindingManager *bindingManager = nsnull;
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
    if (bindingManager) {
      // check for an anonymous ancestor first
      bindingManager->GetInsertionParent(element, getter_AddRefs(ancestor));
    }
    if (!ancestor) {
      // if we didn't find an anonymous ancestor, use the explicit one
      ancestor = element->GetParent();
    }
    if (!ancestor) {
      // reached the top of our parent chain without finding an SVG ancestor
      break;
    }

    nsCOMPtr<nsIDOMSVGSVGElement> viewportElement = do_QueryInterface(ancestor);
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
    if (ownerDoc &&
        ownerDoc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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
      // our immediate parent is an SVG element. get our 'x' and 'y' attribs
      nsCOMPtr<nsIDOMSVGLength> length;
      mX->GetAnimVal(getter_AddRefs(length));
      length->GetValue(&x);
      mY->GetAnimVal(getter_AddRefs(length));
      length->GetValue(&y);
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

  // first try to get the "screen" CTM of our nearest SVG ancestor

  nsIBindingManager *bindingManager = nsnull;
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
    if (bindingManager) {
      // check for an anonymous ancestor first
      bindingManager->GetInsertionParent(element, getter_AddRefs(ancestor));
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
    if (ownerDoc &&
        ownerDoc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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
      nsCOMPtr<nsIDOMSVGLength> length;
      mX->GetAnimVal(getter_AddRefs(length));
      length->GetValue(&x);
      mY->GetAnimVal(getter_AddRefs(length));
      length->GetValue(&y);
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
  return ourScreenCTM->Multiply(tmp, _retval);  // addrefs, so we don't
}

//----------------------------------------------------------------------
// nsIDOMSVGZoomAndPan methods

/* attribute unsigned short zoomAndPan; */
NS_IMETHODIMP
nsSVGSVGElement::GetZoomAndPan(PRUint16 *aZoomAndPan)
{
  return mZoomAndPan->GetIntegerValue(*aZoomAndPan);
}

NS_IMETHODIMP
nsSVGSVGElement::SetZoomAndPan(PRUint16 aZoomAndPan)
{
  if (aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY)
    return mZoomAndPan->SetIntegerValue(aZoomAndPan);

  return NS_ERROR_DOM_SVG_INVALID_VALUE_ERR;
}

//----------------------------------------------------------------------
// nsISVGSVGElement methods:

NS_IMETHODIMP
nsSVGSVGElement::SetParentCoordCtxProvider(nsSVGCoordCtxProvider *parentCtx)
{
  if (!parentCtx) {
    NS_ERROR("null parent context");
    return NS_ERROR_FAILURE;
  }
  
  // set parent's mmPerPx on our coord contexts:
  float mmPerPxX = nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextX())->GetMillimeterPerPixel();
  float mmPerPxY = nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextY())->GetMillimeterPerPixel();
  SetCoordCtxMMPerPx(mmPerPxX, mmPerPxY);
  
  // set the parentCtx as context on our width/height/x/y:
  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mX->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> l = do_QueryInterface(dom_length);
    l->SetContext(nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextX()));
  }

  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mY->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> l = do_QueryInterface(dom_length);
    l->SetContext(nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextY()));
  }

  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mWidth->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> l = do_QueryInterface(dom_length);
    l->SetContext(nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextX()));
  }

  {
    nsCOMPtr<nsIDOMSVGLength> dom_length;
    mHeight->GetAnimVal(getter_AddRefs(dom_length));
    nsCOMPtr<nsISVGLength> l = do_QueryInterface(dom_length);
    l->SetContext(nsRefPtr<nsSVGCoordCtx>(parentCtx->GetContextY()));
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::GetCurrentScaleNumber(nsIDOMSVGNumber **aResult)
{
  *aResult = mCurrentScale;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::GetZoomAndPanEnum(nsISVGEnum **aResult)
{
  *aResult = mZoomAndPan;
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
    nsIPresShell* presShell = doc->GetShellAt(0);
    NS_ASSERTION(presShell, "no presShell");
    if (presShell &&
        doc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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
    nsIPresShell* presShell = doc->GetShellAt(0);
    NS_ASSERTION(presShell, "no presShell");
    if (presShell &&
        doc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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
    sFillStrokeMap,
    sGraphicsMap,
    sTextContentElementsMap,
    sFontSpecificationMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGSVGElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGSVGElement::WillModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
#ifdef DEBUG
  printf("viewport/viewbox/preserveAspectRatio will be changed\n");
#endif

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
  nsIPresShell* presShell = doc->GetShellAt(0);
  NS_ASSERTION(presShell, "no presShell");
  if (!presShell) return NS_ERROR_FAILURE;

  // If currentScale or currentTranslate has changed, we are the root element,
  // and the changes wasn't caused by SetCurrent[Scale]Translate then we must
  // dispatch an SVGZoom or SVGScroll DOM event before repainting
  nsCOMPtr<nsIDOMSVGNumber> n = do_QueryInterface(observable);
  if (n && n==mCurrentScale) {
    if (mDispatchEvent &&
        doc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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
      if (mDispatchEvent &&
          doc->GetRootContent() == NS_STATIC_CAST(nsIContent*, this)) {
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

  // invalidate viewbox -> viewport xform & inform frames
  mViewBoxToViewportTransform = nsnull;

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
  
  
#ifdef DEBUG
  printf("viewport/viewbox/preserveAspectRatio have been changed\n");
#endif
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement overrides

PRBool
nsSVGSVGElement::IsEventName(nsIAtom* aName)
{
  return IsGraphicElementEventName(aName) ||

  /* The following are for events that are only applicable to outermost 'svg'
     elements. We don't check if we're an outer 'svg' element in case we're not
     inserted into the document yet, but since the target of the events in
     question will always be the outermost 'svg' element, this shouldn't cause
     any real problems.
  */
         aName == nsSVGAtoms::onunload    ||
         aName == nsSVGAtoms::onscroll    ||
         aName == nsSVGAtoms::onzoom;
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
  
  nsIPresShell *presShell = document->GetShellAt(0);
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
    x = point.x * context->TwipsToPixels();
    y = point.y * context->TwipsToPixels();
  }
}
