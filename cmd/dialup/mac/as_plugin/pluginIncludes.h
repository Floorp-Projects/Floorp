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
#include <MacHeaders.h>

#define DEBUG

#pragma once

#ifndef _NPAPI_H_
#include "npapi.h"
#endif

// #include "jri.h"
//#define IMPLEMENT_java_lang_String
//#include "netscape_javascript_JSObject.h"

#include "jri_md.h"
#ifndef	JRI_PUBLIC_VAR
#define	JRI_PUBLIC_VAR	JRI_PUBLIC_API
#endif

#ifdef __cplusplus
#define recplusplus
#undef __cplusplus
#endif

#include "java_lang_String.h"
#include "netscape_plugin_Plugin.h"
#define IMPLEMENT_SetupPlugin
#include "SetupPlugin.h"

#ifdef recplusplus
#define __cplusplus
#undef recplusplus
#endif

#include <AddressXlation.h>
//#include <AEObjects.h>
//#include <AERegistry.h>
//#include <Aliases.h>
//#include <AppleEvents.h>
#include <CodeFragments.h>
//#include <Devices.h>
//#include <Dialogs.h>
//#include <Errors.h>
//#include <Files.h>
#include <Finder.h>
//#include <Folders.h>
//#include <Fonts.h>
//#include <Gestalt.h>
//#include <LowMem.h>
#include <MacTCP.h>
//#include <Memory.h>
//#include <MixedMode.h>
#include <OpenTransport.h>
//#include <OSUtils.h>
//#include <Processes.h>
//#include <Resources.h>
#include <Retrace.h>
//#include <StandardFile.h>
//#include <Strings.h>
//#include <TextEdit.h>
//#include <TextUtils.h>
//#include <ToolUtils.h>
#include <Timer.h>
//#include <Traps.h>

#include <stdio.h>
#include <string.h>

#ifndef	PtoCstr
#define	PtoCstr						P2CStr
#define	CtoPstr						C2PStr
#endif

#define kMacTCPDriverName 			"\p.ipp"
#define	gestaltMacTCP				'mtcp'

#include "ppp.h"
#include "ppp_prefs_types.h"
#include "FreePPPPubInterface.h"

#define	PAUSE_TIMEOUT				(30L)
#define	HANGUP_TIMEOUT				(30L*60L)

#include "RC4Encrypt.h"


// typedef long (*PPPRequestProcPtr)(long command, void* data, long refCon);	

#ifndef	PPPRequestUPP
enum {
	uppPPPRequest = kThinkCStackBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(long)))
		 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(long)))
		 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(void *)))
		 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
};

#if GENERATINGCFM
typedef UniversalProcPtr PPPRequestUPP;
#define NewPPPRequestUPP( userRoutine )		\
		(PPPRequestUPP)NewRoutineDescriptor( (ProcPtr)(userRoutine), uppPPPRequest, kM68kISA )		// not GetCurrentArchitecture()
#else
typedef PPPRequestProcPtr PPPRequestUPP;
#define NewPPPRequestUPP( userRoutine )		\
		( (PPPRequestUPP)( userRoutine ) )
#endif

#endif	/* FreePPPPubInterfaceUPP*/



// typedef pascal long (*FreePPPPubInterfaceProcPtr)(long selector, void *parmData, long refCon);	

#ifndef	FreePPPPubInterfaceUPP
enum {
	uppFreePPPPubInterface = kPascalStackBased
		 | RESULT_SIZE(SIZE_CODE(sizeof(long)))
		 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(long)))
		 | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(void *)))
		 | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(long)))
};

#if GENERATINGCFM
typedef UniversalProcPtr FreePPPPubInterfaceUPP;
#define NewFreePPPPubInterfaceUPP( userRoutine )		\
		(FreePPPPubInterfaceUPP)NewRoutineDescriptor( (ProcPtr)(userRoutine), uppFreePPPPubInterface, kM68kISA )		// not GetCurrentArchitecture()
#else
typedef FreePPPPubInterfaceProcPtr FreePPPPubInterfaceUPP;
#define NewFreePPPPubInterfaceUPP( userRoutine )		\
		( (FreePPPPubInterfaceUPP)( userRoutine ) )
#endif

#endif	/* FreePPPPubInterfaceUPP*/


