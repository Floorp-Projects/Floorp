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



// CMailNewsContext.h

#pragma once

// XP
#include "msgcom.h"

// MacFE

#include "CNSContext.h"

//======================================
class CMailNewsContext
//======================================
:	public CNSContext
{
private:
	typedef CNSContext	Inherited;
public:
	
						CMailNewsContext(MWContextType inContextType = MWContextMail);
	virtual				~CMailNewsContext();

// Overrides
public:
//	virtual void		DoProgress(const char* message, int level);	// 1 called by netlib
//	virtual void		DoSetProgressBarPercent(int32 percent);
	
// Safe and guaranteed access
protected:
	static MSG_Prefs*	GetMailPrefs();	
public:
	static MSG_Master*	GetMailMaster();

	// This is for offline/online support.
	// If mail/news isn't up and running, we shouldn't call GetMailMaster.
	// This allows us to determine if we've already allocated the MSG_Master
	static Boolean		HaveMailMaster()
						{ return sMailMaster != NULL; }
	static Boolean		UserHasNoLocalInbox();

	// Prompt callback support
	virtual char*		PromptWithCaption(
							const char*				inTitleBarText,
							const char* 			inMessage,
							const char*				inDefaultText);
	virtual void		SwitchLoadURL(
							URL_Struct*				inURL,
							FO_Present_Types		inOutputFormat);
	virtual void		AllConnectionsComplete();
	Boolean				GetWasPromptOkay() { return mWasPromptOkay; }
	// LSharable
	virtual void		NoMoreUsers();
	static void			ThrowUnlessPrefsSet(MWContextType inContextType);
	static void 		AlertPrefAndThrow(PREF_Enum inPrefPaneSelector);
	static void			ThrowIfNoLocalInbox();

protected:
	static Boolean		IsPrefSet(const char* inPrefKey);
	static Boolean		ThrowUnlessPrefSet(
							const char* inPrefKey,
							PREF_Enum inPrefPaneSelector);

// Data
private:
	static MSG_Prefs*	sMailPrefs;
	static MSG_Master*	sMailMaster;
	static Int32		sMailMasterRefCount;
	Boolean 			mWasPromptOkay;
}; // class CMailNewsContext
