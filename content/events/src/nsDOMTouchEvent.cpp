/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTouchEvent.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "nsPresContext.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchListBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// TouchList

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMTouchList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouchList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(nsDOMTouchList, mParent, mPoints)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMTouchList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMTouchList)

/* virtual */ JSObject*
nsDOMTouchList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TouchListBinding::Wrap(aCx, aScope, this);
}

/* static */ bool
nsDOMTouchList::PrefEnabled()
{
  return nsDOMTouchEvent::PrefEnabled();
}

NS_IMETHODIMP
nsDOMTouchList::GetLength(uint32_t* aLength)
{
  *aLength = Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::Item(uint32_t aIndex, nsIDOMTouch** aRetVal)
{
  NS_IF_ADDREF(*aRetVal = Item(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchList::IdentifiedTouch(int32_t aIdentifier, nsIDOMTouch** aRetVal)
{
  NS_IF_ADDREF(*aRetVal = IdentifiedTouch(aIdentifier));
  return NS_OK;
}

Touch*
nsDOMTouchList::IdentifiedTouch(int32_t aIdentifier) const
{
  for (uint32_t i = 0; i < mPoints.Length(); ++i) {
    Touch* point = mPoints[i];
    if (point && point->Identifier() == aIdentifier) {
      return point;
    }
  }
  return nullptr;
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
      Touch* touch = aEvent->touches[i];
      touch->InitializePoints(mPresContext, aEvent);
    }
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

nsDOMTouchEvent::~nsDOMTouchEvent()
{
  if (mEventIsInternal && mEvent) {
    delete static_cast<nsTouchEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(nsDOMTouchEvent, nsDOMUIEvent,
                                     mTouches,
                                     mTargetTouches,
                                     mChangedTouches)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMTouchEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTouchEvent)
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
  mTouches = static_cast<nsDOMTouchList*>(aTouches);
  mTargetTouches = static_cast<nsDOMTouchList*>(aTargetTouches);
  mChangedTouches = static_cast<nsDOMTouchList*>(aChangedTouches);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTouches(nsIDOMTouchList** aTouches)
{
  NS_ENSURE_ARG_POINTER(aTouches);
  NS_ADDREF(*aTouches = Touches());
  return NS_OK;
}

nsDOMTouchList*
nsDOMTouchEvent::Touches()
{
  if (!mTouches) {
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
    if (mEvent->message == NS_TOUCH_END || mEvent->message == NS_TOUCH_CANCEL) {
      // for touchend events, remove any changed touches from the touches array
      nsTArray< nsRefPtr<Touch> > unchangedTouches;
      const nsTArray< nsRefPtr<Touch> >& touches = touchEvent->touches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        if (!touches[i]->mChanged) {
          unchangedTouches.AppendElement(touches[i]);
        }
      }
      mTouches = new nsDOMTouchList(ToSupports(this), unchangedTouches);
    } else {
      mTouches = new nsDOMTouchList(ToSupports(this), touchEvent->touches);
    }
  }
  return mTouches;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetTargetTouches(nsIDOMTouchList** aTargetTouches)
{
  NS_ENSURE_ARG_POINTER(aTargetTouches);
  NS_ADDREF(*aTargetTouches = TargetTouches());
  return NS_OK;
}

nsDOMTouchList*
nsDOMTouchEvent::TargetTouches()
{
  if (!mTargetTouches) {
    nsTArray< nsRefPtr<Touch> > targetTouches;
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
    const nsTArray< nsRefPtr<Touch> >& touches = touchEvent->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      // for touchend/cancel events, don't append to the target list if this is a
      // touch that is ending
      if ((mEvent->message != NS_TOUCH_END &&
           mEvent->message != NS_TOUCH_CANCEL) || !touches[i]->mChanged) {
        if (touches[i]->mTarget == mEvent->originalTarget) {
          targetTouches.AppendElement(touches[i]);
        }
      }
    }
    mTargetTouches = new nsDOMTouchList(ToSupports(this), targetTouches);
  }
  return mTargetTouches;
}

NS_IMETHODIMP
nsDOMTouchEvent::GetChangedTouches(nsIDOMTouchList** aChangedTouches)
{
  NS_ENSURE_ARG_POINTER(aChangedTouches);
  NS_ADDREF(*aChangedTouches = ChangedTouches());
  return NS_OK;
}

nsDOMTouchList*
nsDOMTouchEvent::ChangedTouches()
{
  if (!mChangedTouches) {
    nsTArray< nsRefPtr<Touch> > changedTouches;
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(mEvent);
    const nsTArray< nsRefPtr<Touch> >& touches = touchEvent->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      if (touches[i]->mChanged) {
        changedTouches.AppendElement(touches[i]);
      }
    }
    mChangedTouches = new nsDOMTouchList(ToSupports(this), changedTouches);
  }
  return mChangedTouches;
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
