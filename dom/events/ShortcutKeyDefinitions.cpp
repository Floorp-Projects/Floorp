/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShortcutKeys.h"

#if !defined(XP_WIN) && !defined(MOZ_WIDGET_COCOA) && \
    !defined(MOZ_WIDGET_GTK) && !defined(MOZ_WIDGET_ANDROID)
#  define USE_EMACS_KEY_BINDINGS
#endif

/**
 * This file defines shortcut keys for <input>, <textarea>, page navigation
 * and HTML editor.  You must write each mapping in one line and append inline
 * comment on which platform it's mapped since this style helps you to looking
 * for the defintion with searchfox, etc.
 * Be aware, the commands defined in this file may not work because editor or
 * other keyboard event listeners may handle before.
 * Note: When you change key mappings, you need to change
 * `test/gtest/TestShortcutKeyDefinitions.cpp` too.
 *
 *  The latest version of the original files are:
 *
 * Windows:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/win/ShortcutKeyDefinitions.cpp
 * macOS:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/mac/ShortcutKeyDefinitions.cpp
 * Linux:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/unix/ShortcutKeyDefinitions.cpp
 * Android:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/android/ShortcutKeyDefinitions.cpp
 * Emacs:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/emacs/ShortcutKeyDefinitions.cpp
 *
 * And common definitions except macOS:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/ShortcutKeyDefinitionsForInputCommon.h
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/ShortcutKeyDefinitionsForTextAreaCommon.h
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/ShortcutKeyDefinitionsForEditorCommon.h
 *
 * And common definitions for page navigation on all platforms:
 * https://searchfox.org/mozilla-central/rev/fd853f4aea89186efdb368e759a71b7a90c2b89c/dom/events/ShortcutKeyDefinitionsForBrowserCommon.h
 *
 * If you don't see shortcut key definitions here, but you see shortcut keys
 * work on Linux or macOS, it probably comes from NativeKeyBindings under
 * widget.
 */

