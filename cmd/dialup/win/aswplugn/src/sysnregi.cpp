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
///////////////////////////////////////////////////////////////////////////////
//
// Sysnregi.cpp
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 02/27/97    xxxxxxxxxxxxxx    Port Native API for win3.x
//             xxxxxxxxxxxxxx    Define Native API for win95 & winNT
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"

#include <string.h>
#include <stdio.h>

// windows include
#ifdef WIN32
// ********************* Win32 includes **************************
#include <winbase.h>
#else
// ********************* Win16 includes **************************
#include <windows.h>
#include <winsock.h>           // for ntohs()
#include <sys\stat.h>  			// for _S_IWRITE etc..
#include <helper16.h>
#include "asw16res.h"
#include "errmsg.h"
#endif // WIN32

// java include 
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"

#ifdef WIN32 //********************************* WIN32 **********************************
#define NAV_REL_PNAME	"\\program\\netscape.exe"	// navigator relative path name
#else       //********************************* WIN32 **********************************
#define NAV_REL_PNAME	"\\netscape.exe"	// navigator relative path name
#endif      //!WIN32

char IconName[256] = {'\0' };
//char g_sScriptFilename[256] = {'\0' };

extern const char *GetStringPlatformChars(JRIEnv *env, struct java_lang_String *string);

extern void GetProfileDirectory(char *profilePath);

//********************************************************************************
// native method:
//
// GetRegInfo
//
// returns the data sent back from regi server
//********************************************************************************
extern JRI_PUBLIC_API(jref)
native_netscape_npasw_SetupPlugin_SECURE_0005fGetRegInfo(JRIEnv* env,
										  struct netscape_npasw_SetupPlugin* self,
										  jbool JSflushDataFlag)
{
   void* data = NULL;
   data = RegDataArray;

   if (JSflushDataFlag == TRUE) {

      RegDataArray = NULL;
   }

   return (jref)data;
}


//********************************************************************************
// native method:
//
// NeedReboot
//
// determine if reboot is needed (win31 only)
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fNeedReboot(JRIEnv* env,
										  struct netscape_npasw_SetupPlugin* self)
{

	// we normally return FALSE, i.e. don't reboot, unless it's
	// needed for seting up modem or whatever condition we
	// should check.

	return (FALSE);
}

//********************************************************************************
// native method:
//
// Reboot
//
// reboots the system
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fReboot(JRIEnv* env,
									  struct netscape_npasw_SetupPlugin* self,
									  struct java_lang_String *JSaccountSetupPathname)
{
	char buf[512];
	char *startupPath;
	DWORD cbData;
   
   const char *ASWpath=NULL;
   if (JSaccountSetupPathname != NULL)
	   ASWpath = GetStringPlatformChars(env, JSaccountSetupPathname); 

	// if path is NOT null, lauch ASW after reboot
	if (ASWpath)
	{
		BOOL bResult = FALSE;
		buf[0] = '\0';
		cbData = sizeof(buf);

		//gets netscape install directory (e.g. "C:\\Program Files\\Netscape\\Navigator")
#ifdef WIN32 // ********************** WIN32 **********************
		HKEY    hKey;
		RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Netscape\\Netscape Communicator\\Main",
					 NULL, KEY_QUERY_VALUE, &hKey);
		bResult = (RegQueryValueEx(hKey, "Install Directory", NULL, NULL, (LPBYTE)buf, &cbData) == ERROR_SUCCESS);
#else        // ********************** WIN16 **********************
		bResult = GetNetscapeInstallPath(buf); 
		assert(bResult);
#endif       // !WIN32
     
		// construct startup path
		if (bResult) 
		{
			// allocate 2 extra space here, 1 for null char at the end, 1 for 
			// space btween navigator and asw file path
			startupPath = (char *)malloc(sizeof(char) * (strlen(buf) + strlen(NAV_REL_PNAME) + strlen(ASWpath) + 2));
			if (NULL == startupPath)					// abort if memory allocation fails
				return;

			strcpy (startupPath, buf);             // copy netscape install dir
			strcat (startupPath, NAV_REL_PNAME);   // append navigator relative path name
			strcat (startupPath, " ");
			strcat (startupPath, ASWpath);         // append ASW path
		}
        
		// add startup item to the system (WIN32: add to system registry, WIN16: add to StartUp program group)
#ifdef WIN32
		RegCloseKey(hKey);

		// Win32: puts ASW path in RunServiceOne registry key
		bResult = (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
								"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunServicesOnce",
								NULL, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS);
		if (!bResult)
			return;
        
		// the actual registry key value assignment
		bResult = (RegSetValueEx(hKey, "(Default)", NULL, REG_SZ, (LPBYTE)startupPath,
								 strlen(startupPath) + 1)  == ERROR_SUCCESS);
		
		RegCloseKey(hKey);
#else
		bResult = AddProgramGroupItem("StartUp", startupPath, "Netscape Account Setup");
		assert(bResult);

		if (!bResult)
		{
        	return;
        }
#endif

		free(startupPath);         // free memory
		if (!bResult)              // abort!  error creating or opening the registry key
		{
			return;
		}
	}

	// reboot system

