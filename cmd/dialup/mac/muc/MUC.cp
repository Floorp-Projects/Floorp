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

#include "MUC.h"
#include "LMUCHandler.h"
#include <CodeFragments.h>
#include <PPobClasses.h>
#include <UDrawingState.h>
#include <Gestalt.h>
#include <ToolUtils.h>
#include <OpenTransport.h>
#include "ppp.h"
#include "ppp.interface.h"
#include "ppp_prefs_types.h"
#include "LNetworkPrefs.h"
#include "OTTCPPriv.h"			// in the RestartTCP project

// error codes that the OT configuration stub might report back to us
// undocumented from Apple
enum kOTConfigurationErrorCodes
{
	kOTErr_noErr							= 0,
	
	kOTErr_UnsupportedSelector			= -1,
		// The specified selector code is not supported in this version
	
	kOTErr_SelectorInvalidWhileConnected	= -2,
		// Some operations are not supported while a connection is open
	
	kOTErr_UnknownConfigurationError		= -3,
		// Tried to select an unknown configuration 
		
	kOTErr_PrefsAccessError				= -4,
		// Got an error trying to access the prefs file 
		
	kOTErr_PrefsMemoryError				= -5,
		// Not enough memory to build the prefs name list 
	
	kOTErr_OTConfigurationNotPresent		= -6,
		// The requested configuration is not present in OT.  If the FreePPP Account
		// is configured to switch OT settings automatically and this error is
		// returned when selecting the Account it also means that FreePPP was not
		// able to create the OT config.
	
	kOTErr_OTConfigurationInterrupts		= -7,
		// The configuration change will interrupt OT connections
	
	kOTErr_OTConfigurationRequiresReboot	= -8,
		// The configuration change will require a reboot
	
	kOTErr_CantLoadNeededResourcesError	= -9,
		// General error for when we can't find a needed resource (or resource file)
	
	kOTErr_OTConfigInfoMissingError		= -10,
		// Missing DNS or Search Domain info in the Account config so no OT config
		// can be created
	
	kOTErr_EndSelector
};
	
LMUCHandler*		gMUCHandler = NULL;
Int16				gRefNum = refNum_Undefined;
FSSpec				gRsrcFile;

static long			MUC_DeleteDialConfiguration( PPPConfigureStruct* data );
static long			MUC_CreateModifyDialConfiguration( PPPConfigureStruct* data );
static long			MUC_OpenDialConnection( void* );
static long			MUC_CloseDialConnection( void* );
static long			MUC_GetActiveDialConfiguration( Str255* configName );
static long			MUC_SetActiveDialConfiguration( Str255* configName );
static long			MUC_GetActiveOTConfiguration( Str255* configName );
static long			MUC_SetActiveOTConfiguration( Str255* configName );
static long			MUC_GetDialConnectionState( Boolean* connected );
static long			MUC_GetAutoConnectState( Boolean* allowsAutoConnect );
static long			MUC_SetAutoConnectState( Boolean* allowsAutoConnect );
static long			MUC_GetModemsList( Str255** modemsList );
static long			MUC_GetActiveModemName( Str255* modemName );
static long			MUC_GetReceivedIPPacketCount( unsigned long* packetCount );


static OSErr		InitConfigStrList( configStringArray configStrList, PPPConfigureStruct* data,
						Boolean* configureTCP, Boolean* configureIC );						
static void			AddressToString( unsigned long address, StringPtr string );
unsigned long		AddressToLong( StringPtr addressStr );
static Boolean		OTAvailable();

extern pascal OSErr	__muc_initialize( const CFragInitBlock* block );
extern pascal void	__muc_terminate( void );
extern pascal OSErr	__initialize( const CFragInitBlock *theInitBlock );
extern pascal void	__terminate( void );

