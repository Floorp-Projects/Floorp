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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CCheckMailContext.cp

#include "CCheckMailContext.h"

// FE
#include "PascalString.h"

// XP
#include "prefapi.h"

extern "C" int GetBiffPrefs(const char* prefnode, void* instanceData);

//======================================
// class CCheckMailContext
//
// Context for biff, regular checking for mail.
// Should only be instantiated once.
//======================================

CCheckMailContext*	CCheckMailContext::sCheckMailContext = nil;

//-----------------------------------
CCheckMailContext* CCheckMailContext::Get()
//-----------------------------------
{
	if (!sCheckMailContext)
		new CCheckMailContext();
	return sCheckMailContext;
}

#if 0 // seems to be obsolete.
//-----------------------------------
extern "C" int GetBiffPrefs(const char* prefnode, void* /*instanceData*/)
//-----------------------------------
{
	return 0;
}
#endif // 0

//-----------------------------------
CCheckMailContext::CCheckMailContext()
//-----------------------------------
:	CMailNewsContext(MWContextBiff)
,	mCheckMailState(MSG_BIFF_Unknown)
,	mOutstandingNotification(false)
,	mMailNotifyIcon(nil)
{
	// should only create one
	Assert_(sCheckMailContext == nil);
	sCheckMailContext = this;
	memset(&mNotification, 0, sizeof(NMRec));
}	

//-----------------------------------
/* static */ void CCheckMailContext::Initialize(void* inUser)	
//-----------------------------------
{
	Boolean firstUser = (sCheckMailContext == nil);
	CCheckMailContext* biff = CCheckMailContext::Get();
	biff->AddUser(inUser); // must do before initialize call;
	Assert_(sCheckMailContext);
	if (firstUser)
		MSG_BiffInit(*sCheckMailContext, MSG_GetPrefsForMaster(GetMailMaster()));
#if 0 // seems to be obsolete.
	GetBiffPrefs(nil, nil);
	PREF_RegisterCallback("mail.check_new_mail", GetBiffPrefs, this);
	PREF_RegisterCallback("mail.check_time", GetBiffPrefs, this);
#endif // 0
} // CCheckMailContext::Initialize

//-----------------------------------
/* static */ void CCheckMailContext::Release(void* inUser)	
//-----------------------------------
{
	if (sCheckMailContext)
		sCheckMailContext->RemoveUser(inUser); // and delete;
} // CCheckMailContext::Release

//-----------------------------------
CCheckMailContext::~CCheckMailContext()	
//-----------------------------------
{
	Assert_(sCheckMailContext == this);
	MSG_BiffCleanupContext(*this);
	sCheckMailContext = nil;
	RemoveNotification();
}	
	
#if 0
//-----------------------------------
void CCheckMailContext::SetState(MSG_BIFF_STATE state)
// Sets biff state and does all the ui fun to reflect the change
//-----------------------------------
{
	Assert_(sCheckMailContext != nil);
	sCheckMailContext->DoSetState(state);
}
#endif

//-----------------------------------
void CCheckMailContext::SuspendResume()
// End Mac notification of new mail, called on suspend/resume
//
// Actually "resets" notification in for the case
// where we've become the front process having
// posted a notification when not the front process.
//
// This allows us to move the blinking icon from the
// process menu to the apple menu when we become frontmost.

// NOT ANY MORE!!!
// This logic breaks when you want Dogbert to beep on new mail when
// it's in the background _or_ foreground.  This logic also breaks
// when trying to move the blinking icon to the process menu on a suspend
// since we are still the foreground app.
//-----------------------------------
{
#if 0
	if (sCheckMailContext)	
		sCheckMailContext->DoSuspendResume();
#endif
}

#if 0
//-----------------------------------
void CCheckMailContext::DoSuspendResume()
//-----------------------------------
{
	RemoveNotification();
	if (GetState() == MSG_BIFF_NewMail)
		InstallNotification();
}
#endif

//-----------------------------------
void CCheckMailContext::SetState(MSG_BIFF_STATE state)
// Sets biff state and does all the ui fun to reflect the change
//-----------------------------------
{
	if (state != GetState())
	{
		mCheckMailState = state;
		BroadcastMessage(msg_MailNotificationState, &state);
		
		// Flash notification icon
		if (state == MSG_BIFF_NewMail)
			InstallNotification();
		else
			RemoveNotification();
	}
}

//-----------------------------------
void CCheckMailContext::RemoveNotification()
//-----------------------------------
{
	if (mOutstandingNotification)
	{
		::NMRemove(&mNotification);
		mOutstandingNotification = false;		
	}
	if (mNotification.nmSound)
	{
		::DisposeHandle(mNotification.nmSound);
		mNotification.nmSound = nil;
	}
}

//-----------------------------------
void CCheckMailContext::InstallNotification()
// If we're not the front process, blink our icon in the process menu and beep.
// If we are the front process, blink our icon in the apple menu (no beep)
//-----------------------------------
{
	if (!mOutstandingNotification)
	{
		ProcessSerialNumber	front, current;		
		::GetCurrentProcess(&current);
		::GetFrontProcess(&front);		
		Boolean	mozillaIsInFront;
		OSErr err = ::SameProcess(&current, &front, &mozillaIsInFront);
		
		if (err == noErr)
		{
			if (mMailNotifyIcon == nil)
				err = ::GetIconSuite(&mMailNotifyIcon, BIFF_NOTIFY_ICONSUITE, kSelectorSmall8Bit | kSelectorSmall1Bit);
			
			if (err == noErr && mMailNotifyIcon != nil)
			{
				Handle soundToPlay = nil; // using nil will play no sound.
//				if (!mozillaIsInFront) Always play sound if one specified
				{
					CStr31 soundName; // constructor initializes to empty.
#define USING_PREF_SOUND 1
#if USING_PREF_SOUND
					char buffer[256];
					int charsReturned = sizeof(buffer);
					PREF_GetCharPref("mail.notification.sound", buffer, &charsReturned);
					if (strlen(buffer) < sizeof(soundName))
						soundName = buffer; // (PascalString takes care of conversion)
#else
					soundName = "Quack";
#endif
					soundToPlay = ::GetNamedResource('snd ', soundName);
					if (soundToPlay)
					{
						::HNoPurge(soundToPlay);
						::DetachResource(soundToPlay);
					}
				}
				// set up the notification manager record
				mNotification.qType		= nmType;
				mNotification.nmMark	= (mozillaIsInFront) ? 0 : 1;	// if we're the front process, blink in apple menu, else in process menu
				mNotification.nmIcon	= mMailNotifyIcon;
				mNotification.nmSound	= soundToPlay;					// can be nil
				mNotification.nmStr		= nil;							// no dialog
				mNotification.nmResp	= nil;							// no callback
				
				if ( ::NMInstall(&mNotification) == noErr)
					mOutstandingNotification = true;
			}
		}
	}
}



	
