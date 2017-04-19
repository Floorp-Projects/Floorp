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
  StyleSet()->RebuildData();

  mHaveNonAnimationRestyles = true;

  // NOTE(emilio): GeckoRestlyeManager does a sync style flush, which seems
  // not to be needed in my testing.
  //
  // If it is, we can just do a content flush and call ProcessPendingRestyles.
  if (Element* root = mPresContext->Document()->GetRootElement()) {
    PostRestyleEvent(root, aRestyleHint, aExtraHint);
  }

  // TODO(emilio, bz): Extensions can add/remove stylesheets that can affect
  // non-inheriting anon boxes. It's not clear if we want to support that, but
  // if we do, we need to re-selector-match them here.
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  NS_WARNING("stylo: ServoRestyleManager::PostRebuildAllStyleDataEvent not implemented");
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

void
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
    MOZ_ASSERT_IF(styleFrame, styleFrame->GetType() == nsGkAtoms::imageFrame);
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
    return;
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
    newContext =
      aStyleSet->GetContext(computedValues.forget(), aParentContext, nullptr,
                            CSSPseudoElementType::NotPseudo, aElement);

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
    }

    // Update pseudo-elements state if appropriate.
    const static CSSPseudoElementType pseudosToRestyle[] = {
      CSSPseudoElementType::before,
      CSSPseudoElementType::after,
    };

    for (CSSPseudoElementType pseudoType : pseudosToRestyle) {
      nsIAtom* pseudoTag = nsCSSPseudoElements::GetPseudoAtom(pseudoType);

      if (nsIFrame* pseudoFrame = FrameForPseudoElement(aElement, pseudoTag)) {
        // TODO: we could maybe make this more performant via calling into
        // Servo just once to know which pseudo-elements we've got to restyle?
        RefPtr<nsStyleContext> pseudoContext =
          aStyleSet->ProbePseudoElementStyle(aElement, pseudoType, newContext);
        MOZ_ASSERT(pseudoContext, "should have taken the ReconstructFrame path above");
        pseudoFrame->SetStyleContext(pseudoContext);

        if (pseudoFrame->GetStateBits() & NS_FRAME_OWNS_ANON_BOXES) {
          // XXX It really would be good to pass the actual changehint for our
          // ::before/::after here, but we never computed it!
          pseudoFrame->UpdateStyleOfOwnedAnonBoxes(*aStyleSet, aChangeList,
                                                   nsChangeHint_Hints_NotHandledForDescendants);
        }

        // We only care restyling text nodes, since other type of nodes
        // (images), are still not supported. If that eventually changes, we
        // may have to write more code here... Or not, I don't think too
        // many inherited properties can affect those other frames.
        StyleChildrenIterator it(pseudoFrame->GetContent());
        for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
          if (n->IsNodeOfType(nsINode::eTEXT)) {
            RefPtr<nsStyleContext> childContext =
              aStyleSet->ResolveStyleForText(n, pseudoContext);
            MOZ_ASSERT(n->GetPrimaryFrame(),
                       "How? This node is created at FC time!");
            n->GetPrimaryFrame()->SetStyleContext(childContext);
          }
        }
      }
    }
  }

  bool descendantsNeedFrames = aElement->HasFlag(NODE_DESCENDANTS_NEED_FRAMES);
  bool traverseElementChildren =
    aElement->HasDirtyDescendantsForServo() || descendantsNeedFrames;
  bool traverseTextChildren = recreateContext || descendantsNeedFrames;
  if (traverseElementChildren || traverseTextChildren) {
    nsStyleContext* upToDateContext =
      recreateContext ? newContext : oldStyleContext;

    StyleChildrenIterator it(aElement);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      if (traverseElementChildren && n->IsElement()) {
        ProcessPostTraversal(n->AsElement(), upToDateContext,
                             aStyleSet, aChangeList);
      } else if (traverseTextChildren && n->IsNodeOfType(nsINode::eTEXT)) {
        ProcessPostTraversalForText(n, upToDateContext, aStyleSet, aChangeList);
      }
    }
  }

  aElement->UnsetHasDirtyDescendantsForServo();
  aElement->UnsetFlags(NODE_DESCENDANTS_NEED_FRAMES);
}

