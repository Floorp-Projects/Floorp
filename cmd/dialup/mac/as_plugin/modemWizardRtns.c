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
#include "MUC.h"

extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fOpenModemWizard(JRIEnv* env, struct SetupPlugin* self)
{
		OSErr			err;
		ProcessSerialNumber	thePSN;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_OpenModemWizard entered");

	if (isAppRunning(MODEM_WIZARD_SIGNATURE,&thePSN, NULL)==false)	{			// if Modem Wizard is not running, Launch it
		SETUP_PLUGIN_INFO_STR("\p OpenModemWizard: launching the modem wizard", NULL);

		err=LaunchApp(MODEM_WIZARD_SIGNATURE,TRUE,NULL,TRUE);
		if (err==fnfErr)	{
			showPluginError(UNABLE_TO_FIND_MODEM_WIZARD_STR_ID, FALSE);
			}
		else if (err)	{
			showPluginError(MODEM_WIZARD_LAUNCH_PROBLEM_STR_ID, FALSE);
			}
		}
	else	{
		SETUP_PLUGIN_INFO_STR("\p OpenModemWizard: bringing the modem wizard to the front", NULL);

		SetFrontProcess(&thePSN);							// if Modem Wizard is running, switch to it
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_OpenModemWizard exiting");
}



extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fCloseModemWizard(JRIEnv* env, struct SetupPlugin* self)
{
		AEAddressDesc		modemWizardAddr={typeNull,NULL};
		AppleEvent		theEvent={typeNull,NULL}, theReply={typeNull,NULL};
		OSErr			err;
		ProcessSerialNumber	modemWizardPSN;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_CloseModemWizard entered");

	if (isAppRunning(MODEM_WIZARD_SIGNATURE, &modemWizardPSN, NULL) == true)	{		// if Modem Wizard is running, send it a Quit AppleEvent
		SETUP_PLUGIN_INFO_STR("\p CloseModemWizard: closing the modem wizard", NULL);

		if (err=AECreateDesc(typeProcessSerialNumber,&modemWizardPSN,sizeof(ProcessSerialNumber),&modemWizardAddr))	{}
		else if (err=AECreateAppleEvent(kCoreEventClass, kAEQuitApplication,&modemWizardAddr,kAutoGenerateReturnID,kAnyTransactionID,&theEvent))	{}
		else if (err=AESend(&theEvent,&theReply,kAENoReply+kAENeverInteract,kAENormalPriority,kAEDefaultTimeout,NULL,NULL))      {}

		if (err)	{
			SETUP_PLUGIN_ERROR("\p CloseModemWizard error;g", err);
			}

		if (modemWizardAddr.dataHandle)	(void)AEDisposeDesc(&modemWizardAddr);
		if (theEvent.dataHandle)	(void)AEDisposeDesc(&theEvent);
		if (theReply.dataHandle)	(void)AEDisposeDesc(&theReply);
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_CloseModemWizard exiting");
}



extern JRI_PUBLIC_API(jbool)
native_SetupPlugin_SECURE_0005fIsModemWizardOpen(JRIEnv* env, struct SetupPlugin* self)
{
		ProcessSerialNumber	thePSN;
		jbool			theFlag;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_IsModemWizardOpen entered");

	theFlag = (jbool)isAppRunning(MODEM_WIZARD_SIGNATURE, &thePSN, NULL);				// look for a process with MODEM_WIZARD_SIGNATURE

	SETUP_PLUGIN_INFO_STR("\p IsModemWizardOpen;g", ((theFlag == TRUE) ? "\p yes" : "\p no"));

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_IsModemWizardOpen exiting");

	return(theFlag);
}



static int
CompareStringWrapper(const void *string1, const void *string2)
{
	return (CompareString(string1, string2, NULL));
}



extern JRI_PUBLIC_API(jstringArray) native_SetupPlugin_SECURE_0005fGetModemList(JRIEnv* env, struct SetupPlugin* self)
{
	JRIGlobalRef		globalRef = NULL;
	OSErr				err = noErr;
	Str255*				modemList = NULL;
	short				modemCount, x;
	void*				theArray;
	java_lang_String*	modem;

	SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetModemList entered");

	err = CallMUCPlugin( kGetModemsList, &modemList );

	if ( err != noErr )
	{
		SETUP_PLUGIN_ERROR( "\p GetModemList error;g", err );
		return NULL;
	}
	
	if ( modemList == NULL )
	{
		SETUP_PLUGIN_ERROR( "\p GetModemList: modemList is NULL", unimpErr );
		return NULL;
	}

	modemCount = (short)( GetPtrSize( (Ptr)modemList ) / sizeof( Str255 ) );			// modemList is not NULL terminated, so get ptr size
	qsort( modemList, modemCount, sizeof( Str255 ), CompareStringWrapper );			// modemList is not sorted, so use qsort
	if ( theArray = JRI_NewObjectArray( env, modemCount, class_java_lang_String(env), NULL ) )
	{
		globalRef = JRI_NewGlobalRef( env, theArray );					// locks the array
		for ( x = 0; x < modemCount; x++ )
		{
			if ( modemList[ x ] == NULL )
				break;

			PtoCstr( modemList[ x ] );
//			if (modem = JRI_NewStringUTF(env, (char *)modemList[x],strlen((char *)modemList[x])))	{
			if (modem = cStr2javaLangString( env, (char*)modemList[ x ], strlen( (char*)modemList[ x ] ) ) )
				JRI_SetObjectArrayElement( env, theArray, x, modem );
			CtoPstr( (char*)modemList[ x ] );
		}
		if ( globalRef )
			JRI_DisposeGlobalRef( env, globalRef );
	}

	for ( x = 0; x < modemCount; x++ )
		// we need to dispose of modemList strings
		DisposePtr( (Ptr)modemList[ x ] );

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_GetModemList exiting" );

	return theArray;
}



extern JRI_PUBLIC_API(java_lang_String *)
native_SetupPlugin_SECURE_0005fGetModemType(JRIEnv* env, struct SetupPlugin* self, java_lang_String *theModem)
{
		java_lang_String		*theString;
		char				*theModemStr;
		char				*theModemType="Modem";

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetModemType entered");

	if ((theModemStr = (char *)javaLangString2Cstr( env, theModem )) != NULL)	{
		if (strstr(theModemStr,"ISDN"))	{
			theModemType="ISDN";
			}
		}
	SETUP_PLUGIN_INFO_STR("\p GetModemType: the modem type is", theModemType);
//	theString=JRI_NewStringUTF(env,theModemType,strlen(theModemType));
	theString=cStr2javaLangString(env,theModemType,strlen(theModemType));

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetModemType exiting");

	return(theString);
}



extern JRI_PUBLIC_API(java_lang_String *) native_SetupPlugin_SECURE_0005fGetCurrentModemName( JRIEnv* env, struct SetupPlugin* self )
{
	java_lang_String*	theString = NULL;
	OSErr				err = noErr;
	Str255				modemName = { 0 };

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_GetCurrentModemName entered" );

	err = CallMUCPlugin( kGetActiveModemName, &modemName );

	if ( !err )
	{
		SETUP_PLUGIN_INFO_STR( "\p GetCurrentModemName: the modem name is;g", modemName );
		PtoCstr( modemName );
//		theString=JRI_NewStringUTF(env,(void *)modemName,strlen((void *)modemName));
		theString = cStr2javaLangString( env, (void*)modemName, strlen( (void*)modemName ) );
	}
	else
		SETUP_PLUGIN_ERROR( "\p GetCurrentModemName: error", err );

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_GetCurrentModemName exiting" );

	return theString;
}
