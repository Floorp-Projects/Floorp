/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerHandleInlines_h
#define mozilla_RestyleManagerHandleInlines_h

#include "mozilla/RestyleManager.h"
#include "mozilla/ServoRestyleManager.h"

#define FORWARD_CONCRETE(method_, geckoargs_, servoargs_) \
  if (IsGecko()) { \
    return AsGecko()->method_ geckoargs_; \
  } else { \
    return AsServo()->method_ servoargs_; \
  }

#define FORWARD(method_, args_) FORWARD_CONCRETE(method_, args_, args_)

namespace mozilla {

MozExternalRefCountType
RestyleManagerHandle::Ptr::AddRef()
{
  FORWARD(AddRef, ());
}

MozExternalRefCountType
RestyleManagerHandle::Ptr::Release()
{
  FORWARD(Release, ());
}

void
RestyleManagerHandle::Ptr::Disconnect()
{
  FORWARD(Disconnect, ());
}

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
RestyleManagerHandle::Ptr::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  FORWARD(ProcessRestyledFrames, (aChangeList));
}

void
RestyleManagerHandle::Ptr::FlushOverflowChangedTracker()
{
  FORWARD(FlushOverflowChangedTracker, ());
}

void
RestyleManagerHandle::Ptr::RestyleForInsertOrChange(dom::Element* aContainer,
                                                    nsIContent* aChild)
{
  FORWARD(RestyleForInsertOrChange, (aContainer, aChild));
}

void
RestyleManagerHandle::Ptr::RestyleForAppend(dom::Element* aContainer,
                                            nsIContent* aFirstNewContent)
{
  FORWARD(RestyleForAppend, (aContainer, aFirstNewContent));
}

void
RestyleManagerHandle::Ptr::RestyleForRemove(dom::Element* aContainer,
                                            nsIContent* aOldChild,
                                            nsIContent* aFollowingSibling)
{
  FORWARD(RestyleForRemove, (aContainer, aOldChild, aFollowingSibling));
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

bool
RestyleManagerHandle::Ptr::HasPendingRestyles()
{
  FORWARD(HasPendingRestyles, ());
}

uint64_t
RestyleManagerHandle::Ptr::GetRestyleGeneration() const
{
  FORWARD(GetRestyleGeneration, ());
}

uint32_t
RestyleManagerHandle::Ptr::GetHoverGeneration() const
{
  FORWARD(GetHoverGeneration, ());
}

void
RestyleManagerHandle::Ptr::SetObservingRefreshDriver(bool aObserving)
{
  FORWARD(SetObservingRefreshDriver, (aObserving));
}


} // namespace mozilla

#undef FORWARD
#undef FORWARD_CONCRETE

#endif // mozilla_RestyleManagerHandleInlines_h
