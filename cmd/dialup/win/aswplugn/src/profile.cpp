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

//preference call backs
#include "prefapi.h"

#include "xp_mem.h"

#include "resource.h"
#include <winbase.h>
#include <crtdbg.h>

#define MAX_PATH_LENGTH			256

// java includes
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);
void GetProfileDirectory(char *profilePath);

//********************************************************************************
// 
// getCurrentProfileDirectory
//
// gets the current Navigator user profile directory 
//********************************************************************************
extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory(JRIEnv* env,
														  struct netscape_npasw_SetupPlugin* self)
{
	struct java_lang_String *profilePath = NULL;
	int bufSize=256;
	char buf[256];
	buf[0] = '\0';

	if(PREF_OK == PREF_GetCharPref("profile.directory", buf, &bufSize))
	{
		// make sure we append the last '\' in the profile dir path
		strcat(buf, "\\");	
		trace("profile.cpp : GetCurrentProfileDirectory : Got the Current User profile = %s", buf);
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile");
	}

	profilePath = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);

	return (struct java_lang_String *)profilePath;
}


//********************************************************************************
// 
// getCurrentProfileName
//
// gets the current Navigator user profile name 
//********************************************************************************
extern JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetCurrentProfileName(JRIEnv* env,
													 struct netscape_npasw_SetupPlugin* self)
{
	struct java_lang_String *profileName = NULL;
	int bufSize=256;
	char buf[256];
	buf[0] = '\0';

	if(PREF_OK == PREF_GetCharPref("profile.name", buf, &bufSize))
	{
		trace("profile.cpp : GetCurrentProfileDirectory : Got the Current profile name = %s", buf);
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileName : Error in obtaining Current User profile");
	}

	profileName = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);

	return (struct java_lang_String *)profileName;
}

//********************************************************************************
//
// GetProfileDirectory
//
// gets the current profile directory
//********************************************************************************
void GetProfileDirectory(char *profilePath)
{

	int bufSize=256;
	char buf[256];
	buf[0] = '\0';

	if(PREF_OK == PREF_GetCharPref("profile.directory", buf, &bufSize))
	{
		// make sure we append the last '\' in the profile dir path
		strcat(buf, "\\");	
		trace("profile.cpp : GetCurrentProfileDirectory : Got the Current User profile = %s", buf);
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile");
	}

	if (buf && buf[0])
		strcpy(profilePath, buf);
}

//********************************************************************************
// 
// SetCurrentProfileName
//
// changes the current Navigator user profile name to a different name
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fSetCurrentProfileName(JRIEnv* env,
													 struct netscape_npasw_SetupPlugin* self,
													 struct java_lang_String *JSprofileName)
{
	const char* newProfileName = NULL;
	if (JSprofileName != NULL)
		newProfileName = GetStringPlatformChars(env, JSprofileName);
	assert(newProfileName);

	if (NULL == newProfileName)	// abort if string allocation fails
		return;
	
	if (PREF_ERROR == PREF_SetDefaultCharPref("profile.name", newProfileName))
	{
		trace("profile.cpp : SetCurrentProfileName : Error in setting Current User profile name.");
	}
	else
	{
		trace("profile.cpp : SetCurrentProfileName : Current User profile is set to = %s", newProfileName);

		if (PREF_ERROR == PREF_SetDefaultBoolPref("profile.temporary", FALSE)) 
		{
			trace("profile.cpp : SetCurrentProfileName : Error in setting Temporary flag to false.");
		}
		else
		{
			trace("profile.cpp : SetCurrentProfileName : Made the profile to be permanent.");
		}
	}

}
 
