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

// ulaunch.cp
// Launching of external applications through AppleEvents
// Created by atotic, June 14th, 1994
// Based on Apple's LaunchWithDoc example snippet

#include <Folders.h>
#include <AERegistry.h>
#include <Errors.h>

#include "BufferStream.h"

#include "PascalString.h"

#include "macutil.h"
#include "uprefd.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "reserr.h"
#include "ulaunch.h"

// ее PROTOTYPES
OSErr FindAppOnVolume(OSType sig, short vRefNum, FSSpec& thefile);

// Sends an ODOC event to creator in fndrInfo,
// with file in fileSpec
OSErr SendODOCEvent(OSType appSig, 
				LFileBufferStream * inFile);
				
// Launches the application with the given doc
void LaunchWithDoc(FInfo& fndrInfo, 
					FSSpec& appSpec,
					LFileBufferStream * inFile,
					const FSSpec inFileSpec);


// Displays a launching error alert.
int LaunchError(ResIDT alertID, OSType creator, const Str63& fileName, OSErr err);

// Creates Finder's OpenSelection event
OSErr BuildOpenSelectionEvent(FSSpec & fileSpec, AppleEvent& theEvent);

// Sends OpenSelection to Finder
OSErr SendOpenSelectionToFinder(FSSpec & fileSpec);

// ее Implementation



// Builds an ODOC event
OSErr BuildODOCEvent(OSType applSig, 
					FSSpec fileSpec, 
					AppleEvent& theEvent)	{
// Builds all the arguments for the event
	AEDesc myAddress;
	AEDesc docDesc;
	AEDescList theList;
	AliasHandle withThis;
	OSErr err;
	
// Anatomy of the event:
// Event class: kCoreEventClass
// Event ID: kAEOpenDocuments
// Event has target description (in the form of typeApplSignature)
// keyDirectObject is a list of aliases

  	err = AECreateDesc(typeApplSignature,
  						(Ptr)&applSig, sizeof(applSig),
  					 	&myAddress);
	if (err) return err;
	
	err = AECreateAppleEvent(kCoreEventClass, kAEOpenDocuments, 
							&myAddress, 
							kAutoGenerateReturnID, kAnyTransactionID, 
							&theEvent);
	if (err) return err;
	// create a list for the alaises.  In this case, I only have one, but 
	// you still need a list

	err = AECreateList(NULL, 0, FALSE, &theList);
	if (err) return err;

	/* create an alias out of the file spec */
	/* I'm not real sure why I did this, since there is a system coercion handler for */
	/* alias to FSSpec, but I'm paranoid */

	err = NewAlias(NULL, &fileSpec, &withThis);
	if (err) return err;

	HLock((Handle)withThis);

	/* now create an alias descriptor */
	err = AECreateDesc(typeAlias, (Ptr) * withThis, GetHandleSize((Handle)withThis), &docDesc);
	if (err) return err;

	HUnlock((Handle)withThis);

	/* put it in the list */
	err = AEPutDesc(&theList, 0, &docDesc);
	if (err) return err;
	err = AEPutParamDesc(&theEvent, keyDirectObject, &theList);
	err = AEDisposeDesc(&myAddress);
	err = AEDisposeDesc(&docDesc);
	err = AEDisposeDesc(&theList);
	return err;
}

// Sends an ODOC event to appliation
OSErr SendODOCEvent(OSType appSig, 
				LFileBufferStream * inFile)
{
	OSErr err;
	Try_ {
		AppleEvent	openEvent;
		FSSpec inFileSpec;
		inFile->GetSpecifier(inFileSpec);
		err = BuildODOCEvent(appSig, 
							inFileSpec, 
							openEvent);
		ThrowIfOSErr_(err);
		AppleEvent result;
		err = AESend(&openEvent, &result, 
					kAENoReply  + kAECanSwitchLayer,
					kAENormalPriority, kAEDefaultTimeout,
					NULL,NULL);
		AEDisposeDesc(&openEvent);
		// err could be memFullErr, app is out of memory
		ThrowIfOSErr_(err);
	}
	Catch_(inErr) {
		return inErr;
	} EndCatch_
	return err;
}

