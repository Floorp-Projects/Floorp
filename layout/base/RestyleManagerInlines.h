/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerInlines_h
#define mozilla_RestyleManagerInlines_h

#include "mozilla/GeckoRestyleManager.h"
#include "mozilla/ServoRestyleManager.h"
#include "mozilla/ServoUtils.h"

namespace mozilla {

MOZ_DEFINE_STYLO_METHODS(RestyleManager,
                         GeckoRestyleManager, ServoRestyleManager)

void
RestyleManager::PostRestyleEvent(dom::Element* aElement,
                                 nsRestyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint)
{
  MOZ_STYLO_FORWARD(PostRestyleEvent, (aElement, aRestyleHint, aMinChangeHint));
}

void
RestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint)
{
  MOZ_STYLO_FORWARD(RebuildAllStyleData, (aExtraHint, aRestyleHint));
}

void
RestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                             nsRestyleHint aRestyleHint)
{
  MOZ_STYLO_FORWARD(PostRebuildAllStyleDataEvent, (aExtraHint, aRestyleHint));
}

void
RestyleManager::ProcessPendingRestyles()
{
  MOZ_STYLO_FORWARD(ProcessPendingRestyles, ());
}

void
RestyleManager::ContentInserted(nsINode* aContainer,
                                nsIContent* aChild)
{
  MOZ_STYLO_FORWARD(ContentInserted, (aContainer, aChild));
}

void
RestyleManager::ContentAppended(nsIContent* aContainer,
                                nsIContent* aFirstNewContent)
{
  MOZ_STYLO_FORWARD(ContentAppended, (aContainer, aFirstNewContent));
}

void
RestyleManager::ContentRemoved(nsINode* aContainer,
                               nsIContent* aOldChild,
			       nsIContent* aFollowingSibling)
{
  MOZ_STYLO_FORWARD(ContentRemoved, (aContainer, aOldChild, aFollowingSibling));
}

void
RestyleManager::RestyleForInsertOrChange(nsINode* aContainer,
                                         nsIContent* aChild)
{
  MOZ_STYLO_FORWARD(RestyleForInsertOrChange, (aContainer, aChild));
}

void
RestyleManager::RestyleForAppend(nsIContent* aContainer,
                                 nsIContent* aFirstNewContent)
{
  MOZ_STYLO_FORWARD(RestyleForAppend, (aContainer, aFirstNewContent));
}

nsresult
RestyleManager::ContentStateChanged(nsIContent* aContent,
                                    EventStates aStateMask)
{
  MOZ_STYLO_FORWARD(ContentStateChanged, (aContent, aStateMask));
}

void
RestyleManager::AttributeWillChange(dom::Element* aElement,
                                    int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType,
                                    const nsAttrValue* aNewValue)
{
  MOZ_STYLO_FORWARD(AttributeWillChange, (aElement, aNameSpaceID, aAttribute,
                                          aModType, aNewValue));
}

void
RestyleManager::AttributeChanged(dom::Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType,
                                 const nsAttrValue* aOldValue)
{
  MOZ_STYLO_FORWARD(AttributeChanged, (aElement, aNameSpaceID, aAttribute,
                                       aModType, aOldValue));
}

nsresult
RestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  MOZ_STYLO_FORWARD(ReparentStyleContext, (aFrame));
}

} // namespace mozilla

#endif // mozilla_RestyleManagerInlines_h
