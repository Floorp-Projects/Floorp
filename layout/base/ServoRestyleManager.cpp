/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"

#include "mozilla/DocumentStyleRootIterator.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoRestyleManagerInlines.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/dom/ChildIterator.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "nsStyleChangeList.h"

using namespace mozilla::dom;

namespace mozilla {

ServoRestyleManager::ServoRestyleManager(nsPresContext* aPresContext)
  : RestyleManagerBase(aPresContext)
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

  if (aRestyleHint == 0 && !aMinChangeHint && !HasPendingRestyles()) {
    return; // Nothing to do.
  }

  // We allow posting change hints during restyling, but not restyle hints
  // themselves, since those would require us to re-traverse the tree.
  MOZ_ASSERT_IF(mInStyleRefresh, aRestyleHint == 0);

  // Processing change hints sometimes causes new change hints to be generated.
  // Doing this after the gecko post-traversal is problematic, so instead we just
  // queue them up for special handling.
  if (mReentrantChanges) {
    MOZ_ASSERT(aRestyleHint == 0);
    mReentrantChanges->AppendElement(ReentrantChange { aElement, aMinChangeHint });
    return;
  }

  // XXX This is a temporary hack to make style attribute change works.
  //     In the future, we should be able to use this hint directly.
  if (aRestyleHint & eRestyle_StyleAttribute) {
    aRestyleHint &= ~eRestyle_StyleAttribute;
    aRestyleHint |= eRestyle_Self | eRestyle_Subtree;
  }

  // XXX For now, convert eRestyle_Subtree into (eRestyle_Self |
  // eRestyle_SomeDescendants), which Servo will interpret as
  // RESTYLE_SELF | RESTYLE_DESCENDANTS, since this is a commonly
  // posted restyle hint that doesn't yet align with RestyleHint's
  // bits.
  if (aRestyleHint & eRestyle_Subtree) {
    aRestyleHint &= ~eRestyle_Subtree;
    aRestyleHint |= eRestyle_Self | eRestyle_SomeDescendants;
  }

  if (aRestyleHint || aMinChangeHint) {
    Servo_NoteExplicitHints(aElement, aRestyleHint, aMinChangeHint);
  }

  PostRestyleEventInternal(false);
}

void
ServoRestyleManager::PostRestyleEventForLazyConstruction()
{
  PostRestyleEventInternal(true);
}

void
ServoRestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                         nsRestyleHint aRestyleHint)
{
  NS_WARNING("stylo: ServoRestyleManager::RebuildAllStyleData not implemented");
  // That said, we do know that rebuilding all style data in Gecko would get rid
  // of the old ruletree, and hence of the cached-on-the-root default computed
  // styles.  So we know we need to clear them here.  I think this is the only
  // way they could get cleared, in fact, though not _all_ calls that come
  // through here may need to clear them in practice.
  //
  // We probably need to do some actual restyling here too, though.  And figure
  // out whether it actually matters that we may be recomputing the default
  // styles in too many cases.  For one thing, we do a bunch of eager work here,
  // whereas we should really just set a bit that says to recompute the default
  // computed styles before the next time we restyle anything!
  StyleSet()->RecomputeDefaultComputedStyles();
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
  aElement->ClearServoData();

  StyleChildrenIterator it(aElement);
  for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
    if (n->IsElement()) {
      ClearServoDataFromSubtree(n->AsElement());
    }
  }

  aElement->UnsetHasDirtyDescendantsForServo();
}

/* static */ void
ServoRestyleManager::ClearDirtyDescendantsFromSubtree(Element* aElement)
{
  StyleChildrenIterator it(aElement);
  for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
    if (n->IsElement()) {
      ClearDirtyDescendantsFromSubtree(n->AsElement());
    }
  }

  aElement->UnsetHasDirtyDescendantsForServo();
}

