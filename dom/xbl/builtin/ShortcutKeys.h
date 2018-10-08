/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ShortcutKeys_h
#define mozilla_dom_ShortcutKeys_h

namespace mozilla {

typedef struct
{
   const char16_t* event;
   const char16_t* keycode;
   const char16_t* key;
   const char16_t* modifiers;
   const char16_t* command;
} ShortcutKeyData;

class ShortcutKeys
{
protected:
  static ShortcutKeyData sBrowserHandlers[];
  static ShortcutKeyData sEditorHandlers[];
  static ShortcutKeyData sInputHandlers[];
  static ShortcutKeyData sTextAreaHandlers[];
};

} // namespace mozilla

#endif // #ifndef mozilla_dom_ShortcutKeys_h