void
ServoRestyleManager::ProcessPostTraversalForText(nsIContent* aTextNode,
                                                 nsStyleContext* aParentContext,
                                                 ServoStyleSet* aStyleSet,
                                                 nsStyleChangeList& aChangeList)
{
  // Handle lazy frame construction.
  if (aTextNode->HasFlag(NODE_NEEDS_FRAME)) {
    aChangeList.AppendChange(nullptr, aTextNode, nsChangeHint_ReconstructFrame);
    return;
  }

  // Handle restyle.
  nsIFrame* primaryFrame = aTextNode->GetPrimaryFrame();
  if (primaryFrame) {
    RefPtr<nsStyleContext> oldStyleContext = primaryFrame->StyleContext();
    RefPtr<nsStyleContext> newContext =
      aStyleSet->ResolveStyleForText(aTextNode, aParentContext);

    for (nsIFrame* f = primaryFrame; f;
         f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
      f->SetStyleContext(newContext);
    }
  }
}

/* static */ nsIFrame*
ServoRestyleManager::FrameForPseudoElement(const nsIContent* aContent,
                                           nsIAtom* aPseudoTagOrNull)
{
  MOZ_ASSERT_IF(aPseudoTagOrNull, aContent->IsElement());
  if (!aPseudoTagOrNull) {
    return aContent->GetPrimaryFrame();
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::before) {
    return nsLayoutUtils::GetBeforeFrame(aContent);
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::after) {
    return nsLayoutUtils::GetAfterFrame(aContent);
  }

  MOZ_CRASH("Unkown pseudo-element given to "
            "ServoRestyleManager::FrameForPseudoElement");
  return nullptr;
}

void
ServoRestyleManager::ProcessPendingRestyles()
{
  MOZ_ASSERT(PresContext()->Document(), "No document?  Pshaw!");
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(), "Missing a script blocker!");

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

  // Ensure the refresh driver is active during traversal to avoid mutating
  // mActiveTimer and mMostRecentRefresh time.
  PresContext()->RefreshDriver()->MostRecentRefresh();


  // Perform the Servo traversal, and the post-traversal if required. We do this
  // in a loop because certain rare paths in the frame constructor (like
  // uninstalling XBL bindings) can trigger additional style validations.
  mInStyleRefresh = true;
  if (mHaveNonAnimationRestyles) {
    ++mAnimationGeneration;
  }
  while (styleSet->StyleDocument()) {
    // Recreate style contexts, and queue up change hints (which also handle
    // lazy frame construction).
    nsStyleChangeList currentChanges(StyleBackendType::Servo);
    DocumentStyleRootIterator iter(doc);
    while (Element* root = iter.GetNextStyleRoot()) {
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
        currentChanges.AppendChange(change.mContent->GetPrimaryFrame(),
                                    change.mContent, change.mHint);
      }
      newChanges.Clear();
    }
    mReentrantChanges = nullptr;

    IncrementRestyleGeneration();
  }

  FlushOverflowChangedTracker();

  mHaveNonAnimationRestyles = false;
  mInStyleRefresh = false;
  styleSet->AssertTreeIsClean();

  // Note: We are in the scope of |animationsWithDestroyedFrame|, so
  //       |mAnimationsWithDestroyedFrame| is still valid.
  MOZ_ASSERT(mAnimationsWithDestroyedFrame);
  mAnimationsWithDestroyedFrame->StopAnimationsForElementsWithoutFrames();
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
  if (!aContent->IsElement()) {
    return;
  }

  Element* aElement = aContent->AsElement();
  nsChangeHint changeHint;
  nsRestyleHint restyleHint;

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

  EventStates previousState = aElement->StyleState() ^ aChangedBits;
  ServoElementSnapshot* snapshot = Servo_Element_GetSnapshot(aElement);
  if (snapshot) {
    snapshot->AddState(previousState);
    PostRestyleEvent(aElement, restyleHint, changeHint);
  }
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute, int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  ServoElementSnapshot* snapshot = Servo_Element_GetSnapshot(aElement);
  if (snapshot) {
    snapshot->AddAttrs(aElement);
  }
}

void
ServoRestyleManager::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                      nsIAtom* aAttribute, int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
#ifdef DEBUG
  ServoElementSnapshot* snapshot = Servo_Element_GetSnapshot(aElement);
  MOZ_ASSERT_IF(snapshot, snapshot->HasAttrs());
#endif

  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  if (primaryFrame) {
    primaryFrame->AttributeChanged(aNameSpaceID, aAttribute, aModType);
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
