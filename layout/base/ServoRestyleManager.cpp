/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"

#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsBlockFrame.h"
#include "nsBulletFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsPrintfCString.h"
#include "nsRefreshDriver.h"
#include "nsStyleChangeList.h"

using namespace mozilla::dom;

namespace mozilla {

ServoRestyleManager::ServoRestyleManager(nsPresContext* aPresContext)
  : RestyleManager(StyleBackendType::Servo, aPresContext)
  , mReentrantChanges(nullptr)
{
}

void
ServoRestyleManager::PostRestyleEvent(Element* aElement,
                                      nsRestyleHint aRestyleHint,
                                      nsChangeHint aMinChangeHint)
{
  MOZ_ASSERT(!(aMinChangeHint & nsChangeHint_NeutralChange),
             "Didn't expect explicit change hints to be neutral!");
  if (MOZ_UNLIKELY(IsDisconnected()) ||
      MOZ_UNLIKELY(PresContext()->PresShell()->IsDestroying())) {
    return;
  }

  // We allow posting restyles from within change hint handling, but not from
  // within the restyle algorithm itself.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

  if (aRestyleHint == 0 && !aMinChangeHint) {
    return; // Nothing to do.
  }

  // Processing change hints sometimes causes new change hints to be generated,
  // and very occasionally, additional restyle hints. We collect the change
  // hints manually to avoid re-traversing the DOM to find them.
  if (mReentrantChanges && !aRestyleHint) {
    mReentrantChanges->AppendElement(ReentrantChange { aElement, aMinChangeHint });
    return;
  }

  if (aRestyleHint & ~eRestyle_AllHintsWithAnimations) {
    mHaveNonAnimationRestyles = true;
  }

  Servo_NoteExplicitHints(aElement, aRestyleHint, aMinChangeHint);
}

void
ServoRestyleManager::PostRestyleEventForCSSRuleChanges()
{
  mRestyleForCSSRuleChanges = true;
  mPresContext->PresShell()->EnsureStyleFlush();
}

/* static */ void
ServoRestyleManager::PostRestyleEventForAnimations(Element* aElement,
                                                   nsRestyleHint aRestyleHint)
{
  Servo_NoteExplicitHints(aElement, aRestyleHint, nsChangeHint(0));
}

void
ServoRestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                         nsRestyleHint aRestyleHint)
{
   // NOTE(emilio): GeckoRestlyeManager does a sync style flush, which seems not
   // to be needed in my testing.
  PostRebuildAllStyleDataEvent(aExtraHint, aRestyleHint);
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  StyleSet()->ClearDataAndMarkDeviceDirty();

  if (Element* root = mPresContext->Document()->GetRootElement()) {
    PostRestyleEvent(root, aRestyleHint, aExtraHint);
  }

  // TODO(emilio, bz): Extensions can add/remove stylesheets that can affect
  // non-inheriting anon boxes. It's not clear if we want to support that, but
  // if we do, we need to re-selector-match them here.
}

/* static */ void
ServoRestyleManager::ClearServoDataFromSubtree(Element* aElement)
{
  if (!aElement->HasServoData()) {
    MOZ_ASSERT(!aElement->HasDirtyDescendantsForServo());
    return;
  }

  StyleChildrenIterator it(aElement);
  for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
    if (n->IsElement()) {
      ClearServoDataFromSubtree(n->AsElement());
    }
  }

  aElement->ClearServoData();
  aElement->UnsetHasDirtyDescendantsForServo();
}


/* static */ void
ServoRestyleManager::ClearRestyleStateFromSubtree(Element* aElement)
{
  if (aElement->HasDirtyDescendantsForServo()) {
    StyleChildrenIterator it(aElement);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      if (n->IsElement()) {
        ClearRestyleStateFromSubtree(n->AsElement());
      }
    }
  }

  Unused << Servo_TakeChangeHint(aElement);
  aElement->UnsetHasDirtyDescendantsForServo();
  aElement->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES);
}

/**
 * This struct takes care of encapsulating some common state that text nodes may
 * need to track during the post-traversal.
 *
 * This is currently used to properly compute change hints when the parent
 * element of this node is a display: contents node, and also to avoid computing
 * the style for text children more than once per element.
 */
