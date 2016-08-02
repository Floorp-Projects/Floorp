/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoStyleSet.h"

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

  // Note that unlike in Servo, we don't mark elements as dirty until we process
  // the restyle hints in ProcessPendingRestyles.
  if (aRestyleHint || aMinChangeHint) {
    ServoElementSnapshot* snapshot = SnapshotForElement(aElement);
    snapshot->AddExplicitRestyleHint(aRestyleHint);
    snapshot->AddExplicitChangeHint(aMinChangeHint);
  }

  nsIPresShell* presShell = PresContext()->PresShell();
  if (!ObservingRefreshDriver()) {
    SetObservingRefreshDriver(
      PresContext()->RefreshDriver()->AddStyleFlushObserver(presShell));
  }

  presShell->GetDocument()->SetNeedStyleFlush();
}

void
ServoRestyleManager::PostRestyleEventForLazyConstruction()
{
  NS_WARNING("stylo: ServoRestyleManager::PostRestyleEventForLazyConstruction not implemented");
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
  MOZ_CRASH("stylo: ServoRestyleManager::PostRebuildAllStyleDataEvent not implemented");
}

void
ServoRestyleManager::RecreateStyleContexts(nsIContent* aContent,
                                           nsStyleContext* aParentContext,
                                           ServoStyleSet* aStyleSet,
                                           nsStyleChangeList& aChangeListToProcess)
{
  nsIFrame* primaryFrame = aContent->GetPrimaryFrame();
  if (!primaryFrame && !aContent->IsDirtyForServo()) {
    NS_WARNING("Frame not found for non-dirty content");
    return;
  }

  if (aContent->IsDirtyForServo()) {
    nsChangeHint changeHint;
    if (primaryFrame) {
      changeHint = primaryFrame->StyleContext()->ConsumeStoredChangeHint();
    } else {
      // TODO: Use the frame constructor's UndisplayedNodeMap to store the old
      // style contexts, and thus the change hints. That way we can in most
      // cases avoid generating ReconstructFrame and push it to the list just to
      // notice at frame construction that it doesn't need a frame.
      changeHint = nsChangeHint_ReconstructFrame;
    }

    // NB: The change list only expects elements.
    if (changeHint && aContent->IsElement()) {
      aChangeListToProcess.AppendChange(primaryFrame, aContent, changeHint);
    }

    if (!primaryFrame) {
      // The frame reconstruction step will ask for the descendant's style
      // correctly.
      return;
    }

    // Even if we don't have a change hint, we still need to swap style contexts
    // so our new style is updated properly.
    RefPtr<ServoComputedValues> computedValues =
      dont_AddRef(Servo_GetComputedValues(aContent));

    // TODO: Figure out what pseudos does this content have, and do the proper
    // thing with them.
    RefPtr<nsStyleContext> newContext =
      aStyleSet->GetContext(computedValues.forget(),
                            aParentContext,
                            nullptr,
                            CSSPseudoElementType::NotPseudo);

    RefPtr<nsStyleContext> oldStyleContext = primaryFrame->StyleContext();
    MOZ_ASSERT(oldStyleContext);

    // XXX This could not always work as expected there are kinds of content
    // with the first split and the last sharing style, but others not. We
    // should handle those properly.
    for (nsIFrame* f = primaryFrame; f;
         f = GetNextContinuationWithSameStyle(f, oldStyleContext)) {
      f->SetStyleContext(newContext);
    }

    // TODO: There are other continuations we still haven't restyled, mostly
    // pseudo-elements. We have to deal with those, and with anonymous boxes.
    aContent->UnsetFlags(NODE_IS_DIRTY_FOR_SERVO);
  }

  if (aContent->HasDirtyDescendantsForServo()) {
    MOZ_ASSERT(primaryFrame,
               "Frame construction should be scheduled, and it takes the "
               "correct style for the children");
    FlattenedChildIterator it(aContent);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      RecreateStyleContexts(n, primaryFrame->StyleContext(),
                            aStyleSet, aChangeListToProcess);
    }
    aContent->UnsetFlags(NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
  }
}

static void
MarkParentsAsHavingDirtyDescendants(Element* aElement)
{
  nsINode* cur = aElement;
  while ((cur = cur->GetParentNode())) {
    if (cur->HasDirtyDescendantsForServo()) {
      break;
    }

    cur->SetHasDirtyDescendantsForServo();
  }
}

static void
MarkChildrenAsDirtyForServo(nsIContent* aContent)
{
  FlattenedChildIterator it(aContent);

  nsIContent* n = it.GetNextChild();
  bool hadChildren = bool(n);
  for (; n; n = it.GetNextChild()) {
    n->SetIsDirtyForServo();
  }

  if (hadChildren) {
    aContent->SetHasDirtyDescendantsForServo();
  }
}

void
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
    MarkParentsAsHavingDirtyDescendants(aElement);
  // NB: Servo gives us a eRestyle_SomeDescendants when it expects us to run
  // selector matching on all the descendants. There's a bug on Servo to align
  // meanings here (#12710) to avoid this potential source of confusion.
  } else if (aHint & eRestyle_SomeDescendants) {
    MarkChildrenAsDirtyForServo(aElement);
    MarkParentsAsHavingDirtyDescendants(aElement);
  }

  if (aHint & eRestyle_LaterSiblings) {
    for (nsINode* cur = aElement->GetNextSibling(); cur;
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
  if (!HasPendingRestyles()) {
    return;
  }

  ServoStyleSet* styleSet = StyleSet();
  if (!styleSet->StylingStarted()) {
    // If something caused us to restyle, and we haven't started styling yet,
    // do nothing. Everything is dirty, and we'll style it all later.
    return;
  }

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
      styleSet->RestyleSubtree(root);

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
    }
  }

  mModifiedElements.Clear();

  // NB: we restyle from the root element, but the document also gets the
  // HAS_DIRTY_DESCENDANTS flag as part of the loop on PostRestyleEvent, and we
  // use that to check we have pending restyles.
  //
  // Thus, they need to get cleared here.
  MOZ_ASSERT(!doc->IsDirtyForServo());
  doc->UnsetFlags(NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);

  IncrementRestyleGeneration();
}

void
ServoRestyleManager::RestyleForInsertOrChange(Element* aContainer,
                                              nsIContent* aChild)
{
  NS_WARNING("stylo: ServoRestyleManager::RestyleForInsertOrChange not implemented");
}

void
ServoRestyleManager::RestyleForAppend(Element* aContainer,
                                      nsIContent* aFirstNewContent)
{
  NS_WARNING("stylo: ServoRestyleManager::RestyleForAppend not implemented");
}

void
ServoRestyleManager::RestyleForRemove(Element* aContainer,
                                      nsIContent* aOldChild,
                                      nsIContent* aFollowingSibling)
{
  NS_WARNING("stylo: ServoRestyleManager::RestyleForRemove not implemented");
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

nsresult
ServoRestyleManager::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  return base_type::ProcessRestyledFrames(aChangeList, *PresContext(),
                                          mOverflowChangedTracker);
}

} // namespace mozilla
