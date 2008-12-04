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

NS_IMPL_ISUPPORTS1(nsSVGRenderingObserver, nsIMutationObserver)

nsSVGRenderingObserver::nsSVGRenderingObserver(nsIURI *aURI,
                                               nsIFrame *aFrame)
  : mElement(this), mFrame(aFrame),
    mFramePresShell(aFrame->PresContext()->PresShell()),
    mReferencedFrame(nsnull),
    mReferencedFramePresShell(nsnull)
{
  // Start watching the target element
  mElement.Reset(aFrame->GetContent(), aURI);
  if (mElement.get()) {
    mElement.get()->AddMutationObserver(this);
  }
}

nsSVGRenderingObserver::~nsSVGRenderingObserver()
{
  if (mElement.get()) {
    mElement.get()->RemoveMutationObserver(this);
  }
  if (mReferencedFrame && !mReferencedFramePresShell->IsDestroying()) {
    nsSVGEffects::RemoveRenderingObserver(mReferencedFrame, this);
  }
}

nsIFrame*
nsSVGRenderingObserver::GetReferencedFrame()
{
  if (mReferencedFrame && !mReferencedFramePresShell->IsDestroying()) {
    NS_ASSERTION(mElement.get() &&
                 static_cast<nsGenericElement*>(mElement.get())->GetPrimaryFrame() == mReferencedFrame,
                 "Cached frame is incorrect!");
    return mReferencedFrame;
  }

  if (mElement.get()) {
    nsIFrame *frame =
      static_cast<nsGenericElement*>(mElement.get())->GetPrimaryFrame();
    if (frame) {
      mReferencedFrame = frame;
      mReferencedFramePresShell = mReferencedFrame->PresContext()->PresShell();
      nsSVGEffects::AddRenderingObserver(mReferencedFrame, this);
      return frame;
    }
  }
  return nsnull;
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
nsSVGRenderingObserver::DoUpdate()
{
  if (mFramePresShell->IsDestroying()) {
    // mFrame is no longer valid. Bail out.
    mFrame = nsnull;
    return;
  }
  if (mReferencedFrame) {
    nsSVGEffects::RemoveRenderingObserver(mReferencedFrame, this);
    mReferencedFrame = nsnull;
    mReferencedFramePresShell = nsnull;
  }
  if (mFrame && mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    // Changes should propagate out to things that might be observing
    // the referencing frame or its ancestors.
    nsSVGEffects::InvalidateRenderingObservers(mFrame);
  }
}

void
nsSVGRenderingObserver::InvalidateViaReferencedFrame()
{
  // Clear mReferencedFrame since the referenced frame has already
  // dropped its reference back to us
  mReferencedFrame = nsnull;
  mReferencedFramePresShell = nsnull;
  DoUpdate();
}

void
nsSVGRenderingObserver::AttributeChanged(nsIDocument *aDocument,
                                         nsIContent *aContent,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom *aAttribute,
                                         PRInt32 aModType,
                                         PRUint32 aStateMask)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentAppended(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        PRInt32 aNewIndexInContainer)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentInserted(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        nsIContent *aChild,
                                        PRInt32 aIndexInContainer)
{
  DoUpdate();
}

void
nsSVGRenderingObserver::ContentRemoved(nsIDocument *aDocument,
                                       nsIContent *aContainer,
                                       nsIContent *aChild,
                                       PRInt32 aIndexInContainer)
{
  DoUpdate();
}

NS_IMPL_ISUPPORTS_INHERITED1(nsSVGFilterProperty,
                             nsSVGRenderingObserver,
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
  nsSVGRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  // Repaint asynchronously in case the filter frame is being torn down
  nsChangeHint changeHint =
    nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_UpdateEffects);

  if (!mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    NS_UpdateHint(changeHint, nsChangeHint_ReflowFrame);
  }
  mFramePresShell->FrameConstructor()->PostRestyleEvent(
    mFrame->GetContent(), nsReStyleHint(0), changeHint);
}

void
nsSVGMarkerProperty::DoUpdate()
{
  nsSVGRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  if (mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    if (!(mFrame->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
      nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(mFrame);
      if (outerSVGFrame) {
        // marker changes can change the covered region
        outerSVGFrame->UpdateAndInvalidateCoveredRegion(mFrame);
      }
    }
  } else {
    InvalidateAllContinuations(mFrame);
  }
}

void
nsSVGTextPathProperty::DoUpdate()
{
  nsSVGRenderingObserver::DoUpdate();
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
  nsSVGRenderingObserver::DoUpdate();
  if (!mFrame)
    return;

  if (mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    nsSVGUtils::InvalidateCoveredRegion(mFrame);
  } else {
    InvalidateAllContinuations(mFrame);
  }
}

static nsSVGRenderingObserver *
CreateFilterProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGFilterProperty(aURI, aFrame); }

static nsSVGRenderingObserver *
CreateMarkerProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGMarkerProperty(aURI, aFrame); }

static nsSVGRenderingObserver *
CreateTextPathProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGTextPathProperty(aURI, aFrame); }

static nsSVGRenderingObserver *
CreatePaintingProperty(nsIURI *aURI, nsIFrame *aFrame)
{ return new nsSVGPaintingProperty(aURI, aFrame); }

static nsSVGRenderingObserver *
GetEffectProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp,
                  nsSVGRenderingObserver * (* aCreate)(nsIURI *, nsIFrame *))
{
  if (!aURI)
    return nsnull;
  nsSVGRenderingObserver *prop =
    static_cast<nsSVGRenderingObserver*>(aFrame->GetProperty(aProp));
  if (prop)
    return prop;
  prop = aCreate(aURI, aFrame);
  if (!prop)
    return nsnull;
  NS_ADDREF(prop);
  aFrame->SetProperty(aProp,
                      static_cast<nsISupports*>(prop),
                      nsPropertyTable::SupportsDtorFunc);
  return prop;
}

