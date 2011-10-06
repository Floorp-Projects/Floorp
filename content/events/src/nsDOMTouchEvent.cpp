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

#include "nsDOMTouchEvent.h"
#include "nsGUIEvent.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

DOMCI_DATA(Touch, nsDOMTouch)

NS_IMPL_CYCLE_COLLECTION_1(nsDOMTouch, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMTouch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMTouch)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouch)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Touch)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTouch)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTouch)

NS_IMETHODIMP
nsDOMTouch::GetIdentifier(PRInt32* aIdentifier)
{
  *aIdentifier = mIdentifier;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_IF_ADDREF(*aTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetScreenX(PRInt32* aScreenX)
{
  *aScreenX = mScreenX;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetScreenY(PRInt32* aScreenY)
{
  *aScreenY = mScreenY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetClientX(PRInt32* aClientX)
{
  *aClientX = mClientX;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetClientY(PRInt32* aClientY)
{
  *aClientY = mClientY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetPageX(PRInt32* aPageX)
{
  *aPageX = mPageX;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetPageY(PRInt32* aPageY)
{
  *aPageY = mPageY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetRadiusX(PRInt32* aRadiusX)
{
  *aRadiusX = mRadiusX;
  return NS_OK;
}
                                             
NS_IMETHODIMP
nsDOMTouch::GetRadiusY(PRInt32* aRadiusY)
{
  *aRadiusY = mRadiusY;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetRotationAngle(float* aRotationAngle)
{
  *aRotationAngle = mRotationAngle;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouch::GetForce(float* aForce)
{
  *aForce = mForce;
  return NS_OK;
}

// TouchList

DOMCI_DATA(TouchList, nsDOMTouchList)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMTouchList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMTouchList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouchList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TouchList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMTouchList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mPoints)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMTouchList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mPoints)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTouchList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTouchList)

NS_IMETHODIMP
nsDOMTouchList::GetLength(PRUint32* aLength)
{
  *aLength = mPoints.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::Item(PRUint32 aIndex, nsIDOMTouch** aRetVal)
{
  NS_IF_ADDREF(*aRetVal = mPoints.SafeObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::IdentifiedTouch(PRInt32 aIdentifier, nsIDOMTouch** aRetVal)
{
  *aRetVal = nsnull;
  for (PRInt32 i = 0; i < mPoints.Count(); ++i) {
    nsCOMPtr<nsIDOMTouch> point = mPoints[i];
    PRInt32 identifier;
    if (point && NS_SUCCEEDED(point->GetIdentifier(&identifier)) &&
        aIdentifier == identifier) {
      point.swap(*aRetVal);
      break;
    }
  }
  return NS_OK;
}

// TouchEvent

nsDOMTouchEvent::nsDOMTouchEvent(nsPresContext* aPresContext,
                                 nsInputEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                                        new nsInputEvent(PR_FALSE, 0, nsnull))
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
  }
}

nsDOMTouchEvent::~nsDOMTouchEvent()
{
  if (mEventIsInternal && mEvent) {
    delete static_cast<nsInputEvent*>(mEvent);
    mEvent = nsnull;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMTouchEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTouches)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTargetTouches)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChangedTouches)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTouches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTargetTouches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChangedTouches)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(TouchEvent, nsDOMTouchEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMTouchEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouchEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TouchEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)


NS_IMETHODIMP
nsDOMTouchEvent::InitTouchEvent(const nsAString& aType,
                                bool aCanBubble,
                                bool aCancelable,
                                nsIDOMWindow* aView,
                                PRInt32 aDetail,
                                bool aCtrlKey,
                                bool aAltKey,
                                bool aShiftKey,
                                bool aMetaKey,
                                nsIDOMTouchList* aTouches,
                                nsIDOMTouchList* aTargetTouches,
                                nsIDOMTouchList* aChangedTouches)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<nsInputEvent*>(mEvent)->isControl = aCtrlKey;
  static_cast<nsInputEvent*>(mEvent)->isAlt = aAltKey;
  static_cast<nsInputEvent*>(mEvent)->isShift = aShiftKey;
  static_cast<nsInputEvent*>(mEvent)->isMeta = aMetaKey;
  mTouches = aTouches;
  mTargetTouches = aTargetTouches;
  mChangedTouches = aChangedTouches;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTouches(nsIDOMTouchList** aTouches)
{
  NS_IF_ADDREF(*aTouches = mTouches);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTargetTouches(nsIDOMTouchList** aTargetTouches)
{
  NS_IF_ADDREF(*aTargetTouches = mTargetTouches);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetChangedTouches(nsIDOMTouchList** aChangedTouches)
{
  NS_IF_ADDREF(*aChangedTouches = mChangedTouches);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetAltKey(bool* aAltKey)
{
  *aAltKey = static_cast<nsInputEvent*>(mEvent)->isAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetMetaKey(bool* aMetaKey)
{
  *aMetaKey = static_cast<nsInputEvent*>(mEvent)->isMeta;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetCtrlKey(bool* aCtrlKey)
{
  *aCtrlKey = static_cast<nsInputEvent*>(mEvent)->isControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetShiftKey(bool* aShiftKey)
{
  *aShiftKey = static_cast<nsInputEvent*>(mEvent)->isShift;
  return NS_OK;
}

bool
nsDOMTouchEvent::PrefEnabled()
{
  static bool sDidCheckPref = false;
  static bool sPrefValue = false;
  if (!sDidCheckPref) {
    sDidCheckPref = PR_TRUE;
    sPrefValue = Preferences::GetBool("dom.w3c_touch_events.enabled", false);
    if (sPrefValue) {
      nsContentUtils::InitializeTouchEventTable();
    }
  }
  return sPrefValue;
}

nsresult
NS_NewDOMTouchEvent(nsIDOMEvent** aInstancePtrResult,
                    nsPresContext* aPresContext,
                    nsInputEvent *aEvent)
{
  nsDOMTouchEvent* it = new nsDOMTouchEvent(aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
