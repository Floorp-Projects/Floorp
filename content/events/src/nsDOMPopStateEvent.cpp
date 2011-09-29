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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
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

#include "nsDOMPopStateEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMPopStateEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMPopStateEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMPopStateEvent, nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMPopStateEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mState)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMPopStateEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mState)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(PopStateEvent, nsDOMPopStateEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMPopStateEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPopStateEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PopStateEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsDOMPopStateEvent::~nsDOMPopStateEvent()
{
}

NS_IMETHODIMP
nsDOMPopStateEvent::GetState(nsIVariant **aState)
{
  NS_PRECONDITION(aState, "null state arg");
  NS_IF_ADDREF(*aState = mState);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMPopStateEvent::InitPopStateEvent(const nsAString &aTypeArg,
                                      bool aCanBubbleArg,
                                      bool aCancelableArg,
                                      nsIVariant *aStateArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mState = aStateArg;
  return NS_OK;
}

nsresult NS_NewDOMPopStateEvent(nsIDOMEvent** aInstancePtrResult,
                                nsPresContext* aPresContext,
                                nsEvent* aEvent)
{
  nsDOMPopStateEvent* event =
    new nsDOMPopStateEvent(aPresContext, aEvent);

  if (!event) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(event, aInstancePtrResult);
}
