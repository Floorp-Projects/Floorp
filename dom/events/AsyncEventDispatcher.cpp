/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "nsContentUtils.h"

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::AsyncEventDispatcher
 ******************************************************************************/

AsyncEventDispatcher::AsyncEventDispatcher(EventTarget* aTarget,
                                           WidgetEvent& aEvent)
  : CancelableRunnable("AsyncEventDispatcher")
  , mTarget(aTarget)
  , mEventMessage(eUnidentifiedEvent)
{
  MOZ_ASSERT(mTarget);
  RefPtr<Event> event =
    EventDispatcher::CreateEvent(aTarget, nullptr, &aEvent, EmptyString());
  mEvent = event.forget();
  mEventType.SetIsVoid(true);
  NS_ASSERTION(mEvent, "Should never fail to create an event");
  mEvent->DuplicatePrivateData();
  mEvent->SetTrusted(aEvent.IsTrusted());
}

NS_IMETHODIMP
AsyncEventDispatcher::Run()
{
  if (mCanceled) {
    return NS_OK;
  }
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (mCheckStillInDoc) {
    MOZ_ASSERT(node);
    if (!node->IsInComposedDoc()) {
      return NS_OK;
    }
  }
  mTarget->AsyncEventRunning(this);
  if (mEventMessage != eUnidentifiedEvent) {
    MOZ_ASSERT(mComposed == Composed::eDefault);
    return nsContentUtils::DispatchTrustedEvent<WidgetEvent>
      (node->OwnerDoc(), mTarget, mEventMessage, mCanBubble,
       Cancelable::eNo, nullptr /* aDefaultAction */, mOnlyChromeDispatch);
  }
  RefPtr<Event> event = mEvent;
  if (!event) {
    event = NS_NewDOMEvent(mTarget, nullptr, nullptr);
    event->InitEvent(mEventType, mCanBubble, Cancelable::eNo);
    event->SetTrusted(true);
  }
  if (mComposed != Composed::eDefault) {
    event->WidgetEventPtr()->mFlags.mComposed =
      mComposed == Composed::eYes;
  }
  if (mOnlyChromeDispatch == ChromeOnlyDispatch::eYes) {
    MOZ_ASSERT(event->IsTrusted());
    event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  }
  mTarget->DispatchEvent(*event);
  return NS_OK;
}

nsresult
AsyncEventDispatcher::Cancel()
{
  mCanceled = true;
  return NS_OK;
}

nsresult
AsyncEventDispatcher::PostDOMEvent()
{
  RefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  if (NS_IsMainThread()) {
    if (nsCOMPtr<nsIGlobalObject> global = mTarget->GetOwnerGlobal()) {
      return global->Dispatch(TaskCategory::Other, ensureDeletionWhenFailing.forget());
    }

    // Sometimes GetOwnerGlobal returns null because it uses
    // GetScriptHandlingObject rather than GetScopeObject.
    if (nsCOMPtr<nsINode> node = do_QueryInterface(mTarget)) {
      nsCOMPtr<nsIDocument> doc = node->OwnerDoc();
      return doc->Dispatch(TaskCategory::Other, ensureDeletionWhenFailing.forget());
    }
  }
  return NS_DispatchToCurrentThread(this);
}

void
AsyncEventDispatcher::RunDOMEventWhenSafe()
{
  RefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  nsContentUtils::AddScriptRunner(this);
}

void
AsyncEventDispatcher::RequireNodeInDocument()
{
#ifdef DEBUG
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  MOZ_ASSERT(node);
#endif

  mCheckStillInDoc = true;
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
