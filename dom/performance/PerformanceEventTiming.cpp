/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceEventTiming.h"
#include "PerformanceMainThread.h"
#include "mozilla/dom/PerformanceEventTimingBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/Event.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include <algorithm>

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PerformanceEventTiming, PerformanceEntry,
                                   mPerformance, mTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceEventTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(PerformanceEventTiming, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(PerformanceEventTiming, PerformanceEntry)

PerformanceEventTiming::PerformanceEventTiming(Performance* aPerformance,
                                               const nsAString& aName,
                                               const TimeStamp& aStartTime,
                                               bool aIsCacelable,
                                               EventMessage aMessage)
    : PerformanceEntry(aPerformance->GetParentObject(), aName, u"event"_ns),
      mPerformance(aPerformance),
      mProcessingStart(aPerformance->NowUnclamped()),
      mProcessingEnd(0),
      mStartTime(
          aPerformance->GetDOMTiming()->TimeStampToDOMHighRes(aStartTime)),
      mDuration(0),
      mCancelable(aIsCacelable),
      mMessage(aMessage) {}

PerformanceEventTiming::PerformanceEventTiming(
    const PerformanceEventTiming& aEventTimingEntry)
    : PerformanceEntry(aEventTimingEntry.mPerformance->GetParentObject(),
                       nsDependentAtomString(aEventTimingEntry.GetName()),
                       nsDependentAtomString(aEventTimingEntry.GetEntryType())),
      mPerformance(aEventTimingEntry.mPerformance),
      mProcessingStart(aEventTimingEntry.mProcessingStart),
      mProcessingEnd(aEventTimingEntry.mProcessingEnd),
      mTarget(aEventTimingEntry.mTarget),
      mStartTime(aEventTimingEntry.mStartTime),
      mDuration(aEventTimingEntry.mDuration),
      mCancelable(aEventTimingEntry.mCancelable),
      mMessage(aEventTimingEntry.mMessage) {}

JSObject* PerformanceEventTiming::WrapObject(
    JSContext* cx, JS::Handle<JSObject*> aGivenProto) {
  return PerformanceEventTiming_Binding::Wrap(cx, this, aGivenProto);
}

already_AddRefed<PerformanceEventTiming>
PerformanceEventTiming::TryGenerateEventTiming(const EventTarget* aTarget,
                                               const WidgetEvent* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!StaticPrefs::dom_enable_event_timing() ||
      aEvent->mFlags.mOnlyChromeDispatch) {
    return nullptr;
  }

  if (!aEvent->IsTrusted()) {
    return nullptr;
  }

  switch (aEvent->mMessage) {
    case eMouseAuxClick:
    case eMouseClick:
    case eContextMenu:
    case eMouseDoubleClick:
    case eMouseDown:
    case eMouseEnter:
    case eMouseLeave:
    case eMouseOut:
    case eMouseOver:
    case eMouseUp:
    case ePointerOver:
    case ePointerEnter:
    case ePointerDown:
    case ePointerUp:
    case ePointerCancel:
    case ePointerOut:
    case ePointerLeave:
    case ePointerGotCapture:
    case ePointerLostCapture:
    case eTouchStart:
    case eTouchEnd:
    case eTouchCancel:
    case eKeyDown:
    case eKeyPress:
    case eKeyUp:
    case eEditorBeforeInput:
    case eEditorInput:
    case eCompositionStart:
    case eCompositionUpdate:
    case eCompositionEnd:
    case eDragStart:
    case eDragEnd:
    case eDragEnter:
    case eDragLeave:
    case eDragOver:
    case eDrop:
      break;
    default:
      return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      do_QueryInterface(aTarget->GetOwnerGlobal());
  if (!innerWindow) {
    return nullptr;
  }

  if (Performance* performance = innerWindow->GetPerformance()) {
    const char16_t* eventName = Event::GetEventName(aEvent->mMessage);
    MOZ_ASSERT(eventName,
               "User defined events shouldn't be considered as event timing");
    return RefPtr<PerformanceEventTiming>(
               new PerformanceEventTiming(
                   performance, nsDependentString(eventName),
                   aEvent->mTimeStamp, aEvent->mFlags.mCancelable,
                   aEvent->mMessage))
        .forget();
  }
  return nullptr;
}

bool PerformanceEventTiming::ShouldAddEntryToBuffer(double aDuration) const {
  if (GetEntryType() == nsGkAtoms::firstInput) {
    return true;
  }
  MOZ_ASSERT(GetEntryType() == nsGkAtoms::event);
  return RawDuration() >= aDuration;
}

bool PerformanceEventTiming::ShouldAddEntryToObserverBuffer(
    PerformanceObserverInit& aOption) const {
  if (!PerformanceEntry::ShouldAddEntryToObserverBuffer(aOption)) {
    return false;
  }

  const double minDuration =
      aOption.mDurationThreshold.WasPassed()
          ? std::max(aOption.mDurationThreshold.Value(),
                     PerformanceMainThread::kDefaultEventTimingMinDuration)
          : PerformanceMainThread::kDefaultEventTimingDurationThreshold;

  return ShouldAddEntryToBuffer(minDuration);
}

void PerformanceEventTiming::BufferEntryIfNeeded() {
  if (ShouldAddEntryToBuffer(
          PerformanceMainThread::kDefaultEventTimingDurationThreshold)) {
    if (GetEntryType() != nsGkAtoms::firstInput) {
      MOZ_ASSERT(GetEntryType() == nsGkAtoms::event);
      mPerformance->BufferEventTimingEntryIfNeeded(this);
    }
  }
}

nsINode* PerformanceEventTiming::GetTarget() const {
  nsCOMPtr<Element> element = do_QueryReferent(mTarget);
  if (!element) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> global =
      do_QueryInterface(element->GetOwnerGlobal());
  if (!global) {
    return nullptr;
  }
  return nsContentUtils::GetAnElementForTiming(element, global->GetExtantDoc(),
                                               mPerformance->GetParentObject());
}

void PerformanceEventTiming::FinalizeEventTiming(EventTarget* aTarget) {
  if (!aTarget) {
    return;
  }
  nsCOMPtr<nsPIDOMWindowInner> global =
      do_QueryInterface(aTarget->GetOwnerGlobal());
  if (!global) {
    return;
  }

  mProcessingEnd = mPerformance->NowUnclamped();

  Element* element = Element::FromEventTarget(aTarget);
  if (!element || element->ChromeOnlyAccess()) {
    return;
  }

  mTarget = do_GetWeakReference(element);

  mPerformance->InsertEventTimingEntry(this);
}
}  // namespace mozilla::dom
