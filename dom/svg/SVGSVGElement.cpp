/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSVGElement.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/DOMMatrix.h"
#include "mozilla/dom/SVGSVGElementBinding.h"
#include "mozilla/dom/SVGMatrix.h"
#include "mozilla/dom/SVGRect.h"
#include "mozilla/dom/SVGViewElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/PresShell.h"
#include "mozilla/SMILAnimationController.h"
#include "mozilla/SMILTimeContainer.h"
#include "mozilla/SVGUtils.h"

#include "DOMSVGAngle.h"
#include "DOMSVGLength.h"
#include "DOMSVGNumber.h"
#include "DOMSVGPoint.h"
#include "nsFrameSelection.h"
#include "nsIFrame.h"
#include "ISVGSVGFrame.h"

NS_IMPL_NS_NEW_SVG_ELEMENT_CHECK_PARSER(SVG)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

using namespace SVGPreserveAspectRatio_Binding;
using namespace SVGSVGElement_Binding;

SVGEnumMapping SVGSVGElement::sZoomAndPanMap[] = {
    {nsGkAtoms::disable, SVG_ZOOMANDPAN_DISABLE},
    {nsGkAtoms::magnify, SVG_ZOOMANDPAN_MAGNIFY},
    {nullptr, 0}};

SVGElement::EnumInfo SVGSVGElement::sEnumInfo[1] = {
    {nsGkAtoms::zoomAndPan, sZoomAndPanMap, SVG_ZOOMANDPAN_MAGNIFY}};

JSObject* SVGSVGElement::WrapNode(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return SVGSVGElement_Binding::Wrap(aCx, this, aGivenProto);
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGSVGElement)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(SVGSVGElement)
NS_INTERFACE_MAP_END_INHERITING(SVGSVGElementBase);

NS_IMPL_ADDREF_INHERITED(SVGSVGElement, SVGSVGElementBase)
NS_IMPL_RELEASE_INHERITED(SVGSVGElement, SVGSVGElementBase)

SVGView::SVGView() {
  mZoomAndPan.Init(SVGSVGElement::ZOOMANDPAN, SVG_ZOOMANDPAN_MAGNIFY);
  mViewBox.Init();
  mPreserveAspectRatio.Init();
}

//----------------------------------------------------------------------
// Implementation

SVGSVGElement::SVGSVGElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : SVGSVGElementBase(std::move(aNodeInfo)),
      mCurrentTranslate(0.0f, 0.0f),
      mCurrentScale(1.0f),
      mStartAnimationOnBindToTree(aFromParser == NOT_FROM_PARSER ||
                                  aFromParser == FROM_PARSER_FRAGMENT ||
                                  aFromParser == FROM_PARSER_XSLT),
      mImageNeedsTransformInvalidation(false) {}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT_AND_PARSER(SVGSVGElement)

//----------------------------------------------------------------------
// nsIDOMSVGSVGElement methods:

