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

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#include <winbase.h>
#include <crtdbg.h>
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#include <windows.h>
#include <string.h>
#include <helper16.h>

#define INI_USER_SECTION				"Users"
#define INI_USERADDINFO_SECTION		"Users Additional Info"
#define INI_CURRUSER_KEY				"CurrentUser"
#endif       // !WIN32

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
	char buf[_MAX_PATH];
	DWORD bufsize = sizeof(buf);
	buf[0] = '\0';

#ifdef WIN32 // ***************************** WIN32 ********************************
	HKEY hKey;
	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return NULL;
	strcpy(keyPath, "SOFTWARE\\Netscape\\Netscape Navigator\\Users");

	// finds the user profile path in registry
	if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey))
	{

		if (ERROR_SUCCESS == RegQueryValueEx(hKey, "CurrentUser", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize)) {

			RegCloseKey(hKey);

			strcat(keyPath, "\\");
			strcat(keyPath, buf);

			buf[0]='\0';
			bufsize = sizeof(buf);	// bufsize got reset from the last RegQueryValueEx

			if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey)) {

				if (ERROR_SUCCESS == RegQueryValueEx(hKey, "DirRoot", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize)) {

					// make sure we append the last '\' in the profile dir path
					strcat(buf, "\\");	

					int len = strlen(buf);
					profilePath = JRI_NewStringPlatform(env, buf, len, NULL, 0);

					RegCloseKey(hKey);

				}
			}
		}
	}

	free(keyPath);

#else  // ***************************** WIN16 ********************************
    
   GetProfileDirectory(buf);
	profilePath = JRI_NewStringUTF(env, buf, strlen(buf));

#endif

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
	char buf[_MAX_PATH];
	DWORD bufsize = sizeof(buf);
	buf[0] = '\0';

#ifdef WIN32  // ***************************** WIN32 ********************************

	HKEY hKey;

	// finds the user profile path in registry
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                     "SOFTWARE\\Netscape\\Netscape Navigator\\Users", 
                                     NULL, 
                                     KEY_ALL_ACCESS, 
                                     &hKey))
	{

		RegQueryValueEx(hKey, "CurrentUser", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize);
		profileName = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);
		RegCloseKey(hKey);

	}

#else         // ***************************** WIN16 ********************************

   // get current profile name
   GetPrivateProfileString(INI_USERADDINFO_SECTION, INI_CURRUSER_KEY, 
                    			"\0", buf, (int) bufsize, INI_NETSCAPE_FILE);
   assert('\0' != buf[0]);
   if ('\0' == '\0')
   	return NULL;

	profileName = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);

#endif

	return (struct java_lang_String *)profileName;
}



