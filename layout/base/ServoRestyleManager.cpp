/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/dom/ChildIterator.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "nsStyleChangeList.h"

using namespace mozilla::dom;

namespace mozilla {

ServoRestyleManager::ServoRestyleManager(nsPresContext* aPresContext)
  : RestyleManagerBase(aPresContext)
{
}

void
ServoRestyleManager::PostRestyleEvent(Element* aElement,
                                      nsRestyleHint aRestyleHint,
                                      nsChangeHint aMinChangeHint)
{
  if (MOZ_UNLIKELY(IsDisconnected()) ||
      MOZ_UNLIKELY(PresContext()->PresShell()->IsDestroying())) {
    return;
  }

  if (aRestyleHint == 0 && !aMinChangeHint && !HasPendingRestyles()) {
    return; // Nothing to do.
  }

  // XXX This is a temporary hack to make style attribute change works.
  //     In the future, we should be able to use this hint directly.
  if (aRestyleHint & eRestyle_StyleAttribute) {
    aRestyleHint &= ~eRestyle_StyleAttribute;
    aRestyleHint |= eRestyle_Self | eRestyle_Subtree;
  }

  // Note that unlike in Servo, we don't mark elements as dirty until we process
  // the restyle hints in ProcessPendingRestyles.
  if (aRestyleHint || aMinChangeHint) {
    ServoElementSnapshot* snapshot = SnapshotForElement(aElement);
    snapshot->AddExplicitRestyleHint(aRestyleHint);
    snapshot->AddExplicitChangeHint(aMinChangeHint);
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
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  NS_WARNING("stylo: ServoRestyleManager::PostRebuildAllStyleDataEvent not implemented");
}

void
ServoRestyleManager::RecreateStyleContexts(nsIContent* aContent,
                                           nsStyleContext* aParentContext,
                                           ServoStyleSet* aStyleSet,
                                           nsStyleChangeList& aChangeListToProcess)
{
  MOZ_ASSERT(aContent->IsElement() || aContent->IsNodeOfType(nsINode::eTEXT));

  nsIFrame* primaryFrame = aContent->GetPrimaryFrame();
  if (!primaryFrame && !aContent->IsDirtyForServo()) {
    // This happens when, for example, a display: none child of a
    // HAS_DIRTY_DESCENDANTS content is reached as part of the traversal.
    return;
  }

  // Work on text before.
  if (!aContent->IsElement()) {
    if (primaryFrame) {
      RefPtr<nsStyleContext> oldStyleContext = primaryFrame->StyleContext();
      RefPtr<nsStyleContext> newContext =
        aStyleSet->ResolveStyleForText(aContent, aParentContext);

      for (nsIFrame* f = primaryFrame; f;
           f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
        f->SetStyleContext(newContext);
      }
    }

    aContent->UnsetIsDirtyForServo();
    return;
  }

  Element* element = aContent->AsElement();
  if (element->IsDirtyForServo()) {
    RefPtr<ServoComputedValues> computedValues =
      Servo_ComputedValues_Get(aContent).Consume();
    MOZ_ASSERT(computedValues);

    nsChangeHint changeHint = nsChangeHint(0);

    // Add an explicit change hint if appropriate.
    ServoElementSnapshot* snapshot;
    if (mModifiedElements.Get(element, &snapshot)) {
      changeHint |= snapshot->ExplicitChangeHint();
    }

    // Add the stored change hint if there's a frame. If there isn't a frame,
    // generate a ReconstructFrame change hint if the new display value
    // (which we can get from the ComputedValues stored on the node) is not
    // none.
    if (primaryFrame) {
      changeHint |= primaryFrame->StyleContext()->ConsumeStoredChangeHint();
    } else {
      const nsStyleDisplay* currentDisplay =
        Servo_GetStyleDisplay(computedValues);
      if (currentDisplay->mDisplay != StyleDisplay::None) {
        changeHint |= nsChangeHint_ReconstructFrame;
      }
    }

    // Add the new change hint to the list of elements to process if
    // we need to do any work.
    if (changeHint) {
      aChangeListToProcess.AppendChange(primaryFrame, element, changeHint);
    }

    // The frame reconstruction step (if needed) will ask for the descendants'
    // style correctly. If not needed, we're done too.
    //
    // Note that we must leave the old style on an existing frame that is
    // about to be reframed, since some frame constructor code wants to
    // inspect the old style to work out what to do.
    if (!primaryFrame || (changeHint & nsChangeHint_ReconstructFrame)) {
      aContent->UnsetIsDirtyForServo();
      return;
    }

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

      if (nsIFrame* pseudoFrame = FrameForPseudoElement(element, pseudoTag)) {
        // TODO: we could maybe make this more performant via calling into
        // Servo just once to know which pseudo-elements we've got to restyle?
        RefPtr<nsStyleContext> pseudoContext =
          aStyleSet->ProbePseudoElementStyle(element, pseudoType, newContext);

        // If pseudoContext is null here, it means the frame is going away, so
        // our change hint computation should have already indicated we need
        // to reframe.
        MOZ_ASSERT_IF(!pseudoContext,
                      changeHint & nsChangeHint_ReconstructFrame);
        if (pseudoContext) {
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

    aContent->UnsetIsDirtyForServo();
  }

  if (aContent->HasDirtyDescendantsForServo()) {
    MOZ_ASSERT(primaryFrame,
               "Frame construction should be scheduled, and it takes the "
               "correct style for the children, so no need to be here.");
    StyleChildrenIterator it(aContent);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      if (n->IsElement() || n->IsNodeOfType(nsINode::eTEXT)) {
        RecreateStyleContexts(n, primaryFrame->StyleContext(),
                              aStyleSet, aChangeListToProcess);
      }
    }
    aContent->UnsetHasDirtyDescendantsForServo();
  }
}

static void
MarkChildrenAsDirtyForServo(nsIContent* aContent)
{
  StyleChildrenIterator it(aContent);

  nsIContent* n = it.GetNextChild();
  bool hadChildren = bool(n);
  for (; n; n = it.GetNextChild()) {
    n->SetIsDirtyForServo();
  }

  if (hadChildren) {
    aContent->SetHasDirtyDescendantsForServo();
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

/* static */ void
ServoRestyleManager::NoteRestyleHint(Element* aElement, nsRestyleHint aHint)
{
  const nsRestyleHint HANDLED_RESTYLE_HINTS = eRestyle_Self |
                                              eRestyle_Subtree |
                                              eRestyle_LaterSiblings |
                                              eRestyle_SomeDescendants;
  // NB: For Servo, at least for now, restyling and running selector-matching
  // against the subtree is necessary as part of restyling the element, so
  // processing eRestyle_Self will perform at least as much work as
  // eRestyle_Subtree.
  if (aHint & (eRestyle_Self | eRestyle_Subtree)) {
    aElement->SetIsDirtyForServo();
    aElement->MarkAncestorsAsHavingDirtyDescendantsForServo();
  // NB: Servo gives us a eRestyle_SomeDescendants when it expects us to run
  // selector matching on all the descendants. There's a bug on Servo to align
  // meanings here (#12710) to avoid this potential source of confusion.
  } else if (aHint & eRestyle_SomeDescendants) {
    MarkChildrenAsDirtyForServo(aElement);
    aElement->MarkAncestorsAsHavingDirtyDescendantsForServo();
  }

  if (aHint & eRestyle_LaterSiblings) {
    aElement->MarkAncestorsAsHavingDirtyDescendantsForServo();
    for (nsIContent* cur = aElement->GetNextSibling(); cur;
         cur = cur->GetNextSibling()) {
      cur->SetIsDirtyForServo();
    }
  }

  // TODO: Handle all other nsRestyleHint values.
  if (aHint & ~HANDLED_RESTYLE_HINTS) {
    NS_WARNING(nsPrintfCString("stylo: Unhandled restyle hint %s",
                               RestyleManagerBase::RestyleHintToString(aHint).get()).get());
  }
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
  Element* root = doc->GetRootElement();
  if (root) {
    for (auto iter = mModifiedElements.Iter(); !iter.Done(); iter.Next()) {
      ServoElementSnapshot* snapshot = iter.UserData();
      Element* element = iter.Key();

      // TODO: avoid the ComputeRestyleHint call if we already have the highest
      // explicit restyle hint?
      nsRestyleHint hint = styleSet->ComputeRestyleHint(element, snapshot);
      hint |= snapshot->ExplicitRestyleHint();

      if (hint) {
        NoteRestyleHint(element, hint);
      }
    }

    if (root->IsDirtyForServo() || root->HasDirtyDescendantsForServo()) {
      mInStyleRefresh = true;
      styleSet->StyleDocument(/* aLeaveDirtyBits = */ true);

      // First do any queued-up frame creation. (see bugs 827239 and 997506).
      //
      // XXXEmilio I'm calling this to avoid random behavior changes, since we
      // delay frame construction after styling we should re-check once our
      // model is more stable whether we can skip this call.
      //
      // Note this has to be *after* restyling, because otherwise frame
      // construction will find unstyled nodes, and that's not funny.
      PresContext()->FrameConstructor()->CreateNeededFrames();

      nsStyleChangeList changeList;
      RecreateStyleContexts(root, nullptr, styleSet, changeList);
      ProcessRestyledFrames(changeList);

      mInStyleRefresh = false;
    }
  }

  MOZ_ASSERT(!doc->IsDirtyForServo());
  doc->UnsetHasDirtyDescendantsForServo();

  mModifiedElements.Clear();

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
  if (aContainer == aContainer->OwnerDoc()) {
    // If we're getting this notification for the insertion of a root element,
    // that means either:
    //   (a) We initialized the PresShell before the root element existed, or
    //   (b) The root element was removed and it or another root is being
    //       inserted.
    //
    // Either way the whole tree is dirty, so we should style the document.
    MOZ_ASSERT(aChild == aChild->OwnerDoc()->GetRootElement());
    MOZ_ASSERT(aChild->IsDirtyForServo());
    StyleSet()->StyleDocument(/* aLeaveDirtyBits = */ false);
    return;
  }

  if (!aContainer->HasServoData()) {
    // This can happen with display:none. Bug 1297249 tracks more investigation
    // and assertions here.
    return;
  }

  // Style the new subtree because we will most likely need it during subsequent
  // frame construction. Bug 1298281 tracks deferring this work in the lazy
  // frame construction case.
  StyleSet()->StyleNewSubtree(aChild);

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
  if (!aContainer->HasServoData()) {
    // This can happen with display:none. Bug 1297249 tracks more investigation
    // and assertions here.
    return;
  }

  // Style the new subtree because we will most likely need it during subsequent
  // frame construction. Bug 1298281 tracks deferring this work in the lazy
  // frame construction case.
  if (aFirstNewContent->GetNextSibling()) {
    aContainer->SetHasDirtyDescendantsForServo();
    StyleSet()->StyleNewChildren(aContainer);
  } else {
    StyleSet()->StyleNewSubtree(aFirstNewContent);
  }

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
  ServoElementSnapshot* snapshot = SnapshotForElement(aElement);
  snapshot->AddState(previousState);

  PostRestyleEvent(aElement, restyleHint, changeHint);
  return NS_OK;
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute, int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  ServoElementSnapshot* snapshot = SnapshotForElement(aElement);
  snapshot->AddAttrs(aElement);
}

void
ServoRestyleManager::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                      nsIAtom* aAttribute, int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  MOZ_ASSERT(SnapshotForElement(aElement)->HasAttrs());
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

ServoElementSnapshot*
ServoRestyleManager::SnapshotForElement(Element* aElement)
{
  // NB: aElement is the argument for the construction of the snapshot in the
  // not found case.
  return mModifiedElements.LookupOrAdd(aElement, aElement);
}

} // namespace mozilla
