/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAsyncDOMEvent_h___
#define nsAsyncDOMEvent_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsINode.h"
#include "nsIDOMEvent.h"
#include "nsString.h"
#include "nsIDocument.h"

/**
 * Use nsAsyncDOMEvent to fire a DOM event that requires safe a stable DOM.
 * For example, you may need to fire an event from within layout, but
 * want to ensure that the event handler doesn't mutate the DOM at
 * the wrong time, in order to avoid resulting instability.
 */
 
class nsAsyncDOMEvent : public nsRunnable {
public:
  nsAsyncDOMEvent(nsINode *aEventNode, const nsAString& aEventType,
                  bool aBubbles, bool aDispatchChromeOnly)
    : mEventNode(aEventNode), mEventType(aEventType),
      mBubbles(aBubbles),
      mDispatchChromeOnly(aDispatchChromeOnly)
  { }

  nsAsyncDOMEvent(nsINode *aEventNode, nsIDOMEvent *aEvent)
    : mEventNode(aEventNode), mEvent(aEvent), mDispatchChromeOnly(false)
  { }

  nsAsyncDOMEvent(nsINode *aEventNode, nsEvent &aEvent);

  NS_IMETHOD Run() MOZ_OVERRIDE;
  nsresult PostDOMEvent();
  void RunDOMEventWhenSafe();

  nsCOMPtr<nsINode>     mEventNode;
  nsCOMPtr<nsIDOMEvent> mEvent;
  nsString              mEventType;
  bool                  mBubbles;
  bool                  mDispatchChromeOnly;
};

class nsLoadBlockingAsyncDOMEvent : public nsAsyncDOMEvent {
public:
  nsLoadBlockingAsyncDOMEvent(nsINode *aEventNode, const nsAString& aEventType,
                              bool aBubbles, bool aDispatchChromeOnly)
    : nsAsyncDOMEvent(aEventNode, aEventType, aBubbles, aDispatchChromeOnly),
      mBlockedDoc(aEventNode->OwnerDoc())
  {
    if (mBlockedDoc) {
      mBlockedDoc->BlockOnload();
    }
  }

  nsLoadBlockingAsyncDOMEvent(nsINode *aEventNode, nsIDOMEvent *aEvent)
    : nsAsyncDOMEvent(aEventNode, aEvent),
      mBlockedDoc(aEventNode->OwnerDoc())
  {
    if (mBlockedDoc) {
      mBlockedDoc->BlockOnload();
    }
  }
  
  ~nsLoadBlockingAsyncDOMEvent();

  nsCOMPtr<nsIDocument> mBlockedDoc;
};

#endif
