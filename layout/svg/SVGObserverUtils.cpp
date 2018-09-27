/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGObserverUtils.h"

// Keep others in (case-insensitive) order:
#include "mozilla/RestyleManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsISupportsImpl.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsIReflowCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "SVGGeometryElement.h"
#include "SVGTextPathElement.h"
#include "SVGUseElement.h"
#include "ImageLoader.h"
#include "mozilla/net/ReferrerPolicy.h"

using namespace mozilla::dom;

namespace mozilla {

void
SVGRenderingObserver::StartObserving()
{
  Element* target = GetTarget();
  if (target) {
    target->AddMutationObserver(this);
  }
}

void
SVGRenderingObserver::StopObserving()
{
  Element* target = GetTarget();

  if (target) {
    target->RemoveMutationObserver(this);
    if (mInObserverList) {
      SVGObserverUtils::RemoveRenderingObserver(target, this);
      mInObserverList = false;
    }
  }
  NS_ASSERTION(!mInObserverList, "still in an observer list?");
}

static SVGRenderingObserverList*
GetObserverList(Element *aElement)
{
  return static_cast<SVGRenderingObserverList*>
    (aElement->GetProperty(nsGkAtoms::renderingobserverlist));
}

Element*
SVGRenderingObserver::GetReferencedElement()
{
  Element* target = GetTarget();
#ifdef DEBUG
  if (target) {
    SVGRenderingObserverList* observerList = GetObserverList(target);
    bool inObserverList = observerList && observerList->Contains(this);
    NS_ASSERTION(inObserverList == mInObserverList, "failed to track whether we're in our referenced element's observer list!");
  } else {
    NS_ASSERTION(!mInObserverList, "In whose observer list are we, then?");
  }
#endif
  if (target && !mInObserverList) {
    SVGObserverUtils::AddRenderingObserver(target, this);
    mInObserverList = true;
  }
  return target;
}

nsIFrame*
SVGRenderingObserver::GetReferencedFrame()
{
  Element* referencedElement = GetReferencedElement();
  return referencedElement ? referencedElement->GetPrimaryFrame() : nullptr;
}

nsIFrame*
SVGRenderingObserver::GetReferencedFrame(LayoutFrameType aFrameType,
                                         bool* aOK)
{
  nsIFrame* frame = GetReferencedFrame();
  if (frame) {
    if (frame->Type() == aFrameType)
      return frame;
    if (aOK) {
      *aOK = false;
    }
  }
  return nullptr;
}

void
SVGRenderingObserver::OnNonDOMMutationRenderingChange()
{
  mInObserverList = false;
  OnRenderingChange();
}

void
SVGRenderingObserver::NotifyEvictedFromRenderingObserverList()
{
  mInObserverList = false; // We've been removed from rendering-obs. list.
  StopObserving();            // Remove ourselves from mutation-obs. list.
}

void
SVGRenderingObserver::AttributeChanged(dom::Element* aElement,
                                       int32_t aNameSpaceID,
                                       nsAtom* aAttribute,
                                       int32_t aModType,
                                       const nsAttrValue* aOldValue)
{
  // An attribute belonging to the element that we are observing *or one of its
  // descendants* has changed.
  //
  // In the case of observing a gradient element, say, we want to know if any
  // of its 'stop' element children change, but we don't actually want to do
  // anything for changes to SMIL element children, for example. Maybe it's not
  // worth having logic to optimize for that, but in most cases it could be a
  // small check?
  //
  // XXXjwatt: do we really want to blindly break the link between our
  // observers and ourselves for all attribute changes? For non-ID changes
  // surely that is unnecessary.

  OnRenderingChange();
}

void
SVGRenderingObserver::ContentAppended(nsIContent* aFirstNewContent)
{
  OnRenderingChange();
}

void
SVGRenderingObserver::ContentInserted(nsIContent* aChild)
{
  OnRenderingChange();
}

void
SVGRenderingObserver::ContentRemoved(nsIContent* aChild,
                                     nsIContent* aPreviousSibling)
{
  OnRenderingChange();
}

/**
 * Note that in the current setup there are two separate observer lists.
 *
 * In SVGIDRenderingObserver's ctor, the new object adds itself to the
 * mutation observer list maintained by the referenced element. In this way the
 * SVGIDRenderingObserver is notified if there are any attribute or content
 * tree changes to the element or any of its *descendants*.
 *
 * In SVGIDRenderingObserver::GetReferencedElement() the
 * SVGIDRenderingObserver object also adds itself to an
 * SVGRenderingObserverList object belonging to the referenced
 * element.
 *
 * XXX: it would be nice to have a clear and concise executive summary of the
 * benefits/necessity of maintaining a second observer list.
 */

SVGIDRenderingObserver::SVGIDRenderingObserver(URLAndReferrerInfo* aURI,
                                               nsIContent* aObservingContent,
                                               bool aReferenceImage)
  : mObservedElementTracker(this)
{
  // Start watching the target element
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> referrer;
  uint32_t referrerPolicy = mozilla::net::RP_Unset;
  if (aURI) {
    uri = aURI->GetURI();
    referrer = aURI->GetReferrer();
    referrerPolicy = aURI->GetReferrerPolicy();
  }

  mObservedElementTracker.Reset(aObservingContent, uri, referrer,
                                referrerPolicy, true, aReferenceImage);
  StartObserving();
}

SVGIDRenderingObserver::~SVGIDRenderingObserver()
{
  StopObserving();
}

void
SVGIDRenderingObserver::OnRenderingChange()
{
  if (mObservedElementTracker.get() && mInObserverList) {
    SVGObserverUtils::RemoveRenderingObserver(mObservedElementTracker.get(), this);
    mInObserverList = false;
  }
}

void
nsSVGFrameReferenceFromProperty::Detach()
{
  mFrame = nullptr;
  mFramePresShell = nullptr;
}

nsIFrame*
nsSVGFrameReferenceFromProperty::Get()
{
  if (mFramePresShell && mFramePresShell->IsDestroying()) {
    // mFrame is no longer valid.
    Detach();
  }
  return mFrame;
}


NS_IMPL_ISUPPORTS(SVGTemplateElementObserver, nsIMutationObserver)

void
SVGTemplateElementObserver::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  if (nsIFrame* frame = mFrameReference.Get()) {
    // We know that we don't need to walk the parent chain notifying rendering
    // observers since changes to a gradient etc. do not affect ancestor
    // elements.  So we only invalidate *direct* rendering observers here.
    // Since we don't need to walk the parent chain, we don't need to worry
    // about coalescing multiple invalidations by using a change hint as we do
    // in nsSVGRenderingObserverProperty::OnRenderingChange.
    SVGObserverUtils::InvalidateDirectRenderingObservers(frame);
  }
}