long PE_PluginFunc( long selectorCode, void* pb, void* returnData )
{
	long		returnCode = 0;
		
	gRefNum = ::FSpOpenResFile( &gRsrcFile, fsRdPerm );
	switch ( selectorCode )
	{
		// ¥Êfill in the version in pb
		case kGetPluginVersion:
			*(long*)pb = 0x00010000;
			*(long*)returnData = NULL;
			returnCode = 0;
		break;
		
		case kCreateNewDialConfig:
		case kModifyDialConfig:
			returnCode = MUC_CreateModifyDialConfiguration( (PPPConfigureStruct*)pb );
		break;
		
		case kDeleteDialConfig:
			returnCode = MUC_DeleteDialConfiguration( (PPPConfigureStruct*)pb );
		break;
		
		case kOpenConnection:
			returnCode = MUC_OpenDialConnection( pb );
		break;
	
		case kCloseConnection:
			returnCode = MUC_CloseDialConnection( pb );
		break;
			
		case kGetActivePPPConfig:
			returnCode = MUC_GetActiveDialConfiguration( (Str255*)pb );
		break;
		
		case kSetActivePPPConfig:
			returnCode = MUC_SetActiveDialConfiguration( (Str255*)pb );
		break;

		case kGetActiveOTConfig:
			returnCode = MUC_GetActiveOTConfiguration( (Str255*)pb );
		break;
		
		case kSetActiveOTConfig:
			returnCode = MUC_SetActiveOTConfiguration( (Str255*)pb );
		break;
		
		case kGetConnectionState:
			returnCode = MUC_GetDialConnectionState( (Boolean*)pb );
		break;
					
		case kGetAutoConnectState:
			returnCode = MUC_GetAutoConnectState( (Boolean*)pb );
		break;
		
		case kSetAutoConnectState:
			returnCode = MUC_SetAutoConnectState( (Boolean*)pb );
		break;
		
		case kGetModemsList:
			returnCode = MUC_GetModemsList( (Str255**)pb );
		break;
		
		case kGetActiveModemName:
			returnCode = MUC_GetActiveModemName( (Str255*)pb );
		break;
		
		case kGetReceivedIPPacketCount:
			returnCode = MUC_GetReceivedIPPacketCount( (unsigned long*)pb );
		break;
		
		case kSelectDialConfig:
			if ( gMUCHandler )
			{
				returnCode = gMUCHandler->SelectProfile( (FSSpec*)pb, false );
				if ( returnCode == errNeedToRunAccountSetup )
				{
					// ¥Êcopy local file URL of AS into returnData
				}
			}
		break;
		
		case kAutoSelectDialConfig:
			if ( gMUCHandler )
			{
				returnCode = gMUCHandler->SelectProfile( (FSSpec*)pb, true );
				if ( returnCode == errNeedToRunAccountSetup )
				{
				}
			}
		break;
		
		case kEditDialConfig:
			if ( gMUCHandler )
				returnCode = gMUCHandler->EditProfile( (FSSpec*)pb );
		break;
		
		case kGetDialConfig:
			if ( gMUCHandler )
				returnCode = gMUCHandler->GetProfile( (FSSpec*)pb, (FreePPPInfo*)returnData );
		break;
			
		case kSetDialConfig:
			if ( gMUCHandler )
				returnCode = gMUCHandler->SetProfile( (FSSpec*)pb, (FreePPPInfo*)returnData );
		break;
			
		case kNewProfileSelect:
			if ( gMUCHandler )
				returnCode = gMUCHandler->ProfileSelectionChanged( (FSSpec*)pb );
		break;
		
		case kClearProfileSelect:
			if ( gMUCHandler )
				returnCode = gMUCHandler->ProfileSelectionChanged( NULL );
		break;
		
		case kInitListener:
			if ( gMUCHandler )
				gMUCHandler->InitDialog( (LDialogBox*)pb );
		break;
		
	}
	::CloseResFile( gRefNum );
	return returnCode;
}

