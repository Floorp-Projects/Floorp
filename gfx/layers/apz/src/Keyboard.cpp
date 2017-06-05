/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/Keyboard.h"

#include "mozilla/TextEvents.h" // for IgnoreModifierState

namespace mozilla {
namespace layers {

/* static */ nsIScrollableFrame::ScrollUnit
KeyboardScrollAction::GetScrollUnit(KeyboardScrollAction::KeyboardScrollActionType aDeltaType)
{
  switch (aDeltaType) {
    case KeyboardScrollAction::eScrollCharacter:
      return nsIScrollableFrame::LINES;
    case KeyboardScrollAction::eScrollLine:
      return nsIScrollableFrame::LINES;
    case KeyboardScrollAction::eScrollPage:
      return nsIScrollableFrame::PAGES;
    case KeyboardScrollAction::eScrollComplete:
      return nsIScrollableFrame::WHOLE;
    case KeyboardScrollAction::eSentinel:
      MOZ_ASSERT_UNREACHABLE("Invalid KeyboardScrollActionType.");
      return nsIScrollableFrame::WHOLE;
  }
}

KeyboardScrollAction::KeyboardScrollAction()
  : mType(KeyboardScrollAction::eScrollCharacter)
  , mForward(false)
{
}

KeyboardScrollAction::KeyboardScrollAction(KeyboardScrollActionType aType, bool aForward)
  : mType(aType)
  , mForward(aForward)
{
}

KeyboardShortcut::KeyboardShortcut()
{
}

KeyboardShortcut::KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                                   uint32_t aKeyCode,
                                   uint32_t aCharCode,
                                   Modifiers aModifiers,
                                   Modifiers aModifiersMask,
                                   const KeyboardScrollAction& aAction)
  : mAction(aAction)
  , mKeyCode(aKeyCode)
  , mCharCode(aCharCode)
  , mModifiers(aModifiers)
  , mModifiersMask(aModifiersMask)
  , mEventType(aEventType)
  , mDispatchToContent(false)
{
}

KeyboardShortcut::KeyboardShortcut(KeyboardInput::KeyboardEventType aEventType,
                                   uint32_t aKeyCode,
                                   uint32_t aCharCode,
                                   Modifiers aModifiers,
                                   Modifiers aModifiersMask)
  : mKeyCode(aKeyCode)
  , mCharCode(aCharCode)
  , mModifiers(aModifiers)
  , mModifiersMask(aModifiersMask)
  , mEventType(aEventType)
  , mDispatchToContent(true)
{
}

bool
KeyboardShortcut::Matches(const KeyboardInput& aInput,
                          const IgnoreModifierState& aIgnore,
                          uint32_t aOverrideCharCode) const
{
  return mEventType == aInput.mType &&
         MatchesKey(aInput, aOverrideCharCode) &&
         MatchesModifiers(aInput, aIgnore);
}

bool
KeyboardShortcut::MatchesKey(const KeyboardInput& aInput,
                             uint32_t aOverrideCharCode) const
{
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

bool
KeyboardShortcut::MatchesModifiers(const KeyboardInput& aInput,
                                   const IgnoreModifierState& aIgnore) const
{
  Modifiers modifiersMask = mModifiersMask;

  // If we are ignoring Shift or OS, then unset that part of the mask
  if (aIgnore.mOS) {
    modifiersMask &= ~MODIFIER_OS;
  }
  if (aIgnore.mShift) {
    modifiersMask &= ~MODIFIER_SHIFT;
  }

  // Mask off the modifiers we are ignoring from the keyboard input
  return (aInput.modifiers & modifiersMask) == mModifiers;
}

} // namespace layers
} // namespace mozilla
