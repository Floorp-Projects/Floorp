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
#include "mozilla/dom/SVGMatrix.h"
#include "DOMSVGPoint.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsISVGSVGFrame.h" //XXX
#include "nsSVGRect.h"
#include "nsError.h"
#include "nsISVGChildFrame.h"
#include "nsGUIEvent.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGSVGElementBinding.h"
#include "nsSVGUtils.h"
#include "mozilla/dom/SVGViewElement.h"
#include "nsStyleUtil.h"
#include "SVGContentUtils.h"

#include "nsEventDispatcher.h"
#include "nsSMILTimeContainer.h"
#include "nsSMILAnimationController.h"
#include "nsSMILTypes.h"
#include "nsIContentIterator.h"
#include "SVGAngle.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include <algorithm>

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(SVG)

namespace mozilla {
namespace dom {

JSObject*
SVGSVGElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGSVGElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGTranslatePoint, mElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGTranslatePoint)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGTranslatePoint)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTranslatePoint)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We have to qualify nsISVGPoint because NS_GET_IID looks for a class in the
  // global namespace
  NS_INTERFACE_MAP_ENTRY(mozilla::nsISVGPoint)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsISVGPoint*
DOMSVGTranslatePoint::Clone()
{
  return new DOMSVGTranslatePoint(this);
}

nsISupports*
DOMSVGTranslatePoint::GetParentObject()
{
  return static_cast<nsIDOMSVGElement*>(mElement);
}

void
DOMSVGTranslatePoint::SetX(float aValue, ErrorResult& rv)
{
  mElement->SetCurrentTranslate(aValue, mPt.GetY());
}

void
DOMSVGTranslatePoint::SetY(float aValue, ErrorResult& rv)
{
  mElement->SetCurrentTranslate(mPt.GetX(), aValue);
}

already_AddRefed<nsISVGPoint>
DOMSVGTranslatePoint::MatrixTransform(SVGMatrix& matrix)
{
  float a = matrix.A(), b = matrix.B(), c = matrix.C();
  float d = matrix.D(), e = matrix.E(), f = matrix.F();
  float x = mPt.GetX();
  float y = mPt.GetY();

  nsCOMPtr<nsISVGPoint> point = new DOMSVGPoint(a*x + c*y + e, b*x + d*y + f);
  return point.forget();
}

nsSVGElement::LengthInfo SVGSVGElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

nsSVGEnumMapping SVGSVGElement::sZoomAndPanMap[] = {
  {&nsGkAtoms::disable, SVG_ZOOMANDPAN_DISABLE},
  {&nsGkAtoms::magnify, SVG_ZOOMANDPAN_MAGNIFY},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGSVGElement::sEnumInfo[1] =
{
  { &nsGkAtoms::zoomAndPan,
    sZoomAndPanMap,
    SVG_ZOOMANDPAN_MAGNIFY
  }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SVGSVGElement,
                                                SVGSVGElementBase)
  if (tmp->mTimedDocumentRoot) {
    tmp->mTimedDocumentRoot->Unlink();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SVGSVGElement,
                                                  SVGSVGElementBase)
  if (tmp->mTimedDocumentRoot) {
    tmp->mTimedDocumentRoot->Traverse(&cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(SVGSVGElement,SVGSVGElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSVGElement,SVGSVGElementBase)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(SVGSVGElement)
  NS_INTERFACE_TABLE_INHERITED3(SVGSVGElement, nsIDOMNode, nsIDOMElement,
                                nsIDOMSVGElement)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGSVGElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSVGElement::SVGSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                 FromParser aFromParser)
  : SVGSVGElementBase(aNodeInfo),
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
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

// From NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSVGElement)
nsresult
SVGSVGElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nullptr;
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  SVGSVGElement *it = new SVGSVGElement(ni.forget(), NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<SVGSVGElement*>(this)->CopyInnerTo(it);
  if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
    kungFuDeathGrip.swap(*aResult);
  }

  return NS_FAILED(rv1) ? rv1 : rv2;
}


//----------------------------------------------------------------------
// nsIDOMSVGSVGElement methods:

already_AddRefed<SVGAnimatedLength>
SVGSVGElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGSVGElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGSVGElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGSVGElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

float
SVGSVGElement::PixelUnitToMillimeterX()
{
  return MM_PER_INCH_FLOAT / 96;
}

float
SVGSVGElement::PixelUnitToMillimeterY()
{
  return PixelUnitToMillimeterX();
}

float
SVGSVGElement::ScreenPixelToMillimeterX()
{
  return MM_PER_INCH_FLOAT / 96;
}

float
SVGSVGElement::ScreenPixelToMillimeterY()
{
  return ScreenPixelToMillimeterX();
}

bool
SVGSVGElement::UseCurrentView()
{
  return mUseCurrentView;
}

float
SVGSVGElement::CurrentScale()
{
  return mCurrentScale;
}

#define CURRENT_SCALE_MAX 16.0f
#define CURRENT_SCALE_MIN 0.0625f

void
SVGSVGElement::SetCurrentScale(float aCurrentScale)
{
  SetCurrentScaleTranslate(aCurrentScale,
    mCurrentTranslate.GetX(), mCurrentTranslate.GetY());
}

already_AddRefed<nsISVGPoint>
SVGSVGElement::CurrentTranslate()
{
  nsCOMPtr<nsISVGPoint> point = new DOMSVGTranslatePoint(&mCurrentTranslate, this);
  return point.forget();
}

uint32_t
SVGSVGElement::SuspendRedraw(uint32_t max_wait_milliseconds)
{
  // suspendRedraw is a no-op in Mozilla, so it doesn't matter what
  // we return
  return 1;
}

/* void unsuspendRedraw (in unsigned long suspend_handle_id); */
void
SVGSVGElement::UnsuspendRedraw(uint32_t suspend_handle_id)
{
  // no-op
}

/* void unsuspendRedrawAll (); */
void
SVGSVGElement::UnsuspendRedrawAll()
{
  // no-op
}

void
SVGSVGElement::ForceRedraw(ErrorResult& rv)
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  doc->FlushPendingNotifications(Flush_Display);
}

