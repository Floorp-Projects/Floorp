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

//Functions to check date and version info, and determine if Beta release is expired.

#include "pluginIncludes.h"
#include <Files.h>
#include "ExpireBeta.h"
#include <OSUtils.h>
#include <String.h>

extern	short		pluginResFile;

//Gets the date of the file whose reference number is passed in.  This is meant to be for the account setup file
unsigned long GetFileCreationDate(short refNum)
{
        FCBPBRec        myFCB;
        HParamBlockRec myHBR;
        OSErr   myErr;
        //StringHandle myFileName;
        Str32   name;

	//Whoops, the following line won't work if we last looked at a different res fork.
  	//short refNum = CurResFile();

        myFCB.ioFCBIndx = 0;
        myFCB.ioVRefNum = 0;
        myFCB.ioNamePtr = name;
        myFCB.ioRefNum = refNum;
        myFCB.ioCompletion = nil;

        myErr = PBGetFCBInfoSync(&myFCB);

        /*if (myErr != 0) {
                ErrorAlert(5005, myErr);
                return false;
        }*/

        if (myErr == noErr) {

                if (myFCB.ioNamePtr != nil)  {
                        myHBR.fileParam.ioNamePtr = myFCB.ioNamePtr;
                        myHBR.fileParam.ioVRefNum = myFCB.ioFCBVRefNum;
                        myHBR.fileParam.ioDirID  = myFCB.ioFCBParID;
                        myHBR.fileParam.ioFDirIndex     = 0;    // use the dir & vRefNum;

                        myErr = PBHGetFInfoSync(&myHBR);

                        if (myErr == noErr) {
                                return (myHBR.fileParam.ioFlCrDat);
                        }
                        else
                                return (-1);
                }
        }
        return -1;
}



	//Beta Expire Alert code.
	//Added 8/16/95 by A. Tayyeb
	// Checks the 'vers' resource to see if this is anything but a release version, if not, 
	//and if current date is at least 2 weeks more than the creation date of the app, exists the application
	//with a stopalert
Boolean CheckIfExpired(void)
{
		
		//LOCALS
		Handle	hVersResourceHandle;
		char		pDevStageByte;
		unsigned long CreationDate, CurrDate;
		int		AcctSetupFileRefNum;
		StringHandle stringDaysToExpire; 
		long daysToExpire, secsToExpire; 

		short origFile = CurResFile();

		//Open the resource, get the version info and creation date
		UseResFile(pluginResFile);
		hVersResourceHandle	=	(char **)GetResource('vers', 1);
		HLock(hVersResourceHandle);
		memcpy(&pDevStageByte,((char *)*hVersResourceHandle+2),1);
		HUnlock(hVersResourceHandle);
		
		//We identify the home res file because it will be the one containing the vers resource we find.
		//Therefore, we need this call to get the file reference number for Account Setup.  We can then pass 
		//GetFileCreationDate a good number.
		AcctSetupFileRefNum=HomeResFile(hVersResourceHandle);
		ReleaseResource(hVersResourceHandle);
		CreationDate = GetFileCreationDate(AcctSetupFileRefNum);

		//Current date
		GetDateTime(&CurrDate);
	
		//Now get the resource which tells us how long till expiration
		//UseResFile(pluginResFile);
		stringDaysToExpire = (unsigned char **)GetString(NumDaysStrID); 

		//Find seconds till expiration, if no resource was found, assume 0 secs (immediate expiration)		
		if (stringDaysToExpire != nil)
			{
				StringToNum(*stringDaysToExpire, &daysToExpire); 
				secsToExpire = (daysToExpire * 24 * 60 * 60);
			} 
		else
			{
				daysToExpire = 0; secsToExpire = 0;
			}
			
			UseResFile(origFile);
			
		//The real test - if the date is past expiration, and this is not a release version, return true.
		if ((CurrDate > (CreationDate+secsToExpire)) && !(pDevStageByte & kVersRelease))
			{
				return kIsExpired;
			}
		
		return kIsNotExpired;
}
