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

#ifndef __PromptService_h
#define __PromptService_h

#include "nsError.h"

// this component is for an MFC app; it's Windows. make sure this is defined.
#ifndef XP_WIN
#define XP_WIN
#endif

class nsIFactory;

// factory creator, in hard and soft link formats
extern "C" NS_EXPORT nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory);
typedef nsresult (__cdecl *MakeFactoryType)(nsIFactory **);
#define kPromptServiceFactoryFuncName "NS_NewPromptServiceFactory"

// initialization function, in hard and soft link formats
extern "C" NS_EXPORT void InitPromptService(HINSTANCE instance);
typedef nsresult (__cdecl *InitPromptServiceType)(HINSTANCE instance);
#define kPromptServiceInitFuncName "InitPromptService"

#endif
