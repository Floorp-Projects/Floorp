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


	jbool		rebootNowNeeded = FALSE;



/*
	isAppRunning:	return TRUE or FALSE depending upon whether a certain process is running
*/

Boolean
isAppRunning(OSType theSig, ProcessSerialNumber *thePSN, ProcessInfoRec *theProcInfo)
{
		Boolean                 flag=FALSE;
		OSErr                   err=noErr;
		ProcessInfoRec          theProcessInfo;

	if (theProcInfo == NULL)	{
		theProcInfo = &theProcessInfo;
		theProcInfo->processInfoLength = sizeof(ProcessInfoRec);
		theProcInfo->processName = NULL;
		theProcInfo->processLocation = NULL;
		theProcInfo->processAppSpec = NULL;
		}

	thePSN->highLongOfPSN=0L;								// start at beginning of process list
	thePSN->lowLongOfPSN=kNoProcess;

	while(GetNextProcess(thePSN) != procNotFound)     {					// loop over all processes
		GetProcessInformation(thePSN,theProcInfo);
		if (theProcInfo->processSignature==theSig)    {					// if we found the appropriate process, we're done
			flag=TRUE;
			break;
			}
		}
	return(flag);
}



OSErr
FindAppInCurrentFolder(OSType theSig, FSSpecPtr theFSSpecPtr)
{
		CInfoPBRec		cBlock;
		OSErr			err=fnfErr;
		ProcessInfoRec		theProcInfo;
		ProcessSerialNumber	thePSN;
		short			ioFDirIndex=1;

	// get current process location

	if (err=GetCurrentProcess(&thePSN))	return(err);
	theProcInfo.processInfoLength = sizeof(theProcInfo);
	theProcInfo.processName = NULL;
	theProcInfo.processLocation = NULL;
	theProcInfo.processAppSpec = theFSSpecPtr;
	if (err=GetProcessInformation(&thePSN,&theProcInfo))	return(err);

	// search folder for app

	cBlock.hFileInfo.ioCompletion=NULL;
	cBlock.hFileInfo.ioNamePtr=theFSSpecPtr->name;
	cBlock.hFileInfo.ioVRefNum=theFSSpecPtr->vRefNum;
	do	{
		cBlock.hFileInfo.ioDirID=theFSSpecPtr->parID;
		cBlock.hFileInfo.ioFDirIndex=ioFDirIndex++;
		if (err=PBGetCatInfoSync(&cBlock))	break;
		if ((cBlock.hFileInfo.ioFlFndrInfo.fdType=='APPL') && (cBlock.hFileInfo.ioFlFndrInfo.fdCreator==theSig))	{
			break;
			}
		} while(!err);
	return(err);
}



/*
	LaunchApp:	search for an application in the desktop database, and launch it if found

	Assumption:	isAppRunning has already checked that the application is not currently running
*/

