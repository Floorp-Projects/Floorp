/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGEffects.h"

// Keep others in (case-insensitive) order:
#include "mozilla/RestyleManagerHandle.h"
#include "mozilla/RestyleManagerHandleInlines.h"
#include "nsCSSFrameConstructor.h"
#include "nsISupportsImpl.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsIReflowCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "SVGGeometryElement.h"
#include "SVGUseElement.h"

using namespace mozilla;
using namespace mozilla::dom;

void
nsSVGRenderingObserver::StartListening()
{
  Element* target = GetTarget();
  if (target) {
    target->AddMutationObserver(this);
  }
}

void
nsSVGRenderingObserver::StopListening()
{
  Element* target = GetTarget();

  if (target) {
    target->RemoveMutationObserver(this);
    if (mInObserverList) {
      nsSVGEffects::RemoveRenderingObserver(target, this);
      mInObserverList = false;
    }
  }
  NS_ASSERTION(!mInObserverList, "still in an observer list?");
}

static nsSVGRenderingObserverList *
GetObserverList(Element *aElement)
{
  return static_cast<nsSVGRenderingObserverList*>
    (aElement->GetProperty(nsGkAtoms::renderingobserverlist));
}

Element*
nsSVGRenderingObserver::GetReferencedElement()
{
  Element* target = GetTarget();
#ifdef DEBUG
  if (target) {
    nsSVGRenderingObserverList *observerList = GetObserverList(target);
    bool inObserverList = observerList && observerList->Contains(this);
    NS_ASSERTION(inObserverList == mInObserverList, "failed to track whether we're in our referenced element's observer list!");
  } else {
    NS_ASSERTION(!mInObserverList, "In whose observer list are we, then?");
  }
#endif
  if (target && !mInObserverList) {
    nsSVGEffects::AddRenderingObserver(target, this);
    mInObserverList = true;
  }
  return target;
}

nsIFrame*
nsSVGRenderingObserver::GetReferencedFrame()
{
  Element* referencedElement = GetReferencedElement();
  return referencedElement ? referencedElement->GetPrimaryFrame() : nullptr;
}

nsIFrame*
nsSVGRenderingObserver::GetReferencedFrame(nsIAtom* aFrameType, bool* aOK)
{
  nsIFrame* frame = GetReferencedFrame();
  if (frame) {
    if (frame->GetType() == aFrameType)
      return frame;
    if (aOK) {
      *aOK = false;
    }
  }
  return nullptr;
}

void
nsSVGRenderingObserver::InvalidateViaReferencedElement()
{
  mInObserverList = false;
  DoUpdate();
}

void
nsSVGRenderingObserver::NotifyEvictedFromRenderingObserverList()
{
  mInObserverList = false; // We've been removed from rendering-obs. list.
  StopListening();            // Remove ourselves from mutation-obs. list.
}

void
nsSVGRenderingObserver::AttributeChanged(nsIDocument* aDocument,
                                         dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
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

  DoUpdate();
}

void
nsSVGRenderingObserver::ContentAppended(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aFirstNewContent,
                                        int32_t /* unused */)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentInserted(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aChild,
                                        int32_t /* unused */)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentRemoved(nsIDocument* aDocument,
                                       nsIContent* aContainer,
                                       nsIContent* aChild,
                                       int32_t aIndexInContainer,
                                       nsIContent* aPreviousSibling)
{
  DoUpdate();
}

/**
 * Note that in the current setup there are two separate observer lists.
 *
 * In nsSVGIDRenderingObserver's ctor, the new object adds itself to the
 * mutation observer list maintained by the referenced element. In this way the
 * nsSVGIDRenderingObserver is notified if there are any attribute or content
 * tree changes to the element or any of its *descendants*.
 *
 * In nsSVGIDRenderingObserver::GetReferencedElement() the
 * nsSVGIDRenderingObserver object also adds itself to an
 * nsSVGRenderingObserverList object belonging to the referenced
 * element.
 *
 * XXX: it would be nice to have a clear and concise executive summary of the
 * benefits/necessity of maintaining a second observer list.
 */

