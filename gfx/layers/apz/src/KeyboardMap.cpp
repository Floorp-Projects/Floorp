/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/KeyboardMap.h"

#include "mozilla/TextEvents.h"  // for IgnoreModifierState, ShortcutKeyCandidate

namespace mozilla {
namespace layers {

KeyboardShortcut::KeyboardShortcut()
    : mKeyCode(0),
      mCharCode(0),
      mModifiers(0),
      mModifiersMask(0),
      mEventType(KeyboardInput::KeyboardEventType::KEY_OTHER),
      mDispatchToContent(false) {}

KeyboardShortcut::KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                                   uint32_t aKeyCode, uint32_t aCharCode,
                                   Modifiers aModifiers,
                                   Modifiers aModifiersMask,
                                   const KeyboardScrollAction& aAction)
    : mAction(aAction),
      mKeyCode(aKeyCode),
      mCharCode(aCharCode),
      mModifiers(aModifiers),
      mModifiersMask(aModifiersMask),
      mEventType(aEventType),
      mDispatchToContent(false) {}

KeyboardShortcut::KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                                   uint32_t aKeyCode, uint32_t aCharCode,
                                   Modifiers aModifiers,
                                   Modifiers aModifiersMask)
    : mKeyCode(aKeyCode),
      mCharCode(aCharCode),
      mModifiers(aModifiers),
      mModifiersMask(aModifiersMask),
      mEventType(aEventType),
      mDispatchToContent(true) {}

/* static */
void KeyboardShortcut::AppendHardcodedShortcuts(
    nsTArray<KeyboardShortcut>& aShortcuts) {
  // Tab
  KeyboardShortcut tab1;
  tab1.mDispatchToContent = true;
  tab1.mKeyCode = NS_VK_TAB;
  tab1.mCharCode = 0;
  tab1.mModifiers = 0;
  tab1.mModifiersMask = 0;
  tab1.mEventType = KeyboardInput::KEY_PRESS;
  aShortcuts.AppendElement(tab1);

  // F6
  KeyboardShortcut tab2;
  tab2.mDispatchToContent = true;
  tab2.mKeyCode = NS_VK_F6;
  tab2.mCharCode = 0;
  tab2.mModifiers = 0;
  tab2.mModifiersMask = 0;
  tab2.mEventType = KeyboardInput::KEY_PRESS;
  aShortcuts.AppendElement(tab2);
}

bool KeyboardShortcut::Matches(const KeyboardInput& aInput,
                               const IgnoreModifierState& aIgnore,
                               uint32_t aOverrideCharCode) const {
  return mEventType == aInput.mType && MatchesKey(aInput, aOverrideCharCode) &&
         MatchesModifiers(aInput, aIgnore);
}

bool KeyboardShortcut::MatchesKey(const KeyboardInput& aInput,
                                  uint32_t aOverrideCharCode) const {
  // Compare by the key code if we have one
  if (!mCharCode) {
    return mKeyCode == aInput.mKeyCode;
  }

  // We are comparing by char code
  uint32_t charCode;

  // If we are comparing against a shortcut candidate then we might
  // have an override char code
  if (aOverrideCharCode) {
    charCode = aOverrideCharCode;
  } else {
    charCode = aInput.mCharCode;
  }

  // Both char codes must be in lowercase to compare correctly
  if (IS_IN_BMP(charCode)) {
    charCode = ToLowerCase(static_cast<char16_t>(charCode));
  }

  return mCharCode == charCode;
}

bool KeyboardShortcut::MatchesModifiers(
    const KeyboardInput& aInput, const IgnoreModifierState& aIgnore) const {
  Modifiers modifiersMask = mModifiersMask;

  // If we are ignoring Shift or Meta (Windows key), then unset that part of the
  // mask
  if (aIgnore.mMeta) {
    modifiersMask &= ~MODIFIER_META;
  }
  if (aIgnore.mShift) {
    modifiersMask &= ~MODIFIER_SHIFT;
  }

  // Mask off the modifiers we are ignoring from the keyboard input
  return (aInput.modifiers & modifiersMask) == mModifiers;
}

KeyboardMap::KeyboardMap(nsTArray<KeyboardShortcut>&& aShortcuts)
    : mShortcuts(aShortcuts) {}

KeyboardMap::KeyboardMap() = default;

Maybe<KeyboardShortcut> KeyboardMap::FindMatch(
    const KeyboardInput& aEvent) const {
  // If there are no shortcut candidates, then just search with with the
  // keyboard input
  if (aEvent.mShortcutCandidates.IsEmpty()) {
    return FindMatchInternal(aEvent, IgnoreModifierState());
  }

  // Otherwise do a search with each shortcut candidate in order
  for (const auto& key : aEvent.mShortcutCandidates) {
    IgnoreModifierState ignoreModifierState;
    ignoreModifierState.mShift =
        key.mShiftState == ShortcutKeyCandidate::ShiftState::Ignorable;

    auto match = FindMatchInternal(aEvent, ignoreModifierState, key.mCharCode);
    if (match) {
      return match;
    }
  }
  return Nothing();
}

Maybe<KeyboardShortcut> KeyboardMap::FindMatchInternal(
    const KeyboardInput& aEvent, const IgnoreModifierState& aIgnore,
    uint32_t aOverrideCharCode) const {
  for (auto& shortcut : mShortcuts) {
    if (shortcut.Matches(aEvent, aIgnore, aOverrideCharCode)) {
      return Some(shortcut);
    }
  }

#ifdef XP_WIN
  // Windows native applications ignore Windows-Logo key state when checking
  // shortcut keys even if the key is pressed.  Therefore, if there is no
  // shortcut key which exactly matches current modifier state, we should
  // retry to look for a shortcut key without the Windows-Logo key press.
  if (!aIgnore.mMeta && (aEvent.modifiers & MODIFIER_META)) {
    IgnoreModifierState ignoreModifierState(aIgnore);
    ignoreModifierState.mMeta = true;
    return FindMatchInternal(aEvent, ignoreModifierState, aOverrideCharCode);
  }
#endif

  return Nothing();
}

}  // namespace layers
}  // namespace mozilla
