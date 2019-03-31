/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XPCOM interface for layout-debug extension to reach layout internals */

#ifndef nsILayoutDebugger_h___
#define nsILayoutDebugger_h___

#include "nsISupports.h"

// 1295f7c0-96b3-41fc-93ed-c95dfb712ce7
#define NS_ILAYOUT_DEBUGGER_IID                      \
  {                                                  \
    0x1295f7c0, 0x96b3, 0x41fc, {                    \
      0x93, 0xed, 0xc9, 0x5d, 0xfb, 0x71, 0x2c, 0xe7 \
    }                                                \
  }

/**
 * API for access and control of layout debugging
 */
class nsILayoutDebugger : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILAYOUT_DEBUGGER_IID)

  NS_IMETHOD SetShowFrameBorders(bool aEnable) = 0;

  NS_IMETHOD GetShowFrameBorders(bool* aResult) = 0;

  NS_IMETHOD SetShowEventTargetFrameBorder(bool aEnable) = 0;

  NS_IMETHOD GetShowEventTargetFrameBorder(bool* aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutDebugger, NS_ILAYOUT_DEBUGGER_IID)

#ifdef DEBUG
nsresult NS_NewLayoutDebugger(nsILayoutDebugger** aResult);
#endif /* DEBUG */

#endif /* nsILayoutDebugger_h___ */
