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
      aEvent->touches[i]->InitializePoints(mPresContext, aEvent);
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

void
nsDOMTouchEvent::InitTouchEvent(const nsAString& aType,
                                bool aCanBubble,
                                bool aCancelable,
                                nsIDOMWindow* aView,
                                int32_t aDetail,
                                bool aCtrlKey,
                                bool aAltKey,
                                bool aShiftKey,
                                bool aMetaKey,
                                nsDOMTouchList* aTouches,
                                nsDOMTouchList* aTargetTouches,
                                nsDOMTouchList* aChangedTouches,
                                mozilla::ErrorResult& aRv)
{
  aRv = nsDOMUIEvent::InitUIEvent(aType,
                                  aCanBubble,
                                  aCancelable,
                                  aView,
                                  aDetail);
  if (aRv.Failed()) {
    return;
  }

  static_cast<nsInputEvent*>(mEvent)->InitBasicModifiers(aCtrlKey, aAltKey,
                                                         aShiftKey, aMetaKey);
  mTouches = aTouches;
  mTargetTouches = aTargetTouches;
  mChangedTouches = aChangedTouches;
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
