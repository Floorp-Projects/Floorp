/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIDOMMutationEvent.h"
#include "nsDOMEvent.h"
#include "nsMutationEvent.h"

class nsIPresContext;

class nsDOMMutationEvent : public nsIDOMMutationEvent, public nsDOMEvent
{
  NS_DECL_IDOMMUTATIONEVENT
  NS_FORWARD_IDOMEVENT(nsDOMEvent::)
  
  NS_DECL_ISUPPORTS_INHERITED

  nsDOMMutationEvent(nsIPresContext* aPresContext, 
                     nsEvent* aEvent);

  ~nsDOMMutationEvent();

};

nsDOMMutationEvent::nsDOMMutationEvent(nsIPresContext* aPresContext,
                                       nsEvent* aEvent)
:nsDOMEvent(aPresContext, aEvent, NS_LITERAL_STRING("MutationEvent")) 
{  
  nsMutationEvent* mutation = (nsMutationEvent*)aEvent;
  SetTarget(mutation->mTarget);
}

nsDOMMutationEvent::~nsDOMMutationEvent() {
  
}

NS_IMPL_ADDREF_INHERITED(nsDOMMutationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMutationEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEvent, nsIDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationEvent)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsDOMMutationEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  *aRelatedNode = nsnull;
  if (mEvent) {
    nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
    *aRelatedNode = mutation->mRelatedNode;
    NS_IF_ADDREF(*aRelatedNode);
  }
  else *aRelatedNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetPrevValue(nsAWritableString& aPrevValue)
{
  if (mEvent) {
    nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
    if (mutation && mutation->mPrevAttrValue)
      mutation->mPrevAttrValue->ToString(aPrevValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetNewValue(nsAWritableString& aNewValue)
{
  if (mEvent) {
    nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
    if (mutation && mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrName(nsAWritableString& aAttrName)
{
  if (mEvent) {
    nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
    if (mutation && mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrChange(PRUint16* aAttrChange)
{
  *aAttrChange = 0;
  if (mEvent) {
    nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
    if (mutation && mutation->mAttrChange)
      *aAttrChange = mutation->mAttrChange;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::InitMutationEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, 
                                      PRBool aCancelableArg, nsIDOMNode* aRelatedNodeArg, 
                                      const nsAReadableString& aPrevValueArg, 
                                      const nsAReadableString& aNewValueArg, 
                                      const nsAReadableString& aAttrNameArg)
{
  NS_ENSURE_SUCCESS(SetEventType(aTypeArg), NS_ERROR_FAILURE);
  mEvent->flags |= aCanBubbleArg ? NS_EVENT_FLAG_NONE : NS_EVENT_FLAG_CANT_BUBBLE;
  mEvent->flags |= aCancelableArg ? NS_EVENT_FLAG_NONE : NS_EVENT_FLAG_CANT_CANCEL;
  
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  if (mutation) {
    mutation->mRelatedNode = aRelatedNodeArg;
    if (!aPrevValueArg.IsEmpty())
      mutation->mPrevAttrValue = getter_AddRefs(NS_NewAtom(aPrevValueArg));
    if (!aNewValueArg.IsEmpty())
      mutation->mNewAttrValue = getter_AddRefs(NS_NewAtom(aNewValueArg));
    if (!aAttrNameArg.IsEmpty()) {
      mutation->mAttrName = getter_AddRefs(NS_NewAtom(aAttrNameArg));
      // I guess we assume modification. Weird that this isn't specifiable.
      mutation->mAttrChange = nsIDOMMutationEvent::MODIFICATION;
    }
  }
    
  return NS_OK;
};

nsresult NS_NewDOMMutationEvent(nsIDOMEvent** aInstancePtrResult,
                                nsIPresContext* aPresContext,
                                nsEvent *aEvent) 
{
  nsDOMMutationEvent* it = new nsDOMMutationEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return it->QueryInterface(NS_GET_IID(nsIDOMEvent), (void **) aInstancePtrResult);
}
