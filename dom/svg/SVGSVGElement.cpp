/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGSVGElementBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGViewElement.h"
#include "mozilla/EventDispatcher.h"

#include "DOMSVGLength.h"
#include "DOMSVGNumber.h"
#include "DOMSVGPoint.h"
#include "nsLayoutStylesheetCache.h"
#include "nsSVGAngle.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "nsISVGSVGFrame.h"
#include "nsSMILAnimationController.h"
#include "nsSMILTimeContainer.h"
#include "nsSVGDisplayableFrame.h"
#include "nsSVGUtils.h"
#include "SVGAngle.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT_CHECK_PARSER(SVG)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

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

JSObject*
SVGSVGElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGSVGElementBinding::Wrap(aCx, this, aGivenProto);
}

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

SVGView::SVGView()
{
  mZoomAndPan.Init(SVGSVGElement::ZOOMANDPAN,
                   SVG_ZOOMANDPAN_MAGNIFY);
  mViewBox.Init();
  mPreserveAspectRatio.Init();
}

//----------------------------------------------------------------------
// Implementation

SVGSVGElement::SVGSVGElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser)
  : SVGSVGElementBase(aNodeInfo),
    mCurrentTranslate(0.0f, 0.0f),
    mCurrentScale(1.0f),
    mPreviousTranslate(0.0f, 0.0f),
    mPreviousScale(1.0f),
    mStartAnimationOnBindToTree(aFromParser == NOT_FROM_PARSER ||
                                aFromParser == FROM_PARSER_FRAGMENT ||
                                aFromParser == FROM_PARSER_XSLT),
    mImageNeedsTransformInvalidation(false)
{
}

SVGSVGElement::~SVGSVGElement()
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT_AND_PARSER(SVGSVGElement)

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

//----------------------------------------------------------------------
// SVGZoomAndPanValues
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
// nsSVGElement
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

  nsresult rv = SVGGraphicsElement::BindToTree(aDocument, aParent,
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

  SVGGraphicsElement::UnbindFromTree(aDeep, aNullParent);
}

nsSVGAnimatedTransformList*
SVGSVGElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!(aFlags & DO_ALLOCATE) && mSVGView && mSVGView->mTransforms) {
    return mSVGView->mTransforms;
  }
  return SVGGraphicsElement::GetAnimatedTransformList(aFlags);
}

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
SVGSVGElement::IsEventAttributeNameInternal(nsIAtom* aName)
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

int32_t
SVGSVGElement::GetIntrinsicWidth()
{
  if (mLengthAttributes[ATTR_WIDTH].IsPercentage()) {
    return -1;
  }
  // Passing |this| as a SVGViewportElement* invokes the variant of GetAnimValue
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
  // Passing |this| as a SVGViewportElement* invokes the variant of GetAnimValue
  // that uses the passed argument as the context, but that's fine since we
  // know the length isn't a percentage so the context won't be used (and we
  // need to pass the element to be able to resolve em/ex units).
  float height = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(this);
  return nsSVGUtils::ClampToInt(height);
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

nsSVGElement::EnumAttributesInfo
SVGSVGElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
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

const nsSVGViewBox&
SVGSVGElement::GetViewBoxInternal() const
{
  SVGViewElement* viewElement = GetCurrentViewElement();

  if (viewElement && viewElement->mViewBox.HasRect()) {
    return viewElement->mViewBox;
  } else if (mSVGView && mSVGView->mViewBox.HasRect()) {
    return mSVGView->mViewBox;
  }

  return mViewBox;
}

nsSVGAnimatedTransformList*
SVGSVGElement::GetTransformInternal() const
{
  return (mSVGView && mSVGView->mTransforms)
         ? mSVGView->mTransforms: mTransforms;
}

} // namespace dom
} // namespace mozilla