void
SVGSVGElement::PauseAnimations()
{
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->Pause(nsSMILTimeContainer::PAUSE_SCRIPT);
  }
  // else we're not the outermost <svg> or not bound to a tree, so silently fail
}

void
SVGSVGElement::UnpauseAnimations()
{
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->Resume(nsSMILTimeContainer::PAUSE_SCRIPT);
  }
  // else we're not the outermost <svg> or not bound to a tree, so silently fail
}

bool
SVGSVGElement::AnimationsPaused()
{
  nsSMILTimeContainer* root = GetTimedDocumentRoot();
  return root && root->IsPausedByType(nsSMILTimeContainer::PAUSE_SCRIPT);
}

float
SVGSVGElement::GetCurrentTime()
{
  nsSMILTimeContainer* root = GetTimedDocumentRoot();
  if (root) {
    double fCurrentTimeMs = double(root->GetCurrentTime());
    return (float)(fCurrentTimeMs / PR_MSEC_PER_SEC);
  } else {
    return 0.f;
  }
}

void
SVGSVGElement::SetCurrentTime(float seconds)
{
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
  }
  // else we're not the outermost <svg> or not bound to a tree, so silently fail
}

already_AddRefed<nsIDOMSVGNumber>
SVGSVGElement::CreateSVGNumber()
{
  nsCOMPtr<nsIDOMSVGNumber> number = new DOMSVGNumber();
  return number.forget();
}

already_AddRefed<nsIDOMSVGLength>
SVGSVGElement::CreateSVGLength()
{
  nsCOMPtr<nsIDOMSVGLength> length = new DOMSVGLength();
  return length.forget();
}

already_AddRefed<SVGAngle>
SVGSVGElement::CreateSVGAngle()
{
  nsSVGAngle* angle = new nsSVGAngle();
  angle->Init();
  nsRefPtr<SVGAngle> svgangle = new SVGAngle(angle, this, SVGAngle::CreatedValue);
  return svgangle.forget();
}

already_AddRefed<nsISVGPoint>
SVGSVGElement::CreateSVGPoint()
{
  nsCOMPtr<nsISVGPoint> point = new DOMSVGPoint(0, 0);
  return point.forget();
}

already_AddRefed<SVGMatrix>
SVGSVGElement::CreateSVGMatrix()
{
  nsRefPtr<SVGMatrix> matrix = new SVGMatrix();
  return matrix.forget();
}

already_AddRefed<nsIDOMSVGRect>
SVGSVGElement::CreateSVGRect()
{
  nsCOMPtr<nsIDOMSVGRect> rect;
  NS_NewSVGRect(getter_AddRefs(rect));
  return rect.forget();
}

