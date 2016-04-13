/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AsyncEventDispatcher_h_
#define mozilla_AsyncEventDispatcher_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
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
  AsyncEventDispatcher(nsINode* aTarget, const nsAString& aEventType,
                       bool aBubbles, bool aOnlyChromeDispatch)
    : mTarget(aTarget)
    , mEventType(aEventType)
    , mBubbles(aBubbles)
    , mOnlyChromeDispatch(aOnlyChromeDispatch)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, const nsAString& aEventType,
                       bool aBubbles)
    : mTarget(aTarget)
    , mEventType(aEventType)
    , mBubbles(aBubbles)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, nsIDOMEvent* aEvent)
    : mTarget(aTarget)
    , mEvent(aEvent)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, WidgetEvent& aEvent);

  NS_IMETHOD Run() override;
  nsresult Cancel() override;
  nsresult PostDOMEvent();
  void RunDOMEventWhenSafe();

  nsCOMPtr<dom::EventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
  nsString              mEventType;
  bool                  mBubbles = false;
  bool                  mOnlyChromeDispatch = false;
  bool                  mCanceled = false;
};

class LoadBlockingAsyncEventDispatcher final : public AsyncEventDispatcher
{
public:
  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode,
                                   const nsAString& aEventType,
                                   bool aBubbles, bool aDispatchChromeOnly)
    : AsyncEventDispatcher(aEventNode, aEventType,
                           aBubbles, aDispatchChromeOnly)
    , mBlockedDoc(aEventNode->OwnerDoc())
  {
    if (mBlockedDoc) {
      mBlockedDoc->BlockOnload();
    }
  }

  LoadBlockingAsyncEventDispatcher(nsINode* aEventNode, nsIDOMEvent* aEvent)
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
