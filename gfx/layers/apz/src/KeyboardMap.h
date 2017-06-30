/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_KeyboardMap_h
#define mozilla_layers_KeyboardMap_h

#include <stdint.h> // for uint32_t

#include "InputData.h"          // for KeyboardInput
#include "nsIScrollableFrame.h" // for nsIScrollableFrame::ScrollUnit
#include "nsTArray.h"           // for nsTArray
#include "mozilla/Maybe.h"      // for mozilla::Maybe
#include "KeyboardScrollAction.h" // for KeyboardScrollAction

namespace mozilla {

struct IgnoreModifierState;

namespace layers {

class KeyboardMap;

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

  /**
   * There are some default actions for keyboard inputs that are hardcoded in
   * EventStateManager instead of being represented as XBL handlers. This adds
   * keyboard shortcuts to match these inputs and dispatch them to content.
   */
  static void AppendHardcodedShortcuts(nsTArray<KeyboardShortcut>& aShortcuts);

protected:
  friend mozilla::layers::KeyboardMap;

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

/**
 * A keyboard map is an off main-thread <xul:binding> for scrolling commands.
 */
class KeyboardMap final
{
public:
  KeyboardMap();
  explicit KeyboardMap(nsTArray<KeyboardShortcut>&& aShortcuts);

  const nsTArray<KeyboardShortcut>& Shortcuts() const { return mShortcuts; }

  /**
   * Search through the internal list of shortcuts for a match for the input event
   */
  Maybe<KeyboardShortcut> FindMatch(const KeyboardInput& aEvent) const;

private:
  Maybe<KeyboardShortcut> FindMatchInternal(const KeyboardInput& aEvent,
                                            const IgnoreModifierState& aIgnore,
                                            uint32_t aOverrideCharCode = 0) const;

  nsTArray<KeyboardShortcut> mShortcuts;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_KeyboardMap_h