#ifdef WIN32 //**************************** WIN32 *************************
	if (!ExitWindows(EWX_REBOOT, 0))
	{
		DWORD err = GetLastError();
	}
#else        //**************************** WIN16 *************************
	BOOL bReboot = ExitWindows(EW_REBOOTSYSTEM, 0);
	if (!bReboot)
	{
		char errStr[255];
		getMsgString(errStr, IDS_ERR_REBOOT_FAILURE);
        DisplayErrMsg(errStr, MB_OK | MB_ICONEXCLAMATION);
	}

	assert(bReboot);
#endif // WIN32 
}

//********************************************************************************
// native method:
//
// QuitNavigator
//
// quits the navigator
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fQuitNavigator(JRIEnv* env,
											 struct netscape_npasw_SetupPlugin* self)
{
	PostMessage(HWND_BROADCAST, WM_COMMAND, ID_APP_SUPER_EXIT, 0L);
}


//********************************************************************************
//
// countRegItems
//
// counts the number of pairs regi sends
//********************************************************************************
long countRegItems(void* regDataBuf,
				   long regDataLen,
				   BOOL extendLen)
{
	unsigned short len;
	unsigned long lenLong;
	long regItemCount = 0;
	char *buf = (char *)malloc(sizeof(char) * regDataLen);
	
	char *buffer = (char *)regDataBuf;
	long position = regDataLen;
	
	if (buf) {
		while (position > 0) {
			if (extendLen == TRUE) {
				memcpy(buf, buffer, sizeof(lenLong));
				lenLong = ntohl(*((long *)buf));
				buffer+=sizeof(lenLong);
				position-=sizeof(lenLong);
				if ((unsigned long) position<lenLong)
					break;
				buffer+=lenLong;
				position-=lenLong;
			} else {
				memcpy(buf, buffer, sizeof(len));
				len = ntohs(*((short *)buf));
				buffer+=sizeof(len);
				position-=sizeof(len);
				if ((unsigned short) position<len)
					break;
				buffer+=len;
				position-=len;
			}
			if (position<0)
				break;
			regItemCount ++;
		}
	}
	
	regItemCount /= 2;
	free(buf);

	return (regItemCount);
}



