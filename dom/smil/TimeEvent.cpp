/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/TimeEvent.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsPresContext.h"
#include "nsGlobalWindow.h"

namespace mozilla {
namespace dom {

TimeEvent::TimeEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     InternalSMILTimeEvent* aEvent)
  : Event(aOwner, aPresContext,
          aEvent ? aEvent : new InternalSMILTimeEvent(false, eVoidEvent))
  , mDetail(mEvent->AsSMILTimeEvent()->mDetail)
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }

  if (mPresContext) {
    nsCOMPtr<nsIDocShell> docShell = mPresContext->GetDocShell();
    if (docShell) {
      mView = docShell->GetWindow();
    }
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TimeEvent, Event,
                                   mView)

NS_IMPL_ADDREF_INHERITED(TimeEvent, Event)
NS_IMPL_RELEASE_INHERITED(TimeEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TimeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTimeEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMETHODIMP
TimeEvent::GetDetail(int32_t* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

void
TimeEvent::InitTimeEvent(const nsAString& aType, nsGlobalWindow* aView,
                         int32_t aDetail)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Event::InitEvent(aType, false /*doesn't bubble*/, false /*can't cancel*/);
  mDetail = aDetail;
  mView = aView ? aView->GetOuterWindow() : nullptr;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<TimeEvent>
NS_NewDOMTimeEvent(EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   InternalSMILTimeEvent* aEvent)
{
  RefPtr<TimeEvent> it = new TimeEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
