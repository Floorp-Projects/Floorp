/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"

using namespace mozilla::dom;

namespace mozilla {

ServoRestyleManager::ServoRestyleManager()
  : mRestyleGeneration(1)
{
}

void
ServoRestyleManager::Disconnect()
{
  NS_ERROR("stylo: ServoRestyleManager::Disconnect not implemented");
}

void
ServoRestyleManager::PostRestyleEvent(Element* aElement,
                                      nsRestyleHint aRestyleHint,
                                      nsChangeHint aMinChangeHint)
{
  NS_ERROR("stylo: ServoRestyleManager::PostRestyleEvent not implemented");
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

void
ServoRestyleManager::ProcessPendingRestyles()
{
  // XXXheycam Do nothing for now.
  mRestyleGeneration++;
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
  NS_ERROR("stylo: ServoRestyleManager::ContentStateChanged not implemented");
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

bool
ServoRestyleManager::HasPendingRestyles()
{
  NS_ERROR("stylo: ServoRestyleManager::HasPendingRestyles not implemented");
  return false;
}

uint64_t
ServoRestyleManager::GetRestyleGeneration() const
{
  return mRestyleGeneration;
}

} // namespace mozilla