//********************************************************************************
//
// getRegElement
//
// gets item values from the regi data buffer
//********************************************************************************
java_lang_String * getRegElement(JRIEnv *env,
								 void *RegDataBuf,
								 long itemNum,
								 BOOL extendLen)
{
	unsigned short lenShort;
	unsigned long len;
	java_lang_String *data = NULL;
	char *buffer = (char *)RegDataBuf;
	long position = RegDataLength;
	char *buf = (char *)malloc(sizeof(char) * RegDataLength);
	char value[100];
	char *rtnValue = NULL;
	BOOL noData = FALSE;
	
	if (buf) {
		while (itemNum>0 && position>0) {
			if (extendLen == TRUE) {
				memcpy(buf, buffer, sizeof(len));
				len = ntohl(*((long *)buf));
				buffer+=sizeof(len);
				position-=sizeof(len);
				if ((unsigned long) position<len) {
					free(buf);
					break;
				}
				buffer+=len;
				position-=len;
			} else {
				memcpy(buf, buffer, sizeof(lenShort));
				lenShort = ntohs(*((short *)buf));
				buffer+=sizeof(lenShort);
				position-=sizeof(lenShort);
				if ((unsigned short) position<lenShort) {
					free(buf);
					break;
				}
				buffer+=lenShort;
				position-=lenShort;
			}
			if (position<0) {
				free(buf);
				break;
			}
			
			if (extendLen == TRUE) {
				memcpy(buf, buffer, sizeof(len));
				len = ntohl(*((long *)buf));
				buffer+=sizeof(len);
				position-=sizeof(len);
				if ((unsigned long) position<len) {
					free(buf);
					break;
				}
				buffer+=len;
				position-=len;
			} else {
				memcpy(buf, buffer, sizeof(lenShort));
				lenShort = ntohs(*((short *)buf));
				buffer+=sizeof(lenShort);
				position-=sizeof(lenShort);
				if ((unsigned short) position<lenShort) {
					free(buf);
					break;
				}
				buffer+=lenShort;
				position-=lenShort;
			}
			if (position<0) {
				free(buf);
				break;
			}
			
			--itemNum;
		}
		
		if (!itemNum && position>0) {
			if (extendLen == TRUE) {
				memcpy(buf, buffer, sizeof(len));
				len = ntohl(*((long *)buf));
				buffer+=sizeof(len);
				position-=sizeof(len);
				if ((unsigned long) position<len) {
					free(buf);
					return (data);
				}
			} else {
				memcpy(buf, buffer, sizeof(lenShort));
				lenShort = ntohs(*((short *)buf));
				buffer+=sizeof(lenShort);
				position-=sizeof(lenShort);
				if ((unsigned short) position<lenShort) {
					free(buf);
					return (data);
				}
			}
			
			
			if (extendLen == TRUE) {
				sprintf(value, "%.*s=", (int)len, buffer);
				buffer+=len;
				position-=len;
			} else {
				sprintf(value, "%.*s=", (int)lenShort, buffer);
				buffer+=lenShort;
				position-=lenShort;
			}
			if(position<0) {
				free(buf);
				return (data);
			}
			
			if (extendLen == TRUE) {
				memcpy(buf, buffer, sizeof(len));
				len = ntohl(*((long *)buf));
				buffer+=sizeof(len);
				position-=sizeof(len);
				if ((unsigned long) position<len) {
					free(buf);
					return (data);
				}
			} else {
				memcpy(buf, buffer, sizeof(lenShort));
				lenShort = ntohs(*((short *)buf));
				buffer+=sizeof(lenShort);
				position-=sizeof(lenShort);
				if ((unsigned short) position<lenShort) {
					free(buf);
					return (data);
				}
			}
			
			if (strcmp(value, "DOMAIN_NAME=") == 0) {
				// gets the name of ISP for icon file name
				int  pos;
				pos = strcspn(buffer, ".");
				strncpy(IconName, buffer, pos);
			}

			BOOL binDataExists = FALSE;
			if (extendLen == TRUE) {
				if (len != 0)
					binDataExists = TRUE;
			} else {
				if (lenShort != 0)
					binDataExists = TRUE;
			}

			if (((strcmp(value, "ICON=") == 0) ||
				 (strcmp(value, "WIN_ANIMATION_20=") == 0) || 
				 (strcmp(value, "WIN_ANIMATION_40=") == 0) ||
				 (strcmp(value, "LCK_FILE=") == 0))
				 && binDataExists) {

				noData=TRUE;

				// save the icon data to a file
#ifdef WIN32
				SECURITY_ATTRIBUTES secAttrib;
				memset(&secAttrib, 0, sizeof(SECURITY_ATTRIBUTES));
				secAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
				secAttrib.lpSecurityDescriptor = NULL;
				secAttrib.bInheritHandle = FALSE;
				char file[MAX_PATH];
				char profileDir[MAX_PATH];
#else
				char file[_MAX_PATH];
				char profileDir[_MAX_PATH];
#endif
				GetProfileDirectory((char *)&profileDir);
				strcpy(file, profileDir);

				if (strcmp(value, "ICON=") == 0) {

					// construct the icon file name
#ifdef WIN32
					strcat(file, IconName);
#else
					strncat(file, IconName, 8);			// win16 limitation: 8.3 file format
#endif
					strcat(file, ".ico");

				} else if (strcmp(value, "WIN_ANIMATION_20=") == 0) {

					strcat(file, "small.bmp");

				} else if (strcmp(value, "WIN_ANIMATION_40=") == 0) {

					strcat(file, "large.bmp");

				} else if (strcmp(value, "LCK_FILE=") == 0) {

					strcat(file, "profile.cfg");
		
				}


#ifdef WIN32
				HANDLE hfile = CreateFile (file, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
										   &secAttrib, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hfile != INVALID_HANDLE_VALUE) {
#else
				HFILE hfile = _lcreat(file, 0);								
				if (HFILE_ERROR != hfile) {
#endif
					DWORD bytesWritten = 0;
					if (extendLen == TRUE) {
						for (unsigned long i=0; i<len; i++)
#ifdef WIN32
							WriteFile(hfile, &buffer[i], 1, &bytesWritten, NULL);
#else
							bytesWritten = _lwrite(hfile, &buffer[i], 1);
#endif
					} else {
						for (short i=0; i< (short) lenShort; i++) 
#ifdef WIN32						
							WriteFile(hfile, &buffer[i], 1, &bytesWritten, NULL);
#else
							bytesWritten = _lwrite(hfile, &buffer[i], 1);
#endif
					} 
					
					// close file
#ifdef WIN32					
					CloseHandle(hfile);
#else
					_lclose(hfile);
#endif

					// save icon filename
					strcpy(IconFile, file);
				} else {
					strcpy(IconFile, "\0");
				}
			}


			// long length = 1 + strlen(value);  // 1 extra for the null char -- WRONG!
			long length = strlen(value);

			if (extendLen == TRUE) {
				rtnValue = (char *)malloc(sizeof(char) * length);

				if (!rtnValue)
					return NULL;
				
				strcpy(rtnValue, value);
				
				if (!noData) {
					length = length + len;
					rtnValue = (char *)realloc(rtnValue, sizeof(char) * length);
					strncat(rtnValue, buffer, len);
				}
			} else {

				rtnValue = (char *)malloc(sizeof(char) * length);

				if (!rtnValue)
					return NULL;

				strcpy(rtnValue, value);

				if (!noData) {
					length = length + lenShort;
					rtnValue = (char *)realloc(rtnValue, sizeof(char) * length);
					strncat(rtnValue, buffer, lenShort);
				}
			}
			
			data = JRI_NewStringPlatform(env, rtnValue, length, NULL, 0);
			if (rtnValue) {
				free(rtnValue);
				rtnValue = NULL;
			}

		}
		free(buf);
	}
	
	return (data);
}
