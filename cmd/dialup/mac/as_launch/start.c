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

#include "startIncludes.h"



void
main(void)
{
		AEAddressDesc		editorAddr={typeNull,NULL};
		AEDesc			launchDesc={typeNull,NULL};
		AEDescList		fileList={typeNull,NULL};
		AliasHandle		docAliasH,theAliasH=NULL;
		AppleEvent		theAEEvent={typeNull,NULL};
		Boolean			appRunningFlag,createdMagicFile=FALSE;
		EventRecord		theEvent;
		FInfo			fndrInfo;
		FSSpec			theAppFSSpec,theDocFSSpec;
		Handle			h;
		OSErr			err;
		ProcessInfoRec		theProcInfo;
		ProcessSerialNumber	thePSN;
		Str255			theName;
		short			foundVRefNum,refNum,saveResFile;
		long			foundDirID,theTick;

	MaxApplZone();
	InitGraf(&qd.thePort);
	InitFonts();
	InitMenus();
	InitWindows();
	TEInit();
	InitDialogs(0L);
	InitCursor();

	// if Netscape process is running, and its version 4.x or later, then quit it, then relaunch it

	theProcInfo.processInfoLength = sizeof(theProcInfo);
	theProcInfo.processName = theName;
	theProcInfo.processLocation = NULL;
	theProcInfo.processAppSpec = &theAppFSSpec;
	if (isAppRunning(NETSCAPE_SIGNATURE,&thePSN,&theProcInfo) == TRUE)	{
		if (checkVERS(&theAppFSSpec)==FALSE)	{
			showError(NETSCAPE_VERS_TOO_OLD_ERR);
			ExitToShell();
			}
		err=QuitApp(&thePSN);

		// wait for up to "TIME_TO_DIE" seconds for Netscape to die
		
		theTick=TickCount() + TIME_TO_DIE;
		while (theTick > TickCount())	{
			WaitNextEvent(everyEvent,&theEvent,TIME_TO_PAUSE,NULL);
			if (!(appRunningFlag=isAppRunning(NETSCAPE_SIGNATURE,&thePSN,NULL)))	{
				break;
				}
			}
		if (appRunningFlag)	{
			// Netscape didn't quit within "TIME_TO_DIE"... display an error maybe?  Or just give up.
			ExitToShell();
			}
		}
	else	{

		// search desktop database, find Netscape 4.x or later

		if (err=FindApp(NETSCAPE_SIGNATURE,&theAppFSSpec))	{
			if (err != userCanceledErr)	{
				showError(UNABLE_TO_LOCATE_NETSCAPE_APP_ERR);
				}
			ExitToShell();
			}
		}


	err=FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &foundVRefNum, &foundDirID);
	if (err)	{
		showError(UNABLE_TO_LOCATE_PREFS_FOLDER_ERR);
		ExitToShell();
		}

#if ACCOUNT_SETUP_BUILD

	// get alias to start.htm (Installer placed it in our binary)

	if (!(theAliasH=(AliasHandle)Get1IndResource(rAliasType,1)))	{
		showError(UNABLE_TO_LOCATE_ACCOUNT_SETUP_ERR);
		ExitToShell();
		}
	DetachResource((Handle)theAliasH);

	// create/find Account Setup file (with magic filetype)

#ifdef	ACCOUNT_SETUP_B3_HACK
	err=FSMakeFSSpec(theAppFSSpec.vRefNum, theAppFSSpec.parID, START_ACCTSETUP_NAME_B3, &theDocFSSpec);
	if (err==fnfErr)	{
		(void)FSpCreate(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_ACCTSETUP_SIGNATURE, smSystemScript);
		FSpCreateResFile(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_ACCTSETUP_SIGNATURE, smSystemScript);
		createdMagicFile=TRUE;

		// for B3 hack, make the magic file invisible if we created it

		if (err==fnfErr)	{
			if (!(err=FSpGetFInfo(&theDocFSSpec,&fndrInfo)))	{
				fndrInfo.fdFlags |= fInvisible;
				err=FSpSetFInfo(&theDocFSSpec,&fndrInfo);
				}
			}
		}