OSErr
LaunchApp(OSType theSig, Boolean appInForegroundFlag, AEDesc *launchDesc, Boolean searchAppFolderFlag)
{
		Boolean                 foundFlag=FALSE;
		DTPBRec                 theDatabase;
		FSSpec                  theFSSpec;
		GetVolParmsInfoBuffer   volInfo;
		HParamBlockRec          hpb;
		LaunchParamBlockRec     lBlock;
		OSErr                   err=noErr;
		OSType			applType = {'APPL'};
		ParamBlockRec           pb;
//		StandardFileReply	theReply;
		short                   volIndex=0,vRefNum;
		long                    dirID;

	if (searchAppFolderFlag==TRUE)	{
		if (!(err=FindAppInCurrentFolder(theSig,&theFSSpec)))	{			// look in folder of current process
			foundFlag=TRUE;
			}
		}
	if ((foundFlag==FALSE) && (!(err=FindFolder(kOnSystemDisk,kSystemFolderType,FALSE,&vRefNum,&dirID))))   {       // look on startup volume first
		while (!err)    {
			hpb.ioParam.ioCompletion=NULL;
			hpb.ioParam.ioNamePtr=NULL;
			hpb.ioParam.ioVRefNum=vRefNum;
			hpb.ioParam.ioBuffer=(Ptr)&volInfo;
			hpb.ioParam.ioReqCount=sizeof(volInfo);
	                if (!(err=PBHGetVolParmsSync(&hpb)))    {
	                        if ((volInfo.vMAttrib & (1L << bHasDesktopMgr)) != 0)   {       // volume has desktop db?
					theDatabase.ioCompletion=NULL;
					theDatabase.ioNamePtr=NULL;
					theDatabase.ioVRefNum=vRefNum;
	                                if (!(err=PBDTGetPath(&theDatabase)))   {               // if so, open db
						theDatabase.ioIndex=0;                                  // most recent creation date
						theDatabase.ioFileCreator=theSig;
						theDatabase.ioNamePtr=(StringPtr)theFSSpec.name;
	                                        if (!(err=PBDTGetAPPL(&theDatabase,FALSE)))     {
	                                                theFSSpec.vRefNum=theDatabase.ioVRefNum;
	                                                theFSSpec.parID=theDatabase.ioAPPLParID;
	                                                foundFlag=TRUE;
	                                                break;
	                                                }
	                                        else if (err==afpItemNotFound)	{
	                                        	err=noErr;
	                                        	}
	                                        }
	                                }
	                        }
			pb.volumeParam.ioCompletion=NULL;
			pb.volumeParam.ioNamePtr=NULL;
			pb.volumeParam.ioVolIndex=++volIndex;
	                if (!(err=PBGetVInfoSync(&pb))) {                                       // search next mounted volume
	                        vRefNum=pb.volumeParam.ioVRefNum;
	                        }
	                else if (err==nsvErr)   {                                               // stop if no more volumes
	                        err=fnfErr;
	                        break;
	                        }
	                }
/*
		if (err==fnfErr)	{							// if not found, prompt for it?
			StandardGetFile(NULL, 1, &applType, &theReply);
			if (theReply.sfGood == TRUE)	{
				theFSSpec.vRefNum = theReply.sfFile.vRefNum;
				theFSSpec.parID = theReply.sfFile.parID;
				BlockMove(theReply.sfFile.name, theFSSpec.name, 1L+(unsigned)theFSSpec.name[0]);
				foundFlag=TRUE;
				}
			}
*/
		}
        if (!err && foundFlag)  {
		lBlock.launchBlockID=extendedBlock;
		lBlock.launchEPBLength=extendedBlockLen;
		lBlock.launchFileFlags=0;
		lBlock.launchControlFlags=launchContinue|launchNoFileFlags|launchUseMinimum;
		if (!appInForegroundFlag)       lBlock.launchControlFlags |= launchDontSwitch;
		lBlock.launchAppSpec=&theFSSpec;
		if (launchDesc)	{
			HLock(launchDesc->dataHandle);
			lBlock.launchAppParameters=(AppParametersPtr)*(launchDesc->dataHandle);
			}
		else	{
			lBlock.launchAppParameters=NULL;
			}
		err=LaunchApplication(&lBlock);
                }
	return(err);
}



/*
	QuitApp:	send a kAEQuitApplication AppleEvent to quit a running application

	Assumption:	isAppRunning has already checked that the application is currently running
*/

OSErr
QuitApp(ProcessSerialNumber *thePSN)
{
		AEAddressDesc		theAddr={typeNull,NULL};
		AppleEvent		theEvent={typeNull,NULL}, theReply={typeNull,NULL};
		OSErr			err=paramErr;

	if (!thePSN)	return(err);

	if (err=AECreateDesc(typeProcessSerialNumber,thePSN,sizeof(ProcessSerialNumber),&theAddr))	{}
	else if (err=AECreateAppleEvent(kCoreEventClass, kAEQuitApplication,&theAddr,kAutoGenerateReturnID,kAnyTransactionID,&theEvent))	{}
	else if (err=AESend(&theEvent,&theReply,kAENoReply+kAENeverInteract,kAENormalPriority,kAEDefaultTimeout,NULL,NULL))      {}

	if (err)	{
		SETUP_PLUGIN_ERROR("\p QuitApp error;g", err);
		}

	if (theAddr.dataHandle)		(void)AEDisposeDesc(&theAddr);
	if (theEvent.dataHandle)	(void)AEDisposeDesc(&theEvent);
	if (theReply.dataHandle)	(void)AEDisposeDesc(&theReply);

	return(err);
}



extern JRI_PUBLIC_API(jbool)
native_SetupPlugin_SECURE_0005fNeedReboot(JRIEnv* env,struct SetupPlugin* self)
{
SETUP_PLUGIN_TRACE("\p native_SetupPlugin_NeedReboot entered");
	if (rebootNowNeeded == TRUE)	{
		SETUP_PLUGIN_INFO_STR("\p NeedReboot: a reboot is needed", NULL);
		}	
SETUP_PLUGIN_TRACE("\p native_SetupPlugin_NeedReboot exiting");
	return(rebootNowNeeded);
}



/*
	Reboot:		send Restart AppleEvent to the Finder... if needed, created two aliases in the
			Startup Items folder.  Alias one restarts the current running Nav (just in case
			more than one Nav is installed, use the current one). Alias two brings up the
			specified HTML page.
	
	Note:		Items in the Startup Items folder opened in alphanumeric order.
			NAV_STARTUP_ALIAS_NAME and ACCOUNT_SETUP_STARTUP_ALIAS_NAME rely on this.
			
			The creator code for the HTML-page is forced to point to Netscape.
*/

extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fReboot(JRIEnv* env,struct SetupPlugin* self, java_lang_String *AccountSetupPathname)
{
		AEAddressDesc		finderAddr={typeNull,NULL};
		AppleEvent		theEvent={typeNull,NULL}, theReply={typeNull,NULL};
		FInfo			fndrInfo;
		FSSpec			theASFSSpec,navFSSpec;
		OSErr			err=noErr;
		ProcessInfoRec		netscapeProcInfo; 
		ProcessSerialNumber	finderPSN, netscapePSN;
		char			*accountSetupName;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_Reboot entered");

	if (AccountSetupPathname)	{			// do we need to create an alias in the Startup Items folder?
		if (accountSetupName=(char *)javaLangString2Cstr( env, AccountSetupPathname ))	{
			if (*accountSetupName)	{
				CtoPstr(accountSetupName);
				SETUP_PLUGIN_INFO_STR("\p Reboot: making alias for;g", accountSetupName);

				netscapeProcInfo.processInfoLength = sizeof(ProcessInfoRec);
				netscapeProcInfo.processName = NULL;
				netscapeProcInfo.processLocation = NULL;
				netscapeProcInfo.processAppSpec = &navFSSpec;
				if (isAppRunning(NETSCAPE_SIGNATURE, &netscapePSN, &netscapeProcInfo) == true)	{
				
					SETUP_PLUGIN_INFO_STR("\p Reboot: processAppSpec->name is;g", netscapeProcInfo.processAppSpec->name);

					if (!(err=createStartupFolderEntry(netscapeProcInfo.processAppSpec,NAV_STARTUP_ALIAS_NAME)))	{
						SETUP_PLUGIN_INFO_STR("\p Reboot: Nav alias created", NULL);

						if (!(err = FSMakeFSSpec(0, 0L, (unsigned char *)accountSetupName, &theASFSSpec)))	{
						
							// make sure the HTML file to load has a creator type of Netscape
							if (!(err=FSpGetFInfo(&theASFSSpec,&fndrInfo)))	{
								fndrInfo.fdCreator = NETSCAPE_SIGNATURE;
								if (err=FSpSetFInfo(&theASFSSpec,&fndrInfo))	{
									SETUP_PLUGIN_ERROR("\p Reboot: FSpSetFInfo error;g", err);
									}
								}
							else	{
								SETUP_PLUGIN_ERROR("\p Reboot: FSpGetFInfo error;g", err);
								}

							if (!(err=createStartupFolderEntry(&theASFSSpec,ACCOUNT_SETUP_STARTUP_ALIAS_NAME)))	{
								SETUP_PLUGIN_INFO_STR("\p Reboot: Account Setup alias created", NULL);
								}
							else	{
								SETUP_PLUGIN_ERROR("\p Reboot: creating ACCOUNT_SETUP_STARTUP_ALIAS_NAME startup alias error;g", err);
								}
							}
						else	{
							SETUP_PLUGIN_ERROR("\p Reboot: FSMakeFSSpec error;g", err);
							}
						}
					else	{
						SETUP_PLUGIN_ERROR("\p Reboot: creating NAV_STARTUP_ALIAS_NAME startup alias error;g", err);
						}
					}
				}
			}
		}
	else	{
		SETUP_PLUGIN_INFO_STR("\p Reboot: AccountSetupPathname not specified so no startup alias was created", NULL);
		}

	if (!err)	{
		if (isAppRunning(FINDER_SIGNATURE, &finderPSN, NULL) == true)	{	// if Finder is running, send it a Restart AppleEvent
			SETUP_PLUGIN_INFO_STR("\p Reboot: Finder is running so sending kAERestart", NULL);

			if (err=AECreateDesc(typeProcessSerialNumber,&finderPSN,sizeof(ProcessSerialNumber),&finderAddr))	{}
			else if (err=AECreateAppleEvent(kAEFinderEvents, kAERestart,&finderAddr,kAutoGenerateReturnID,kAnyTransactionID,&theEvent))	{}
			else if (err=AESend(&theEvent,&theReply,kAENoReply,kAENormalPriority,kAEDefaultTimeout,NULL,NULL))      {}

			if (err)	{
				SETUP_PLUGIN_ERROR("\p Reboot error;g", err);
				}

			if (finderAddr.dataHandle)	(void)AEDisposeDesc(&finderAddr);
			if (theEvent.dataHandle)	(void)AEDisposeDesc(&theEvent);
			if (theReply.dataHandle)	(void)AEDisposeDesc(&theReply);
			}
		else	{
			SETUP_PLUGIN_INFO_STR("\p Reboot: Finder is not running so unable to reboot", NULL);
			}
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_Reboot exiting");
}