namespace mozilla {

ShortcutKeyData ShortcutKeys::sInputHandlers[] = {
// clang-format off
    /**************************************************************************
     * Arrow keys to move caret in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, nullptr,  u"cmd_moveLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, nullptr,  u"cmd_moveRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, nullptr,  u"cmd_moveUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, nullptr,  u"cmd_moveDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys to select a char/line in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"shift", u"cmd_selectLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, u"shift", u"cmd_selectUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, u"shift", u"cmd_selectDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys per word in <input>.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_wordPrevious"},        // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_wordNext"},            // Android, Emacs
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,control", u"cmd_selectWordPrevious"},  // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control", u"cmd_selectWordNext"},      // Android, Emacs
#endif // MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,control", u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control", u"cmd_selectRight2"},        // Win
#endif // XP_WIN

    /**************************************************************************
     * Arrow keys per block in <input>.
     **************************************************************************/
#if defined(XP_WIN)
    {u"keypress", u"VK_UP",   nullptr, u"control",       u"cmd_moveUp2"},      // Win
    {u"keypress", u"VK_DOWN", nullptr, u"control",       u"cmd_moveDown2"},    // Win
    {u"keypress", u"VK_UP",   nullptr, u"shift,control", u"cmd_selectUp2"},    // Win
    {u"keypress", u"VK_DOWN", nullptr, u"shift,control", u"cmd_selectDown2"},  // Win
#endif // XP_WIN

    /**************************************************************************
     * Arrow keys to begin/end of a line in <input>.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_LEFT",  nullptr, u"alt",       u"cmd_beginLine"},        // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"alt",       u"cmd_endLine"},          // Android
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,alt", u"cmd_selectBeginLine"},  // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},    // Android
#endif // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Home/End keys in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_HOME", nullptr, nullptr,          u"cmd_beginLine"},        // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, nullptr,          u"cmd_endLine"},          // Win, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"shift",         u"cmd_selectBeginLine"},  // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"shift",         u"cmd_selectEndLine"},    // Win, Android, Emacs
#endif // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_HOME", nullptr, u"control",       u"cmd_beginLine"},        // Emacs
    {u"keypress", u"VK_END",  nullptr, u"control",       u"cmd_endLine"},          // Emacs
    {u"keypress", u"VK_HOME", nullptr, u"control,shift", u"cmd_selectBeginLine"},  // Emacs
    {u"keypress", u"VK_END",  nullptr, u"control,shift", u"cmd_selectEndLine"},    // Emacs
#endif // USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN)
    {u"keypress", u"VK_HOME", nullptr, u"control",        u"cmd_moveTop"},         // Win
    {u"keypress", u"VK_END",  nullptr, u"control",        u"cmd_moveBottom"},      // Win
    {u"keypress", u"VK_HOME", nullptr, u"shift,control",  u"cmd_selectTop"},       // Win
    {u"keypress", u"VK_END",  nullptr, u"shift,control",  u"cmd_selectBottom"},    // Win
#endif // XP_WIN

    /**************************************************************************
     * Insert key in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_INSERT", nullptr, u"control", u"cmd_copy"},   // Win, Emacs
    {u"keypress", u"VK_INSERT", nullptr, u"shift",   u"cmd_paste"},  // Win, Emacs
#endif  // XP_WIN || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Delete key in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"shift",   u"cmd_cutOrDelete"},        // Win, Emacs
#endif // XP_WIN || USE_EMACS_KEY_BINDINGS
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_copyOrDelete"},       // Emacs
#endif // USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},  // Win, Android
#endif  // XP_WIN
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"alt",     u"cmd_deleteToEndOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Backspace key in <input>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_BACK", nullptr, u"control",   u"cmd_deleteWordBackward"},       // Win, Android, Emacs
#endif // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_deleteToBeginningOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID
#if defined(XP_WIN)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_undo"},                     // Win
    {u"keypress", u"VK_BACK", nullptr, u"alt,shift", u"cmd_redo"},                     // Win
#endif  // XP_WIN

    /**************************************************************************
     * Common editor commands in <input>.
     **************************************************************************/
    {u"keypress", nullptr, u"c", u"accel",       u"cmd_copy"},   // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"x", u"accel",       u"cmd_cut"},    // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel",       u"cmd_paste"},  // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel",       u"cmd_undo"},   // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel,shift", u"cmd_redo"},   // Win, macOS, Linux, Android, Emacs

    {u"keypress", nullptr, u"v", u"accel,shift",     u"cmd_paste"},  // Win, macOS, Linux, Android, Emacs
// Mac uses Option+Shift+Command+V for Paste and Match Style
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", nullptr, u"v", u"accel,alt,shift", u"cmd_paste"},  // macOS
#endif  // MOZ_WIDGET_COCOA

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"y", u"accel",       u"cmd_redo"},   // Win, Linux, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS

#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_GTK) ||\
    defined(MOZ_WIDGET_ANDROID)
    {u"keypress", nullptr, u"a", u"accel",       u"cmd_selectAll"},  // Win, macOS, Linux, Android
#endif  // XP_WIN || MOZ_WIDGET_COCOA || MOZ_WIDGET_GTK || MOZ_WIDGET_ANDROID
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"alt",         u"cmd_selectAll"},  // Emacs
#endif  // USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Emacs specific shortcut keys in <input>.
     **************************************************************************/
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"control", u"cmd_beginLine"},                // Emacs
    {u"keypress", nullptr, u"e", u"control", u"cmd_endLine"},                  // Emacs
    {u"keypress", nullptr, u"b", u"control", u"cmd_charPrevious"},             // Emacs
    {u"keypress", nullptr, u"f", u"control", u"cmd_charNext"},                 // Emacs
    {u"keypress", nullptr, u"h", u"control", u"cmd_deleteCharBackward"},       // Emacs
    {u"keypress", nullptr, u"d", u"control", u"cmd_deleteCharForward"},        // Emacs
    {u"keypress", nullptr, u"w", u"control", u"cmd_deleteWordBackward"},       // Emacs
    {u"keypress", nullptr, u"u", u"control", u"cmd_deleteToBeginningOfLine"},  // Emacs
    {u"keypress", nullptr, u"k", u"control", u"cmd_deleteToEndOfLine"},        // Emacs
