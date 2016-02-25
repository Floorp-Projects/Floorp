/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoRestyleManager.h"

using namespace mozilla::dom;

namespace mozilla {

void
ServoRestyleManager::Disconnect()
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::PostRestyleEvent(Element* aElement,
                                      nsRestyleHint aRestyleHint,
                                      nsChangeHint aMinChangeHint)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::PostRestyleEventForLazyConstruction()
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                         nsRestyleHint aRestyleHint)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                                  nsRestyleHint aRestyleHint)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::ProcessPendingRestyles()
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::RestyleForInsertOrChange(Element* aContainer,
                                              nsIContent* aChild)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::RestyleForAppend(Element* aContainer,
                                      nsIContent* aFirstNewContent)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::RestyleForRemove(Element* aContainer,
                                      nsIContent* aOldChild,
                                      nsIContent* aFollowingSibling)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoRestyleManager::ContentStateChanged(nsIContent* aContent,
                                         EventStates aStateMask)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::AttributeWillChange(Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t aModType,
                                         const nsAttrValue* aNewValue)
{
  MOZ_CRASH("stylo: not implemented");
}

void
ServoRestyleManager::AttributeChanged(Element* aElement,
                                      int32_t aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  MOZ_CRASH("stylo: not implemented");
}

nsresult
ServoRestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  MOZ_CRASH("stylo: not implemented");
}

bool
ServoRestyleManager::HasPendingRestyles()
{
  MOZ_CRASH("stylo: not implemented");
}

uint64_t
ServoRestyleManager::GetRestyleGeneration() const
{
  MOZ_CRASH("stylo: not implemented");
}

} // namespace mozilla
