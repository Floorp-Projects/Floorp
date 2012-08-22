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

class nsDOMTouch MOZ_FINAL : public nsIDOMTouch
{
public:
  nsDOMTouch(nsIDOMEventTarget* aTarget,
             int32_t aIdentifier,
             int32_t aPageX,
             int32_t aPageY,
             int32_t aScreenX,
             int32_t aScreenY,
             int32_t aClientX,
             int32_t aClientY,
             int32_t aRadiusX,
             int32_t aRadiusY,
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
  nsDOMTouch(int32_t aIdentifier,
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

  int32_t mIdentifier;
  nsIntPoint mPagePoint;
  nsIntPoint mClientPoint;
  nsIntPoint mScreenPoint;
  nsIntPoint mRadius;
  float mRotationAngle;
  float mForce;
protected:
  bool mPointsInitialized;
};

class nsDOMTouchList MOZ_FINAL : public nsIDOMTouchList
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
