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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@mozilla.com
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

#include "nsSVGEffects.h"
#include "nsISupportsImpl.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGTextPathFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsFrameManager.h"

using namespace mozilla;
using namespace mozilla::dom;

// nsSVGRenderingObserver impl
NS_IMPL_ISUPPORTS1(nsSVGRenderingObserver, nsIMutationObserver)

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
      mInObserverList = PR_FALSE;
    }
  }
  NS_ASSERTION(!mInObserverList, "still in an observer list?");
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

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
// We can ignore that warning because we know that mElement's constructor 
// doesn't dereference the pointer passed to it.
#pragma warning(push)
#pragma warning(disable:4355)
#endif
nsSVGIDRenderingObserver::nsSVGIDRenderingObserver(nsIURI *aURI,
                                                   nsIFrame *aFrame,
                                                   PRBool aReferenceImage)
  : mElement(this), mFrame(aFrame),
    mFramePresShell(aFrame->PresContext()->PresShell())
#ifdef _MSC_VER
#pragma warning(pop)
#endif
{
  // Start watching the target element
  mElement.Reset(aFrame->GetContent(), aURI, PR_TRUE, aReferenceImage);
  StartListening();
}

nsSVGIDRenderingObserver::~nsSVGIDRenderingObserver()
{
  StopListening();
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
    PRBool inObserverList = observerList && observerList->Contains(this);
    NS_ASSERTION(inObserverList == mInObserverList, "failed to track whether we're in our referenced element's observer list!");
  } else {
    NS_ASSERTION(!mInObserverList, "In whose observer list are we, then?");
  }
#endif
  if (target && !mInObserverList) {
    nsSVGEffects::AddRenderingObserver(target, this);
    mInObserverList = PR_TRUE;
  }
  return target;
}

nsIFrame*
nsSVGRenderingObserver::GetReferencedFrame()
{
  Element* referencedElement = GetReferencedElement();
  return referencedElement ? referencedElement->GetPrimaryFrame() : nsnull;
}

nsIFrame*
nsSVGRenderingObserver::GetReferencedFrame(nsIAtom* aFrameType, PRBool* aOK)
{
  nsIFrame* frame = GetReferencedFrame();
  if (frame && frame->GetType() == aFrameType)
    return frame;
  if (aOK) {
    *aOK = PR_FALSE;
  }
  return nsnull;
}

void
nsSVGIDRenderingObserver::DoUpdate()
{
  if (mFramePresShell->IsDestroying()) {
    // mFrame is no longer valid. Bail out.
    mFrame = nsnull;
    return;
  }
  if (mElement.get() && mInObserverList) {
    nsSVGEffects::RemoveRenderingObserver(mElement.get(), this);
    mInObserverList = PR_FALSE;
  }
  if (mFrame && mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    // Changes should propagate out to things that might be observing
    // the referencing frame or its ancestors.
    nsSVGEffects::InvalidateRenderingObservers(mFrame);
  }
}

void
nsSVGRenderingObserver::InvalidateViaReferencedElement()
{
  mInObserverList = PR_FALSE;
  DoUpdate();
}

void
nsSVGRenderingObserver::AttributeChanged(nsIDocument* aDocument,
                                         dom::Element* aElement,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         PRInt32 aModType)
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
nsSVGRenderingObserver::ContentAppended(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        nsIContent *aFirstNewContent,
                                        PRInt32 /* unused */)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentInserted(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        nsIContent *aChild,
                                        PRInt32 /* unused */)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentRemoved(nsIDocument *aDocument,
                                       nsIContent *aContainer,
                                       nsIContent *aChild,
                                       PRInt32 aIndexInContainer,
                                       nsIContent *aPreviousSibling)
{
  DoUpdate();
}

NS_IMPL_ISUPPORTS_INHERITED1(nsSVGFilterProperty,
                             nsSVGIDRenderingObserver,
                             nsISVGFilterProperty)

nsSVGFilterFrame *
nsSVGFilterProperty::GetFilterFrame()
{
  return static_cast<nsSVGFilterFrame *>
    (GetReferencedFrame(nsGkAtoms::svgFilterFrame, nsnull));
}

static void
InvalidateAllContinuations(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f; f = f->GetNextContinuation()) {
    f->InvalidateOverflowRect();
  }
}

void
nsSVGFilterProperty::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  // Repaint asynchronously in case the filter frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_UpdateEffects);

  if (!mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    NS_UpdateHint(changeHint, nsChangeHint_ReflowFrame);
  }
  mFramePresShell->FrameConstructor()->PostRestyleEvent(
    mFrame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}

void
nsSVGMarkerProperty::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  NS_ASSERTION(mFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");

  // Repaint asynchronously
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_UpdateEffects);

  mFramePresShell->FrameConstructor()->PostRestyleEvent(
    mFrame->GetContent()->AsElement(), nsRestyleHint(0), changeHint);
}

