/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMCompositionEvent.h"
#include "nsDOMClassInfo.h"

nsDOMCompositionEvent::nsDOMCompositionEvent(nsPresContext* aPresContext,
                                             nsCompositionEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsCompositionEvent(PR_FALSE, 0, nsnull))
{
  NS_ASSERTION(mEvent->eventStructType == NS_COMPOSITION_EVENT,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();

    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sence for us, we cannot cancel composition
    //     when we sends compositionstart event.
    mEvent->flags |= NS_EVENT_FLAG_CANT_CANCEL;
  }

  mData = static_cast<nsCompositionEvent*>(mEvent)->data;
  // TODO: Native event should have locale information.
}

nsDOMCompositionEvent::~nsDOMCompositionEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsCompositionEvent*>(mEvent);
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMCompositionEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMCompositionEvent, nsDOMUIEvent)

DOMCI_DATA(CompositionEvent, nsDOMCompositionEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMCompositionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCompositionEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CompositionEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMCompositionEvent::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCompositionEvent::GetLocale(nsAString& aLocale)
{
  aLocale = mLocale;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCompositionEvent::InitCompositionEvent(const nsAString& aType,
                                            bool aCanBubble,
                                            bool aCancelable,
                                            nsIDOMWindow* aView,
                                            const nsAString& aData,
                                            const nsAString& aLocale)
{
  nsresult rv =
    nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mLocale = aLocale;
  return NS_OK;
}

nsresult
NS_NewDOMCompositionEvent(nsIDOMEvent** aInstancePtrResult,
                          nsPresContext* aPresContext,
                          nsCompositionEvent *aEvent)
{
  nsDOMCompositionEvent* event =
    new nsDOMCompositionEvent(aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
