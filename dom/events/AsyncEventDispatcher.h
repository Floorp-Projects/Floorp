/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AsyncEventDispatcher_h_
#define mozilla_AsyncEventDispatcher_h_

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Document.h"
#include "nsString.h"
#include "nsThreadUtils.h"

class nsINode;

namespace mozilla {

/**
 * Use AsyncEventDispatcher to fire a DOM event that requires safe a stable DOM.
 * For example, you may need to fire an event from within layout, but
 * want to ensure that the event handler doesn't mutate the DOM at
 * the wrong time, in order to avoid resulting instability.
 */

class AsyncEventDispatcher : public CancelableRunnable {
 public:
  /**
   * If aOnlyChromeDispatch is true, the event is dispatched to only
   * chrome node. In that case, if aTarget is already a chrome node,
   * the event is dispatched to it, otherwise the dispatch path starts
   * at the first chrome ancestor of that target.
   */
  AsyncEventDispatcher(
      dom::EventTarget* aTarget, const nsAString& aEventType,
      CanBubble aCanBubble,
      ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo,
      Composed aComposed = Composed::eDefault)
      : CancelableRunnable("AsyncEventDispatcher"),
        mTarget(aTarget),
        mEventType(aEventType),
        mEventMessage(eUnidentifiedEvent),
        mCanBubble(aCanBubble),
        mOnlyChromeDispatch(aOnlyChromeDispatch),
        mComposed(aComposed) {}

  /**
   * If aOnlyChromeDispatch is true, the event is dispatched to only
   * chrome node. In that case, if aTarget is already a chrome node,
   * the event is dispatched to it, otherwise the dispatch path starts
   * at the first chrome ancestor of that target.
   */
  AsyncEventDispatcher(nsINode* aTarget, mozilla::EventMessage aEventMessage,
                       CanBubble aCanBubble,
                       ChromeOnlyDispatch aOnlyChromeDispatch)
      : CancelableRunnable("AsyncEventDispatcher"),
        mTarget(aTarget),
        mEventMessage(aEventMessage),
        mCanBubble(aCanBubble),
        mOnlyChromeDispatch(aOnlyChromeDispatch) {
    mEventType.SetIsVoid(true);
    MOZ_ASSERT(mEventMessage != eUnidentifiedEvent);
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget,
                       mozilla::EventMessage aEventMessage,
                       CanBubble aCanBubble)
      : CancelableRunnable("AsyncEventDispatcher"),
        mTarget(aTarget),
        mEventMessage(aEventMessage),
        mCanBubble(aCanBubble) {
    mEventType.SetIsVoid(true);
    MOZ_ASSERT(mEventMessage != eUnidentifiedEvent);
  }

  /**
   * aEvent must have been created without Widget*Event and Internal*Event
   * because this constructor assumes that it's safe to use aEvent
   * asynchronously (i.e., after all objects allocated in the stack are
   * destroyed).
   */
  AsyncEventDispatcher(
      dom::EventTarget* aTarget, already_AddRefed<dom::Event> aEvent,
      ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo)
      : CancelableRunnable("AsyncEventDispatcher"),
        mTarget(aTarget),
        mEvent(aEvent),
        mEventMessage(eUnidentifiedEvent),
        mOnlyChromeDispatch(aOnlyChromeDispatch) {
    MOZ_ASSERT(
        mEvent->IsSafeToBeDispatchedAsynchronously(),
        "The DOM event should be created without Widget*Event and "
        "Internal*Event "
        "because if it needs to be safe to be dispatched asynchronously");
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, WidgetEvent& aEvent);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override;
  nsresult Cancel() override;
  nsresult PostDOMEvent();