//********************************************************************************
// copyRegKeys
//
// copies registry keys
//********************************************************************************
#ifdef WIN32
BOOL  CopyRegKeys(HKEY  hKeyOldName,
						HKEY  hKeyNewName,
						DWORD subkeys,
						DWORD maxSubKeyLen,
						DWORD maxClassLen,
						DWORD values,
						DWORD maxValueNameLen,
						DWORD maxValueLen,
						char *OldPath,
						char *NewPath)
{
	BOOL Err = FALSE;
	DWORD index;


	// first loop through and copies all the value keys
	if (values > 0) {

		DWORD valueNameSize = maxValueNameLen + 1;
		char *valueName = (char *)malloc(sizeof(char) * valueNameSize);
		DWORD dataSize = maxValueLen + 1;
		unsigned char *data = (unsigned char *)malloc(sizeof(char) * dataSize);
		DWORD type;

		if ((valueName) && (data)) {

			for (index=0; index<values; index++) {

				// gets each value name and it's data
				if (ERROR_SUCCESS == RegEnumValue(hKeyOldName, index, valueName, &valueNameSize, NULL, &type, data, &dataSize)) {
				
					// create value in our new profile key
					if (ERROR_SUCCESS != RegSetValueEx(hKeyNewName, valueName, 0, type, data, dataSize)) {
						// unknown err occured... what do we do?
						Err = TRUE;
						return FALSE;
					}
				}

				valueNameSize = maxValueNameLen + 1;
				dataSize = maxValueLen + 1;
			}

			free(valueName);
			free(data);
		}
	}


	// next, we need to creates subkey folders
	if (subkeys > 0) {

		char OldSubkeyPath[260];
		char NewSubkeyPath[260];
		HKEY hkeyOldSubkey;
		HKEY hkeyNewSubkey;

		for (index=0; index<subkeys; index++) {

			DWORD subkeyNameSize = maxSubKeyLen + 1;
			char *subkeyName = (char *)malloc(sizeof(char) * subkeyNameSize);

			if (subkeyName) {

				// gets each subkey name
				if (ERROR_SUCCESS == RegEnumKey(hKeyOldName, index, subkeyName, subkeyNameSize)) {
				
					// create subkey in our new profile keypath
					if (ERROR_SUCCESS != RegCreateKey(hKeyNewName, subkeyName, &hkeyNewSubkey))  {
						Err = TRUE;
						return FALSE;
					}

					// now the subkey is created, we need to get hkey value for oldpath and 
					// then copy everything in the keys again

					// construct key path
					strcpy((char *)OldSubkeyPath, OldPath);
					strcat((char *)OldSubkeyPath, "\\");
					strcat((char *)OldSubkeyPath, subkeyName);

					strcpy((char *)NewSubkeyPath, NewPath);
					strcat((char *)NewSubkeyPath, "\\");
					strcat((char *)NewSubkeyPath, subkeyName);

					free(subkeyName);


					// now... try to start the copying process for our subkey here
					// first, gets the hkey for the old subkey profile path
					if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, (char *)OldSubkeyPath, NULL, KEY_ALL_ACCESS, &hkeyOldSubkey)) {
			
						DWORD SubKeys;
						DWORD MaxSubKeyLen;
						DWORD MaxClassLen;
						DWORD Values;
						DWORD MaxValueNameLen;
						DWORD MaxValueLen;
						DWORD SecurityDescriptor;
						FILETIME LastWriteTime;

						// get some information about this old profile subkey 
						if (ERROR_SUCCESS == RegQueryInfoKey(hkeyOldSubkey, NULL, NULL, NULL, &SubKeys, &MaxSubKeyLen, &MaxClassLen, &Values, &MaxValueNameLen, &MaxValueLen, &SecurityDescriptor, &LastWriteTime)) {

							// copy the values & key stuff

							Err = CopyRegKeys(hkeyOldSubkey, hkeyNewSubkey, SubKeys, MaxSubKeyLen, MaxClassLen, Values, MaxValueNameLen, MaxValueLen, (char *)OldSubkeyPath, (char *)NewSubkeyPath);

							if (!Err)
								return FALSE;

						}

						RegCloseKey(hkeyOldSubkey);
					}
				}
			}
		}
	}

	return TRUE;
}
#endif // WIN32


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
	
#ifdef WIN32  // ***************************** WIN32 ********************************

	char *newPath;
	char *oldPath;
	char *Path	= "SOFTWARE\\Netscape\\Netscape Navigator\\Users";
	char buf[_MAX_PATH];
	DWORD bufsize = sizeof(buf);

	HKEY hKeyUsers;
	HKEY hKeyNewName;
	HKEY hKeyOldName;

	DWORD dwDisposition;
	BOOL Err = FALSE;

	if (JSprofileName != NULL)
		newProfileName = GetStringPlatformChars(env, JSprofileName);

	oldPath = (char *)malloc(sizeof(char) * 260);
	newPath = (char *)malloc(sizeof(char) * 260);

	if ((!oldPath) && (!newPath))
		return;

	// finds the user profile path in registry
	if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, Path, NULL, KEY_ALL_ACCESS, &hKeyUsers))  {

		// finds out the value of CurrentUser (the default user)
		if (ERROR_SUCCESS == RegQueryValueEx(hKeyUsers, "CurrentUser", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize)) {

			// old user profile key path
			strcpy (oldPath, Path);
			strcat (oldPath, "\\");
			strcat (oldPath, buf);

			// new user profile key path
			strcpy (newPath, Path);
			strcat (newPath, "\\");
			strcat (newPath, newProfileName);

			// check if the new ProfileName is same as old ProfileName, if it is, we don't
			// need to do anything.
			if (strcmp(oldPath, newPath) == 0) {
				RegCloseKey(hKeyUsers);
				free(newPath);
				free(oldPath);
				return;
			}

			// create a new key for our new profile name
			if (ERROR_SUCCESS == RegCreateKeyEx(HKEY_LOCAL_MACHINE, newPath, NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKeyNewName, &dwDisposition)) {

				// now... we enumerate old profile key contents and copy everything into new profile key
				// first, gets the hkey for the old profile path
				if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, oldPath, NULL, KEY_ALL_ACCESS, &hKeyOldName)) {
		
					DWORD subKeys;
					DWORD maxSubKeyLen;
					DWORD maxClassLen;
					DWORD values;
					DWORD maxValueNameLen;
					DWORD maxValueLen;
					DWORD securityDescriptor;
					FILETIME lastWriteTime;

					// get some information about this old profile key 
					if (ERROR_SUCCESS == RegQueryInfoKey(hKeyOldName, NULL, NULL, NULL, &subKeys, &maxSubKeyLen, &maxClassLen, &values, &maxValueNameLen, &maxValueLen, &securityDescriptor, &lastWriteTime)) {

						// copy the values
						BOOL NoErr = CopyRegKeys(hKeyOldName, hKeyNewName, subKeys, maxSubKeyLen, maxClassLen, values, maxValueNameLen, maxValueLen, oldPath, newPath);

						// delete the old Key, if everything went well
						if (NoErr) {

							if (ERROR_SUCCESS != RegDeleteKey(hKeyUsers, buf)) {
								// unknown err happened, can't delete key
								Err = TRUE;
							}

							// also, resets the CurrentUser key to point to the newly renamed key
							if (hKeyUsers) {

								char *CurrentUserkey = "CurrentUser";
								long ret = RegSetValueEx(hKeyUsers, CurrentUserkey, 0, REG_SZ, (unsigned char *)newProfileName, strlen(newProfileName) + 1);
							}
						}
					}
				}

				RegCloseKey(hKeyUsers);
				RegCloseKey(hKeyNewName);
				RegCloseKey(hKeyOldName);

				free(oldPath);
			}

			free(newPath);
		}
	}