NS_IMPL_ISUPPORTS(nsSVGRenderingObserverProperty, nsIMutationObserver)

void
nsSVGRenderingObserverProperty::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();

  if (frame && frame->HasAllStateBits(NS_FRAME_SVG_LAYOUT)) {
    // We need to notify anything that is observing the referencing frame or
    // any of its ancestors that the referencing frame has been invalidated.
    // Since walking the parent chain checking for observers is expensive we
    // do that using a change hint (multiple change hints of the same type are
    // coalesced).
    nsLayoutUtils::PostRestyleEvent(
      frame->GetContent()->AsElement(), nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
  }
}


NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGFilterObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGFilterObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGFilterObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGFilterObserver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGFilterObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObservedElementTracker)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGFilterObserver)
  tmp->StopObserving();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObservedElementTracker);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsSVGFilterFrame *
SVGFilterObserver::GetFilterFrame()
{
  return static_cast<nsSVGFilterFrame*>(
    GetReferencedFrame(LayoutFrameType::SVGFilter, nullptr));
}

void
SVGFilterObserver::OnRenderingChange()
{
  SVGIDRenderingObserver::OnRenderingChange();

  if (mFilterObserverList) {
    mFilterObserverList->Invalidate();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGFilterObserverList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGFilterObserverList)

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGFilterObserverList)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGFilterObserverList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObservers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGFilterObserverList)
  tmp->DetachObservers();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObservers);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGFilterObserverList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SVGFilterObserverList::SVGFilterObserverList(const nsTArray<nsStyleFilter>& aFilters,
                                             nsIContent* aFilteredElement,
                                             nsIFrame* aFilteredFrame)
{
  for (uint32_t i = 0; i < aFilters.Length(); i++) {
    if (aFilters[i].GetType() != NS_STYLE_FILTER_URL)
      continue;

    // aFilteredFrame can be null if this filter belongs to a
    // CanvasRenderingContext2D.
    RefPtr<URLAndReferrerInfo> filterURL;
    if (aFilteredFrame) {
      filterURL = SVGObserverUtils::GetFilterURI(aFilteredFrame, i);
    } else {
      nsCOMPtr<nsIURI> resolvedURI =
        aFilters[i].GetURL()->ResolveLocalRef(aFilteredElement);
      if (resolvedURI) {
        filterURL = new URLAndReferrerInfo(
          resolvedURI,
          aFilters[i].GetURL()->mExtraData->GetReferrer(),
          aFilters[i].GetURL()->mExtraData->GetReferrerPolicy());
      }
    }

    RefPtr<SVGFilterObserver> observer =
      new SVGFilterObserver(filterURL, aFilteredElement, this);
    mObservers.AppendElement(observer);
  }
}

SVGFilterObserverList::~SVGFilterObserverList()
{
  DetachObservers();
}

bool
SVGFilterObserverList::ReferencesValidResources()
{
  for (uint32_t i = 0; i < mObservers.Length(); i++) {
    if (!mObservers[i]->ReferencesValidResource()) {
      return false;
    }
  }
  return true;
}

