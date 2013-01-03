/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StandardInteger.h"
#include "mozilla/Util.h"
#include "mozilla/Likely.h"

#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "DOMSVGNumber.h"
#include "DOMSVGLength.h"
#include "nsSVGAngle.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "DOMSVGMatrix.h"
#include "DOMSVGPoint.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsISVGSVGFrame.h" //XXX
#include "nsSVGRect.h"
#include "nsError.h"
#include "nsISVGChildFrame.h"
#include "nsGUIEvent.h"
#include "nsSVGSVGElement.h"
#include "nsSVGUtils.h"
#include "nsSVGViewElement.h"
#include "nsStyleUtil.h"
#include "SVGContentUtils.h"

#include "nsEventDispatcher.h"
#include "nsSMILTimeContainer.h"
#include "nsSMILAnimationController.h"
#include "nsSMILTypes.h"
#include "nsIContentIterator.h"
#include "SVGAngle.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(nsSVGTranslatePoint::DOMVal, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGTranslatePoint::DOMVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGTranslatePoint::DOMVal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGTranslatePoint::DOMVal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISVGPoint)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsresult
nsSVGTranslatePoint::ToDOMVal(nsSVGSVGElement *aElement,
                              nsISupports **aResult)
{
  NS_ADDREF(*aResult = new DOMVal(this, aElement));
  return NS_OK;
}

nsISupports*
nsSVGTranslatePoint::DOMVal::GetParentObject()
{
  return static_cast<nsIDOMSVGSVGElement*>(mElement);
}

void
nsSVGTranslatePoint::DOMVal::SetX(float aValue, ErrorResult& rv)
{
  rv = mElement->SetCurrentTranslate(aValue, mVal->GetY());
}

void
nsSVGTranslatePoint::DOMVal::SetY(float aValue, ErrorResult& rv)
{
  rv = mElement->SetCurrentTranslate(mVal->GetX(), aValue);
}

already_AddRefed<nsISVGPoint>
nsSVGTranslatePoint::DOMVal::MatrixTransform(DOMSVGMatrix& matrix)
{
  float a = matrix.A(), b = matrix.B(), c = matrix.C();
  float d = matrix.D(), e = matrix.E(), f = matrix.F();
  float x = mVal->GetX();
  float y = mVal->GetY();

  nsCOMPtr<nsISVGPoint> point = new DOMSVGPoint(a*x + c*y + e, b*x + d*y + f);
  return point.forget();
}

nsSVGElement::LengthInfo nsSVGSVGElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

nsSVGEnumMapping nsSVGSVGElement::sZoomAndPanMap[] = {
  {&nsGkAtoms::disable, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE},
  {&nsGkAtoms::magnify, nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY},
  {nullptr, 0}
};