nsSVGIDRenderingObserver::nsSVGIDRenderingObserver(nsIURI* aURI,
                                                   nsIContent* aObservingContent,
                                                   bool aReferenceImage)
  : mElement(this)
{
  // Start watching the target element
  mElement.Reset(aObservingContent, aURI, true, aReferenceImage);
  StartListening();
}

nsSVGIDRenderingObserver::~nsSVGIDRenderingObserver()
{
  StopListening();
}

void
nsSVGIDRenderingObserver::DoUpdate()
{
  if (mElement.get() && mInObserverList) {
    nsSVGEffects::RemoveRenderingObserver(mElement.get(), this);
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

NS_IMPL_ISUPPORTS(nsSVGRenderingObserverProperty, nsIMutationObserver)

void
nsSVGRenderingObserverProperty::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();

  nsIFrame* frame = mFrameReference.Get();
  if (frame && frame->IsFrameOfType(nsIFrame::eSVG)) {
    // Changes should propagate out to things that might be observing
    // the referencing frame or its ancestors.
    nsLayoutUtils::PostRestyleEvent(
      frame->GetContent()->AsElement(), nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGFilterReference)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGFilterReference)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGFilterReference)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsSVGFilterReference)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsSVGFilterReference)
  tmp->StopListening();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGFilterReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsSVGIDRenderingObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilterReference)
NS_INTERFACE_MAP_END

nsSVGFilterFrame *
nsSVGFilterReference::GetFilterFrame()
{
  return static_cast<nsSVGFilterFrame *>
    (GetReferencedFrame(nsGkAtoms::svgFilterFrame, nullptr));
}

void
nsSVGFilterReference::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();

  if (mFilterChainObserver) {
    mFilterChainObserver->Invalidate();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGFilterChainObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGFilterChainObserver)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsSVGFilterChainObserver)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsSVGFilterChainObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReferences)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsSVGFilterChainObserver)
  tmp->DetachReferences();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReferences);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGFilterChainObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsSVGFilterChainObserver::nsSVGFilterChainObserver(const nsTArray<nsStyleFilter>& aFilters,
                                                   nsIContent* aFilteredElement,
                                                   nsIFrame* aFilteredFrame)
{
  for (uint32_t i = 0; i < aFilters.Length(); i++) {
    if (aFilters[i].GetType() != NS_STYLE_FILTER_URL)
      continue;

    // aFilteredFrame can be null if this filter belongs to a
    // CanvasRenderingContext2D.
    nsCOMPtr<nsIURI> filterURL = aFilteredFrame
      ? nsSVGEffects::GetFilterURI(aFilteredFrame, i)
      : aFilters[i].GetURL()->ResolveLocalRef(aFilteredElement);

    RefPtr<nsSVGFilterReference> reference =
      new nsSVGFilterReference(filterURL, aFilteredElement, this);
    mReferences.AppendElement(reference);
  }
}

nsSVGFilterChainObserver::~nsSVGFilterChainObserver()
{
  DetachReferences();
}

bool
nsSVGFilterChainObserver::ReferencesValidResources()
{
  for (uint32_t i = 0; i < mReferences.Length(); i++) {
    if (!mReferences[i]->ReferencesValidResource())
      return false;
  }
  return true;
}

bool
nsSVGFilterChainObserver::IsInObserverLists() const
{
  for (uint32_t i = 0; i < mReferences.Length(); i++) {
    if (!mReferences[i]->IsInObserverList())
      return false;
  }
  return true;
}