struct ServoRestyleManager::TextPostTraversalState
{
  nsStyleContext& mParentContext;
  ServoStyleSet& mStyleSet;
  RefPtr<nsStyleContext> mStyle;
  bool mShouldPostHints;
  bool mShouldComputeHints;
  nsChangeHint mComputedHint;

  TextPostTraversalState(nsStyleContext& aParentContext,
                         ServoStyleSet& aStyleSet,
                         bool aDisplayContentsParentStyleChanged)
    : mParentContext(aParentContext)
    , mStyleSet(aStyleSet)
    , mStyle(nullptr)
    , mShouldPostHints(aDisplayContentsParentStyleChanged)
    , mShouldComputeHints(aDisplayContentsParentStyleChanged)
    , mComputedHint(nsChangeHint_Empty)
  {}

  nsStyleContext& ComputeStyle(nsIContent* aTextNode)
  {
    if (!mStyle) {
      mStyle = mStyleSet.ResolveStyleForText(aTextNode, &mParentContext);
    }
    MOZ_ASSERT(mStyle);
    return *mStyle;
  }

  void ComputeHintIfNeeded(nsIContent* aContent,
                           nsIFrame* aTextFrame,
                           nsStyleContext& aNewContext,
                           nsStyleChangeList& aChangeList)
  {
    MOZ_ASSERT(aTextFrame);
    MOZ_ASSERT(aNewContext.GetPseudo() == nsCSSAnonBoxes::mozText);

    if (MOZ_LIKELY(!mShouldPostHints)) {
      return;
    }

    nsStyleContext* oldContext = aTextFrame->StyleContext();
    MOZ_ASSERT(oldContext->GetPseudo() == nsCSSAnonBoxes::mozText);

    // We rely on the fact that all the text children for the same element share
    // style to avoid recomputing style differences for all of them.
    //
    // TODO(emilio): The above may not be true for ::first-{line,letter}, but
    // we'll cross that bridge when we support those in stylo.
    if (mShouldComputeHints) {
      mShouldComputeHints = false;
      uint32_t equalStructs, samePointerStructs;
      mComputedHint =
        oldContext->CalcStyleDifference(&aNewContext,
                                        &equalStructs,
                                        &samePointerStructs);
    }

    if (mComputedHint) {
      aChangeList.AppendChange(aTextFrame, aContent, mComputedHint);
    }
  }
};

static void
UpdateBlockFramePseudoElements(nsBlockFrame* aFrame,
                               ServoStyleSet& aStyleSet,
                               nsStyleChangeList& aChangeList)
{
  if (nsBulletFrame* bullet = aFrame->GetBullet()) {
    RefPtr<nsStyleContext> newContext =
      aStyleSet.ResolvePseudoElementStyle(
          aFrame->GetContent()->AsElement(),
          bullet->StyleContext()->GetPseudoType(),
          aFrame->StyleContext(),
          /* aPseudoElement = */ nullptr);

    aFrame->UpdateStyleOfOwnedChildFrame(bullet, newContext, aChangeList);
  }
}

static void
UpdateBackdropIfNeeded(nsIFrame* aFrame,
                       ServoStyleSet& aStyleSet,
                       nsStyleChangeList& aChangeList)
{
  const nsStyleDisplay* display = aFrame->StyleContext()->StyleDisplay();
  if (display->mTopLayer != NS_STYLE_TOP_LAYER_TOP) {
    return;
  }

  // Elements in the top layer are guaranteed to have absolute or fixed
  // position per https://fullscreen.spec.whatwg.org/#new-stacking-layer.
  MOZ_ASSERT(display->IsAbsolutelyPositionedStyle());

  nsIFrame* backdropPlaceholder =
    aFrame->GetChildList(nsIFrame::kBackdropList).FirstChild();
  if (!backdropPlaceholder) {
    return;
  }

  MOZ_ASSERT(backdropPlaceholder->IsPlaceholderFrame());
  nsIFrame* backdropFrame =
    nsPlaceholderFrame::GetRealFrameForPlaceholder(backdropPlaceholder);
  MOZ_ASSERT(backdropFrame->IsBackdropFrame());
  MOZ_ASSERT(backdropFrame->StyleContext()->GetPseudoType() ==
             CSSPseudoElementType::backdrop);

  RefPtr<nsStyleContext> newContext =
    aStyleSet.ResolvePseudoElementStyle(
        aFrame->GetContent()->AsElement(),
        CSSPseudoElementType::backdrop,
        aFrame->StyleContext(),
        /* aPseudoElement = */ nullptr);

  aFrame->UpdateStyleOfOwnedChildFrame(backdropFrame,
                                       newContext,
                                       aChangeList);
}

