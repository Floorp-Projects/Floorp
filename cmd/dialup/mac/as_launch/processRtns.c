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



/*
	isAppRunning:	return TRUE or FALSE depending upon whether a certain process is running
*/

Boolean
isAppRunning(OSType theSig, ProcessSerialNumber *thePSN, ProcessInfoRec *theProcInfo)
{
		Boolean                 flag=FALSE;
		OSErr                   err=noErr;
		ProcessInfoRec          theProcessInfo;

	if (thePSN)	{
		if (theProcInfo == NULL)	{
			theProcInfo = &theProcessInfo;
			theProcInfo->processInfoLength = sizeof(theProcessInfo);
			theProcInfo->processName = NULL;
			theProcInfo->processLocation = NULL;
			theProcInfo->processAppSpec = NULL;
			}

		thePSN->highLongOfPSN=0L;							// start at beginning of process list
		thePSN->lowLongOfPSN=kNoProcess;

		while(GetNextProcess(thePSN) != procNotFound)     {				// loop over all processes
			GetProcessInformation(thePSN,theProcInfo);
			if (theProcInfo->processSignature==theSig)    {				// if we found the appropriate process, we're done
				flag=TRUE;
				break;
				}
			}
		}
	return(flag);
}



int
strlen(char *p)
{
		int			len=0L;

	if (p)	{
		while (*p++)	++len;
		}
	return(len);
}



void
modifyComponentDockPref(FSSpecPtr appFSSpecPtr)
{
		Handle			h;
		short			refNum,saveResFile;
		long			theOffset=0L;
	
	if (appFSSpecPtr)	{
		saveResFile = CurResFile();
		SetResLoad(FALSE);
		refNum=FSpOpenResFile(appFSSpecPtr, fsRdWrPerm);
		SetResLoad(TRUE);
		if (refNum != kResFileNotOpened)	{
			UseResFile(refNum);

			// check version resource
			
			if (h=Get1Resource(TEXT_RESOURCE,MAC_PREFS_TEXT_RES_ID))	{
				HNoPurge(h);
				HUnlock(h);
				if ((theOffset=Munger(h,theOffset,DOCK_UNDOCKED,(long)strlen(DOCK_UNDOCKED),DOCK_DOCKED,(long)strlen(DOCK_DOCKED)))>=0L)	{
					ChangedResource(h);
					WriteResource(h);
					UpdateResFile(refNum);
					}
				}

			CloseResFile(refNum);
			}
		UseResFile(saveResFile);
		}
}



Boolean
checkVERS(FSSpecPtr appFSSpecPtr)
{
		Handle			versH;
		Boolean			versionOKFlag=FALSE;
		short			refNum,saveResFile;
	
	if (appFSSpecPtr)	{
		saveResFile = CurResFile();
		SetResLoad(FALSE);
		refNum=FSpOpenResFile(appFSSpecPtr, fsRdPerm);
		SetResLoad(TRUE);
		if (refNum != kResFileNotOpened)	{
			UseResFile(refNum);

			// check version resource
			
			if (versH=Get1Resource(VERS_RESOURCE,VERS_RESOURCE_ID))	{
				HLock(versH);
				if (*(char *)(*versH) >= MIN_NETSCAPE_VERSION)	{
					versionOKFlag=TRUE;
					}
				ReleaseResource(versH);
				}

			CloseResFile(refNum);
			}
		UseResFile(saveResFile);
		}
	return(versionOKFlag);
}



pascal Boolean
netscapeFileFilter(CInfoPBPtr pb, void *data)
{
		Boolean			hideFlag=TRUE;
		//*dataPtr;

	if (pb)	{
		if ((pb->hFileInfo.ioFlFndrInfo.fdType==APPLICATION_TYPE) && (pb->hFileInfo.ioFlFndrInfo.fdCreator==NETSCAPE_SIGNATURE))	{
			hideFlag=FALSE;
			}
		}
	return(hideFlag);
}



