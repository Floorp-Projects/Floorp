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
// muc.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "muc.h"
#include "dialshr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Keeps track of OS version, either win95 or winNT
int platformOS;
HINSTANCE DLLinstance;
int GetAcctConfig(char *returnData);
int GetModemConfig(char *returnData);

/////////////////////////////////////////////////////////////////////////////
// CMucApp

BEGIN_MESSAGE_MAP(CMucApp, CWinApp)
	//{{AFX_MSG_MAP(CMucApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMucApp construction

CMucApp::CMucApp()
{
}
/////////////////////////////////////////////////////////////////////////////
// CMucApp deconstruction
CMucApp::~CMucApp()
{
}

/////////////////////////////////////////////////////////////////////////////
// CMucApp InitInstance

CMucApp::InitInstance()
{
	CWinApp::InitInstance();

	SetDialogBkColor();        // Set dialog background color to gray
	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

#ifdef WIN32
	platformOS = 0;

	OSVERSIONINFO *lpOsVersionInfo = new OSVERSIONINFO;
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	if (GetVersionEx(lpOsVersionInfo)) 
		platformOS = (int)lpOsVersionInfo->dwPlatformId;

	switch (platformOS)
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			if(!LoadRasFunctions("rasapi32.dll"))
				return FALSE;
			SizeofRAS95();
		break;
	
		case VER_PLATFORM_WIN32_NT:             // win nt
			if(!LoadRasFunctionsNT("rasapi32.dll"))
				return FALSE;
			SizeofRASNT40();
		break;

		default:
		break;
	} 
#else
	if(!LoadRasFunctions("rasapi16.dll"))
		return FALSE;
#endif

	return TRUE;
}
/////////////////////////////////////////////////////////////////////////////
// CMucApp ExitInstance

BOOL CMucApp::ExitInstance()
{
#ifdef WIN32
	if(lpOsVersionInfo != NULL)
	{
		delete lpOsVersionInfo;
		lpOsVersionInfo = NULL;
	}
#else
	FreeRasFunctions();
#endif

	return CWinApp::ExitInstance();
}
/////////////////////////////////////////////////////////////////////////////
// The one and only CMucApp object

CMucApp theApp;

enum
{
	kGetPluginVersion,
	kSelectAcctConfig,
	kSelectModemConfig,
	kSelectDialOnDemand
};

#ifdef WIN32
STDAPI_(long) 
#else
extern "C" long FAR PASCAL _export
#endif   
PEPluginFunc( long selectorCode, void* paramBlock, void* returnData )
{
	long    returnCode = 0;
	BOOL    flag = TRUE;
	char    acctStr[MAX_PATH];

	DLLinstance = theApp.m_hInstance;

	switch ( selectorCode )
	{
		// fill in the version in paramBlock
		case kGetPluginVersion:
			*(long*)returnData = 0x00010000;
			returnCode = 0;
		break;
		
		// get account list
		case kSelectAcctConfig:
			*(int*)paramBlock = GetAcctConfig((char*)returnData);
		break;

		// get modem list
		case kSelectModemConfig:
			*(int*)paramBlock = GetModemConfig((char*)returnData);
		break;

		case kSelectDialOnDemand:
		// kludge: dealing with dogbert PR3 bug
			if (*(int*)paramBlock == 1)
				strcpy(acctStr, (char*)returnData);
			else if( *(int*)returnData == 1)
			{
				strcpy(acctStr, (char *)paramBlock);
				if(strcmp(acctStr, "None") == 0)
					flag = FALSE;
			}
			else
			{
				strcpy(acctStr, "");
				flag = FALSE;
			}
#ifdef WIN32
			switch(platformOS)
			{
				case VER_PLATFORM_WIN32_WINDOWS: //win95
					EnableDialOnDemand95(acctStr,flag);
				break;
	
				case VER_PLATFORM_WIN32_NT:             // win nt
					EnableDialOnDemandNT(acctStr,flag);
				break;
							
				default:
				break;
			}
#else
			EnableDialOnDemand16(acctStr,flag);
		break;
#endif

		default:
			returnCode = 0;
		break;
	}
	
	return returnCode;
}

/////////////////////////////////////////////////////////////////////////////
int GetAcctConfig (char *returnData)
{
	CONNECTIONPARAMS *connectionNames;
	int             numNames=0;
	int             i=0, rtn; 
	CString str, tmp;

#ifdef WIN32
	switch (platformOS)
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			rtn = GetDialUpConnection95(&connectionNames, &numNames);
		break;

		case VER_PLATFORM_WIN32_NT:             // win nt
			rtn = GetDialUpConnectionNT(&connectionNames, &numNames);
		break;

		default:
			return FALSE;
	}
#else 
	rtn = GetDialUpConnection16(&connectionNames, &numNames);
#endif
	
	if(rtn)
	{
		returnData[0] = 0x00;
		if(connectionNames != NULL)
		{    
		// pile up account names in a single array, separated by a ()
			for(i=0; i < numNames; i++) 
			{   
				tmp = connectionNames[i].szEntryName;
				str += tmp;
				str += "()";
			}
			strcpy(returnData, (const char*)str); 

			delete []connectionNames;
		}
	}
	return numNames;
}

/////////////////////////////////////////////////////////////////////////////

int GetModemConfig(char *returnData)
{
	char    **modemResults;
	int             numDevices;
	int             i;
	CString str, tmp;

	if(!::GetModemList(&modemResults, &numDevices))
	{
		if(modemResults != NULL)
		{
			for(i=0; i<numDevices; i++)
			{
				if(modemResults[i] != NULL)
					delete []modemResults[i];
			}
			delete []modemResults;
		}
		return numDevices;
	}

	// copy all entries to the array
	returnData[0] = 0x00;

	// pile up account names in a single array, separated by a ()
	for(i=0; i < numDevices; i++) 
	{   
		tmp = modemResults[i];
		str += tmp;
		str += "()";  
		delete []modemResults[i];
	}
	strcpy(returnData, (const char*)str); 

	delete []modemResults;

	return numDevices;
}

