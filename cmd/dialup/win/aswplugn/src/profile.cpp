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

// Netscape Registry header file
#include "NSReg.h"
 
#include "xp_mem.h"

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

#define MAX_PATH_LENGTH			256

// java includes
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);


void GetProfileDirectory(char *profilePath);
REGERR CopyNetscapeRegKey(HREG hReg, RKEY key, const char *path, RKEY newKey, char *newPath);
REGERR DeleteKeyTree(HREG hReg, RKEY key, const char *path);


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
	char buf[MAX_PATH_LENGTH];
	buf[0] = '\0';

#ifdef WIN32 // ***************************** WIN32 ********************************
	HREG   reg;
	RKEY   rKey1, rKey2;

	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return NULL;

	strcpy(keyPath, "Netscape/ProfileManager");

	// Open Netscape Registry
	if (REGERR_OK == NR_RegOpen(NULL, &reg))
	{
		// Get the handle to the common information
		if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_COMMON, keyPath, &rKey1))
		{
			//Get the current user profile
			if (REGERR_OK == NR_RegGetEntryString(reg, rKey1, "LastNetscapeUser", buf, MAX_PATH_LENGTH))
			{
				//Get the handle to the current profile
				if( REGERR_OK == NR_RegGetKey(reg, ROOTKEY_USERS, buf, &rKey2))
				{
					// Get the current profile location
					if (REGERR_OK == NR_RegGetEntryString(reg, rKey2, "ProfileLocation", (char *)buf, MAX_PATH_LENGTH))
					{
						// make sure we append the last '\' in the profile dir path
						strcat(buf, "\\");	

						profilePath = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);

						if (REGERR_OK == NR_RegSetEntryString(reg, rKey2, "Temporary", "Temporary"))
						{
							trace("profile.cpp : GetCurrentProfileDirectiory : Temp flag set for Current User profile directory");
						}	
						else
						{
							trace("profile.cpp : GetCurrentProfileDirectiory : Error in setting Temp flag to Current User profile location");
						}

						NR_RegClose(reg);
					} 
					else
					{
						trace("profile.cpp : GetCurrentProfileDirectiory : Error in obtaining Current User profile location");
					}
				}
				else
				{
					trace("profile.cpp : GetCurrentProfileDirectory : Could not obtain handle to ROOTKEY_USERS");
				}
			}
			else
			{
				trace("profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile");
			}
		}
		else
		{
			trace("profile.cpp : GetCurrentProfileDirectory : Could not obtain handle to ROOTKEY_COMMON");
		}
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileDirectory : Could not open Netscape Registry");
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
	char buf[MAX_PATH_LENGTH];
	buf[0] = '\0';