#endif  // USE_EMACS_KEY_BINDINGS
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sTextAreaHandlers[] = {
// clang-format off
    /**************************************************************************
     * Arrow keys to move caret in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, nullptr, u"cmd_moveLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, nullptr, u"cmd_moveRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, nullptr, u"cmd_moveUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, nullptr, u"cmd_moveDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys to select a char/line in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"shift", u"cmd_selectLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, u"shift", u"cmd_selectUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, u"shift", u"cmd_selectDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys per word in <textarea>.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_wordPrevious"},        // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_wordNext"},            // Android, Emacs
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,control", u"cmd_selectWordPrevious"},  // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control", u"cmd_selectWordNext"},      // Android, Emacs
#endif // MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,control", u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control", u"cmd_selectRight2"},        // Win
#endif // XP_WIN

    /**************************************************************************
     * Arrow keys per block in <textarea>.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_UP",   nullptr, u"alt",           u"cmd_moveTop"},      // Android
    {u"keypress", u"VK_DOWN", nullptr, u"alt",           u"cmd_moveBottom"},   // Android
    {u"keypress", u"VK_UP",   nullptr, u"shift,alt",     u"cmd_selectTop"},    // Android
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt",     u"cmd_selectBottom"}, // Android
#endif // MOZ_WIDGET_ANDROID
#if defined(XP_WIN)
    {u"keypress", u"VK_UP",   nullptr, u"control",       u"cmd_moveUp2"},      // Win
    {u"keypress", u"VK_DOWN", nullptr, u"control",       u"cmd_moveDown2"},    // Win
    {u"keypress", u"VK_UP",   nullptr, u"shift,control", u"cmd_selectUp2"},    // Win
    {u"keypress", u"VK_DOWN", nullptr, u"shift,control", u"cmd_selectDown2"},  // Win
#endif // XP_WIN

    /**************************************************************************
     * Arrow keys to begin/end of a line in <textarea>.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_LEFT",  nullptr, u"alt",       u"cmd_beginLine"},        // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"alt",       u"cmd_endLine"},          // Android
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,alt", u"cmd_selectBeginLine"},  // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},    // Android
#endif // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * PageUp/PageDown keys in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,      u"cmd_movePageUp"},      // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,      u"cmd_movePageDown"},    // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",     u"cmd_selectPageUp"},    // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",     u"cmd_selectPageDown"},  // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"alt",       u"cmd_moveTop"},         // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt",       u"cmd_moveBottom"},      // Android
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift,alt", u"cmd_selectTop"},       // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},    // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Home/End keys in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_HOME",      nullptr, nullptr,           u"cmd_beginLine"},                // Win, Android, Emacs
    {u"keypress", u"VK_END",       nullptr, nullptr,           u"cmd_endLine"},                  // Win, Android, Emacs
    {u"keypress", u"VK_HOME",      nullptr, u"shift",          u"cmd_selectBeginLine"},          // Win, Android, Emacs
    {u"keypress", u"VK_END",       nullptr, u"shift",          u"cmd_selectEndLine"},            // Win, Android, Emacs
    {u"keypress", u"VK_HOME",      nullptr, u"control",        u"cmd_moveTop"},                  // Win, Android, Emacs
    {u"keypress", u"VK_END",       nullptr, u"control",        u"cmd_moveBottom"},               // Win, Android, Emacs
    {u"keypress", u"VK_HOME",      nullptr, u"shift,control",  u"cmd_selectTop"},                // Win, Android, Emacs
    {u"keypress", u"VK_END",       nullptr, u"shift,control",  u"cmd_selectBottom"},             // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Insert key in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_INSERT", nullptr, u"control", u"cmd_copy"},   // Win, Emacs
    {u"keypress", u"VK_INSERT", nullptr, u"shift",   u"cmd_paste"},  // Win, Emacs
#endif  // XP_WIN || USE_EMACS_KEY_BINDINGS

    {u"keypress", nullptr, u"v", u"accel,shift",     u"cmd_paste"},  // Win, macOS, Linux, Android, Emacs
// Mac uses Option+Shift+Command+V for Paste and Match Style
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", nullptr, u"v", u"accel,alt,shift", u"cmd_paste"},  // macOS
#endif  // MOZ_WIDGET_COCOA

    /**************************************************************************
     * Delete key in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"shift",   u"cmd_cutOrDelete"},        // Win, Emacs
#endif  // XP_WIN || USE_EMACS_KEY_BINDINGS
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_copyOrDelete"},       // Emacs
#endif  // USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},  // Win, Android
#endif  // XP_WIN || MOZ_WIDGET_ANDROID
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"alt",     u"cmd_deleteToEndOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Backspace key in <textarea>.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_BACK", nullptr, u"control",   u"cmd_deleteWordBackward"},       // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_deleteToBeginningOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID
#if defined(XP_WIN)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_undo"},                     // Win
    {u"keypress", u"VK_BACK", nullptr, u"alt,shift", u"cmd_redo"},                     // Win
#endif  // XP_WIN

    /**************************************************************************
     * Common editor commands in <textarea>.
     **************************************************************************/
    {u"keypress", nullptr, u"c", u"accel",       u"cmd_copy"},       // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"x", u"accel",       u"cmd_cut"},        // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel",       u"cmd_paste"},      // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel",       u"cmd_undo"},       // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel,shift", u"cmd_redo"},       // Win, macOS, Linux, Android, Emacs

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"y", u"accel",       u"cmd_redo"},       // Win, Linux, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS

