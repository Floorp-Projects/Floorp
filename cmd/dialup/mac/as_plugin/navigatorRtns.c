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

#include "pluginIncludes.h"

extern JRI_PUBLIC_API( void ) native_SetupPlugin_SECURE_0005fSetKiosk(
	JRIEnv* env, struct SetupPlugin* self, jbool flag )
{
	AEAddressDesc	netscapeAddr = { typeNull,NULL };
	AEDesc			objDesc = { typeNull, NULL };
	AEDesc			propertyDesc = { typeNull,NULL };
	AEDesc			propertyClassDesc = { typeNull, NULL };
	AEDesc			nullSpecifier = { typeNull, NULL };
	AEDesc			objSpecifier = { typeNull, NULL };
	AERecord		kioskRecord = { typeNull, NULL };
	AppleEvent		theEvent = { typeNull, NULL };
	AppleEvent		theReply = { typeNull, NULL };
	DescType		property = 'prop';
	DescType		propertyClass = KIOSK_MODE;
	DescType		objClass = cProperty;
	OSErr			err;
	ProcessSerialNumber	netscapePSN;
	long			kioskMode = 0L;

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_SetKiosk entered" );

	if ( isAppRunning( NETSCAPE_SIGNATURE, &netscapePSN, NULL ) == true )
	{
		kioskMode = ( flag==true ) ? 1L : 0L;

//		patchPopUpMenuSelect(flag);
		useCursor( watchCursor );

		if ( err = AECreateList( NULL, 0L, TRUE, &kioskRecord ) )
		{
		}										// create record list
		else if ( err = AECreateDesc( typeEnumeration, &objClass, sizeof( DescType ),&objDesc ) )
		{
		}
		else if ( err = AEPutKeyDesc( &kioskRecord, keyAEKeyForm, &objDesc ) )
		{
		}								// add keyAEKeyForm to record list
		else if ( err = AECreateDesc( typeType, &property, sizeof( DescType ), &propertyDesc ) )
		{
		}
		else if ( err = AEPutKeyDesc( &kioskRecord, keyAEDesiredClass, &propertyDesc ) )
		{
		}							// add keyAEDesiredClass to record list
		else if ( err = AECreateDesc( typeType, &propertyClass, sizeof( DescType ), &propertyClassDesc ) )
		{
		}
		else if ( err = AEPutKeyDesc( &kioskRecord, keyAEKeyData, &propertyClassDesc ) )
		{
		}							// add keyAEKeyData to record list
		else if ( err = AEPutKeyDesc( &kioskRecord, keyAEContainer, &nullSpecifier ) )
		{
		}							// add keyAEContainer (null represents app) to record list
		else if ( err = AECoerceDesc( &kioskRecord, typeObjectSpecifier, &objSpecifier ) )
		{
		}							// coerce to an object specifier
		else if ( err = AECreateDesc( typeProcessSerialNumber, &netscapePSN, sizeof( ProcessSerialNumber ), &netscapeAddr ) )
		{
		}
		else if ( err = AECreateAppleEvent( kAECoreSuite, kAESetData, &netscapeAddr, kAutoGenerateReturnID, kAnyTransactionID, &theEvent ) )
		{
		}
		else if ( err = AEPutKeyDesc( &theEvent, keyDirectObject, &objSpecifier ) )
		{
		}
		else if ( err = AEPutKeyPtr( &theEvent, keyAEData, typeLongInteger, &kioskMode, sizeof( long ) ) )
		{
		}					// add keyAEData (kioskMode value)
		else if ( err = AESend( &theEvent, &theReply, kAENoReply + kAENeverInteract, kAENormalPriority, kAEDefaultTimeout, NULL, NULL ) )
		{
		}

		if ( netscapeAddr.dataHandle )
			(void)AEDisposeDesc( &netscapeAddr );
		if ( objDesc.dataHandle )
			(void)AEDisposeDesc( &objDesc );
		if ( propertyDesc.dataHandle )
			(void)AEDisposeDesc( &propertyDesc );
		if ( propertyClassDesc.dataHandle )
			(void)AEDisposeDesc( &propertyClassDesc );
		if ( objSpecifier.dataHandle )
			(void)AEDisposeDesc( &objSpecifier );
		if ( kioskRecord.dataHandle )
			(void)AEDisposeDesc( &kioskRecord );
		if ( theEvent.dataHandle )
			(void)AEDisposeDesc( &theEvent );
		if ( theReply.dataHandle )
			(void)AEDisposeDesc( &theReply );
	}

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_SetKiosk exiting" );
}

extern JRI_PUBLIC_API( void ) native_SetupPlugin_SECURE_0005fQuitNavigator( JRIEnv* env,struct SetupPlugin* self )
{
	OSErr				err;
	ProcessSerialNumber	netscapePSN;

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_QuitNavigator entered" );

	if ( isAppRunning( NETSCAPE_SIGNATURE, &netscapePSN, NULL ) == true )
	{
		SETUP_PLUGIN_INFO_STR( "\p QuitNavigator: Nav is running... quitting.", NULL );

		useCursor( watchCursor );

		if ( err = QuitApp( &netscapePSN ) )
		{
			SETUP_PLUGIN_ERROR( "\p QuitNavigator error;g", err );
		}
	}

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_QuitNavigator exiting" );
}
