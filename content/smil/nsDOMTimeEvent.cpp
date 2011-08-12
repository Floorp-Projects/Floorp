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
 * The Original Code is the Mozilla SMIL Module.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "nsDOMTimeEvent.h"
#include "nsGUIEvent.h"
#include "nsPresContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDOMClassInfoID.h"

nsDOMTimeEvent::nsDOMTimeEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent ? aEvent : new nsUIEvent(PR_FALSE, 0, 0)),
    mDetail(0)
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
    mEvent->eventStructType = NS_SMIL_TIME_EVENT;
  }

  if (mEvent->eventStructType == NS_SMIL_TIME_EVENT) {
    nsUIEvent* event = static_cast<nsUIEvent*>(mEvent);
    mDetail = event->detail;
  }

  mEvent->flags |= NS_EVENT_FLAG_CANT_BUBBLE |
                   NS_EVENT_FLAG_CANT_CANCEL;

  if (mPresContext) {
    nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
    if (container) {
      nsCOMPtr<nsIDOMWindow> window = do_GetInterface(container);
      if (window) {
        mView = do_QueryInterface(window);
      }
    }
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMTimeEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMTimeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mView)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMTimeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mView)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMTimeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTimeEvent, nsDOMEvent)

DOMCI_DATA(TimeEvent, nsDOMTimeEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMTimeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTimeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TimeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMTimeEvent::GetView(nsIDOMWindow** aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTimeEvent::GetDetail(PRInt32* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTimeEvent::InitTimeEvent(const nsAString& aTypeArg,
                              nsIDOMWindow* aViewArg,
                              PRInt32 aDetailArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, PR_FALSE /*doesn't bubble*/,
                                                PR_FALSE /*can't cancel*/);
  NS_ENSURE_SUCCESS(rv, rv);

  mDetail = aDetailArg;
  mView = aViewArg;

  return NS_OK;
}

nsresult NS_NewDOMTimeEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsEvent* aEvent)
{
  nsDOMTimeEvent* it = new nsDOMTimeEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
