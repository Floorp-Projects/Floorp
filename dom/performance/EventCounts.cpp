/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIGlobalObject.h"
#include "EventCounts.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventCounts.h"
#include "mozilla/dom/PerformanceEventTimingBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(EventCounts, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(EventCounts, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(EventCounts, Release)

static const EventMessage sQualifiedEventType[36] = {
    EventMessage::eMouseAuxClick,
    EventMessage::eMouseClick,
    EventMessage::eContextMenu,
    EventMessage::eMouseDoubleClick,
    EventMessage::eMouseDown,
    EventMessage::eMouseEnter,
    EventMessage::eMouseLeave,
    EventMessage::eMouseOut,
    EventMessage::eMouseOver,
    EventMessage::eMouseUp,
    EventMessage::ePointerOver,
    EventMessage::ePointerEnter,
    EventMessage::ePointerDown,
    EventMessage::ePointerUp,
    EventMessage::ePointerCancel,
    EventMessage::ePointerOut,
    EventMessage::ePointerLeave,
    EventMessage::ePointerGotCapture,
    EventMessage::ePointerLostCapture,
    EventMessage::eTouchStart,
    EventMessage::eTouchEnd,
    EventMessage::eTouchCancel,
    EventMessage::eKeyDown,
    EventMessage::eKeyPress,
    EventMessage::eKeyUp,
    EventMessage::eEditorBeforeInput,
    EventMessage::eEditorInput,
    EventMessage::eCompositionStart,
    EventMessage::eCompositionUpdate,
    EventMessage::eCompositionEnd,
    EventMessage::eDragStart,
    EventMessage::eDragEnd,
    EventMessage::eDragEnter,
    EventMessage::eDragLeave,
    EventMessage::eDragOver,
    EventMessage::eDrop};

EventCounts::EventCounts(nsISupports* aParent) : mParent(aParent) {
  ErrorResult rv;

  for (const EventMessage& eventType : sQualifiedEventType) {
    EventCounts_Binding::MaplikeHelpers::Set(
        this, nsDependentString(Event::GetEventName(eventType)), 0, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
#ifdef DEBUG
      nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
      if (global) {
        MOZ_ASSERT(global->IsDying());
      }
#endif
      return;
    }
  }
}

JSObject* EventCounts::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return EventCounts_Binding::Wrap(aCx, this, aGivenProto);
}
}  // namespace mozilla::dom