void
SVGFilterObserverListForCSSProp::OnRenderingChange()
{
  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  // Repaint asynchronously in case the filter frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame);

  // Since we don't call nsSVGRenderingObserverProperty::
  // OnRenderingChange, we have to add this bit ourselves.
  if (frame->HasAllStateBits(NS_FRAME_SVG_LAYOUT)) {
    // Changes should propagate out to things that might be observing
    // the referencing frame or its ancestors.
    changeHint |= nsChangeHint_InvalidateRenderingObservers;
  }

  // Don't need to request UpdateOverflow if we're being reflowed.
  if (!(frame->GetStateBits() & NS_FRAME_IN_REFLOW)) {
    changeHint |= nsChangeHint_UpdateOverflow;
  }
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}

void
SVGMarkerObserver::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  NS_ASSERTION(frame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");

  // Don't need to request ReflowFrame if we're being reflowed.
  if (!(frame->GetStateBits() & NS_FRAME_IN_REFLOW)) {
    // XXXjwatt: We need to unify SVG into standard reflow so we can just use
    // nsChangeHint_NeedReflow | nsChangeHint_NeedDirtyReflow here.
    // XXXSDL KILL THIS!!!
    nsSVGUtils::ScheduleReflowSVG(frame);
  }
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0),
    nsChangeHint_RepaintFrame);
}

NS_IMPL_ISUPPORTS(SVGMaskObserverList, nsISupports)

SVGMaskObserverList::SVGMaskObserverList(nsIFrame* aFrame)
 : mFrame(aFrame)
{
  const nsStyleSVGReset *svgReset = aFrame->StyleSVGReset();

  for (uint32_t i = 0; i < svgReset->mMask.mImageCount; i++) {
    RefPtr<URLAndReferrerInfo> maskUri =
      SVGObserverUtils::GetMaskURI(aFrame, i);
    bool hasRef = false;
    if (maskUri) {
      maskUri->GetURI()->GetHasRef(&hasRef);
    }

    // Accrording to maskUri, nsSVGPaintingProperty's ctor may trigger an
    // external SVG resource download, so we should pass maskUri in only if
    // maskUri has a chance pointing to an SVG mask resource.
    //
    // And, an URL may refer to an SVG mask resource if it consists of
    // a fragment.
    nsSVGPaintingProperty* prop =
      new nsSVGPaintingProperty(hasRef ? maskUri.get() : nullptr,
                                aFrame, false);
    mProperties.AppendElement(prop);
  }
}

void
SVGMaskObserverList::ResolveImage(uint32_t aIndex)
{
  const nsStyleSVGReset* svgReset = mFrame->StyleSVGReset();
  MOZ_ASSERT(aIndex < svgReset->mMask.mImageCount);

  nsStyleImage& image =
    const_cast<nsStyleImage&>(svgReset->mMask.mLayers[aIndex].mImage);

  if (!image.IsResolved()) {
    MOZ_ASSERT(image.GetType() == nsStyleImageType::eStyleImageType_Image);
    image.ResolveImage(mFrame->PresContext(), nullptr);

    mozilla::css::ImageLoader* imageLoader =
      mFrame->PresContext()->Document()->StyleImageLoader();
    if (imgRequestProxy* req = image.GetImageData()) {
      imageLoader->AssociateRequestToFrame(req, mFrame, 0);
    }
  }
}

bool
SVGTextPathObserver::TargetIsValid()
{
  Element* target = GetTarget();
  return target && target->IsSVGElement(nsGkAtoms::path);
}

void
SVGTextPathObserver::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  NS_ASSERTION(frame->IsFrameOfType(nsIFrame::eSVG) ||
               nsSVGUtils::IsInSVGTextSubtree(frame),
               "SVG frame expected");

  // Avoid getting into an infinite loop of reflows if the <textPath> is
  // pointing to one of its ancestors.  TargetIsValid returns true iff
  // the target element is a <path> element, and we would not have this
  // SVGTextPathObserver if this <textPath> were a descendant of the
  // target <path>.
  //
  // Note that we still have to post the restyle event when we
  // change from being valid to invalid, so that mPositions on the
  // SVGTextFrame gets updated, skipping the <textPath>, ensuring
  // that nothing gets painted for that element.
  bool nowValid = TargetIsValid();
  if (!mValid && !nowValid) {
    // Just return if we were previously invalid, and are still invalid.
    return;
  }
  mValid = nowValid;

  // Repaint asynchronously in case the path frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_UpdateTextPath);
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}

static void
InvalidateAllContinuations(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
    f->InvalidateFrame();
  }
}

void
nsSVGPaintingProperty::OnRenderingChange()
{
  nsSVGRenderingObserverProperty::OnRenderingChange();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  if (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    frame->InvalidateFrameSubtree();
  } else {
    InvalidateAllContinuations(frame);
  }
}