#else         // ***************************** WIN16 ********************************

	char szCurrProfileName[_MAX_PATH], szCurrProfilePath[_MAX_PATH];

	// get current profile name
	// [Users Additional Info]
	// CurrentUser=User1
	GetPrivateProfileString(INI_USERADDINFO_SECTION, INI_CURRUSER_KEY, 
                    			"\0", szCurrProfileName, (int) _MAX_PATH, INI_NETSCAPE_FILE);
	assert('\0' != szCurrProfileName[0]);
	if ('\0' == szCurrProfileName[0])
		return;

	// get profile directory path
	// [Users]
	// User1=C:\netscape\users\user1
	GetPrivateProfileString(INI_USER_SECTION, szCurrProfileName, 
                    			"\0", szCurrProfilePath, (int) _MAX_PATH, INI_NETSCAPE_FILE);
	assert('\0' != szCurrProfilePath[0]);
	if ('\0' == szCurrProfilePath[0])
		return;

	// write new current user
	// [Users Additional Info]
	// CurrentUser=newuser
	WritePrivateProfileString(INI_USERADDINFO_SECTION, INI_CURRUSER_KEY, 
							  newProfileName, INI_NETSCAPE_FILE);
	// delete old profile directory path
	// [Users]
	// User1=C:\netscape\users\User1
	WritePrivateProfileString(INI_USER_SECTION, szCurrProfileName, 
							  NULL, INI_NETSCAPE_FILE);
	// write profile directory path
	// [Users]
	// NewUser=C:\netscape\users\newuser
	WritePrivateProfileString(INI_USER_SECTION, newProfileName, 
							  szCurrProfilePath, INI_NETSCAPE_FILE);

#endif
}
 
//********************************************************************************
//
// GetProfileDirectory
//
// gets the current profile directory
//********************************************************************************
void GetProfileDirectory(char *profilePath)
{
	char buf[_MAX_PATH];
	DWORD bufsize = sizeof(buf);
	buf[0] = '\0';

#ifdef WIN32
 	HKEY hKey;
	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return;
	strcpy(keyPath, "SOFTWARE\\Netscape\\Netscape Navigator\\Users");

	// finds the user profile path in registry
	if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey))
	{

		if (ERROR_SUCCESS == RegQueryValueEx(hKey, "CurrentUser", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize)) {

			RegCloseKey(hKey);

			strcat(keyPath, "\\");
			strcat(keyPath, buf);

			buf[0]='\0';
			bufsize = sizeof(buf);	// bufsize got reset from the last RegQueryValueEx

			if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath, NULL, KEY_ALL_ACCESS, &hKey)) {

				if (ERROR_SUCCESS == RegQueryValueEx(hKey, "DirRoot", NULL, NULL, (LPBYTE)buf, (LPDWORD)&bufsize)) {

					// make sure we append the last '\' in the profile dir path
					strcat(buf, "\\");	
					RegCloseKey(hKey);

				}
			}
		}
	}

	free(keyPath);
#else // WIN16
	char keyPath[_MAX_PATH];
   
	// get current user profile
	GetPrivateProfileString(INI_USERADDINFO_SECTION, INI_CURRUSER_KEY, 
                    			"\0", keyPath, _MAX_PATH, INI_NETSCAPE_FILE);
	if ('\0' != keyPath[0])		// successfully got current user profile
	{
		// get profile directory path
		GetPrivateProfileString(INI_USER_SECTION, keyPath, 
							"\0", buf, (int) bufsize, INI_NETSCAPE_FILE);
		if ('\0' != buf[0])		// successfully got curret directory path
		{
			// make sure we append the last '\' in the profile dir path
			strcat(buf, "\\");
	   }
	}
#endif // !WIN32

	if (buf && buf[0])
		strcpy(profilePath, buf);
}


