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



extern JRI_PUBLIC_API(struct java_lang_String *)
native_SetupPlugin_SECURE_0005fGetExternalEditor(JRIEnv* env, struct SetupPlugin* self)
{
		Boolean			targetIsFolder,wasAliased;
		Handle			h=NULL;
		OSErr			err=noErr;
		SFTypeList		typeList;
		StandardFileReply	theReply;
		java_lang_String	*retVal=NULL;
		Str255			fileString;							// necessary to get the resource strings for the CustomGetFile
		Point			centerPoint = {-1,-1};

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetExternalEditor entered");
	
	typeList[0] = 'APPL';										// look for applications and
	typeList[1] = kApplicationAliasType;								// application aliases

	GetIndString(fileString, CUSTOMGETFILE_RESID, CHOOSEEDITOR_STRINGID);
	ParamText(fileString,"\p", "\p", "\p");
	CustomGetFile(NULL, 2, typeList, &theReply, CUSTOMGETFILE_RESID, centerPoint, NULL, NULL, NULL, NULL, NULL);	// call StandardFile, tell it to list applications
	if (theReply.sfGood == TRUE)	{
 		if (!(err=ResolveAliasFile(&theReply.sfFile, TRUE, &targetIsFolder, &wasAliased)))	{
			SETUP_PLUGIN_INFO_STR("\p User selected;g", theReply.sfFile.name);

			if (h=pathFromFSSpec(&theReply.sfFile))	{
				HNoPurge(h);
				HLock(h);
//				retVal=JRI_NewStringUTF(env, (char *)*h, (unsigned)GetHandleSize(h));
				retVal=cStr2javaLangString(env, (char *)*h, (unsigned)GetHandleSize(h));
				DisposeHandle(h);
				}
			else	{
				SETUP_PLUGIN_INFO_STR("\p GetExternalEditor: pathFromFSSpec returned NULL.", NULL);
				}

			}
		else	{
			SETUP_PLUGIN_ERROR("\p GetExternalEditor: ResolveAliasFile error;g", err);
			}
		}
	else	{
		SETUP_PLUGIN_INFO_STR("\p User did not select an application.", NULL);
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetExternalEditor exiting");

	return(retVal);
}



extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fOpenFileWithEditor(JRIEnv* env, struct SetupPlugin* self, struct java_lang_String *app, struct java_lang_String *file)
{
		AEDesc			launchDesc={typeNull,NULL};
		AEAddressDesc		editorAddr={typeNull,NULL};
       		AEDescList		fileList={typeNull,NULL};
		AliasHandle		theAliasH=NULL;
		AppleEvent		theEvent={typeNull,NULL}, theReply={typeNull,NULL};
		Boolean			appRunningFlag;
		FInfo			fndrInfo;
		FSSpec			theAppFSSpec,theDocFSSpec;
		OSErr			err=noErr;
		ProcessSerialNumber	editorPSN={0,kNoProcess};
		char			*appName,*fileName;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_OpenFileWithEditor entered");

/*
	const char *appName = javaLangString2Cstr( env, app );
	JRI_ThrowNew(env, JRI_FindClass(env, "java/io/NullPointerException"), appName);
*/

	if (app == NULL)	{
		SETUP_PLUGIN_INFO_STR("\p OpenFileWithEditor: an app to use was not specified.", NULL);
		return;
		}
	appName = (char *)javaLangString2Cstr( env, app );
	CtoPstr((char *)appName);
	if (err = FSMakeFSSpec(0, 0L, (unsigned char *)appName, &theAppFSSpec))	{
		SETUP_PLUGIN_ERROR("\p OpenFileWithEditor: app FSMakeFSSpec error;g", err);
		return;
		}

	if (err = FSpGetFInfo(&theAppFSSpec,&fndrInfo))	{
		SETUP_PLUGIN_ERROR("\p OpenFileWithEditor: FSpGetFInfo error;g", err);
		return;
		}

	if (file == NULL)	{
		SETUP_PLUGIN_INFO_STR("\p OpenFileWithEditor: a file to open was not specified.", NULL);
		return;
		}
	fileName = (char *)javaLangString2Cstr( env, file );
	CtoPstr((char *)fileName);
	err = FSMakeFSSpec(0, 0L, (unsigned char *)fileName, &theDocFSSpec);
	if (err)	{
		SETUP_PLUGIN_ERROR("\p OpenFileWithEditor: doc FSMakeFSSpec error;g", err);
		return;
		}

	appRunningFlag = isAppRunning(fndrInfo.fdCreator,&editorPSN, NULL);
	if (appRunningFlag)	{
		SETUP_PLUGIN_INFO_STR("\p The selected editor is currently running.", NULL);

		err=AECreateDesc(typeProcessSerialNumber,&editorPSN,sizeof(ProcessSerialNumber),&editorAddr);		// if app is running, reference by PSN
		}
	else	{
		SETUP_PLUGIN_INFO_STR("\p The selected editor is not currently running.", NULL);

		err=AECreateDesc(typeApplSignature,&fndrInfo.fdCreator,sizeof(OSType),&editorAddr);			// if app is not running, reference by signature
		}

	if (!err)	{
		if (err=AECreateList(NULL,0L,FALSE,&fileList))      {}							// create list of aliases to file(s)
		else if (err=NewAlias(NULL,&theDocFSSpec,&theAliasH))	{}
		else	HLock((Handle)theAliasH);

		if (err)	{}
		else if (err=AEPutPtr(&fileList, 1L, typeAlias, *theAliasH, GetHandleSize((Handle)theAliasH)))	{}
		else if (err=AECreateAppleEvent(kCoreEventClass,kAEOpenDocuments,&editorAddr,kAutoGenerateReturnID,kAnyTransactionID,&theEvent))	{}
		else if (err=AEPutParamDesc(&theEvent,keyDirectObject,&fileList))    {}
		}
	if (!err)	{
		if (appRunningFlag)	{										// if app is running, send event
			if (err=AESend(&theEvent,&theReply,kAENoReply+kAENeverInteract,kAENormalPriority,kAEDefaultTimeout,NULL,NULL))      {}
			else	SetFrontProcess(&editorPSN);
			}
		else	{
			if (err=AECoerceDesc(&theEvent,typeAppParameters,&launchDesc))	{}				// if app is not running, coerce event to launch parameter
			else	LaunchApp(fndrInfo.fdCreator,TRUE,&launchDesc,FALSE);					// and launch it
			}
		}

	if (err)	{
		SETUP_PLUGIN_ERROR("\p OpenFileWithEditor error;g", err);
		}

	if (editorAddr.dataHandle)	(void)AEDisposeDesc(&editorAddr);
	if (fileList.dataHandle)	(void)AEDisposeDesc(&fileList);
	if (theEvent.dataHandle)	(void)AEDisposeDesc(&theEvent);
	if (theReply.dataHandle)	(void)AEDisposeDesc(&theReply);
	if (launchDesc.dataHandle)	(void)AEDisposeDesc(&launchDesc);
	if (theAliasH)			DisposeHandle((Handle)theAliasH);

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_OpenFileWithEditor exiting");
}
