/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../ShortcutKeys.h"

namespace mozilla {

ShortcutKeyData ShortcutKeys::sInputHandlers[] =
{
#include "../ShortcutKeyDefinitionsForInputCommon.h"

    { u"keypress", nullptr,         u"a",    u"control",         u"cmd_beginLine" },
    { u"keypress", nullptr,         u"e",    u"control",         u"cmd_endLine" },
    { u"keypress", nullptr,         u"b",    u"control",         u"cmd_charPrevious" },
    { u"keypress", nullptr,         u"f",    u"control",         u"cmd_charNext" },
    { u"keypress", nullptr,         u"h",    u"control",         u"cmd_deleteCharBackward" },
    { u"keypress", nullptr,         u"d",    u"control",         u"cmd_deleteCharForward" },
    { u"keypress", nullptr,         u"w",    u"control",         u"cmd_deleteWordBackward" },
    { u"keypress", nullptr,         u"u",    u"control",         u"cmd_deleteToBeginningOfLine" },
    { u"keypress", nullptr,         u"k",    u"control",         u"cmd_deleteToEndOfLine" },
    { u"keypress", u"VK_DELETE",    nullptr, u"shift",           u"cmd_cutOrDelete" },
    { u"keypress", u"VK_DELETE",    nullptr, u"control",         u"cmd_copyOrDelete" },
    { u"keypress", u"VK_INSERT",    nullptr, u"control",         u"cmd_copy" },
    { u"keypress", u"VK_INSERT",    nullptr, u"shift",           u"cmd_paste" },
    { u"keypress", u"VK_HOME",      nullptr, nullptr,            u"cmd_beginLine" },
    { u"keypress", u"VK_END",       nullptr, nullptr,            u"cmd_endLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift",           u"cmd_selectBeginLine" },
    { u"keypress", u"VK_END",       nullptr, u"shift",           u"cmd_selectEndLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"control",         u"cmd_beginLine" },
    { u"keypress", u"VK_END",       nullptr, u"control",         u"cmd_endLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"control,shift",   u"cmd_selectBeginLine" },
    { u"keypress", u"VK_END",       nullptr, u"control,shift",   u"cmd_selectEndLine" },
    { u"keypress", u"VK_BACK",      nullptr, u"control",         u"cmd_deleteWordBackward" },
    { u"keypress", u"VK_LEFT",      nullptr, u"control",         u"cmd_wordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"control",         u"cmd_wordNext" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift,control",   u"cmd_selectWordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift,control",   u"cmd_selectWordNext" },
    { u"keypress", nullptr,         u"y",    u"accel",           u"cmd_redo" },
    { u"keypress", nullptr,         u"a",    u"alt",             u"cmd_selectAll" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sTextAreaHandlers[] =
{
#include "../ShortcutKeyDefinitionsForTextAreaCommon.h"

    { u"keypress", nullptr,         u"a",    u"control",         u"cmd_beginLine" },
    { u"keypress", nullptr,         u"e",    u"control",         u"cmd_endLine" },
    { u"keypress", nullptr,         u"b",    u"control",         u"cmd_charPrevious" },
    { u"keypress", nullptr,         u"f",    u"control",         u"cmd_charNext" },
    { u"keypress", nullptr,         u"h",    u"control",         u"cmd_deleteCharBackward" },
    { u"keypress", nullptr,         u"d",    u"control",         u"cmd_deleteCharForward" },
    { u"keypress", nullptr,         u"w",    u"control",         u"cmd_deleteWordBackward" },
    { u"keypress", nullptr,         u"u",    u"control",         u"cmd_deleteToBeginningOfLine" },
    { u"keypress", nullptr,         u"k",    u"control",         u"cmd_deleteToEndOfLine" },
    { u"keypress", u"VK_DELETE",    nullptr, u"shift",           u"cmd_cutOrDelete" },
    { u"keypress", u"VK_DELETE",    nullptr, u"control",         u"cmd_copyOrDelete" },
    { u"keypress", u"VK_INSERT",    nullptr, u"control",         u"cmd_copy" },
    { u"keypress", u"VK_INSERT",    nullptr, u"shift",           u"cmd_paste" },
    { u"keypress", nullptr,         u"n",    u"control",         u"cmd_lineNext" },
    { u"keypress", nullptr,         u"p",    u"control",         u"cmd_linePrevious" },
    { u"keypress", u"VK_HOME",      nullptr, nullptr,            u"cmd_beginLine" },
    { u"keypress", u"VK_END",       nullptr, nullptr,            u"cmd_endLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift",           u"cmd_selectBeginLine" },
    { u"keypress", u"VK_END",       nullptr, u"shift",           u"cmd_selectEndLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"control",         u"cmd_moveTop" },
    { u"keypress", u"VK_END",       nullptr, u"control",         u"cmd_moveBottom" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift,control",   u"cmd_selectTop" },
    { u"keypress", u"VK_END",       nullptr, u"shift,control",   u"cmd_selectBottom" },
    { u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,            u"cmd_movePageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,            u"cmd_movePageDown" },
    { u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",           u"cmd_selectPageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",           u"cmd_selectPageDown" },
    { u"keypress", u"VK_LEFT",      nullptr, u"control",         u"cmd_wordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"control",         u"cmd_wordNext" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift,control",   u"cmd_selectWordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift,control",   u"cmd_selectWordNext" },
    { u"keypress", u"VK_BACK",      nullptr, u"control",         u"cmd_deleteWordBackward" },
    { u"keypress", nullptr,         u"y",    u"accel",           u"cmd_redo" },
    { u"keypress", nullptr,         u"a",    u"alt",             u"cmd_selectAll" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sBrowserHandlers[] =
{
#include "../ShortcutKeyDefinitionsForBrowserCommon.h"

    { u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,            u"cmd_movePageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,            u"cmd_movePageDown" },
    { u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",           u"cmd_selectPageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",           u"cmd_selectPageDown" },
    { u"keypress", u"VK_DELETE",    nullptr, u"shift",           u"cmd_cut" },
    { u"keypress", u"VK_DELETE",    nullptr, u"control",         u"cmd_copy" },
    { u"keypress", u"VK_INSERT",    nullptr, u"control",         u"cmd_copy" },
    { u"keypress", u"VK_HOME",      nullptr, nullptr,            u"cmd_beginLine" },
    { u"keypress", u"VK_END",       nullptr, nullptr,            u"cmd_endLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"control",         u"cmd_moveTop" },
    { u"keypress", u"VK_END",       nullptr, u"control",         u"cmd_moveBottom" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift,control",   u"cmd_selectTop" },
    { u"keypress", u"VK_END",       nullptr, u"shift,control",   u"cmd_selectBottom" },
    { u"keypress", u"VK_LEFT",      nullptr, u"control",         u"cmd_wordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"control",         u"cmd_wordNext" },
    { u"keypress", u"VK_LEFT",      nullptr, u"control,shift",   u"cmd_selectWordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"control,shift",   u"cmd_selectWordNext" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift",           u"cmd_selectCharPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift",           u"cmd_selectCharNext" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift",           u"cmd_selectBeginLine" },
    { u"keypress", u"VK_END",       nullptr, u"shift",           u"cmd_selectEndLine" },
    { u"keypress", u"VK_UP",        nullptr, u"shift",           u"cmd_selectLinePrevious" },
    { u"keypress", u"VK_DOWN",      nullptr, u"shift",           u"cmd_selectLineNext" },
    { u"keypress", nullptr,         u"a",    u"alt",             u"cmd_selectAll" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

ShortcutKeyData ShortcutKeys::sEditorHandlers[] =
{
#include "../ShortcutKeyDefinitionsForEditorCommon.h"

    { u"keypress", nullptr,         u"h",    u"control",         u"cmd_deleteCharBackward" },
    { u"keypress", nullptr,         u"d",    u"control",         u"cmd_deleteCharForward" },
    { u"keypress", nullptr,         u"k",    u"control",         u"cmd_deleteToEndOfLine" },
    { u"keypress", nullptr,         u"u",    u"control",         u"cmd_deleteToBeginningOfLine" },
    { u"keypress", nullptr,         u"a",    u"control",         u"cmd_beginLine" },
    { u"keypress", nullptr,         u"e",    u"control",         u"cmd_endLine" },
    { u"keypress", nullptr,         u"b",    u"control",         u"cmd_charPrevious" },
    { u"keypress", nullptr,         u"f",    u"control",         u"cmd_charNext" },
    { u"keypress", nullptr,         u"p",    u"control",         u"cmd_linePrevious" },
    { u"keypress", nullptr,         u"n",    u"control",         u"cmd_lineNext" },
    { u"keypress", nullptr,         u"x",    u"control",         u"cmd_cut" },
    { u"keypress", nullptr,         u"c",    u"control",         u"cmd_copy" },
    { u"keypress", nullptr,         u"v",    u"control",         u"cmd_paste" },
    { u"keypress", nullptr,         u"z",    u"control",         u"cmd_undo" },
    { u"keypress", nullptr,         u"y",    u"accel",           u"cmd_redo" },
    { u"keypress", nullptr,         u"a",    u"alt",             u"cmd_selectAll" },
    { u"keypress", u"VK_DELETE",    nullptr, u"shift",           u"cmd_cutOrDelete" },
    { u"keypress", u"VK_DELETE",    nullptr, u"control",         u"cmd_copyOrDelete" },
    { u"keypress", u"VK_INSERT",    nullptr, u"control",         u"cmd_copy" },
    { u"keypress", u"VK_INSERT",    nullptr, u"shift",           u"cmd_paste" },
    { u"keypress", u"VK_LEFT",      nullptr, u"control",         u"cmd_wordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"control",         u"cmd_wordNext" },
    { u"keypress", u"VK_LEFT",      nullptr, u"shift,control",   u"cmd_selectWordPrevious" },
    { u"keypress", u"VK_RIGHT",     nullptr, u"shift,control",   u"cmd_selectWordNext" },
    { u"keypress", u"VK_BACK",      nullptr, u"control",         u"cmd_deleteWordBackward" },
    { u"keypress", u"VK_HOME",      nullptr, nullptr,            u"cmd_beginLine" },
    { u"keypress", u"VK_END",       nullptr, nullptr,            u"cmd_endLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift",           u"cmd_selectBeginLine" },
    { u"keypress", u"VK_END",       nullptr, u"shift",           u"cmd_selectEndLine" },
    { u"keypress", u"VK_HOME",      nullptr, u"shift,control",   u"cmd_selectTop" },
    { u"keypress", u"VK_END",       nullptr, u"shift,control",   u"cmd_selectBottom" },
    { u"keypress", u"VK_HOME",      nullptr, u"control",         u"cmd_moveTop" },
    { u"keypress", u"VK_END",       nullptr, u"control",         u"cmd_moveBottom" },
    { u"keypress", u"VK_PAGE_UP",   nullptr, nullptr,            u"cmd_movePageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr,            u"cmd_movePageDown" },
    { u"keypress", u"VK_PAGE_UP",   nullptr, u"shift",           u"cmd_selectPageUp" },
    { u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift",           u"cmd_selectPageDown" },

    { nullptr,     nullptr,         nullptr, nullptr,            nullptr }
};

} // namespace mozilla