#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_GTK) ||\
    defined(MOZ_WIDGET_ANDROID)
    {u"keypress", nullptr, u"a", u"accel",       u"cmd_selectAll"},  // Win, macOS, Linux, Android
#endif  // XP_WIN || MOZ_WIDGET_COCOA || MOZ_WIDGET_GTK || MOZ_WIDGET_ANDROID
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"alt",         u"cmd_selectAll"},  // Emacs
#endif  // USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Emacs specific shortcut keys in <textarea>.
     **************************************************************************/
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"control",     u"cmd_beginLine"},                // Emacs
    {u"keypress", nullptr, u"e", u"control",     u"cmd_endLine"},                  // Emacs
    {u"keypress", nullptr, u"b", u"control",     u"cmd_charPrevious"},             // Emacs
    {u"keypress", nullptr, u"f", u"control",     u"cmd_charNext"},                 // Emacs
    {u"keypress", nullptr, u"h", u"control",     u"cmd_deleteCharBackward"},       // Emacs
    {u"keypress", nullptr, u"d", u"control",     u"cmd_deleteCharForward"},        // Emacs
    {u"keypress", nullptr, u"w", u"control",     u"cmd_deleteWordBackward"},       // Emacs
    {u"keypress", nullptr, u"u", u"control",     u"cmd_deleteToBeginningOfLine"},  // Emacs
    {u"keypress", nullptr, u"k", u"control",     u"cmd_deleteToEndOfLine"},        // Emacs
    {u"keypress", nullptr, u"n", u"control",     u"cmd_lineNext"},                 // Emacs
    {u"keypress", nullptr, u"p", u"control",     u"cmd_linePrevious"},             // Emacs
