/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ShortcutKeys.h"
#include "../nsXBLPrototypeHandler.h"
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
  sInstance = nullptr;
  return NS_OK;
}

/* static */
nsXBLPrototypeHandler* ShortcutKeys::GetHandlers(HandlerType aType) {
  if (!sInstance) {
    sInstance = new ShortcutKeys();
  }

  return sInstance->EnsureHandlers(aType);
}

/* static */
nsAtom* ShortcutKeys::ConvertEventToDOMEventType(
    const WidgetKeyboardEvent* aWidgetKeyboardEvent) {
  if (aWidgetKeyboardEvent->IsKeyDownOrKeyDownOnPlugin()) {
    return nsGkAtoms::keydown;
  }
  if (aWidgetKeyboardEvent->IsKeyUpOrKeyUpOnPlugin()) {
    return nsGkAtoms::keyup;
  }
  // eAccessKeyNotFound event is always created from eKeyPress event and
  // the original eKeyPress event has stopped its propagation before dispatched
  // into the DOM tree in this process and not matched with remote content's
  // access keys.  So, we should treat it as an eKeyPress event and execute
  // a command if it's registered as a shortcut key.
  if (aWidgetKeyboardEvent->mMessage == eKeyPress ||
      aWidgetKeyboardEvent->mMessage == eAccessKeyNotFound) {
    return nsGkAtoms::keypress;
  }
  MOZ_ASSERT_UNREACHABLE(
      "All event messages relating to shortcut keys should be handled");
  return nullptr;
}

nsXBLPrototypeHandler* ShortcutKeys::EnsureHandlers(HandlerType aType) {
  ShortcutKeyData* keyData;
  nsXBLPrototypeHandler** cache;

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

  nsXBLPrototypeHandler* lastHandler = nullptr;
  while (keyData->event) {
    nsXBLPrototypeHandler* handler = new nsXBLPrototypeHandler(keyData);
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