void
nsSVGTextPathProperty::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  NS_ASSERTION(mFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");

  if (mFrame->GetType() == nsGkAtoms::svgTextPathFrame) {
    nsSVGTextPathFrame* textPathFrame = static_cast<nsSVGTextPathFrame*>(mFrame);
    textPathFrame->NotifyGlyphMetricsChange();
  }
}

void
nsSVGPaintingProperty::DoUpdate()
{
  nsSVGIDRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  if (mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    nsSVGUtils::InvalidateCoveredRegion(mFrame);
  } else {
    InvalidateAllContinuations(mFrame);
  }
}

static nsSVGRenderingObserver *
CreateFilterProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
{ return new nsSVGFilterProperty(aURI, aFrame, aReferenceImage); }

static nsSVGRenderingObserver *
CreateMarkerProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
{ return new nsSVGMarkerProperty(aURI, aFrame, aReferenceImage); }

static nsSVGRenderingObserver *
CreateTextPathProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
{ return new nsSVGTextPathProperty(aURI, aFrame, aReferenceImage); }

static nsSVGRenderingObserver *
CreatePaintingProperty(nsIURI *aURI, nsIFrame *aFrame, PRBool aReferenceImage)
{ return new nsSVGPaintingProperty(aURI, aFrame, aReferenceImage); }

static nsSVGRenderingObserver *
GetEffectProperty(nsIURI *aURI, nsIFrame *aFrame,
                  const FramePropertyDescriptor *aProperty,
                  nsSVGRenderingObserver * (* aCreate)(nsIURI *, nsIFrame *, PRBool))
{
  if (!aURI)
    return nsnull;

  FrameProperties props = aFrame->Properties();
  nsSVGRenderingObserver *prop =
    static_cast<nsSVGRenderingObserver*>(props.Get(aProperty));
  if (prop)
    return prop;
  prop = aCreate(aURI, aFrame, PR_FALSE);
  if (!prop)
    return nsnull;
  NS_ADDREF(prop);
  props.Set(aProperty, static_cast<nsISupports*>(prop));
  return prop;
}

nsSVGMarkerProperty *
nsSVGEffects::GetMarkerProperty(nsIURI *aURI, nsIFrame *aFrame,
                                const FramePropertyDescriptor *aProp)
{
  return static_cast<nsSVGMarkerProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreateMarkerProperty));
}

nsSVGTextPathProperty *
nsSVGEffects::GetTextPathProperty(nsIURI *aURI, nsIFrame *aFrame,
                                  const FramePropertyDescriptor *aProp)
{
  return static_cast<nsSVGTextPathProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreateTextPathProperty));
}

nsSVGPaintingProperty *
nsSVGEffects::GetPaintingProperty(nsIURI *aURI, nsIFrame *aFrame,
                                  const FramePropertyDescriptor *aProp)
{
  return static_cast<nsSVGPaintingProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreatePaintingProperty));
}

static nsSVGRenderingObserver *
GetEffectPropertyForURI(nsIURI *aURI, nsIFrame *aFrame,
                        const FramePropertyDescriptor *aProperty,
                        nsSVGRenderingObserver * (* aCreate)(nsIURI *, nsIFrame *, PRBool))
{
  FrameProperties props = aFrame->Properties();
  nsSVGEffects::URIObserverHashtable *hashtable =
    static_cast<nsSVGEffects::URIObserverHashtable*>(props.Get(aProperty));
  if (!hashtable) {
    hashtable = new nsSVGEffects::URIObserverHashtable();
    hashtable->Init();
    props.Set(aProperty, hashtable);
  }
  nsSVGRenderingObserver* prop =
    static_cast<nsSVGRenderingObserver*>(hashtable->GetWeak(aURI));
  if (!prop) {
    PRBool watchImage = aProperty == nsSVGEffects::BackgroundImageProperty();
    prop = aCreate(aURI, aFrame, watchImage);
    hashtable->Put(aURI, prop);
  }
  return prop;
}

nsSVGPaintingProperty *
nsSVGEffects::GetPaintingPropertyForURI(nsIURI *aURI, nsIFrame *aFrame,
                                        const FramePropertyDescriptor *aProp)
{
  return static_cast<nsSVGPaintingProperty*>(
          GetEffectPropertyForURI(aURI, aFrame, aProp, CreatePaintingProperty));
}

nsSVGEffects::EffectProperties
nsSVGEffects::GetEffectProperties(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  EffectProperties result;
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  result.mFilter = static_cast<nsSVGFilterProperty*>
    (GetEffectProperty(style->mFilter, aFrame, FilterProperty(),
                       CreateFilterProperty));
  result.mClipPath =
    GetPaintingProperty(style->mClipPath, aFrame, ClipPathProperty());
  result.mMask =
    GetPaintingProperty(style->mMask, aFrame, MaskProperty());
  return result;
}

