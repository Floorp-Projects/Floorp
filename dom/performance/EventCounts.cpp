/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventCounts.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventCounts.h"
#include "mozilla/dom/PerformanceEventTimingBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(EventCounts, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(EventCounts, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(EventCounts, Release)

EventCounts::EventCounts(nsISupports* aParent) : mParent(aParent) {
  ErrorResult rv;
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseAuxClick)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseClick)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eContextMenu)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseDoubleClick)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseDown)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseEnter)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseLeave)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseOut)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseOver)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eMouseUp)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerOver)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerEnter)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerDown)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerUp)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerCancel)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerOut)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerLeave)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerGotCapture)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(ePointerLostCapture)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eTouchStart)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eTouchEnd)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eTouchCancel)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eKeyDown)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eKeyPress)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eKeyUp)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eEditorBeforeInput)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eEditorInput)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eCompositionStart)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eCompositionUpdate)), 0,
      rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eCompositionEnd)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDragStart)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDragEnd)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDragEnter)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDragLeave)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDragOver)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
  EventCounts_Binding::MaplikeHelpers::Set(
      this, NS_ConvertUTF8toUTF16(Event::GetEventName(eDrop)), 0, rv);
  MOZ_ASSERT(!rv.Failed());
}

JSObject* EventCounts::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return EventCounts_Binding::Wrap(aCx, this, aGivenProto);
}
}  // namespace mozilla::dom
