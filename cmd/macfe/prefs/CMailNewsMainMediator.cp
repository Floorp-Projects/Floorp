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

#ifndef MOZ_LITE

#include "CMailNewsMainMediator.h"

#include "xp_help.h"
#include "prefapi.h"
#include "macgui.h"

#include "PascalString.h"
#include "StSetBroadcasting.h"
#include <LGACheckbox.h>

enum
{
	eQuoteStylePopup				= 12601
,	eQuoteSizePopup
,	eQuoteColorButton
,	eFixedRButton
,	eVariableRButton

,	eLocalMailDirFilePicker			= 12910
,	eCheckForMailBox				= 12912
,	eCheckForMailIntervalEditField	= 12913
,	eNotificatonSoundPopup			= 12914
,	eRememberPasswordBox			= 12915
};

//-----------------------------------
CMailNewsMainMediator::CMailNewsMainMediator(LStream*)
//-----------------------------------
:	CPrefsMediator(class_ID)
{
}

//-----------------------------------
CMailNewsMainMediator::~CMailNewsMainMediator()
//-----------------------------------
{
}

//-----------------------------------
void CMailNewsMainMediator::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
		case eNotificatonSoundPopup:
			LGAPopup *soundMenu =
					(LGAPopup *)FindPaneByID(eNotificatonSoundPopup);
			int	menuItem = soundMenu->GetValue();
			MenuHandle	soundMenuH = soundMenu->GetMacMenuH();
			CStr255	stringResName;
			if (menuItem > 2)
				GetMenuItemText(soundMenuH, menuItem, stringResName);
			if (stringResName[0])
			{
				SndListHandle soundH =
					(SndListHandle)GetNamedResource('snd ', stringResName);
				if (soundH)
				{
					::DetachResource((Handle)soundH);
					SndPlay(nil, soundH, false);
					DisposeHandle((Handle)soundH);
				}
			}
			break;
		default:
			CPrefsMediator::ListenToMessage(inMessage, ioParam);
			break;
	}
}

//-----------------------------------
void CMailNewsMainMediator::WritePrefs()
//-----------------------------------
{
	if (!PaneHasLockedPref(eNotificatonSoundPopup) && !PREF_PrefIsLocked("mail.play_sound"))
	{
		char* soundName = nil;
		LControl* popup = (LControl*)FindPaneByID(eNotificatonSoundPopup);
		XP_Bool	playSound = popup->GetValue() != 0;
		PREF_SetBoolPref("mail.play_sound", playSound);
	}
} // CMailNewsMainMediator::WritePrefs

//-----------------------------------
Boolean CMailNewsMainMediator::BiffIntervalValidationFunc(CValidEditField *biffInterval)
//-----------------------------------
{
	// If the checkbox isn't set then this value is really
	// ignored, so I will only put up the alert if the checkbox
	// is set, but I will force a valid value in any case.

	Boolean		result = true;

	// force valid value
	if (1 > biffInterval->GetValue())
	{
		int32	newInterval = 10;
		PREF_GetDefaultIntPref("mail.check_time", &newInterval);
		biffInterval->SetValue(newInterval);
		biffInterval->SelectAll();
		result = false;
	}
	if (!result)	// if the value is within the range, who cares
	{
		// Check for the check box...
		// We are assuming that the checkbox is a sub of the field's superview.
		LView	*superView = biffInterval->GetSuperView();
		XP_ASSERT(superView);
		LGACheckbox	*checkbox =
				(LGACheckbox *)superView->FindPaneByID(eCheckForMailBox);
		XP_ASSERT(checkbox);
		if (checkbox->GetValue())
		{
			StPrepareForDialog	prepare;
			::StopAlert(1067, NULL);
		}
		else
		{
			result = true;	// go ahead and let them switch (after correcting the value)
							// if the checkbox isn't set
		}
	}
	return result;
}

#endif // MOZ_LITE