#else
	err=FSMakeFSSpec(foundVRefNum, foundDirID, START_ACCTSETUP_NAME, &theDocFSSpec);
	if (err==fnfErr)	{
		err=FSpCreate(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_ACCTSETUP_SIGNATURE, smSystemScript);
		FSpCreateResFile(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_ACCTSETUP_SIGNATURE, smSystemScript);
		}
#endif

	// add alias to start.htm into magic file (Netscape will use it)

	saveResFile = CurResFile();
	if ((refNum=FSpOpenResFile(&theDocFSSpec, fsRdWrPerm)) == kResFileNotOpened)	{
		// XXX display an error? Or just give up.
		ExitToShell();
		}
	UseResFile(refNum);
	if (h=Get1Resource(rAliasType, 1))	{							// if already exists, delete old before adding new
		RemoveResource(h);
		UpdateResFile(refNum);
		}
	AddResource((Handle)theAliasH, rAliasType, 1, "\p");
	WriteResource((Handle)theAliasH);
	CloseResFile(refNum);
	UseResFile(saveResFile);

	// hack to set default Component Dock pref to docked-state

	modifyComponentDockPref(&theAppFSSpec);

#elif PROFILE_MANAGER_BUILD

	// create/find {Preferences}:Netscape Profiles Temp file (with magic filetype)

	err=FSMakeFSSpec(foundVRefNum, foundDirID, START_PROFILE_NAME, &theDocFSSpec);
	if (err==fnfErr)	{
		err=FSpCreate(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_PROFILE_SIGNATURE, smSystemScript);
		FSpCreateResFile(&theDocFSSpec, NETSCAPE_SIGNATURE, MAGIC_PROFILE_SIGNATURE, smSystemScript);
		}

#else

#error Is this an Account Setup or Profile Manager build?

#endif

	if (err)	{
		ExitToShell();
		}

	// create AppleEvent and launch app

	if (err=AECreateDesc(typeApplSignature,&fndrInfo.fdCreator,sizeof(OSType),&editorAddr))	{}
	else if (err=AECreateList(NULL,0L,FALSE,&fileList))      {}					// create list of aliases to file(s)
	else if (err=NewAlias(NULL,&theDocFSSpec,&docAliasH))	{}
	else	HLock((Handle)docAliasH);
	if (err)	{}
	else if (err=AEPutPtr(&fileList, 1L, typeAlias, *docAliasH, GetHandleSize((Handle)docAliasH)))	{}
	else if (err=AECreateAppleEvent(kCoreEventClass,kAEOpenDocuments,&editorAddr,kAutoGenerateReturnID,kAnyTransactionID,&theAEEvent))	{}
	else if (err=AEPutParamDesc(&theAEEvent,keyDirectObject,&fileList))    {}
	else if (err=AECoerceDesc(&theAEEvent,typeAppParameters,&launchDesc))	{}			// coerce event to launch parameter
	else if (err=LaunchApp(&theAppFSSpec, &launchDesc))	{}

#ifdef	ACCOUNT_SETUP_B3_HACK
	// if created magic file, wait for up to "TIME_TO_STARTUP" seconds for Netscape to start and use it, then try to delete it

	else if (createdMagicFile==TRUE)	{
		theTick=TickCount() + TIME_TO_STARTUP;
		while (theTick > TickCount())	{
			WaitNextEvent(everyEvent,&theEvent,TIME_TO_PAUSE,NULL);
			}
		(void)FSpDelete(&theDocFSSpec);
		}
#endif
}



void
showError(short errStrIndex)
{
		Str255			appToLaunchNameString={0},errorString={0};

	GetIndString(errorString, ERROR_STR_RESID, errStrIndex);
	GetIndString(appToLaunchNameString, APP_NAME_STR_RESID, APP_NAME_STR_ID);
	ParamText(errorString, appToLaunchNameString, "\p", "\p");
	StopAlert(ERROR_DIALOG_RESID, NULL);
}
