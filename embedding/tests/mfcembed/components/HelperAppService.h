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
 *   Chak Nanga <chak@netscape.com> 
 */
#ifndef __HelperAppService_h
#define __HelperAppService_h

#include "nsError.h"

// this component is for an MFC app; it's Windows. make sure this is defined.
#ifndef XP_WIN
#define XP_WIN
#endif

class nsIFactory;

// factory creator, in hard and soft link formats
extern "C" NS_EXPORT nsresult NS_NewHelperAppDlgFactory(nsIFactory** aFactory);
typedef nsresult (__cdecl *MakeFactoryType)(nsIFactory **);
#define kHelperAppDlgFactoryFuncName "NS_NewHelperAppDlgFactory"

// initialization function, in hard and soft link formats
extern "C" NS_EXPORT void InitHelperAppDlg(HINSTANCE instance);
typedef nsresult (__cdecl *InitHelperAppDlgType)(HINSTANCE instance);
#define kHelperAppDlgInitFuncName "InitHelperAppDlg"

#endif
