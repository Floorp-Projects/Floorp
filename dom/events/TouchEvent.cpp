/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchListBinding.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TouchEvents.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsExceptionHandler.h"

namespace mozilla::dom {

/******************************************************************************
 * TouchList
 *****************************************************************************/

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TouchList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TouchList, mParent, mPoints)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TouchList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TouchList)

JSObject* TouchList::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return TouchList_Binding::Wrap(aCx, this, aGivenProto);
}

// static
bool TouchList::PrefEnabled(JSContext* aCx, JSObject* aGlobal) {
  return TouchEvent::PrefEnabled(aCx, aGlobal);
}

/******************************************************************************
 * TouchEvent
 *****************************************************************************/

TouchEvent::TouchEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                       WidgetTouchEvent* aEvent)
    : UIEvent(
          aOwner, aPresContext,
          aEvent ? aEvent : new WidgetTouchEvent(false, eVoidEvent, nullptr)) {
  if (aEvent) {
    mEventIsInternal = false;

    for (uint32_t i = 0; i < aEvent->mTouches.Length(); ++i) {
      Touch* touch = aEvent->mTouches[i];
      touch->InitializePoints(mPresContext, aEvent);
    }
  } else {
    mEventIsInternal = true;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(TouchEvent, UIEvent,
                                   mEvent->AsTouchEvent()->mTouches, mTouches,
                                   mTargetTouches, mChangedTouches)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TouchEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

NS_IMPL_ADDREF_INHERITED(TouchEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(TouchEvent, UIEvent)

void TouchEvent::InitTouchEvent(const nsAString& aType, bool aCanBubble,
                                bool aCancelable, nsGlobalWindowInner* aView,
                                int32_t aDetail, bool aCtrlKey, bool aAltKey,
                                bool aShiftKey, bool aMetaKey,
                                TouchList* aTouches, TouchList* aTargetTouches,
                                TouchList* aChangedTouches) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  mEvent->AsInputEvent()->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey,
                                             aMetaKey);

  mEvent->AsTouchEvent()->mTouches.Clear();

  // To support touch.target retargeting also when the event is
  // created by JS, we need to copy Touch objects to the widget event.
  // In order to not affect targetTouches, we don't check duplicates in that
  // list.
  mTargetTouches = aTargetTouches;
  AssignTouchesToWidgetEvent(mTargetTouches, false);
  mTouches = aTouches;
  AssignTouchesToWidgetEvent(mTouches, true);
  mChangedTouches = aChangedTouches;
  AssignTouchesToWidgetEvent(mChangedTouches, true);
}

void TouchEvent::AssignTouchesToWidgetEvent(TouchList* aList,
                                            bool aCheckDuplicates) {
  if (!aList) {
    return;
  }
  WidgetTouchEvent* widgetTouchEvent = mEvent->AsTouchEvent();
  for (uint32_t i = 0; i < aList->Length(); ++i) {
    Touch* touch = aList->Item(i);
    if (touch &&
        (!aCheckDuplicates || !widgetTouchEvent->mTouches.Contains(touch))) {
      widgetTouchEvent->mTouches.AppendElement(touch);
    }
  }
}

TouchList* TouchEvent::Touches() {
  if (!mTouches) {
    WidgetTouchEvent* touchEvent = mEvent->AsTouchEvent();
    if (mEvent->mMessage == eTouchEnd || mEvent->mMessage == eTouchCancel) {
      // for touchend events, remove any changed touches from mTouches
      WidgetTouchEvent::AutoTouchArray unchangedTouches;
      const WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        if (!touches[i]->mChanged) {
          unchangedTouches.AppendElement(touches[i]);
        }
      }
      mTouches = new TouchList(ToSupports(this), unchangedTouches);
    } else {
      mTouches = new TouchList(ToSupports(this), touchEvent->mTouches);
    }
  }
  return mTouches;
}

TouchList* TouchEvent::TargetTouches() {
  if (!mTargetTouches || !mTargetTouches->Length()) {
    WidgetTouchEvent* touchEvent = mEvent->AsTouchEvent();
    if (!mTargetTouches) {
      mTargetTouches = new TouchList(ToSupports(this));
    }
    const WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      // for touchend/cancel events, don't append to the target list if this is
      // a touch that is ending
      if ((mEvent->mMessage != eTouchEnd && mEvent->mMessage != eTouchCancel) ||
          !touches[i]->mChanged) {
        bool equalTarget = touches[i]->mTarget == mEvent->mTarget;
        if (!equalTarget) {
          // Need to still check if we're inside native anonymous content
          // and the non-NAC target would be the same.
          nsIContent* touchTarget =
              nsIContent::FromEventTargetOrNull(touches[i]->mTarget);
          nsIContent* eventTarget =
              nsIContent::FromEventTargetOrNull(mEvent->mTarget);
          equalTarget = touchTarget && eventTarget &&
                        touchTarget->FindFirstNonChromeOnlyAccessContent() ==
                            eventTarget->FindFirstNonChromeOnlyAccessContent();
        }
        if (equalTarget) {
          mTargetTouches->Append(touches[i]);
        }
      }
    }
  }
  return mTargetTouches;
}

TouchList* TouchEvent::ChangedTouches() {
  if (!mChangedTouches) {
    WidgetTouchEvent::AutoTouchArray changedTouches;
    WidgetTouchEvent* touchEvent = mEvent->AsTouchEvent();
    const WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      if (touches[i]->mChanged) {
        changedTouches.AppendElement(touches[i]);
      }
    }
    mChangedTouches = new TouchList(ToSupports(this), changedTouches);
  }
  return mChangedTouches;
}

