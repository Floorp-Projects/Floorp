/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClipboardEvent.h"
#include "nsContentUtils.h"
#include "nsClientRect.h"
#include "nsDOMDataTransfer.h"
#include "nsIClipboard.h"

nsDOMClipboardEvent::nsDOMClipboardEvent(mozilla::dom::EventTarget* aOwner,
                                         nsPresContext* aPresContext,
                                         nsClipboardEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent ? aEvent :
               new nsClipboardEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

nsDOMClipboardEvent::~nsDOMClipboardEvent()
{
  if (mEventIsInternal && mEvent->eventStructType == NS_CLIPBOARD_EVENT) {
    delete static_cast<nsClipboardEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMClipboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClipboardEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMClipboardEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMClipboardEvent, nsDOMEvent)

nsresult
nsDOMClipboardEvent::InitClipboardEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                        nsIDOMDataTransfer* clipboardData)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  nsClipboardEvent* event = static_cast<nsClipboardEvent*>(mEvent);
  event->clipboardData = clipboardData;

  return NS_OK;
}

already_AddRefed<nsDOMClipboardEvent>
nsDOMClipboardEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                                 const nsAString& aType,
                                 const mozilla::dom::ClipboardEventInit& aParam,
                                 mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.Get());
  nsRefPtr<nsDOMClipboardEvent> e =
    new nsDOMClipboardEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  nsRefPtr<nsDOMDataTransfer> clipboardData;
  if (e->mEventIsInternal) {
    nsClipboardEvent* event = static_cast<nsClipboardEvent*>(e->mEvent);
    if (event) {
      // Always create a clipboardData for the copy event. If this is changed to
      // support other types of events, make sure that read/write privileges are
      // checked properly within nsDOMDataTransfer.
      clipboardData = new nsDOMDataTransfer(NS_COPY, false, -1);
      clipboardData->SetData(aParam.mDataType, aParam.mData);
    }
  }

  aRv = e->InitClipboardEvent(aType, aParam.mBubbles, aParam.mCancelable,
                              clipboardData);
  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
nsDOMClipboardEvent::GetClipboardData(nsIDOMDataTransfer** aClipboardData)
{
  NS_IF_ADDREF(*aClipboardData = GetClipboardData());
  return NS_OK;
}

nsIDOMDataTransfer*
nsDOMClipboardEvent::GetClipboardData()
{
  nsClipboardEvent* event = static_cast<nsClipboardEvent*>(mEvent);

  if (!event->clipboardData) {
    if (mEventIsInternal) {
      event->clipboardData = new nsDOMDataTransfer(NS_COPY, false, -1);
    } else {
      event->clipboardData =
        new nsDOMDataTransfer(event->message, event->message == NS_PASTE, nsIClipboard::kGlobalClipboard);
    }
  }

  return event->clipboardData;
}

nsresult NS_NewDOMClipboardEvent(nsIDOMEvent** aInstancePtrResult,
                                 mozilla::dom::EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 nsClipboardEvent *aEvent)
{
  nsDOMClipboardEvent* it =
    new nsDOMClipboardEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