static void
UpdateFramePseudoElementStyles(nsIFrame* aFrame,
                               ServoStyleSet& aStyleSet,
                               nsStyleChangeList& aChangeList)
{
  if (aFrame->IsFrameOfType(nsIFrame::eBlockFrame)) {
    UpdateBlockFramePseudoElements(static_cast<nsBlockFrame*>(aFrame),
                                   aStyleSet,
                                   aChangeList);
  }

  UpdateBackdropIfNeeded(aFrame, aStyleSet, aChangeList);
}

bool
ServoRestyleManager::ProcessPostTraversal(Element* aElement,
                                          nsStyleContext* aParentContext,
                                          ServoStyleSet* aStyleSet,
                                          nsStyleChangeList& aChangeList)
{
  nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(aElement);

  // Grab the change hint from Servo.
  nsChangeHint changeHint = Servo_TakeChangeHint(aElement);

  // Handle lazy frame construction by posting a reconstruct for any lazily-
  // constructed roots.
  if (aElement->HasFlag(NODE_NEEDS_FRAME)) {
    changeHint |= nsChangeHint_ReconstructFrame;
    // The only time the primary frame is non-null is when image maps do hacky
    // SetPrimaryFrame calls.
    MOZ_ASSERT(!styleFrame || styleFrame->IsImageFrame());
    styleFrame = nullptr;
  }

  // Although we shouldn't generate non-ReconstructFrame hints for elements with
  // no frames, we can still get them here if they were explicitly posted by
  // PostRestyleEvent, such as a RepaintFrame hint when a :link changes to be
  // :visited.  Skip processing these hints if there is no frame.
  if ((styleFrame || (changeHint & nsChangeHint_ReconstructFrame)) && changeHint) {
    aChangeList.AppendChange(styleFrame, aElement, changeHint);
  }

  // If our change hint is reconstruct, we delegate to the frame constructor,
  // which consumes the new style and expects the old style to be on the frame.
  //
  // XXXbholley: We should teach the frame constructor how to clear the dirty
  // descendants bit to avoid the traversal here.
  if (changeHint & nsChangeHint_ReconstructFrame) {
    ClearRestyleStateFromSubtree(aElement);
    return true;
  }

  // TODO(emilio): We could avoid some refcount traffic here, specially in the
  // ServoComputedValues case, which uses atomic refcounting.
  //
  // Hold the old style context alive, because it could become a dangling
  // pointer during the replacement. In practice it's not a huge deal (on
  // GetNextContinuationWithSameStyle the pointer is not dereferenced, only
  // compared), but better not playing with dangling pointers if not needed.
  RefPtr<nsStyleContext> oldStyleContext =
    styleFrame ? styleFrame->StyleContext() : nullptr;

  UndisplayedNode* displayContentsNode = nullptr;
  // FIXME(emilio, bug 1303605): This can be simpler for Servo.
  // Note that we intentionally don't check for display: none content.
  if (!oldStyleContext) {
    displayContentsNode =
      PresContext()->FrameConstructor()->GetDisplayContentsNodeFor(aElement);
    if (displayContentsNode) {
      oldStyleContext = displayContentsNode->mStyle;
    }
  }

  RefPtr<ServoComputedValues> computedValues =
    aStyleSet->ResolveServoStyle(aElement);

  // Note that we rely in the fact that we don't cascade pseudo-element styles
  // separately right now (that is, if a pseudo style changes, the normal style
  // changes too).
  //
  // Otherwise we should probably encode that information somehow to avoid
  // expensive checks in the common case.
  //
  // Also, we're going to need to check for pseudos of display: contents
  // elements, though that is buggy right now even in non-stylo mode, see
  // bug 1251799.
  const bool recreateContext = oldStyleContext &&
    oldStyleContext->StyleSource().AsServoComputedValues() != computedValues;

  RefPtr<nsStyleContext> newContext = nullptr;
  if (recreateContext) {
    MOZ_ASSERT(styleFrame || displayContentsNode);

    auto pseudo = aElement->GetPseudoElementType();
    nsIAtom* pseudoTag = pseudo == CSSPseudoElementType::NotPseudo
      ? nullptr : nsCSSPseudoElements::GetPseudoAtom(pseudo);

    newContext =
      aStyleSet->GetContext(computedValues.forget(),
                            aParentContext,
                            pseudoTag,
                            pseudo,
                            aElement);

    newContext->EnsureSameStructsCached(oldStyleContext);

    // XXX This could not always work as expected: there are kinds of content
    // with the first split and the last sharing style, but others not. We
    // should handle those properly.
    // XXXbz I think the UpdateStyleOfOwnedAnonBoxes call below handles _that_
    // right, but not other cases where we happen to have different styles on
    // different continuations... (e.g. first-line).
    for (nsIFrame* f = styleFrame; f;
         f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
      f->SetStyleContext(newContext);
    }

    if (MOZ_UNLIKELY(displayContentsNode)) {
      MOZ_ASSERT(!styleFrame);
      displayContentsNode->mStyle = newContext;
    }

    if (styleFrame) {
      styleFrame->UpdateStyleOfOwnedAnonBoxes(*aStyleSet, aChangeList, changeHint);
      UpdateFramePseudoElementStyles(styleFrame, *aStyleSet, aChangeList);
    }

    // Some changes to animations don't affect the computed style and yet still
    // require the layer to be updated. For example, pausing an animation via
    // the Web Animations API won't affect an element's style but still
    // requires to update the animation on the layer.
    //
    // We can sometimes reach this when the animated style is being removed.
    // Since AddLayerChangesForAnimation checks if |styleFrame| has a transform
    // style or not, we need to call it *after* setting |newContext| to
    // |styleFrame| to ensure the animated transform has been removed first.
    AddLayerChangesForAnimation(styleFrame, aElement, aChangeList);
  }

  const bool descendantsNeedFrames =
    aElement->HasFlag(NODE_DESCENDANTS_NEED_FRAMES);
  const bool traverseElementChildren =
    aElement->HasDirtyDescendantsForServo() || descendantsNeedFrames;
  const bool traverseTextChildren = recreateContext || descendantsNeedFrames;
  bool recreatedAnyContext = recreateContext;
  if (traverseElementChildren || traverseTextChildren) {
    nsStyleContext* upToDateContext =
      recreateContext ? newContext : oldStyleContext;

    StyleChildrenIterator it(aElement);
    TextPostTraversalState textState(
        *upToDateContext, *aStyleSet, displayContentsNode && recreateContext);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      if (traverseElementChildren && n->IsElement()) {
        recreatedAnyContext |=
          ProcessPostTraversal(n->AsElement(), upToDateContext,
                               aStyleSet, aChangeList);
      } else if (traverseTextChildren && n->IsNodeOfType(nsINode::eTEXT)) {
        recreatedAnyContext |=
          ProcessPostTraversalForText(n, aChangeList, textState);
      }
    }
  }

  aElement->UnsetHasDirtyDescendantsForServo();
  aElement->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES);
  return recreatedAnyContext;
}