nsSVGElement::EnumInfo nsSVGSVGElement::sEnumInfo[1] =
{
  { &nsGkAtoms::zoomAndPan,
    sZoomAndPanMap,
    nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT_CHECK_PARSER(SVG)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGSVGElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsSVGSVGElement,
                                                nsSVGSVGElementBase)
  if (tmp->mTimedDocumentRoot) {
    tmp->mTimedDocumentRoot->Unlink();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsSVGSVGElement,
                                                  nsSVGSVGElementBase)
  if (tmp->mTimedDocumentRoot) {
    tmp->mTimedDocumentRoot->Traverse(&cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSVGElement,nsSVGSVGElementBase)

DOMCI_NODE_DATA(SVGSVGElement, nsSVGSVGElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsSVGSVGElement)
  NS_NODE_INTERFACE_TABLE8(nsSVGSVGElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGSVGElement,
                           nsIDOMSVGFitToViewBox, nsIDOMSVGLocatable,
                           nsIDOMSVGZoomAndPan)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSVGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSVGElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSVGElement::nsSVGSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                 FromParser aFromParser)
  : nsSVGSVGElementBase(aNodeInfo),
    mViewportWidth(0),
    mViewportHeight(0),
    mCurrentTranslate(0.0f, 0.0f),
    mCurrentScale(1.0f),
    mPreviousTranslate(0.0f, 0.0f),
    mPreviousScale(1.0f),
    mStartAnimationOnBindToTree(aFromParser == NOT_FROM_PARSER ||
                                aFromParser == FROM_PARSER_FRAGMENT ||
                                aFromParser == FROM_PARSER_XSLT),
    mImageNeedsTransformInvalidation(false),
    mIsPaintingSVGImageElement(false),
    mHasChildrenOnlyTransform(false),
    mUseCurrentView(false)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

// From NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSVGElement)
nsresult
nsSVGSVGElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nullptr;
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsSVGSVGElement *it = new nsSVGSVGElement(ni.forget(), NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<nsSVGSVGElement*>(this)->CopyInnerTo(it);
  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return NS_FAILED(rv1) ? rv1 : rv2;
}


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
  *aPixelUnitToMillimeterX = MM_PER_INCH_FLOAT / 96;
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
  *aScreenPixelToMillimeterX = MM_PER_INCH_FLOAT / 96;
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
nsSVGSVGElement::GetUseCurrentView(bool *aUseCurrentView)
{
  *aUseCurrentView = mUseCurrentView;
  return NS_OK;
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
  *aCurrentScale = mCurrentScale;
  return NS_OK;
}

#define CURRENT_SCALE_MAX 16.0f
#define CURRENT_SCALE_MIN 0.0625f

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentScale(float aCurrentScale)
{
  return SetCurrentScaleTranslate(aCurrentScale,
    mCurrentTranslate.GetX(), mCurrentTranslate.GetY());
}

/* readonly attribute DOMSVGPoint currentTranslate; */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTranslate(nsISupports * *aCurrentTranslate)
{
  return mCurrentTranslate.ToDOMVal(this, aCurrentTranslate);
}

/* unsigned long suspendRedraw (in unsigned long max_wait_milliseconds); */
NS_IMETHODIMP
nsSVGSVGElement::SuspendRedraw(uint32_t max_wait_milliseconds, uint32_t *_retval)
{
  // suspendRedraw is a no-op in Mozilla, so it doesn't matter what
  // we set the ID out-param to:
  *_retval = 1;
  return NS_OK;
}

/* void unsuspendRedraw (in unsigned long suspend_handle_id); */
NS_IMETHODIMP
nsSVGSVGElement::UnsuspendRedraw(uint32_t suspend_handle_id)
{
  // no-op
  return NS_OK;
}

/* void unsuspendRedrawAll (); */
NS_IMETHODIMP
nsSVGSVGElement::UnsuspendRedrawAll()
{
  // no-op
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
  if (NS_SMILEnabled()) {
    if (mTimedDocumentRoot) {
      mTimedDocumentRoot->Pause(nsSMILTimeContainer::PAUSE_SCRIPT);
    }
    // else we're not the outermost <svg> or not bound to a tree, so silently fail
    return NS_OK;
  }
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::PauseAnimations");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unpauseAnimations (); */
NS_IMETHODIMP
nsSVGSVGElement::UnpauseAnimations()
{
  if (NS_SMILEnabled()) {
    if (mTimedDocumentRoot) {
      mTimedDocumentRoot->Resume(nsSMILTimeContainer::PAUSE_SCRIPT);
    }
    // else we're not the outermost <svg> or not bound to a tree, so silently fail
    return NS_OK;
  }
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::UnpauseAnimations");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean animationsPaused (); */
NS_IMETHODIMP
nsSVGSVGElement::AnimationsPaused(bool *_retval)
{
  if (NS_SMILEnabled()) {
    nsSMILTimeContainer* root = GetTimedDocumentRoot();
    *_retval = root && root->IsPausedByType(nsSMILTimeContainer::PAUSE_SCRIPT);
    return NS_OK;
  }
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::AnimationsPaused");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* float getCurrentTime (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCurrentTime(float *_retval)
{
  if (NS_SMILEnabled()) {
    nsSMILTimeContainer* root = GetTimedDocumentRoot();
    if (root) {
      double fCurrentTimeMs = double(root->GetCurrentTime());
      *_retval = (float)(fCurrentTimeMs / PR_MSEC_PER_SEC);
    } else {
      *_retval = 0.f;
    }
    return NS_OK;
  }
  NS_NOTYETIMPLEMENTED("nsSVGSVGElement::GetCurrentTime");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCurrentTime (in float seconds); */
NS_IMETHODIMP
nsSVGSVGElement::SetCurrentTime(float seconds)
{
  NS_ENSURE_FINITE(seconds, NS_ERROR_ILLEGAL_VALUE);
  if (NS_SMILEnabled()) {
    if (mTimedDocumentRoot) {
      // Make sure the timegraph is up-to-date
      FlushAnimations();
      double fMilliseconds = double(seconds) * PR_MSEC_PER_SEC;
      // Round to nearest whole number before converting, to avoid precision
      // errors
      nsSMILTime lMilliseconds = int64_t(NS_round(fMilliseconds));
      mTimedDocumentRoot->SetCurrentTime(lMilliseconds);
      AnimationNeedsResample();
      // Trigger synchronous sample now, to:
      //  - Make sure we get an up-to-date paint after this method
      //  - re-enable event firing (it got disabled during seeking, and it
      //  doesn't get re-enabled until the first sample after the seek -- so
      //  let's make that happen now.)
      FlushAnimations();
    } // else we're not the outermost <svg> or not bound to a tree, so silently
      // fail
    return NS_OK;
  }
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
                                   bool *_retval)
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
                                bool *_retval)
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
  NS_ADDREF(*_retval = new DOMSVGNumber());
  return NS_OK;
}

/* nsIDOMSVGLength createSVGLength (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGLength(nsIDOMSVGLength **_retval)
{
  NS_ADDREF(*_retval = new DOMSVGLength());
  return NS_OK;
}

/* SVGAngle createSVGAngle (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGAngle(nsISupports **_retval)
{
  nsSVGAngle* angle = new nsSVGAngle();
  angle->Init();
  NS_ADDREF(*_retval = new SVGAngle(angle, this, SVGAngle::CreatedValue));
  return NS_OK;
}

/* DOMSVGPoint createSVGPoint (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGPoint(nsISupports **_retval)
{
  NS_ADDREF(*_retval = new DOMSVGPoint(0, 0));
  return NS_OK;
}

/* DOMSVGMatrix createSVGMatrix (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGMatrix(nsISupports **_retval)
{
  NS_ADDREF(*_retval = new DOMSVGMatrix());
  return NS_OK;
}

/* nsIDOMSVGRect createSVGRect (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGRect(nsIDOMSVGRect **_retval)
{
  return NS_NewSVGRect(_retval);
}

/* DOMSVGTransform createSVGTransform (); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGTransform(nsISupports **_retval)
{
  NS_ADDREF(*_retval = new DOMSVGTransform());
  return NS_OK;
}

/* DOMSVGTransform createSVGTransformFromMatrix (in DOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGSVGElement::CreateSVGTransformFromMatrix(nsISupports *matrix,
                                              nsISupports **_retval)
{
  nsCOMPtr<DOMSVGMatrix> domItem = do_QueryInterface(matrix);
  if (!domItem) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }

  NS_ADDREF(*_retval = new DOMSVGTransform(domItem->Matrix()));
  return NS_OK;
}

/* nsIDOMElement getElementById (in DOMString elementId); */
NS_IMETHODIMP
nsSVGSVGElement::GetElementById(const nsAString & elementId, nsIDOMElement **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  nsAutoString selector(NS_LITERAL_STRING("#"));
  nsStyleUtil::AppendEscapedCSSIdent(PromiseFlatString(elementId), selector);
  ErrorResult rv;
  nsIContent* element = QuerySelector(selector, rv);
  if (!rv.Failed() && element) {
    return CallQueryInterface(element, _retval);
  }
  return rv.ErrorCode();
}

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP
nsSVGSVGElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  return mViewBox.ToDOMAnimatedRect(aViewBox, this);
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGSVGElement::GetPreserveAspectRatio(nsISupports
                                        **aPreserveAspectRatio)
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(aPreserveAspectRatio, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGLocatable methods

/* readonly attribute nsIDOMSVGElement nearestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetNearestViewportElement(nsIDOMSVGElement * *aNearestViewportElement)
{
  *aNearestViewportElement = SVGContentUtils::GetNearestViewportElement(this).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGElement farthestViewportElement; */
NS_IMETHODIMP
nsSVGSVGElement::GetFarthestViewportElement(nsIDOMSVGElement * *aFarthestViewportElement)
{
  NS_IF_ADDREF(*aFarthestViewportElement = SVGContentUtils::GetOuterSVGElement(this));
  return NS_OK;
}

/* nsIDOMSVGRect getBBox (); */
NS_IMETHODIMP
nsSVGSVGElement::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nullptr;

  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);

  if (!frame || (frame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD))
    return NS_ERROR_FAILURE;

  nsISVGChildFrame* svgframe = do_QueryFrame(frame);
  if (svgframe) {
    return NS_NewSVGRect(_retval, nsSVGUtils::GetBBox(frame));
  }
  return NS_ERROR_NOT_IMPLEMENTED; // XXX: outer svg
}

/* DOMSVGMatrix getCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetCTM(nsISupports * *aCTM)
{
  gfxMatrix m = SVGContentUtils::GetCTM(this, false);
  *aCTM = m.IsSingular() ? nullptr : new DOMSVGMatrix(m);
  NS_IF_ADDREF(*aCTM);
  return NS_OK;
}

/* DOMSVGMatrix getScreenCTM (); */
NS_IMETHODIMP
nsSVGSVGElement::GetScreenCTM(nsISupports **aCTM)
{
  gfxMatrix m = SVGContentUtils::GetCTM(this, true);
  *aCTM = m.IsSingular() ? nullptr : new DOMSVGMatrix(m);
  NS_IF_ADDREF(*aCTM);
  return NS_OK;
}

/* DOMSVGMatrix getTransformToElement (in nsIDOMSVGElement element); */
NS_IMETHODIMP
nsSVGSVGElement::GetTransformToElement(nsIDOMSVGElement *element,
                                       nsISupports **_retval)
{
  if (!element)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsresult rv;
  *_retval = nullptr;
  nsCOMPtr<DOMSVGMatrix> ourScreenCTM;
  nsCOMPtr<DOMSVGMatrix> targetScreenCTM;
  nsCOMPtr<nsIDOMSVGLocatable> target = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return rv;

  // the easiest way to do this (if likely to increase rounding error):
  GetScreenCTM(getter_AddRefs(ourScreenCTM));
  if (!ourScreenCTM) return NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE;
  target->GetScreenCTM(getter_AddRefs(targetScreenCTM));
  if (!targetScreenCTM) return NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE;
  ErrorResult result;
  nsCOMPtr<DOMSVGMatrix> tmp = targetScreenCTM->Inverse(result);
  if (result.Failed()) return result.ErrorCode();
  if (NS_FAILED(rv)) return rv;
  *_retval = tmp->Multiply(*ourScreenCTM).get();  // addrefs, so we don't
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGZoomAndPan methods

/* attribute unsigned short zoomAndPan; */
NS_IMETHODIMP
nsSVGSVGElement::GetZoomAndPan(uint16_t *aZoomAndPan)
{
  nsSVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mEnumAttributes[
                       nsSVGViewElement::ZOOMANDPAN].IsExplicitlySet()) {
    *aZoomAndPan = viewElement->mEnumAttributes[
                     nsSVGViewElement::ZOOMANDPAN].GetAnimValue();
  } else {
    *aZoomAndPan = mEnumAttributes[ZOOMANDPAN].GetAnimValue();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::SetZoomAndPan(uint16_t aZoomAndPan)
{
  if (aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this);
    return NS_OK;
  }

  return NS_ERROR_RANGE_ERR;
}

//----------------------------------------------------------------------
// helper methods for implementing SVGZoomEvent:

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentScaleTranslate(float s, float x, float y)
{
  NS_ENSURE_FINITE3(s, x, y, NS_ERROR_ILLEGAL_VALUE);

  if (s == mCurrentScale &&
      x == mCurrentTranslate.GetX() && y == mCurrentTranslate.GetY()) {
    return NS_OK;
  }

  // Prevent bizarre behaviour and maxing out of CPU and memory by clamping
  if (s < CURRENT_SCALE_MIN)
    s = CURRENT_SCALE_MIN;
  else if (s > CURRENT_SCALE_MAX)
    s = CURRENT_SCALE_MAX;
  
  // IMPORTANT: If either mCurrentTranslate *or* mCurrentScale is changed then
  // mPreviousTranslate_x, mPreviousTranslate_y *and* mPreviousScale must all
  // be updated otherwise SVGZoomEvents will end up with invalid data. I.e. an
  // SVGZoomEvent's properties previousScale and previousTranslate must contain
  // the state of currentScale and currentTranslate immediately before the
  // change that caused the event's dispatch, which is *not* necessarily the
  // same thing as the values of currentScale and currentTranslate prior to
  // their own last change.
  mPreviousScale = mCurrentScale;
  mPreviousTranslate = mCurrentTranslate;
  
  mCurrentScale = s;
  mCurrentTranslate = nsSVGTranslatePoint(x, y);

  // now dispatch the appropriate event if we are the root element
  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    if (presShell && IsRoot()) {
      bool scaling = (mPreviousScale != mCurrentScale);
      nsEventStatus status = nsEventStatus_eIgnore;
      nsGUIEvent event(true, scaling ? NS_SVG_ZOOM : NS_SVG_SCROLL, 0);
      event.eventStructType = scaling ? NS_SVGZOOM_EVENT : NS_SVG_EVENT;
      presShell->HandleDOMEventWithTarget(this, &event, &status);
      InvalidateTransformNotifyFrame();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGSVGElement::SetCurrentTranslate(float x, float y)
{
  return SetCurrentScaleTranslate(mCurrentScale, x, y);
}

nsSMILTimeContainer*
nsSVGSVGElement::GetTimedDocumentRoot()
{
  if (mTimedDocumentRoot) {
    return mTimedDocumentRoot;
  }

  // We must not be the outermost <svg> element, try to find it
  nsSVGSVGElement *outerSVGElement =
    SVGContentUtils::GetOuterSVGElement(this);

  if (outerSVGElement) {
    return outerSVGElement->GetTimedDocumentRoot();
  }
  // invalid structure
  return nullptr;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGSVGElement::IsAttributeMapped(const nsIAtom* name) const
{
  // We want to map the 'width' and 'height' attributes into style for
  // outer-<svg>, except when the attributes aren't set (since their default
  // values of '100%' can cause unexpected and undesirable behaviour for SVG
  // inline in HTML). We rely on nsSVGElement::UpdateContentStyleRule() to
  // prevent mapping of the default values into style (it only maps attributes
  // that are set). We also rely on a check in nsSVGElement::
  // UpdateContentStyleRule() to prevent us mapping the attributes when they're
  // given a <length> value that is not currently recognized by the SVG
  // specification.

  if (!IsInner() && (name == nsGkAtoms::width || name == nsGkAtoms::height)) {
    return true;
  }

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

  return FindAttributeDependence(name, map) ||
    nsSVGSVGElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
nsSVGSVGElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->message == NS_SVG_LOAD) {
    if (mTimedDocumentRoot) {
      mTimedDocumentRoot->Begin();
      // Set 'resample needed' flag, so that if any script calls a DOM method
      // that requires up-to-date animations before our first sample callback,
      // we'll force a synchronous sample.
      AnimationNeedsResample();
    }
  }
  return nsSVGSVGElementBase::PreHandleEvent(aVisitor);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
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

// Helper for GetViewBoxTransform on root <svg> node
// * aLength: internal value for our <svg> width or height attribute.
// * aViewportLength: length of the corresponding dimension of the viewport.
// * aSelf: the outermost <svg> node itself.
// NOTE: aSelf is not an ancestor viewport element, so it can't be used to
// resolve percentage lengths. (It can only be used to resolve
// 'em'/'ex'-valued units).
inline float
ComputeSynthesizedViewBoxDimension(const nsSVGLength2& aLength,
                                   float aViewportLength,
                                   const nsSVGSVGElement* aSelf)
{
  if (aLength.IsPercentage()) {
    return aViewportLength * aLength.GetAnimValInSpecifiedUnits() / 100.0f;
  }

  return aLength.GetAnimValue(const_cast<nsSVGSVGElement*>(aSelf));
}

//----------------------------------------------------------------------
// public helpers:

gfxMatrix
nsSVGSVGElement::GetViewBoxTransform() const
{
  float viewportWidth, viewportHeight;
  if (IsInner()) {
    nsSVGSVGElement *ctx = GetCtx();
    viewportWidth = mLengthAttributes[WIDTH].GetAnimValue(ctx);
    viewportHeight = mLengthAttributes[HEIGHT].GetAnimValue(ctx);
  } else {
    viewportWidth = mViewportWidth;
    viewportHeight = mViewportHeight;
  }

  if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  nsSVGViewBoxRect viewBox =
    GetViewBoxWithSynthesis(viewportWidth, viewportHeight);

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    return gfxMatrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  return SVGContentUtils::GetViewBoxTransform(this,
                                              viewportWidth, viewportHeight,
                                              viewBox.x, viewBox.y,
                                              viewBox.width, viewBox.height,
                                              GetPreserveAspectRatioWithOverride());
}

void
nsSVGSVGElement::UpdateHasChildrenOnlyTransform()
{
  bool hasChildrenOnlyTransform =
    HasViewBoxOrSyntheticViewBox() ||
    (IsRoot() && (mCurrentTranslate != nsSVGTranslatePoint(0.0f, 0.0f) ||
                  mCurrentScale != 1.0f));
  mHasChildrenOnlyTransform = hasChildrenOnlyTransform;
}

void
nsSVGSVGElement::ChildrenOnlyTransformChanged(uint32_t aFlags)
{
  // Avoid wasteful calls:
  NS_ABORT_IF_FALSE(!(GetPrimaryFrame()->GetStateBits() &
                      NS_STATE_SVG_NONDISPLAY_CHILD),
                    "Non-display SVG frames don't maintain overflow rects");

  nsChangeHint changeHint;

  bool hadChildrenOnlyTransform = mHasChildrenOnlyTransform;

  UpdateHasChildrenOnlyTransform();

  if (hadChildrenOnlyTransform != mHasChildrenOnlyTransform) {
    // Reconstruct the frame tree to handle stacking context changes:
    // XXXjwatt don't do this for root-<svg> or even outer-<svg>?
    changeHint = nsChangeHint_ReconstructFrame;
  } else {
    // We just assume the old and new transforms are different.
    changeHint = nsChangeHint(nsChangeHint_RepaintFrame |
                   nsChangeHint_UpdateOverflow |
                   nsChangeHint_ChildrenOnlyTransform);
  }

  // If we're not reconstructing the frame tree, then we only call
  // PostRestyleEvent if we're not being called under reflow to avoid recursing
  // to death. See bug 767056 comments 10 and 12. Since our nsSVGOuterSVGFrame
  // is being reflowed we're going to invalidate and repaint its entire area
  // anyway (which will include our children).
  if ((changeHint & nsChangeHint_ReconstructFrame) ||
      !(aFlags & eDuringReflow)) {
    nsLayoutUtils::PostRestyleEvent(this, nsRestyleHint(0), changeHint);
  }
}

nsresult
nsSVGSVGElement::BindToTree(nsIDocument* aDocument,
                            nsIContent* aParent,
                            nsIContent* aBindingParent,
                            bool aCompileEventHandlers)
{
  nsSMILAnimationController* smilController = nullptr;

  if (aDocument) {
    smilController = aDocument->GetAnimationController();
    if (smilController) {
      // SMIL is enabled in this document
      if (WillBeOutermostSVG(aParent, aBindingParent)) {
        // We'll be the outermost <svg> element.  We'll need a time container.
        if (!mTimedDocumentRoot) {
          mTimedDocumentRoot = new nsSMILTimeContainer();
          NS_ENSURE_TRUE(mTimedDocumentRoot, NS_ERROR_OUT_OF_MEMORY);
        }
      } else {
        // We're a child of some other <svg> element, so we don't need our own
        // time container. However, we need to make sure that we'll get a
        // kick-start if we get promoted to be outermost later on.
        mTimedDocumentRoot = nullptr;
        mStartAnimationOnBindToTree = true;
      }
    }
  }

  nsresult rv = nsSVGSVGElementBase::BindToTree(aDocument, aParent,
                                                aBindingParent,
                                                aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  if (mTimedDocumentRoot && smilController) {
    rv = mTimedDocumentRoot->SetParent(smilController);
    if (mStartAnimationOnBindToTree) {
      mTimedDocumentRoot->Begin();
      mStartAnimationOnBindToTree = false;
    }
  }

  return rv;
}

void
nsSVGSVGElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->SetParent(nullptr);
  }

  nsSVGSVGElementBase::UnbindFromTree(aDeep, aNullParent);
}

//----------------------------------------------------------------------
// implementation helpers

bool
nsSVGSVGElement::WillBeOutermostSVG(nsIContent* aParent,
                                    nsIContent* aBindingParent) const
{
  nsIContent* parent = aBindingParent ? aBindingParent : aParent;

  while (parent && parent->IsSVG()) {
    nsIAtom* tag = parent->Tag();
    if (tag == nsGkAtoms::foreignObject) {
      // SVG in a foreignObject must have its own <svg> (nsSVGOuterSVGFrame).
      return false;
    }
    if (tag == nsGkAtoms::svg) {
      return false;
    }
    parent = parent->GetParent();
  }

  return true;
}

void
nsSVGSVGElement::InvalidateTransformNotifyFrame()
{
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    nsISVGSVGFrame* svgframe = do_QueryFrame(frame);
    // might fail this check if we've failed conditional processing
    if (svgframe) {
      svgframe->NotifyViewportOrTransformChanged(
                  nsISVGChildFrame::TRANSFORM_CHANGED);
    }
  }
}

bool
nsSVGSVGElement::HasPreserveAspectRatio()
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::preserveAspectRatio) ||
    mPreserveAspectRatio.IsAnimated();
}

nsSVGViewElement*
nsSVGSVGElement::GetCurrentViewElement() const
{
  if (mCurrentViewID) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      Element *element = doc->GetElementById(*mCurrentViewID);
      if (element && element->IsSVG(nsGkAtoms::view)) {
        return static_cast<nsSVGViewElement*>(element);
      }
    }
  }
  return nullptr;
}

