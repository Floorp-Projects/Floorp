/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ClipboardEvent.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsIClipboard.h"

namespace mozilla {
namespace dom {

ClipboardEvent::ClipboardEvent(EventTarget* aOwner,
                               nsPresContext* aPresContext,
                               InternalClipboardEvent* aEvent)
  : Event(aOwner, aPresContext,
          aEvent ? aEvent : new InternalClipboardEvent(false, eVoidEvent))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
  }
}

NS_INTERFACE_MAP_BEGIN(ClipboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClipboardEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(ClipboardEvent, Event)
NS_IMPL_RELEASE_INHERITED(ClipboardEvent, Event)

nsresult
ClipboardEvent::InitClipboardEvent(const nsAString& aType,
                                   bool aCanBubble,
                                   bool aCancelable,
                                   nsIDOMDataTransfer* aClipboardData)
{
  nsCOMPtr<DataTransfer> clipboardData = do_QueryInterface(aClipboardData);
  // Null clipboardData is OK

  ErrorResult rv;
  InitClipboardEvent(aType, aCanBubble, aCancelable, clipboardData);

  return rv.StealNSResult();
}

void
ClipboardEvent::InitClipboardEvent(const nsAString& aType, bool aCanBubble,
                                   bool aCancelable,
                                   DataTransfer* aClipboardData)
{
  Event::InitEvent(aType, aCanBubble, aCancelable);
  mEvent->AsClipboardEvent()->mClipboardData = aClipboardData;
}

already_AddRefed<ClipboardEvent>
ClipboardEvent::Constructor(const GlobalObject& aGlobal,
                            const nsAString& aType,
                            const ClipboardEventInit& aParam,
                            ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<ClipboardEvent> e = new ClipboardEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  RefPtr<DataTransfer> clipboardData;
  if (e->mEventIsInternal) {
    InternalClipboardEvent* event = e->mEvent->AsClipboardEvent();
    if (event) {
      // Always create a clipboardData for the copy event. If this is changed to
      // support other types of events, make sure that read/write privileges are
      // checked properly within DataTransfer.
      clipboardData = new DataTransfer(ToSupports(e), eCopy, false, -1);
      clipboardData->SetData(aParam.mDataType, aParam.mData, aRv);
      NS_ENSURE_TRUE(!aRv.Failed(), nullptr);
    }
  }

  e->InitClipboardEvent(aType, aParam.mBubbles, aParam.mCancelable,
                        clipboardData);
  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
ClipboardEvent::GetClipboardData(nsIDOMDataTransfer** aClipboardData)
{
  NS_IF_ADDREF(*aClipboardData = GetClipboardData());
  return NS_OK;
}

DataTransfer*
ClipboardEvent::GetClipboardData()
{
  InternalClipboardEvent* event = mEvent->AsClipboardEvent();

  if (!event->mClipboardData) {
    if (mEventIsInternal) {
      event->mClipboardData =
        new DataTransfer(ToSupports(this), eCopy, false, -1);
    } else {
      event->mClipboardData =
        new DataTransfer(ToSupports(this), event->mMessage,
                         event->mMessage == ePaste,
                         nsIClipboard::kGlobalClipboard);
    }
  }

  return event->mClipboardData;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<ClipboardEvent>
NS_NewDOMClipboardEvent(EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        InternalClipboardEvent* aEvent)
{
  RefPtr<ClipboardEvent> it =
    new ClipboardEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