pascal OSErr __muc_initialize( const CFragInitBlock* block )
{
	OSErr err;
	
	err = __initialize( block );
	
	if ( err == noErr )
	{
		if ( !gMUCHandler )
			gMUCHandler =  new LMUCHandler;

		if ( !gMUCHandler )

#ifdef CWGOLD11
			return fragNoMem;
#else
			return cfragInitFunctionErr;
#endif
			
		gMUCHandler->RegisterClasses();

#ifdef CWGOLD11
		CFragHFSLocator			location;
#else
		CFragSystem7Locator		location;
#endif
		location = block->fragLocator;
		if ( location.where == kDataForkCFragLocator )
			gRsrcFile = *(location.u.onDisk.fileSpec);
	}		
	return err;
}

pascal void __muc_terminate()
{
	if ( gMUCHandler )
		delete gMUCHandler;
	gMUCHandler = NULL;
	__terminate();
}

static long MUC_CreateModifyDialConfiguration( PPPConfigureStruct* data )
{
	long		configError = noErr;

	Boolean		configureTCP;
	Boolean		configureIC;
	ConfigStringArray	configStrList;
	
	if ( gRefNum == -1 )
		return fnfErr;
	
	configError = InitConfigStrList( configStrList, data, &configureTCP, &configureIC );
	if ( configError == noErr )
	{
		Try_
		{
			ThrowIfOSErr_( CallPPPRequest( eNewConfigureRequest, (void*)data ) );
			CreateModifyPrefs( configStrList, gRefNum, configureTCP, configureIC, OTAvailable() );
		}
		Catch_( e )
		{
			configError = e;
		}
	}
	
	if ( !OTAvailable() && configError == noErr )
	{
		// No OpenTransport, we might need to reboot
		if ( gMacTCPDirty )
			configError = 1;
	}
	// If 	configError < 0  (An error occured)
	// If	configError == 0  noError, no reboot required
	// If   configError > 0 and MacTCP is running, noError And Reboot required
	return configError;
}

static long MUC_DeleteDialConfiguration( PPPConfigureStruct* data )
{
	Boolean			dummy;
	short			configError = noErr;	// This should be global
	ConfigStringArray	configStrList;

	if ( !OTAvailable() )	
		return 0;

	configError = InitConfigStrList( configStrList, data, &dummy, &dummy );
	if ( configError == noErr )
	{
		Try_
		{
			ThrowIfOSErr_( CallPPPRequest( eDeleteConfiguration, (void*)data ) );
			DeleteConfig( configStrList );
		}
		Catch_( e )
		{
			configError = e;
		}
	}

	return configError;
}

static long MUC_OpenDialConnection( void* )
{
	return OpenFreePPP();
}

static long MUC_CloseDialConnection( void* )
{
	return CloseFreePPP();
}

static long MUC_GetActiveDialConfiguration( Str255* pppConfig )
{
	GetCurrentAccountName( pppConfig );
	return noErr;
}

static long MUC_SetActiveDialConfiguration( Str255* pppConfig )
{
	return SetCurrentAccountName( pppConfig );
}

static long MUC_GetActiveOTConfiguration( Str255* otConfig )
{
	return noErr;
}

static long MUC_SetActiveOTConfiguration( Str255* otConfig )
{
	long				err;
	OTChangeConfigPB	pb;
	Handle				resource;
	OTConfigProcUPP		thing;
	
	// ¥ this is our safe FAT resource (an 'sdes')
	resource = ::Get1Resource( 'COTC', 128 );
	
	if ( !resource )
		return errCantLoadNeededResources;

	DetachResource( resource );
	HLock( resource );
	
	pb.selector = kOTChangeSelector_CheckChange;
	LString::CopyPStr( (unsigned char*)&pb.configurationName, (unsigned char*)otConfig );
	thing = (OTConfigProcUPP)*resource;
	err = CallOTConfigProc( thing, &pb );
	if ( err == noErr )
	{
		// ¥Êassume it's benign to go bitch-slap OT
		pb.selector = kOTChangeSelector_Reconfigure;
		err = CallOTConfigProc( thing, &pb );
	}
	else if ( err == kOTErr_OTConfigurationNotPresent )
	{
		// ¥ fill in the OTChangeConfigPB with the rest of the relevant stuff from
		//	the AccountSpecDef, and call back into the code resource to create it
	}
	
	DisposeHandle( resource );		// Get rid of the OT reconfig glue
	
	return err;

}

