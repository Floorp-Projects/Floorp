/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*	acptlang.c	*/
/*  File implement Accept-Language and Accept-Charset */

#include "intlpriv.h"
#include "prefapi.h"

static char *accept_language = NULL;
static const char *pref_accept_language = "intl.accept_languages";

static char *accept_charset = NULL;
static const char *pref_accept_charset = "intl.accept_charsets";


PRIVATE void FilterOutHumanReadableText(char* str)
{
	if(NULL == str)
		return;
/*
	Currently, we only do this on Window build because
	1. I only know window have this problem
	2. Macintosh should be ok since we do not store this in this
		format on the Mac
	3. I don't know about UNIX for now.

	We probably need to build this for all the platform since
	the preference will be for xp so a window pref could be 
		used on Mac and UNIX

*/
#if ((defined XP_WIN) || (defined XP_UNIX) || (defined XP_OS2))
	if(strchr(str, '[') != NULL) 	/* Searching [ to determinate wheather 		*/
									/* This is "Window-Style" AcceptLang Pref 	*/
	{
		int state= 0;
		char *in, *out;
		/*
			State Machine:

		            State 0      State 1        State 2
		    Hit [   Goto State 1 Copy in (Err!) Emit [,] Goto State 1
		    Hit ]   Ignore in    Goto State 2   ignore In
		    Else    Ignore in    Copy in        ignore In		

			Sample data:
			in: 	AAA BBB [abb-a], CC DD [ee-ff]
			state:  0000000001111112222222221111112
			action:          abb-a         ,ee-ff  \0
			Out: 	abb-a,ee-ff
	

		*/
		for(state=0, in=out=str; *in!=0; in++)
		{
			switch(state)
			{
				case 1:					/* Between [ and ] */
					if(*in == ']')		/* hit ] */
						state = 2;		/* Change state to 2 */
					else
						*out++ = *in;	/* Copy text between [ and ] */
					break;
				case 2:					/* Before hit not-first [ */ 
					if(*in == '[')		/* hit the not-first [ */
					{
						state = 1;		/* Change state to 1 */
						*out++ = ',';	/* Need to copy , */
					}
					break;
				case 0:					/* Before hit first [ */
				default:
					if(*in == '[')		/* hit first [ */
						state = 1;		/* Change state to 1 */
					break;
			}
			
		}
		*out = '\0';					/* NULL terminate output */
	}
#endif
}

/* callback routine invoked by prefapi when the pref value changes */
/* According to the comment in mime2fun.c 
   Win16 build fails if PR_CALLBACK is declared as static
   So I change it to MODULE_PRIVATE
*/

MODULE_PRIVATE int PR_CALLBACK intl_SetAcceptLanguage(const char * newpref, void * data)
{
	if (accept_language) {
		XP_FREE(accept_language);
		accept_language = NULL;
	}
	
	PREF_CopyCharPref(pref_accept_language, &accept_language);
	FilterOutHumanReadableText(accept_language);
	return PREF_NOERROR;
}

/* INTL_GetAcceptLanguage()						*/
/* return the AcceptLanguage from XP Preference */
/* this should be a C style NULL terminated string */
PUBLIC char* INTL_GetAcceptLanguage()
{
	if (accept_language == NULL)
	{
		PREF_CopyCharPref(pref_accept_language, &accept_language);
		
		if (accept_language)
			PREF_RegisterCallback(pref_accept_language, intl_SetAcceptLanguage, NULL);
	}
	FilterOutHumanReadableText(accept_language);
	
	return accept_language;
}

/* callback routine invoked by prefapi when the pref value changes */
/* According to the comment in mime2fun.c 
   Win16 build fails if PR_CALLBACK is declared as static
   So I change it to MODULE_PRIVATE 
*/
MODULE_PRIVATE int PR_CALLBACK intl_SetAcceptCharset(const char * newpref, void * data)
{
	if (accept_charset) {
		XP_FREE(accept_charset);
		accept_charset = NULL;
	}
	
	PREF_CopyCharPref(pref_accept_charset, &accept_charset);
	FilterOutHumanReadableText(accept_charset);
	return PREF_NOERROR;
}

/* INTL_GetAcceptCharset()						*/
/* return the AcceptCharset from XP Preference */
/* this should be a C style NULL terminated string */
PUBLIC char* INTL_GetAcceptCharset()
{
	if (accept_charset == NULL)
	{
		PREF_CopyCharPref(pref_accept_charset, &accept_charset);
		
		if (accept_charset)
			PREF_RegisterCallback(pref_accept_charset, intl_SetAcceptCharset, NULL);
	}
	
	return accept_charset;
}