// Note that the returned list will be empty in the case of a 'filter' property
// that only specifies CSS filter functions (no url()'s to SVG filters).
static SVGFilterObserverListForCSSProp*
GetOrCreateFilterObserverListForCSS(nsIFrame* aFrame)
{
  const nsStyleEffects* effects = aFrame->StyleEffects();
  if (!effects->HasFilters())
    return nullptr;

  SVGFilterObserverListForCSSProp* observers =
    aFrame->GetProperty(SVGObserverUtils::FilterProperty());
  if (observers) {
    return observers;
  }
  observers = new SVGFilterObserverListForCSSProp(effects->mFilters, aFrame);
  NS_ADDREF(observers);
  aFrame->SetProperty(SVGObserverUtils::FilterProperty(), observers);
  return observers;
}

static SVGMaskObserverList*
GetOrCreateMaskProperty(nsIFrame* aFrame)
{
  SVGMaskObserverList *prop =
    aFrame->GetProperty(SVGObserverUtils::MaskProperty());
  if (prop)
    return prop;

  prop = new SVGMaskObserverList(aFrame);
  NS_ADDREF(prop);
  aFrame->SetProperty(SVGObserverUtils::MaskProperty(), prop);
  return prop;
}

template<class T>
static T*
GetEffectProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<T>* aProperty)
{
  if (!aURI)
    return nullptr;

  T* prop = aFrame->GetProperty(aProperty);
  if (prop)
    return prop;
  prop = new T(aURI, aFrame, false);
  NS_ADDREF(prop);
  aFrame->SetProperty(aProperty, prop);
  return prop;
}