bool
ServoRestyleManager::ProcessPostTraversalForText(
    nsIContent* aTextNode,
    nsStyleChangeList& aChangeList,
    TextPostTraversalState& aPostTraversalState)
{
  // Handle lazy frame construction.
  if (aTextNode->HasFlag(NODE_NEEDS_FRAME)) {
    aChangeList.AppendChange(nullptr, aTextNode, nsChangeHint_ReconstructFrame);
    return true;
  }

  // Handle restyle.
  nsIFrame* primaryFrame = aTextNode->GetPrimaryFrame();
  if (!primaryFrame) {
    return false;
  }

  RefPtr<nsStyleContext> oldStyleContext = primaryFrame->StyleContext();
  nsStyleContext& newContext = aPostTraversalState.ComputeStyle(aTextNode);
  aPostTraversalState.ComputeHintIfNeeded(
      aTextNode, primaryFrame, newContext, aChangeList);

  for (nsIFrame* f = primaryFrame; f;
       f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
    f->SetStyleContext(&newContext);
  }

  return true;
}

void
ServoRestyleManager::ClearSnapshots()
{
  for (auto iter = mSnapshots.Iter(); !iter.Done(); iter.Next()) {
    iter.Key()->UnsetFlags(ELEMENT_HAS_SNAPSHOT | ELEMENT_HANDLED_SNAPSHOT);
    iter.Remove();
  }
}