#endif  // USE_EMACS_KEY_BINDINGS
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sBrowserHandlers[] = {
    // clang-format off
    /**************************************************************************
     * Arrow keys to move caret in non-editable element.
     **************************************************************************/
    {u"keypress", u"VK_LEFT",  nullptr, nullptr,  u"cmd_moveLeft"},   // Win, macOS, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, nullptr,  u"cmd_moveRight"},  // Win, macOS, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, nullptr,  u"cmd_moveUp"},     // Win, macOS, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, nullptr,  u"cmd_moveDown"},   // Win, macOS, Linux, Android, Emacs

    /**************************************************************************
     * Arrow keys to select a char/line in non-editable element.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_GTK)
    {u"keypress", u"VK_LEFT",  nullptr, u"shift", u"cmd_selectLeft"},          // Win, macOS, Linux
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectRight"},         // Win, macOS, Linux
    {u"keypress", u"VK_UP",    nullptr, u"shift", u"cmd_selectUp"},            // Win, macOS, Linux
    {u"keypress", u"VK_DOWN",  nullptr, u"shift", u"cmd_selectDown"},          // Win, macOS, Linux
#endif  // XP_WIN || MOZ_WIDGET_COCOA || MOZ_WIDGET_GTK
#if defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"shift", u"cmd_selectCharPrevious"},  // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectCharNext"},      // Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, u"shift", u"cmd_selectLinePrevious"},  // Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, u"shift", u"cmd_selectLineNext"},      // Android, Emacs
#endif  // MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Arrow keys per word in non-editable element.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_wordPrevious"},        // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_wordNext"},            // Android, Emacs
    {u"keypress", u"VK_LEFT",  nullptr, u"control,shift", u"cmd_selectWordPrevious"},  // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"control,shift", u"cmd_selectWordNext"},      // Android, Emacs
#endif  // MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_moveLeft2"},           // Win, Linux
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_moveRight2"},          // Win, Linux
    {u"keypress", u"VK_LEFT",  nullptr, u"control,shift", u"cmd_selectLeft2"},         // Win, Linux
    {u"keypress", u"VK_RIGHT", nullptr, u"control,shift", u"cmd_selectRight2"},        // Win, Linux
#endif  // XP_WIN || MOZ_WIDGET_GTK
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", u"VK_LEFT",  nullptr, u"alt",           u"cmd_moveLeft2"},           // macOS
    {u"keypress", u"VK_RIGHT", nullptr, u"alt",           u"cmd_moveRight2"},          // macOS
    {u"keypress", u"VK_LEFT",  nullptr, u"alt,shift",     u"cmd_selectLeft2"},         // macOS
    {u"keypress", u"VK_RIGHT", nullptr, u"alt,shift",     u"cmd_selectRight2"},        // macOS
#endif  // MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys per block in non-editable element.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    {u"keypress", u"VK_UP",   nullptr, u"control",       u"cmd_moveUp2"},       // Win, Linux
    {u"keypress", u"VK_DOWN", nullptr, u"control",       u"cmd_moveDown2"},     // Win, Linux
    {u"keypress", u"VK_UP",   nullptr, u"control,shift", u"cmd_selectUp2"},     // Win, Linux
    {u"keypress", u"VK_DOWN", nullptr, u"control,shift", u"cmd_selectDown2"},   // Win, Linux
#endif  // XP_WIN || MOZ_WIDGET_GTK
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", u"VK_UP",   nullptr, u"accel",         u"cmd_moveUp2"},       // macOS
    {u"keypress", u"VK_DOWN", nullptr, u"accel",         u"cmd_moveDown2"},     // macOS
    {u"keypress", u"VK_UP",   nullptr, u"alt,shift",     u"cmd_selectUp2"},     // macOS
    {u"keypress", u"VK_DOWN", nullptr, u"alt,shift",     u"cmd_selectDown2"},   // macOS
#endif  // MOZ_WIDGET_COCOA
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_UP",   nullptr, u"alt",           u"cmd_moveTop"},       // Android
    {u"keypress", u"VK_DOWN", nullptr, u"alt",           u"cmd_moveBottom"},    // Android
    {u"keypress", u"VK_UP",   nullptr, u"shift,alt",     u"cmd_selectTop"},     // Android
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt",     u"cmd_selectBottom"},  // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Arrow keys to begin/end of a line in non-editable element.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_LEFT",  nullptr, u"alt",       u"cmd_beginLine"},        // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"alt",       u"cmd_endLine"},          // Android
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,alt", u"cmd_selectBeginLine"},  // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},    // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * PageUp/PageDown keys in non-editable element.
     **************************************************************************/
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,      u"cmd_scrollPageUp"},    // macOS
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,      u"cmd_scrollPageDown"},  // macOS
#endif  // MOZ_WIDGET_COCOA
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,      u"cmd_movePageUp"},      // Win, Linux, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,      u"cmd_movePageDown"},    // Win, Linux, Android, Emacs
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",     u"cmd_selectPageUp"},    // Win, Linux, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",     u"cmd_selectPageDown"},  // Win, Linux, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"alt",       u"cmd_moveTop"},         // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt",       u"cmd_moveBottom"},      // Android
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift,alt", u"cmd_selectTop"},       // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},    // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Home/End keys in non-editable element.
     **************************************************************************/
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", u"VK_HOME", nullptr, nullptr,          u"cmd_scrollTop"},        // macOS
    {u"keypress", u"VK_END",  nullptr, nullptr,          u"cmd_scrollBottom"},     // macOS