#define kDelete 2
#define kSave	1
#define kTryAgain 3
// Displays launch error dialogs with appropriate arguments.
// Alerts used are:
// ALRT_ODOCFailed
// ALRT_AppNotFound
// ALRT_AppMemFull
// ALRT_MiscLaunchError
// Returns: kDelete, kSave, or kTryAgain
int LaunchError(ResIDT alertID, OSType creator, const Str63& fileName, OSErr err)
{
		CMimeMapper * map = CPrefs::sMimeTypes.FindCreator(creator);
		ErrorManager::PrepareToInteract();
		CStr255		errorString;
		ErrorManager::OSNumToStr(err, errorString);
		ParamText(map->GetAppName(), fileName, errorString, "\p");
		UDesktop::Deactivate();
		int retVal = ::CautionAlert(alertID, NULL);
		UDesktop::Activate();
		return retVal;
}

// Launches an application with a given doc
OSErr StartDocInApp(FSSpec theDocument,  FSSpec theApplication)
{
	FInfo fndrInfo;
	OSErr	err;
	
	HGetFInfo(	theApplication.vRefNum,
				theApplication.parID,
				theApplication.name,
				&fndrInfo);
				
	FSSpec applSpecTemp;
	ProcessSerialNumber processSN;
	err = FindProcessBySignature(fndrInfo.fdCreator, 'APPL',  processSN, &applSpecTemp);
	if (err == noErr)	// App is running. Send 'odoc'
	{
		Try_ {
			AppleEvent theEvent;
			err = BuildODOCEvent(fndrInfo.fdCreator, theDocument, theEvent);
			ThrowIfOSErr_(err);
			AppleEvent result;
			err = AESend(&theEvent, &result, 
						kAENoReply + kAEAlwaysInteract + kAECanSwitchLayer,
						kAENormalPriority, kAEDefaultTimeout,
						NULL,NULL);
			AEDisposeDesc(&theEvent);
			// err could be memFullErr, app is out of memory
			ThrowIfOSErr_(err);
			if (IsFrontApplication())
				SetFrontProcess(&processSN);
		}
		Catch_(inErr) {
			return inErr;
		} EndCatch_
		
		return noErr;
	}

	Try_	{
		LaunchParamBlockRec launchThis;
		AEDesc launchDesc;
		AppleEvent theEvent;
	
		ThrowIfOSErr_(BuildODOCEvent(fndrInfo.fdCreator, theDocument, theEvent));
		ThrowIfOSErr_(AECoerceDesc(&theEvent, typeAppParameters, &launchDesc));
		
		launchThis.launchAppSpec = (FSSpecPtr)&theApplication;
		launchThis.launchAppParameters = (AppParametersPtr)*(launchDesc.dataHandle);
		launchThis.launchBlockID = extendedBlock;
		launchThis.launchEPBLength = extendedBlockLen;
		launchThis.launchFileFlags = NULL;
		launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
		if (!IsFrontApplication())
			launchThis.launchControlFlags += launchDontSwitch;
		err = LaunchApplication(&launchThis);
		ThrowIfOSErr_(err);
	}
	Catch_(inErr)
	{
	} EndCatch_
	return err;
}

