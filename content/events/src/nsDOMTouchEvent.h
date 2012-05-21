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

class nsDOMTouch : public nsIDOMTouch
{
public:
  nsDOMTouch(nsIDOMEventTarget* aTarget,
             PRInt32 aIdentifier,
             PRInt32 aPageX,
             PRInt32 aPageY,
             PRInt32 aScreenX,
             PRInt32 aScreenY,
             PRInt32 aClientX,
             PRInt32 aClientY,
             PRInt32 aRadiusX,
             PRInt32 aRadiusY,
             float aRotationAngle,
             float aForce)
    {
      mTarget = aTarget;
      mIdentifier = aIdentifier;
      mPagePoint = nsIntPoint(aPageX, aPageY);
      mScreenPoint = nsIntPoint(aScreenX, aScreenY);
      mClientPoint = nsIntPoint(aClientX, aClientY);
      mRefPoint = nsIntPoint(0, 0);
      mPointsInitialized = true;
      mRadius.x = aRadiusX;
      mRadius.y = aRadiusY;
      mRotationAngle = aRotationAngle;
      mForce = aForce;

      mChanged = false;
      mMessage = 0;
    }
  nsDOMTouch(PRInt32 aIdentifier,
             nsIntPoint aPoint,
             nsIntPoint aRadius,
             float aRotationAngle,
             float aForce)
    {
      mIdentifier = aIdentifier;
      mPagePoint = nsIntPoint(0, 0);
      mScreenPoint = nsIntPoint(0, 0);
      mClientPoint = nsIntPoint(0, 0);
      mRefPoint = aPoint;
      mPointsInitialized = false;
      mRadius = aRadius;
      mRotationAngle = aRotationAngle;
      mForce = aForce;

      mChanged = false;
      mMessage = 0;
    }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouch)
  NS_DECL_NSIDOMTOUCH
  void InitializePoints(nsPresContext* aPresContext, nsEvent* aEvent)
  {
    if (mPointsInitialized) {
      return;
    }
    mClientPoint = nsDOMEvent::GetClientCoords(aPresContext,
                                               aEvent,
                                               mRefPoint,
                                               mClientPoint);
    mPagePoint = nsDOMEvent::GetPageCoords(aPresContext,
                                           aEvent,
                                           mRefPoint,
                                           mClientPoint);
    mScreenPoint = nsDOMEvent::GetScreenCoords(aPresContext, aEvent, mRefPoint);
    mPointsInitialized = true;
  }
  void SetTarget(nsIDOMEventTarget *aTarget)
  {
    mTarget = aTarget;
  }
  bool Equals(nsIDOMTouch* aTouch);
protected:
  bool mPointsInitialized;
  PRInt32 mIdentifier;
  nsIntPoint mPagePoint;
  nsIntPoint mClientPoint;
  nsIntPoint mScreenPoint;
  nsIntPoint mRadius;
  float mRotationAngle;
  float mForce;
};

class nsDOMTouchList : public nsIDOMTouchList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouchList)
  NS_DECL_NSIDOMTOUCHLIST

  nsDOMTouchList() { }
  nsDOMTouchList(nsTArray<nsCOMPtr<nsIDOMTouch> > &aTouches);
  
  void Append(nsIDOMTouch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

  nsIDOMTouch* GetItemAt(PRUint32 aIndex)
  {
    return mPoints.SafeElementAt(aIndex, nsnull);
  }
protected:
  nsTArray<nsCOMPtr<nsIDOMTouch> > mPoints;
};

class nsDOMTouchEvent : public nsDOMUIEvent,
                        public nsIDOMTouchEvent
{
public:
  nsDOMTouchEvent(nsPresContext* aPresContext, nsTouchEvent* aEvent);
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