void
ServoRestyleManager::RecreateStyleContexts(Element* aElement,
                                           nsStyleContext* aParentContext,
                                           ServoStyleSet* aStyleSet,
                                           nsStyleChangeList& aChangeListToProcess)
{
  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();

  // FIXME(bholley): Once we transfer ownership of the styles to the frame, we
  // can fast-reject without the FFI call by checking mServoData for null.
  nsChangeHint changeHint = Servo_TakeChangeHint(aElement);
  if (changeHint & ~nsChangeHint_NeutralChange) {
    aChangeListToProcess.AppendChange(
      primaryFrame, aElement, changeHint & ~nsChangeHint_NeutralChange);
  }

  // If our change hint is reconstruct, we delegate to the frame constructor,
  // which consumes the new style and expects the old style to be on the frame.
  //
  // XXXbholley: We should teach the frame constructor how to clear the dirty
  // descendants bit to avoid the traversal here.
  if (changeHint & nsChangeHint_ReconstructFrame) {
    ClearDirtyDescendantsFromSubtree(aElement);
    return;
  }

  // If we have a frame and a non-zero + non-reconstruct change hint, we need to
  // attach a new style context.
  bool recreateContext = primaryFrame && changeHint;
  if (recreateContext) {
    RefPtr<ServoComputedValues> computedValues = aStyleSet->ResolveServoStyle(aElement);

    // Hold the old style context alive, because it could become a dangling
    // pointer during the replacement. In practice it's not a huge deal (on
    // GetNextContinuationWithSameStyle the pointer is not dereferenced, only
    // compared), but better not playing with dangling pointers if not needed.
    RefPtr<nsStyleContext> oldStyleContext = primaryFrame->StyleContext();
    MOZ_ASSERT(oldStyleContext);

    RefPtr<nsStyleContext> newContext =
      aStyleSet->GetContext(computedValues.forget(), aParentContext, nullptr,
                            CSSPseudoElementType::NotPseudo);

    // XXX This could not always work as expected: there are kinds of content
    // with the first split and the last sharing style, but others not. We
    // should handle those properly.
    for (nsIFrame* f = primaryFrame; f;
         f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
      f->SetStyleContext(newContext);
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

  bool traverseElementChildren = aElement->HasDirtyDescendantsForServo();
  bool traverseTextChildren = recreateContext;
  if (traverseElementChildren || traverseTextChildren) {
    StyleChildrenIterator it(aElement);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      if (traverseElementChildren && n->IsElement()) {
        if (!primaryFrame) {
          // The frame constructor presumably decided to suppress frame
          // construction on this subtree. Just clear the dirty descendants
          // bit from the subtree, since there's no point in harvesting the
          // change hints.
          MOZ_ASSERT(!n->AsElement()->GetPrimaryFrame(),
                     "Only display:contents should do this, and we don't handle that yet");
          ClearDirtyDescendantsFromSubtree(n->AsElement());
        } else {
          RecreateStyleContexts(n->AsElement(), primaryFrame->StyleContext(),
                                aStyleSet, aChangeListToProcess);
        }
      } else if (traverseTextChildren && n->IsNodeOfType(nsINode::eTEXT)) {
        RecreateStyleContextsForText(n, primaryFrame->StyleContext(),
                                     aStyleSet);
      }
    }
  }

  aElement->UnsetHasDirtyDescendantsForServo();
}

void
ServoRestyleManager::RecreateStyleContextsForText(nsIContent* aTextNode,
                                                  nsStyleContext* aParentContext,
                                                  ServoStyleSet* aStyleSet)
{
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
  nsIFrame* primaryFrame = aContent->GetPrimaryFrame();

  if (!aPseudoTagOrNull) {
    return primaryFrame;
  }

  if (!primaryFrame) {
    return nullptr;
  }

  // NOTE: we probably need to special-case display: contents here. Gecko's
  // RestyleManager passes the primary frame of the parent instead.
  if (aPseudoTagOrNull == nsCSSPseudoElements::before) {
    return nsLayoutUtils::GetBeforeFrameForContent(primaryFrame, aContent);
  }

  if (aPseudoTagOrNull == nsCSSPseudoElements::after) {
    return nsLayoutUtils::GetAfterFrameForContent(primaryFrame, aContent);
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

  if (!HasPendingRestyles()) {
    return;
  }

  ServoStyleSet* styleSet = StyleSet();
  nsIDocument* doc = PresContext()->Document();

  // XXXbholley: Should this be while() per bug 1316247?
  if (HasPendingRestyles()) {
    mInStyleRefresh = true;
    styleSet->StyleDocument();

    // First do any queued-up frame creation. (see bugs 827239 and 997506).
    //
    // XXXEmilio I'm calling this to avoid random behavior changes, since we
    // delay frame construction after styling we should re-check once our
    // model is more stable whether we can skip this call.
    //
    // Note this has to be *after* restyling, because otherwise frame
    // construction will find unstyled nodes, and that's not funny.
    PresContext()->FrameConstructor()->CreateNeededFrames();

    // Recreate style contexts and queue up change hints.
    nsStyleChangeList currentChanges;
    DocumentStyleRootIterator iter(doc);
    while (Element* root = iter.GetNextStyleRoot()) {
      RecreateStyleContexts(root, nullptr, styleSet, currentChanges);
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

    styleSet->AssertTreeIsClean();
    mInStyleRefresh = false;
  }

  IncrementRestyleGeneration();
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
ServoRestyleManager::ContentInserted(nsINode* aContainer, nsIContent* aChild)
{
  RestyleForInsertOrChange(aContainer, aChild);
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
ServoRestyleManager::ContentAppended(nsIContent* aContainer,
                                     nsIContent* aFirstNewContent)
{
  RestyleForAppend(aContainer, aFirstNewContent);
}

void
ServoRestyleManager::ContentRemoved(nsINode* aContainer,
                                    nsIContent* aOldChild,
                                    nsIContent* aFollowingSibling)
{
  NS_WARNING("stylo: ServoRestyleManager::ContentRemoved not implemented");
}

nsresult
ServoRestyleManager::ContentStateChanged(nsIContent* aContent,
                                         EventStates aChangedBits)
{
  if (!aContent->IsElement()) {
    return NS_OK;
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

  return NS_OK;
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
  if (aAttribute == nsGkAtoms::style) {
    PostRestyleEvent(aElement, eRestyle_StyleAttribute, nsChangeHint(0));
  }
}

nsresult
ServoRestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  NS_WARNING("stylo: ServoRestyleManager::ReparentStyleContext not implemented");
  return NS_OK;
}

} // namespace mozilla