void
nsSVGFilterProperty::DoUpdate()
{
  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  // Repaint asynchronously in case the filter frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame);

  if (frame && frame->IsFrameOfType(nsIFrame::eSVG)) {
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
nsSVGMarkerProperty::DoUpdate()
{
  nsSVGRenderingObserverProperty::DoUpdate();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  NS_ASSERTION(frame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");

  // Repaint asynchronously in case the marker frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame);

  // Don't need to request ReflowFrame if we're being reflowed.
  if (!(frame->GetStateBits() & NS_FRAME_IN_REFLOW)) {
    changeHint |= nsChangeHint_InvalidateRenderingObservers;
    // XXXjwatt: We need to unify SVG into standard reflow so we can just use
    // nsChangeHint_NeedReflow | nsChangeHint_NeedDirtyReflow here.
    // XXXSDL KILL THIS!!!
    nsSVGUtils::ScheduleReflowSVG(frame);
  }
  frame->PresContext()->RestyleManager()->PostRestyleEvent(
    frame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}

NS_IMPL_ISUPPORTS(nsSVGMaskProperty, nsISupports)

nsSVGMaskProperty::nsSVGMaskProperty(nsIFrame* aFrame)
{
  const nsStyleSVGReset *svgReset = aFrame->StyleSVGReset();

  for (uint32_t i = 0; i < svgReset->mMask.mImageCount; i++) {
    nsCOMPtr<nsIURI> maskUri = nsSVGEffects::GetMaskURI(aFrame, i);
    nsSVGPaintingProperty* prop = new nsSVGPaintingProperty(maskUri, aFrame,
                                                            false);
    mProperties.AppendElement(prop);
  }
}

bool
nsSVGTextPathProperty::TargetIsValid()
{
  Element* target = GetTarget();
  return target && target->IsSVGElement(nsGkAtoms::path);
}

void
nsSVGTextPathProperty::DoUpdate()
{
  nsSVGRenderingObserverProperty::DoUpdate();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  NS_ASSERTION(frame->IsFrameOfType(nsIFrame::eSVG) || frame->IsSVGText(),
               "SVG frame expected");

  // Avoid getting into an infinite loop of reflows if the <textPath> is
  // pointing to one of its ancestors.  TargetIsValid returns true iff
  // the target element is a <path> element, and we would not have this
  // nsSVGTextPathProperty if this <textPath> were a descendant of the
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
nsSVGPaintingProperty::DoUpdate()
{
  nsSVGRenderingObserverProperty::DoUpdate();

  nsIFrame* frame = mFrameReference.Get();
  if (!frame)
    return;

  if (frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    nsLayoutUtils::PostRestyleEvent(
      frame->GetContent()->AsElement(), nsRestyleHint(0),
      nsChangeHint_InvalidateRenderingObservers);
    frame->InvalidateFrameSubtree();
  } else {
    InvalidateAllContinuations(frame);
  }
}

static nsSVGFilterProperty*
GetOrCreateFilterProperty(nsIFrame* aFrame)
{
  const nsStyleEffects* effects = aFrame->StyleEffects();
  if (!effects->HasFilters())
    return nullptr;

  FrameProperties props = aFrame->Properties();
  nsSVGFilterProperty *prop = props.Get(nsSVGEffects::FilterProperty());
  if (prop)
    return prop;
  prop = new nsSVGFilterProperty(effects->mFilters, aFrame);
  NS_ADDREF(prop);
  props.Set(nsSVGEffects::FilterProperty(), prop);
  return prop;
}

static nsSVGMaskProperty*
GetOrCreateMaskProperty(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  nsSVGMaskProperty *prop = props.Get(nsSVGEffects::MaskProperty());
  if (prop)
    return prop;

  prop = new nsSVGMaskProperty(aFrame);
  NS_ADDREF(prop);
  props.Set(nsSVGEffects::MaskProperty(), prop);
  return prop;
}

template<class T>
static T*
GetEffectProperty(nsIURI* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<T>* aProperty)
{
  if (!aURI)
    return nullptr;

  FrameProperties props = aFrame->Properties();
  T* prop = props.Get(aProperty);
  if (prop)
    return prop;
  prop = new T(aURI, aFrame, false);
  NS_ADDREF(prop);
  props.Set(aProperty, prop);
  return prop;
}

nsSVGMarkerProperty*
nsSVGEffects::GetMarkerProperty(nsIURI* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<nsSVGMarkerProperty>* aProperty)
{
  MOZ_ASSERT(aFrame->GetType() == nsGkAtoms::svgGeometryFrame &&
             static_cast<SVGGeometryElement*>(aFrame->GetContent())->IsMarkable(),
             "Bad frame");
  return GetEffectProperty(aURI, aFrame, aProperty);
}

nsSVGTextPathProperty*
nsSVGEffects::GetTextPathProperty(nsIURI* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<nsSVGTextPathProperty>* aProperty)
{
  return GetEffectProperty(aURI, aFrame, aProperty);
}

nsSVGPaintingProperty*
nsSVGEffects::GetPaintingProperty(nsIURI* aURI, nsIFrame* aFrame,
  const mozilla::FramePropertyDescriptor<nsSVGPaintingProperty>* aProperty)
{
  return GetEffectProperty(aURI, aFrame, aProperty);
}

nsSVGPaintingProperty*
nsSVGEffects::GetPaintingPropertyForURI(nsIURI* aURI, nsIFrame* aFrame,
  URIObserverHashtablePropertyDescriptor aProperty)
{
  if (!aURI)
    return nullptr;

  FrameProperties props = aFrame->Properties();
  nsSVGEffects::URIObserverHashtable *hashtable = props.Get(aProperty);
  if (!hashtable) {
    hashtable = new nsSVGEffects::URIObserverHashtable();
    props.Set(aProperty, hashtable);
  }
  nsSVGPaintingProperty* prop =
    static_cast<nsSVGPaintingProperty*>(hashtable->GetWeak(aURI));
  if (!prop) {
    bool watchImage = aProperty == nsSVGEffects::BackgroundImageProperty();
    prop = new nsSVGPaintingProperty(aURI, aFrame, watchImage);
    hashtable->Put(aURI, prop);
  }
  return prop;
}

nsSVGEffects::EffectProperties
nsSVGEffects::GetEffectProperties(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  EffectProperties result;
  const nsStyleSVGReset *style = aFrame->StyleSVGReset();

  result.mFilter = GetOrCreateFilterProperty(aFrame);

  if (style->mClipPath.GetType() == StyleShapeSourceType::URL) {
    nsCOMPtr<nsIURI> pathURI = nsSVGEffects::GetClipPathURI(aFrame);
    result.mClipPath =
      GetPaintingProperty(pathURI, aFrame, ClipPathProperty());
  } else {
    result.mClipPath = nullptr;
  }

  MOZ_ASSERT(style->mMask.mImageCount > 0);
  result.mMask = style->mMask.HasLayerWithImage()
                 ? GetOrCreateMaskProperty(aFrame) : nullptr;

  return result;
}

nsSVGPaintServerFrame *
nsSVGEffects::GetPaintServer(nsIFrame* aTargetFrame,
                             nsStyleSVGPaint nsStyleSVG::* aPaint,
                             PaintingPropertyDescriptor aType)
{
  const nsStyleSVG* svgStyle = aTargetFrame->StyleSVG();
  if ((svgStyle->*aPaint).Type() != eStyleSVGPaintType_Server)
    return nullptr;

  // If we're looking at a frame within SVG text, then we need to look up
  // to find the right frame to get the painting property off.  We should at
  // least look up past a text frame, and if the text frame's parent is the
  // anonymous block frame, then we look up to its parent (the SVGTextFrame).
  nsIFrame* frame = aTargetFrame;
  if (frame->GetContent()->IsNodeOfType(nsINode::eTEXT)) {
    frame = frame->GetParent();
    nsIFrame* grandparent = frame->GetParent();
    if (grandparent && grandparent->GetType() == nsGkAtoms::svgTextFrame) {
      frame = grandparent;
    }
  }

  nsCOMPtr<nsIURI> paintServerURL = nsSVGEffects::GetPaintURI(frame, aPaint);
  nsSVGPaintingProperty *property =
    nsSVGEffects::GetPaintingProperty(paintServerURL, frame, aType);
  if (!property)
    return nullptr;
  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nullptr;

  nsIAtom *type = result->GetType();
  if (type != nsGkAtoms::svgLinearGradientFrame &&
      type != nsGkAtoms::svgRadialGradientFrame &&
      type != nsGkAtoms::svgPatternFrame)
    return nullptr;

  return static_cast<nsSVGPaintServerFrame*>(result);
}

nsSVGClipPathFrame *
nsSVGEffects::EffectProperties::GetClipPathFrame()
{
  if (!mClipPath)
    return nullptr;

  nsSVGClipPathFrame *frame = static_cast<nsSVGClipPathFrame *>
    (mClipPath->GetReferencedFrame(nsGkAtoms::svgClipPathFrame, nullptr));

  return frame;
}

nsTArray<nsSVGMaskFrame *>
nsSVGEffects::EffectProperties::GetMaskFrames()
{
  nsTArray<nsSVGMaskFrame *> result;
  if (!mMask)
    return result;

  bool ok = true;
  const nsTArray<RefPtr<nsSVGPaintingProperty>>& props = mMask->GetProps();
  for (size_t i = 0; i < props.Length(); i++) {
    nsSVGMaskFrame* maskFrame =
      static_cast<nsSVGMaskFrame *>(props[i]->GetReferencedFrame(
                                                nsGkAtoms::svgMaskFrame, &ok));
    MOZ_ASSERT_IF(maskFrame, ok);
    result.AppendElement(maskFrame);
  }

  return result;
}

bool
nsSVGEffects::EffectProperties::HasNoOrValidEffects()
{
  return HasNoOrValidClipPath() && HasNoOrValidMask() && HasNoOrValidFilter();
}

bool
nsSVGEffects::EffectProperties::MightHaveNoneSVGMask() const
{
  if (!mMask) {
    return false;
  }

  const nsTArray<RefPtr<nsSVGPaintingProperty>>& props = mMask->GetProps();
  for (size_t i = 0; i < props.Length(); i++) {
    if (!props[i] ||
        !props[i]->GetReferencedFrame(nsGkAtoms::svgMaskFrame, nullptr)) {
      return true;
    }
  }

  return false;
}

bool
nsSVGEffects::EffectProperties::HasNoOrValidClipPath()
{
  if (mClipPath) {
    bool ok = true;
    nsSVGClipPathFrame *frame = static_cast<nsSVGClipPathFrame *>
      (mClipPath->GetReferencedFrame(nsGkAtoms::svgClipPathFrame, &ok));
    if (!ok || (frame && !frame->IsValid())) {
      return false;
    }
  }

  return true;
}

bool
nsSVGEffects::EffectProperties::HasNoOrValidMask()
{
  if (mMask) {
    bool ok = true;
    const nsTArray<RefPtr<nsSVGPaintingProperty>>& props = mMask->GetProps();
    for (size_t i = 0; i < props.Length(); i++) {
      props[i]->GetReferencedFrame(nsGkAtoms::svgMaskFrame, &ok);
      if (!ok) {
        return false;
      }
    }
  }

  return true;
}

void
nsSVGEffects::UpdateEffects(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame->GetContent()->IsElement(),
               "aFrame's content should be an element");

  FrameProperties props = aFrame->Properties();
  props.Delete(FilterProperty());
  props.Delete(MaskProperty());
  props.Delete(ClipPathProperty());
  props.Delete(MarkerBeginProperty());
  props.Delete(MarkerMiddleProperty());
  props.Delete(MarkerEndProperty());
  props.Delete(FillProperty());
  props.Delete(StrokeProperty());
  props.Delete(BackgroundImageProperty());

  // Ensure that the filter is repainted correctly
  // We can't do that in DoUpdate as the referenced frame may not be valid
  GetOrCreateFilterProperty(aFrame);

  if (aFrame->GetType() == nsGkAtoms::svgGeometryFrame &&
      static_cast<SVGGeometryElement*>(aFrame->GetContent())->IsMarkable()) {
    // Set marker properties here to avoid reference loops
    nsCOMPtr<nsIURI> markerURL =
      GetMarkerURI(aFrame, &nsStyleSVG::mMarkerStart);
    GetMarkerProperty(markerURL, aFrame, MarkerBeginProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerMid);
    GetMarkerProperty(markerURL, aFrame, MarkerMiddleProperty());
    markerURL = GetMarkerURI(aFrame, &nsStyleSVG::mMarkerEnd);
    GetMarkerProperty(markerURL, aFrame, MarkerEndProperty());
  }
}

nsSVGFilterProperty*
nsSVGEffects::GetFilterProperty(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  if (!aFrame->StyleEffects()->HasFilters())
    return nullptr;

  return aFrame->Properties().Get(FilterProperty());
}

void
nsSVGRenderingObserverList::InvalidateAll()
{
  if (mObservers.Count() == 0)
    return;

  AutoTArray<nsSVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    observers.AppendElement(it.Get()->GetKey());
  }
  mObservers.Clear();

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->InvalidateViaReferencedElement();
  }
}

void
nsSVGRenderingObserverList::InvalidateAllForReflow()
{
  if (mObservers.Count() == 0)
    return;

  AutoTArray<nsSVGRenderingObserver*,10> observers;

  for (auto it = mObservers.Iter(); !it.Done(); it.Next()) {
    nsSVGRenderingObserver* obs = it.Get()->GetKey();
    if (obs->ObservesReflow()) {
      observers.AppendElement(obs);
      it.Remove();
    }
  }

  for (uint32_t i = 0; i < observers.Length(); ++i) {
    observers[i]->InvalidateViaReferencedElement();
  }
}

void
nsSVGRenderingObserverList::RemoveAll()
{
  AutoTArray<nsSVGRenderingObserver*,10> observers;

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
nsSVGEffects::AddRenderingObserver(Element* aElement,
                                   nsSVGRenderingObserver* aObserver)
{
  nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
  if (!observerList) {
    observerList = new nsSVGRenderingObserverList();
    if (!observerList)
      return;
    aElement->SetProperty(nsGkAtoms::renderingobserverlist, observerList,
                          nsINode::DeleteProperty<nsSVGRenderingObserverList>);
  }
  aElement->SetHasRenderingObservers(true);
  observerList->Add(aObserver);
}

void
nsSVGEffects::RemoveRenderingObserver(Element* aElement,
                                      nsSVGRenderingObserver* aObserver)
{
  nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
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
nsSVGEffects::RemoveAllRenderingObservers(Element* aElement)
{
  nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
  if (observerList) {
    observerList->RemoveAll();
    aElement->SetHasRenderingObservers(false);
  }
}

void
nsSVGEffects::InvalidateRenderingObservers(nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement())
    return;

  // If the rendering has changed, the bounds may well have changed too:
  aFrame->Properties().Delete(nsSVGUtils::ObjectBoundingBoxProperty());

  nsSVGRenderingObserverList *observerList =
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
nsSVGEffects::InvalidateDirectRenderingObservers(Element* aElement, uint32_t aFlags /* = 0 */)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (frame) {
    // If the rendering has changed, the bounds may well have changed too:
    frame->Properties().Delete(nsSVGUtils::ObjectBoundingBoxProperty());
  }

  if (aElement->HasRenderingObservers()) {
    nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
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
nsSVGEffects::InvalidateDirectRenderingObservers(nsIFrame* aFrame, uint32_t aFlags /* = 0 */)
{
  nsIContent* content = aFrame->GetContent();
  if (content && content->IsElement()) {
    InvalidateDirectRenderingObservers(content->AsElement(), aFlags);
  }
}

static already_AddRefed<nsIURI>
ResolveURLUsingLocalRef(nsIFrame* aFrame, const css::URLValueData* aURL)
{
  MOZ_ASSERT(aFrame);

  if (!aURL) {
    return nullptr;
  }

  // Non-local-reference URL.
  if (!aURL->IsLocalRef()) {
    nsCOMPtr<nsIURI> result = aURL->GetURI();
    return result.forget();
  }

  // For a local-reference URL, resolve that fragment against the current
  // document that relative URLs are resolved against.
  nsIContent* content = aFrame->GetContent();
  nsCOMPtr<nsIURI> baseURI = content->OwnerDoc()->GetDocumentURI();

  if (content->IsInAnonymousSubtree()) {
    nsIContent* bindingParent = content->GetBindingParent();
    nsCOMPtr<nsIURI> originalURI;

    // content is in a shadow tree.  If this URL was specified in the subtree
    // referenced by the <use>(or -moz-binding) element, and that subtree came
    // from a separate resource document, then we want the fragment-only URL
    // to resolve to an element from the resource document.  Otherwise, the
    // URL was specified somewhere in the document with the <use> element, and
    // we want the fragment-only URL to resolve to an element in that document.
    if (bindingParent) {
      if (content->IsAnonymousContentInSVGUseSubtree()) {
        SVGUseElement* useElement = static_cast<SVGUseElement*>(bindingParent);
        originalURI = useElement->GetSourceDocURI();
      } else {
        nsXBLBinding* binding = bindingParent->GetXBLBinding();
        if (binding) {
          originalURI = binding->GetSourceDocURI();
        } else {
          MOZ_ASSERT(content->IsInNativeAnonymousSubtree(),
                     "an non-native anonymous tree which is not from "
                     "an XBL binding?");
        }
      }

      if (originalURI && aURL->EqualsExceptRef(originalURI)) {
        baseURI = originalURI;
      }
    }
  }

  return aURL->ResolveLocalRef(baseURI);
}

already_AddRefed<nsIURI>
nsSVGEffects::GetMarkerURI(nsIFrame* aFrame,
                           RefPtr<css::URLValue> nsStyleSVG::* aMarker)
{
  return ResolveURLUsingLocalRef(aFrame, aFrame->StyleSVG()->*aMarker);
}

already_AddRefed<nsIURI>
nsSVGEffects::GetClipPathURI(nsIFrame* aFrame)
{
  const nsStyleSVGReset* svgResetStyle = aFrame->StyleSVGReset();
  MOZ_ASSERT(svgResetStyle->mClipPath.GetType() == StyleShapeSourceType::URL);

  css::URLValue* url = svgResetStyle->mClipPath.GetURL();
  return ResolveURLUsingLocalRef(aFrame, url);
}

already_AddRefed<nsIURI>
nsSVGEffects::GetFilterURI(nsIFrame* aFrame, uint32_t aIndex)
{
  const nsStyleEffects* effects = aFrame->StyleEffects();
  MOZ_ASSERT(effects->mFilters.Length() > aIndex);
  MOZ_ASSERT(effects->mFilters[aIndex].GetType() == NS_STYLE_FILTER_URL);

  return ResolveURLUsingLocalRef(aFrame, effects->mFilters[aIndex].GetURL());
}

already_AddRefed<nsIURI>
nsSVGEffects::GetFilterURI(nsIFrame* aFrame, const nsStyleFilter& aFilter)
{
  MOZ_ASSERT(aFrame->StyleEffects()->mFilters.Length());
  MOZ_ASSERT(aFilter.GetType() == NS_STYLE_FILTER_URL);

  return ResolveURLUsingLocalRef(aFrame, aFilter.GetURL());
}

already_AddRefed<nsIURI>
nsSVGEffects::GetPaintURI(nsIFrame* aFrame,
                          nsStyleSVGPaint nsStyleSVG::* aPaint)
{
  const nsStyleSVG* svgStyle = aFrame->StyleSVG();
  MOZ_ASSERT((svgStyle->*aPaint).Type() ==
             nsStyleSVGPaintType::eStyleSVGPaintType_Server);

  return ResolveURLUsingLocalRef(aFrame,
                                 (svgStyle->*aPaint).GetPaintServer());
}

already_AddRefed<nsIURI>
nsSVGEffects::GetMaskURI(nsIFrame* aFrame, uint32_t aIndex)
{
  const nsStyleSVGReset* svgReset = aFrame->StyleSVGReset();
  MOZ_ASSERT(svgReset->mMask.mLayers.Length() > aIndex);

  return ResolveURLUsingLocalRef(aFrame,
                                 svgReset->mMask.mLayers[aIndex].mSourceURI);
}
