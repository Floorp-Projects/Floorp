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