already_AddRefed<DOMSVGTransform>
SVGSVGElement::CreateSVGTransform()
{
  nsRefPtr<DOMSVGTransform> transform = new DOMSVGTransform();
  return transform.forget();
}

already_AddRefed<DOMSVGTransform>
SVGSVGElement::CreateSVGTransformFromMatrix(SVGMatrix& matrix)
{
  nsRefPtr<DOMSVGTransform> transform = new DOMSVGTransform(matrix.Matrix());
  return transform.forget();
}

Element*
SVGSVGElement::GetElementById(const nsAString& elementId, ErrorResult& rv)
{
  nsAutoString selector(NS_LITERAL_STRING("#"));
  nsStyleUtil::AppendEscapedCSSIdent(PromiseFlatString(elementId), selector);
  nsIContent* element = QuerySelector(selector, rv);
  if (!rv.Failed() && element) {
    return element->AsElement();
  }
  return nullptr;
}

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGAnimatedRect>
SVGSVGElement::ViewBox()
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
  mViewBox.ToDOMAnimatedRect(getter_AddRefs(rect), this);
  return rect.forget();
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGSVGElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  return ratio.forget();
}

uint16_t
SVGSVGElement::ZoomAndPan()
{
  SVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mEnumAttributes[
                       SVGViewElement::ZOOMANDPAN].IsExplicitlySet()) {
    return viewElement->mEnumAttributes[
             SVGViewElement::ZOOMANDPAN].GetAnimValue();
  }
  return mEnumAttributes[ZOOMANDPAN].GetAnimValue();
}

void
SVGSVGElement::SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv)
{
  if (aZoomAndPan == SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == SVG_ZOOMANDPAN_MAGNIFY) {
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this);
    return;
  }

  rv.Throw(NS_ERROR_RANGE_ERR);
}

//----------------------------------------------------------------------
// helper methods for implementing SVGZoomEvent:

void
SVGSVGElement::SetCurrentScaleTranslate(float s, float x, float y)
{
  if (s == mCurrentScale &&
      x == mCurrentTranslate.GetX() && y == mCurrentTranslate.GetY()) {
    return;
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
  mCurrentTranslate = SVGPoint(x, y);

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
}

void
SVGSVGElement::SetCurrentTranslate(float x, float y)
{
  SetCurrentScaleTranslate(mCurrentScale, x, y);
}

nsSMILTimeContainer*
SVGSVGElement::GetTimedDocumentRoot()
{
  if (mTimedDocumentRoot) {
    return mTimedDocumentRoot;
  }

  // We must not be the outermost <svg> element, try to find it
  SVGSVGElement *outerSVGElement =
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
SVGSVGElement::IsAttributeMapped(const nsIAtom* name) const
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
    SVGSVGElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
SVGSVGElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
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
  return SVGSVGElementBase::PreHandleEvent(aVisitor);
}

bool
SVGSVGElement::IsEventAttributeName(nsIAtom* aName)
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
// nsSVGElement overrides

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
                                   const SVGSVGElement* aSelf)
{
  if (aLength.IsPercentage()) {
    return aViewportLength * aLength.GetAnimValInSpecifiedUnits() / 100.0f;
  }

  return aLength.GetAnimValue(const_cast<SVGSVGElement*>(aSelf));
}

//----------------------------------------------------------------------
// public helpers:

