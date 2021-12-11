/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShortcutKeys_h
#define mozilla_ShortcutKeys_h

#include "nsIObserver.h"

class nsAtom;

namespace mozilla {
class KeyEventHandler;
class WidgetKeyboardEvent;

struct ShortcutKeyData {
  const char16_t* event;
  const char16_t* keycode;
  const char16_t* key;
  const char16_t* modifiers;
  const char16_t* command;
};

enum class HandlerType {
  eInput,
  eTextArea,
  eBrowser,
  eEditor,
};

class ShortcutKeys : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // Returns a pointer to the first handler for the given type.
  static KeyEventHandler* GetHandlers(HandlerType aType);

  // Gets the event type for a widget keyboard event.
  static nsAtom* ConvertEventToDOMEventType(
      const WidgetKeyboardEvent* aWidgetKeyboardEvent);

  static void Shutdown();

 protected:
  ShortcutKeys();
  virtual ~ShortcutKeys();

  // Returns a pointer to the first handler for the given type.
  KeyEventHandler* EnsureHandlers(HandlerType aType);

  // Maintains a strong reference to the only instance.
  static StaticRefPtr<ShortcutKeys> sInstance;

  // Shortcut keys for different elements.
  static ShortcutKeyData sBrowserHandlers[];
  static ShortcutKeyData sEditorHandlers[];
  static ShortcutKeyData sInputHandlers[];
  static ShortcutKeyData sTextAreaHandlers[];

  // Cached event handlers generated from the above data.
  KeyEventHandler* mBrowserHandlers;
  KeyEventHandler* mEditorHandlers;
  KeyEventHandler* mInputHandlers;
  KeyEventHandler* mTextAreaHandlers;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_ShortcutKeys_h