bool
SVGObserverUtils::GetMarkerFrames(nsIFrame* aMarkedFrame,
                                  nsSVGMarkerFrame*(*aFrames)[3])
{
  MOZ_ASSERT(!aMarkedFrame->GetPrevContinuation() &&
             aMarkedFrame->IsSVGGeometryFrame() &&
             static_cast<SVGGeometryElement*>(aMarkedFrame->GetContent())->IsMarkable(),
             "Bad frame");

  bool foundMarker = false;
  RefPtr<URLAndReferrerInfo> markerURL;
  SVGMarkerObserver* observer;
  nsIFrame* marker;

#define GET_MARKER(type)                                                      \
  markerURL = GetMarkerURI(aMarkedFrame, &nsStyleSVG::mMarker##type);         \
  observer = GetEffectProperty(markerURL, aMarkedFrame,                       \
                               SVGObserverUtils::Marker##type##Property());   \
  marker = observer ?                                                         \
           observer->GetReferencedFrame(LayoutFrameType::SVGMarker, nullptr) :\
           nullptr;                                                           \
  foundMarker = foundMarker || bool(marker);                                  \
  (*aFrames)[nsSVGMark::e##type] = static_cast<nsSVGMarkerFrame*>(marker);

  GET_MARKER(Start)
  GET_MARKER(Mid)
  GET_MARKER(End)

#undef GET_MARKER

  return foundMarker;
}

SVGObserverUtils::ReferenceState
SVGObserverUtils::GetAndObserveFilters(nsIFrame* aFilteredFrame,
                                       nsTArray<nsSVGFilterFrame*>* aFilterFrames)
{
  SVGFilterObserverListForCSSProp* observerList =
    GetOrCreateFilterObserverListForCSS(aFilteredFrame);
  if (!observerList) {
    return eHasNoRefs;
  }

  const nsTArray<RefPtr<SVGFilterObserver>>& observers =
    observerList->GetObservers();
  if (observers.IsEmpty()) {
    return eHasNoRefs;
  }

  for (uint32_t i = 0; i < observers.Length(); i++) {
    nsSVGFilterFrame* filter = observers[i]->GetFilterFrame();
    if (!filter) {
      if (aFilterFrames) {
        aFilterFrames->Clear();
      }
      return eHasRefsSomeInvalid;
    }
    if (aFilterFrames) {
      aFilterFrames->AppendElement(filter);
    }
  }

  return eHasRefsAllValid;
}

SVGGeometryElement*
SVGObserverUtils::GetTextPathsReferencedPath(nsIFrame* aTextPathFrame)
{
  SVGTextPathObserver* property =
    aTextPathFrame->GetProperty(SVGObserverUtils::HrefAsTextPathProperty());

  if (!property) {
    nsIContent* content = aTextPathFrame->GetContent();
    nsAutoString href;
    static_cast<SVGTextPathElement*>(content)->HrefAsString(href);
    if (href.IsEmpty()) {
      return nullptr; // no URL
    }

    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = content->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              content->GetUncomposedDoc(), base);

    // There's no clear refererer policy spec about non-CSS SVG resource references
    // Bug 1415044 to investigate which referrer we should use
    RefPtr<URLAndReferrerInfo> target =
      new URLAndReferrerInfo(targetURI,
                             content->OwnerDoc()->GetDocumentURI(),
                             content->OwnerDoc()->GetReferrerPolicy());

    property = GetEffectProperty(target, aTextPathFrame,
                                 HrefAsTextPathProperty());
    if (!property) {
      return nullptr;
    }
  }

  Element* element = property->GetReferencedElement();
  return (element && element->IsNodeOfType(nsINode::eSHAPE)) ?
    static_cast<SVGGeometryElement*>(element) : nullptr;
}

void
SVGObserverUtils::RemoveTextPathObserver(nsIFrame* aTextPathFrame)
{
  aTextPathFrame->DeleteProperty(HrefAsTextPathProperty());
}

nsIFrame*
SVGObserverUtils::GetTemplateFrame(nsIFrame* aFrame,
                                   HrefToTemplateCallback aGetHref)
{
  SVGTemplateElementObserver* observer =
    aFrame->GetProperty(SVGObserverUtils::HrefToTemplateProperty());

  if (!observer) {
    nsAutoString href;
    aGetHref(href);
    if (href.IsEmpty()) {
      return nullptr; // no URL
    }

    // Convert href to an nsIURI
    nsIContent* content = aFrame->GetContent();
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = content->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              content->GetUncomposedDoc(), base);

    // There's no clear refererer policy spec about non-CSS SVG resource
    // references.  Bug 1415044 to investigate which referrer we should use.
    RefPtr<URLAndReferrerInfo> target =
      new URLAndReferrerInfo(targetURI,
                             content->OwnerDoc()->GetDocumentURI(),
                             content->OwnerDoc()->GetReferrerPolicy());

    observer = GetEffectProperty(target, aFrame,
                                 SVGObserverUtils::HrefToTemplateProperty());
  }

  return observer ? observer->GetReferencedFrame() : nullptr;
}

void
SVGObserverUtils::RemoveTemplateObserver(nsIFrame* aFrame)
{
  aFrame->DeleteProperty(SVGObserverUtils::HrefToTemplateProperty());
}

nsSVGPaintingProperty*
SVGObserverUtils::GetPaintingProperty(URLAndReferrerInfo* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>* aProperty)
{
  return GetEffectProperty(aURI, aFrame, aProperty);
}

nsSVGPaintingProperty*
SVGObserverUtils::GetPaintingPropertyForURI(URLAndReferrerInfo* aURI,
  nsIFrame* aFrame,
  URIObserverHashtablePropertyDescriptor aProperty)
{
  if (!aURI)
    return nullptr;

  SVGObserverUtils::URIObserverHashtable *hashtable =
    aFrame->GetProperty(aProperty);
  if (!hashtable) {
    hashtable = new SVGObserverUtils::URIObserverHashtable();
    aFrame->SetProperty(aProperty, hashtable);
  }
  nsSVGPaintingProperty* prop =
    static_cast<nsSVGPaintingProperty*>(hashtable->GetWeak(aURI));
  if (!prop) {
    bool watchImage = aProperty == SVGObserverUtils::BackgroundImageProperty();
    prop = new nsSVGPaintingProperty(aURI, aFrame, watchImage);
    hashtable->Put(aURI, prop);
  }
  return prop;
}

SVGObserverUtils::EffectProperties
SVGObserverUtils::GetEffectProperties(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  EffectProperties result;
  const nsStyleSVGReset *style = aFrame->StyleSVGReset();

  if (style->mClipPath.GetType() == StyleShapeSourceType::URL) {
    RefPtr<URLAndReferrerInfo> pathURI = SVGObserverUtils::GetClipPathURI(aFrame);
    result.mClipPath =
      GetPaintingProperty(pathURI, aFrame, ClipPathProperty());
  } else {
    result.mClipPath = nullptr;
  }

  MOZ_ASSERT(style->mMask.mImageCount > 0);
  result.mMaskObservers = style->HasMask()
                          ? GetOrCreateMaskProperty(aFrame) : nullptr;

  return result;
}

nsSVGPaintServerFrame *
SVGObserverUtils::GetPaintServer(nsIFrame* aTargetFrame,
                                 nsStyleSVGPaint nsStyleSVG::* aPaint)
{
  // If we're looking at a frame within SVG text, then we need to look up
  // to find the right frame to get the painting property off.  We should at
  // least look up past a text frame, and if the text frame's parent is the
  // anonymous block frame, then we look up to its parent (the SVGTextFrame).
  nsIFrame* frame = aTargetFrame;
  if (frame->GetContent()->IsText()) {
    frame = frame->GetParent();
    nsIFrame* grandparent = frame->GetParent();
    if (grandparent && grandparent->IsSVGTextFrame()) {
      frame = grandparent;
    }
  }

  const nsStyleSVG* svgStyle = frame->StyleSVG();
  if ((svgStyle->*aPaint).Type() != eStyleSVGPaintType_Server)
    return nullptr;

  RefPtr<URLAndReferrerInfo> paintServerURL =
    SVGObserverUtils::GetPaintURI(frame, aPaint);
  MOZ_ASSERT(aPaint == &nsStyleSVG::mFill || aPaint == &nsStyleSVG::mStroke);
  PaintingPropertyDescriptor propDesc = (aPaint == &nsStyleSVG::mFill) ?
                                        SVGObserverUtils::FillProperty() :
                                        SVGObserverUtils::StrokeProperty();
  nsSVGPaintingProperty *property =
    SVGObserverUtils::GetPaintingProperty(paintServerURL, frame, propDesc);
  if (!property)
    return nullptr;
  nsIFrame* result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  LayoutFrameType type = result->Type();
  if (type != LayoutFrameType::SVGLinearGradient &&
      type != LayoutFrameType::SVGRadialGradient &&
      type != LayoutFrameType::SVGPattern)
    return nullptr;

  return static_cast<nsSVGPaintServerFrame*>(result);
}

nsSVGClipPathFrame *
SVGObserverUtils::EffectProperties::GetClipPathFrame()
{
  if (!mClipPath)
    return nullptr;

  nsSVGClipPathFrame* frame = static_cast<nsSVGClipPathFrame*>(
    mClipPath->GetReferencedFrame(LayoutFrameType::SVGClipPath, nullptr));

  return frame;
}

nsTArray<nsSVGMaskFrame *>
SVGObserverUtils::EffectProperties::GetMaskFrames()
{
  nsTArray<nsSVGMaskFrame *> result;
  if (!mMaskObservers) {
    return result;
  }

  bool ok = true;
  const nsTArray<RefPtr<nsSVGPaintingProperty>>& observers =
    mMaskObservers->GetObservers();
  for (size_t i = 0; i < observers.Length(); i++) {
    nsSVGMaskFrame* maskFrame = static_cast<nsSVGMaskFrame*>(
      observers[i]->GetReferencedFrame(LayoutFrameType::SVGMask, &ok));
    MOZ_ASSERT(!maskFrame || ok);
    if (!ok) {
      // We can not find the specific SVG mask resource in the downloaded SVG
      // document. There are two possibilities:
      // 1. The given resource id is invalid.
      // 2. The given resource id refers to a viewbox.
      //
      // Hand it over to the style image.
      mMaskObservers->ResolveImage(i);
    }
    result.AppendElement(maskFrame);
  }

  return result;
}

bool
SVGObserverUtils::EffectProperties::HasNoOrValidEffects()
{
  return HasNoOrValidClipPath() && HasNoOrValidMask();
}

bool
SVGObserverUtils::EffectProperties::HasNoOrValidClipPath()
{
  if (mClipPath) {
    bool ok = true;
    nsSVGClipPathFrame* frame = static_cast<nsSVGClipPathFrame*>(
      mClipPath->GetReferencedFrame(LayoutFrameType::SVGClipPath, &ok));
    if (!ok || (frame && !frame->IsValid())) {
      return false;
    }
  }

  return true;
}

bool
SVGObserverUtils::EffectProperties::HasNoOrValidMask()
{
  if (mMaskObservers) {
    bool ok = true;
    const nsTArray<RefPtr<nsSVGPaintingProperty>>& observers =
      mMaskObservers->GetObservers();
    for (size_t i = 0; i < observers.Length(); i++) {
      observers[i]->GetReferencedFrame(LayoutFrameType::SVGMask, &ok);
      if (!ok) {
        return false;
      }
    }
  }

  return true;
}

void
SVGObserverUtils::UpdateEffects(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame->GetContent()->IsElement(),
               "aFrame's content should be an element");

  aFrame->DeleteProperty(FilterProperty());
  aFrame->DeleteProperty(MaskProperty());
  aFrame->DeleteProperty(ClipPathProperty());
  aFrame->DeleteProperty(MarkerStartProperty());
  aFrame->DeleteProperty(MarkerMidProperty());
  aFrame->DeleteProperty(MarkerEndProperty());
  aFrame->DeleteProperty(FillProperty());
  aFrame->DeleteProperty(StrokeProperty());
  aFrame->DeleteProperty(BackgroundImageProperty());

  // Ensure that the filter is repainted correctly
  // We can't do that in OnRenderingChange as the referenced frame may
  // not be valid
  GetOrCreateFilterObserverListForCSS(aFrame);

  if (aFrame->IsSVGGeometryFrame() &&
      static_cast<SVGGeometryElement*>(aFrame->GetContent())->IsMarkable()) {
    // Set marker properties here to avoid reference loops
    RefPtr<URLAndReferrerInfo> markerURL =
      GetMarkerURI(aFrame, &nsStyleSVG::mMarkerStart);
    GetEffectProperty(markerURL, aFrame, MarkerStartProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerMid);
    GetEffectProperty(markerURL, aFrame, MarkerMidProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerEnd);
    GetEffectProperty(markerURL, aFrame, MarkerEndProperty());
  }
}

