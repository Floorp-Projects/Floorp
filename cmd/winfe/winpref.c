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

#include "windows.h"
#include "jsapi.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "prefapi.h"

extern HINSTANCE hAppInst;

int pref_InitInitialObjects(JSContext *js_context,JSObject *js_object) {
    int ok;
    jsval result;
	HRSRC hFound;
	HGLOBAL hRes;
	char * lpBuff = NULL;
	XP_File fp;
	XP_StatStruct stats;
	long fileLength;

	if (!(js_context&&js_object)) return FALSE;

	hFound = FindResource(hAppInst, "init_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;

#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

#ifdef DEBUG
	if (!ok) {
		MessageBox(NULL,"Initial preference resource failed!  Is there a bug in initpref.js? -- Aborting","Debug Error!", MB_OK);
		return ok;
	}
#endif


	hFound = FindResource(hAppInst, "all_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

	hFound = FindResource(hAppInst, "mailnews_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

	hFound = FindResource(hAppInst, "editor_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

	hFound = FindResource(hAppInst, "security_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

	hFound = FindResource(hAppInst, "win_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} 
	else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

#ifdef DEBUG
	if (!ok) MessageBox(NULL,"Initial preference resource failed!  Is there a bug in all.js or winpref.js? -- Aborting","Debug Error!", MB_OK);
#endif

	hFound = FindResource(hAppInst, "config_prefs", RT_RCDATA);
	hRes = LoadResource(hAppInst, hFound);
	lpBuff = (char *)LockResource(hRes);
    if (lpBuff) {
    	ok = JS_EvaluateScript(js_context,js_object,
			      lpBuff, strlen(lpBuff), NULL, 0,
			      &result);
	} else ok = JS_FALSE;
#ifndef XP_WIN32	
	UnlockResource(hRes);
	FreeResource(hRes);
#endif

#ifdef DEBUG
	//if (!ok) MessageBox(NULL,"Initial preference resource failed!  Is there a bug in all.js? -- Aborting","Debug Error!", MB_OK);
#endif

	return ok;
}
