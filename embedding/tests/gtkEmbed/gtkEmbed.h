/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "prtypes.h"
#include "nsIBaseWindow.h"

// just a couple of global functions shared between main.cpp
// and WebBrowserChrome.cpp

class nsIWebBrowserChrome;

// a little ooky, but new windows gotta come from somewhere
nsresult CreateBrowserWindow(PRUint32 aChromeFlags,
           nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow);

nativeWindow CreateNativeWindow(nsIWebBrowserChrome* chrome);