static long MUC_GetDialConnectionState( Boolean* connected )
{
	*connected = IsFreePPPOpen();
	return noErr;
}

static long MUC_GetAutoConnectState( Boolean* allowsAutoConnect )
{
	*allowsAutoConnect = GetAutoConnectState();
	return noErr;
}

static long MUC_SetAutoConnectState( Boolean* allowsAutoConnect )
{
	return SetAutoConnectState( *allowsAutoConnect );
}

static long MUC_GetModemsList( Str255** modemsList )
{
	return GetModemsList( modemsList );
}

static long MUC_GetActiveModemName( Str255* modemName )
{
	GetCurrentModemName( modemName );
	return noErr;
}

static long MUC_GetReceivedIPPacketCount( unsigned long* packetCount )
{
	return GetReceivedIPPacketCount( packetCount );
}

static OSErr InitConfigStrList( ConfigStringArray configStrList, PPPConfigureStruct* data,
						Boolean* configureTCP, Boolean* configureIC )
{
	Str255					empty="\p";
	long					dns1 = 0;
	long					dns2 = 0;	// Used to avoid duplicate DNS settings
	
	int						i;

	if ( ( configStrList == NULL ) || ( data == NULL ) )
		return fnfErr;

	*configureTCP = false;
	*configureIC = false;

	// Initialize the data
	for ( i = 0; i < eNumConfigStrings; i++ )
		LString::CopyPStr( empty, configStrList[ i ] );
	AddressToString( 0UL, configStrList[ eDNSAddress ] );	// These need to be initialized to 0.0.0.0
	AddressToString( 0UL, configStrList[ eAltDNSAddress ] );	// These need to be initialized to 0.0.0.0
	AddressToString( 0UL, configStrList[ eBackupDNSAddress ] );	// These need to be initialized to 0.0.0.0
	
	i = 0;
	while ( data[ i ].id != 0 )
	{
		switch ( data[ i ].id )
		{
			// TCP prefs
			case kAccountID:
				DebugNumStr( "\pAccountID", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eSiteName ] );
				break;

			case kAsearchDomain:
				DebugNumStr( "\p kAsearchDomain", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eDomainName ] );
				*configureTCP = true;
				break;
			
			case kAprimaryDNS:
				DebugNumStr( "\p kAprimaryDNS", data[ i ].value );
				dns1 = (long)data[ i ].value;
				AddressToString( (unsigned long)data[ i ].value, configStrList[ eDNSAddress ] );
				*configureTCP = true;
				break;
			
			case kAsecondaryDNS:
				dns2 = (long)data[ i ].value;
				if ( (long)data[ i ].value == dns1 )	// Avoid the DNS duplicates
					AddressToString( 0UL, configStrList[ eAltDNSAddress ] );
				else
					AddressToString( (unsigned long)data[ i ].value, configStrList[ eAltDNSAddress ] );
				*configureTCP = true;
				break;
			
			case kAbackupDNS:
				DebugNumStr( "\p kAbackupDNS", data[ i ].value );
				if ( ( (long)data[ i ].value == dns1 ) || ( (long)data[ i ].value == dns2 ) )
					AddressToString( 0UL, configStrList[ eBackupDNSAddress ] );
				else
					AddressToString( (unsigned long)data[ i ].value, configStrList[ eBackupDNSAddress ] );
				*configureTCP = true;
				break;
						
			// IC prefs
			case kConfigPluginLogin:
				DebugNumStr( "\p kConfigPluginLogin", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eUserLogin ] );
				*configureIC = true;
				break;
			case kConfigPluginPassword:
				DebugNumStr( "\p kConfigPluginPassword", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eUserPassword ] );
				*configureIC = true;
				break;
			case kConfigPluginPopLogin:
				DebugNumStr( "\p kConfigPluginPopLogin", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eEMailLogin ] );
				*configureIC = true;
				break;
			case kConfigPluginPopPassword:
				DebugNumStr( "\p kConfigPluginPopPassword", data[ i ].value);
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eEMailPassword ] );
				*configureIC = true;
				break;
			case kConfigPluginReturnAddress:
				DebugNumStr( "\p kConfigPluginReturnAddress", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eEMailReturnAddress ] );
				*configureIC = true;
				break;
			case kConfigPluginPopServer:
				DebugNumStr( "\p kConfigPluginPopServer", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eEMailPOPServer ] );
				*configureIC = true;
				break;
			case kConfigPluginNNTPHost:
				DebugNumStr( "\p kConfigPluginNNTPHost", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eNNTPHost ] );
				*configureIC = true;
				break;
			case kConfigPluginSMTPHost:
				DebugNumStr( "\p kConfigPluginSMTPHost", data[ i ].value );
				LString::CopyPStr( (unsigned char*)data[ i ].value, configStrList[ eSMTPHost ] );
				*configureIC = true;
				break;
			default:
				DebugNumStr("\p Dunno", data[i].value);
				// It is OK, we ignore the unknown prefss
				break;
		}
		i++;
	}

	return noErr;
}

