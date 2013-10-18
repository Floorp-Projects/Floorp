/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsDOMMutationEvent.h"
#include "mozilla/MutationEvent.h"

using namespace mozilla;

class nsPresContext;

nsDOMMutationEvent::nsDOMMutationEvent(mozilla::dom::EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       InternalMutationEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new InternalMutationEvent(false, 0))
{
  mEventIsInternal = (aEvent == nullptr);
}

NS_INTERFACE_MAP_BEGIN(nsDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMutationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMutationEvent, nsDOMEvent)

already_AddRefed<nsINode>
nsDOMMutationEvent::GetRelatedNode()
{
  nsCOMPtr<nsINode> n =
    do_QueryInterface(mEvent->AsMutationEvent()->mRelatedNode);
  return n.forget();
}

NS_IMETHODIMP
nsDOMMutationEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  nsCOMPtr<nsINode> relatedNode = GetRelatedNode();
  nsCOMPtr<nsIDOMNode> relatedDOMNode = relatedNode ? relatedNode->AsDOMNode() : nullptr;
  relatedDOMNode.forget(aRelatedNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetPrevValue(nsAString& aPrevValue)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mPrevAttrValue)
    mutation->mPrevAttrValue->ToString(aPrevValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetNewValue(nsAString& aNewValue)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrName(nsAString& aAttrName)
{
  InternalMutationEvent* mutation = mEvent->AsMutationEvent();
  if (mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
  return NS_OK;
}

uint16_t
nsDOMMutationEvent::AttrChange()
{
  return mEvent->AsMutationEvent()->mAttrChange;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrChange(uint16_t* aAttrChange)
{
  *aAttrChange = AttrChange();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::InitMutationEvent(const nsAString& aTypeArg, bool aCanBubbleArg, bool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAString& aPrevValueArg, const nsAString& aNewValueArg, const nsAString& aAttrNameArg, uint16_t aAttrChangeArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
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

nsresult NS_NewDOMMutationEvent(nsIDOMEvent** aInstancePtrResult,
                                mozilla::dom::EventTarget* aOwner,
                                nsPresContext* aPresContext,
                                InternalMutationEvent* aEvent) 
{
  nsDOMMutationEvent* it = new nsDOMMutationEvent(aOwner, aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