OSErr
FindAppInCurrentFolder(OSType theSig, FSSpecPtr theFSSpecPtr)
{
		CInfoPBRec		cBlock;
		OSErr			err=fnfErr;
		ProcessInfoRec		theProcInfo;
		ProcessSerialNumber	thePSN;
		Str255			theName;
		short			ioFDirIndex=1;

	// get current process location

	if (err=GetCurrentProcess(&thePSN))	return(err);
	theProcInfo.processInfoLength = sizeof(theProcInfo);
	theProcInfo.processName = NULL;
	theProcInfo.processLocation = NULL;
	theProcInfo.processAppSpec = theFSSpecPtr;
	if (err=GetProcessInformation(&thePSN,&theProcInfo))	return(err);

	// search folder for app with new enough version

	cBlock.hFileInfo.ioCompletion=NULL;
	cBlock.hFileInfo.ioNamePtr=theFSSpecPtr->name;
	cBlock.hFileInfo.ioVRefNum=theFSSpecPtr->vRefNum;
	do	{
		cBlock.hFileInfo.ioDirID=theFSSpecPtr->parID;
		cBlock.hFileInfo.ioFDirIndex=ioFDirIndex++;
		if (err=PBGetCatInfoSync(&cBlock))	break;
		if ((cBlock.hFileInfo.ioFlFndrInfo.fdType==APPLICATION_TYPE) && (cBlock.hFileInfo.ioFlFndrInfo.fdCreator==theSig))	{
			if (checkVERS(theFSSpecPtr)==TRUE)	{
				break;
				}
			}
		} while(!err);
	return(err);
}



/*
	FindApp:	search for an application in the current folder, then in the desktop database

	Assumption:	isAppRunning has already checked that the application is not currently running
*/

OSErr
FindApp(OSType theSig, FSSpecPtr theFSSpecPtr)
{
		Boolean                 foundFlag=FALSE;
		DTPBRec                 theDatabase;
		FileFilterYDUPP		netscapeFileFilterUPP;
		GetVolParmsInfoBuffer   volInfo;
		HParamBlockRec          hpb;
		OSErr                   err=noErr;
		OSType				applType = {'APPL'};
		ParamBlockRec           pb;
		StandardFileReply	theReply;
		short                   volIndex=0,vRefNum;
		long                    dirID;
		Str255				fileString1;
		Point				centerPoint = {-1,-1};

	if (!(err=FindAppInCurrentFolder(theSig,theFSSpecPtr)))	{				// look in folder of current process
		foundFlag=TRUE;
		}
	else if (!(err=FindFolder(kOnSystemDisk,kSystemFolderType,FALSE,&vRefNum,&dirID)))   {	// look on startup volume
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
						theDatabase.ioNamePtr=(StringPtr)theFSSpecPtr->name;
						
	                                        while (!(err=PBDTGetAPPL(&theDatabase,FALSE)))     {
	                                                theFSSpecPtr->vRefNum=theDatabase.ioVRefNum;
	                                                theFSSpecPtr->parID=theDatabase.ioAPPLParID;
	                                        	if (checkVERS(theFSSpecPtr)==TRUE)	{
								foundFlag=TRUE;
		                                                break;
		                                                }
							++theDatabase.ioIndex;
	                                                }
						if (foundFlag == TRUE)	break;
	                                        if (err==afpItemNotFound)	{
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

		if (err==fnfErr)	{							// if not found, prompt for it
			if (netscapeFileFilterUPP=NewFileFilterYDProc(netscapeFileFilter))	{

				GetIndString(fileString1, ERROR_STR_RESID, WHERE_IS_STR_ID);
				
				ParamText(fileString1,"\p", "\p", "\p");
				CustomGetFile(netscapeFileFilterUPP, 2, &applType, &theReply, CUSTOMGETFILE_RESID, centerPoint, nil, nil, nil, nil, (void *)NULL);							// call StandardFile, tell it to list milan files

				DisposeRoutineDescriptor(netscapeFileFilterUPP);
				if (theReply.sfGood == TRUE)	{
					theFSSpecPtr->vRefNum = theReply.sfFile.vRefNum;
					theFSSpecPtr->parID = theReply.sfFile.parID;
					BlockMove(theReply.sfFile.name, theFSSpecPtr->name, 1L+(unsigned)theFSSpecPtr->name[0]);
					err=noErr;
					foundFlag=TRUE;
					}
				else	{
					err=userCanceledErr;
					}
				}
			}

		}
	return(err);
}



/*
	LaunchApp:	launch an application

	Assumption:	isAppRunning has already checked that the application is not currently running
*/

OSErr
LaunchApp(FSSpecPtr theFSSpecPtr, AEDesc *launchDesc)
{
		LaunchParamBlockRec     lBlock;
		OSErr			err=paramErr;

	if (theFSSpecPtr)	{
		lBlock.launchBlockID=extendedBlock;
		lBlock.launchEPBLength=extendedBlockLen;
		lBlock.launchFileFlags=0;
		lBlock.launchControlFlags=launchContinue|launchNoFileFlags|launchUseMinimum;
		lBlock.launchAppSpec=theFSSpecPtr;
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

	if (theAddr.dataHandle)		(void)AEDisposeDesc(&theAddr);
	if (theEvent.dataHandle)	(void)AEDisposeDesc(&theEvent);
	if (theReply.dataHandle)	(void)AEDisposeDesc(&theReply);

	return(err);
}