ServoElementSnapshot&
ServoRestyleManager::SnapshotFor(Element* aElement)
{
  MOZ_ASSERT(!mInStyleRefresh);

  // NOTE(emilio): We can handle snapshots from a one-off restyle of those that
  // we do to restyle stuff for reconstruction, for example.
  //
  // It seems to be the case that we always flush in between that happens and
  // the next attribute change, so we can assert that we haven't handled the
  // snapshot here yet. If this assertion didn't hold, we'd need to unset that
  // flag from here too.
  //
  // Can't wait to make ProcessPendingRestyles the only entry-point for styling,
  // so this becomes much easier to reason about. Today is not that day though.
  MOZ_ASSERT(aElement->HasServoData());
  MOZ_ASSERT(!aElement->HasFlag(ELEMENT_HANDLED_SNAPSHOT));

  ServoElementSnapshot* snapshot = mSnapshots.LookupOrAdd(aElement, aElement);
  aElement->SetFlags(ELEMENT_HAS_SNAPSHOT);

  nsIPresShell* presShell = mPresContext->PresShell();
  presShell->EnsureStyleFlush();

  return *snapshot;
}

/* static */ nsIFrame*
ServoRestyleManager::FrameForPseudoElement(const nsIContent* aContent,
                                           nsIAtom* aPseudoTagOrNull)
{
  MOZ_ASSERT(!aPseudoTagOrNull || aContent->IsElement());
  if (!aPseudoTagOrNull) {
    return aContent->GetPrimaryFrame();
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::before) {
    return nsLayoutUtils::GetBeforeFrame(aContent);
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::after) {
    return nsLayoutUtils::GetAfterFrame(aContent);
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::firstLine ||
      aPseudoTagOrNull == nsCSSPseudoElements::firstLetter) {
    // TODO(emilio, bz): Figure out the best way to diff these styles.
    return nullptr;
  }

  MOZ_CRASH("Unkown pseudo-element given to "
            "ServoRestyleManager::FrameForPseudoElement");
  return nullptr;
}

