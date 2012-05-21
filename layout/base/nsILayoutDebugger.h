/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XPCOM interface for layout-debug extension to reach layout internals */

#ifndef nsILayoutDebugger_h___
#define nsILayoutDebugger_h___

#include "nsISupports.h"

class nsIDocument;
class nsIPresShell;

/* a6cf90f8-15b3-11d2-932e-00805f8add32 */
#define NS_ILAYOUT_DEBUGGER_IID \
 { 0xa6cf90f8, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

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

  NS_IMETHOD GetContentSize(nsIDocument* aDocument,
                            PRInt32* aSizeInBytesResult) = 0;

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult) = 0;

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutDebugger, NS_ILAYOUT_DEBUGGER_IID)

#endif /* nsILayoutDebugger_h___ */