nsSVGClipPathFrame *
nsSVGEffects::EffectProperties::GetClipPathFrame(PRBool *aOK)
{
  if (!mClipPath)
    return nsnull;
  nsSVGClipPathFrame *frame = static_cast<nsSVGClipPathFrame *>
    (mClipPath->GetReferencedFrame(nsGkAtoms::svgClipPathFrame, aOK));
  if (frame && aOK && *aOK) {
    *aOK = frame->IsValid();
  }
  return frame;
}

nsSVGMaskFrame *
nsSVGEffects::EffectProperties::GetMaskFrame(PRBool *aOK)
{
  if (!mMask)
    return nsnull;
  return static_cast<nsSVGMaskFrame *>
    (mMask->GetReferencedFrame(nsGkAtoms::svgMaskFrame, aOK));
}

void
nsSVGEffects::UpdateEffects(nsIFrame *aFrame)
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
  GetEffectProperty(aFrame->GetStyleSVGReset()->mFilter,
                    aFrame, FilterProperty(), CreateFilterProperty);

  if (aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    // Set marker properties here to avoid reference loops
    const nsStyleSVG *style = aFrame->GetStyleSVG();
    GetEffectProperty(style->mMarkerStart, aFrame, MarkerBeginProperty(),
                      CreateMarkerProperty);
    GetEffectProperty(style->mMarkerMid, aFrame, MarkerMiddleProperty(),
                      CreateMarkerProperty);
    GetEffectProperty(style->mMarkerEnd, aFrame, MarkerEndProperty(),
                      CreateMarkerProperty);
  }

  nsIFrame *kid = aFrame->GetFirstChild(nsnull);
  while (kid) {
    if (kid->GetContent()->IsElement()) {
      UpdateEffects(kid);
    }
    kid = kid->GetNextSibling();
  }
}

nsSVGFilterProperty *
nsSVGEffects::GetFilterProperty(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  if (!aFrame->GetStyleSVGReset()->mFilter)
    return nsnull;

  return static_cast<nsSVGFilterProperty *>
    (aFrame->Properties().Get(FilterProperty()));
}

static PLDHashOperator
GatherEnumerator(nsVoidPtrHashKey* aEntry, void* aArg)
{
  nsTArray<nsSVGRenderingObserver*>* array =
    static_cast<nsTArray<nsSVGRenderingObserver*>*>(aArg);
  array->AppendElement(static_cast<nsSVGRenderingObserver*>(
          const_cast<void*>(aEntry->GetKey())));
  return PL_DHASH_REMOVE;
}

void
nsSVGRenderingObserverList::InvalidateAll()
{
  if (mObservers.Count() == 0)
    return;

  nsAutoTArray<nsSVGRenderingObserver*,10> observers;

  // The PL_DHASH_REMOVE in GatherEnumerator drops all our observers here:
  mObservers.EnumerateEntries(GatherEnumerator, &observers);

  for (PRUint32 i = 0; i < observers.Length(); ++i) {
    observers[i]->InvalidateViaReferencedElement();
  }
}

static void
DestroyObservers(void *aObject, nsIAtom *aPropertyName,
                 void *aPropertyValue, void *aData)
{
  delete static_cast<nsSVGRenderingObserverList*>(aPropertyValue);
}

void
nsSVGEffects::AddRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver)
{
  nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
  if (!observerList) {
    observerList = new nsSVGRenderingObserverList();
    if (!observerList)
      return;
    aElement->SetProperty(nsGkAtoms::renderingobserverlist, observerList, DestroyObservers);
  }
  aElement->SetHasRenderingObservers(true);
  observerList->Add(aObserver);
}

void
nsSVGEffects::RemoveRenderingObserver(Element *aElement, nsSVGRenderingObserver *aObserver)
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
nsSVGEffects::InvalidateRenderingObservers(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  if (!aFrame->GetContent()->IsElement())
    return;

  nsSVGRenderingObserverList *observerList =
    GetObserverList(aFrame->GetContent()->AsElement());
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
nsSVGEffects::InvalidateDirectRenderingObservers(Element *aElement)
{
  if (aElement->HasRenderingObservers()) {
    nsSVGRenderingObserverList *observerList = GetObserverList(aElement);
    if (observerList) {
      observerList->InvalidateAll();
    }
  }
}

void
nsSVGEffects::InvalidateDirectRenderingObservers(nsIFrame *aFrame)
{
  if (aFrame->GetContent() && aFrame->GetContent()->IsElement()) {
    InvalidateDirectRenderingObservers(aFrame->GetContent()->AsElement());
  }
}
