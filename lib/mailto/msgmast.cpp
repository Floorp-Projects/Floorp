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

#define FORCE_PR_LOG /* Allow logging in the release build */

#include "rosetta.h"
#include "msg.h"
#include "errcode.h"

#include "msgmast.h"
#include "msgprefs.h" 
#include "prtime.h"
#include "prefapi.h"
#include "rosetta.h"
#include HG99877
#include "msgurlq.h"
#include "xpgetstr.h"
#include "prlog.h"
#include "nslocks.h"
#include "pw_public.h"

extern "C" {
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_SET_HTML_NEWSGROUP_HEIRARCHY_CONFIRM;
	extern int MK_MSG_FOLDER_ALREADY_EXISTS;
	extern int MK_MSG_INBOX_L10N_NAME;
	extern int MK_IMAP_UPGRADE_WAIT_WHILE_UPGRADE;
	extern int MK_IMAP_UPGRADE_PROMPT_QUESTION;
	extern int MK_IMAP_UPGRADE_CUSTOM;
	extern int MK_POP3_USERNAME_UNDEFINED;
	extern int XP_PASSWORD_FOR_POP3_USER;
	extern int XP_MSG_CACHED_PASSWORD_NOT_MATCHED;
	extern int XP_MSG_PASSWORD_FAILED;
	extern int MK_POP3_PASSWORD_UNDEFINED;
}

PRLogModuleInfo *IMAP;

MSG_Master::MSG_Master(MSG_Prefs* prefs)
{
	XP_Bool purgeBodiesByAge;
	int32		purgeMethod;
	int32		daysToKeepHdrs;
	int32		headersToKeep;
	int32		daysToKeepBodies;

	m_prefs = prefs;
	m_prefs->AddNotify(this);
	

	IMAP = PR_NewLogModule("IMAP");
	// on the mac, use this java script preference
	// as an alternate to setenv
	XP_Bool imapIOlogging;
	PREF_GetBoolPref("imap.io.mac.logging",	    &imapIOlogging); 
	if (imapIOlogging)
		IMAP->level = PR_LOG_ALWAYS;

}

MSG_Master::~MSG_Master()
{
	m_prefs->RemoveNotify(this);
}


void
MSG_Master::SetFirstPane(MSG_Pane* pane)
{
	m_firstpane = pane;
}


MSG_Pane*
MSG_Master::GetFirstPane()
{
	return m_firstpane;
}

void MSG_Master::NotifyPrefsChange(NotifyCode )
{
}
