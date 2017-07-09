/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerHandleInlines_h
#define mozilla_RestyleManagerHandleInlines_h

#include "mozilla/GeckoRestyleManager.h"
#include "mozilla/ServoRestyleManager.h"

#define FORWARD_CONCRETE(method_, geckoargs_, servoargs_) \
  if (IsGecko()) { \
    return AsGecko()->method_ geckoargs_; \
  } else { \
    return AsServo()->method_ servoargs_; \
  }

#define FORWARD(method_, args_) FORWARD_CONCRETE(method_, args_, args_)

namespace mozilla {

void
RestyleManagerHandle::Ptr::PostRestyleEvent(dom::Element* aElement,
                                            nsRestyleHint aRestyleHint,
                                            nsChangeHint aMinChangeHint)
{
  FORWARD(PostRestyleEvent, (aElement, aRestyleHint, aMinChangeHint));
}

void
RestyleManagerHandle::Ptr::PostRestyleEventForLazyConstruction()
{
  FORWARD(PostRestyleEventForLazyConstruction, ());
}

void
RestyleManagerHandle::Ptr::RebuildAllStyleData(nsChangeHint aExtraHint,
                                               nsRestyleHint aRestyleHint)
{
  FORWARD(RebuildAllStyleData, (aExtraHint, aRestyleHint));
}

void
RestyleManagerHandle::Ptr::PostRebuildAllStyleDataEvent(
    nsChangeHint aExtraHint,
    nsRestyleHint aRestyleHint)
{
  FORWARD(PostRebuildAllStyleDataEvent, (aExtraHint, aRestyleHint));
}

void
RestyleManagerHandle::Ptr::ProcessPendingRestyles()
{
  FORWARD(ProcessPendingRestyles, ());
}

nsresult
RestyleManagerHandle::Ptr::ContentStateChanged(nsIContent* aContent,
                                          EventStates aStateMask)
{
  FORWARD(ContentStateChanged, (aContent, aStateMask));
}

void
RestyleManagerHandle::Ptr::AttributeWillChange(dom::Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsIAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aNewValue)
{
  FORWARD(AttributeWillChange, (aElement, aNameSpaceID, aAttribute, aModType,
                                aNewValue));
}

void
RestyleManagerHandle::Ptr::AttributeChanged(dom::Element* aElement,
                                            int32_t aNameSpaceID,
                                            nsIAtom* aAttribute,
                                            int32_t aModType,
                                            const nsAttrValue* aOldValue)
{
  FORWARD(AttributeChanged, (aElement, aNameSpaceID, aAttribute, aModType,
                             aOldValue));
}

nsresult
RestyleManagerHandle::Ptr::ReparentStyleContext(nsIFrame* aFrame)
{
  FORWARD(ReparentStyleContext, (aFrame));
}

} // namespace mozilla

#undef FORWARD
#undef FORWARD_CONCRETE

#endif // mozilla_RestyleManagerHandleInlines_h
