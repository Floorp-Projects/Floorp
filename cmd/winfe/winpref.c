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

#include <windows.h>
#include "jsapi.h"
#include "xp_core.h"
#include "xp_mcom.h"
#include "prefapi.h"

extern HINSTANCE hAppInst;

#define GETRESOURCE(A, B, C)	((LPSTR) LockResource((A) = LoadResource((B), FindResource((B), (C), RT_RCDATA))))
#define ZAPRESOURCE(A)			(UnlockResource(A), FreeResource(A))

static char
szss[] = "%s%s",
szPref[][9] = {
	"init", "all", "mailnews", "editor", "security", "win", "config"
},
szprefs[] = "_prefs"
;

#define _PREFS_		7

int pref_InitInitialObjects(JSContext *js_context, JSObject *js_object)
{
	BOOL fok = 1; jsval result; HGLOBAL hglobal; LPSTR lpstr; char s[MAX_PATH]; register r;

	if (js_context && js_object)
		for (r = 0; r < _PREFS_; r++) {
			wsprintf(s, szss, szPref[r], szprefs);

			fok = ((lpstr = GETRESOURCE(hglobal, hAppInst, s))?
					JS_EvaluateScript(js_context, js_object, lpstr, lstrlen(lpstr), 0, 0, &result): 0);

#ifndef XP_WIN32
			ZAPRESOURCE(hglobal);
#endif

#ifdef DEBUG
			if (!fok) {
					char szdebug[MAX_PATH * 2];

					wsprintf(szdebug, "Initialization failure - %s.", s);
					MessageBox(0, szdebug, "pref_InitInitialObjects", MB_ICONINFORMATION);
			}
#endif
		}

	return(fok);
}