#endif  // MOZ_WIDGET_COCOA
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_HOME", nullptr, nullptr,          u"cmd_beginLine"},        // Win, Linux, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, nullptr,          u"cmd_endLine"},          // Win, Linux, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"shift",         u"cmd_selectBeginLine"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"shift",         u"cmd_selectEndLine"},    // Win, Linux, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"control",       u"cmd_moveTop"},          // Win, Linux, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"control",       u"cmd_moveBottom"},       // Win, Linux, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"shift,control", u"cmd_selectTop"},        // Win, Linux, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"shift,control", u"cmd_selectBottom"},     // Win, Linux, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Insert key in non-editable element.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_INSERT",    nullptr, u"control",        u"cmd_copy"},  // Win, Linux, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Delete key in non-editable element.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"shift",   u"cmd_cut"},                // Win, Linux, Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_GTK) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_copy"},               // Linux, Emacs
#endif  // MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},  // Win, Android
#endif  // XP_WIN || MOZ_WIDGET_ANDROID
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"alt",     u"cmd_deleteToEndOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Backspace key in non-editable element.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_BACK", nullptr, u"alt",     u"cmd_deleteToBeginningOfLine"},  // Android
    {u"keypress", u"VK_BACK", nullptr, u"control", u"cmd_deleteWordBackward"},       // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Common editor commands in non-editable element.
     **************************************************************************/
    {u"keypress", nullptr, u"c", u"accel",       u"cmd_copy"},              // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"x", u"accel",       u"cmd_cut"},               // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel",       u"cmd_paste"},             // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel,shift", u"cmd_pasteNoFormatting"}, // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel",       u"cmd_undo"},              // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel,shift", u"cmd_redo"},              // Win, macOS, Linux, Android, Emacs

// Mac uses Option+Shift+Command+V for Paste and Match Style
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", nullptr, u"v", u"accel,alt,shift", u"cmd_pasteNoFormatting"},  // macOS
#endif  // MOZ_WIDGET_COCOA

#if defined(XP_WIN)
    {u"keypress", nullptr, u"y", u"accel",       u"cmd_redo"},       // Win