// Launches the application with the given doc
void LaunchWithDoc(FInfo& fndrInfo, 
				FSSpec& appSpec,
				LFileBufferStream * inFile,
				const FSSpec inFileSpec)
{
	OSErr err = CFileMgr::FindApplication(fndrInfo.fdCreator, appSpec);
	if (err)	// Application not found error
	{
		int whatToDo = ::LaunchError(ALRT_AppNotFound, fndrInfo.fdCreator, 
									inFileSpec.name, err);
		if (whatToDo == kSave)
			CFileMgr::sFileManager.CancelRegister(inFile);		// Save the file
		else	// kDelete
			CFileMgr::sFileManager.CancelAndDelete(inFile);		// Delete the file
		return;
	}
	
	Try_	{
		LaunchParamBlockRec launchThis;
		AEDesc launchDesc;
		AppleEvent theEvent;
	
		ThrowIfOSErr_(BuildODOCEvent(fndrInfo.fdCreator, inFileSpec, theEvent));
		ThrowIfOSErr_(AECoerceDesc(&theEvent, typeAppParameters, &launchDesc));
		
		launchThis.launchAppSpec = (FSSpecPtr)&appSpec;
		launchThis.launchAppParameters = (AppParametersPtr)*(launchDesc.dataHandle);
		/* launch the thing */
		launchThis.launchBlockID = extendedBlock;
		launchThis.launchEPBLength = extendedBlockLen;
		launchThis.launchFileFlags = NULL;
		launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
		if (!IsFrontApplication())
			launchThis.launchControlFlags += launchDontSwitch;
		do		// Launch until we succeed, or user gives up.
		{
			err = LaunchApplication(&launchThis);
			if ((err == memFullErr) || (err == memFragErr))	
			// Launch failed because of low memory
			{
				int whatToDo = ::LaunchError(ALRT_AppMemFull, fndrInfo.fdCreator, 
											inFileSpec.name, err);
				switch (whatToDo)	{
				case kSave:
					CFileMgr::sFileManager.CancelRegister(inFile);		// Save the file
					err = noErr;
					break;
				case kDelete:
					CFileMgr::sFileManager.CancelAndDelete(inFile);		// Save the file
					err = noErr;
					break;
				case kTryAgain:								// Loop again
					break;
				}
			}
			else	// Unknown launch error
				ThrowIfOSErr_(err);
		} while (err != noErr);
	}
	Catch_(inErr)
	{
		int whatToDo = ::LaunchError(ALRT_AppMiscError, fndrInfo.fdCreator, 
									inFileSpec.name, inErr);
		if (whatToDo == kSave)
			CFileMgr::sFileManager.CancelRegister(inFile);		// Save the file
		else	// kDelete
			CFileMgr::sFileManager.CancelAndDelete(inFile);		// Delete the file
	} EndCatch_
}


OSErr
CreateFinderAppleEvent(		AEEventID		eventID,
							SInt16 			returnID, 
							SInt32 			transactionID,
							AppleEvent	&	theEvent)
{
	OSErr				err;
	FSSpec				finder;
	ProcessSerialNumber	psn;
	AEDesc				finderAddress;
	Boolean				validAddress = false;
	
	try
	{
		err = FindProcessBySignature('MACS', 'FNDR', psn, &finder);
		ThrowIfOSErr_(err);
 		
 		err = ::AECreateDesc(typeProcessSerialNumber, (Ptr)&psn, sizeof(psn), &finderAddress);
		ThrowIfOSErr_(err);
		validAddress = true;
		
		err = ::AECreateAppleEvent(	kAEFinderEvents, 
									eventID, 
									(const AEAddressDesc *) &finderAddress, 
									returnID, 
									transactionID, 
									&theEvent	);

	}
	catch(long tErr)
	{
		if (validAddress) 
			::AEDisposeDesc(&finderAddress);
	}

	return err;
}

// Builds an OpenSelection event for Finder
OSErr BuildOpenSelectionEvent(FSSpec & fileSpec, AppleEvent& theEvent)	{
	FSSpec dirSpec, procSpec;
	FSSpecPtr theFileToOpen = nil;
	CStr63 processName;
	AEDesc aeDirDesc, listElem;
	AEDesc fileList;
	ConstStr255Param * dummy  = NULL;
// Create the event
	OSErr err;
	Try_ {
		ProcessInfoRec pir;
		pir.processInfoLength = sizeof(ProcessInfoRec);
		pir.processName = (StringPtr)&processName;
		pir.processAppSpec = &procSpec;
	// Find a Finder, and create its description as an address for an apple event
	
		err = CreateFinderAppleEvent(kAEOpenSelection, kAutoGenerateReturnID, kAnyTransactionID, theEvent);
		ThrowIfOSErr_(err);
		
	// Create a description of the file, and the enclosing folder
	// keyDirectObject is directory description
	//
		err = CFileMgr::FolderSpecFromFolderID(fileSpec.vRefNum, fileSpec.parID, dirSpec);
		ThrowIfOSErr_(err);
		
		err = AECreateList(nil, 0, false, &fileList);
		ThrowIfOSErr_(err);

	 	AliasHandle DirAlias, FileAlias;
		NewAlias(nil, &dirSpec, &DirAlias);
		HLock((Handle)DirAlias);
		err = AECreateDesc(typeAlias, (Ptr)*DirAlias, GetHandleSize((Handle)DirAlias), &aeDirDesc);
		ThrowIfOSErr_(err);
		HUnlock((Handle)DirAlias);
		DisposeHandle((Handle)DirAlias);
		
		err = AEPutParamDesc(&theEvent, keyDirectObject, &aeDirDesc);
		ThrowIfOSErr_(err);
		AEDisposeDesc(&aeDirDesc);

		NewAlias(nil, &fileSpec, &FileAlias);
		HLock((Handle)FileAlias);
		err = AECreateDesc(typeAlias, (Ptr)*FileAlias, GetHandleSize((Handle)FileAlias), &listElem);
		ThrowIfOSErr_(err);
		HUnlock((Handle)FileAlias);
		err = AEPutDesc(&fileList, 0, &listElem);
		ThrowIfOSErr_(err);
		DisposeHandle((Handle)FileAlias);

		err = AEPutParamDesc( &theEvent, keySelection, &fileList);
		ThrowIfOSErr_(err);		
	}
	Catch_(inErr)
	{
		return inErr;
	} EndCatch_
	return noErr;
}

