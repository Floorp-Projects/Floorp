/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShortcutKeys.h"
#include "mozilla/KeyEventHandler.h"
#include "nsContentUtils.h"
#include "nsAtom.h"
#include "mozilla/TextEvents.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(ShortcutKeys, nsIObserver);

StaticRefPtr<ShortcutKeys> ShortcutKeys::sInstance;

ShortcutKeys::ShortcutKeys()
    : mBrowserHandlers(nullptr),
      mEditorHandlers(nullptr),
      mInputHandlers(nullptr),
      mTextAreaHandlers(nullptr) {
  MOZ_ASSERT(!sInstance, "Attempt to instantiate a second ShortcutKeys.");
  nsContentUtils::RegisterShutdownObserver(this);
}

ShortcutKeys::~ShortcutKeys() {
  delete mBrowserHandlers;
  delete mEditorHandlers;
  delete mInputHandlers;
  delete mTextAreaHandlers;
}

nsresult ShortcutKeys::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  // Clear our strong reference so we can clean up.
  ShortcutKeys::Shutdown();
  return NS_OK;
}

void ShortcutKeys::Shutdown() { sInstance = nullptr; }

/* static */
KeyEventHandler* ShortcutKeys::GetHandlers(HandlerType aType) {
  if (!sInstance) {
    sInstance = new ShortcutKeys();
  }

  return sInstance->EnsureHandlers(aType);
}

/* static */
nsAtom* ShortcutKeys::ConvertEventToDOMEventType(
    const WidgetKeyboardEvent* aWidgetKeyboardEvent) {
  switch (aWidgetKeyboardEvent->mMessage) {
    case eKeyDown:
      return nsGkAtoms::keydown;
    case eKeyUp:
      return nsGkAtoms::keyup;
    // eAccessKeyNotFound event is always created from eKeyPress event and
    // the original eKeyPress event has stopped its propagation before
    // dispatched into the DOM tree in this process and not matched with remote
    // content's access keys.  So, we should treat it as an eKeyPress event and
    // execute a command if it's registered as a shortcut key.
    case eKeyPress:
    case eAccessKeyNotFound:
      return nsGkAtoms::keypress;
    default:
      MOZ_ASSERT_UNREACHABLE(
          "All event messages relating to shortcut keys should be handled");
      return nullptr;
  }
}

KeyEventHandler* ShortcutKeys::EnsureHandlers(HandlerType aType) {
  ShortcutKeyData* keyData;
  KeyEventHandler** cache;

  switch (aType) {
    case HandlerType::eBrowser:
      keyData = &sBrowserHandlers[0];
      cache = &mBrowserHandlers;
      break;
    case HandlerType::eEditor:
      keyData = &sEditorHandlers[0];
      cache = &mEditorHandlers;
      break;
    case HandlerType::eInput:
      keyData = &sInputHandlers[0];
      cache = &mInputHandlers;
      break;
    case HandlerType::eTextArea:
      keyData = &sTextAreaHandlers[0];
      cache = &mTextAreaHandlers;
      break;
    default:
      MOZ_ASSERT(false, "Unknown handler type requested.");
  }

  if (*cache) {
    return *cache;
  }

  KeyEventHandler* lastHandler = nullptr;
  while (keyData->event) {
    KeyEventHandler* handler = new KeyEventHandler(keyData);
    if (lastHandler) {
      lastHandler->SetNextHandler(handler);
    } else {
      *cache = handler;
    }
    lastHandler = handler;
    keyData++;
  }

  return *cache;
}

}  // namespace mozilla
