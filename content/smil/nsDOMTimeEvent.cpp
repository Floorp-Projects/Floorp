/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTimeEvent.h"
#include "nsGUIEvent.h"
#include "nsPresContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDOMClassInfoID.h"

nsDOMTimeEvent::nsDOMTimeEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent ? aEvent : new nsUIEvent(false, 0, 0)),
    mDetail(0)
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->eventStructType = NS_SMIL_TIME_EVENT;
  }

  if (mEvent->eventStructType == NS_SMIL_TIME_EVENT) {
    nsUIEvent* event = static_cast<nsUIEvent*>(mEvent);
    mDetail = event->detail;
  }

  mEvent->mFlags.mBubbles = false;
  mEvent->mFlags.mCancelable = false;

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

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMTimeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mView)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMTimeEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mView)
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
nsDOMTimeEvent::GetDetail(int32_t* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTimeEvent::InitTimeEvent(const nsAString& aTypeArg,
                              nsIDOMWindow* aViewArg,
                              int32_t aDetailArg)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, false /*doesn't bubble*/,
                                                false /*can't cancel*/);
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
