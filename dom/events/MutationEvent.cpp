/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozilla/dom/MutationEvent.h"
#include "mozilla/InternalMutationEvent.h"

class nsPresContext;

namespace mozilla {
namespace dom {

MutationEvent::MutationEvent(EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             InternalMutationEvent* aEvent)
  : Event(aOwner, aPresContext,
          aEvent ? aEvent : new InternalMutationEvent(false, eVoidEvent))
{
  mEventIsInternal = (aEvent == nullptr);
}

nsINode*
MutationEvent::GetRelatedNode()
{
  return mEvent->AsMutationEvent()->mRelatedNode;
}

void
MutationEvent::GetPrevValue(nsAString& aPrevValue) const
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mPrevAttrValue)
    mutation->mPrevAttrValue->ToString(aPrevValue);
}

void
MutationEvent::GetNewValue(nsAString& aNewValue) const
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
}

void
MutationEvent::GetAttrName(nsAString& aAttrName) const
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
}

uint16_t
MutationEvent::AttrChange()
{
  return mEvent->AsMutationEvent()->mAttrChange;
}

void
MutationEvent::InitMutationEvent(const nsAString& aType,
                                 bool aCanBubble, bool aCancelable,
                                 nsINode* aRelatedNode,
                                 const nsAString& aPrevValue,
                                 const nsAString& aNewValue,
                                 const nsAString& aAttrName,
                                 uint16_t& aAttrChange,
                                 ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, aCanBubble, aCancelable);

  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  mutation->mRelatedNode = aRelatedNode;
  if (!aPrevValue.IsEmpty())
    mutation->mPrevAttrValue = NS_Atomize(aPrevValue);
  if (!aNewValue.IsEmpty())
    mutation->mNewAttrValue = NS_Atomize(aNewValue);
  if (!aAttrName.IsEmpty()) {
    mutation->mAttrName = NS_Atomize(aAttrName);
  }
  mutation->mAttrChange = aAttrChange;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<MutationEvent>
NS_NewDOMMutationEvent(EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       InternalMutationEvent* aEvent)
{
  RefPtr<MutationEvent> it = new MutationEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