nsSVGViewBoxRect
nsSVGSVGElement::GetViewBoxWithSynthesis(
  float aViewportWidth, float aViewportHeight) const
{
  // The logic here should match HasViewBox().
  nsSVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mViewBox.IsExplicitlySet()) {
    return viewElement->mViewBox.GetAnimValue();
  }
  if (mViewBox.IsExplicitlySet()) {
    return mViewBox.GetAnimValue();
  }

  if (ShouldSynthesizeViewBox()) {
    // Special case -- fake a viewBox, using height & width attrs.
    // (Use |this| as context, since if we get here, we're outermost <svg>.)
    return nsSVGViewBoxRect(0, 0,
              ComputeSynthesizedViewBoxDimension(mLengthAttributes[WIDTH],
                                                 mViewportWidth, this),
              ComputeSynthesizedViewBoxDimension(mLengthAttributes[HEIGHT],
                                                 mViewportHeight, this));

  }

  // No viewBox attribute, so we shouldn't auto-scale. This is equivalent
  // to having a viewBox that exactly matches our viewport size.
  return nsSVGViewBoxRect(0, 0, aViewportWidth, aViewportHeight);
}

SVGPreserveAspectRatio
nsSVGSVGElement::GetPreserveAspectRatioWithOverride() const
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc && doc->IsBeingUsedAsImage()) {
    const SVGPreserveAspectRatio *pAROverridePtr = GetPreserveAspectRatioProperty();
    if (pAROverridePtr) {
      return *pAROverridePtr;
    }
  }

  nsSVGViewElement* viewElement = GetCurrentViewElement();

  // This check is equivalent to "!HasViewBox() && ShouldSynthesizeViewBox()".
  // We're just holding onto the viewElement that HasViewBox() would look up,
  // so that we don't have to look it up again later.
  if (!((viewElement && viewElement->mViewBox.IsExplicitlySet()) ||
        mViewBox.IsExplicitlySet()) &&
      ShouldSynthesizeViewBox()) {
    // If we're synthesizing a viewBox, use preserveAspectRatio="none";
    return SVGPreserveAspectRatio(SVG_PRESERVEASPECTRATIO_NONE, SVG_MEETORSLICE_SLICE);
  }

  if (viewElement && viewElement->mPreserveAspectRatio.IsExplicitlySet()) {
    return viewElement->mPreserveAspectRatio.GetAnimValue();
  }
  return mPreserveAspectRatio.GetAnimValue();
}