// Sends 'open selection event to Finder
OSErr SendOpenSelectionToFinder(FSSpec & fileSpec)
{
	AppleEvent event;
	AppleEvent result;
	OSErr err = BuildOpenSelectionEvent(fileSpec, event);
	if (err)
		return err;
	err = AESend(&event, &result, 
					kAENoReply + kAEAlwaysInteract + kAECanSwitchLayer,
					kAENormalPriority, kAEDefaultTimeout,
					NULL,NULL);
	AEDisposeDesc(&event);
	return err;
}

// A somewhat tricky way of opening a foreign document
// Algorithm: 
//	- if a process is not running, launch it with the document
//  - if a process is running and AE aware, send it an AppleEvent
//  - if a process is running and is not AE aware, send openSelection to the Finder.
void LaunchFile(LFileBufferStream * inFile)
{
	FSSpec applSpec;
	FInfo fndrInfo;
	
// Get file info
	FSSpec inFileSpec;
	inFile->GetSpecifier(inFileSpec);
	
	HGetFInfo(inFileSpec.vRefNum,
				inFileSpec.parID,
				inFileSpec.name,
				&fndrInfo);

// Find if the application is already running
	ProcessSerialNumber processSN;
	ProcessInfoRec infoRecToFill;
	Str63 processName;
	infoRecToFill.processInfoLength = sizeof(ProcessInfoRec);
	infoRecToFill.processName = (StringPtr)&processName;
	infoRecToFill.processAppSpec = &applSpec;
	OSErr err = FindProcessBySignature(fndrInfo.fdCreator, 'APPL',  processSN, &applSpec);
	
	if (err == noErr)	// App is running. Send 'odoc'
	{
		err = SendODOCEvent(fndrInfo.fdCreator, inFile);
		if (err == noErr)
		{
			if (IsFrontApplication())
				SetFrontProcess(&processSN);
		}
		else
		{
			// Application did not accept apple event for some reason (err = connectionInvalid)
			// Send 'odoc' to Finder. Finder can figure out how to fake menu events when
			// it tries to open the file
			err = SendOpenSelectionToFinder(inFileSpec);
			if (err == noErr)
			{		// If finder launched the application successfully, find it and bring it to front
				err = FindProcessBySignature(fndrInfo.fdCreator, 'APPL',  processSN, &applSpec);
				if (err == noErr && IsFrontApplication())
					SetFrontProcess(&processSN);
			}
			else	// Finder launch also failed. Notify the user
			{
				//Notify the user, try to handle the error
				int whatToDo = LaunchError(ALRT_ODOCFailed, 
											fndrInfo.fdCreator,
											 inFileSpec.name, err);
				if (whatToDo == 1)
					CFileMgr::sFileManager.CancelRegister(inFile);		// Save the file
				else
					CFileMgr::sFileManager.CancelAndDelete(inFile);		// Delete the file
			}
		}
	}
	else				// App is not running. Launch it with this file
		LaunchWithDoc(fndrInfo, applSpec, inFile, inFileSpec);
}

