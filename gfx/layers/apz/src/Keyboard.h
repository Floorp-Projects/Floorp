/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Keyboard_h
#define mozilla_layers_Keyboard_h

#include <stdint.h> // for uint32_t

#include "InputData.h"          // for KeyboardInput
#include "nsIScrollableFrame.h" // for nsIScrollableFrame::ScrollUnit

namespace mozilla {

struct IgnoreModifierState;

namespace layers {

/**
 * This class represents a scrolling action to be performed on a scrollable layer.
 */
struct KeyboardScrollAction final
{
public:
  enum KeyboardScrollActionType : uint8_t
  {
    eScrollCharacter,
    eScrollLine,
    eScrollPage,
    eScrollComplete,

    // Used as an upper bound for ContiguousEnumSerializer
    eSentinel,
  };

  static nsIScrollableFrame::ScrollUnit
  GetScrollUnit(KeyboardScrollActionType aDeltaType);

  KeyboardScrollAction();
  KeyboardScrollAction(KeyboardScrollActionType aType, bool aForward);

  // The type of scroll to perform for this action
  KeyboardScrollActionType mType;
  // Whether to scroll forward or backward along the axis of this action type
  bool mForward;
};

/**
 * This class is an off main-thread <xul:handler> for scrolling commands.
 */
class KeyboardShortcut final
{
public:
  KeyboardShortcut();

  /**
   * Create a keyboard shortcut that when matched can be handled by executing
   * the specified keyboard action.
   */
  KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                   uint32_t aKeyCode,
                   uint32_t aCharCode,
                   Modifiers aModifiers,
                   Modifiers aModifiersMask,
                   const KeyboardScrollAction& aAction);

  /**
   * Create a keyboard shortcut that when matched should be handled by ignoring
   * the keyboard event and dispatching it to content.
   */
  KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                   uint32_t aKeyCode,
                   uint32_t aCharCode,
                   Modifiers aModifiers,
                   Modifiers aModifiersMask);

  bool Matches(const KeyboardInput& aInput,
               const IgnoreModifierState& aIgnore,
               uint32_t aOverrideCharCode = 0) const;

private:
  bool MatchesKey(const KeyboardInput& aInput,
                  uint32_t aOverrideCharCode) const;
  bool MatchesModifiers(const KeyboardInput& aInput,
                        const IgnoreModifierState& aIgnore) const;

public:
  // The action to perform when this shortcut is matched,
  // and not flagged to be dispatched to content
  KeyboardScrollAction mAction;

  // Only one of mKeyCode or mCharCode may be non-zero
  // whichever one is non-zero is the one to compare when matching
  uint32_t mKeyCode;
  uint32_t mCharCode;

  // The modifiers that must be active for this shortcut
  Modifiers mModifiers;
  // The modifiers to compare when matching this shortcut
  Modifiers mModifiersMask;

  // The type of keyboard event to match against
  KeyboardInput::KeyboardEventType mEventType;

  // Whether events matched by this must be dispatched to content
  bool mDispatchToContent;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_Keyboard_h
