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

// Common header file

#ifdef _WIN32
#include <windows.h>
#else
#include <ole2ui.h>
#include <dispatch.h>
#endif
#include <olectl.h>
#include <windowsx.h>

#ifndef _WIN32
typedef DWORD	LCID;

#define GET_WM_COMMAND_ID(wp, lp)               ((int)(wp))
#define GET_WM_COMMAND_HWND(wp, lp)             ((HWND)LOWORD(lp))
#define GET_WM_COMMAND_CMD(wp, lp)              ((UINT)HIWORD(lp))
#endif

