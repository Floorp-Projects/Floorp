/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../ShortcutKeys.h"

namespace mozilla {

ShortcutKeyData ShortcutKeys::sInputHandlers[] =
{
    { u"keypress", nullptr,         u"c",    u"accel",           u"cmd_copy" },
    { u"keypress", nullptr,         u"x",    u"accel",           u"cmd_cut" },
    { u"keypress", nullptr,         u"v",    u"accel",           u"cmd_paste" },
    { u"keypress", nullptr,         u"z",    u"accel",           u"cmd_undo" },
    { u"keypress", nullptr,         u"z",    u"accel,shift",     u"cmd_redo" },
    { u"keypress", nullptr,         u"a",    u"accel",           u"cmd_selectAll" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sTextAreaHandlers[] =
{
    { u"keypress", nullptr,         u"c",    u"accel",           u"cmd_copy" },
    { u"keypress", nullptr,         u"x",    u"accel",           u"cmd_cut" },
    { u"keypress", nullptr,         u"v",    u"accel",           u"cmd_paste" },
    { u"keypress", nullptr,         u"z",    u"accel",           u"cmd_undo" },
    { u"keypress", nullptr,         u"z",    u"accel,shift",     u"cmd_redo" },
    { u"keypress", nullptr,         u"a",    u"accel",           u"cmd_selectAll" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sBrowserHandlers[] =
{
#include "../ShortcutKeyDefinitionsForBrowserCommon.h"

    { u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,            u"cmd_scrollPageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,            u"cmd_scrollPageDown" },
    { u"keypress", u"VK_HOME",      nullptr, nullptr,            u"cmd_scrollTop" },
    { u"keypress", u"VK_END",       nullptr, nullptr,            u"cmd_scrollBottom" },
    { u"keypress", u"VK_LEFT",      nullptr, u"alt",             u"cmd_moveLeft2" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"alt",             u"cmd_moveRight2" },
    { u"keypress", u"VK_LEFT",      nullptr, u"alt,shift",       u"cmd_selectLeft2" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"alt,shift",       u"cmd_selectRight2" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift",           u"cmd_selectLeft" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift",           u"cmd_selectRight" },
    { u"keypress", u"VK_UP",        nullptr, u"alt,shift",       u"cmd_selectUp2" },
    { u"keypress", u"VK_DOWN",      nullptr, u"alt,shift",       u"cmd_selectDown2" },
    { u"keypress", u"VK_UP",        nullptr, u"shift",           u"cmd_selectUp" },
    { u"keypress", u"VK_DOWN",      nullptr, u"shift",           u"cmd_selectDown" },
    { u"keypress", u"VK_UP",        nullptr, u"accel",           u"cmd_moveUp2" },
    { u"keypress", u"VK_DOWN",      nullptr, u"accel",           u"cmd_moveDown2" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sEditorHandlers[] =
{
    { u"keypress", nullptr,         u" ",    u"shift",           u"cmd_scrollPageUp" },
    { u"keypress", nullptr,         u" ",    nullptr,            u"cmd_scrollPageDown" },
    { u"keypress", nullptr,         u"z",    u"accel",           u"cmd_undo" },
    { u"keypress", nullptr,         u"z",    u"accel,shift",     u"cmd_redo" },
    { u"keypress", nullptr,         u"x",    u"accel",           u"cmd_cut" },
    { u"keypress", nullptr,         u"c",    u"accel",           u"cmd_copy" },
    { u"keypress", nullptr,         u"v",    u"accel",           u"cmd_paste" },
    { u"keypress", nullptr,         u"v",    u"accel,shift",     u"cmd_pasteNoFormatting" },
    { u"keypress", nullptr,         u"a",    u"accel",           u"cmd_selectAll" },
    { u"keypress", nullptr,         u"v",    u"accel,alt,shift", u"cmd_pasteNoFormatting" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

} // namespace mozilla
