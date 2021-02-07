/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)

#  include "ShortcutKeys.h"

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
 */

namespace mozilla {

ShortcutKeyData ShortcutKeys::sInputHandlers[] = {
#  include "ShortcutKeyDefinitionsForInputCommon.h"

    // clang-format off
    {u"keypress", u"VK_HOME",      nullptr, nullptr,           u"cmd_beginLine"},           // Win
    {u"keypress", u"VK_END",       nullptr, nullptr,           u"cmd_endLine"},             // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift",          u"cmd_selectBeginLine"},     // Win
    {u"keypress", u"VK_END",       nullptr, u"shift",          u"cmd_selectEndLine"},       // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift,control",  u"cmd_selectTop"},           // Win
    {u"keypress", u"VK_END",       nullptr, u"shift,control",  u"cmd_selectBottom"},        // Win
    {u"keypress", u"VK_HOME",      nullptr, u"control",        u"cmd_moveTop"},             // Win
    {u"keypress", u"VK_END",       nullptr, u"control",        u"cmd_moveBottom"},          // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"control",        u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"control",        u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"shift,control",  u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"shift,control",  u"cmd_selectRight2"},        // Win
    {u"keypress", u"VK_UP",        nullptr, u"control",        u"cmd_moveUp2"},             // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"control",        u"cmd_moveDown2"},           // Win
    {u"keypress", u"VK_UP",        nullptr, u"shift,control",  u"cmd_selectUp2"},           // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"shift,control",  u"cmd_selectDown2"},         // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"shift",          u"cmd_cutOrDelete"},         // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"control",        u"cmd_deleteWordForward"},   // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"control",        u"cmd_copy"},                // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"shift",          u"cmd_paste"},               // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt",            u"cmd_undo"},                // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt,shift",      u"cmd_redo"},                // Win
    {u"keypress", u"VK_BACK",      nullptr, u"control",        u"cmd_deleteWordBackward"},  // Win
    {u"keypress", nullptr,         u"a",    u"accel",          u"cmd_selectAll"},           // Win
    {u"keypress", nullptr,         u"y",    u"accel",          u"cmd_redo"},                // Win
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sTextAreaHandlers[] = {
#  include "ShortcutKeyDefinitionsForTextAreaCommon.h"

    // clang-format off
    {u"keypress", u"VK_HOME",      nullptr, nullptr,           u"cmd_beginLine"},           // Win
    {u"keypress", u"VK_END",       nullptr, nullptr,           u"cmd_endLine"},             // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift",          u"cmd_selectBeginLine"},     // Win
    {u"keypress", u"VK_END",       nullptr, u"shift",          u"cmd_selectEndLine"},       // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift,control",  u"cmd_selectTop"},           // Win
    {u"keypress", u"VK_END",       nullptr, u"shift,control",  u"cmd_selectBottom"},        // Win
    {u"keypress", u"VK_HOME",      nullptr, u"control",        u"cmd_moveTop"},             // Win
    {u"keypress", u"VK_END",       nullptr, u"control",        u"cmd_moveBottom"},          // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"control",        u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"control",        u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"shift,control",  u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"shift,control",  u"cmd_selectRight2"},        // Win
    {u"keypress", u"VK_UP",        nullptr, u"control",        u"cmd_moveUp2"},             // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"control",        u"cmd_moveDown2"},           // Win
    {u"keypress", u"VK_UP",        nullptr, u"shift,control",  u"cmd_selectUp2"},           // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"shift,control",  u"cmd_selectDown2"},         // Win
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,           u"cmd_movePageUp"},          // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,           u"cmd_movePageDown"},        // Win
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",          u"cmd_selectPageUp"},        // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",          u"cmd_selectPageDown"},      // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"shift",          u"cmd_cutOrDelete"},         // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"control",        u"cmd_deleteWordForward"},   // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"control",        u"cmd_copy"},                // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"shift",          u"cmd_paste"},               // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt",            u"cmd_undo"},                // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt,shift",      u"cmd_redo"},                // Win
    {u"keypress", u"VK_BACK",      nullptr, u"control",        u"cmd_deleteWordBackward"},  // Win
    {u"keypress", nullptr,         u"a",    u"accel",          u"cmd_selectAll"},           // Win
    {u"keypress", nullptr,         u"y",    u"accel",          u"cmd_redo"},                // Win
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sBrowserHandlers[] = {
#  include "ShortcutKeyDefinitionsForBrowserCommon.h"

    // clang-format off
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,           u"cmd_movePageUp"},         // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,           u"cmd_movePageDown"},       // Win
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",          u"cmd_selectPageUp"},       // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",          u"cmd_selectPageDown"},     // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"shift",          u"cmd_cut"},                // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"control",        u"cmd_deleteWordForward"},  // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"control",        u"cmd_copy"},               // Win
    {u"keypress", u"VK_HOME",      nullptr, nullptr,           u"cmd_beginLine"},          // Win
    {u"keypress", u"VK_END",       nullptr, nullptr,           u"cmd_endLine"},            // Win
    {u"keypress", u"VK_HOME",      nullptr, u"control",        u"cmd_moveTop"},            // Win
    {u"keypress", u"VK_END",       nullptr, u"control",        u"cmd_moveBottom"},         // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift,control",  u"cmd_selectTop"},          // Win
    {u"keypress", u"VK_END",       nullptr, u"shift,control",  u"cmd_selectBottom"},       // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"control",        u"cmd_moveLeft2"},          // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"control",        u"cmd_moveRight2"},         // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"control,shift",  u"cmd_selectLeft2"},        // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"control,shift",  u"cmd_selectRight2"},       // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"shift",          u"cmd_selectLeft"},         // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"shift",          u"cmd_selectRight"},        // Win
    {u"keypress", u"VK_UP",        nullptr, u"control",        u"cmd_moveUp2"},            // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"control",        u"cmd_moveDown2"},          // Win
    {u"keypress", u"VK_UP",        nullptr, u"control,shift",  u"cmd_selectUp2"},          // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"control,shift",  u"cmd_selectDown2"},        // Win
    {u"keypress", u"VK_UP",        nullptr, u"shift",          u"cmd_selectUp"},           // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"shift",          u"cmd_selectDown"},         // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift",          u"cmd_selectBeginLine"},    // Win
    {u"keypress", u"VK_END",       nullptr, u"shift",          u"cmd_selectEndLine"},      // Win
    {u"keypress", nullptr,         u"y",    u"accel",          u"cmd_redo"},               // Win
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sEditorHandlers[] = {
#  include "ShortcutKeyDefinitionsForEditorCommon.h"

    // clang-format off
    {u"keypress", nullptr,         u"a",    u"accel",          u"cmd_selectAll"},           // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"shift",          u"cmd_cutOrDelete"},         // Win
    {u"keypress", u"VK_DELETE",    nullptr, u"control",        u"cmd_deleteWordForward"},   // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"control",        u"cmd_copy"},                // Win
    {u"keypress", u"VK_INSERT",    nullptr, u"shift",          u"cmd_paste"},               // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt",            u"cmd_undo"},                // Win
    {u"keypress", u"VK_BACK",      nullptr, u"alt,shift",      u"cmd_redo"},                // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"accel",          u"cmd_moveLeft2"},           // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"accel",          u"cmd_moveRight2"},          // Win
    {u"keypress", u"VK_LEFT",      nullptr, u"shift,accel",    u"cmd_selectLeft2"},         // Win
    {u"keypress", u"VK_RIGHT",     nullptr, u"shift,accel",    u"cmd_selectRight2"},        // Win
    {u"keypress", u"VK_UP",        nullptr, u"accel",          u"cmd_moveUp2"},             // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"accel",          u"cmd_moveDown2"},           // Win
    {u"keypress", u"VK_UP",        nullptr, u"shift,accel",    u"cmd_selectUp2"},           // Win
    {u"keypress", u"VK_DOWN",      nullptr, u"shift,accel",    u"cmd_selectDown2"},         // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift,control",  u"cmd_selectTop"},           // Win
    {u"keypress", u"VK_END",       nullptr, u"shift,control",  u"cmd_selectBottom"},        // Win
    {u"keypress", u"VK_HOME",      nullptr, u"control",        u"cmd_moveTop"},             // Win
    {u"keypress", u"VK_END",       nullptr, u"control",        u"cmd_moveBottom"},          // Win
    {u"keypress", u"VK_BACK",      nullptr, u"control",        u"cmd_deleteWordBackward"},  // Win
    {u"keypress", u"VK_HOME",      nullptr, nullptr,           u"cmd_beginLine"},           // Win
    {u"keypress", u"VK_END",       nullptr, nullptr,           u"cmd_endLine"},             // Win
    {u"keypress", u"VK_HOME",      nullptr, u"shift",          u"cmd_selectBeginLine"},     // Win
    {u"keypress", u"VK_END",       nullptr, u"shift",          u"cmd_selectEndLine"},       // Win
    {u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,           u"cmd_movePageUp"},          // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,           u"cmd_movePageDown"},        // Win
    {u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",          u"cmd_selectPageUp"},        // Win
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",          u"cmd_selectPageDown"},      // Win
    {u"keypress", nullptr,         u"y",    u"accel",          u"cmd_redo"},                // Win
    // clang-format on

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

}  // namespace mozilla

#endif
