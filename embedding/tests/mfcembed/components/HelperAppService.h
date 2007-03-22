/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

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
