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

// ULocationIndependence.cp - MacFE specific location independence code

#include "uapp.h"
#include "li_public.h"
#include "pprthred.h"
#include "plevent.h"
#include <LCommander.h>

// CLICommander
// Location independence commands
class CLICommander : public LCommander, public LEventDispatcher	{
	enum EQuitState	{
		eWaitingForUploadAll,
		eUploadAllStarted,
		eUploadAllComplete
	};
	
public:
	int				fVerifyLoginCount;
	int 			fNumOfCriticalFiles;
	EQuitState		fState;

// Constructors
	CLICommander(LCommander * inSuper);
	virtual ~CLICommander() {};

// Startup logic
	static void GetCriticalClosure(void * closure,  LIStatus result);
	void GetCriticalFiles();
	void VerifyLogin();

// Quit logic
	virtual Boolean	AttemptQuit(long inSaveOption);
private:
	static void VerifyLoginCallback(void * closure, LIStatus result);
	static void uploadAllClosure( void * closure, LIStatus result);

	void UploadAllComplete(LIStatus status);
	
	// Does a busy wait, while processing events
	void WaitOnInt(int * boolean);
};


CLICommander::CLICommander(LCommander * inSuper) 
	: LCommander(inSuper) 
{
	fState = eWaitingForUploadAll;
	
/*
	LIFile * file1 = new LIFile ("/Speedy/coreprofile/file1", "FirstFile", "Test file 1");
	LIFile * file2 = new LIFile ("/Speedy/coreprofile/file2", "SecondFile", "Test file 2");
	LIFile * file3 = new LIFile ("/Speedy/coreprofile/file3", "ThirdFile", "Test file 3");
	fGroup = new LIClientGroup();
	fGroup->addFile( file1, NULL, NULL, NULL, NULL);
	fGroup->addFile( file2, NULL, NULL, NULL, NULL);
	fGroup->addFile( file3, NULL, NULL, NULL, NULL);
*/
}

extern PREventQueue *mozilla_event_queue;

void CLICommander::GetCriticalFiles()
{
	fNumOfCriticalFiles = 0;
	LI_StartGettingCriticalFiles( &fNumOfCriticalFiles );
	WaitOnInt(&fNumOfCriticalFiles);
}

void CLICommander::VerifyLoginCallback(void * closure, LIStatus result)
{
	CLICommander * commander = (CLICommander*) closure;
	commander->fVerifyLoginCount = 0;
}

void CLICommander::VerifyLogin()
{
	fVerifyLoginCount = 1;
	LIMediator::verifyLogin(VerifyLoginCallback, this);
	WaitOnInt(&fVerifyLoginCount);
}

void CLICommander::WaitOnInt(int * waitInt)
{
	EventRecord macEvent;
	
	while (*waitInt > 0)
	{
		if (IsOnDuty()) {
			::OSEventAvail(0, &macEvent);
			AdjustCursor(macEvent);
		}

		SetUpdateCommandStatus(false);

		Boolean gotEvent = ::WaitNextEvent(everyEvent, &macEvent,
											1, mMouseRgnH);
		
			// Let Attachments process the event. Continue with normal
			// event dispatching unless suppressed by an Attachment.
		
		if (LEventDispatcher::ExecuteAttachments(msg_Event, &macEvent)) {
			if (gotEvent) {
				DispatchEvent(macEvent);
			} else {
				UseIdleTime(macEvent);
			}
		}

										// Repeaters get time after every event
		LPeriodical::DevoteTimeToRepeaters(macEvent);
		
										// Update status of menu items
		if (IsOnDuty() && GetUpdateCommandStatus()) {
			UpdateMenus();
		}
		// This pumps the mocha thread
		PL_ProcessPendingEvents(mozilla_event_queue);
	}
	// We need to give time to idlers once again because:
	// HTML conflict dialogs are being destroyed on a timer by libsec
	// CreateStartupEnvironment() will not display any new windows if any HTML windows are visible
	// => Must let timers run to destroy HTML dialogs before we proceed
	macEvent.when = ::TickCount();
	LPeriodical::DevoteTimeToIdlers(macEvent);
}


void CLICommander::uploadAllClosure( void * closure, LIStatus status)
{
	CLICommander * c = (CLICommander*) closure;
	c->UploadAllComplete(status);
}

void CLICommander::UploadAllComplete(LIStatus status)
{
	fState = eUploadAllComplete;
	LI_Shutdown();
	(CFrontApp::GetApplication())->DoQuit();
}

Boolean	CLICommander::AttemptQuit(long	inSaveOption)
{
	switch (fState)
	{
		case eWaitingForUploadAll:
			LIMediator::uploadAll(uploadAllClosure, this);
			break;

		case eUploadAllStarted:
			return false;
			break;

		case eUploadAllComplete:
			return true;	// We can quit now
	}
	return false;
}

#ifndef MOZ_LITE
// InitializeLocationIndependence
// Busy wait until all the Critical Files have been downloaded
void CFrontApp::InitializeLocationIndependence()
{
//	LI_Startup();
//	CLICommander * newCommander = new CLICommander(this);
//	newCommander->VerifyLogin();
//	newCommander->GetCriticalFiles();	
}
#endif
