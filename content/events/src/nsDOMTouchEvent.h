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
#include "nsCOMArray.h"

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
  : mTarget(aTarget),
    mIdentifier(aIdentifier),
    mPageX(aPageX),
    mPageY(aPageY),
    mScreenX(aScreenX),
    mScreenY(aScreenY),
    mClientX(aClientX),
    mClientY(aClientY),
    mRadiusX(aRadiusX),
    mRadiusY(aRadiusY),
    mRotationAngle(aRotationAngle),
    mForce(aForce)
    {}
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouch)
  NS_DECL_NSIDOMTOUCH
protected:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  PRInt32 mIdentifier;
  PRInt32 mPageX;
  PRInt32 mPageY;
  PRInt32 mScreenX;
  PRInt32 mScreenY;
  PRInt32 mClientX;
  PRInt32 mClientY;
  PRInt32 mRadiusX;
  PRInt32 mRadiusY;
  float mRotationAngle;
  float mForce;
};

class nsDOMTouchList : public nsIDOMTouchList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMTouchList)
  NS_DECL_NSIDOMTOUCHLIST
  
  void Append(nsIDOMTouch* aPoint)
  {
    mPoints.AppendObject(aPoint);
  }

  nsIDOMTouch* GetItemAt(PRUint32 aIndex)
  {
    return mPoints.SafeObjectAt(aIndex);
  }
protected:
  nsCOMArray<nsIDOMTouch> mPoints;
};

class nsDOMTouchEvent : public nsDOMUIEvent,
                        public nsIDOMTouchEvent
{
public:
  nsDOMTouchEvent(nsPresContext* aPresContext, nsInputEvent* aEvent);
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