SVGFilterObserverListForCSSProp*
SVGObserverUtils::GetFilterObserverList(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  if (!aFrame->StyleEffects()->HasFilters())
    return nullptr;

  return aFrame->GetProperty(FilterProperty());
}

void
SVGRenderingObserverList::InvalidateAll()
{
  if (mObservers.Count() == 0)
    return;

  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    observers.AppendElement(it.Get()->GetKey());
  }
  mObservers.Clear();

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->OnNonDOMMutationRenderingChange();
  }
}

void
SVGRenderingObserverList::InvalidateAllForReflow()
{
  if (mObservers.Count() == 0)
    return;

  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    SVGRenderingObserver* obs = it.Get()->GetKey();
    if (obs->ObservesReflow()) {
      observers.AppendElement(obs);
      it.Remove();
    }
  }

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->OnNonDOMMutationRenderingChange();
  }
}

void
SVGRenderingObserverList::RemoveAll()
{
  AutoTArray<SVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    observers.AppendElement(it.Get()->GetKey());
  }
  mObservers.Clear();

  // Our list is now cleared.  We need to notify the observers we've removed,
  // so they can update their state & remove themselves as mutation-observers.
  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->NotifyEvictedFromRenderingObserverList();
  }
}

void
SVGObserverUtils::AddRenderingObserver(Element* aElement,
                                       SVGRenderingObserver* aObserver)
{
  SVGRenderingObserverList* observerList = GetObserverList(aElement);
  if (!observerList) {
    observerList = new SVGRenderingObserverList();
    if (!observerList)
      return;
    aElement->SetProperty(nsGkAtoms::renderingobserverlist, observerList,
                          nsINode::DeleteProperty<SVGRenderingObserverList>);
  }
  aElement->SetHasRenderingObservers(true);
  observerList->Add(aObserver);
}

