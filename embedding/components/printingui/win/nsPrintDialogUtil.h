/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsFlyOwnDialog_h___
#define nsFlyOwnDialog_h___

nsresult NativeShowPrintDialog(HWND                aHWnd,
                               nsIWebBrowserPrint* aWebBrowserPrint,
                               nsIPrintSettings*   aPrintSettings);

HGLOBAL CreateGlobalDevModeAndInit(const nsXPIDLString& aPrintName, nsIPrintSettings* aPS);

#endif /* nsFlyOwnDialog_h___ */
