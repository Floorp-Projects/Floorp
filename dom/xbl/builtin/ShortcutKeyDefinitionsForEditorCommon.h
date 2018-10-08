/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    { u"keypress", nullptr,         u" ",    u"shift",           u"cmd_scrollPageUp" },
    { u"keypress", nullptr,         u" ",    nullptr,            u"cmd_scrollPageDown" },
    { u"keypress", u"VK_LEFT",      nullptr, nullptr,            u"cmd_moveLeft" },
    { u"keypress", u"VK_RIGHT",     nullptr, nullptr,            u"cmd_moveRight" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift",           u"cmd_selectLeft" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift",           u"cmd_selectRight" },
    { u"keypress", u"VK_UP",        nullptr, nullptr,            u"cmd_moveUp" },
    { u"keypress", u"VK_DOWN",      nullptr, nullptr,            u"cmd_moveDown" },
    { u"keypress", u"VK_UP",        nullptr, u"shift",           u"cmd_selectUp" },
    { u"keypress", u"VK_DOWN",      nullptr, u"shift",           u"cmd_selectDown" },
    { u"keypress", nullptr,         u"z",    u"accel",           u"cmd_undo" },
    { u"keypress", nullptr,         u"z",    u"accel,shift",     u"cmd_redo" },
    { u"keypress", nullptr,         u"x",    u"accel",           u"cmd_cut" },
    { u"keypress", nullptr,         u"c",    u"accel",           u"cmd_copy" },
    { u"keypress", nullptr,         u"v",    u"accel",           u"cmd_paste" },
    { u"keypress", nullptr,         u"v",    u"accel,shift",     u"cmd_pasteNoFormatting" },
