/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/UserActivationBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"

#include "mozilla/TextEvents.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(UserActivation, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UserActivation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UserActivation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UserActivation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

UserActivation::UserActivation(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {}

JSObject* UserActivation::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return UserActivation_Binding::Wrap(aCx, this, aGivenProto);
};

// https://html.spec.whatwg.org/multipage/interaction.html#dom-useractivation-hasbeenactive
bool UserActivation::HasBeenActive() const {
  // The hasBeenActive getter steps are to return true if this's relevant global
  // object has sticky activation, and false otherwise.

  WindowContext* wc = mWindow->GetWindowContext();
  return wc && wc->HasBeenUserGestureActivated();
}

// https://html.spec.whatwg.org/multipage/interaction.html#dom-useractivation-isactive
bool UserActivation::IsActive() const {
  // The isActive getter steps are to return true if this's relevant global
  // object has transient activation, and false otherwise.

  WindowContext* wc = mWindow->GetWindowContext();
  return wc && wc->HasValidTransientUserGestureActivation();
}

namespace {

// The current depth of user and keyboard inputs. sUserInputEventDepth
// is the number of any user input events, page load events and mouse over
// events.  sUserKeyboardEventDepth is the number of keyboard input events.
// Incremented whenever we start handling a user input, decremented when we
// have finished handling a user input. This depth is *not* reset in case
// of nested event loops.
static int32_t sUserInputEventDepth = 0;
static int32_t sUserKeyboardEventDepth = 0;

// Time at which we began handling user input. Reset to the epoch
// once we have finished handling user input.
static TimeStamp sHandlingInputStart;

// Time at which we began handling the latest user input. Not reset
// at the end of the input.
static TimeStamp sLatestUserInputStart;

}  // namespace

/* static */
bool UserActivation::IsHandlingUserInput() { return sUserInputEventDepth > 0; }

/* static */
bool UserActivation::IsHandlingKeyboardInput() {
  return sUserKeyboardEventDepth > 0;
}

/* static */
bool UserActivation::IsUserInteractionEvent(const WidgetEvent* aEvent) {
  if (!aEvent->IsTrusted()) {
    return false;
  }

  switch (aEvent->mMessage) {
    // eKeyboardEventClass
    case eKeyPress:
    case eKeyDown:
    case eKeyUp:
      // Not all keyboard events are treated as user input, so that popups
      // can't be opened, fullscreen mode can't be started, etc at
      // unexpected time.
      return aEvent->AsKeyboardEvent()->CanTreatAsUserInput();
    // eBasicEventClass
    // eMouseEventClass
    case eMouseClick:
    case eMouseDown:
    case eMouseUp:
    // ePointerEventClass
    case ePointerDown:
    case ePointerUp:
    // eTouchEventClass
    case eTouchStart:
    case eTouchEnd:
      return true;
    default:
      return false;
  }
}

/* static */
void UserActivation::StartHandlingUserInput(EventMessage aMessage) {
  ++sUserInputEventDepth;
  if (sUserInputEventDepth == 1) {
    sLatestUserInputStart = sHandlingInputStart = TimeStamp::Now();
  }
  if (WidgetEvent::IsKeyEventMessage(aMessage)) {
    ++sUserKeyboardEventDepth;
  }
}

/* static */
void UserActivation::StopHandlingUserInput(EventMessage aMessage) {
  --sUserInputEventDepth;
  if (sUserInputEventDepth == 0) {
    sHandlingInputStart = TimeStamp();
  }
  if (WidgetEvent::IsKeyEventMessage(aMessage)) {
    --sUserKeyboardEventDepth;
  }
}

/* static */
TimeStamp UserActivation::GetHandlingInputStart() {
  return sHandlingInputStart;
}

/* static */
TimeStamp UserActivation::LatestUserInputStart() {
  return sLatestUserInputStart;
}

//-----------------------------------------------------------------------------
// mozilla::dom::AutoHandlingUserInputStatePusher
//-----------------------------------------------------------------------------

AutoHandlingUserInputStatePusher::AutoHandlingUserInputStatePusher(
    bool aIsHandlingUserInput, WidgetEvent* aEvent)
    : mMessage(aEvent ? aEvent->mMessage : eVoidEvent),
      mIsHandlingUserInput(aIsHandlingUserInput) {
  if (!aIsHandlingUserInput) {
    return;
  }
  UserActivation::StartHandlingUserInput(mMessage);
}

AutoHandlingUserInputStatePusher::~AutoHandlingUserInputStatePusher() {
  if (!mIsHandlingUserInput) {
    return;
  }
  UserActivation::StopHandlingUserInput(mMessage);
}

}  // namespace mozilla::dom
