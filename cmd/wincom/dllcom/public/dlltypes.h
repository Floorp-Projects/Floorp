/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __DllTypes_H
#define __DllTypes_H

//  Common windows header file.
#include <windows.h>

//  Do not depend on normal netscape mechanisms to
//      control code compilation, compilation
//      options, macro expansion, etc....
//  These DLLs may exist entirely outside of that
//      build system and use DLL custom macros
//      to do things the right way under the
//      restrictive conditions that a DLL runs.
//  Failure to comply will undoubtedly result
//      in many long nights working with a 16
//      bit debugger.

#if defined(_WINDOWS) && !defined(WIN32)
#define DLL_WIN16
#else
#define DLL_WIN32
#endif

#if defined(_DEBUG)
#define DLL_DEBUG
#else
#define DLL_RELEASE
#endif

#endif // __DllTypes_H