#endif  // XP_WIN

    {u"keypress", nullptr, u"a", u"accel",       u"cmd_selectAll"},  // Win, macOS, Linux, Android, Emacs
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"alt",         u"cmd_selectAll"},  // Emacs
#endif  // USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Space key in non-editable element.
     **************************************************************************/
    {u"keypress", nullptr, u" ", nullptr,  u"cmd_scrollPageDown"},  // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u" ", u"shift", u"cmd_scrollPageUp"},    // Win, macOS, Linux, Android, Emacs


    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sEditorHandlers[] = {
// clang-format off
    /**************************************************************************
     * Arrow keys to move caret in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, nullptr,  u"cmd_moveLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, nullptr,  u"cmd_moveRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, nullptr,  u"cmd_moveUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, nullptr,  u"cmd_moveDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys to select a char/line in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || \
    defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"shift", u"cmd_selectLeft"},   // Win, Linux, Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectRight"},  // Win, Linux, Android, Emacs
    {u"keypress", u"VK_UP",    nullptr, u"shift", u"cmd_selectUp"},     // Win, Linux, Android, Emacs
    {u"keypress", u"VK_DOWN",  nullptr, u"shift", u"cmd_selectDown"},   // Win, Linux, Android, Emacs
#endif  // Except MOZ_WIDGET_COCOA

    /**************************************************************************
     * Arrow keys per word in HTMLEditor.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_LEFT",  nullptr, u"control",       u"cmd_wordPrevious"},        // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"control",       u"cmd_wordNext"},            // Android, Emacs
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,control", u"cmd_selectWordPrevious"},  // Android, Emacs
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control", u"cmd_selectWordNext"},      // Android, Emacs
#endif  // MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN)
    {u"keypress", u"VK_LEFT",  nullptr, u"accel",         u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"accel",         u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,accel",   u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,accel",   u"cmd_selectRight2"},        // Win
#endif  // XP_WIN

    /**************************************************************************
     * Arrow keys per block in HTMLEditor.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_UP",   nullptr, u"alt",         u"cmd_moveTop"},       // Android
    {u"keypress", u"VK_DOWN", nullptr, u"alt",         u"cmd_moveBottom"},    // Android
    {u"keypress", u"VK_UP",   nullptr, u"shift,alt",   u"cmd_selectTop"},     // Android
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt",   u"cmd_selectBottom"},  // Android
#endif  // MOZ_WIDGET_ANDROID
#if defined(XP_WIN)
    {u"keypress", u"VK_UP",   nullptr, u"accel",       u"cmd_moveUp2"},       // Win
    {u"keypress", u"VK_DOWN", nullptr, u"accel",       u"cmd_moveDown2"},     // Win
    {u"keypress", u"VK_UP",   nullptr, u"shift,accel", u"cmd_selectUp2"},     // Win
    {u"keypress", u"VK_DOWN", nullptr, u"shift,accel", u"cmd_selectDown2"},   // Win
#endif  // XP_WIN

    /**************************************************************************
     * Arrow keys to begin/end of a line in HTMLEditor.
     **************************************************************************/
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_LEFT",  nullptr, u"alt",       u"cmd_beginLine"},        // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"alt",       u"cmd_endLine"},          // Android
    {u"keypress", u"VK_LEFT",  nullptr, u"shift,alt", u"cmd_selectBeginLine"},  // Android
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},    // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * PageUp/PageDown keys in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,      u"cmd_movePageUp"},      // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,      u"cmd_movePageDown"},    // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",     u"cmd_selectPageUp"},    // Win, Android, Emacs
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",     u"cmd_selectPageDown"},  // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"alt",       u"cmd_moveTop"},         // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt",       u"cmd_moveBottom"},      // Android
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift,alt", u"cmd_selectTop"},       // Android
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},    // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Home/End keys in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_HOME", nullptr, nullptr,          u"cmd_beginLine"},        // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, nullptr,          u"cmd_endLine"},          // Win, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"shift",         u"cmd_selectBeginLine"},  // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"shift",         u"cmd_selectEndLine"},    // Win, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"control",       u"cmd_moveTop"},          // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"control",       u"cmd_moveBottom"},       // Win, Android, Emacs
    {u"keypress", u"VK_HOME", nullptr, u"shift,control", u"cmd_selectTop"},        // Win, Android, Emacs
    {u"keypress", u"VK_END",  nullptr, u"shift,control", u"cmd_selectBottom"},     // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Insert key in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_INSERT", nullptr, u"control", u"cmd_copy"},   // Win, Emacs
    {u"keypress", u"VK_INSERT", nullptr, u"shift",   u"cmd_paste"},  // Win, Emacs
