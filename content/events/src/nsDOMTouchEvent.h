/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDOMTouchEvent_h_
#define nsDOMTouchEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMTouchEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "nsJSEnvironment.h"

class nsDOMTouchList MOZ_FINAL : public nsIDOMTouchList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouchList)
  NS_DECL_NSIDOMTOUCHLIST

  nsDOMTouchList()
  {
    nsJSContext::LikelyShortLivingObjectCreated();
  }
  nsDOMTouchList(nsTArray<nsCOMPtr<nsIDOMTouch> > &aTouches);

  void Append(nsIDOMTouch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

  nsIDOMTouch* GetItemAt(uint32_t aIndex)
  {
    return mPoints.SafeElementAt(aIndex, nullptr);
  }

protected:
  nsTArray<nsCOMPtr<nsIDOMTouch> > mPoints;
};

class nsDOMTouchEvent : public nsDOMUIEvent,
                        public nsIDOMTouchEvent
{
public:
  nsDOMTouchEvent(mozilla::dom::EventTarget* aOwner,
                  nsPresContext* aPresContext, nsTouchEvent* aEvent);
  virtual ~nsDOMTouchEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_DECL_NSIDOMTOUCHEVENT

  NS_FORWARD_TO_NSDOMUIEVENT

  static bool PrefEnabled();
protected:
  nsCOMPtr<nsIDOMTouchList> mTouches;
  nsCOMPtr<nsIDOMTouchList> mTargetTouches;
  nsCOMPtr<nsIDOMTouchList> mChangedTouches;
};

#endif /* !defined(nsDOMTouchEvent_h_) */