#ifdef WIN32  // ***************************** WIN32 ********************************

	HREG   reg;
	RKEY   rKey;

	// Open Netscape Registry
	if (REGERR_OK == NR_RegOpen(NULL, &reg))
	{
		// finds the user profile path in registry
		if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_COMMON, "Netscape/ProfileManager", &rKey))
		{
			//Get the current user profile
			if (REGERR_OK == NR_RegGetEntryString(reg, rKey, "LastNetscapeUser", buf, MAX_PATH_LENGTH))
			{
				profileName = JRI_NewStringPlatform(env, buf, strlen(buf), NULL, 0);

				NR_RegClose(reg);
			}
			else
			{
				trace("profile.cpp : GetCurrentProfileName : Error in obtaining Current User profile location");
			}
		}
		else
		{
			trace("profile.cpp : GetCurrentProfileName : Could not obtain handle to ROOTKEY_COMMON");
		}
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileName : Could not open Netscape Registry");
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

	char *newProfName;
	char *oldProfName;
	char buf[MAX_PATH_LENGTH];
	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return;

	strcpy(keyPath, "Netscape/ProfileManager");


	HREG   reg;
	RKEY   rKey1, rKey2, rKey3;

	BOOL Err = FALSE;

	if (JSprofileName != NULL)
		newProfileName = GetStringPlatformChars(env, JSprofileName);

	oldProfName = (char *)malloc(sizeof(char) * 260);
	newProfName = (char *)malloc(sizeof(char) * 260);

	if ((!oldProfName) && (!newProfName))
		return;

	// Open Netscape Registry
	if (REGERR_OK == NR_RegOpen(NULL, &reg))
	{
		// Get the handle to the common information
		if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_COMMON, keyPath, &rKey1))
		{
			//Get the current user profile
			if (REGERR_OK == NR_RegGetEntryString(reg, rKey1, "LastNetscapeUser", buf, MAX_PATH_LENGTH))
			{
				// old user profile key path
				strcpy (oldProfName, buf);

				// new user profile key path
				strcpy (newProfName, newProfileName);

				// check if the new ProfileName is same as old ProfileName, if it is, we don't
				// need to do anything.
				if (strcmp(oldProfName, newProfName) == 0) {
					NR_RegClose(reg);
					free(newProfName);
					free(oldProfName);
					return;
				}

				// Get the handle to the user profile to be modified
				if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_USERS, oldProfName, &rKey2))
				{
					if (REGERR_OK == CopyNetscapeRegKey (reg, ROOTKEY_USERS, (char *)oldProfName, ROOTKEY_USERS, (char *) newProfName))	
					{
						if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_USERS, newProfName, &rKey3))
						{
							if (REGERR_OK == NR_RegDeleteEntry(reg, rKey3, "Temporary"))
							{
								trace("profile.cpp : SetCurrentProfile : Profile marked permanent");
							}
							else
							{
								trace("profile.cpp : SetCurrentProfile : Error in deleting Temp entry fron new profile");
							}
						}	
						else
						{
							trace("profile.cpp : SetCurrentProfile : Error in obtaining new profile handle");
						}

						DeleteKeyTree(reg, ROOTKEY_USERS, (char *)oldProfName);
					}
					else
					{
						trace("profile.cpp : SetCurrentProfile : Error in copying keys");
					}
				}
				else
				{
					trace("profile.cpp : SetCurrentProfile : Error in obtaining a handle to ROOTKEY_USERS");
				}
			}
			else
			{
				trace("profile.cpp : SetCurrentProfile : Error in obtaining current profile value");
			}
		}
		else
		{
			trace("profile.cpp : SetCurrentProfile : Error in obtaining a handle to ROOTKEY_COMMON");
		}
	}
	else
	{
		trace("profile.cpp : SetCurrentProfile : Error opening Netscape registry");
	}


	if (REGERR_OK == NR_RegSetUsername(newProfName))
	{
		trace("profile.cpp : SetCurrentProfile : New profile name is the current user name");
	}
	else
	{
		trace("profile.cpp : SetCurrentProfile : Error in setting the new profile name to be the current profile name");
	}

	NR_RegClose(reg);
	free(oldProfName);
	free(newProfName);

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
	char buf[MAX_PATH_LENGTH];
	buf[0] = '\0';

#ifdef WIN32

	HREG   reg;
	RKEY   rKey1, rKey2;

	char *keyPath = (char *)malloc(sizeof(char) * 512);

	assert(keyPath);
	if (!keyPath)
		return;

	strcpy(keyPath, "Netscape/ProfileManager");

	// Open Netscape Registry
	if (REGERR_OK == NR_RegOpen(NULL, &reg))
	{
		// Get the handle to the common information
		if (REGERR_OK == NR_RegGetKey(reg, ROOTKEY_COMMON, keyPath, &rKey1))
		{
			//Get the current user profile
			if (REGERR_OK == NR_RegGetEntryString(reg, rKey1, "LastNetscapeUser", buf, MAX_PATH_LENGTH))
			{
				//Get the handle to the current profile
				if( REGERR_OK == NR_RegGetKey(reg, ROOTKEY_USERS, buf, &rKey2))
				{
					// Get the current profile location
					if (REGERR_OK == NR_RegGetEntryString(reg, rKey2, "ProfileLocation", (char *)buf, MAX_PATH_LENGTH))
					{
						// make sure we append the last '\' in the profile dir path
						strcat(buf, "\\");	

						NR_RegClose(reg);
					} 
					else
					{
						trace("profile.cpp : GetCurrentProfileDirectiory : Error in obtaining Current User profile location");
					}
				}
				else
				{
					trace("profile.cpp : GetCurrentProfileDirectory : Could not obtain handle to ROOTKEY_USERS");
				}
			}
			else
			{
				trace("profile.cpp : GetCurrentProfileDirectory : Error in obtaining Current User profile");
			}
		}
		else
		{
			trace("profile.cpp : GetCurrentProfileDirectory : Could not obtain handle to ROOTKEY_COMMON");
		}
	}
	else
	{
		trace("profile.cpp : GetCurrentProfileDirectory : Could not open Netscape Registry");
	}
	free(keyPath);