void
ServoRestyleManager::DoProcessPendingRestyles(TraversalRestyleBehavior
                                                aRestyleBehavior)
{
  MOZ_ASSERT(PresContext()->Document(), "No document?  Pshaw!");
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(), "Missing a script blocker!");
  MOZ_ASSERT(!mInStyleRefresh, "Reentrant call?");

  if (MOZ_UNLIKELY(!PresContext()->PresShell()->DidInitialize())) {
    // PresShell::FlushPendingNotifications doesn't early-return in the case
    // where the PreShell hasn't yet been initialized (and therefore we haven't
    // yet done the initial style traversal of the DOM tree). We should arguably
    // fix up the callers and assert against this case, but we just detect and
    // handle it for now.
    return;
  }

  // Create a AnimationsWithDestroyedFrame during restyling process to
  // stop animations and transitions on elements that have no frame at the end
  // of the restyling process.
  AnimationsWithDestroyedFrame animationsWithDestroyedFrame(this);

  ServoStyleSet* styleSet = StyleSet();
  nsIDocument* doc = PresContext()->Document();
  bool animationOnly = aRestyleBehavior ==
                         TraversalRestyleBehavior::ForAnimationOnly;

  // Ensure the refresh driver is active during traversal to avoid mutating
  // mActiveTimer and mMostRecentRefresh time.
  PresContext()->RefreshDriver()->MostRecentRefresh();


  // Perform the Servo traversal, and the post-traversal if required. We do this
  // in a loop because certain rare paths in the frame constructor (like
  // uninstalling XBL bindings) can trigger additional style validations.
  mInStyleRefresh = true;
  if (mHaveNonAnimationRestyles && !animationOnly) {
    ++mAnimationGeneration;
  }

  TraversalRestyleBehavior restyleBehavior = mRestyleForCSSRuleChanges
    ? TraversalRestyleBehavior::ForCSSRuleChanges
    : TraversalRestyleBehavior::Normal;
  while (animationOnly ? styleSet->StyleDocumentForAnimationOnly()
                       : styleSet->StyleDocument(restyleBehavior)) {
    if (!animationOnly) {
      ClearSnapshots();
    }

    // Recreate style contexts, and queue up change hints (which also handle
    // lazy frame construction).
    nsStyleChangeList currentChanges(StyleBackendType::Servo);
    DocumentStyleRootIterator iter(doc);
    bool anyStyleChanged = false;
    while (Element* root = iter.GetNextStyleRoot()) {
      anyStyleChanged |=
        ProcessPostTraversal(root, nullptr, styleSet, currentChanges);
    }

    // Process the change hints.
    //
    // Unfortunately, the frame constructor can generate new change hints while
    // processing existing ones. We redirect those into a secondary queue and
    // iterate until there's nothing left.
    ReentrantChangeList newChanges;
    mReentrantChanges = &newChanges;
    while (!currentChanges.IsEmpty()) {
      ProcessRestyledFrames(currentChanges);
      MOZ_ASSERT(currentChanges.IsEmpty());
      for (ReentrantChange& change: newChanges)  {
        if (!(change.mHint & nsChangeHint_ReconstructFrame) &&
            !change.mContent->GetPrimaryFrame()) {
          // SVG Elements post change hints without ensuring that the primary
          // frame will be there after that (see bug 1366142).
          //
          // Just ignore those, since we can't really process them.
          continue;
        }
        currentChanges.AppendChange(change.mContent->GetPrimaryFrame(),
                                    change.mContent, change.mHint);
      }
      newChanges.Clear();
    }
    mReentrantChanges = nullptr;


    if (anyStyleChanged) {
      // Maybe no styles changed when:
      //
      //  * Only explicit change hints were posted in the first place.
      //  * When an attribute or state change in the content happens not to need
      //    a restyle after all.
      //
      // In any case, we don't need to increment the restyle generation in that
      // case.
      IncrementRestyleGeneration();
    }
  }

  FlushOverflowChangedTracker();

  if (!animationOnly) {
    ClearSnapshots();
    styleSet->AssertTreeIsClean();
    mHaveNonAnimationRestyles = false;
  }
  mRestyleForCSSRuleChanges = false;
  mInStyleRefresh = false;

  // Note: We are in the scope of |animationsWithDestroyedFrame|, so
  //       |mAnimationsWithDestroyedFrame| is still valid.
  MOZ_ASSERT(mAnimationsWithDestroyedFrame);
  mAnimationsWithDestroyedFrame->StopAnimationsForElementsWithoutFrames();
}

void
ServoRestyleManager::ProcessPendingRestyles()
{
  DoProcessPendingRestyles(TraversalRestyleBehavior::Normal);
}

void
ServoRestyleManager::UpdateOnlyAnimationStyles()
{
  // Bug 1365855: We also need to implement this for SMIL.
  bool doCSS = PresContext()->EffectCompositor()->HasPendingStyleUpdates();
  if (!doCSS) {
    return;
  }

  DoProcessPendingRestyles(TraversalRestyleBehavior::ForAnimationOnly);
}

void
ServoRestyleManager::RestyleForInsertOrChange(nsINode* aContainer,
                                              nsIContent* aChild)
{
  //
  // XXXbholley: We need the Gecko logic here to correctly restyle for things
  // like :empty and positional selectors (though we may not need to post
  // restyle events as agressively as the Gecko path does).
  //
  // Bug 1297899 tracks this work.
  //
}

void
ServoRestyleManager::RestyleForAppend(nsIContent* aContainer,
                                      nsIContent* aFirstNewContent)
{
  //
  // XXXbholley: We need the Gecko logic here to correctly restyle for things
  // like :empty and positional selectors (though we may not need to post
  // restyle events as agressively as the Gecko path does).
  //
  // Bug 1297899 tracks this work.
  //
}

void
ServoRestyleManager::ContentRemoved(nsINode* aContainer,
                                    nsIContent* aOldChild,
                                    nsIContent* aFollowingSibling)
{
  NS_WARNING("stylo: ServoRestyleManager::ContentRemoved not implemented");
}

