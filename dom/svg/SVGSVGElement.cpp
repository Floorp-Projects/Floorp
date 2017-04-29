/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "mozilla/ArrayUtils.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Likely.h"

#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsLayoutStylesheetCache.h"
#include "DOMSVGNumber.h"
#include "DOMSVGLength.h"
#include "nsSVGAngle.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "mozilla/dom/SVGMatrix.h"
#include "DOMSVGPoint.h"
#include "nsIFrame.h"
#include "nsFrameSelection.h"
#include "nsISVGSVGFrame.h" //XXX
#include "mozilla/dom/SVGRect.h"
#include "nsError.h"
#include "nsSVGDisplayableFrame.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGSVGElementBinding.h"
#include "nsSVGUtils.h"
#include "mozilla/dom/SVGViewElement.h"
#include "nsStyleUtil.h"
#include "SVGContentUtils.h"

#include "nsSMILTimeContainer.h"
#include "nsSMILAnimationController.h"
#include "nsSMILTypes.h"
#include "SVGAngle.h"
#include <algorithm>
#include "prtime.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(SVG)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

class SVGAnimatedLength;

JSObject*
SVGSVGElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGSVGElementBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(DOMSVGTranslatePoint, nsISVGPoint,
                                   mElement)

NS_IMPL_ADDREF_INHERITED(DOMSVGTranslatePoint, nsISVGPoint)
NS_IMPL_RELEASE_INHERITED(DOMSVGTranslatePoint, nsISVGPoint)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTranslatePoint)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  // We have to qualify nsISVGPoint because NS_GET_IID looks for a class in the
  // global namespace
  NS_INTERFACE_MAP_ENTRY(mozilla::nsISVGPoint)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SVGSVGElement::~SVGSVGElement()
{
}

