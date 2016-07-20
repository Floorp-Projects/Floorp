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

/* static */ void
ServoRestyleManager::DirtyTree(nsIContent* aContent)
{
  if (aContent->IsDirtyForServo()) {
    return;
  }

  aContent->SetIsDirtyForServo();

  FlattenedChildIterator it(aContent);

  nsIContent* n = it.GetNextChild();
  bool hadChildren = bool(n);
  for ( ; n; n = it.GetNextChild()) {
    DirtyTree(n);
  }

  if (hadChildren) {
    aContent->SetHasDirtyDescendantsForServo();
  }
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

  if (aRestyleHint == 0 && !aMinChangeHint) {
    // Nothing to do here
    return;
  }

  nsIPresShell* presShell = PresContext()->PresShell();
  if (!ObservingRefreshDriver()) {
    SetObservingRefreshDriver(PresContext()->RefreshDriver()->
        AddStyleFlushObserver(presShell));
  }

  // Propagate the IS_DIRTY flag down the tree.
  DirtyTree(aElement);

  // Propagate the HAS_DIRTY_DESCENDANTS flag to the root.
  nsINode* cur = aElement;
  while ((cur = cur->GetParentNode())) {
    if (cur->HasDirtyDescendantsForServo())
      break;
    cur->SetHasDirtyDescendantsForServo();
  }

  presShell->GetDocument()->SetNeedStyleFlush();
}

void
ServoRestyleManager::PostRestyleEventForLazyConstruction()
{
  NS_ERROR("stylo: ServoRestyleManager::PostRestyleEventForLazyConstruction not implemented");
}

void
ServoRestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                         nsRestyleHint aRestyleHint)
{
  NS_ERROR("stylo: ServoRestyleManager::RebuildAllStyleData not implemented");
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  MOZ_CRASH("stylo: ServoRestyleManager::PostRebuildAllStyleDataEvent not implemented");
}

/* static */ void
ServoRestyleManager::RecreateStyleContexts(nsIContent* aContent,
                                           nsStyleContext* aParentContext,
                                           ServoStyleSet* aStyleSet)
{
  nsIFrame* primaryFrame = aContent->GetPrimaryFrame();

  // TODO: AFAIK this can happen when we have, let's say, display: none. Here we
  // should trigger frame construction if the element is actually dirty (I
  // guess), but we'd better do that once we have all the restyle hints thing
  // figured out.
  if (!primaryFrame) {
    aContent->UnsetFlags(NODE_IS_DIRTY_FOR_SERVO | NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
    return;
  }

  if (aContent->IsDirtyForServo()) {
    RefPtr<ServoComputedValues> computedValues =
      dont_AddRef(Servo_GetComputedValues(aContent));

    // TODO: Figure out what pseudos does this content have, and do the proper
    // thing with them.
    RefPtr<nsStyleContext> context =
      aStyleSet->GetContext(computedValues.forget(),
                            aParentContext,
                            nullptr,
                            CSSPseudoElementType::NotPseudo);

    // TODO: Compare old and new styles to generate restyle change hints, and
    // process them.
    primaryFrame->SetStyleContext(context.get());

    aContent->UnsetFlags(NODE_IS_DIRTY_FOR_SERVO);
  }

  if (aContent->HasDirtyDescendantsForServo()) {
    FlattenedChildIterator it(aContent);
    for (nsIContent* n = it.GetNextChild(); n; n = it.GetNextChild()) {
      RecreateStyleContexts(n, primaryFrame->StyleContext(), aStyleSet);
    }
    aContent->UnsetFlags(NODE_HAS_DIRTY_DESCENDANTS_FOR_SERVO);
  }
}

void
ServoRestyleManager::ProcessPendingRestyles()
{
  if (!HasPendingRestyles()) {
    return;
  }
  ServoStyleSet* styleSet = StyleSet();

  nsIDocument* doc = PresContext()->Document();

  Element* root = doc->GetRootElement();
  if (root) {
    styleSet->RestyleSubtree(root, /* aForce = */ false);
    RecreateStyleContexts(root, nullptr, styleSet);
  }

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
  NS_ERROR("stylo: ServoRestyleManager::RestyleForInsertOrChange not implemented");
}

void
ServoRestyleManager::RestyleForAppend(Element* aContainer,
                                      nsIContent* aFirstNewContent)
{
  NS_ERROR("stylo: ServoRestyleManager::RestyleForAppend not implemented");
}

void
ServoRestyleManager::RestyleForRemove(Element* aContainer,
                                      nsIContent* aOldChild,
                                      nsIContent* aFollowingSibling)
{
  NS_ERROR("stylo: ServoRestyleManager::RestyleForRemove not implemented");
}

nsresult
ServoRestyleManager::ContentStateChanged(nsIContent* aContent,
                                         EventStates aStateMask)
{
  if (!aContent->IsElement()) {
    return NS_OK;
  }

  Element* aElement = aContent->AsElement();
  nsChangeHint changeHint;
  nsRestyleHint restyleHint;
  ContentStateChangedInternal(aElement, aStateMask, &changeHint, &restyleHint);

  PostRestyleEvent(aElement, restyleHint, changeHint);
  return NS_OK;
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  NS_ERROR("stylo: ServoRestyleManager::AttributeWillChange not implemented");
}

void
ServoRestyleManager::AttributeChanged(Element* aElement,
                                      int32_t aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  NS_ERROR("stylo: ServoRestyleManager::AttributeChanged not implemented");
}

nsresult
ServoRestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  MOZ_CRASH("stylo: ServoRestyleManager::ReparentStyleContext not implemented");
}

} // namespace mozilla