void
SVGObserverUtils::RemoveRenderingObserver(Element* aElement,
                                          SVGRenderingObserver* aObserver)
{
  SVGRenderingObserverList* observerList = GetObserverList(aElement);
  if (observerList) {
    NS_ASSERTION(observerList->Contains(aObserver),
                 "removing observer from an element we're not observing?");
    observerList->Remove(aObserver);
    if (observerList->IsEmpty()) {
      aElement->SetHasRenderingObservers(false);
    }
  }
}

void
SVGObserverUtils::RemoveAllRenderingObservers(Element* aElement)
{
  SVGRenderingObserverList* observerList = GetObserverList(aElement);
  if (observerList) {
    observerList->RemoveAll();
    aElement->SetHasRenderingObservers(false);
  }
}

void
SVGObserverUtils::InvalidateRenderingObservers(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement())
    return;

  // If the rendering has changed, the bounds may well have changed too:
  aFrame->DeleteProperty(nsSVGUtils::ObjectBoundingBoxProperty());

  SVGRenderingObserverList* observerList =
    GetObserverList(content->AsElement());
  if (observerList) {
    observerList->InvalidateAll();
    return;
  }

  // Check ancestor SVG containers. The root frame cannot be of type
  // eSVGContainer so we don't have to check f for null here.
  for (nsIFrame *f = aFrame->GetParent();
       f->IsFrameOfType(nsIFrame::eSVGContainer); f = f->GetParent()) {
    if (f->GetContent()->IsElement()) {
      observerList = GetObserverList(f->GetContent()->AsElement());
      if (observerList) {
        observerList->InvalidateAll();
        return;
      }
    }
  }
}

void
SVGObserverUtils::InvalidateDirectRenderingObservers(Element* aElement,
                                                     uint32_t aFlags /* = 0 */)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (frame) {
    // If the rendering has changed, the bounds may well have changed too:
    frame->DeleteProperty(nsSVGUtils::ObjectBoundingBoxProperty());
  }

  if (aElement->HasRenderingObservers()) {
    SVGRenderingObserverList* observerList = GetObserverList(aElement);
    if (observerList) {
      if (aFlags & INVALIDATE_REFLOW) {
        observerList->InvalidateAllForReflow();
      } else {
        observerList->InvalidateAll();
      }
    }
  }
}

void
SVGObserverUtils::InvalidateDirectRenderingObservers(nsIFrame* aFrame,
                                                     uint32_t aFlags /* = 0 */)
{
  nsIContent* content = aFrame->GetContent();
  if (content && content->IsElement()) {
    InvalidateDirectRenderingObservers(content->AsElement(), aFlags);
  }
}