DOMSVGPoint*
DOMSVGTranslatePoint::Copy()
{
  return new DOMSVGPoint(mPt.GetX(), mPt.GetY());
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

SVGView::SVGView()
{
  mZoomAndPan.Init(SVGSVGElement::ZOOMANDPAN,
                   SVG_ZOOMANDPAN_MAGNIFY);
  mViewBox.Init();
  mPreserveAspectRatio.Init();
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

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGSVGElement)

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
  NS_INTERFACE_TABLE_INHERITED(SVGSVGElement, nsIDOMNode, nsIDOMElement,
                               nsIDOMSVGElement)
NS_INTERFACE_TABLE_TAIL_INHERITING(SVGSVGElementBase)

//----------------------------------------------------------------------
// Implementation

SVGSVGElement::SVGSVGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
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
    mHasChildrenOnlyTransform(false)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

// From NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSVGElement)
nsresult
SVGSVGElement::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                     bool aPreallocateChildren) const
{
  *aResult = nullptr;
  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  SVGSVGElement *it = new SVGSVGElement(ni, NOT_FROM_PARSER);

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv1 = it->Init();
  nsresult rv2 = const_cast<SVGSVGElement*>(this)->CopyInnerTo(it, aPreallocateChildren);
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
  return mSVGView || mCurrentViewID;
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

void
SVGSVGElement::UnsuspendRedraw(uint32_t suspend_handle_id)
{
  // no-op
}

void
SVGSVGElement::UnsuspendRedrawAll()
{
  // no-op
}

void
SVGSVGElement::ForceRedraw()
{
  // no-op
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

void
SVGSVGElement::DeselectAll()
{
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    RefPtr<nsFrameSelection> frameSelection = frame->GetFrameSelection();
    frameSelection->ClearNormalSelection();
  }
}

already_AddRefed<DOMSVGNumber>
SVGSVGElement::CreateSVGNumber()
{
  RefPtr<DOMSVGNumber> number = new DOMSVGNumber(ToSupports(this));
  return number.forget();
}

already_AddRefed<DOMSVGLength>
SVGSVGElement::CreateSVGLength()
{
  nsCOMPtr<DOMSVGLength> length = new DOMSVGLength();
  return length.forget();
}

already_AddRefed<SVGAngle>
SVGSVGElement::CreateSVGAngle()
{
  nsSVGAngle* angle = new nsSVGAngle();
  angle->Init();
  RefPtr<SVGAngle> svgangle = new SVGAngle(angle, this, SVGAngle::CreatedValue);
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
  RefPtr<SVGMatrix> matrix = new SVGMatrix();
  return matrix.forget();
}

already_AddRefed<SVGIRect>
SVGSVGElement::CreateSVGRect()
{
  return NS_NewSVGRect(this);
}

already_AddRefed<SVGTransform>
SVGSVGElement::CreateSVGTransform()
{
  RefPtr<SVGTransform> transform = new SVGTransform();
  return transform.forget();
}

already_AddRefed<SVGTransform>
SVGSVGElement::CreateSVGTransformFromMatrix(SVGMatrix& matrix)
{
  RefPtr<SVGTransform> transform = new SVGTransform(matrix.GetMatrix());
  return transform.forget();
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedRect>
SVGSVGElement::ViewBox()
{
  return mViewBox.ToSVGAnimatedRect(this);
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGSVGElement::PreserveAspectRatio()
{
  return mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(this);
}

uint16_t
SVGSVGElement::ZoomAndPan()
{
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

  rv.ThrowRangeError<MSG_INVALID_ZOOMANDPAN_VALUE_ERROR>();
}

//----------------------------------------------------------------------
// helper method for implementing SetCurrentScale/Translate

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
  //
  // XXX This comment is out-of-date due to removal of SVGZoomEvent.  Can we
  // remove some of this code?
  mPreviousScale = mCurrentScale;
  mPreviousTranslate = mCurrentTranslate;
  
  mCurrentScale = s;
  mCurrentTranslate = SVGPoint(x, y);

  // now dispatch the appropriate event if we are the root element
  nsIDocument* doc = GetUncomposedDoc();
  if (doc) {
    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    if (presShell && IsRoot()) {
      nsEventStatus status = nsEventStatus_eIgnore;
      if (mPreviousScale == mCurrentScale) {
        WidgetEvent svgScrollEvent(true, eSVGScroll);
        presShell->HandleDOMEventWithTarget(this, &svgScrollEvent, &status);
      }
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
SVGSVGElement::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  if (aVisitor.mEvent->mMessage == eSVGLoad) {
    if (mTimedDocumentRoot) {
      mTimedDocumentRoot->Begin();
      // Set 'resample needed' flag, so that if any script calls a DOM method
      // that requires up-to-date animations before our first sample callback,
      // we'll force a synchronous sample.
      AnimationNeedsResample();
    }
  }
  return SVGSVGElementBase::GetEventTargetParent(aVisitor);
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

gfx::Matrix
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
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  nsSVGViewBoxRect viewBox =
    GetViewBoxWithSynthesis(viewportWidth, viewportHeight);

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    return gfx::Matrix(0.0, 0.0, 0.0, 0.0, 0.0, 0.0); // singular
  }

  return SVGContentUtils::GetViewBoxTransform(viewportWidth, viewportHeight,
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
  MOZ_ASSERT(!(GetPrimaryFrame()->GetStateBits() & NS_FRAME_IS_NONDISPLAY),
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
    changeHint = nsChangeHint(nsChangeHint_UpdateOverflow |
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
  nsSMILAnimationController* smilController = nullptr;

  if (aDocument) {
    smilController = aDocument->GetAnimationController();
    if (smilController) {
      // SMIL is enabled in this document
      if (WillBeOutermostSVG(aParent, aBindingParent)) {
        // We'll be the outermost <svg> element.  We'll need a time container.
        if (!mTimedDocumentRoot) {
          mTimedDocumentRoot = new nsSMILTimeContainer();
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

  nsIDocument* doc = GetComposedDoc();
  if (doc) {
    // Setup the style sheet during binding, not element construction,
    // because we could move the root SVG element from the document
    // that created it to another document.
    auto cache = nsLayoutStylesheetCache::For(doc->GetStyleBackendType());
    doc->EnsureOnDemandBuiltInUASheet(cache->SVGSheet());
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

  while (parent && parent->IsSVGElement()) {
    if (parent->IsSVGElement(nsGkAtoms::foreignObject)) {
      // SVG in a foreignObject must have its own <svg> (nsSVGOuterSVGFrame).
      return false;
    }
    if (parent->IsSVGElement(nsGkAtoms::svg)) {
      return false;
    }
    parent = parent->GetParent();
  }

  return true;
}

void
SVGSVGElement::InvalidateTransformNotifyFrame()
{
  nsISVGSVGFrame* svgframe = do_QueryFrame(GetPrimaryFrame());
  // might fail this check if we've failed conditional processing
  if (svgframe) {
    svgframe->NotifyViewportOrTransformChanged(
                nsSVGDisplayableFrame::TRANSFORM_CHANGED);
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
    //XXXsmaug It is unclear how this should work in case we're in Shadow DOM.
    nsIDocument* doc = GetUncomposedDoc();
    if (doc) {
      Element *element = doc->GetElementById(*mCurrentViewID);
      if (element && element->IsSVGElement(nsGkAtoms::view)) {
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
  if (mSVGView && mSVGView->mViewBox.HasRect()) {
    return mSVGView->mViewBox.GetAnimValue();
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
  nsIDocument* doc = GetUncomposedDoc();
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
        (mSVGView && mSVGView->mViewBox.HasRect()) ||
        mViewBox.HasRect()) &&
      ShouldSynthesizeViewBox()) {
    // If we're synthesizing a viewBox, use preserveAspectRatio="none";
    return SVGPreserveAspectRatio(SVG_PRESERVEASPECTRATIO_NONE, SVG_MEETORSLICE_SLICE);
  }

  if (viewElement && viewElement->mPreserveAspectRatio.IsExplicitlySet()) {
    return viewElement->mPreserveAspectRatio.GetAnimValue();
  }
  if (mSVGView && mSVGView->mPreserveAspectRatio.IsExplicitlySet()) {
    return mSVGView->mPreserveAspectRatio.GetAnimValue();
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
  } else if (mSVGView && mSVGView->mViewBox.HasRect()) {
    viewbox = &mSVGView->mViewBox.GetAnimValue();
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
SVGSVGElement::PrependLocalTransformsTo(const gfxMatrix& aMatrix,
                                        SVGTransformTypes aWhich) const
{
  // 'transform' attribute (or an override from a fragment identifier):
  gfxMatrix userToParent;

  if (aWhich == eUserSpaceToParent || aWhich == eAllTransforms) {
    userToParent = GetUserToParentTransform(mAnimateMotionTransform,
                                            mSVGView && mSVGView->mTransforms
                                              ? mSVGView->mTransforms
                                              : mTransforms);
    if (aWhich == eUserSpaceToParent) {
      return userToParent * aMatrix;
    }
  }

  gfxMatrix childToUser;

  if (IsInner()) {
    float x, y;
    const_cast<SVGSVGElement*>(this)->GetAnimatedLengthValues(&x, &y, nullptr);
    childToUser = ThebesMatrix(GetViewBoxTransform().PostTranslate(x, y));
  } else if (IsRoot()) {
    childToUser = ThebesMatrix(GetViewBoxTransform()
                                 .PostScale(mCurrentScale, mCurrentScale)
                                 .PostTranslate(mCurrentTranslate.GetX(),
                                                mCurrentTranslate.GetY()));
  } else {
    // outer-<svg>, but inline in some other content:
    childToUser = ThebesMatrix(GetViewBoxTransform());
  }

  if (aWhich == eAllTransforms) {
    return childToUser * userToParent * aMatrix;
  }

  MOZ_ASSERT(aWhich == eChildToUserSpace, "Unknown TransformTypes");

  // The following may look broken because pre-multiplying our eChildToUserSpace
  // transform with another matrix without including our eUserSpaceToParent
  // transform between the two wouldn't make sense.  We don't expect that to
  // ever happen though.  We get here either when the identity matrix has been
  // passed because our caller just wants our eChildToUserSpace transform, or
  // when our eUserSpaceToParent transform has already been multiplied into the
  // matrix that our caller passes (such as when we're called from PaintSVG).
  return childToUser * aMatrix;
}

nsSVGAnimatedTransformList*
SVGSVGElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!(aFlags & DO_ALLOCATE) && mSVGView && mSVGView->mTransforms) {
    return mSVGView->mTransforms;
  }
  return SVGSVGElementBase::GetAnimatedTransformList(aFlags);
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

nsSVGViewBox*
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
  if ((viewElement && viewElement->mViewBox.HasRect()) ||
      (mSVGView && mSVGView->mViewBox.HasRect())) {
    return true;
  }
  return mViewBox.HasRect();
}

bool
SVGSVGElement::ShouldSynthesizeViewBox() const
{
  MOZ_ASSERT(!HasViewBoxRect(), "Should only be called if we lack a viewBox");

  return IsRoot() && OwnerDoc()->IsBeingUsedAsImage();
}

bool
SVGSVGElement::SetPreserveAspectRatioProperty(const SVGPreserveAspectRatio& aPAR)
{
  SVGPreserveAspectRatio* pAROverridePtr = new SVGPreserveAspectRatio(aPAR);
  nsresult rv = SetProperty(nsGkAtoms::overridePreserveAspectRatio,
                            pAROverridePtr,
                            nsINode::DeleteProperty<SVGPreserveAspectRatio>,
                            true);
  MOZ_ASSERT(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
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
  MOZ_ASSERT(OwnerDoc()->IsBeingUsedAsImage(),
             "should only override preserveAspectRatio in images");
#endif

  bool hasViewBoxRect = HasViewBoxRect();
  if (!hasViewBoxRect && ShouldSynthesizeViewBox()) {
    // My non-<svg:image> clients will have been painting me with a synthesized
    // viewBox, but my <svg:image> client that's about to paint me now does NOT
    // want that.  Need to tell ourselves to flush our transform.
    mImageNeedsTransformInvalidation = true;
  }

  if (!hasViewBoxRect) {
    return; // preserveAspectRatio irrelevant (only matters if we have viewBox)
  }

  if (SetPreserveAspectRatioProperty(aPAR)) {
    mImageNeedsTransformInvalidation = true;
  }
}

void
SVGSVGElement::ClearImageOverridePreserveAspectRatio()
{
#ifdef DEBUG
  MOZ_ASSERT(OwnerDoc()->IsBeingUsedAsImage(),
             "should only override image preserveAspectRatio in images");
#endif

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
  MOZ_ASSERT(!GetParent(), "Should only be called on root node");
  MOZ_ASSERT(OwnerDoc()->IsBeingUsedAsImage(),
             "Should only be called on image documents");

  if (mImageNeedsTransformInvalidation) {
    InvalidateTransformNotifyFrame();
    mImageNeedsTransformInvalidation = false;
  }
}

int32_t
SVGSVGElement::GetIntrinsicWidth()
{
  if (mLengthAttributes[ATTR_WIDTH].IsPercentage()) {
    return -1;
  }
  // Passing |this| as a SVGSVGElement* invokes the variant of GetAnimValue
  // that uses the passed argument as the context, but that's fine since we
  // know the length isn't a percentage so the context won't be used (and we
  // need to pass the element to be able to resolve em/ex units).
  float width = mLengthAttributes[ATTR_WIDTH].GetAnimValue(this);
  return nsSVGUtils::ClampToInt(width);
}

int32_t
SVGSVGElement::GetIntrinsicHeight()
{
  if (mLengthAttributes[ATTR_HEIGHT].IsPercentage()) {
    return -1;
  }
  // Passing |this| as a SVGSVGElement* invokes the variant of GetAnimValue
  // that uses the passed argument as the context, but that's fine since we
  // know the length isn't a percentage so the context won't be used (and we
  // need to pass the element to be able to resolve em/ex units).
  float height = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(this);
  return nsSVGUtils::ClampToInt(height);
}

} // namespace dom
} // namespace mozilla