//----------------------------------------------------------------------
// nsSVGSVGElement

float
nsSVGSVGElement::GetLength(uint8_t aCtxType)
{
  float h, w;

  nsSVGViewElement* viewElement = GetCurrentViewElement();
  const nsSVGViewBoxRect* viewbox = nullptr;

  // The logic here should match HasViewBox().
  if (viewElement && viewElement->mViewBox.IsExplicitlySet()) {
    viewbox = &viewElement->mViewBox.GetAnimValue();
  } else if (mViewBox.IsExplicitlySet()) {
    viewbox = &mViewBox.GetAnimValue();
  }

  if (viewbox) {
    w = viewbox->width;
    h = viewbox->height;
  } else if (IsInner()) {
    nsSVGSVGElement *ctx = GetCtx();
    w = mLengthAttributes[WIDTH].GetAnimValue(ctx);
    h = mLengthAttributes[HEIGHT].GetAnimValue(ctx);
  } else if (ShouldSynthesizeViewBox()) {
    w = ComputeSynthesizedViewBoxDimension(mLengthAttributes[WIDTH],
                                           mViewportWidth, this);
    h = ComputeSynthesizedViewBoxDimension(mLengthAttributes[HEIGHT],
                                           mViewportHeight, this);
  } else {
    w = mViewportWidth;
    h = mViewportHeight;
  }

  w = NS_MAX(w, 0.0f);
  h = NS_MAX(h, 0.0f);

  switch (aCtxType) {
  case SVGContentUtils::X:
    return w;
  case SVGContentUtils::Y:
    return h;
  case SVGContentUtils::XY:
    return float(SVGContentUtils::ComputeNormalizedHypotenuse(w, h));
  }
  return 0;
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
nsSVGSVGElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                          TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  if (IsInner()) {
    float x, y;
    const_cast<nsSVGSVGElement*>(this)->GetAnimatedLengthValues(&x, &y, nullptr);
    if (aWhich == eAllTransforms) {
      // the common case
      return GetViewBoxTransform() * gfxMatrix().Translate(gfxPoint(x, y)) * aMatrix;
    }
    if (aWhich == eUserSpaceToParent) {
      return gfxMatrix().Translate(gfxPoint(x, y)) * aMatrix;
    }
    NS_ABORT_IF_FALSE(aWhich == eChildToUserSpace, "Unknown TransformTypes");
    return GetViewBoxTransform(); // no need to multiply identity aMatrix
  }

  if (aWhich == eUserSpaceToParent) {
    // only inner-<svg> has eUserSpaceToParent transforms
    return aMatrix;
  }

  if (IsRoot()) {
    gfxMatrix zoomPanTM;
    zoomPanTM.Translate(gfxPoint(mCurrentTranslate.GetX(), mCurrentTranslate.GetY()));
    zoomPanTM.Scale(mCurrentScale, mCurrentScale);
    gfxMatrix matrix = mFragmentIdentifierTransform ? 
                         *mFragmentIdentifierTransform * aMatrix : aMatrix;
    return GetViewBoxTransform() * zoomPanTM * matrix;
  }

  // outer-<svg>, but inline in some other content:
  return GetViewBoxTransform() * aMatrix;
}