#define	ACUR_RESID				256
#define	SPIN_CYCLE				8L		// spin every 8/60ths of a second
#define	SPIN_TIMEOUT			90L		// stop spinning after 1.5 seconds
#define	MAX_NUM_SPINS			(8L*120L)	// after two minutes, stop spinning


// options for kLprefixTwiddleSetting

#define	FREEPPP_LOCATION_DONOTHING			0
#define	FREEPPP_LOCATION_ADDPREFIX			1
#define	FREEPPP_LOCATION_STRIP_AREACODE_ADDPREFIX	2
#define	FREEPPP_LOCATION_DIAL_AREACODE_NO_PREFIX	3



#define	REG_STREAM_TYPE					"application/x-netscape-autoconfigure-dialer"
#define	REG_STREAM_TYPE_V2				"application/x-netscape-autoconfigure-dialer-v2"

#define	CFG_FILENAME					"\pPROFILE.CFG"
#define	ANIMATION_FILENAME				"\pCustom Animation"

#define USERPROFILEDATABASE_NAME_B2			"\p:Netscape Ä:User Profiles"
#define USERPROFILEDATABASE_NAME_POSTB2		"\p:Netscape Users:User Profiles"
#define	USERPROFILE_RESTYPE				'user'
#define	USERPROFILE_RESID				128

#define	FINDER_SIGNATURE				'MACS'
#define	FREEPPP_SETUP_SIGNATURE			'FPPP'
#define	MODEM_WIZARD_SIGNATURE			'MW”z'
#define	TCPIP_CONTROL_PANEL_SIGNATURE	'ztcp'
#define	UNKNOWN_SIGNATURE				'????'
#define	NETSCAPE_SIGNATURE				'MOSS'
#define	KIOSK_MODE						'KOSK'

#define	TEXTFILE_TYPE					'TEXT'
#define	MILAN_TYPE						'MLN1'
#define	MILAN_TYPE_EXT					'MLN2'

#define	SIMPLETEXT_SIGNATURE			'ttxt'
#define	SIMPLETEXT_TYPE					'ttro'				// 'TEXT' for writeable file, 'ttro' for read-only file

#define	NAV_STARTUP_ALIAS_NAME				"\pzzzz Account Setup 1"	// magic alias name to use for Nav reference in Startup Items Folder
#define	ACCOUNT_SETUP_STARTUP_ALIAS_NAME	"\pzzzz Account Setup 2"	// magic alias name to use for Account Setup reference in Startup Items Folder

#define	HOME_LOCATION					"\pHome"

#define	SCRIPT_FILE_SEPARATOR			"|"
#define	SCRIPT_MAGIC_NAME				"\p\%name"
#define	SCRIPT_FREEPPP_NAME				"\p\\A"
#define	SCRIPT_MAGIC_PASSWORD			"\p\%password"
#define	SCRIPT_FREEPPP_PASSWORD			"\p\\P"

#define CUSTOMGETFILE_RESID				6042	//this is both the DLOG # and the STR# resource that contains custom strings
#define WHEREIS_STRINGID				1
#define QUESTIONMARK_STRINGID			2
#define CHOOSEEDITOR_STRINGID			3
#define CHOOSEMILAN_STRINGID			4

typedef struct _PluginInstance
{
	NPWindow*		fWindow;
	uint16			fMode;
	Handle			regData;
	Handle			data;
}
PluginInstance;


typedef struct _regStream
{
	Handle			data;
	int32			dataLen;
	Boolean			extendedDataFlag;
}
regStream;


#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

typedef	struct	_styleData
{
	short		numRuns;
	long		runOffset;
	short		lineHeight;
	short		fontAscent;
	short		fontFamily;
	short		fontStyle;
	short		fontSize;
	RGBColor	theColor;
}
_styleData, **_styleDataH;

typedef	struct
{
	short		numCursors;
	short		index;
	CursHandle	cursors[1];
}
**acurHandle;

typedef	struct
{
	VBLTask		theTask;
	long		theA5;
	acurHandle	cursorsH;
	long		numSpins;
}
VBLTaskWithA5, *VBLTaskWithA5Ptr;

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

typedef	struct	_fileArray
{
	struct	_fileArray	*next;
	Str63			name;
}
_fileArray, *_fileArrayPtr;


