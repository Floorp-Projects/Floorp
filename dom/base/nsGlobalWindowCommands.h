/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindowCommands_h__
#define nsGlobalWindowCommands_h__

#include "nscore.h"

namespace mozilla {
namespace layers {
struct KeyboardScrollAction;
} // namespace layers
} // namespace mozilla

class nsIControllerCommandTable;

class nsWindowCommandRegistration
{
public:
  static nsresult  RegisterWindowCommands(nsIControllerCommandTable *ccm);
};

class nsGlobalWindowCommands
{
public:
  typedef mozilla::layers::KeyboardScrollAction KeyboardScrollAction;

  /**
   * Search through nsGlobalWindowCommands to find the keyboard scrolling action
   * that would be done in response to a command.
   *
   * @param aCommandName the name of the command
   * @param aOutAction the result of searching for this command, must not be null
   * @returns whether a keyboard action was found or not
   */
  static bool FindScrollCommand(const char* aCommandName,
                                KeyboardScrollAction* aOutAction);
};

// XXX find a better home for these
#define NS_WINDOWCONTROLLER_CID        \
 { /* 7BD05C78-6A26-11D7-B16F-0003938A9D96 */       \
  0x7BD05C78, 0x6A26, 0x11D7, {0xB1, 0x6F, 0x00, 0x03, 0x93, 0x8A, 0x9D, 0x96} }

#define NS_WINDOWCONTROLLER_CONTRACTID "@mozilla.org/dom/window-controller;1"

#endif // nsGlobalWindowCommands_h__

