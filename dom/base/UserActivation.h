/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UserActivation_h
#define mozilla_dom_UserActivation_h

#include "mozilla/Assertions.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace mozilla::dom {

class UserActivation final : public nsISupports, public nsWrapperCache {
 public:
  // WebIDL UserActivation

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(UserActivation)

  explicit UserActivation(nsPIDOMWindowInner* aWindow);

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }
  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  bool HasBeenActive() const;
  bool IsActive() const;

  // End of WebIDL UserActivation

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

  class StateAndModifiers;

  // Modifier keys held while the user activation.
  class Modifiers {
   public:
    static constexpr uint8_t Shift = 0x10;
    static constexpr uint8_t Meta = 0x20;
    static constexpr uint8_t Control = 0x40;
    static constexpr uint8_t Alt = 0x80;

    static constexpr uint8_t Mask = 0xF0;

    constexpr Modifiers() = default;
    explicit constexpr Modifiers(uint8_t aModifiers) : mModifiers(aModifiers) {}

    static constexpr Modifiers None() { return Modifiers(0); }

    void SetShift() { mModifiers |= Shift; }
    void SetMeta() { mModifiers |= Meta; }
    void SetControl() { mModifiers |= Control; }
    void SetAlt() { mModifiers |= Alt; }

    bool IsShift() const { return mModifiers & Shift; }
    bool IsMeta() const { return mModifiers & Meta; }
    bool IsControl() const { return mModifiers & Control; }
    bool IsAlt() const { return mModifiers & Alt; }

   private:
    uint8_t mModifiers = 0;

    friend class StateAndModifiers;
    template <class P>
    friend struct IPC::ParamTraits;
  };

  // State and Modifiers encoded into single data, for WindowContext field.
  class StateAndModifiers {
   public:
    using DataT = uint8_t;

    constexpr StateAndModifiers() = default;
    explicit constexpr StateAndModifiers(DataT aStateAndModifiers)
        : mStateAndModifiers(aStateAndModifiers) {}

    DataT GetRawData() const { return mStateAndModifiers; }

    State GetState() const { return State(RawState()); }
    void SetState(State aState) {
      MOZ_ASSERT((uint8_t(aState) & Modifiers::Mask) == 0);
      mStateAndModifiers = uint8_t(aState) | RawModifiers();
    }

    Modifiers GetModifiers() const { return Modifiers(RawModifiers()); }
    void SetModifiers(Modifiers aModifiers) {
      mStateAndModifiers = RawState() | aModifiers.mModifiers;
    }

   private:
    uint8_t RawState() const { return mStateAndModifiers & ~Modifiers::Mask; }

    uint8_t RawModifiers() const {
      return mStateAndModifiers & Modifiers::Mask;
    }

    uint8_t mStateAndModifiers = 0;
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

 private:
  ~UserActivation() = default;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
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

}  // namespace mozilla::dom

#endif  // mozilla_dom_UserActivation_h