#else  // ***************************** WIN16 ********************************
    
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


    REGERR
CopyNetscapeRegKey(
         HREG       hReg,        /* handle of open registry */
         RKEY       key,         /* root key */
         const char *path,       /* relative path of subkey to find */
         RKEY       newKey,      /* new root key */
         char       *newPath     /* new path */
        )
{
    char            keyName[MAX_PATH_LENGTH];
    REGERR          err;
    REGENUM         keyState = NULL, entryState = NULL;
    REGINFO         regInfo;
    RKEY            sourceKey, destKey;
    
    err = NR_RegGetKey(hReg, key, (char *) path, &sourceKey);

    if (err == REGERR_OK) {
        err = NR_RegAddKey(hReg, newKey, newPath, &destKey);
    }

    if (err == REGERR_OK) {
        char            entryName[MAX_PATH_LENGTH];
        uint32          dataSize;
        unsigned char   smallBuffer[256];
        void            *pData;

        regInfo.size = sizeof(REGINFO);
        entryState = NULL;

        while (err == REGERR_OK) {
            err = NR_RegEnumEntries(hReg, sourceKey, &entryState, entryName, MAX_PATH_LENGTH, &regInfo);

            if (err == REGERR_OK) {
                if (regInfo.entryLength < 256) {
                    pData = (void *) smallBuffer;
                    dataSize = 256;
                } else {
                    pData = (void *) XP_ALLOC(regInfo.entryLength);
                    dataSize = regInfo.entryLength;
                }


                err = NR_RegGetEntry(hReg, sourceKey, entryName, pData, &dataSize);

                if (err == REGERR_OK) {
                    err = NR_RegSetEntry(hReg, destKey, entryName, regInfo.entryType, pData, dataSize);
                }

                if (pData != smallBuffer) {
                    XP_FREEIF(pData);
                }
            }
        }

        if (err == REGERR_NOMORE) {
            err = REGERR_OK;
        }

        while (err == REGERR_OK) {
            err = NR_RegEnumSubkeys(hReg, sourceKey, &keyState, keyName, MAX_PATH_LENGTH, 0);

            if (err == REGERR_OK) {
                err = CopyNetscapeRegKey(hReg, sourceKey, keyName, destKey, keyName);
                
            }
        }

        if (err == REGERR_NOMORE) {
            err = REGERR_OK;
        }
    
    } /* if (destKey) */


    return err;
}

REGERR
DeleteKeyTree(
         HREG       hReg,        /* handle of open registry */
         RKEY       key,         /* root key */
         const char *path        /* relative path of subkey to find */
        )
{
    char            keyName[MAX_PATH_LENGTH];
    REGERR          err;
    REGENUM         keyState = NULL;
    RKEY            keyToDelete;

    err = NR_RegGetKey(hReg, key, (char *) path, &keyToDelete);

    while (err == REGERR_OK) {
        err = NR_RegEnumSubkeys(hReg, keyToDelete, &keyState, keyName, MAX_PATH_LENGTH, REGENUM_DEPTH_FIRST);

        if (err == REGERR_OK) {
            NR_RegDeleteKey(hReg, keyToDelete, keyName);
        }
    }

    err = NR_RegDeleteKey(hReg, key, (char *) path);

    return err;
}