gfxMatrix
SVGSVGElement::GetViewBoxTransform() const
{
  float viewportWidth, viewportHeight;
  if (IsInner()) {
    SVGSVGElement *ctx = GetCtx();
    viewportWidth = mLengthAttributes[ATTR_WIDTH].GetAnimValue(ctx);
    viewportHeight = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(ctx);
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
SVGSVGElement::UpdateHasChildrenOnlyTransform()
{
  bool hasChildrenOnlyTransform =
    HasViewBoxOrSyntheticViewBox() ||
    (IsRoot() && (mCurrentTranslate != SVGPoint(0.0f, 0.0f) ||
                  mCurrentScale != 1.0f));
  mHasChildrenOnlyTransform = hasChildrenOnlyTransform;
}

void
SVGSVGElement::ChildrenOnlyTransformChanged(uint32_t aFlags)
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
SVGSVGElement::BindToTree(nsIDocument* aDocument,
                          nsIContent* aParent,
                          nsIContent* aBindingParent,
                          bool aCompileEventHandlers)
{
  static const char kSVGStyleSheetURI[] = "resource://gre/res/svg.css";

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

  nsresult rv = SVGSVGElementBase::BindToTree(aDocument, aParent,
                                              aBindingParent,
                                              aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv,rv);

  if (aDocument) {
    // Setup the style sheet during binding, not element construction,
    // because we could move the root SVG element from the document
    // that created it to another document.
    aDocument->EnsureCatalogStyleSheet(kSVGStyleSheetURI);
  }

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
SVGSVGElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->SetParent(nullptr);
  }

  SVGSVGElementBase::UnbindFromTree(aDeep, aNullParent);
}

//----------------------------------------------------------------------
// implementation helpers

bool
SVGSVGElement::WillBeOutermostSVG(nsIContent* aParent,
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
SVGSVGElement::InvalidateTransformNotifyFrame()
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
SVGSVGElement::HasPreserveAspectRatio()
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::preserveAspectRatio) ||
    mPreserveAspectRatio.IsAnimated();
}

SVGViewElement*
SVGSVGElement::GetCurrentViewElement() const
{
  if (mCurrentViewID) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      Element *element = doc->GetElementById(*mCurrentViewID);
      if (element && element->IsSVG(nsGkAtoms::view)) {
        return static_cast<SVGViewElement*>(element);
      }
    }
  }
  return nullptr;
}

nsSVGViewBoxRect
SVGSVGElement::GetViewBoxWithSynthesis(
  float aViewportWidth, float aViewportHeight) const
{
  // The logic here should match HasViewBoxRect().
  SVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mViewBox.HasRect()) {
    return viewElement->mViewBox.GetAnimValue();
  }
  if (mViewBox.HasRect()) {
    return mViewBox.GetAnimValue();
  }

  if (ShouldSynthesizeViewBox()) {
    // Special case -- fake a viewBox, using height & width attrs.
    // (Use |this| as context, since if we get here, we're outermost <svg>.)
    return nsSVGViewBoxRect(0, 0,
              ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_WIDTH],
                                                 mViewportWidth, this),
              ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_HEIGHT],
                                                 mViewportHeight, this));

  }

  // No viewBox attribute, so we shouldn't auto-scale. This is equivalent
  // to having a viewBox that exactly matches our viewport size.
  return nsSVGViewBoxRect(0, 0, aViewportWidth, aViewportHeight);
}

