/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../ShortcutKeys.h"

namespace mozilla {

ShortcutKeyData ShortcutKeys::sInputHandlers[] = {
#include "../ShortcutKeyDefinitionsForInputCommon.h"

    {u"keypress", nullptr, u"a", u"accel", u"cmd_selectAll"},
    {u"keypress", u"VK_LEFT", nullptr, u"control", u"cmd_wordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"control", u"cmd_wordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,control",
     u"cmd_selectWordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control",
     u"cmd_selectWordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"alt", u"cmd_beginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"alt", u"cmd_endLine"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,alt", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},
    {u"keypress", u"VK_HOME", nullptr, nullptr, u"cmd_beginLine"},
    {u"keypress", u"VK_END", nullptr, nullptr, u"cmd_endLine"},
    {u"keypress", u"VK_HOME", nullptr, u"shift", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_END", nullptr, u"shift", u"cmd_selectEndLine"},
    {u"keypress", u"VK_BACK", nullptr, u"alt", u"cmd_deleteToBeginningOfLine"},
    {u"keypress", u"VK_BACK", nullptr, u"control", u"cmd_deleteWordBackward"},
    {u"keypress", u"VK_DELETE", nullptr, u"alt", u"cmd_deleteToEndOfLine"},
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sTextAreaHandlers[] = {
#include "../ShortcutKeyDefinitionsForTextAreaCommon.h"

    {u"keypress", nullptr, u"a", u"accel", u"cmd_selectAll"},
    {u"keypress", u"VK_LEFT", nullptr, u"control", u"cmd_wordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"control", u"cmd_wordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,control",
     u"cmd_selectWordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control",
     u"cmd_selectWordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"alt", u"cmd_beginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"alt", u"cmd_endLine"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,alt", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},
    {u"keypress", u"VK_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, nullptr, u"cmd_movePageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr, u"cmd_movePageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift", u"cmd_selectPageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift", u"cmd_selectPageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_HOME", nullptr, nullptr, u"cmd_beginLine"},
    {u"keypress", u"VK_END", nullptr, nullptr, u"cmd_endLine"},
    {u"keypress", u"VK_HOME", nullptr, u"shift", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_END", nullptr, u"shift", u"cmd_selectEndLine"},
    {u"keypress", u"VK_HOME", nullptr, u"control", u"cmd_moveTop"},
    {u"keypress", u"VK_END", nullptr, u"control", u"cmd_moveBottom"},
    {u"keypress", u"VK_HOME", nullptr, u"shift,control", u"cmd_selectTop"},
    {u"keypress", u"VK_END", nullptr, u"shift,control", u"cmd_selectBottom"},
    {u"keypress", u"VK_BACK", nullptr, u"alt", u"cmd_deleteToBeginningOfLine"},
    {u"keypress", u"VK_BACK", nullptr, u"control", u"cmd_deleteWordBackward"},
    {u"keypress", u"VK_DELETE", nullptr, u"alt", u"cmd_deleteToEndOfLine"},
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sBrowserHandlers[] = {
#include "../ShortcutKeyDefinitionsForBrowserCommon.h"

    {u"keypress", u"VK_LEFT", nullptr, u"shift", u"cmd_selectCharPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift", u"cmd_selectCharNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"control", u"cmd_wordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"control", u"cmd_wordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"control,shift",
     u"cmd_selectWordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"control,shift",
     u"cmd_selectWordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"alt", u"cmd_beginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"alt", u"cmd_endLine"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,alt", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},
    {u"keypress", u"VK_UP", nullptr, u"shift", u"cmd_selectLinePrevious"},
    {u"keypress", u"VK_DOWN", nullptr, u"shift", u"cmd_selectLineNext"},
    {u"keypress", u"VK_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, nullptr, u"cmd_movePageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr, u"cmd_movePageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift", u"cmd_selectPageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift", u"cmd_selectPageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_HOME", nullptr, nullptr, u"cmd_beginLine"},
    {u"keypress", u"VK_END", nullptr, nullptr, u"cmd_endLine"},
    {u"keypress", u"VK_HOME", nullptr, u"shift", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_END", nullptr, u"shift", u"cmd_selectEndLine"},
    {u"keypress", u"VK_HOME", nullptr, u"control", u"cmd_moveTop"},
    {u"keypress", u"VK_END", nullptr, u"control", u"cmd_moveBottom"},
    {u"keypress", u"VK_HOME", nullptr, u"shift,control", u"cmd_selectTop"},
    {u"keypress", u"VK_END", nullptr, u"shift,control", u"cmd_selectBottom"},
    {u"keypress", u"VK_BACK", nullptr, u"alt", u"cmd_deleteToBeginningOfLine"},
    {u"keypress", u"VK_BACK", nullptr, u"control", u"cmd_deleteWordBackward"},
    {u"keypress", u"VK_DELETE", nullptr, u"alt", u"cmd_deleteToEndOfLine"},
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

ShortcutKeyData ShortcutKeys::sEditorHandlers[] = {
#include "../ShortcutKeyDefinitionsForEditorCommon.h"

    {u"keypress", nullptr, u"a", u"accel", u"cmd_selectAll"},
    {u"keypress", u"VK_LEFT", nullptr, u"control", u"cmd_wordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"control", u"cmd_wordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,control",
     u"cmd_selectWordPrevious"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,control",
     u"cmd_selectWordNext"},
    {u"keypress", u"VK_LEFT", nullptr, u"alt", u"cmd_beginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"alt", u"cmd_endLine"},
    {u"keypress", u"VK_LEFT", nullptr, u"shift,alt", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_RIGHT", nullptr, u"shift,alt", u"cmd_selectEndLine"},
    {u"keypress", u"VK_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, nullptr, u"cmd_movePageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, nullptr, u"cmd_movePageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift", u"cmd_selectPageUp"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift", u"cmd_selectPageDown"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"alt", u"cmd_moveTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"alt", u"cmd_moveBottom"},
    {u"keypress", u"VK_PAGE_UP", nullptr, u"shift,alt", u"cmd_selectTop"},
    {u"keypress", u"VK_PAGE_DOWN", nullptr, u"shift,alt", u"cmd_selectBottom"},
    {u"keypress", u"VK_HOME", nullptr, nullptr, u"cmd_beginLine"},
    {u"keypress", u"VK_END", nullptr, nullptr, u"cmd_endLine"},
    {u"keypress", u"VK_HOME", nullptr, u"shift", u"cmd_selectBeginLine"},
    {u"keypress", u"VK_END", nullptr, u"shift", u"cmd_selectEndLine"},
    {u"keypress", u"VK_HOME", nullptr, u"control", u"cmd_moveTop"},
    {u"keypress", u"VK_END", nullptr, u"control", u"cmd_moveBottom"},
    {u"keypress", u"VK_HOME", nullptr, u"shift,control", u"cmd_selectTop"},
    {u"keypress", u"VK_END", nullptr, u"shift,control", u"cmd_selectBottom"},
    {u"keypress", u"VK_BACK", nullptr, u"alt", u"cmd_deleteToBeginningOfLine"},
    {u"keypress", u"VK_BACK", nullptr, u"control", u"cmd_deleteWordBackward"},
    {u"keypress", u"VK_DELETE", nullptr, u"alt", u"cmd_deleteToEndOfLine"},
    {u"keypress", u"VK_DELETE", nullptr, u"control", u"cmd_deleteWordForward"},

    {nullptr, nullptr, nullptr, nullptr, nullptr}};

}  // namespace mozilla
