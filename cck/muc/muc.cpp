/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
// muc.cpp : Defines the initialization routines for the DLL.
//

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#include "muc.h"
#include "dialshr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

enum
{
	kGetPluginVersion,
	kSelectAcctConfig,
	kSelectModemConfig,
	kSelectDialOnDemand,
	kConfigureDialer,
	kConnect,
	kHangup,
	kEditEntry,
	kDeleteEntry,
	kRenameEntry,
	kMonitor,
};

typedef long ( STDAPICALLTYPE* FARPEFUNC )( long selectorCode, void* paramBlock, void* returnData ); 


// Keeps track of OS version, either win95 or winNT
int				gPlatformOS;
HINSTANCE		gDLL;

int GetAcctConfig( char* returnData );
int GetModemConfig( char* returnData );

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
	
	gPlatformOS = 0;

	OSVERSIONINFO*	lpOsVersionInfo = new OSVERSIONINFO;
	lpOsVersionInfo->dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

	if ( GetVersionEx( lpOsVersionInfo ) ) 
		gPlatformOS = (int)lpOsVersionInfo->dwPlatformId;

	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			if( !LoadRasFunctions( "rasapi32.dll" ) )
				return FALSE;
		break;
	
		case VER_PLATFORM_WIN32_NT:             // win nt
			if( !LoadRasFunctionsNT( "rasapi32.dll" ) )
				return FALSE;
		break;

		default:
		break;
	} 

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMucApp ExitInstance

BOOL CMucApp::ExitInstance()
{
	if ( lpOsVersionInfo != NULL )
	{
		delete lpOsVersionInfo;
		lpOsVersionInfo = NULL;
	}

	return CWinApp::ExitInstance();
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CMucApp object

CMucApp gMUCDLL;

STDAPI_( long ) 
PEPluginFunc( long selectorCode, void* paramBlock, void* returnData )
{
	long    returnCode = 0;
	BOOL    flag = TRUE;
	char    acctStr[ MAX_PATH ];

	gDLL = gMUCDLL.m_hInstance;

	switch ( selectorCode )
	{
		// fill in the version in paramBlock
		case kGetPluginVersion:
			*(long*)returnData = 0x00010001;
		break;
		
		// get account list
		case kSelectAcctConfig:
			*(int*)paramBlock = GetAcctConfig( (char*)returnData );
		break;

		// get modem list
		case kSelectModemConfig:
			*(int*)paramBlock = GetModemConfig( (char*)returnData );
		break;

		case kSelectDialOnDemand:
			// kludge: dealing with dogbert PR3 bug
			if ( *(int*)paramBlock == 1 )
				strcpy( acctStr, (char*)returnData );
			else if( *(int*)returnData == 1 )
			{
				strcpy( acctStr, (char *)paramBlock );
				if( strcmp( acctStr, "None" ) == 0 )
					flag = FALSE;
			}
			else
			{
				strcpy( acctStr, "" );
				flag = FALSE;
			}
			switch ( gPlatformOS )
			{
				case VER_PLATFORM_WIN32_WINDOWS: //win95
					EnableDialOnDemand95( acctStr, flag );
				break;
	
				case VER_PLATFORM_WIN32_NT:             // win nt
					EnableDialOnDemandNT( acctStr, flag );
				break;
							
				default:
				break;
			}

		case kConfigureDialer:
			returnCode = DialerConfig( (char**)paramBlock );
		break;
		
		case kConnect:
			returnCode = ( DialerConnect() == TRUE ? 0 : -1 );
		break;
		
		case kHangup:
			DialerHangup();
			returnCode = 0;
		break;

		default:
			returnCode = 0;
		break;
	}
	
	return returnCode;
}

/////////////////////////////////////////////////////////////////////////////
int GetAcctConfig( char* returnData )
{
	CONNECTIONPARAMS*		connectionNames;
	int             		numNames = 0;
	int             		i = 0, rtn; 
	CString					str, tmp;

	switch ( gPlatformOS )
	{
		case VER_PLATFORM_WIN32_WINDOWS: //win95
			rtn = GetDialUpConnection95( &connectionNames, &numNames );
		break;

		case VER_PLATFORM_WIN32_NT:             // win nt
			rtn = GetDialUpConnectionNT( &connectionNames, &numNames );
		break;

		default:
			return FALSE;
	}

	if ( rtn )
	{
		returnData[ 0 ] = 0x00;
		if ( connectionNames != NULL )
		{    
			// pile up account names in a single array, separated by a ()
			for ( i = 0; i < numNames; i++ ) 
			{   
				tmp = connectionNames[ i ].szEntryName;
				str += tmp;
				str += "()";
			}
			strcpy( returnData, (const char*)str ); 

			delete []connectionNames;
		}
	}
	return numNames;
}

/////////////////////////////////////////////////////////////////////////////
int GetModemConfig( char* returnData )
{
	char**			modemResults;
	int             numDevices;
	int             i;
	CString			str, tmp;

	if ( !::GetModemList( &modemResults, &numDevices ) )
	{
		if ( modemResults != NULL )
		{
			for ( i = 0; i < numDevices; i++ )
			{
				if ( modemResults[ i ] != NULL )
					delete []modemResults[ i ];
			}
			delete []modemResults;
		}
		return numDevices;
	}

	// copy all entries to the array
	returnData[ 0 ] = 0x00;

	// pile up account names in a single array, separated by a ()
	for ( i = 0; i < numDevices; i++ ) 
	{   
		tmp = modemResults[ i ];
		str += tmp;
		str += "()";  
		delete []modemResults[ i ];
	}
	strcpy( returnData, (const char*)str ); 

	delete []modemResults;

	return numDevices;
}
