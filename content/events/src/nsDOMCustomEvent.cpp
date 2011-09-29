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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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

#include "nsDOMCustomEvent.h"
#include "nsContentUtils.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMCustomEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMCustomEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDetail)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMCustomEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDetail)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(CustomEvent, nsDOMCustomEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMCustomEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCustomEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CustomEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMCustomEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMCustomEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMCustomEvent::GetDetail(nsIVariant** aDetail)
{
  NS_IF_ADDREF(*aDetail = mDetail);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCustomEvent::InitCustomEvent(const nsAString& aType,
                                  bool aCanBubble,
                                  bool aCancelable,
                                  nsIVariant* aDetail)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mDetail = aDetail;
  return NS_OK;
}

nsresult
NS_NewDOMCustomEvent(nsIDOMEvent** aInstancePtrResult,
                     nsPresContext* aPresContext,
                     nsEvent* aEvent) 
{
  nsDOMCustomEvent* e = new nsDOMCustomEvent(aPresContext, aEvent);
  return CallQueryInterface(e, aInstancePtrResult);
}