void
ServoRestyleManager::ContentStateChanged(nsIContent* aContent,
                                         EventStates aChangedBits)
{
  MOZ_ASSERT(!mInStyleRefresh);

  if (!aContent->IsElement()) {
    return;
  }

  Element* aElement = aContent->AsElement();
  nsChangeHint changeHint;
  nsRestyleHint restyleHint;

  if (!aElement->HasServoData()) {
    return;
  }

  // NOTE: restyleHint here is effectively always 0, since that's what
  // ServoStyleSet::HasStateDependentStyle returns. Servo computes on
  // ProcessPendingRestyles using the ElementSnapshot, but in theory could
  // compute it sequentially easily.
  //
  // Determine what's the best way to do it, and how much work do we save
  // processing the restyle hint early (i.e., computing the style hint here
  // sequentially, potentially saving the snapshot), vs lazily (snapshot
  // approach).
  //
  // If we take the sequential approach we need to specialize Servo's restyle
  // hints system a bit more, and mesure whether we save something storing the
  // restyle hint in the table and deferring the dirtiness setting until
  // ProcessPendingRestyles (that's a requirement if we store snapshots though),
  // vs processing the restyle hint in-place, dirtying the nodes on
  // PostRestyleEvent.
  //
  // If we definitely take the snapshot approach, we should take rid of
  // HasStateDependentStyle, etc (though right now they're no-ops).
  ContentStateChangedInternal(aElement, aChangedBits, &changeHint,
                              &restyleHint);

  ServoElementSnapshot& snapshot = SnapshotFor(aElement);
  EventStates previousState = aElement->StyleState() ^ aChangedBits;
  snapshot.AddState(previousState);

  if (Element* parent = aElement->GetFlattenedTreeParentElementForStyle()) {
    parent->NoteDirtyDescendantsForServo();
  }
  PostRestyleEvent(aElement, restyleHint, changeHint);
}

static inline bool
AttributeInfluencesOtherPseudoClassState(Element* aElement, nsIAtom* aAttribute)
{
  // We must record some state for :-moz-browser-frame and
  // :-moz-table-border-nonzero.
  return (aAttribute == nsGkAtoms::mozbrowser &&
          aElement->IsAnyOfHTMLElements(nsGkAtoms::iframe, nsGkAtoms::frame)) ||
         (aAttribute == nsGkAtoms::border &&
          aElement->IsHTMLElement(nsGkAtoms::table));
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute, int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  MOZ_ASSERT(!mInStyleRefresh);

  if (!aElement->HasServoData()) {
    return;
  }

  ServoElementSnapshot& snapshot = SnapshotFor(aElement);
  snapshot.AddAttrs(aElement);

  if (AttributeInfluencesOtherPseudoClassState(aElement, aAttribute)) {
    snapshot.AddOtherPseudoClassState(aElement);
  }

  if (Element* parent = aElement->GetFlattenedTreeParentElementForStyle()) {
    parent->NoteDirtyDescendantsForServo();
  }
}

void
ServoRestyleManager::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                      nsIAtom* aAttribute, int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  MOZ_ASSERT(!mInStyleRefresh);
  MOZ_ASSERT(!mSnapshots.Get(aElement) || mSnapshots.Get(aElement)->HasAttrs());

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  if (primaryFrame) {
    primaryFrame->AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }

  nsChangeHint hint = aElement->GetAttributeChangeHint(aAttribute, aModType);
  if (hint) {
    PostRestyleEvent(aElement, nsRestyleHint(0), hint);
  }

  if (aAttribute == nsGkAtoms::style) {
    PostRestyleEvent(aElement, eRestyle_StyleAttribute, nsChangeHint(0));
  }
  // <td> is affected by the cellpadding on its ancestor table,
  // so we should restyle the whole subtree
  if (aAttribute == nsGkAtoms::cellpadding && aElement->IsHTMLElement(nsGkAtoms::table)) {
    PostRestyleEvent(aElement, eRestyle_Subtree, nsChangeHint(0));
  }
  if (aElement->IsAttributeMapped(aAttribute)) {
    Servo_NoteExplicitHints(aElement, eRestyle_Self, nsChangeHint(0));
  }
}

nsresult
ServoRestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  NS_WARNING("stylo: ServoRestyleManager::ReparentStyleContext not implemented");
  return NS_OK;
}

} // namespace mozilla
