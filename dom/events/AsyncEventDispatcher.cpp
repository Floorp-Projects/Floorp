/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/DocumentInlines.h"
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
    : CancelableRunnable("AsyncEventDispatcher"),
      mTarget(aTarget),
      mEventMessage(eUnidentifiedEvent) {
  MOZ_ASSERT(mTarget);
  RefPtr<Event> event =
      EventDispatcher::CreateEvent(aTarget, nullptr, &aEvent, u""_ns);
  mEvent = std::move(event);
  mEventType.SetIsVoid(true);
  NS_ASSERTION(mEvent, "Should never fail to create an event");
  mEvent->DuplicatePrivateData();
  mEvent->SetTrusted(aEvent.IsTrusted());
}

NS_IMETHODIMP
AsyncEventDispatcher::Run() {
  if (mCanceled) {
    return NS_OK;
  }
  nsINode* node = nsINode::FromEventTargetOrNull(mTarget);
  if (mCheckStillInDoc) {
    MOZ_ASSERT(node);
    if (!node->IsInComposedDoc()) {
      return NS_OK;
    }
  }
  mTarget->AsyncEventRunning(this);
  if (mEventMessage != eUnidentifiedEvent) {
    MOZ_ASSERT(mComposed == Composed::eDefault);
    return nsContentUtils::DispatchTrustedEvent<WidgetEvent>(
        node->OwnerDoc(), mTarget, mEventMessage, mCanBubble, Cancelable::eNo,
        nullptr /* aDefaultAction */, mOnlyChromeDispatch);
  }
  // MOZ_KnownLives because this instance shouldn't be touched while running.
  if (mEvent) {
    DispatchEventOnTarget(MOZ_KnownLive(mTarget), MOZ_KnownLive(mEvent),
                          mOnlyChromeDispatch, mComposed);
  } else {
    DispatchEventOnTarget(MOZ_KnownLive(mTarget), mEventType, mCanBubble,
                          mOnlyChromeDispatch, mComposed);
  }
  return NS_OK;
}

// static
void AsyncEventDispatcher::DispatchEventOnTarget(
    EventTarget* aTarget, const nsAString& aEventType, CanBubble aCanBubble,
    ChromeOnlyDispatch aOnlyChromeDispatch, Composed aComposed) {
  RefPtr<Event> event = NS_NewDOMEvent(aTarget, nullptr, nullptr);
  event->InitEvent(aEventType, aCanBubble, Cancelable::eNo);
  event->SetTrusted(true);
  DispatchEventOnTarget(aTarget, event, aOnlyChromeDispatch, aComposed);
}

// static
void AsyncEventDispatcher::DispatchEventOnTarget(
    EventTarget* aTarget, Event* aEvent, ChromeOnlyDispatch aOnlyChromeDispatch,
    Composed aComposed) {
  if (aComposed != Composed::eDefault) {
    aEvent->WidgetEventPtr()->mFlags.mComposed = aComposed == Composed::eYes;
  }
  if (aOnlyChromeDispatch == ChromeOnlyDispatch::eYes) {
    MOZ_ASSERT(aEvent->IsTrusted());
    aEvent->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  }
  aTarget->DispatchEvent(*aEvent);
}

nsresult AsyncEventDispatcher::Cancel() {
  mCanceled = true;
  return NS_OK;
}

nsresult AsyncEventDispatcher::PostDOMEvent() {
  RefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  return NS_DispatchToCurrentThread(ensureDeletionWhenFailing.forget());
}

void AsyncEventDispatcher::RunDOMEventWhenSafe() {
  RefPtr<AsyncEventDispatcher> ensureDeletionWhenFailing = this;
  nsContentUtils::AddScriptRunner(ensureDeletionWhenFailing.forget());
}

// static
void AsyncEventDispatcher::RunDOMEventWhenSafe(
    EventTarget& aTarget, const nsAString& aEventType, CanBubble aCanBubble,
    ChromeOnlyDispatch aOnlyChromeDispatch /* = ChromeOnlyDispatch::eNo */,
    Composed aComposed /* = Composed::eDefault */) {
  if (nsContentUtils::IsSafeToRunScript()) {
    OwningNonNull<EventTarget> target = aTarget;
    DispatchEventOnTarget(target, aEventType, aCanBubble, aOnlyChromeDispatch,
                          aComposed);
    return;
  }
  (new AsyncEventDispatcher(&aTarget, aEventType, aCanBubble,
                            aOnlyChromeDispatch, aComposed))
      ->RunDOMEventWhenSafe();
}

void AsyncEventDispatcher::RunDOMEventWhenSafe(
    EventTarget& aTarget, Event& aEvent,
    ChromeOnlyDispatch aOnlyChromeDispatch /* = ChromeOnlyDispatch::eNo */) {
  if (nsContentUtils::IsSafeToRunScript()) {
    DispatchEventOnTarget(&aTarget, &aEvent, aOnlyChromeDispatch,
                          Composed::eDefault);
    return;
  }
  (new AsyncEventDispatcher(&aTarget, do_AddRef(&aEvent), aOnlyChromeDispatch))
      ->RunDOMEventWhenSafe();
}

// static
nsresult AsyncEventDispatcher::RunDOMEventWhenSafe(
    nsINode& aTarget, WidgetEvent& aEvent,
    nsEventStatus* aEventStatus /* = nullptr */) {
  if (nsContentUtils::IsSafeToRunScript()) {
    // MOZ_KnownLive due to bug 1832202
    nsPresContext* presContext = aTarget.OwnerDoc()->GetPresContext();
    return EventDispatcher::Dispatch(MOZ_KnownLive(&aTarget),
                                     MOZ_KnownLive(presContext), &aEvent,
                                     nullptr, aEventStatus);
  }
  (new AsyncEventDispatcher(&aTarget, aEvent))->RunDOMEventWhenSafe();
  return NS_OK;
}

void AsyncEventDispatcher::RequireNodeInDocument() {
  MOZ_ASSERT(mTarget);
  MOZ_ASSERT(mTarget->IsNode());
  mCheckStillInDoc = true;
}

}  // namespace mozilla
