/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "mozilla/dom/EventTarget.h"
#include "nsContentUtils.h"
#include "nsIDOMEvent.h"

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::AsyncEventDispatcher
 ******************************************************************************/

AsyncEventDispatcher::AsyncEventDispatcher(EventTarget* aTarget,
                                           WidgetEvent& aEvent)
  : mTarget(aTarget)
  , mOnlyChromeDispatch(false)
{
  MOZ_ASSERT(mTarget);
  EventDispatcher::CreateEvent(aTarget, nullptr, &aEvent, EmptyString(),
                               getter_AddRefs(mEvent));
  NS_ASSERTION(mEvent, "Should never fail to create an event");
  mEvent->DuplicatePrivateData();
  mEvent->SetTrusted(aEvent.mFlags.mIsTrusted);
}

NS_IMETHODIMP
AsyncEventDispatcher::Run()
{
  nsCOMPtr<nsIDOMEvent> event = mEvent;
  if (!event) {
    NS_NewDOMEvent(getter_AddRefs(event), mTarget, nullptr, nullptr);
    nsresult rv = event->InitEvent(mEventType, mBubbles, false);
    NS_ENSURE_SUCCESS(rv, rv);
    event->SetTrusted(true);
  }
  if (mOnlyChromeDispatch) {
    MOZ_ASSERT(event->InternalDOMEvent()->IsTrusted());
    event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  }
  bool dummy;
  mTarget->DispatchEvent(event, &dummy);
  return NS_OK;
}

nsresult
AsyncEventDispatcher::PostDOMEvent()
{
  nsRefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  return NS_DispatchToCurrentThread(this);
}

void
AsyncEventDispatcher::RunDOMEventWhenSafe()
{
  nsRefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  nsContentUtils::AddScriptRunner(this);
}

/******************************************************************************
 * mozilla::LoadBlockingAsyncEventDispatcher
 ******************************************************************************/

LoadBlockingAsyncEventDispatcher::~LoadBlockingAsyncEventDispatcher()
{
  if (mBlockedDoc) {
    mBlockedDoc->UnblockOnload(true);
  }
}

} // namespace mozilla