/* virtual */ bool
nsSVGSVGElement::HasValidDimensions() const
{
  return !IsInner() ||
    ((!mLengthAttributes[WIDTH].IsExplicitlySet() ||
       mLengthAttributes[WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
     (!mLengthAttributes[HEIGHT].IsExplicitlySet() || 
       mLengthAttributes[HEIGHT].GetAnimValInSpecifiedUnits() > 0));
}

nsSVGElement::LengthAttributesInfo
nsSVGSVGElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGViewBox *
nsSVGSVGElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
nsSVGSVGElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

bool
nsSVGSVGElement::HasViewBox() const
{
  nsSVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mViewBox.IsExplicitlySet()) {
    return true;
  }
  return mViewBox.IsExplicitlySet();
}

bool
nsSVGSVGElement::ShouldSynthesizeViewBox() const
{
  NS_ABORT_IF_FALSE(!HasViewBox(),
                    "Should only be called if we lack a viewBox");

  nsIDocument* doc = GetCurrentDoc();
  return doc &&
    doc->IsBeingUsedAsImage() &&
    !mIsPaintingSVGImageElement &&
    !GetParent();
}


// Callback function, for freeing uint64_t values stored in property table
static void
ReleasePreserveAspectRatioPropertyValue(void*    aObject,       /* unused */
                                        nsIAtom* aPropertyName, /* unused */
                                        void*    aPropertyValue,
                                        void*    aData          /* unused */)
{
  SVGPreserveAspectRatio* valPtr =
    static_cast<SVGPreserveAspectRatio*>(aPropertyValue);
  delete valPtr;
}

bool
nsSVGSVGElement::
  SetPreserveAspectRatioProperty(const SVGPreserveAspectRatio& aPAR)
{
  SVGPreserveAspectRatio* pAROverridePtr = new SVGPreserveAspectRatio(aPAR);
  nsresult rv = SetProperty(nsGkAtoms::overridePreserveAspectRatio,
                            pAROverridePtr,
                            ReleasePreserveAspectRatioPropertyValue,
                            true);
  NS_ABORT_IF_FALSE(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
                    "Setting override value when it's already set...?"); 

  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    // property-insertion failed (e.g. OOM in property-table code)
    delete pAROverridePtr;
    return false;
  }
  return true;
}

