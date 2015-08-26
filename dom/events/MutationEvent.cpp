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
          aEvent ? aEvent : new InternalMutationEvent(false, NS_EVENT_NULL))
{
  mEventIsInternal = (aEvent == nullptr);
}

NS_INTERFACE_MAP_BEGIN(MutationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MutationEvent, Event)
NS_IMPL_RELEASE_INHERITED(MutationEvent, Event)

already_AddRefed<nsINode>
MutationEvent::GetRelatedNode()
{
  nsCOMPtr<nsINode> n =
    do_QueryInterface(mEvent->AsMutationEvent()->mRelatedNode);
  return n.forget();
}

NS_IMETHODIMP
MutationEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  nsCOMPtr<nsINode> relatedNode = GetRelatedNode();
  nsCOMPtr<nsIDOMNode> relatedDOMNode = relatedNode ? relatedNode->AsDOMNode() : nullptr;
  relatedDOMNode.forget(aRelatedNode);
  return NS_OK;
}

NS_IMETHODIMP
MutationEvent::GetPrevValue(nsAString& aPrevValue)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mPrevAttrValue)
    mutation->mPrevAttrValue->ToString(aPrevValue);
  return NS_OK;
}

NS_IMETHODIMP
MutationEvent::GetNewValue(nsAString& aNewValue)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
  return NS_OK;
}

NS_IMETHODIMP
MutationEvent::GetAttrName(nsAString& aAttrName)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
  return NS_OK;
}

uint16_t
MutationEvent::AttrChange()
{
  return mEvent->AsMutationEvent()->mAttrChange;
}

NS_IMETHODIMP
MutationEvent::GetAttrChange(uint16_t* aAttrChange)
{
  *aAttrChange = AttrChange();
  return NS_OK;
}

NS_IMETHODIMP
MutationEvent::InitMutationEvent(const nsAString& aTypeArg,
                                 bool aCanBubbleArg,
                                 bool aCancelableArg,
                                 nsIDOMNode* aRelatedNodeArg,
                                 const nsAString& aPrevValueArg,
                                 const nsAString& aNewValueArg,
                                 const nsAString& aAttrNameArg,
                                 uint16_t aAttrChangeArg)
{
  nsresult rv = Event::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  mutation->mRelatedNode = aRelatedNodeArg;
  if (!aPrevValueArg.IsEmpty())
    mutation->mPrevAttrValue = do_GetAtom(aPrevValueArg);
  if (!aNewValueArg.IsEmpty())
    mutation->mNewAttrValue = do_GetAtom(aNewValueArg);
  if (!aAttrNameArg.IsEmpty()) {
    mutation->mAttrName = do_GetAtom(aAttrNameArg);
  }
  mutation->mAttrChange = aAttrChangeArg;
    
  return NS_OK;
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
  nsRefPtr<MutationEvent> it = new MutationEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