typedef struct	_fileCache
{
	struct	_fileCache		*next;
	Boolean				dirtyFlag;
	FSSpec				theSpec;
	unsigned long			ioFlMdDat;
	Handle				dataH;
}
_fileCache, *_fileCachePtr;

#define	MAX_CACHE_ENTRY_SIZE_BYTES				2048			// don't cache files larger than this size


#define	MIN_OPEN_TRANSPORT_VERSION_REQUIRED		0x01100000		// require Open Transport version 1.1 or later
#define	OPEN_TRANSPORT_VERSION_TOO_NEW			0x01500000		// Open Transport version 1.5 and later require revs

#define	MIN_FREEPPP_VERSION_REQUIRED			0x0260			// require FreePPP version 2.6 or later


#include "prototypes.h"


// Following are positive error codes used by the plugin

#define	PLUGIN_ERROR_DIALOG_RESID				256
#define	PLUGIN_ERROR_STR_RESID					256
#define	PLUGIN_SECURITY_DIALOG_RESID			257

#define	POPUP_MENU_ITEMNAME_STR_RESID			257
#define	POPUP_MENU_BACK_ID						1
#define	POPUP_MENU_FORWARD_ID					2

// #define	SECURITY_DIALOG_ENABLED					// comment this out to not use the security dialog

#define	FREEPPP_NOT_INSTALLED_ERROR_STR_ID				1
#define	FREEPPPCONFIGPLUGIN_NOT_INSTALLED_ERROR_STR_ID	2
#define	FREEPPP_TOO_OLD_ERROR_STR_ID					3
#define	OT_TOO_OLD_ERROR_STR_ID							4
#define	OT_TOO_NEW_ERROR_STR_ID							5
#define	OT_NEEDS_REINSTALL_ERROR_STR_ID					6
#define	UNABLE_TO_FIND_MODEM_WIZARD_STR_ID				7
#define	MODEM_WIZARD_LAUNCH_PROBLEM_STR_ID				8
#define BETAEXPIRED_STRINGID							9


//
// Define SETUP_PLUGIN_TRACE_SETTING to 1 to have the wrapper functions emit
// DebugStr messages whenever they are called. SETUP_PLUGIN_TRACE is used to
// display when a routine is entered or exited.
//
#define SETUP_PLUGIN_TRACE_SETTING	 				0

#if SETUP_PLUGIN_TRACE_SETTING
#define SETUP_PLUGIN_TRACE(msg)						DebugStr( (void*)msg )
#else
#define SETUP_PLUGIN_TRACE
#endif


//
// Define SETUP_PLUGIN_TRACE_ERRORS_SETTING to 1 to have the wrapper functions emit
// DebugStr messages whenever they are called. SETUP_PLUGIN_ERROR is used to display
// error information if an error occurs.
//
#define SETUP_PLUGIN_TRACE_ERRORS_SETTING			0

#if SETUP_PLUGIN_TRACE_ERRORS_SETTING
#define SETUP_PLUGIN_ERROR( msg, errNum )\
if ( errNum != noErr )	\
{\
	Str255 data;\
	DebugStr( (void*)msg );\
	NumToString( (long)errNum, data );\
	DebugStr( data );\
}
#else
#define SETUP_PLUGIN_ERROR
#endif


//
// Define SETUP_PLUGIN_INFO_SETTING to 1 to have the wrapper functions emit
// DebugStr messages whenever they are called. SETUP_PLUGIN_INFO_STR is used
// to display status messages,  values of variables, etc.
//
#define SETUP_PLUGIN_INFO_SETTING					0

#if SETUP_PLUGIN_INFO_SETTING
#define SETUP_PLUGIN_INFO_STR( msg, data )\
if ( msg != NULL )\
	DebugStr( (void *)msg );\
if ( data != NULL )\
	DebugStr( (void*)data);
#else
#define SETUP_PLUGIN_INFO_STR
#endif


// Defines for a couple of arbitrary things that are necessary for our IAS password encryption routines
// (native_SetupPlugin_SECURE_0005fEncryptPassword in dialerRtns.c)
#define kArbitraryMungedString						"\pahsf73j1nf,ho482jsozlt0w"

