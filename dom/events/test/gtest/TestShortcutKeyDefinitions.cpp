/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "KeyEventHandler.h"
#include "ShortcutKeys.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/KeyboardEvent.h"

namespace mozilla {

/**
 * These tests checks whether the key mapping defined by
 * `ShortcutKeyDefinitions.cpp` and its including header files.
 * Note that these tests do NOT define how shortcut keys mapped.  These tests
 * exist for checking unexpected change occurs by cleaning up or changing
 * the mapping.
 */

#ifndef MOZ_WIDGET_COCOA
#  define MODIFIER_ACCEL MODIFIER_CONTROL
#else
#  define MODIFIER_ACCEL MODIFIER_META
#endif

struct ShortcutKeyMap final {
  uint32_t const mKeyCode;
  char32_t const mCharCode;
  Modifiers const mModifiers;
  const char16ptr_t mCommandWin;
  const char16ptr_t mCommandMac;
  const char16ptr_t mCommandLinux;
  const char16ptr_t mCommandAndroid;
  const char16ptr_t mCommandEmacs;
};

#if defined(XP_WIN)
#  define GetCommandForPlatform(aMap) aMap.mCommandWin
#elif defined(MOZ_WIDGET_COCOA)
#  define GetCommandForPlatform(aMap) aMap.mCommandMac
#elif defined(MOZ_WIDGET_GTK)
#  define GetCommandForPlatform(aMap) aMap.mCommandLinux
#elif defined(MOZ_WIDGET_ANDROID)
#  define GetCommandForPlatform(aMap) aMap.mCommandAndroid
#else
#  define GetCommandForPlatform(aMap) aMap.mCommandEmacs
#endif

static bool GetCommandFor(KeyEventHandler* aFirstHandler,
                          dom::KeyboardEvent* aDOMEvent, nsAString& aCommand) {
  if (!aFirstHandler) {
    return false;
  }
  aCommand.Truncate();
  for (KeyEventHandler* handler = aFirstHandler; handler;
       handler = handler->GetNextHandler()) {
    if (handler->KeyEventMatched(aDOMEvent, 0, IgnoreModifierState())) {
      handler->GetCommand(aCommand);
      return true;
    }
  }
  return false;
}

static char16ptr_t GetExpectedCommandFor(
    const nsTArray<ShortcutKeyMap>& aExpectedMap,
    const WidgetKeyboardEvent& aWidgetEvent) {
  MOZ_ASSERT(!aExpectedMap.IsEmpty());
  for (const ShortcutKeyMap& map : aExpectedMap) {
    if (aWidgetEvent.mKeyCode == map.mKeyCode &&
        aWidgetEvent.mCharCode == map.mCharCode &&
        aWidgetEvent.mModifiers == map.mModifiers) {
      return GetCommandForPlatform(map);
    }
  }
  return nullptr;
}

struct KeyCodeAndStr final {
  uint32_t mKeyCode;
  KeyNameIndex mKeyNameIndex;
  const char* mStr;
};
static const KeyCodeAndStr kKeyCodes[] = {
    {NS_VK_BACK, KEY_NAME_INDEX_Backspace, "Backspace"},
    {NS_VK_RETURN, KEY_NAME_INDEX_Enter, "Enter"},
    {NS_VK_ESCAPE, KEY_NAME_INDEX_Escape, "Escape"},
    {NS_VK_PAGE_UP, KEY_NAME_INDEX_PageUp, "PageUp"},
    {NS_VK_PAGE_DOWN, KEY_NAME_INDEX_PageDown, "PageDown"},
    {NS_VK_END, KEY_NAME_INDEX_End, "End"},
    {NS_VK_HOME, KEY_NAME_INDEX_Home, "Home"},
    {NS_VK_LEFT, KEY_NAME_INDEX_ArrowLeft, "ArrowLeft"},
    {NS_VK_UP, KEY_NAME_INDEX_ArrowUp, "ArrowUp"},
    {NS_VK_RIGHT, KEY_NAME_INDEX_ArrowRight, "ArrowRight"},
    {NS_VK_DOWN, KEY_NAME_INDEX_ArrowDown, "ArrowDown"},
#ifndef MOZ_WIDGET_COCOA
    // No Insert key on macOS
    {NS_VK_INSERT, KEY_NAME_INDEX_Insert, "Insert"},
#endif
    {NS_VK_DELETE, KEY_NAME_INDEX_Delete, "Delete"},
    {NS_VK_F1, KEY_NAME_INDEX_F1, "F1"},
    {NS_VK_F2, KEY_NAME_INDEX_F2, "F2"},
    {NS_VK_F3, KEY_NAME_INDEX_F3, "F3"},
    {NS_VK_F4, KEY_NAME_INDEX_F4, "F4"},
    {NS_VK_F5, KEY_NAME_INDEX_F5, "F5"},
    {NS_VK_F6, KEY_NAME_INDEX_F6, "F6"},
    {NS_VK_F7, KEY_NAME_INDEX_F7, "F7"},
    {NS_VK_F8, KEY_NAME_INDEX_F8, "F8"},
    {NS_VK_F9, KEY_NAME_INDEX_F9, "F9"},
    {NS_VK_F10, KEY_NAME_INDEX_F10, "F10"},
    {NS_VK_F11, KEY_NAME_INDEX_F11, "F11"},
    {NS_VK_F12, KEY_NAME_INDEX_F12, "F12"},
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_ANDROID)
    {NS_VK_F13, KEY_NAME_INDEX_F13, "F13"},
    {NS_VK_F14, KEY_NAME_INDEX_F14, "F14"},
    {NS_VK_F15, KEY_NAME_INDEX_F15, "F15"},
    {NS_VK_F16, KEY_NAME_INDEX_F16, "F16"},
    {NS_VK_F17, KEY_NAME_INDEX_F17, "F17"},
    {NS_VK_F18, KEY_NAME_INDEX_F18, "F18"},
    {NS_VK_F19, KEY_NAME_INDEX_F19, "F19"},
#endif
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    {NS_VK_F20, KEY_NAME_INDEX_F20, "F20"},
    {NS_VK_F21, KEY_NAME_INDEX_F21, "F21"},
    {NS_VK_F22, KEY_NAME_INDEX_F22, "F22"},
    {NS_VK_F23, KEY_NAME_INDEX_F23, "F23"},
    {NS_VK_F24, KEY_NAME_INDEX_F24, "F24"},
#endif
};