const SVGPreserveAspectRatio*
nsSVGSVGElement::GetPreserveAspectRatioProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::overridePreserveAspectRatio);
  if (valPtr) {
    return static_cast<SVGPreserveAspectRatio*>(valPtr);
  }
  return nullptr;
}

bool
nsSVGSVGElement::ClearPreserveAspectRatioProperty()
{
  void* valPtr = UnsetProperty(nsGkAtoms::overridePreserveAspectRatio);
  delete static_cast<SVGPreserveAspectRatio*>(valPtr);
  return valPtr;
}

void
nsSVGSVGElement::
  SetImageOverridePreserveAspectRatio(const SVGPreserveAspectRatio& aPAR)
{
#ifdef DEBUG
  NS_ABORT_IF_FALSE(GetCurrentDoc()->IsBeingUsedAsImage(),
                    "should only override preserveAspectRatio in images");
#endif

  bool hasViewBox = HasViewBox();
  if (!hasViewBox && ShouldSynthesizeViewBox()) {
    // My non-<svg:image> clients will have been painting me with a synthesized
    // viewBox, but my <svg:image> client that's about to paint me now does NOT
    // want that.  Need to tell ourselves to flush our transform.
    mImageNeedsTransformInvalidation = true;
  }
  mIsPaintingSVGImageElement = true;

  if (!hasViewBox) {
    return; // preserveAspectRatio irrelevant (only matters if we have viewBox)
  }

  if (aPAR.GetDefer() && HasPreserveAspectRatio()) {
    return; // Referring element defers to my own preserveAspectRatio value.
  }

  if (SetPreserveAspectRatioProperty(aPAR)) {
    mImageNeedsTransformInvalidation = true;
  }
}

