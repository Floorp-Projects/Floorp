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

/*  Simply include this file into a source file
 *      in order to cause linkage of the DLL entry point.
 *
 *  This file exists simply because the DLL entry point can not be
 *      added to the wincom library and still successfully link
 *      due to conflicts with other run time libraries containing
 *      the same routine and is provided solely for your convenience.
 */
#ifdef _WIN32
BOOL WINAPI DllMain(HANDLE hDLL, DWORD, LPVOID)
#else
extern "C" int CALLBACK LibMain(HINSTANCE hDLL, WORD, WORD, LPSTR)
#endif
{
    CComDll::m_hInstance = (HINSTANCE)hDLL;
    return TRUE;
}