already_AddRefed<nsIURI>
SVGObserverUtils::GetBaseURLForLocalRef(nsIContent* content, nsIURI* aDocURI)
{
  MOZ_ASSERT(content);

  // For a local-reference URL, resolve that fragment against the current
  // document that relative URLs are resolved against.
  nsCOMPtr<nsIURI> baseURI = content->OwnerDoc()->GetDocumentURI();

  nsCOMPtr<nsIURI> originalURI;
  // Content is in a shadow tree.  If this URL was specified in the subtree
  // referenced by the <use>(or -moz-binding) element, and that subtree came
  // from a separate resource document, then we want the fragment-only URL
  // to resolve to an element from the resource document.  Otherwise, the
  // URL was specified somewhere in the document with the <use> element, and
  // we want the fragment-only URL to resolve to an element in that document.
  if (SVGUseElement* use = content->GetContainingSVGUseShadowHost()) {
    originalURI = use->GetSourceDocURI();
  } else if (content->IsInAnonymousSubtree()) {
    nsIContent* bindingParent = content->GetBindingParent();

    if (bindingParent) {
      nsXBLBinding* binding = bindingParent->GetXBLBinding();
      if (binding) {
        originalURI = binding->GetSourceDocURI();
      } else {
        MOZ_ASSERT(content->IsInNativeAnonymousSubtree(),
                   "a non-native anonymous tree which is not from "
                   "an XBL binding?");
      }
    }
  }

  if (originalURI) {
    bool isEqualsExceptRef = false;
    aDocURI->EqualsExceptRef(originalURI, &isEqualsExceptRef);
    if (isEqualsExceptRef) {
      return originalURI.forget();
    }
  }

  return baseURI.forget();
}

static already_AddRefed<URLAndReferrerInfo>
ResolveURLUsingLocalRef(nsIFrame* aFrame, const css::URLValueData* aURL)
{
  MOZ_ASSERT(aFrame);

  if (!aURL) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = aURL->GetURI();
  RefPtr<URLAndReferrerInfo> result;

  // Non-local-reference URL.
  if (!aURL->IsLocalRef()) {
    if (!uri) {
      return nullptr;
    }
    result = new URLAndReferrerInfo(uri,
                                    aURL->mExtraData->GetReferrer(),
                                    aURL->mExtraData->GetReferrerPolicy());
    return result.forget();
  }

  nsCOMPtr<nsIURI> baseURI =
    SVGObserverUtils::GetBaseURLForLocalRef(aFrame->GetContent(), uri);

  nsCOMPtr<nsIURI> resolvedURI = aURL->ResolveLocalRef(baseURI);
  if (!resolvedURI) {
    return nullptr;
  }

  result = new URLAndReferrerInfo(resolvedURI,
                                  aURL->mExtraData->GetReferrer(),
                                  aURL->mExtraData->GetReferrerPolicy());
  return result.forget();
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetMarkerURI(nsIFrame* aFrame,
                               RefPtr<css::URLValue> nsStyleSVG::* aMarker)
{
  return ResolveURLUsingLocalRef(aFrame, aFrame->StyleSVG()->*aMarker);
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetClipPathURI(nsIFrame* aFrame)
{
  const nsStyleSVGReset* svgResetStyle = aFrame->StyleSVGReset();
  MOZ_ASSERT(svgResetStyle->mClipPath.GetType() == StyleShapeSourceType::URL);

  css::URLValue* url = svgResetStyle->mClipPath.GetURL();
  return ResolveURLUsingLocalRef(aFrame, url);
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetFilterURI(nsIFrame* aFrame, uint32_t aIndex)
{
  const nsStyleEffects* effects = aFrame->StyleEffects();
  MOZ_ASSERT(effects->mFilters.Length() > aIndex);
  MOZ_ASSERT(effects->mFilters[aIndex].GetType() == NS_STYLE_FILTER_URL);

  return ResolveURLUsingLocalRef(aFrame, effects->mFilters[aIndex].GetURL());
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetFilterURI(nsIFrame* aFrame, const nsStyleFilter& aFilter)
{
  MOZ_ASSERT(aFrame->StyleEffects()->mFilters.Length());
  MOZ_ASSERT(aFilter.GetType() == NS_STYLE_FILTER_URL);

  return ResolveURLUsingLocalRef(aFrame, aFilter.GetURL());
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetPaintURI(nsIFrame* aFrame,
                              nsStyleSVGPaint nsStyleSVG::* aPaint)
{
  const nsStyleSVG* svgStyle = aFrame->StyleSVG();
  MOZ_ASSERT((svgStyle->*aPaint).Type() ==
             nsStyleSVGPaintType::eStyleSVGPaintType_Server);

  return ResolveURLUsingLocalRef(aFrame,
                                 (svgStyle->*aPaint).GetPaintServer());
}

already_AddRefed<URLAndReferrerInfo>
SVGObserverUtils::GetMaskURI(nsIFrame* aFrame, uint32_t aIndex)
{
  const nsStyleSVGReset* svgReset = aFrame->StyleSVGReset();
  MOZ_ASSERT(svgReset->mMask.mLayers.Length() > aIndex);

  css::URLValueData* data =
    svgReset->mMask.mLayers[aIndex].mImage.GetURLValue();
  return ResolveURLUsingLocalRef(aFrame, data);
}

} // namespace mozilla