already_AddRefed<DOMSVGAnimatedLength> SVGSVGElement::X() {
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGSVGElement::Y() {
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGSVGElement::Width() {
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<DOMSVGAnimatedLength> SVGSVGElement::Height() {
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

bool SVGSVGElement::UseCurrentView() const {
  return mSVGView || mCurrentViewID;
}

float SVGSVGElement::CurrentScale() const { return mCurrentScale; }

#define CURRENT_SCALE_MAX 16.0f
#define CURRENT_SCALE_MIN 0.0625f

void SVGSVGElement::SetCurrentScale(float aCurrentScale) {
  // Prevent bizarre behaviour and maxing out of CPU and memory by clamping
  aCurrentScale = clamped(aCurrentScale, CURRENT_SCALE_MIN, CURRENT_SCALE_MAX);

  if (aCurrentScale == mCurrentScale) {
    return;
  }
  mCurrentScale = aCurrentScale;

  if (IsRootSVGSVGElement()) {
    InvalidateTransformNotifyFrame();
  }
}

already_AddRefed<DOMSVGPoint> SVGSVGElement::CurrentTranslate() {
  return DOMSVGPoint::GetTranslateTearOff(&mCurrentTranslate, this);
}

uint32_t SVGSVGElement::SuspendRedraw(uint32_t max_wait_milliseconds) {
  // suspendRedraw is a no-op in Mozilla, so it doesn't matter what
  // we return
  return 1;
}

void SVGSVGElement::UnsuspendRedraw(uint32_t suspend_handle_id) {
  // no-op
}

void SVGSVGElement::UnsuspendRedrawAll() {
  // no-op
}

void SVGSVGElement::ForceRedraw() {
  // no-op
}

void SVGSVGElement::PauseAnimations() {
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->Pause(SMILTimeContainer::PAUSE_SCRIPT);
  }
  // else we're not the outermost <svg> or not bound to a tree, so silently fail
}

void SVGSVGElement::UnpauseAnimations() {
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->Resume(SMILTimeContainer::PAUSE_SCRIPT);
  }
  // else we're not the outermost <svg> or not bound to a tree, so silently fail
}

bool SVGSVGElement::AnimationsPaused() {
  SMILTimeContainer* root = GetTimedDocumentRoot();
  return root && root->IsPausedByType(SMILTimeContainer::PAUSE_SCRIPT);
}

float SVGSVGElement::GetCurrentTimeAsFloat() {
  SMILTimeContainer* root = GetTimedDocumentRoot();
  if (root) {
    double fCurrentTimeMs = double(root->GetCurrentTimeAsSMILTime());
    return (float)(fCurrentTimeMs / PR_MSEC_PER_SEC);
  }
  return 0.f;
}

void SVGSVGElement::SetCurrentTime(float seconds) {
  if (mTimedDocumentRoot) {
    // Make sure the timegraph is up-to-date
    FlushAnimations();
    double fMilliseconds = double(seconds) * PR_MSEC_PER_SEC;
    // Round to nearest whole number before converting, to avoid precision
    // errors
    SMILTime lMilliseconds = int64_t(NS_round(fMilliseconds));
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

void SVGSVGElement::DeselectAll() {
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    RefPtr<nsFrameSelection> frameSelection = frame->GetFrameSelection();
    frameSelection->ClearNormalSelection();
  }
}

already_AddRefed<DOMSVGNumber> SVGSVGElement::CreateSVGNumber() {
  return do_AddRef(new DOMSVGNumber(this));
}

already_AddRefed<DOMSVGLength> SVGSVGElement::CreateSVGLength() {
  return do_AddRef(new DOMSVGLength());
}

already_AddRefed<DOMSVGAngle> SVGSVGElement::CreateSVGAngle() {
  return do_AddRef(new DOMSVGAngle(this));
}

already_AddRefed<DOMSVGPoint> SVGSVGElement::CreateSVGPoint() {
  return do_AddRef(new DOMSVGPoint(Point(0, 0)));
}

already_AddRefed<SVGMatrix> SVGSVGElement::CreateSVGMatrix() {
  return do_AddRef(new SVGMatrix());
}

already_AddRefed<SVGRect> SVGSVGElement::CreateSVGRect() {
  return do_AddRef(new SVGRect(this));
}

already_AddRefed<DOMSVGTransform> SVGSVGElement::CreateSVGTransform() {
  return do_AddRef(new DOMSVGTransform());
}

already_AddRefed<DOMSVGTransform> SVGSVGElement::CreateSVGTransformFromMatrix(
    const DOMMatrix2DInit& matrix, ErrorResult& rv) {
  return do_AddRef(new DOMSVGTransform(matrix, rv));
}

void SVGSVGElement::DidChangeTranslate() {
  if (Document* doc = GetUncomposedDoc()) {
    RefPtr<PresShell> presShell = doc->GetPresShell();
    // now dispatch the appropriate event if we are the root element
    if (presShell && IsRootSVGSVGElement()) {
      nsEventStatus status = nsEventStatus_eIgnore;
      WidgetEvent svgScrollEvent(true, eSVGScroll);
      presShell->HandleDOMEventWithTarget(this, &svgScrollEvent, &status);
      InvalidateTransformNotifyFrame();
    }
  }
}

//----------------------------------------------------------------------
// SVGZoomAndPanValues
uint16_t SVGSVGElement::ZoomAndPan() const {
  return mEnumAttributes[ZOOMANDPAN].GetAnimValue();
}

void SVGSVGElement::SetZoomAndPan(uint16_t aZoomAndPan, ErrorResult& rv) {
  if (aZoomAndPan == SVG_ZOOMANDPAN_DISABLE ||
      aZoomAndPan == SVG_ZOOMANDPAN_MAGNIFY) {
    ErrorResult nestedRv;
    mEnumAttributes[ZOOMANDPAN].SetBaseValue(aZoomAndPan, this, nestedRv);
    MOZ_ASSERT(!nestedRv.Failed(),
               "We already validated our aZoomAndPan value!");
    return;
  }

  rv.ThrowRangeError<MSG_INVALID_ZOOMANDPAN_VALUE_ERROR>();
}

//----------------------------------------------------------------------
SMILTimeContainer* SVGSVGElement::GetTimedDocumentRoot() {
  if (mTimedDocumentRoot) {
    return mTimedDocumentRoot.get();
  }

  // We must not be the outermost <svg> element, try to find it
  SVGSVGElement* outerSVGElement = SVGContentUtils::GetOuterSVGElement(this);

  if (outerSVGElement) {
    return outerSVGElement->GetTimedDocumentRoot();
  }
  // invalid structure
  return nullptr;
}
//----------------------------------------------------------------------
// SVGElement
nsresult SVGSVGElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  SMILAnimationController* smilController = nullptr;

  if (Document* doc = aContext.GetComposedDoc()) {
    if ((smilController = doc->GetAnimationController())) {
      // SMIL is enabled in this document
      if (WillBeOutermostSVG(aParent)) {
        // We'll be the outermost <svg> element.  We'll need a time container.
        if (!mTimedDocumentRoot) {
          mTimedDocumentRoot = MakeUnique<SMILTimeContainer>();
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

  nsresult rv = SVGGraphicsElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mTimedDocumentRoot && smilController) {
    rv = mTimedDocumentRoot->SetParent(smilController);
    if (mStartAnimationOnBindToTree) {
      mTimedDocumentRoot->Begin();
      mStartAnimationOnBindToTree = false;
    }
  }

  return rv;
}

void SVGSVGElement::UnbindFromTree(bool aNullParent) {
  if (mTimedDocumentRoot) {
    mTimedDocumentRoot->SetParent(nullptr);
  }

  SVGGraphicsElement::UnbindFromTree(aNullParent);
}

SVGAnimatedTransformList* SVGSVGElement::GetAnimatedTransformList(
    uint32_t aFlags) {
  if (!(aFlags & DO_ALLOCATE) && mSVGView && mSVGView->mTransforms) {
    return mSVGView->mTransforms.get();
  }
  return SVGGraphicsElement::GetAnimatedTransformList(aFlags);
}

void SVGSVGElement::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  if (aVisitor.mEvent->mMessage == eSVGLoad) {
    if (mTimedDocumentRoot) {
      mTimedDocumentRoot->Begin();
      // Set 'resample needed' flag, so that if any script calls a DOM method
      // that requires up-to-date animations before our first sample callback,
      // we'll force a synchronous sample.
      AnimationNeedsResample();
    }
  }
  SVGSVGElementBase::GetEventTargetParent(aVisitor);
}

bool SVGSVGElement::IsEventAttributeNameInternal(nsAtom* aName) {
  /* The events in EventNameType_SVGSVG are for events that are only
     applicable to outermost 'svg' elements. We don't check if we're an outer
     'svg' element in case we're not inserted into the document yet, but since
     the target of the events in question will always be the outermost 'svg'
     element, this shouldn't cause any real problems.
  */
  return nsContentUtils::IsEventAttributeName(
      aName, (EventNameType_SVGGraphic | EventNameType_SVGSVG));
}

//----------------------------------------------------------------------
// public helpers:

int32_t SVGSVGElement::GetIntrinsicWidth() {
  if (mLengthAttributes[ATTR_WIDTH].IsPercentage()) {
    return -1;
  }
  // Passing |this| as a SVGViewportElement* invokes the variant of GetAnimValue
  // that uses the passed argument as the context, but that's fine since we
  // know the length isn't a percentage so the context won't be used (and we
  // need to pass the element to be able to resolve em/ex units).
  float width = mLengthAttributes[ATTR_WIDTH].GetAnimValue(this);
  return SVGUtils::ClampToInt(width);
}

int32_t SVGSVGElement::GetIntrinsicHeight() {
  if (mLengthAttributes[ATTR_HEIGHT].IsPercentage()) {
    return -1;
  }
  // Passing |this| as a SVGViewportElement* invokes the variant of GetAnimValue
  // that uses the passed argument as the context, but that's fine since we
  // know the length isn't a percentage so the context won't be used (and we
  // need to pass the element to be able to resolve em/ex units).
  float height = mLengthAttributes[ATTR_HEIGHT].GetAnimValue(this);
  return SVGUtils::ClampToInt(height);
}

void SVGSVGElement::FlushImageTransformInvalidation() {
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

bool SVGSVGElement::WillBeOutermostSVG(nsINode& aParent) const {
  nsINode* parent = &aParent;
  while (parent && parent->IsSVGElement()) {
    if (parent->IsSVGElement(nsGkAtoms::foreignObject)) {
      // SVG in a foreignObject must have its own <svg> (SVGOuterSVGFrame).
      return false;
    }
    if (parent->IsSVGElement(nsGkAtoms::svg)) {
      return false;
    }
    parent = parent->GetParentOrShadowHostNode();
  }

  return true;
}

void SVGSVGElement::InvalidateTransformNotifyFrame() {
  ISVGSVGFrame* svgframe = do_QueryFrame(GetPrimaryFrame());
  // might fail this check if we've failed conditional processing
  if (svgframe) {
    svgframe->NotifyViewportOrTransformChanged(
        ISVGDisplayableFrame::TRANSFORM_CHANGED);
  }
}

SVGElement::EnumAttributesInfo SVGSVGElement::GetEnumInfo() {
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo, ArrayLength(sEnumInfo));
}

void SVGSVGElement::SetImageOverridePreserveAspectRatio(
    const SVGPreserveAspectRatio& aPAR) {
  MOZ_ASSERT(OwnerDoc()->IsBeingUsedAsImage(),
             "should only override preserveAspectRatio in images");

  bool hasViewBox = HasViewBox();
  if (!hasViewBox && ShouldSynthesizeViewBox()) {
    // My non-<svg:image> clients will have been painting me with a synthesized
    // viewBox, but my <svg:image> client that's about to paint me now does NOT
    // want that.  Need to tell ourselves to flush our transform.
    mImageNeedsTransformInvalidation = true;
  }

  if (!hasViewBox) {
    return;  // preserveAspectRatio irrelevant (only matters if we have viewBox)
  }

  if (SetPreserveAspectRatioProperty(aPAR)) {
    mImageNeedsTransformInvalidation = true;
  }
}

void SVGSVGElement::ClearImageOverridePreserveAspectRatio() {
  MOZ_ASSERT(OwnerDoc()->IsBeingUsedAsImage(),
             "should only override image preserveAspectRatio in images");

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

bool SVGSVGElement::SetPreserveAspectRatioProperty(
    const SVGPreserveAspectRatio& aPAR) {
  SVGPreserveAspectRatio* pAROverridePtr = new SVGPreserveAspectRatio(aPAR);
  nsresult rv =
      SetProperty(nsGkAtoms::overridePreserveAspectRatio, pAROverridePtr,
                  nsINode::DeleteProperty<SVGPreserveAspectRatio>, true);
  MOZ_ASSERT(rv != NS_PROPTABLE_PROP_OVERWRITTEN,
             "Setting override value when it's already set...?");

  if (MOZ_UNLIKELY(NS_FAILED(rv))) {
    // property-insertion failed (e.g. OOM in property-table code)
    delete pAROverridePtr;
    return false;
  }
  return true;
}

const SVGPreserveAspectRatio* SVGSVGElement::GetPreserveAspectRatioProperty()
    const {
  void* valPtr = GetProperty(nsGkAtoms::overridePreserveAspectRatio);
  if (valPtr) {
    return static_cast<SVGPreserveAspectRatio*>(valPtr);
  }
  return nullptr;
}

bool SVGSVGElement::ClearPreserveAspectRatioProperty() {
  void* valPtr = TakeProperty(nsGkAtoms::overridePreserveAspectRatio);
  bool didHaveProperty = !!valPtr;
  delete static_cast<SVGPreserveAspectRatio*>(valPtr);
  return didHaveProperty;
}

SVGPreserveAspectRatio SVGSVGElement::GetPreserveAspectRatioWithOverride()
    const {
  Document* doc = GetUncomposedDoc();
  if (doc && doc->IsBeingUsedAsImage()) {
    const SVGPreserveAspectRatio* pAROverridePtr =
        GetPreserveAspectRatioProperty();
    if (pAROverridePtr) {
      return *pAROverridePtr;
    }
  }

  SVGViewElement* viewElement = GetCurrentViewElement();

  // This check is equivalent to "!HasViewBox() &&
  // ShouldSynthesizeViewBox()". We're just holding onto the viewElement that
  // HasViewBox() would look up, so that we don't have to look it up again
  // later.
  if (!((viewElement && viewElement->mViewBox.HasRect()) ||
        (mSVGView && mSVGView->mViewBox.HasRect()) || mViewBox.HasRect()) &&
      ShouldSynthesizeViewBox()) {
    // If we're synthesizing a viewBox, use preserveAspectRatio="none";
    return SVGPreserveAspectRatio(SVG_PRESERVEASPECTRATIO_NONE,
                                  SVG_MEETORSLICE_SLICE);
  }

  if (viewElement && viewElement->mPreserveAspectRatio.IsExplicitlySet()) {
    return viewElement->mPreserveAspectRatio.GetAnimValue();
  }
  if (mSVGView && mSVGView->mPreserveAspectRatio.IsExplicitlySet()) {
    return mSVGView->mPreserveAspectRatio.GetAnimValue();
  }
  return mPreserveAspectRatio.GetAnimValue();
}

SVGViewElement* SVGSVGElement::GetCurrentViewElement() const {
  if (mCurrentViewID) {
    // XXXsmaug It is unclear how this should work in case we're in Shadow DOM.
    Document* doc = GetUncomposedDoc();
    if (doc) {
      Element* element = doc->GetElementById(*mCurrentViewID);
      if (element && element->IsSVGElement(nsGkAtoms::view)) {
        return static_cast<SVGViewElement*>(element);
      }
    }
  }
  return nullptr;
}

const SVGAnimatedViewBox& SVGSVGElement::GetViewBoxInternal() const {
  SVGViewElement* viewElement = GetCurrentViewElement();

  if (viewElement && viewElement->mViewBox.HasRect()) {
    return viewElement->mViewBox;
  }
  if (mSVGView && mSVGView->mViewBox.HasRect()) {
    return mSVGView->mViewBox;
  }

  return mViewBox;
}

SVGAnimatedTransformList* SVGSVGElement::GetTransformInternal() const {
  return (mSVGView && mSVGView->mTransforms) ? mSVGView->mTransforms.get()
                                             : mTransforms.get();
}

}  // namespace dom
}  // namespace mozilla
