/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ClipboardEvent.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsIClipboard.h"

namespace mozilla::dom {

ClipboardEvent::ClipboardEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                               InternalClipboardEvent* aEvent)
    : Event(aOwner, aPresContext,
            aEvent ? aEvent : new InternalClipboardEvent(false, eVoidEvent)) {
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }
}

void ClipboardEvent::InitClipboardEvent(const nsAString& aType, bool aCanBubble,
                                        bool aCancelable,
                                        DataTransfer* aClipboardData) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, aCanBubble, aCancelable);
  mEvent->AsClipboardEvent()->mClipboardData = aClipboardData;
}

already_AddRefed<ClipboardEvent> ClipboardEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const ClipboardEventInit& aParam, ErrorResult& aRv) {
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
      clipboardData->SetData(aParam.mDataType, aParam.mData,
                             *aGlobal.GetSubjectPrincipal(), aRv);
      NS_ENSURE_TRUE(!aRv.Failed(), nullptr);
    }
  }

  e->InitClipboardEvent(aType, aParam.mBubbles, aParam.mCancelable,
                        clipboardData);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

DataTransfer* ClipboardEvent::GetClipboardData() {
  InternalClipboardEvent* event = mEvent->AsClipboardEvent();

  if (!event->mClipboardData) {
    if (mEventIsInternal) {
      event->mClipboardData =
          new DataTransfer(ToSupports(this), eCopy, false, -1);
    } else {
      event->mClipboardData = new DataTransfer(
          ToSupports(this), event->mMessage, event->mMessage == ePaste,
          nsIClipboard::kGlobalClipboard);
    }
  }

  return event->mClipboardData;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<ClipboardEvent> NS_NewDOMClipboardEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    InternalClipboardEvent* aEvent) {
  RefPtr<ClipboardEvent> it = new ClipboardEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
