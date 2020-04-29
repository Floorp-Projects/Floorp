/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UserAcitvation_h
#define mozilla_dom_UserAcitvation_h

#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace dom {

class UserActivation final {
 public:
  enum class State : uint8_t {
    // Not activated.
    None,
    // It is considered as has-been-activated, but not transient-activated given
    // that it is being consumed.
    HasBeenActivated,
    // It is considered as has-been-activated, and also transient-activated if
    // haven't timed out.
    FullActivated,
    EndGuard_
  };

  /**
   * Returns true if the current code is being executed as a result of
   * user input or keyboard input.  The former includes anything that is
   * initiated by user, with the exception of page load events or mouse
   * over events.  And the latter returns true when one of the user inputs
   * is an input from keyboard.  If these methods are called from asynchronously
   * executed code, such as during layout reflows, it will return false.
   */
  static bool IsHandlingUserInput();
  static bool IsHandlingKeyboardInput();

  /**
   * Returns true if the event is considered as user interaction event. I.e.,
   * enough obvious input to allow to open popup, etc. Otherwise, returns false.
   */
  static bool IsUserInteractionEvent(const WidgetEvent* aEvent);

  /**
   * StartHandlingUserInput() is called when we start to handle a user input.
   * StopHandlingUserInput() is called when we finish handling a user input.
   * If the caller knows which input event caused that, it should set
   * aMessage to the event message.  Otherwise, set eVoidEvent.
   * Note that StopHandlingUserInput() caller should call it with exactly same
   * event message as its corresponding StartHandlingUserInput() call because
   * these methods may count the number of specific event message.
   */
  static void StartHandlingUserInput(EventMessage aMessage);
  static void StopHandlingUserInput(EventMessage aMessage);

  static TimeStamp GetHandlingInputStart();

  /**
   * Get the timestamp at which the latest user input was handled.
   *
   * Guaranteed to be monotonic. Until the first user input, return
   * the epoch.
   */
  static TimeStamp LatestUserInputStart();
};

/**
 * This class is used while processing real user input. During this time, popups
 * are allowed. For mousedown events, mouse capturing is also permitted.
 */
class MOZ_RAII AutoHandlingUserInputStatePusher final {
 public:
  explicit AutoHandlingUserInputStatePusher(bool aIsHandlingUserInput,
                                            WidgetEvent* aEvent = nullptr);
  ~AutoHandlingUserInputStatePusher();

 protected:
  EventMessage mMessage;
  bool mIsHandlingUserInput;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_UserAcitvation_h