void AddressToString( unsigned long address, StringPtr string )
{
	Str255			aString;
	unsigned char	net;
	unsigned char	subnet1;
	unsigned char	subnet2;
	unsigned char	node;

	string[ 0 ] = 0;

	net = ( address & 0xff000000 ) >> 24;
	subnet1 = ( address & 0x00ff0000 ) >> 16;
	subnet2 = ( address & 0x0000ff00 ) >> 8;
	node = address & 0x000000ff;
	
	NumToString( (long)net, aString );
	LString::CopyPStr( aString, string );
	
	LString::AppendPStr( string,"\p." );

	NumToString( (long)subnet1, aString);
	LString::AppendPStr( string, aString );
	LString::AppendPStr( string, "\p." );

	NumToString( (long)subnet2, aString );
	LString::AppendPStr( string, aString );
	LString::AppendPStr( string, "\p." );
	
	NumToString( (long)node, aString );
	LString::AppendPStr( string, aString );
}

unsigned long AddressToLong( StringPtr addressStr )
{
	unsigned long		address = 0;
	long				offset = 1;
	long				i;
	long 				count;
	unsigned char		buffer[ 5 ];
	long				addressLen = addressStr[ 0 ];

	long	shiftValue = 24;
	
	for ( count = 0; count < 4; count++ )
	{
		buffer[ 0 ] = 0;
		for ( i = 1; i < 4 ; i++, offset++ )
		{
			if ( offset > addressLen )
				break;
					
			if ( addressStr[ offset ] == '.' )
				break;
			
			buffer[ i ] = addressStr[ offset ] ;
			buffer[ 0 ] += 1;
		}
		buffer[ i ] = 0;
		offset++;
		
		::StringToNum( (StringPtr)buffer, &i );
		address |= (unsigned char)i << shiftValue;
		shiftValue -= 8;
	}
	
	return address;
}

static Boolean OTAvailable()
{
	// Does system have OpenTransport TCP?
	OSErr 			error;
	long 			response;
	
	//	Check for Open Transport.
	//	gestaltOpenTpt = 'otan'
	//	gestaltOpenTptPresent = 0x00000001
	//	gestaltOpenTptTCPPresent = 0x00000010
	error = Gestalt( gestaltOpenTpt, &response);
	if ( !error )
		return ( ( response & 0x00000011 ) ? true : false );
	else
		return false;
}