void
nsSVGSVGElement::ClearImageOverridePreserveAspectRatio()
{
#ifdef DEBUG
  NS_ABORT_IF_FALSE(GetCurrentDoc()->IsBeingUsedAsImage(),
                    "should only override image preserveAspectRatio in images");
#endif

  mIsPaintingSVGImageElement = false;
  if (!HasViewBox() && ShouldSynthesizeViewBox()) {
    // My non-<svg:image> clients will want to paint me with a synthesized
    // viewBox, but my <svg:image> client that just painted me did NOT
    // use that.  Need to tell ourselves to flush our transform.
    mImageNeedsTransformInvalidation = true;
  }

  if (ClearPreserveAspectRatioProperty()) {
    mImageNeedsTransformInvalidation = true;
  }
}

void
nsSVGSVGElement::FlushImageTransformInvalidation()
{
  NS_ABORT_IF_FALSE(!GetParent(), "Should only be called on root node");
  NS_ABORT_IF_FALSE(GetCurrentDoc()->IsBeingUsedAsImage(),
                    "Should only be called on image documents");

  if (mImageNeedsTransformInvalidation) {
    InvalidateTransformNotifyFrame();
    mImageNeedsTransformInvalidation = false;
  }
}

// Callback function, for freeing uint64_t values stored in property table
static void
ReleaseViewBoxPropertyValue(void*    aObject,       /* unused */
                            nsIAtom* aPropertyName, /* unused */
                            void*    aPropertyValue,
                            void*    aData          /* unused */)
{
  nsSVGViewBoxRect* valPtr =
    static_cast<nsSVGViewBoxRect*>(aPropertyValue);
  delete valPtr;
}

