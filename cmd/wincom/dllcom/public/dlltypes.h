/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
