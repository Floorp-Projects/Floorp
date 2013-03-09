/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMMutationEvent.h"
#include "nsMutationEvent.h"


class nsPresContext;

nsDOMMutationEvent::nsDOMMutationEvent(mozilla::dom::EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       nsMutationEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext,
               aEvent ? aEvent : new nsMutationEvent(false, 0))
{
  mEventIsInternal = (aEvent == nullptr);
}

nsDOMMutationEvent::~nsDOMMutationEvent()
{
  if (mEventIsInternal) {
    nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
    delete mutation;
    mEvent = nullptr;
  }
}

DOMCI_DATA(MutationEvent, nsDOMMutationEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MutationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMutationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMutationEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMutationEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  *aRelatedNode = nullptr;
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
  *aRelatedNode = mutation->mRelatedNode;
  NS_IF_ADDREF(*aRelatedNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetPrevValue(nsAString& aPrevValue)
{
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
  if (mutation->mPrevAttrValue)
    mutation->mPrevAttrValue->ToString(aPrevValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetNewValue(nsAString& aNewValue)
{
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
  if (mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrName(nsAString& aAttrName)
{
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
  if (mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrChange(uint16_t* aAttrChange)
{
  *aAttrChange = 0;
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
  if (mutation->mAttrChange)
      *aAttrChange = mutation->mAttrChange;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::InitMutationEvent(const nsAString& aTypeArg, bool aCanBubbleArg, bool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAString& aPrevValueArg, const nsAString& aNewValueArg, const nsAString& aAttrNameArg, uint16_t aAttrChangeArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsMutationEvent* mutation = static_cast<nsMutationEvent*>(mEvent);
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
                                nsMutationEvent *aEvent) 
{
  nsDOMMutationEvent* it = new nsDOMMutationEvent(aOwner, aPresContext, aEvent);
  if (nullptr == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