#endif  // XP_WIN || USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Delete key in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"shift",   u"cmd_cutOrDelete"},        // Win, Emacs
#endif  // XP_WIN || USE_EMACS_KEY_BINDINGS
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_copyOrDelete"},       // Emacs
#endif  // USE_EMACS_KEY_BINDINGS
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},  // Win, Android
#endif  // XP_WIN || MOZ_WIDGET_ANDROID
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_DELETE", nullptr, u"alt",     u"cmd_deleteToEndOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID

    /**************************************************************************
     * Backspace key in HTMLEditor.
     **************************************************************************/
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) || defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", u"VK_BACK", nullptr, u"control",   u"cmd_deleteWordBackward"},       // Win, Android, Emacs
#endif  // XP_WIN || MOZ_WIDGET_ANDROID || USE_EMACS_KEY_BINDINGS
#if defined(MOZ_WIDGET_ANDROID)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_deleteToBeginningOfLine"},  // Android
#endif  // MOZ_WIDGET_ANDROID
#if defined(XP_WIN)
    {u"keypress", u"VK_BACK", nullptr, u"alt",       u"cmd_undo"},                     // Win
    {u"keypress", u"VK_BACK", nullptr, u"alt,shift", u"cmd_redo"},                     // Win
#endif  // XP_WIN

    /**************************************************************************
     * Common editor commands in HTMLEditor.
     **************************************************************************/
    {u"keypress", nullptr, u"c", u"accel",           u"cmd_copy"},               // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"x", u"accel",           u"cmd_cut"},                // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel",           u"cmd_paste"},              // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"v", u"accel,shift",     u"cmd_pasteNoFormatting"},  // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel",           u"cmd_undo"},               // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u"z", u"accel,shift",     u"cmd_redo"},               // Win, macOS, Linux, Android, Emacs

// Mac uses Option+Shift+Command+V for Paste and Match Style
#if defined(MOZ_WIDGET_COCOA)
    {u"keypress", nullptr, u"v", u"accel,alt,shift", u"cmd_pasteNoFormatting"},  // macOS
#endif  // MOZ_WIDGET_COCOA

#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) ||\
    defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"y", u"accel",           u"cmd_redo"},               // Emacs
#endif  // XP_WIN || MOZ_WIDGET_GTK || USE_EMACS_KEY_BINDINGS

#if defined(XP_WIN) || defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_GTK) ||\
    defined(MOZ_WIDGET_ANDROID)
    {u"keypress", nullptr, u"a", u"accel",           u"cmd_selectAll"},          // Win, macOS, Linux, Android
#endif  // XP_WIN || MOZ_WIDGET_COCOA || MOZ_WIDGET_GTK || MOZ_WIDGET_ANDROID
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"a", u"alt",             u"cmd_selectAll"},          // Emacs
#endif  // USE_EMACS_KEY_BINDINGS

    /**************************************************************************
     * Space key in HTMLEditor.
     **************************************************************************/
    {u"keypress", nullptr, u" ", nullptr,  u"cmd_scrollPageDown"},  // Win, macOS, Linux, Android, Emacs
    {u"keypress", nullptr, u" ", u"shift", u"cmd_scrollPageUp"},    // Win, macOS, Linux, Android, Emacs

    /**************************************************************************
     * Emacs specific shortcut keys in HTMLEditor.
     **************************************************************************/
#if defined(USE_EMACS_KEY_BINDINGS)
    {u"keypress", nullptr, u"h", u"control", u"cmd_deleteCharBackward"},       // Emacs
    {u"keypress", nullptr, u"d", u"control", u"cmd_deleteCharForward"},        // Emacs
    {u"keypress", nullptr, u"k", u"control", u"cmd_deleteToEndOfLine"},        // Emacs
    {u"keypress", nullptr, u"u", u"control", u"cmd_deleteToBeginningOfLine"},  // Emacs
    {u"keypress", nullptr, u"a", u"control", u"cmd_beginLine"},                // Emacs
    {u"keypress", nullptr, u"e", u"control", u"cmd_endLine"},                  // Emacs
    {u"keypress", nullptr, u"b", u"control", u"cmd_charPrevious"},             // Emacs
    {u"keypress", nullptr, u"f", u"control", u"cmd_charNext"},                 // Emacs
    {u"keypress", nullptr, u"p", u"control", u"cmd_linePrevious"},             // Emacs
    {u"keypress", nullptr, u"n", u"control", u"cmd_lineNext"},                 // Emacs
#endif  // USE_EMACS_KEY_BINDINGS
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

}  // namespace mozilla

#undef USE_EMACS_KEY_BINDINGS
