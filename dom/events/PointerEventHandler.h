/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PointerEventHandler_h
#define mozilla_PointerEventHandler_h

#include "mozilla/EventForwards.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

class nsIFrame;
class nsIContent;

namespace mozilla {

class PresShell;

class PointerCaptureInfo final
{
public:
  nsCOMPtr<nsIContent> mPendingContent;
  nsCOMPtr<nsIContent> mOverrideContent;

  explicit PointerCaptureInfo(nsIContent* aPendingContent)
    : mPendingContent(aPendingContent)
  {
    MOZ_COUNT_CTOR(PointerCaptureInfo);
  }

  ~PointerCaptureInfo()
  {
    MOZ_COUNT_DTOR(PointerCaptureInfo);
  }

  bool Empty()
  {
    return !(mPendingContent || mOverrideContent);
  }
};

class PointerEventHandler final
{
public:
  // Called in PresShell::Initialize to initialize pointer event related
  // preferences.
  static void Initialize();

  // Called in nsLayoutStatics::Initialize/Shutdown to initialize pointer event
  // related static variables.
  static void InitializeStatics();
  static void ReleaseStatics();

  // Return the preference value of pointer event enabled.
  static bool IsPointerEventEnabled();

  // Return the preference value of implicit capture.
  static bool IsPointerEventImplicitCaptureForTouchEnabled();

  // Called in ESM::PreHandleEvent to update current active pointers in a hash
  // table.
  static void UpdateActivePointerState(WidgetMouseEvent* aEvent);

  // Got/release pointer capture of the specified pointer by the content.
  static void SetPointerCaptureById(uint32_t aPointerId, nsIContent* aContent);
  static void ReleasePointerCaptureById(uint32_t aPointerId);
  static void ReleaseAllPointerCapture();

  // Get the pointer captured info of the specified pointer.
  static PointerCaptureInfo* GetPointerCaptureInfo(uint32_t aPointerId);

  // GetPointerInfo returns true if pointer with aPointerId is situated in
  // device, false otherwise.
  // aActiveState is additional information, which shows state of pointer like
  // button state for mouse.
  static bool GetPointerInfo(uint32_t aPointerId, bool& aActiveState);

  // CheckPointerCaptureState checks cases, when got/lostpointercapture events
  // should be fired.
  static void MaybeProcessPointerCapture(WidgetGUIEvent* aEvent);
  static void ProcessPointerCaptureForMouse(WidgetMouseEvent* aEvent);
  static void ProcessPointerCaptureForTouch(WidgetTouchEvent* aEvent);
  static void CheckPointerCaptureState(WidgetPointerEvent* aEvent);

  // Implicitly get and release capture of current pointer for touch.
  static void ImplicitlyCapturePointer(nsIFrame* aFrame, WidgetEvent* aEvent);
  static void ImplicitlyReleasePointerCapture(WidgetEvent* aEvent);

  /**
   * GetPointerCapturingContent returns a target content which captures the
   * pointer. It's applied to mouse or pointer event (except mousedown and
   * pointerdown). When capturing, return the content. Otherwise, nullptr.
   *
   * @param aEvent               A mouse event or pointer event which may be
   *                             captured.
   *
   * @return                     Target content for aEvent.
   */
  static nsIContent* GetPointerCapturingContent(WidgetGUIEvent* aEvent);

  static nsIContent* GetPointerCapturingContent(uint32_t aPointerId);

  // Release pointer capture if captured by the specified content or it's
  // descendant. This is called to handle the case that the pointer capturing
  // content or it's parent is removed from the document.
  static void ReleaseIfCaptureByDescendant(nsIContent* aContent);

  /*
   * This function handles the case when content had called preventDefault on
   * the active pointer. In that case we have to prevent firing subsequent mouse
   * to content. We check the flag PointerInfo::mPreventMouseEventByContent and
   * call PreventDefault(false) to stop default behaviors and stop firing mouse
   * events to content and chrome.
   *
   * note: mouse transition events are excluded
   * note: we have to clean mPreventMouseEventByContent on pointerup for those
   *       devices support hover
   * note: we don't suppress firing mouse events to chrome and system group
   *       handlers because they may implement default behaviors
   */
  static void PreHandlePointerEventsPreventDefault(
                WidgetPointerEvent* aPointerEvent,
                WidgetGUIEvent* aMouseOrTouchEvent);

  /*
   * This function handles the preventDefault behavior of pointerdown. When user
   * preventDefault on pointerdown, We have to mark the active pointer to
   * prevent sebsequent mouse events (except mouse transition events) and
   * default behaviors.
   *
   * We add mPreventMouseEventByContent flag in PointerInfo to represent the
   * active pointer won't firing compatible mouse events. It's set to true when
   * content preventDefault on pointerdown
   */
  static void PostHandlePointerEventsPreventDefault(
                WidgetPointerEvent* aPointerEvent,
                WidgetGUIEvent* aMouseOrTouchEvent);

  MOZ_CAN_RUN_SCRIPT
  static void DispatchPointerFromMouseOrTouch(PresShell* aShell,
                                              nsIFrame* aFrame,
                                              nsIContent* aContent,
                                              WidgetGUIEvent* aEvent,
                                              bool aDontRetargetEvents,
                                              nsEventStatus* aStatus,
                                              nsIContent** aTargetContent);

  static void InitPointerEventFromMouse(WidgetPointerEvent* aPointerEvent,
                                        WidgetMouseEvent* aMouseEvent,
                                        EventMessage aMessage);

  static void InitPointerEventFromTouch(WidgetPointerEvent* aPointerEvent,
                                        WidgetTouchEvent* aTouchEvent,
                                        mozilla::dom::Touch* aTouch,
                                        bool aIsPrimary);

  static bool ShouldGeneratePointerEventFromMouse(WidgetGUIEvent* aEvent)
  {
    return aEvent->mMessage == eMouseDown || aEvent->mMessage == eMouseUp ||
           aEvent->mMessage == eMouseMove;
  }

  static bool ShouldGeneratePointerEventFromTouch(WidgetGUIEvent* aEvent)
  {
    return aEvent->mMessage == eTouchStart || aEvent->mMessage == eTouchMove ||
           aEvent->mMessage == eTouchEnd || aEvent->mMessage == eTouchCancel ||
           aEvent->mMessage == eTouchPointerCancel;
  }

private:
  // GetPointerType returns pointer type like mouse, pen or touch for pointer
  // event with pointerId. The return value must be one of
  // MouseEvent_Binding::MOZ_SOURCE_*
  static uint16_t GetPointerType(uint32_t aPointerId);

  // GetPointerPrimaryState returns state of attribute isPrimary for pointer
  // event with pointerId
  static bool GetPointerPrimaryState(uint32_t aPointerId);

  static void DispatchGotOrLostPointerCaptureEvent(
                bool aIsGotCapture,
                const WidgetPointerEvent* aPointerEvent,
                nsIContent* aCaptureTarget);
};

} // namespace mozilla

#endif // mozilla_PointerEventHandler_h