// static
bool TouchEvent::PrefEnabled(JSContext* aCx, JSObject* aGlobal) {
  nsIDocShell* docShell = nullptr;
  if (aGlobal) {
    nsGlobalWindowInner* win = xpc::WindowOrNull(aGlobal);
    if (win) {
      docShell = win->GetDocShell();
    }
  }
  return PrefEnabled(docShell);
}

static bool PlatformSupportsTouch() {
  // Touch events are only actually supported if APZ is enabled. If APZ is
  // disabled globally, we can check that once and incorporate that into the
  // cached state. If APZ is enabled, we need to further check based on the
  // widget, which we do in PrefEnabled (and don't cache that result).
  static bool sIsTouchDeviceSupportPresent =
      !!LookAndFeel::GetInt(LookAndFeel::IntID::TouchDeviceSupportPresent) &&
      gfxPlatform::AsyncPanZoomEnabled();

  return sIsTouchDeviceSupportPresent;
}

// static
bool TouchEvent::PrefEnabled(nsIDocShell* aDocShell) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  auto touchEventsOverride = mozilla::dom::TouchEventsOverride::None;
  if (aDocShell) {
    if (BrowsingContext* bc = aDocShell->GetBrowsingContext()) {
      touchEventsOverride = bc->TouchEventsOverride();
    }
  }

  bool enabled = false;
  if (touchEventsOverride == mozilla::dom::TouchEventsOverride::Enabled) {
    enabled = true;
  } else if (touchEventsOverride ==
             mozilla::dom::TouchEventsOverride::Disabled) {
    enabled = false;
  } else {
    const int32_t prefValue = StaticPrefs::dom_w3c_touch_events_enabled();
    if (prefValue == 2) {
      enabled = PlatformSupportsTouch();

      static bool firstTime = true;
      // The touch screen data seems to be inaccurate in the parent process,
      // and we really need the crash annotation in child processes.
      if (firstTime && !XRE_IsParentProcess()) {
        CrashReporter::AnnotateCrashReport(
            CrashReporter::Annotation::HasDeviceTouchScreen, enabled);
        firstTime = false;
      }

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
      if (enabled && aDocShell) {
        // APZ might be disabled on this particular widget, in which case
        // TouchEvent support will also be disabled. Try to detect that.
        RefPtr<nsPresContext> pc = aDocShell->GetPresContext();
        if (pc) {
          nsCOMPtr<nsIWidget> widget = pc->GetRootWidget();
          if (widget) {
            enabled &= widget->AsyncPanZoomEnabled();
          }
        }
      }
#endif
    } else {
      enabled = !!prefValue;
    }
  }

  if (enabled) {
    nsContentUtils::InitializeTouchEventTable();
  }
  return enabled;
}

// static
bool TouchEvent::LegacyAPIEnabled(JSContext* aCx, JSObject* aGlobal) {
  nsIPrincipal* principal = nsContentUtils::SubjectPrincipal(aCx);
  bool isSystem = principal && principal->IsSystemPrincipal();

  nsIDocShell* docShell = nullptr;
  if (aGlobal) {
    nsGlobalWindowInner* win = xpc::WindowOrNull(aGlobal);
    if (win) {
      docShell = win->GetDocShell();
    }
  }
  return LegacyAPIEnabled(docShell, isSystem);
}

// static
bool TouchEvent::LegacyAPIEnabled(nsIDocShell* aDocShell,
                                  bool aCallerIsSystem) {
  return (aCallerIsSystem ||
          StaticPrefs::dom_w3c_touch_events_legacy_apis_enabled() ||
          (aDocShell && aDocShell->GetBrowsingContext() &&
           aDocShell->GetBrowsingContext()->TouchEventsOverride() ==
               mozilla::dom::TouchEventsOverride::Enabled)) &&
         PrefEnabled(aDocShell);
}

// static
already_AddRefed<TouchEvent> TouchEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const TouchEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<TouchEvent> e = new TouchEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  RefPtr<TouchList> touches = e->CopyTouches(aParam.mTouches);
  RefPtr<TouchList> targetTouches = e->CopyTouches(aParam.mTargetTouches);
  RefPtr<TouchList> changedTouches = e->CopyTouches(aParam.mChangedTouches);
  e->InitTouchEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                    aParam.mDetail, aParam.mCtrlKey, aParam.mAltKey,
                    aParam.mShiftKey, aParam.mMetaKey, touches, targetTouches,
                    changedTouches);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

already_AddRefed<TouchList> TouchEvent::CopyTouches(
    const Sequence<OwningNonNull<Touch>>& aTouches) {
  RefPtr<TouchList> list = new TouchList(GetParentObject());
  size_t len = aTouches.Length();
  for (size_t i = 0; i < len; ++i) {
    list->Append(aTouches[i]);
  }
  return list.forget();
}

bool TouchEvent::AltKey() { return mEvent->AsTouchEvent()->IsAlt(); }

bool TouchEvent::MetaKey() { return mEvent->AsTouchEvent()->IsMeta(); }

bool TouchEvent::CtrlKey() { return mEvent->AsTouchEvent()->IsControl(); }

bool TouchEvent::ShiftKey() { return mEvent->AsTouchEvent()->IsShift(); }

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<TouchEvent> NS_NewDOMTouchEvent(EventTarget* aOwner,
                                                 nsPresContext* aPresContext,
                                                 WidgetTouchEvent* aEvent) {
  RefPtr<TouchEvent> it = new TouchEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