nsSVGMarkerProperty *
nsSVGEffects::GetMarkerProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp)
{
  return static_cast<nsSVGMarkerProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreateMarkerProperty));
}

nsSVGTextPathProperty *
nsSVGEffects::GetTextPathProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp)
{
  return static_cast<nsSVGTextPathProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreateTextPathProperty));
}

nsSVGPaintingProperty *
nsSVGEffects::GetPaintingProperty(nsIURI *aURI, nsIFrame *aFrame, nsIAtom *aProp)
{
  return static_cast<nsSVGPaintingProperty*>(
          GetEffectProperty(aURI, aFrame, aProp, CreatePaintingProperty));
}

nsSVGEffects::EffectProperties
nsSVGEffects::GetEffectProperties(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  EffectProperties result;
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  result.mFilter = static_cast<nsSVGFilterProperty*>
    (GetEffectProperty(style->mFilter, aFrame, nsGkAtoms::filter, CreateFilterProperty));
  result.mClipPath = GetPaintingProperty(style->mClipPath, aFrame, nsGkAtoms::clipPath);
  result.mMask = GetPaintingProperty(style->mMask, aFrame, nsGkAtoms::mask);
  return result;
}

nsSVGClipPathFrame *
nsSVGEffects::EffectProperties::GetClipPathFrame(PRBool *aOK)
{
  if (!mClipPath)
    return nsnull;
  return static_cast<nsSVGClipPathFrame *>
    (mClipPath->GetReferencedFrame(nsGkAtoms::svgClipPathFrame, aOK));
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
  aFrame->DeleteProperty(nsGkAtoms::filter);
  aFrame->DeleteProperty(nsGkAtoms::mask);
  aFrame->DeleteProperty(nsGkAtoms::clipPath);

  aFrame->DeleteProperty(nsGkAtoms::marker_start);
  aFrame->DeleteProperty(nsGkAtoms::marker_mid);
  aFrame->DeleteProperty(nsGkAtoms::marker_end);

  aFrame->DeleteProperty(nsGkAtoms::stroke);
  aFrame->DeleteProperty(nsGkAtoms::fill);

  // Ensure that the filter is repainted correctly
  // We can't do that in DoUpdate as the referenced frame may not be valid
  const nsStyleSVGReset *style = aFrame->GetStyleSVGReset();
  if (style->mFilter) {
    GetEffectProperty(style->mFilter, aFrame, nsGkAtoms::filter, CreateFilterProperty);
  }
}

nsSVGFilterProperty *
nsSVGEffects::GetFilterProperty(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame should be first continuation");

  if (!aFrame->GetStyleSVGReset()->mFilter)
    return nsnull;

  return static_cast<nsSVGFilterProperty *>(aFrame->GetProperty(nsGkAtoms::filter));
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
  mObservers.EnumerateEntries(GatherEnumerator, &observers);
  for (PRUint32 i = 0; i < observers.Length(); ++i) {
    observers[i]->InvalidateViaReferencedFrame();
  }
}

static nsSVGRenderingObserverList *
GetObserverList(nsIFrame *aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_MAY_BE_TRANSFORMED_OR_HAVE_RENDERING_OBSERVERS))
    return nsnull;
  return static_cast<nsSVGRenderingObserverList*>(aFrame->GetProperty(nsGkAtoms::observer));
}

void
nsSVGEffects::AddRenderingObserver(nsIFrame *aFrame, nsSVGRenderingObserver *aObserver)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsSVGRenderingObserverList *observerList = GetObserverList(aFrame);
  if (!observerList) {
    observerList = new nsSVGRenderingObserverList();
    if (!observerList)
      return;
    for (nsIFrame* f = aFrame; f; f = f->GetNextContinuation()) {
      f->AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED_OR_HAVE_RENDERING_OBSERVERS);
    }
    aFrame->SetProperty(nsGkAtoms::observer, observerList);
  }
  observerList->Add(aObserver);
}

void
nsSVGEffects::RemoveRenderingObserver(nsIFrame *aFrame, nsSVGRenderingObserver *aObserver)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsSVGRenderingObserverList *observerList = GetObserverList(aFrame);
  if (observerList) {
    observerList->Remove(aObserver);
    // Don't remove the property even if the observer list is empty.
    // This might not be a good time to modify the frame property
    // hashtables.
  }
}

void
nsSVGEffects::InvalidateRenderingObservers(nsIFrame *aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(), "aFrame must be first continuation");

  nsSVGRenderingObserverList *observerList = GetObserverList(aFrame);
  if (observerList) {
    observerList->InvalidateAll();
    return;
  }

  // Check ancestor SVG containers. The root frame cannot be of type
  // eSVGContainer so we don't have to check f for null here.
  for (nsIFrame *f = aFrame->GetParent();
       f->IsFrameOfType(nsIFrame::eSVGContainer); f = f->GetParent()) {
    observerList = GetObserverList(f);
    if (observerList) {
      observerList->InvalidateAll();
      return;
    }
  }
}

void
nsSVGEffects::InvalidateDirectRenderingObservers(nsIFrame *aFrame)
{
  nsSVGRenderingObserverList *observerList = GetObserverList(aFrame);
  if (observerList) {
    observerList->InvalidateAll();
  }
}
