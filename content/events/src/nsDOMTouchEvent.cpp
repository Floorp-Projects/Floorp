/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTouchEvent.h"
#include "nsGUIEvent.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsPresContext.h"
#include "mozilla/dom/Touch.h"

using namespace mozilla;

// TouchList
nsDOMTouchList::nsDOMTouchList(nsTArray<nsCOMPtr<nsIDOMTouch> > &aTouches)
{
  mPoints.AppendElements(aTouches);
  nsJSContext::LikelyShortLivingObjectCreated();
}

DOMCI_DATA(TouchList, nsDOMTouchList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMTouchList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouchList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TouchList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_1(nsDOMTouchList, mPoints)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTouchList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTouchList)

NS_IMETHODIMP
nsDOMTouchList::GetLength(uint32_t* aLength)
{
  *aLength = mPoints.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::Item(uint32_t aIndex, nsIDOMTouch** aRetVal)
{
  NS_IF_ADDREF(*aRetVal = mPoints.SafeElementAt(aIndex, nullptr));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::IdentifiedTouch(int32_t aIdentifier, nsIDOMTouch** aRetVal)
{
  *aRetVal = nullptr;
  for (uint32_t i = 0; i < mPoints.Length(); ++i) {
    nsCOMPtr<nsIDOMTouch> point = mPoints[i];
    int32_t identifier;
    if (point && NS_SUCCEEDED(point->GetIdentifier(&identifier)) &&
        aIdentifier == identifier) {
      point.swap(*aRetVal);
      break;
    }
  }
  return NS_OK;
}

// TouchEvent

nsDOMTouchEvent::nsDOMTouchEvent(mozilla::dom::EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 nsTouchEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext,
                 aEvent ? aEvent : new nsTouchEvent(false, 0, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;

    for (uint32_t i = 0; i < aEvent->touches.Length(); ++i) {
      nsIDOMTouch *touch = aEvent->touches[i];
      dom::Touch *domtouch = static_cast<dom::Touch*>(touch);
      domtouch->InitializePoints(mPresContext, aEvent);
    }
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
  SetIsDOMBinding();
}

nsDOMTouchEvent::~nsDOMTouchEvent()
{
  if (mEventIsInternal && mEvent) {
    delete static_cast<nsTouchEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTouches)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTargetTouches)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChangedTouches)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMTouchEvent, nsDOMUIEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTouches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTargetTouches)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChangedTouches)
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
                                int32_t aDetail,
                                bool aCtrlKey,
                                bool aAltKey,
                                bool aShiftKey,
                                bool aMetaKey,
                                nsIDOMTouchList* aTouches,
                                nsIDOMTouchList* aTargetTouches,
                                nsIDOMTouchList* aChangedTouches)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType,
                                          aCanBubble,
                                          aCancelable,
                                          aView,
                                          aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<nsInputEvent*>(mEvent)->InitBasicModifiers(aCtrlKey, aAltKey,
                                                         aShiftKey, aMetaKey);
  mTouches = aTouches;
  mTargetTouches = aTargetTouches;
  mChangedTouches = aChangedTouches;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTouches(nsIDOMTouchList** aTouches)
{
  NS_ENSURE_ARG_POINTER(aTouches);
  NS_ENSURE_STATE(mEvent);
  nsRefPtr<nsDOMTouchList> t;

  if (mTouches) {
    return CallQueryInterface(mTouches, aTouches);
  }

  nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
  if (mEvent->message == NS_TOUCH_END || mEvent->message == NS_TOUCH_CANCEL) {
    // for touchend events, remove any changed touches from the touches array
    nsTArray<nsCOMPtr<nsIDOMTouch> > unchangedTouches;
    const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      if (!touches[i]->mChanged) {
        unchangedTouches.AppendElement(touches[i]);
      }
    }
    t = new nsDOMTouchList(unchangedTouches);
  } else {
    t = new nsDOMTouchList(touchEvent->touches);
  }
  mTouches = t;
  return CallQueryInterface(mTouches, aTouches);
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTargetTouches(nsIDOMTouchList** aTargetTouches)
{
  NS_ENSURE_ARG_POINTER(aTargetTouches);
  NS_ENSURE_STATE(mEvent);

  if (mTargetTouches) {
    return CallQueryInterface(mTargetTouches, aTargetTouches);
  }

  nsTArray<nsCOMPtr<nsIDOMTouch> > targetTouches;
  nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
  const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    // for touchend/cancel events, don't append to the target list if this is a
    // touch that is ending
    if ((mEvent->message != NS_TOUCH_END &&
         mEvent->message != NS_TOUCH_CANCEL) || !touches[i]->mChanged) {
      nsIDOMEventTarget* targetPtr = touches[i]->GetTarget();
      if (targetPtr == mEvent->originalTarget) {
        targetTouches.AppendElement(touches[i]);
      }
    }
  }
  mTargetTouches = new nsDOMTouchList(targetTouches);
  return CallQueryInterface(mTargetTouches, aTargetTouches);
}

NS_IMETHODIMP
nsDOMTouchEvent::GetChangedTouches(nsIDOMTouchList** aChangedTouches)
{
  NS_ENSURE_ARG_POINTER(aChangedTouches);
  NS_ENSURE_STATE(mEvent);

  if (mChangedTouches) {
    return CallQueryInterface(mChangedTouches, aChangedTouches);
  }

  nsTArray<nsCOMPtr<nsIDOMTouch> > changedTouches;
  nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
  const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    if (touches[i]->mChanged) {
      changedTouches.AppendElement(touches[i]);
    }
  }
  mChangedTouches = new nsDOMTouchList(changedTouches);
  return CallQueryInterface(mChangedTouches, aChangedTouches);
}

NS_IMETHODIMP
nsDOMTouchEvent::GetAltKey(bool* aAltKey)
{
  *aAltKey = AltKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetMetaKey(bool* aMetaKey)
{
  *aMetaKey = MetaKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetCtrlKey(bool* aCtrlKey)
{
  *aCtrlKey = CtrlKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetShiftKey(bool* aShiftKey)
{
  *aShiftKey = ShiftKey();
  return NS_OK;
}

#ifdef XP_WIN
namespace mozilla {
namespace widget {
extern int32_t IsTouchDeviceSupportPresent();
} }
#endif

bool
nsDOMTouchEvent::PrefEnabled()
{
  static bool sDidCheckPref = false;
  static bool sPrefValue = false;
  if (!sDidCheckPref) {
    sDidCheckPref = true;
    int32_t flag = 0;
    if (NS_SUCCEEDED(Preferences::GetInt("dom.w3c_touch_events.enabled",
                                         &flag))) {
      if (flag == 2) {
#ifdef XP_WIN
        // On Windows we auto-detect based on device support.
        sPrefValue = mozilla::widget::IsTouchDeviceSupportPresent();
#else
        NS_WARNING("dom.w3c_touch_events.enabled=2 not implemented!");
        sPrefValue = false;
#endif
      } else {
        sPrefValue = !!flag;
      }
    }
    if (sPrefValue) {
      nsContentUtils::InitializeTouchEventTable();
    }
  }
  return sPrefValue;
}

nsresult
NS_NewDOMTouchEvent(nsIDOMEvent** aInstancePtrResult,
                    mozilla::dom::EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    nsTouchEvent *aEvent)
{
  nsDOMTouchEvent* it = new nsDOMTouchEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
