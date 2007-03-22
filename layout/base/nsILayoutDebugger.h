/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  NS_IMETHOD SetShowFrameBorders(PRBool aEnable) = 0;

  NS_IMETHOD GetShowFrameBorders(PRBool* aResult) = 0;

  NS_IMETHOD SetShowEventTargetFrameBorder(PRBool aEnable) = 0;

  NS_IMETHOD GetShowEventTargetFrameBorder(PRBool* aResult) = 0;

  NS_IMETHOD GetContentSize(nsIDocument* aDocument,
                            PRInt32* aSizeInBytesResult) = 0;

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult) = 0;

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutDebugger, NS_ILAYOUT_DEBUGGER_IID)

#endif /* nsILayoutDebugger_h___ */
