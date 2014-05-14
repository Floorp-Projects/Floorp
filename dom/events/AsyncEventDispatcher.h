/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AsyncEventDispatcher_h_
#define mozilla_AsyncEventDispatcher_h_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsINode.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * Use nsAsyncDOMEvent to fire a DOM event that requires safe a stable DOM.
 * For example, you may need to fire an event from within layout, but
 * want to ensure that the event handler doesn't mutate the DOM at
 * the wrong time, in order to avoid resulting instability.
 */
 
class AsyncEventDispatcher : public nsRunnable
{
public:
  AsyncEventDispatcher(nsINode* aTarget, const nsAString& aEventType,
                       bool aBubbles, bool aDispatchChromeOnly)
    : mTarget(aTarget)
    , mEventType(aEventType)
    , mBubbles(aBubbles)
    , mDispatchChromeOnly(aDispatchChromeOnly)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, const nsAString& aEventType,
                       bool aBubbles)
    : mTarget(aTarget)
    , mEventType(aEventType)
    , mBubbles(aBubbles)
    , mDispatchChromeOnly(false)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, nsIDOMEvent* aEvent)
    : mTarget(aTarget)
    , mEvent(aEvent)
    , mDispatchChromeOnly(false)
  {
  }

  AsyncEventDispatcher(dom::EventTarget* aTarget, WidgetEvent& aEvent);

  NS_IMETHOD Run() MOZ_OVERRIDE;
  nsresult PostDOMEvent();
  void RunDOMEventWhenSafe();

  nsCOMPtr<dom::EventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
  nsString              mEventType;
  bool                  mBubbles;
  bool                  mDispatchChromeOnly;
};

class LoadBlockingAsyncEventDispatcher MOZ_FINAL : public AsyncEventDispatcher
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