  /**
   * Dispatch event immediately if it's safe to dispatch the event.
   * Otherwise, posting the event into the queue to dispatch it when it's safe.
   *
   * Note that this method allows callers to call itself with unsafe aTarget
   * because its lifetime is guaranteed by this method (in the case of
   * synchronous dispatching) or AsyncEventDispatcher (in the case of
   * asynchronous dispatching).
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static void RunDOMEventWhenSafe(
      dom::EventTarget& aTarget, const nsAString& aEventType,
      CanBubble aCanBubble,
      ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo,
      Composed aComposed = Composed::eDefault);

  /**
   * Dispatch event immediately if it's safe to dispatch the event.
   * Otherwise, posting the event into the queue to dispatch it when it's safe.
   *
   * NOTE: Only this is now marked as MOZ_CAN_RUN_SCRIPT because all callers
   * have already had safe smart pointers for both aTarget and aEvent.
   * If you need to use this in a hot path without smart pointers, you may need
   * to create unsafe version of this method for avoiding the extra add/release
   * refcount cost in the case of asynchronous dispatching.
   */
  MOZ_CAN_RUN_SCRIPT static void RunDOMEventWhenSafe(
      dom::EventTarget& aTarget, dom::Event& aEvent,
      ChromeOnlyDispatch aOnlyChromeDispatch = ChromeOnlyDispatch::eNo);

  /**
   * Dispatch event immediately if it's safe to dispatch the event.
   * Otherwise, posting the event into the queue to dispatch it when it's safe.
   *
   * Note that this method allows callers to call itself with unsafe aTarget
   * because its lifetime is guaranteed by EventDispatcher (in the case of
   * synchronous dispatching) or AsyncEventDispatcher (in the case of
   * asynchronous dispatching).
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static nsresult RunDOMEventWhenSafe(
      nsINode& aTarget, WidgetEvent& aEvent,
      nsEventStatus* aEventStatus = nullptr);

  // Calling this causes the Run() method to check that
  // mTarget->IsInComposedDoc(). mTarget must be an nsINode or else we'll
  // assert.
  void RequireNodeInDocument();

  // NOTE: The static version of this should be preferred when possible, because
  // it avoids the allocation but this is useful when used in combination with
  // LoadBlockingAsyncEventDispatcher.
  void RunDOMEventWhenSafe();

 protected:
  MOZ_CAN_RUN_SCRIPT static void DispatchEventOnTarget(
      dom::EventTarget* aTarget, dom::Event* aEvent,
      ChromeOnlyDispatch aOnlyChromeDispatch, Composed aComposed);
  MOZ_CAN_RUN_SCRIPT static void DispatchEventOnTarget(
      dom::EventTarget* aTarget, const nsAString& aEventType,
      CanBubble aCanBubble, ChromeOnlyDispatch aOnlyChromeDispatch,
      Composed aComposed);

 public:
  nsCOMPtr<dom::EventTarget> mTarget;
  RefPtr<dom::Event> mEvent;
  // If mEventType is set, mEventMessage will be eUnidentifiedEvent.
  // If mEventMessage is set, mEventType will be void.
  // They can never both be set at the same time.
  nsString mEventType;
  EventMessage mEventMessage;
  CanBubble mCanBubble = CanBubble::eNo;
  ChromeOnlyDispatch mOnlyChromeDispatch = ChromeOnlyDispatch::eNo;
  Composed mComposed = Composed::eDefault;
  bool mCanceled = false;
  bool mCheckStillInDoc = false;
};

class LoadBlockingAsyncEventDispatcher final : public AsyncEventDispatcher {
 public:
  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode,
                                   const nsAString& aEventType,
                                   CanBubble aBubbles,
                                   ChromeOnlyDispatch aDispatchChromeOnly)
      : AsyncEventDispatcher(aEventNode, aEventType, aBubbles,
                             aDispatchChromeOnly),
        mBlockedDoc(aEventNode->OwnerDoc()) {
    mBlockedDoc->BlockOnload();
  }

  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode,
                                   already_AddRefed<dom::Event> aEvent)
      : AsyncEventDispatcher(aEventNode, std::move(aEvent)),
        mBlockedDoc(aEventNode->OwnerDoc()) {
    mBlockedDoc->BlockOnload();
  }

  ~LoadBlockingAsyncEventDispatcher() { mBlockedDoc->UnblockOnload(true); }

 private:
  RefPtr<dom::Document> mBlockedDoc;
};

}  // namespace mozilla

#endif  // mozilla_AsyncEventDispatcher_h_
