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
/**********************************************************************
 altmail.c

 Implementation for alternate mailers.
**********************************************************************/

#include "altmail.h"
#include "xp.h"
#include "xfe.h"
#include "prefapi.h"
#include "nspr.h"

int (*altmail_RegisterMailClient)(void) = NULL;
int (*altmail_UnRegisterMailClient)(void) = NULL;
int (*altmail_OpenMailSession)(void*, void*) = NULL;
int (*altmail_CloseMailSession)(void) = NULL;
int (*altmail_ComposeMailMessage)(void* reserved, char* to, char* org, 
								char* subject, char* body, char* 
								cc, char* bcc) = NULL;
int (*altmail_ShowMailBox)(void) = NULL;
int (*altmail_ShowMessageCenter)(void) = NULL;
char* (*altmail_GetMenuItemString)(void) = NULL;
char* (*altmail_GetNewsMenuItemString)(void) = NULL;
char* (*altmail_HandleNewsUrl)(char*) = NULL;


/*
 * change_resource
 */
static void
change_resource(char* resource, char* value)
{
	XrmDatabase database;

	if ( resource == NULL || value == NULL ) return;

	database = XrmGetDatabase(fe_display);
	XrmPutStringResource(&database, resource, value);
}


/*
 * AltMailInit
 */
void
AltMailInit(void)
{
	XP_Bool use_altmail = FALSE;
	char* lib_name;
	PRLibrary* lib;

	PREF_GetBoolPref("mail.use_altmail", &use_altmail);

	if ( use_altmail == FALSE ) return;

	PREF_CopyCharPref("mail.altmail_dll", &lib_name);

	if ( lib_name == NULL || *lib_name == '\0' ) return;

	lib = PR_LoadLibrary(lib_name);

	if ( lib == NULL ) return;

	altmail_RegisterMailClient = PR_FindSymbol(lib, "RegisterMailClient");
	altmail_UnRegisterMailClient = PR_FindSymbol(lib, "UnRegisterMailClient");
	altmail_OpenMailSession = PR_FindSymbol(lib, "OpenMailSession");
	altmail_CloseMailSession = PR_FindSymbol(lib, "CloseMailSession");
	altmail_ComposeMailMessage = PR_FindSymbol(lib, "ComposeMailMessage");
	altmail_ShowMailBox = PR_FindSymbol(lib, "ShowMailBox");
	altmail_ShowMessageCenter = PR_FindSymbol(lib, "ShowMessageCenter");
	altmail_GetMenuItemString = PR_FindSymbol(lib, "GetMenuItemString");
	altmail_GetNewsMenuItemString = PR_FindSymbol(lib, "GetNewsMenuItemString");
	altmail_HandleNewsUrl = PR_FindSymbol(lib, "HandleNewsUrl");

	if ( altmail_RegisterMailClient ) {
		altmail_RegisterMailClient();
	}

	if ( altmail_GetMenuItemString ) {
		change_resource("*menuBar*openInbox.labelString", 
						altmail_GetMenuItemString());
	}

	if ( altmail_GetNewsMenuItemString ) {
		change_resource("*menuBar*openNewsgroups.labelString", 
						altmail_GetNewsMenuItemString());
	}
}


/*
 * AltMailOpenSession
 */
void
AltMailOpenSession(void)
{
	static int session_open = FALSE;

	if ( session_open == FALSE && altmail_OpenMailSession != NULL ) {
		altmail_OpenMailSession(NULL, NULL);
		session_open = TRUE;
	}
}


/*
 * AltMailExit
 */
void
AltMailExit(void)
{
	if ( altmail_CloseMailSession ) {
		altmail_CloseMailSession();
	}
	if ( altmail_UnRegisterMailClient ) {
		altmail_UnRegisterMailClient();
	}
}


/*
 * FE_AlternateCompose
 */
void
FE_AlternateCompose(char* from, char* reply_to, char* to, char* cc, 
					char* bcc, char* fcc, char* newsgroups, 
					char* followup_to, char* organization, char* subject, 
					char* references, char* other_random_headers, 
					char* priority, char* attachment, char* newspost_url, 
					char* body)
{
	if ( altmail_ComposeMailMessage ) {
		altmail_ComposeMailMessage(NULL, to, organization, subject, body,
								   cc, bcc);
	}
}


/*
 * FE_AlternateNewsReader
 */
int
FE_AlternateNewsReader(char* url)
{
	if ( altmail_HandleNewsUrl ) {
		altmail_HandleNewsUrl(url);
		return 1;
	}

	return 0;
}