SVGPreserveAspectRatio
SVGSVGElement::GetPreserveAspectRatioWithOverride() const
{
  nsIDocument* doc = GetCurrentDoc();
  if (doc && doc->IsBeingUsedAsImage()) {
    const SVGPreserveAspectRatio *pAROverridePtr = GetPreserveAspectRatioProperty();
    if (pAROverridePtr) {
      return *pAROverridePtr;
    }
  }

  SVGViewElement* viewElement = GetCurrentViewElement();

  // This check is equivalent to "!HasViewBoxRect() && ShouldSynthesizeViewBox()".
  // We're just holding onto the viewElement that HasViewBoxRect() would look up,
  // so that we don't have to look it up again later.
  if (!((viewElement && viewElement->mViewBox.HasRect()) ||
        mViewBox.HasRect()) &&
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
// SVGSVGElement

float
SVGSVGElement::GetLength(uint8_t aCtxType)
{
  float h, w;

  SVGViewElement* viewElement = GetCurrentViewElement();
  const nsSVGViewBoxRect* viewbox = nullptr;

  // The logic here should match HasViewBoxRect().
  if (viewElement && viewElement->mViewBox.HasRect()) {
    viewbox = &viewElement->mViewBox.GetAnimValue();
  } else if (mViewBox.HasRect()) {
    viewbox = &mViewBox.GetAnimValue();
  }

  if (viewbox) {
    w = viewbox->width;
    h = viewbox->height;
  } else if (IsInner()) {
    SVGSVGElement *ctx = GetCtx();
    w = mLengthAttributes[ATTR_WIDTH].GetAnimValue(ctx);
    h = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(ctx);
  } else if (ShouldSynthesizeViewBox()) {
    w = ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_WIDTH],
                                           mViewportWidth, this);
    h = ComputeSynthesizedViewBoxDimension(mLengthAttributes[ATTR_HEIGHT],
                                           mViewportHeight, this);
  } else {
    w = mViewportWidth;
    h = mViewportHeight;
  }

  w = std::max(w, 0.0f);
  h = std::max(h, 0.0f);

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
SVGSVGElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                        TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  if (IsInner()) {
    float x, y;
    const_cast<SVGSVGElement*>(this)->GetAnimatedLengthValues(&x, &y, nullptr);
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
SVGSVGElement::HasValidDimensions() const
{
  return !IsInner() ||
    ((!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
       mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
     (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
       mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0));
}

nsSVGElement::LengthAttributesInfo
SVGSVGElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
SVGSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGViewBox *
SVGSVGElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
SVGSVGElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

bool
SVGSVGElement::HasViewBoxRect() const
{
  SVGViewElement* viewElement = GetCurrentViewElement();
  if (viewElement && viewElement->mViewBox.HasRect()) {
    return true;
  }
  return mViewBox.HasRect();
}

bool
SVGSVGElement::ShouldSynthesizeViewBox() const
{
  NS_ABORT_IF_FALSE(!HasViewBoxRect(),
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
SVGSVGElement::SetPreserveAspectRatioProperty(const SVGPreserveAspectRatio& aPAR)
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
SVGSVGElement::GetPreserveAspectRatioProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::overridePreserveAspectRatio);
  if (valPtr) {
    return static_cast<SVGPreserveAspectRatio*>(valPtr);
  }
  return nullptr;
}

bool
SVGSVGElement::ClearPreserveAspectRatioProperty()
{
  void* valPtr = UnsetProperty(nsGkAtoms::overridePreserveAspectRatio);
  delete static_cast<SVGPreserveAspectRatio*>(valPtr);
  return valPtr;
}

void
SVGSVGElement::
  SetImageOverridePreserveAspectRatio(const SVGPreserveAspectRatio& aPAR)
{
#ifdef DEBUG
  NS_ABORT_IF_FALSE(GetCurrentDoc()->IsBeingUsedAsImage(),
                    "should only override preserveAspectRatio in images");
#endif

  bool hasViewBoxRect = HasViewBoxRect();
  if (!hasViewBoxRect && ShouldSynthesizeViewBox()) {
    // My non-<svg:image> clients will have been painting me with a synthesized
    // viewBox, but my <svg:image> client that's about to paint me now does NOT
    // want that.  Need to tell ourselves to flush our transform.
    mImageNeedsTransformInvalidation = true;
  }
  mIsPaintingSVGImageElement = true;

  if (!hasViewBoxRect) {
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
SVGSVGElement::ClearImageOverridePreserveAspectRatio()
{
#ifdef DEBUG
  NS_ABORT_IF_FALSE(GetCurrentDoc()->IsBeingUsedAsImage(),
                    "should only override image preserveAspectRatio in images");
#endif

  mIsPaintingSVGImageElement = false;
  if (!HasViewBoxRect() && ShouldSynthesizeViewBox()) {
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
SVGSVGElement::FlushImageTransformInvalidation()
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
SVGSVGElement::SetViewBoxProperty(const nsSVGViewBoxRect& aViewBox)
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
SVGSVGElement::GetViewBoxProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::viewBox);
  if (valPtr) {
    return static_cast<nsSVGViewBoxRect*>(valPtr);
  }
  return nullptr;
}

bool
SVGSVGElement::ClearViewBoxProperty()
{
  void* valPtr = UnsetProperty(nsGkAtoms::viewBox);
  delete static_cast<nsSVGViewBoxRect*>(valPtr);
  return valPtr;
}

bool
SVGSVGElement::SetZoomAndPanProperty(uint16_t aValue)
{
  nsresult rv = SetProperty(nsGkAtoms::zoomAndPan,
                            reinterpret_cast<void*>(aValue),
                            nullptr, true);
  NS_ABORT_IF_FALSE(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
                    "Setting override value when it's already set...?"); 

  return NS_SUCCEEDED(rv);
}

uint16_t
SVGSVGElement::GetZoomAndPanProperty() const
{
  void* valPtr = GetProperty(nsGkAtoms::zoomAndPan);
  if (valPtr) {
    return reinterpret_cast<uintptr_t>(valPtr);
  }
  return SVG_ZOOMANDPAN_UNKNOWN;
}

bool
SVGSVGElement::ClearZoomAndPanProperty()
{
  return UnsetProperty(nsGkAtoms::zoomAndPan);
}

} // namespace dom
} // namespace mozilla
