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

#include <npapi.h>
#include "plugin.h"

// windows include
#ifdef WIN32
#include <winbase.h>
#else
#include <windows.h>
#include <ctype.h>
#endif

#include <string.h>
#include <stdio.h>
#include <time.h>


// java include 
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"


extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *JSstring);

BOOL ConvertPassword(LPCSTR lpszPassword, LPSTR lpBuf)
{
    strcpy(lpBuf, lpszPassword);
    return TRUE;
}


extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fEncryptString(JRIEnv* env,
											 struct netscape_npasw_SetupPlugin* self,
											 struct java_lang_String *JSclrTextStr)
{
	return (struct java_lang_String *)JSclrTextStr;
}

extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fEncryptPassword(JRIEnv* env,
											   struct netscape_npasw_SetupPlugin* self,
											   struct java_lang_String *JSclrTextPwd)
{
	return (struct java_lang_String *)JSclrTextPwd;
}