bool
nsSVGSVGElement::SetViewBoxProperty(const nsSVGViewBoxRect& aViewBox)
{
  nsSVGViewBoxRect* pViewBoxOverridePtr = new nsSVGViewBoxRect(aViewBox);
  nsresult rv = SetProperty(nsGkAtoms::viewBox,
                            pViewBoxOverridePtr,
                            ReleaseViewBoxPropertyValue,
                            true);
  NS_ABORT_IF_FALSE(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
                    "Setting override value when it's already set...?"); 

  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    // property-insertion failed (e.g. OOM in property-table code)
    delete pViewBoxOverridePtr;
    return false;
  }
  return true;
}

const nsSVGViewBoxRect*
nsSVGSVGElement::GetViewBoxProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::viewBox);
  if (valPtr) {
    return static_cast<nsSVGViewBoxRect*>(valPtr);
  }
  return nullptr;
}

bool
nsSVGSVGElement::ClearViewBoxProperty()
{
  void* valPtr = UnsetProperty(nsGkAtoms::viewBox);
  delete static_cast<nsSVGViewBoxRect*>(valPtr);
  return valPtr;
}

bool
nsSVGSVGElement::SetZoomAndPanProperty(uint16_t aValue)
{
  nsresult rv = SetProperty(nsGkAtoms::zoomAndPan,
                            reinterpret_cast<void*>(aValue),
                            nullptr, true);
  NS_ABORT_IF_FALSE(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
                    "Setting override value when it's already set...?"); 

  return NS_SUCCEEDED(rv);
}

uint16_t
nsSVGSVGElement::GetZoomAndPanProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::zoomAndPan);
  if (valPtr) {
    return reinterpret_cast<uintptr_t>(valPtr);
  }
  return nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_UNKNOWN;
}

bool
nsSVGSVGElement::ClearZoomAndPanProperty()
{
  return UnsetProperty(nsGkAtoms::zoomAndPan);
}

