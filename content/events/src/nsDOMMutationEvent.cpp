/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsContentUtils.h"

class nsPresContext;

nsDOMMutationEvent::nsDOMMutationEvent(nsPresContext* aPresContext, nsMutationEvent* aEvent)
  :nsDOMEvent(aPresContext, aEvent ? aEvent : new nsMutationEvent())
{  
  if ( aEvent ) {
    mEventIsInternal = PR_FALSE;
    nsMutationEvent* mutation = (nsMutationEvent*)aEvent;
    SetTarget(mutation->mTarget);
  }
  else
  {
    mEventIsInternal = PR_TRUE;
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(MutationEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMMutationEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMutationEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMMutationEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  *aRelatedNode = nsnull;
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  *aRelatedNode = mutation->mRelatedNode;
  NS_IF_ADDREF(*aRelatedNode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetPrevValue(nsAString& aPrevValue)
{
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  if (mutation->mPrevAttrValue)
    mutation->mPrevAttrValue->ToString(aPrevValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetNewValue(nsAString& aNewValue)
{
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  if (mutation->mNewAttrValue)
      mutation->mNewAttrValue->ToString(aNewValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrName(nsAString& aAttrName)
{
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  if (mutation->mAttrName)
      mutation->mAttrName->ToString(aAttrName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::GetAttrChange(PRUint16* aAttrChange)
{
  *aAttrChange = 0;
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
  if (mutation->mAttrChange)
      *aAttrChange = mutation->mAttrChange;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMutationEvent::InitMutationEvent(const nsAString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAString& aPrevValueArg, const nsAString& aNewValueArg, const nsAString& aAttrNameArg, PRUint16 aAttrChangeArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsMutationEvent* mutation = NS_STATIC_CAST(nsMutationEvent*, mEvent);
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
                                nsPresContext* aPresContext,
                                nsMutationEvent *aEvent) 
{
  nsDOMMutationEvent* it = new nsDOMMutationEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
