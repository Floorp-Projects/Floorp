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

//	UProcessUtils.cp


#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include <LString.h>
#include "UProcessUtils.h"

#ifndef _STRING
#include <string.h>
#endif

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LocateApplication
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean UProcessUtils::LocateApplication(OSType inSignature, FSSpec &outSpec)
{
	Boolean bAppFound = false;
	FSSpec theMostRecentSpec;
	Uint32 theMostRecentDate = 0;
	TString<Str63> theAppName;
	Int16 theIndex = 0;
	
	HParamBlockRec hpb;

	do {
		DTPBRec dpb;
		CInfoPBRec cpb;

		memset(&hpb, 0, sizeof(HParamBlockRec));
		memset(&dpb, 0, sizeof(DTPBRec));

		hpb.volumeParam.ioVolIndex = ++theIndex;
		if (PBHGetVInfoSync(&hpb) != noErr)
			break;

		dpb.ioVRefNum = hpb.volumeParam.ioVRefNum;
		if (PBDTGetPath(&dpb) == noErr)
			{
			dpb.ioFileCreator = inSignature;
			dpb.ioNamePtr = theAppName;

			if (PBDTGetAPPLSync(&dpb) == noErr)
				{
				memset(&cpb, 0, sizeof(CInfoPBRec));
				cpb.hFileInfo.ioNamePtr = theAppName;
				cpb.hFileInfo.ioVRefNum = dpb.ioVRefNum;
				cpb.hFileInfo.ioDirID = dpb.ioAPPLParID;

				if ((PBGetCatInfoSync(&cpb) == noErr) && (cpb.hFileInfo.ioFlMdDat > theMostRecentDate))
					{
					theMostRecentSpec.vRefNum = dpb.ioVRefNum;
					theMostRecentSpec.parID = dpb.ioAPPLParID;
					::BlockMoveData(&theAppName[0], theMostRecentSpec.name, theAppName.Length() + 1);
					theMostRecentDate = cpb.hFileInfo.ioFlMdDat;
					}
					
				bAppFound = true;
				}
			}
		}
	while (!hpb.volumeParam.ioResult);

	if (bAppFound)
		outSpec = theMostRecentSpec;

	return bAppFound;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	ApplicationRunning
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean UProcessUtils::ApplicationRunning(
	OSType 				inSignature,
	ProcessSerialNumber &outPSN)
{
	Boolean bRunning = false;
	//	spin through the process list to determine whether there's a
	//	process with our desired signature.
	outPSN.highLongOfPSN = 0;
	outPSN.lowLongOfPSN = 0;

	ProcessInfoRec info;	//	information from the process list.
	info.processInfoLength = sizeof(ProcessInfoRec);
	info.processName = nil;
	info.processAppSpec = nil;

	while ((GetNextProcess(&outPSN) == noErr) && (GetProcessInformation(&outPSN, &info) == noErr))
		{
		if (info.processSignature == inSignature)
			{
			bRunning = true;
			break;
			}
		}

	return bRunning;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	LaunchApplication
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

OSErr UProcessUtils::LaunchApplication(
	FSSpec& 				inSpec,
	Uint16					inLaunchFlags,
	ProcessSerialNumber& 	outPSN)
{
	LaunchParamBlockRec lpb;

	lpb.launchBlockID = extendedBlock;
	lpb.launchEPBLength = extendedBlockLen;
	lpb.launchFileFlags = 0;
	lpb.launchControlFlags = inLaunchFlags;
	lpb.launchAppSpec = &inSpec;
	lpb.launchAppParameters = 0L;

	OSErr theResult = ::LaunchApplication(&lpb);
	outPSN = lpb.launchProcessSN;
	return theResult;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	PullAppToFront
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void UProcessUtils::PullAppToFront(const ProcessSerialNumber& inWhichProcess)
{
	EventRecord scratch;

	//	SetFrontProcess(), then call EventAvail so that it actually happens.
	::SetFrontProcess(&inWhichProcess);
	::EventAvail(nullEvent, &scratch);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	IsFrontProcess
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean UProcessUtils::IsFrontProcess(const ProcessSerialNumber& inWhichProcess)
{
	ProcessSerialNumber theFrontProcess;
	::GetFrontProcess(&theFrontProcess);

	Boolean isInFront;
	::SameProcess(&theFrontProcess, &inWhichProcess, &isInFront);

	return isInFront;
}