static const unsigned char kCharCodes[] = {
    ' ', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

struct ModifiersAndStr final {
  Modifiers mModifiers;
  const char* mStr;
};
static const ModifiersAndStr kModifiers[] = {
    {MODIFIER_NONE, ""},
    {MODIFIER_SHIFT, "Shift+"},
    {MODIFIER_CONTROL, "Control+"},
    {MODIFIER_CONTROL | MODIFIER_SHIFT, "Contrl+Shift+"},
    {MODIFIER_ALT, "Alt+"},
    {MODIFIER_ALT | MODIFIER_SHIFT, "Alt+Shift+"},
    {MODIFIER_META, "Meta+"},
    {MODIFIER_META | MODIFIER_SHIFT, "Meta+Shift+"},
    {MODIFIER_META | MODIFIER_ALT | MODIFIER_SHIFT, "Meta+Alt+Shift+"},
    {MODIFIER_CONTROL | MODIFIER_ALT, "Control+Alt+"},
    {MODIFIER_CONTROL | MODIFIER_ALT | MODIFIER_SHIFT, "Control+Alt+Shift+"},
};

static void TestAllKeyCodes(KeyEventHandler* aFirstHandler,
                            const nsTArray<ShortcutKeyMap>& aExpectedMap) {
  WidgetKeyboardEvent widgetEvent(true, eKeyPress, nullptr);
  RefPtr<dom::KeyboardEvent> domEvent =
      NS_NewDOMKeyboardEvent(nullptr, nullptr, &widgetEvent);
  nsAutoString command;
  uint32_t foundCommand = 0;
  for (const auto& mod : kModifiers) {
    widgetEvent.mModifiers = mod.mModifiers;
    for (const auto& keyCode : kKeyCodes) {
      widgetEvent.mKeyCode = keyCode.mKeyCode;
      widgetEvent.mKeyNameIndex = keyCode.mKeyNameIndex;
      const char16ptr_t expectedCommand =
          GetExpectedCommandFor(aExpectedMap, widgetEvent);
      if (GetCommandFor(aFirstHandler, domEvent, command)) {
        foundCommand++;
        if (expectedCommand) {
          ASSERT_TRUE(command.Equals(expectedCommand))
          << mod.mStr << keyCode.mStr << ": got "
          << NS_ConvertUTF16toUTF8(command).get() << ", but expected "
          << NS_ConvertUTF16toUTF8(expectedCommand).get();
        } else {
          ASSERT_TRUE(false)
          << mod.mStr << keyCode.mStr << ": got "
          << NS_ConvertUTF16toUTF8(command).get()
          << ", but expected no command";
        }
      } else if (expectedCommand) {
        ASSERT_TRUE(false)
        << mod.mStr << keyCode.mStr << ": got no command, but expected "
        << NS_ConvertUTF16toUTF8(expectedCommand).get();
      }
    }
    widgetEvent.mKeyCode = 0;
    widgetEvent.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
    for (unsigned char charCode : kCharCodes) {
      widgetEvent.mCharCode = charCode;
      widgetEvent.mKeyValue.Assign(charCode);
      const char16ptr_t expectedCommand =
          GetExpectedCommandFor(aExpectedMap, widgetEvent);
      if (GetCommandFor(aFirstHandler, domEvent, command)) {
        foundCommand++;
        if (expectedCommand) {
          ASSERT_TRUE(command.Equals(expectedCommand))
          << mod.mStr << "'" << nsAutoCString(charCode).get() << "': got "
          << NS_ConvertUTF16toUTF8(command).get() << ", but expected "
          << NS_ConvertUTF16toUTF8(expectedCommand).get();
        } else {
          ASSERT_TRUE(false)
          << mod.mStr << "'" << nsAutoCString(charCode).get() << "': got "
          << NS_ConvertUTF16toUTF8(command).get()
          << ", but expected no command";
        }
      } else if (expectedCommand) {
        ASSERT_TRUE(false)
        << mod.mStr << "'" << nsAutoCString(charCode).get()
        << "': got no command, but expected "
        << NS_ConvertUTF16toUTF8(expectedCommand).get();
      }
    }
    widgetEvent.mCharCode = 0;
    widgetEvent.mKeyValue.Truncate();
  }
  uint32_t expectedCommandCount = 0;
  for (const auto& map : aExpectedMap) {
    if (GetCommandForPlatform(map)) {
      expectedCommandCount++;
    }
  }
  ASSERT_EQ(foundCommand, expectedCommandCount)
      << "Some expected shortcut keys have not been tested";
  uint32_t countOfHandler = 0;
  for (KeyEventHandler* handler = aFirstHandler; handler;
       handler = handler->GetNextHandler()) {
    countOfHandler++;
  }
  ASSERT_EQ(countOfHandler, expectedCommandCount)
      << "Some unnecessary key handlers found";
}

TEST(ShortcutKeyDefinitions, HTMLInputElement)
{
  // clang-format off
  const nsTArray<ShortcutKeyMap> expectedMap{
    // KeyCode       Modifiers         Windows             macOS    Linux               Android              Emacs
    {NS_VK_LEFT,  0, MODIFIER_NONE,    u"cmd_moveLeft",    nullptr, u"cmd_moveLeft",    u"cmd_moveLeft",     u"cmd_moveLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_NONE,    u"cmd_moveRight",   nullptr, u"cmd_moveRight",   u"cmd_moveRight",    u"cmd_moveRight"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT,   u"cmd_selectLeft",  nullptr, u"cmd_selectLeft",  u"cmd_selectLeft",   u"cmd_selectLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT,   u"cmd_selectRight", nullptr, u"cmd_selectRight", u"cmd_selectRight",  u"cmd_selectRight"},
    {NS_VK_LEFT,  0, MODIFIER_CONTROL, u"cmd_moveLeft2",   nullptr, nullptr,            u"cmd_wordPrevious", u"cmd_wordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_CONTROL, u"cmd_moveRight2",  nullptr, nullptr,            u"cmd_wordNext",     u"cmd_wordNext"},
    {NS_VK_LEFT,  0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_beginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_endLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_NONE,    u"cmd_moveUp",      nullptr, u"cmd_moveUp",      u"cmd_moveUp",       u"cmd_moveUp"},
    {NS_VK_DOWN,  0, MODIFIER_NONE,    u"cmd_moveDown",    nullptr, u"cmd_moveDown",    u"cmd_moveDown",     u"cmd_moveDown"},
    {NS_VK_UP,    0, MODIFIER_SHIFT,   u"cmd_selectUp",    nullptr, u"cmd_selectUp",    u"cmd_selectUp",     u"cmd_selectUp"},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT,   u"cmd_selectDown",  nullptr, u"cmd_selectDown",  u"cmd_selectDown",   u"cmd_selectDown"},
    {NS_VK_UP,    0, MODIFIER_CONTROL, u"cmd_moveUp2",     nullptr, nullptr,            nullptr,             nullptr},
    {NS_VK_DOWN,  0, MODIFIER_CONTROL, u"cmd_moveDown2",   nullptr, nullptr,            nullptr,             nullptr},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android                    Emacs
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectLeft2",  nullptr, nullptr, u"cmd_selectWordPrevious", u"cmd_selectWordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectRight2", nullptr, nullptr, u"cmd_selectWordNext",     u"cmd_selectWordNext"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectBeginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectEndLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectUp2",    nullptr, nullptr, nullptr,                   nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectDown2",  nullptr, nullptr, nullptr,                   nullptr},

    // KeyCode       Modifiers         Windows                 macOS    Linux    Android                 Emacs
    {NS_VK_HOME,  0, MODIFIER_NONE,    u"cmd_beginLine",       nullptr, nullptr, u"cmd_beginLine",       u"cmd_beginLine"},
    {NS_VK_END,   0, MODIFIER_NONE,    u"cmd_endLine",         nullptr, nullptr, u"cmd_endLine",         u"cmd_endLine"},
    {NS_VK_HOME,  0, MODIFIER_SHIFT,   u"cmd_selectBeginLine", nullptr, nullptr, u"cmd_selectBeginLine", u"cmd_selectBeginLine"},
    {NS_VK_END,   0, MODIFIER_SHIFT,   u"cmd_selectEndLine",   nullptr, nullptr, u"cmd_selectEndLine",   u"cmd_selectEndLine"},
    {NS_VK_HOME,  0, MODIFIER_CONTROL, u"cmd_moveTop",         nullptr, nullptr, nullptr,                u"cmd_beginLine"},
    {NS_VK_END,   0, MODIFIER_CONTROL, u"cmd_moveBottom",      nullptr, nullptr, nullptr,                u"cmd_endLine"},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android  Emacs
    {NS_VK_HOME,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectTop",    nullptr, nullptr, nullptr, u"cmd_selectBeginLine"},
    {NS_VK_END,   0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectBottom", nullptr, nullptr, nullptr, u"cmd_selectEndLine"},

    // KeyCode         Modifiers         Windows                    macOS    Linux    Android
    {NS_VK_INSERT,  0, MODIFIER_SHIFT,   u"cmd_paste",              nullptr, nullptr, nullptr,                        u"cmd_paste"},
    {NS_VK_INSERT,  0, MODIFIER_CONTROL, u"cmd_copy",               nullptr, nullptr, nullptr,                        u"cmd_copy"},
    {NS_VK_DELETE,  0, MODIFIER_SHIFT,   u"cmd_cutOrDelete",        nullptr, nullptr, nullptr,                        u"cmd_cutOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_CONTROL, u"cmd_deleteWordForward",  nullptr, nullptr, u"cmd_deleteWordForward",       u"cmd_copyOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_ALT,     nullptr,                   nullptr, nullptr, u"cmd_deleteToEndOfLine",       nullptr},
    {NS_VK_BACK,    0, MODIFIER_CONTROL, u"cmd_deleteWordBackward", nullptr, nullptr, u"cmd_deleteWordBackward",      u"cmd_deleteWordBackward"},
    {NS_VK_BACK,    0, MODIFIER_ALT,     u"cmd_undo",               nullptr, nullptr, u"cmd_deleteToBeginningOfLine", nullptr},

    // KeyCode      Modifiers                      Windows      macOS    Linux    Android  Emacs
    {NS_VK_BACK, 0, MODIFIER_SHIFT | MODIFIER_ALT, u"cmd_redo", nullptr, nullptr, nullptr, nullptr},

    // charCode Modifiers,      Windows           macOS             Linux             Android           Emacs
    {0, 'a',    MODIFIER_ACCEL, u"cmd_selectAll", u"cmd_selectAll", u"cmd_selectAll", u"cmd_selectAll", nullptr},
    {0, 'a',    MODIFIER_ALT,   nullptr,          nullptr,          nullptr,          nullptr,          u"cmd_selectAll"},
    {0, 'c',    MODIFIER_ACCEL, u"cmd_copy",      u"cmd_copy",      u"cmd_copy",      u"cmd_copy",      u"cmd_copy"},
    {0, 'x',    MODIFIER_ACCEL, u"cmd_cut",       u"cmd_cut",       u"cmd_cut",       u"cmd_cut",       u"cmd_cut"},
    {0, 'v',    MODIFIER_ACCEL, u"cmd_paste",     u"cmd_paste",     u"cmd_paste",     u"cmd_paste",     u"cmd_paste"},
    {0, 'y',    MODIFIER_ACCEL, u"cmd_redo",      nullptr,          u"cmd_redo",      nullptr,          u"cmd_redo"},
    {0, 'z',    MODIFIER_ACCEL, u"cmd_undo",      u"cmd_undo",      u"cmd_undo",      u"cmd_undo",      u"cmd_undo"},

    // charCode Modifiers,                                      Windows       macOS         Linux         Android       Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ACCEL,                u"cmd_paste", u"cmd_paste", u"cmd_paste", u"cmd_paste", u"cmd_paste"},
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ALT | MODIFIER_ACCEL, nullptr,      u"cmd_paste", nullptr,      nullptr,      nullptr},

    // charCode Modifiers                        Windows      macOS        Linux        Android      Emacs
    {0, 'z',    MODIFIER_SHIFT | MODIFIER_ACCEL, u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo"},

    // charCode Modifiers         Windows  macOS    Linux    Android  Emacs
    {0, 'a',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_beginLine"},
    {0, 'b',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charPrevious"},
    {0, 'd',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharForward"},
    {0, 'e',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_endLine"},
    {0, 'f',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charNext"},
    {0, 'h',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharBackward"},
    {0, 'k',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToEndOfLine"},
    {0, 'u',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToBeginningOfLine"},
    {0, 'w',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteWordBackward"},
  };
  // clang-format on

  TestAllKeyCodes(ShortcutKeys::GetHandlers(HandlerType::eInput), expectedMap);

  ShortcutKeys::Shutdown();  // Destroy the singleton instance.
}

TEST(ShortcutKeyDefinitions, HTMLTextAreaElement)
{
  // clang-format off
  const nsTArray<ShortcutKeyMap> expectedMap{
    // KeyCode       Modifiers         Windows             macOS    Linux               Android              Emacs
    {NS_VK_LEFT,  0, MODIFIER_NONE,    u"cmd_moveLeft",    nullptr, u"cmd_moveLeft",    u"cmd_moveLeft",     u"cmd_moveLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_NONE,    u"cmd_moveRight",   nullptr, u"cmd_moveRight",   u"cmd_moveRight",    u"cmd_moveRight"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT,   u"cmd_selectLeft",  nullptr, u"cmd_selectLeft",  u"cmd_selectLeft",   u"cmd_selectLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT,   u"cmd_selectRight", nullptr, u"cmd_selectRight", u"cmd_selectRight",  u"cmd_selectRight"},
    {NS_VK_LEFT,  0, MODIFIER_CONTROL, u"cmd_moveLeft2",   nullptr, nullptr,            u"cmd_wordPrevious", u"cmd_wordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_CONTROL, u"cmd_moveRight2",  nullptr, nullptr,            u"cmd_wordNext",     u"cmd_wordNext"},
    {NS_VK_LEFT,  0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_beginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_endLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_NONE,    u"cmd_moveUp",      nullptr, u"cmd_moveUp",      u"cmd_moveUp",       u"cmd_moveUp"},
    {NS_VK_DOWN,  0, MODIFIER_NONE,    u"cmd_moveDown",    nullptr, u"cmd_moveDown",    u"cmd_moveDown",     u"cmd_moveDown"},
    {NS_VK_UP,    0, MODIFIER_SHIFT,   u"cmd_selectUp",    nullptr, u"cmd_selectUp",    u"cmd_selectUp",     u"cmd_selectUp"},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT,   u"cmd_selectDown",  nullptr, u"cmd_selectDown",  u"cmd_selectDown",   u"cmd_selectDown"},
    {NS_VK_UP,    0, MODIFIER_CONTROL, u"cmd_moveUp2",     nullptr, nullptr,            nullptr,             nullptr},
    {NS_VK_DOWN,  0, MODIFIER_CONTROL, u"cmd_moveDown2",   nullptr, nullptr,            nullptr,             nullptr},
    {NS_VK_UP,    0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_moveTop",      nullptr},
    {NS_VK_DOWN,  0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_moveBottom",   nullptr},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android                    Emacs
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectLeft2",  nullptr, nullptr, u"cmd_selectWordPrevious", u"cmd_selectWordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectRight2", nullptr, nullptr, u"cmd_selectWordNext",     u"cmd_selectWordNext"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectBeginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectEndLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectUp2",    nullptr, nullptr, nullptr,                   nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectDown2",  nullptr, nullptr, nullptr,                   nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectTop",          nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectBottom",       nullptr},

    // KeyCode           Modifiers       Windows                macOS    Linux    Android                Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_NONE,  u"cmd_movePageUp",     nullptr, nullptr, u"cmd_movePageUp",     u"cmd_movePageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_NONE,  u"cmd_movePageDown",   nullptr, nullptr, u"cmd_movePageDown",   u"cmd_movePageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT, u"cmd_selectPageUp",   nullptr, nullptr, u"cmd_selectPageUp",   u"cmd_selectPageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT, u"cmd_selectPageDown", nullptr, nullptr, u"cmd_selectPageDown", u"cmd_selectPageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_ALT,   nullptr,               nullptr, nullptr, u"cmd_moveTop",        nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_ALT,   nullptr,               nullptr, nullptr, u"cmd_moveBottom",     nullptr},

    // KeyCode           Modifiers                        Windows  macOS    Linux    Android              Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectTop",    nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectBottom", nullptr},

    // KeyCode       Modifiers         Windows                 macOS    Linux    Android                 Emacs
    {NS_VK_HOME,  0, MODIFIER_NONE,    u"cmd_beginLine",       nullptr, nullptr, u"cmd_beginLine",       u"cmd_beginLine"},
    {NS_VK_END,   0, MODIFIER_NONE,    u"cmd_endLine",         nullptr, nullptr, u"cmd_endLine",         u"cmd_endLine"},
    {NS_VK_HOME,  0, MODIFIER_SHIFT,   u"cmd_selectBeginLine", nullptr, nullptr, u"cmd_selectBeginLine", u"cmd_selectBeginLine"},
    {NS_VK_END,   0, MODIFIER_SHIFT,   u"cmd_selectEndLine",   nullptr, nullptr, u"cmd_selectEndLine",   u"cmd_selectEndLine"},
    {NS_VK_HOME,  0, MODIFIER_CONTROL, u"cmd_moveTop",         nullptr, nullptr, u"cmd_moveTop",         u"cmd_moveTop"},
    {NS_VK_END,   0, MODIFIER_CONTROL, u"cmd_moveBottom",      nullptr, nullptr, u"cmd_moveBottom",      u"cmd_moveBottom"},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android              Emacs
    {NS_VK_HOME,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectTop",    nullptr, nullptr, u"cmd_selectTop",    u"cmd_selectTop"},
    {NS_VK_END,   0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectBottom", nullptr, nullptr, u"cmd_selectBottom", u"cmd_selectBottom"},

    // KeyCode         Modifiers         Windows                    macOS    Linux    Android                         Emacs
    {NS_VK_INSERT,  0, MODIFIER_SHIFT,   u"cmd_paste",              nullptr, nullptr, nullptr,                        u"cmd_paste"},
    {NS_VK_INSERT,  0, MODIFIER_CONTROL, u"cmd_copy",               nullptr, nullptr, nullptr,                        u"cmd_copy"},
    {NS_VK_DELETE,  0, MODIFIER_SHIFT,   u"cmd_cutOrDelete",        nullptr, nullptr, nullptr,                        u"cmd_cutOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_CONTROL, u"cmd_deleteWordForward",  nullptr, nullptr, u"cmd_deleteWordForward",       u"cmd_copyOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_ALT,     nullptr,                   nullptr, nullptr, u"cmd_deleteToEndOfLine",       nullptr},
    {NS_VK_BACK,    0, MODIFIER_CONTROL, u"cmd_deleteWordBackward", nullptr, nullptr, u"cmd_deleteWordBackward",      u"cmd_deleteWordBackward"},
    {NS_VK_BACK,    0, MODIFIER_ALT,     u"cmd_undo",               nullptr, nullptr, u"cmd_deleteToBeginningOfLine", nullptr},

    // KeyCode      Modifiers                      Windows      macOS    Linux    Android  Emacs
    {NS_VK_BACK, 0, MODIFIER_SHIFT | MODIFIER_ALT, u"cmd_redo", nullptr, nullptr, nullptr, nullptr},

    // charCode Modifiers,      Windows           macOS             Linux             Android           Emacs
    {0, 'a',    MODIFIER_ACCEL, u"cmd_selectAll", u"cmd_selectAll", u"cmd_selectAll", u"cmd_selectAll", nullptr},
    {0, 'a',    MODIFIER_ALT,   nullptr,          nullptr,          nullptr,          nullptr,          u"cmd_selectAll"},
    {0, 'c',    MODIFIER_ACCEL, u"cmd_copy",      u"cmd_copy",      u"cmd_copy",      u"cmd_copy",      u"cmd_copy"},
    {0, 'x',    MODIFIER_ACCEL, u"cmd_cut",       u"cmd_cut",       u"cmd_cut",       u"cmd_cut",       u"cmd_cut"},
    {0, 'v',    MODIFIER_ACCEL, u"cmd_paste",     u"cmd_paste",     u"cmd_paste",     u"cmd_paste",     u"cmd_paste"},
    {0, 'y',    MODIFIER_ACCEL, u"cmd_redo",      nullptr,          u"cmd_redo",      nullptr,          u"cmd_redo"},
    {0, 'z',    MODIFIER_ACCEL, u"cmd_undo",      u"cmd_undo",      u"cmd_undo",      u"cmd_undo",      u"cmd_undo"},

    // charCode Modifiers,                                      Windows       macOS         Linux         Android       Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ACCEL,                u"cmd_paste", u"cmd_paste", u"cmd_paste", u"cmd_paste", u"cmd_paste"},
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ALT | MODIFIER_ACCEL, nullptr,      u"cmd_paste", nullptr,      nullptr,      nullptr},

    // charCode Modifiers                        Windows      macOS        Linux        Android      Emacs
    {0, 'z',    MODIFIER_SHIFT | MODIFIER_ACCEL, u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo"},

    // charCode Modifiers         Windows  macOS    Linux    Android  Emacs
    {0, 'a',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_beginLine"},
    {0, 'b',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charPrevious"},
    {0, 'd',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharForward"},
    {0, 'e',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_endLine"},
    {0, 'f',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charNext"},
    {0, 'h',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharBackward"},
    {0, 'k',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToEndOfLine"},
    {0, 'n',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_lineNext"},
    {0, 'p',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_linePrevious"},
    {0, 'u',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToBeginningOfLine"},
    {0, 'w',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteWordBackward"},
  };
  // clang-format on

  TestAllKeyCodes(ShortcutKeys::GetHandlers(HandlerType::eTextArea),
                  expectedMap);

  ShortcutKeys::Shutdown();  // Destroy the singleton instance.
}

TEST(ShortcutKeyDefinitions, Browser)
{
  // clang-format off
  const nsTArray<ShortcutKeyMap> expectedMap{
    // KeyCode       Modifiers         Windows             macOS               Linux               Android                    Emacs
    {NS_VK_LEFT,  0, MODIFIER_NONE,    u"cmd_moveLeft",    u"cmd_moveLeft",    u"cmd_moveLeft",    u"cmd_moveLeft",           u"cmd_moveLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_NONE,    u"cmd_moveRight",   u"cmd_moveRight",   u"cmd_moveRight",   u"cmd_moveRight",          u"cmd_moveRight"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT,   u"cmd_selectLeft",  u"cmd_selectLeft",  u"cmd_selectLeft",  u"cmd_selectCharPrevious", u"cmd_selectLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT,   u"cmd_selectRight", u"cmd_selectRight", u"cmd_selectRight", u"cmd_selectCharNext",     u"cmd_selectRight"},
    {NS_VK_LEFT,  0, MODIFIER_CONTROL, u"cmd_moveLeft2",   nullptr,            u"cmd_moveLeft2",   u"cmd_wordPrevious",       u"cmd_wordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_CONTROL, u"cmd_moveRight2",  nullptr,            u"cmd_moveRight2",  u"cmd_wordNext",           u"cmd_wordNext"},
    {NS_VK_LEFT,  0, MODIFIER_ALT,     nullptr,            u"cmd_moveLeft2",   nullptr,            u"cmd_beginLine",          nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_ALT,     nullptr,            u"cmd_moveRight2",  nullptr,            u"cmd_endLine",            nullptr},
    {NS_VK_UP,    0, MODIFIER_NONE,    u"cmd_moveUp",      u"cmd_moveUp",      u"cmd_moveUp",      u"cmd_moveUp",             u"cmd_moveUp"},
    {NS_VK_DOWN,  0, MODIFIER_NONE,    u"cmd_moveDown",    u"cmd_moveDown",    u"cmd_moveDown",    u"cmd_moveDown",           u"cmd_moveDown"},
    {NS_VK_UP,    0, MODIFIER_SHIFT,   u"cmd_selectUp",    u"cmd_selectUp",    u"cmd_selectUp",    u"cmd_selectLinePrevious", u"cmd_selectUp"},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT,   u"cmd_selectDown",  u"cmd_selectDown",  u"cmd_selectDown",  u"cmd_selectLineNext",     u"cmd_selectDown"},
    {NS_VK_UP,    0, MODIFIER_CONTROL, u"cmd_moveUp2",     nullptr,            u"cmd_moveUp2",     nullptr,                   nullptr},
    {NS_VK_DOWN,  0, MODIFIER_CONTROL, u"cmd_moveDown2",   nullptr,            u"cmd_moveDown2",   nullptr,                   nullptr},
    {NS_VK_UP,    0, MODIFIER_ALT,     nullptr,            nullptr,            nullptr,            u"cmd_moveTop",            nullptr},
    {NS_VK_DOWN,  0, MODIFIER_ALT,     nullptr,            nullptr,            nullptr,            u"cmd_moveBottom",         nullptr},
    {NS_VK_UP,    0, MODIFIER_META,    nullptr,            u"cmd_moveUp2",     nullptr,            nullptr,                   nullptr},
    {NS_VK_DOWN,  0, MODIFIER_META,    nullptr,            u"cmd_moveDown2",   nullptr,            nullptr,                   nullptr},

    // KeyCode       Modifiers                          Windows              macOS                Linux                Android                    Emacs
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectLeft2",  nullptr,             u"cmd_selectLeft2",  u"cmd_selectWordPrevious", u"cmd_selectWordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectRight2", nullptr,             u"cmd_selectRight2", u"cmd_selectWordNext",     u"cmd_selectWordNext"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             u"cmd_selectLeft2",  nullptr,             u"cmd_selectBeginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             u"cmd_selectRight2", nullptr,             u"cmd_selectEndLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectUp2",    nullptr,             u"cmd_selectUp2",    nullptr,                   u"cmd_selectWordPrevious"},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectDown2",  nullptr,             u"cmd_selectDown2",  nullptr,                   u"cmd_selectWordNext"},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             u"cmd_selectUp2",    nullptr,             u"cmd_selectTop",          nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             u"cmd_selectDown2",  nullptr,             u"cmd_selectBottom",       nullptr},

    // KeyCode           Modifiers       Windows                macOS                  Linux                  Android                Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_NONE,  u"cmd_movePageUp",     u"cmd_scrollPageUp",   u"cmd_movePageUp",     u"cmd_movePageUp",     u"cmd_movePageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_NONE,  u"cmd_movePageDown",   u"cmd_scrollPageDown", u"cmd_movePageDown",   u"cmd_movePageDown",   u"cmd_movePageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT, u"cmd_selectPageUp",   nullptr,               u"cmd_selectPageUp",   u"cmd_selectPageUp",   u"cmd_selectPageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT, u"cmd_selectPageDown", nullptr,               u"cmd_selectPageDown", u"cmd_selectPageDown", u"cmd_selectPageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_ALT,   nullptr,               nullptr,               nullptr,               u"cmd_moveTop",        nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_ALT,   nullptr,               nullptr,               nullptr,               u"cmd_moveBottom",     nullptr},

    // KeyCode           Modifiers                        Windows  macOS    Linux    Android              Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectTop",    nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectBottom", nullptr},

    // KeyCode       Modifiers         Windows                 macOS                Linux                   Android                 Emacs
    {NS_VK_HOME,  0, MODIFIER_NONE,    u"cmd_beginLine",       u"cmd_scrollTop",    u"cmd_beginLine",       u"cmd_beginLine",       u"cmd_beginLine"},
    {NS_VK_END,   0, MODIFIER_NONE,    u"cmd_endLine",         u"cmd_scrollBottom", u"cmd_endLine",         u"cmd_endLine",         u"cmd_endLine"},
    {NS_VK_HOME,  0, MODIFIER_SHIFT,   u"cmd_selectBeginLine", nullptr,             u"cmd_selectBeginLine", u"cmd_selectBeginLine", u"cmd_selectBeginLine"},
    {NS_VK_END,   0, MODIFIER_SHIFT,   u"cmd_selectEndLine",   nullptr,             u"cmd_selectEndLine",   u"cmd_selectEndLine",   u"cmd_selectEndLine"},
    {NS_VK_HOME,  0, MODIFIER_CONTROL, u"cmd_moveTop",         nullptr,             u"cmd_moveTop",         u"cmd_moveTop",         u"cmd_moveTop"},
    {NS_VK_END,   0, MODIFIER_CONTROL, u"cmd_moveBottom",      nullptr,             u"cmd_moveBottom",      u"cmd_moveBottom",      u"cmd_moveBottom"},

    // KeyCode       Modifiers                          Windows              macOS    Linux                Android              Emacs
    {NS_VK_HOME,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectTop",    nullptr, u"cmd_selectTop",    u"cmd_selectTop",    u"cmd_selectTop"},
    {NS_VK_END,   0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectBottom", nullptr, u"cmd_selectBottom", u"cmd_selectBottom", u"cmd_selectBottom"},

    // KeyCode         Modifiers         Windows                    macOS    Linux        Android                         Emacs
    {NS_VK_INSERT,  0, MODIFIER_CONTROL, u"cmd_copy",               nullptr, u"cmd_copy", nullptr,                        u"cmd_copy"},
    {NS_VK_DELETE,  0, MODIFIER_SHIFT,   u"cmd_cut",                nullptr, u"cmd_cut",  nullptr,                        u"cmd_cut"},
    {NS_VK_DELETE,  0, MODIFIER_CONTROL, u"cmd_deleteWordForward",  nullptr, u"cmd_copy", u"cmd_deleteWordForward",       u"cmd_copy"},
    {NS_VK_DELETE,  0, MODIFIER_ALT,     nullptr,                   nullptr, nullptr,     u"cmd_deleteToEndOfLine",       nullptr},
    {NS_VK_BACK,    0, MODIFIER_CONTROL, nullptr,                   nullptr, nullptr,     u"cmd_deleteWordBackward",      nullptr},
    {NS_VK_BACK,    0, MODIFIER_ALT,     nullptr,                   nullptr, nullptr,     u"cmd_deleteToBeginningOfLine", nullptr},

    // charCode Modifiers,      Windows                macOS                  Linux                  Android                Emacs
    {0, ' ',    MODIFIER_NONE,  u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown"},
    {0, ' ',    MODIFIER_SHIFT, u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp"},
    {0, 'a',    MODIFIER_ACCEL, u"cmd_selectAll",      u"cmd_selectAll",      u"cmd_selectAll",      u"cmd_selectAll",      u"cmd_selectAll"},
    {0, 'a',    MODIFIER_ALT,   nullptr,               nullptr,               nullptr,               nullptr,               u"cmd_selectAll"},
    {0, 'c',    MODIFIER_ACCEL, u"cmd_copy",           u"cmd_copy",           u"cmd_copy",           u"cmd_copy",           u"cmd_copy"},
    {0, 'x',    MODIFIER_ACCEL, u"cmd_cut",            u"cmd_cut",            u"cmd_cut",            u"cmd_cut",            u"cmd_cut"},
    {0, 'v',    MODIFIER_ACCEL, u"cmd_paste",          u"cmd_paste",          u"cmd_paste",          u"cmd_paste",          u"cmd_paste"},
    {0, 'y',    MODIFIER_ACCEL, u"cmd_redo",           nullptr,               nullptr,               nullptr,               nullptr},
    {0, 'z',    MODIFIER_ACCEL, u"cmd_undo",           u"cmd_undo",           u"cmd_undo",           u"cmd_undo",           u"cmd_undo"},

    // charCode Modifiers,                                      Windows       macOS         Linux         Android       Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ACCEL,                u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting"},

    // charCode Modifiers,                                      Windows  macOS                     Linux    Android  Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ALT | MODIFIER_ACCEL, nullptr, u"cmd_pasteNoFormatting", nullptr, nullptr, nullptr},

    // charCode Modifiers                        Windows      macOS        Linux        Android      Emacs
    {0, 'z',    MODIFIER_SHIFT | MODIFIER_ACCEL, u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo", u"cmd_redo"},
  };
  // clang-format on

  TestAllKeyCodes(ShortcutKeys::GetHandlers(HandlerType::eBrowser),
                  expectedMap);

  ShortcutKeys::Shutdown();  // Destroy the singleton instance.
}

TEST(ShortcutKeyDefinitions, HTMLEditor)
{
  // clang-format off
  const nsTArray<ShortcutKeyMap> expectedMap{
    // KeyCode       Modifiers         Windows             macOS    Linux               Android              Emacs
    {NS_VK_LEFT,  0, MODIFIER_NONE,    u"cmd_moveLeft",    nullptr, u"cmd_moveLeft",    u"cmd_moveLeft",     u"cmd_moveLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_NONE,    u"cmd_moveRight",   nullptr, u"cmd_moveRight",   u"cmd_moveRight",    u"cmd_moveRight"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT,   u"cmd_selectLeft",  nullptr, u"cmd_selectLeft",  u"cmd_selectLeft",   u"cmd_selectLeft"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT,   u"cmd_selectRight", nullptr, u"cmd_selectRight", u"cmd_selectRight",  u"cmd_selectRight"},
    {NS_VK_LEFT,  0, MODIFIER_CONTROL, u"cmd_moveLeft2",   nullptr, nullptr,            u"cmd_wordPrevious", u"cmd_wordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_CONTROL, u"cmd_moveRight2",  nullptr, nullptr,            u"cmd_wordNext",     u"cmd_wordNext"},
    {NS_VK_LEFT,  0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_beginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_endLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_NONE,    u"cmd_moveUp",      nullptr, u"cmd_moveUp",      u"cmd_moveUp",       u"cmd_moveUp"},
    {NS_VK_DOWN,  0, MODIFIER_NONE,    u"cmd_moveDown",    nullptr, u"cmd_moveDown",    u"cmd_moveDown",     u"cmd_moveDown"},
    {NS_VK_UP,    0, MODIFIER_SHIFT,   u"cmd_selectUp",    nullptr, u"cmd_selectUp",    u"cmd_selectUp",     u"cmd_selectUp"},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT,   u"cmd_selectDown",  nullptr, u"cmd_selectDown",  u"cmd_selectDown",   u"cmd_selectDown"},
    {NS_VK_UP,    0, MODIFIER_CONTROL, u"cmd_moveUp2",     nullptr, nullptr,            nullptr,             nullptr},
    {NS_VK_DOWN,  0, MODIFIER_CONTROL, u"cmd_moveDown2",   nullptr, nullptr,            nullptr,             nullptr},
    {NS_VK_UP,    0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_moveTop",      nullptr},
    {NS_VK_DOWN,  0, MODIFIER_ALT,     nullptr,            nullptr, nullptr,            u"cmd_moveBottom",   nullptr},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android                    Emacs
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectLeft2",  nullptr, nullptr, u"cmd_selectWordPrevious", u"cmd_selectWordPrevious"},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectRight2", nullptr, nullptr, u"cmd_selectWordNext",     u"cmd_selectWordNext"},
    {NS_VK_LEFT,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectBeginLine",    nullptr},
    {NS_VK_RIGHT, 0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectEndLine",      nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectUp2",    nullptr, nullptr, nullptr,                   nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectDown2",  nullptr, nullptr, nullptr,                   nullptr},
    {NS_VK_UP,    0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectTop",          nullptr},
    {NS_VK_DOWN,  0, MODIFIER_SHIFT | MODIFIER_ALT,     nullptr,             nullptr, nullptr, u"cmd_selectBottom",       nullptr},

    // KeyCode           Modifiers       Windows                macOS    Linux    Android                Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_NONE,  u"cmd_movePageUp",     nullptr, nullptr, u"cmd_movePageUp",     u"cmd_movePageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_NONE,  u"cmd_movePageDown",   nullptr, nullptr, u"cmd_movePageDown",   u"cmd_movePageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT, u"cmd_selectPageUp",   nullptr, nullptr, u"cmd_selectPageUp",   u"cmd_selectPageUp"},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT, u"cmd_selectPageDown", nullptr, nullptr, u"cmd_selectPageDown", u"cmd_selectPageDown"},
    {NS_VK_PAGE_UP,   0, MODIFIER_ALT,   nullptr,               nullptr, nullptr, u"cmd_moveTop",        nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_ALT,   nullptr,               nullptr, nullptr, u"cmd_moveBottom",     nullptr},

    // KeyCode           Modifiers                        Windows  macOS    Linux    Android              Emacs
    {NS_VK_PAGE_UP,   0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectTop",    nullptr},
    {NS_VK_PAGE_DOWN, 0, MODIFIER_SHIFT | MODIFIER_ALT,   nullptr, nullptr, nullptr, u"cmd_selectBottom", nullptr},

    // KeyCode       Modifiers         Windows                 macOS    Linux    Android                 Emacs
    {NS_VK_HOME,  0, MODIFIER_NONE,    u"cmd_beginLine",       nullptr, nullptr, u"cmd_beginLine",       u"cmd_beginLine"},
    {NS_VK_END,   0, MODIFIER_NONE,    u"cmd_endLine",         nullptr, nullptr, u"cmd_endLine",         u"cmd_endLine"},
    {NS_VK_HOME,  0, MODIFIER_SHIFT,   u"cmd_selectBeginLine", nullptr, nullptr, u"cmd_selectBeginLine", u"cmd_selectBeginLine"},
    {NS_VK_END,   0, MODIFIER_SHIFT,   u"cmd_selectEndLine",   nullptr, nullptr, u"cmd_selectEndLine",   u"cmd_selectEndLine"},
    {NS_VK_HOME,  0, MODIFIER_CONTROL, u"cmd_moveTop",         nullptr, nullptr, u"cmd_moveTop",         u"cmd_moveTop"},
    {NS_VK_END,   0, MODIFIER_CONTROL, u"cmd_moveBottom",      nullptr, nullptr, u"cmd_moveBottom",      u"cmd_moveBottom"},

    // KeyCode       Modifiers                          Windows              macOS    Linux    Android              Emacs
    {NS_VK_HOME,  0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectTop",    nullptr, nullptr, u"cmd_selectTop",    u"cmd_selectTop"},
    {NS_VK_END,   0, MODIFIER_SHIFT | MODIFIER_CONTROL, u"cmd_selectBottom", nullptr, nullptr, u"cmd_selectBottom", u"cmd_selectBottom"},

    // KeyCode         Modifiers         Windows                    macOS    Linux    Android                         Emacs
    {NS_VK_INSERT,  0, MODIFIER_SHIFT,   u"cmd_paste",              nullptr, nullptr, nullptr,                        u"cmd_paste"},
    {NS_VK_INSERT,  0, MODIFIER_CONTROL, u"cmd_copy",               nullptr, nullptr, nullptr,                        u"cmd_copy"},
    {NS_VK_DELETE,  0, MODIFIER_SHIFT,   u"cmd_cutOrDelete",        nullptr, nullptr, nullptr,                        u"cmd_cutOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_CONTROL, u"cmd_deleteWordForward",  nullptr, nullptr, u"cmd_deleteWordForward",       u"cmd_copyOrDelete"},
    {NS_VK_DELETE,  0, MODIFIER_ALT,     nullptr,                   nullptr, nullptr, u"cmd_deleteToEndOfLine",       nullptr},
    {NS_VK_BACK,    0, MODIFIER_CONTROL, u"cmd_deleteWordBackward", nullptr, nullptr, u"cmd_deleteWordBackward",      u"cmd_deleteWordBackward"},
    {NS_VK_BACK,    0, MODIFIER_ALT,     u"cmd_undo",               nullptr, nullptr, u"cmd_deleteToBeginningOfLine", nullptr},

    // KeyCode      Modifiers                      Windows      macOS    Linux    Android  Emacs
    {NS_VK_BACK, 0, MODIFIER_SHIFT | MODIFIER_ALT, u"cmd_redo", nullptr, nullptr, nullptr, nullptr},

    // charCode Modifiers,      Windows                macOS                  Linux                  Android                Emacs
    {0, ' ',    MODIFIER_NONE,  u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown", u"cmd_scrollPageDown"},
    {0, ' ',    MODIFIER_SHIFT, u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp",   u"cmd_scrollPageUp"},
    {0, 'a',    MODIFIER_ACCEL, u"cmd_selectAll",      u"cmd_selectAll",      u"cmd_selectAll",      u"cmd_selectAll",      nullptr},
    {0, 'a',    MODIFIER_ALT,   nullptr,               nullptr,               nullptr,               nullptr,               u"cmd_selectAll"},
    {0, 'c',    MODIFIER_ACCEL, u"cmd_copy",           u"cmd_copy",           u"cmd_copy",           u"cmd_copy",           u"cmd_copy"},
    {0, 'x',    MODIFIER_ACCEL, u"cmd_cut",            u"cmd_cut",            u"cmd_cut",            u"cmd_cut",            u"cmd_cut"},
    {0, 'v',    MODIFIER_ACCEL, u"cmd_paste",          u"cmd_paste",          u"cmd_paste",          u"cmd_paste",          u"cmd_paste"},
    {0, 'y',    MODIFIER_ACCEL, u"cmd_redo",           nullptr,               u"cmd_redo",           nullptr,               u"cmd_redo"},
    {0, 'z',    MODIFIER_ACCEL, u"cmd_undo",           u"cmd_undo",           u"cmd_undo",           u"cmd_undo",           u"cmd_undo"},

    // charCode Modifiers                        Windows                   macOS                     Linux                     Android                   Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ACCEL, u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting", u"cmd_pasteNoFormatting"},
    {0, 'z',    MODIFIER_SHIFT | MODIFIER_ACCEL, u"cmd_redo",              u"cmd_redo",              u"cmd_redo",              u"cmd_redo",              u"cmd_redo"},

    // charCode Modifiers                                       Windows  macOS                     Linux    Android  Emacs
    {0, 'v',    MODIFIER_SHIFT | MODIFIER_ACCEL | MODIFIER_ALT, nullptr, u"cmd_pasteNoFormatting", nullptr, nullptr, nullptr},

    // charCode Modifiers         Windows  macOS    Linux    Android  Emacs
    {0, 'a',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_beginLine"},
    {0, 'b',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charPrevious"},
    {0, 'd',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharForward"},
    {0, 'e',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_endLine"},
    {0, 'f',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_charNext"},
    {0, 'h',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteCharBackward"},
    {0, 'k',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToEndOfLine"},
    {0, 'n',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_lineNext"},
    {0, 'p',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_linePrevious"},
    {0, 'u',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteToBeginningOfLine"},
    {0, 'w',    MODIFIER_CONTROL, nullptr, nullptr, nullptr, nullptr, u"cmd_deleteWordBackward"},
  };
  // clang-format on

  TestAllKeyCodes(ShortcutKeys::GetHandlers(HandlerType::eEditor), expectedMap);

  ShortcutKeys::Shutdown();  // Destroy the singleton instance.
}

#undef MODIFIER_ACCEL
#undef GetCommandForPlatform

}  // namespace mozilla
