/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindowCommands_h__
#define nsGlobalWindowCommands_h__

#include "nscore.h"

class nsIControllerCommandTable;

class nsWindowCommandRegistration
{
public:
  static nsresult  RegisterWindowCommands(nsIControllerCommandTable *ccm);
};


// XXX find a better home for these
#define NS_WINDOWCONTROLLER_CID        \
 { /* 7BD05C78-6A26-11D7-B16F-0003938A9D96 */       \
  0x7BD05C78, 0x6A26, 0x11D7, {0xB1, 0x6F, 0x00, 0x03, 0x93, 0x8A, 0x9D, 0x96} }

#define NS_WINDOWCONTROLLER_CONTRACTID "@mozilla.org/dom/window-controller;1"

#endif // nsGlobalWindowCommands_h__

