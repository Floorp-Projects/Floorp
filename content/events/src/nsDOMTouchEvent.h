/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsDOMTouchEvent.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsDOMTouchEvent_h_
#define nsDOMTouchEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMTouchEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIDocument.h"
#include "dombindings.h"

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

class nsDOMTouchList MOZ_FINAL : public nsIDOMTouchList,
                                 public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMTouchList)
  NS_DECL_NSIDOMTOUCHLIST

  nsDOMTouchList(nsISupports *aParent) : mParent(aParent)
  {
    SetIsProxy();
  }
  nsDOMTouchList(nsISupports *aParent,
                 nsTArray<nsCOMPtr<nsIDOMTouch> > &aTouches)
   : mPoints(aTouches),
     mParent(aParent)
  {
    SetIsProxy();
  }

  virtual JSObject* WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::binding::TouchList::create(cx, scope, this,
                                                    triedToWrap);
  }

  nsISupports *GetParentObject()
  {
    return mParent;
  }

  void Append(nsIDOMTouch* aPoint)
  {
    mPoints.AppendElement(aPoint);
  }

protected:
  nsTArray<nsCOMPtr<nsIDOMTouch> > mPoints;
  nsCOMPtr<nsISupports> mParent;
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
