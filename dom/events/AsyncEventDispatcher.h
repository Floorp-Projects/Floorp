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
#include "nsIDocument.h"
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

class AsyncEventDispatcher : public CancelableRunnable
{
public:
  /**
   * If aOnlyChromeDispatch is true, the event is dispatched to only
   * chrome node. In that case, if aTarget is already a chrome node,
   * the event is dispatched to it, otherwise the dispatch path starts
   * at the first chrome ancestor of that target.
   */
  AsyncEventDispatcher(nsINode* aTarget,
                       const nsAString& aEventType,
                       CanBubble aCanBubble,
                       ChromeOnlyDispatch aOnlyChromeDispatch,
                       Composed aComposed = Composed::eDefault)
    : CancelableRunnable("AsyncEventDispatcher")
    , mTarget(aTarget)
    , mEventType(aEventType)
    , mEventMessage(eUnidentifiedEvent)
    , mCanBubble(aCanBubble)
    , mOnlyChromeDispatch(aOnlyChromeDispatch)
    , mComposed(aComposed)
  {
  }

  /**
   * If aOnlyChromeDispatch is true, the event is dispatched to only
   * chrome node. In that case, if aTarget is already a chrome node,
   * the event is dispatched to it, otherwise the dispatch path starts
   * at the first chrome ancestor of that target.
   */
  AsyncEventDispatcher(nsINode* aTarget,
                       mozilla::EventMessage aEventMessage,
                       CanBubble aCanBubble,
                       ChromeOnlyDispatch aOnlyChromeDispatch)
    : CancelableRunnable("AsyncEventDispatcher")
    , mTarget(aTarget)
    , mEventMessage(aEventMessage)
    , mCanBubble(aCanBubble)
    , mOnlyChromeDispatch(aOnlyChromeDispatch)
  {
    mEventType.SetIsVoid(true);
    MOZ_ASSERT(mEventMessage != eUnidentifiedEvent);
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget,
                       const nsAString& aEventType,
                       CanBubble aCanBubble)
    : CancelableRunnable("AsyncEventDispatcher")
    , mTarget(aTarget)
    , mEventType(aEventType)
    , mEventMessage(eUnidentifiedEvent)
    , mCanBubble(aCanBubble)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget,
                       mozilla::EventMessage aEventMessage,
                       CanBubble aCanBubble)
    : CancelableRunnable("AsyncEventDispatcher")
    , mTarget(aTarget)
    , mEventMessage(aEventMessage)
    , mCanBubble(aCanBubble)
  {
    mEventType.SetIsVoid(true);
    MOZ_ASSERT(mEventMessage != eUnidentifiedEvent);
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, dom::Event* aEvent)
    : CancelableRunnable("AsyncEventDispatcher")
    , mTarget(aTarget)
    , mEvent(aEvent)
    , mEventMessage(eUnidentifiedEvent)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, WidgetEvent& aEvent);

  NS_IMETHOD Run() override;
  nsresult Cancel() override;
  nsresult PostDOMEvent();
  void RunDOMEventWhenSafe();

  // Calling this causes the Run() method to check that
  // mTarget->IsInComposedDoc(). mTarget must be an nsINode or else we'll
  // assert.
  void RequireNodeInDocument();

  nsCOMPtr<dom::EventTarget> mTarget;
  RefPtr<dom::Event> mEvent;
  // If mEventType is set, mEventMessage will be eUnidentifiedEvent.
  // If mEventMessage is set, mEventType will be void.
  // They can never both be set at the same time.
  nsString              mEventType;
  EventMessage          mEventMessage;
  CanBubble             mCanBubble = CanBubble::eNo;
  ChromeOnlyDispatch    mOnlyChromeDispatch = ChromeOnlyDispatch::eNo;
  Composed              mComposed = Composed::eDefault;
  bool                  mCanceled = false;
  bool                  mCheckStillInDoc = false;
};

class LoadBlockingAsyncEventDispatcher final : public AsyncEventDispatcher
{
public:
  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode,
                                   const nsAString& aEventType,
                                   CanBubble aBubbles,
                                   ChromeOnlyDispatch aDispatchChromeOnly)
    : AsyncEventDispatcher(aEventNode,
                           aEventType,
                           aBubbles,
                           aDispatchChromeOnly)
    , mBlockedDoc(aEventNode->OwnerDoc())
  {
    if (mBlockedDoc) {
      mBlockedDoc->BlockOnload();
    }
  }

  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode, dom::Event* aEvent)
    : AsyncEventDispatcher(aEventNode, aEvent)
    , mBlockedDoc(aEventNode->OwnerDoc())
  {
    if (mBlockedDoc) {
      mBlockedDoc->BlockOnload();
    }
  }

  ~LoadBlockingAsyncEventDispatcher();

private:
  nsCOMPtr<nsIDocument> mBlockedDoc;
};

} // namespace mozilla

#endif // mozilla_AsyncEventDispatcher_h_
