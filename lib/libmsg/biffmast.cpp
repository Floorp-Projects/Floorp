/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
	Implementation file for the Biff Master (check to see if there is mail) for multiple
	servers. rb
*/

#include "msg.h"
#include "msgfpane.h"
#include "msgprefs.h"
#include "msgfinfo.h"
#include "imaphost.h"
#include "msgimap.h"
#include "msgsend.h"
#include "msgmast.h"
#include "client.h"
#include "imap.h"
#include "prefapi.h"
#include "msgbiff.h"
#include "pw_public.h"




/********************************************************************/


/* OBSOLETE WRAPPER STUFF */

extern "C" void MSG_RegisterBiffCallback( MSG_BIFF_CALLBACK /*cb*/ )
{
	XP_ASSERT(FALSE);
}
extern "C" void MSG_UnregisterBiffCallback()
{
	XP_ASSERT(FALSE);
}
/* Set the preference of how often to run biff.  If zero is passed in, then
   never check. */
extern "C" void MSG_SetBiffInterval(int32 /*seconds*/)
{
	XP_ASSERT(FALSE);
}
#ifdef XP_UNIX
/* Set the file to stat, instead of using pop3.  This is for the Unix movemail
   nonsense.  If the filename is NULL (the default), then use pop3. */
extern "C" void MSG_SetBiffStatFile(const char* filename)
{
//	XP_ASSERT(FALSE);
}
#endif
/* Causes a biff check to occur immediately.  This gets caused
   automatically by MSG_SetBiffInterval or whenever libmsg gets new mail. */
extern "C" void MSG_BiffCheckNow(MWContext* /*context*/)
{
	XP_ASSERT(FALSE);
}


/* END WRAPPER LEGACY STUFF */



/****************   T H E   B I F F     M A S T E R    *************************/

static MSG_Biff_Master *gBiffMaster = NULL;

extern "C" void MSG_SetBiffStateAndUpdateFE(MSG_BIFF_STATE newState)
{
	if (gBiffMaster)
		gBiffMaster->SetNikiBiffState(newState);
}


void MSG_Biff_Master::SetNikiBiffState(MSG_BIFF_STATE state)
{
	if (state != m_state) {
		FE_UpdateBiff(state);
		// if (m_biffcallback)
		//	(*m_biffcallback)(state, m_state);
		m_state = state;
	}
}


/* Get and set the current biff state */
extern "C" MSG_BIFF_STATE MSG_GetBiffState()
{
	if (gBiffMaster)
		return (gBiffMaster->GetNikiBiffState());
	return MSG_BIFF_NoMail;
}


MSG_BIFF_STATE MSG_Biff_Master::GetNikiBiffState(void)
{
	return m_state;
}


extern "C" XP_Bool MSG_Biff_Master_NikiCallingGetNewMail()
{
	return MSG_Biff_Master::NikiCallingGetNewMail();
}


/* static */
XP_Bool MSG_Biff_Master::NikiCallingGetNewMail(void)
{
	if (gBiffMaster)
		return gBiffMaster->NikiBusy();
	return FALSE;
}


XP_Bool MSG_Biff_Master::NikiBusy(void)
{
	return m_nikiBusy;
}

/* static */
MSG_Pane* MSG_Biff_Master::GetPane(void)
{
	if (gBiffMaster)
		return gBiffMaster->m_nikiPane;
	return 0L;
}


void MSG_Biff_Master::PrefCalls(const char* pref, void* closure)
{
	int32 serverType = 0;

	if (m_biff)
		m_biff->PrefsChanged(pref, closure);
	PREF_GetIntPref("mail.server_type", &serverType);
	if (serverType != m_serverType)
	{
		m_serverType = serverType;		// User switched from POP to IMAP or vice-versa
	}
	FindNextBiffer();	// someone may be enabled now
}


/* static */
void MSG_Biff_Master::TimerCall(void* /*closure*/)
{
	if (gBiffMaster)
		gBiffMaster->TimeIsUp();
}


int PR_CALLBACK
MSG_Biff_Master::PrefsChanged(const char* pref, void* closure)
{
	if (gBiffMaster)
		gBiffMaster->PrefCalls(pref, closure);
	return 0;
}

// show progress information to the user, but if the context we are being given is
// our own, then find another one visible to the user and that has to do with mail,
// since the biff context is not visible.

extern "C"
void MSG_Biff_Master_FE_Progress(MWContext *context, char *msg)
{
	MWContext *msgContext = NULL;
	
	if (gBiffMaster)
		msgContext = gBiffMaster->GetContext();
	if (context == msgContext)
		msgContext = gBiffMaster->FindMailContext();
	else
		msgContext = context;
	FE_Progress(msgContext, msg);
}


MWContext* MSG_Biff_Master::FindMailContext(void)
{
	MWContext *context = m_context;
	MSG_Pane *mailPane = NULL;

	if (!m_master)
		return context;
	mailPane = m_master->FindFirstPaneOfType(MSG_FOLDERPANE);
	if (!mailPane)
		mailPane = m_master->FindFirstPaneOfType(MSG_THREADPANE);
	if (mailPane)
		context = mailPane->GetContext();
	return context;
}

MWContext *MSG_GetBiffContext()
{
	return (gBiffMaster) ? gBiffMaster->GetContext() : 0;
}

MWContext* MSG_Biff_Master::GetContext(void)
{
	return m_context;
}


extern "C" int
MSG_BiffCleanupContext(MWContext* context)
{
	if (gBiffMaster)
	{
		MWContext *ct = context;
		
		if (!ct)
			ct = gBiffMaster->GetContext();
		PREF_UnregisterCallback("mail.", MSG_Biff_Master::PrefsChanged, ct);
		delete gBiffMaster;
		gBiffMaster = 0;
	}
	return 0;
}



extern "C" int
MSG_BiffInit(MWContext* context, MSG_Prefs* prefs)
{
	MSG_Biff_Master::Init_Biff_Master(context, prefs);
	return 0;
}

/* static */
void MSG_Biff_Master::AddBiffFolder(char *name, XP_Bool checkMail, int interval, XP_Bool enabled, char *host)
{
	if (gBiffMaster)
		gBiffMaster->AddFolder(name, checkMail, interval, enabled, host);
}

/* static */
void MSG_Biff_Master::RemoveBiffFolder(char *name)
{
	if (gBiffMaster)
		gBiffMaster->RemoveFolder(name);
}

void MSG_Biff_Master::RemoveFolder(char *name)
{
	MSG_NikiBiff *biff = m_biff, *prevBiff = NULL;

	while (biff)
	{
		if (XP_STRCMP(biff->GetName(), name) == 0)
		{
			if (!prevBiff)
				m_biff = biff->RemoveSelf();
			else
				prevBiff->SetNext(biff->RemoveSelf());
			return;
		}
		prevBiff = biff;
		biff = biff->GetNext();
	}
}


// When we add a folder, see if we have a folder with no name, which would indicate it was
// meant to check for POP3 mail until the user opens the mail component, so re-use that folder
// with the real settings from the user now.

void MSG_Biff_Master::AddFolder(char *name, XP_Bool checkMail, int interval, XP_Bool enabled, char *host)
{
	MSG_NikiBiff *biff = NULL;

	if (m_biff)
	{
		if (m_biff->GetName() == NULL)
			m_biff = m_biff->RemoveSelf();
	}
	biff = new MSG_NikiBiff();
	if (!biff)
		return;
	biff->SetFolderName(name);
	biff->SetInterval(interval);
	biff->SetCheckFlag(checkMail);
	biff->SetEnabled(enabled);
	biff->SetPrefs(m_prefs);
	biff->SetHostName(host);
	biff->SetContext(m_context);
	if (m_biff)
		biff->SetNext(m_biff);
	m_biff = biff;
	FindNextBiffer();	// start our checks if enabled
}


/* static */
void MSG_Biff_Master::Init_Biff_Master(MWContext* context, MSG_Prefs* prefs)
{
	if (!gBiffMaster)
		gBiffMaster = new MSG_Biff_Master();
	if (gBiffMaster)
		gBiffMaster->Init(context, prefs);
}

// Our timer has expired, so call whoever was next and then calculate once
// more who will be next

void MSG_Biff_Master::TimeIsUp()
{
	//if (m_timer)
	//	FE_ClearTimeout(m_timer);	// already cleared by timer API
	m_nikiBusy = TRUE;
	m_timer = NULL;
	if (m_currentBiff)
		m_currentBiff->MailCheck();
	m_nikiBusy = FALSE;
	FindNextBiffer();
}

MSG_Biff_Master::~MSG_Biff_Master()
{
	MSG_NikiBiff *biff;
	
	if (m_timer)
		FE_ClearTimeout(m_timer);
	biff = m_biff;
	while (biff)
		biff = biff->RemoveSelf();
	if (m_nikiContext)
	{
		PW_AssociateWindowWithContext(m_nikiContext, 0L);	// The FE's destroy our windows
		PW_DestroyProgressContext(m_nikiContext);
	}
	m_nikiContext = NULL;
}



MSG_Biff_Master::MSG_Biff_Master()
{
	m_context = m_nikiContext = NULL;
	m_nikiPane = NULL;
	m_prefs = NULL;
	m_biff = m_currentBiff = NULL;
	m_timer = NULL;
	m_state = MSG_BIFF_Unknown;
	m_nikiBusy = FALSE;
	m_master = NULL;
	FE_UpdateBiff(m_state);
}


// Init our stuff, if the user i using POP3 and they have the options for check new mail
// and remember password, then make a fake niki biff object to check for mail. We will
// kill it if they open mail.

void MSG_Biff_Master::Init(MWContext* context, MSG_Prefs* prefs)
{
	int32 serverType = 0;
	
	m_context = context;
	m_prefs = prefs;
	PREF_RegisterCallback("mail.", MSG_Biff_Master::PrefsChanged, m_context);
	if (m_prefs)
	{
		m_master = m_prefs->GetMasterForBiff();
		if (!m_master)
			m_master = MSG_InitializeMail(m_prefs);
		PREF_GetIntPref("mail.server_type", &serverType);
		m_serverType = serverType;
		if (m_serverType == 0)	// see if they want to check mail, create fake folder for biffing POP3
		{
			int32 interval = 0;
			XP_Bool checkMail = FALSE, enabled = FALSE;

			PREF_GetIntPref("mail.check_time", &interval);
			PREF_GetBoolPref("mail.check_new_mail", &checkMail);
			PREF_GetBoolPref("mail.remember_password", &enabled);
			AddFolder(NULL, checkMail, interval, enabled, NULL);
		}
	}
	if (!m_nikiContext)
	{
		XP_Bool show = FALSE;

		PREF_GetBoolPref("mail.progress_pane", &show);
		if (show)
		{
			m_nikiContext = PW_CreateProgressContext();
			if (m_nikiContext)
				m_nikiPane = MSG_CreateProgressPane (m_context, m_prefs->GetMasterForBiff(), NULL);
		}
	}
}


/* static */
void MSG_Biff_Master::MailCheckEnable(char *name, XP_Bool enable)
{
	if (gBiffMaster)
	{
		gBiffMaster->FindAndEnable(name, enable);
		gBiffMaster->FindNextBiffer();
	}
}


void MSG_Biff_Master::FindAndEnable(char *name, XP_Bool enable)
{
	MSG_NikiBiff *biff = m_biff;

	while (biff)
	{
		if (XP_STRCMP(biff->GetName(), name) == 0)
		{
			biff->SetEnabled(enable);
			return;
		}
		biff = biff->GetNext();
	}
}


void MSG_Biff_Master::FindNextBiffer(void)
{
	MSG_NikiBiff *biff = NULL;
	time_t low = 0, next = 0, current = 0;

	if (m_timer)
		FE_ClearTimeout(m_timer);
	m_timer = NULL;
	biff = m_biff;
	m_currentBiff = NULL;
	time(&low);
	low += 30000;		// make bogus future date
	while (biff)
	{
		next = biff->GetNextBiffTime();		// returns 0 if not active
		if (next && (next <= low))
		{
			low = next;
			m_currentBiff = biff;
		}
		biff = biff->GetNext();
	}
	if (m_currentBiff)		// we have a winner !
	{
		time(&current);
		low = low - current;	// stopped on debugger or whatever
		if (low < 0)
			low = 1;
		m_timer = FE_SetTimeout(MSG_Biff_Master::TimerCall, m_context, low * 1000L);
	}
}








/***********************************************************************/


// I18N - this is a preference string and should not be localized
static const char *kPrefTemplate = "mail.imap.server.%s.%s";


void MSG_NikiBiff::PrefsChanged(const char* pref, void *closure)
{
	char *prefName = NULL;
	XP_Bool check = FALSE;
	int32 interval = 0;

	// See if our server changed mail_check prefs
	if (m_next)
		m_next->PrefsChanged(pref, closure);
	if (!m_host)										// POP3
	{
		PREF_GetIntPref("mail.check_time", &interval);
		m_interval = interval * 60;

		PREF_GetBoolPref("mail.check_new_mail", &check);
		m_needsToCheck = check;

		PREF_GetBoolPref("mail.pop3_gets_new_mail", &check);
		m_pop3GetsMail = check;
		return;
	}
	if (XP_STRSTR(pref, m_host) && (XP_STRSTR(pref, "check_time") || XP_STRSTR(pref, "check_new_mail") ||
		XP_STRSTR(pref, "get_headers")))
	{
		prefName = (char*) XP_ALLOC(200);
		if (!prefName)
			return;
		PR_snprintf(prefName, 200, kPrefTemplate, m_host, "check_time");
		PREF_GetIntPref(prefName, &interval);
		m_interval = interval * 60;

		PR_snprintf(prefName, 200, kPrefTemplate, m_host, "check_new_mail");
		PREF_GetBoolPref(prefName, &check);
		m_needsToCheck = check;

		PREF_GetBoolPref("mail.imap.new_mail_get_headers", &check);
		m_getHeaders = check;
		XP_FREE(prefName);
	}
}


char * MSG_NikiBiff::GetName(void)
{
	return m_folderName;
}


void MSG_NikiBiff::SetCallback(void *call)
{
	m_callback = (void*) call;
}

// If set, and enabled also set the we need to check for new mail, otherwise sleep

void MSG_NikiBiff::SetCheckFlag(XP_Bool check)
{
	m_needsToCheck = check;
}


time_t MSG_NikiBiff::GetNextBiffTime(void)
{
	if (m_needsToCheck && m_enabled && !NET_IsOffline())
		return m_nextTime;
	return 0;
}

// Remove ourselves from the list, the iterator will get our next and give to
// our previous

MSG_NikiBiff* MSG_NikiBiff::RemoveSelf(void)
{
	MSG_NikiBiff *next = m_next;

	delete this;
	return next;
}

void MSG_NikiBiff::SetInterval(int32 interval)
{
	time_t t;
	
	m_nextTime = 0;
	m_interval = interval * 60;	// calculate seconds, since interval is minutes
	if (m_interval < 0)
		m_interval = 0;
	if (m_interval)
	{
		time(&t);
		m_nextTime = t + m_interval;
	}
}

// Set the next guy in out list

void MSG_NikiBiff::SetNext(MSG_NikiBiff *next)
{
	m_next = next;
}


MSG_NikiBiff* MSG_NikiBiff::GetNext(void)
{
	return m_next;
}



MSG_NikiBiff::MSG_NikiBiff()
{
	m_folderName = m_host = NULL;
	m_pane = NULL;
	m_master = NULL;
	m_context = NULL;
	m_prefs = NULL;
	m_interval = 0;
	m_biffStatFile = NULL;
	m_next = 0L;
	m_nextTime = 0;
	m_needsToCheck = m_enabled = FALSE;
	m_callback = 0;
	m_getHeaders = m_pop3GetsMail = FALSE;
}



MSG_NikiBiff::~MSG_NikiBiff()
{
	FREEIF(m_folderName);
	FREEIF(m_biffStatFile);
	FREEIF(m_host);
}


void MSG_NikiBiff::SetEnabled(XP_Bool enabled)
{
	m_enabled = enabled;
}


void MSG_NikiBiff::SetFolderName(char *name)
{
	FREEIF(m_folderName);
	m_folderName = NULL;
	if (name)
		m_folderName = XP_STRDUP(name);
}

void MSG_NikiBiff::SetHostName(char *host)
{
	XP_Bool check = FALSE;
	
	FREEIF(m_host);
	m_host = NULL;
	if (host)
	{
		m_host = XP_STRDUP(host);
		PREF_GetBoolPref("mail.imap.new_mail_get_headers", &check);
		m_getHeaders = check;
	} else {
		PREF_GetBoolPref("mail.pop3_gets_new_mail", &check);
		m_pop3GetsMail = check;
	}
}

void MSG_NikiBiff::SetStatFile(char *name)
{
	FREEIF(m_biffStatFile);
	m_biffStatFile = NULL;
	if (name)
		m_biffStatFile = XP_STRDUP(name);
}


void MSG_NikiBiff::SetPrefs(MSG_Prefs *prefs)
{
	m_prefs = prefs;
}


void MSG_NikiBiff::SetContext(MWContext *context)
{
	m_context = context;
}


// If the user does not want us to get their POP3 new mail, but just to check for it...
// then check it.

void MSG_NikiBiff::OldPOP3Biff(void)
{
	char* url = NULL;
	URL_Struct* url_struct = NULL;

	url = PR_smprintf("pop3://%s/?check", MSG_GetPopHost(m_prefs));
	if (!url)
		return;
	url_struct = NET_CreateURLStruct(url, NET_SUPER_RELOAD);	/* Old style biff */
	if (!url_struct)
		return;
	url_struct->internal_url = TRUE;
	msg_InterruptContext(m_context, FALSE);
	NET_GetURL(url_struct, FO_PRESENT, m_context, 0L);
	XP_FREE(url); /* allocated by PR_smprintf() */
}



// Reset (recalculate) our time and check for mail if possible

void MSG_NikiBiff::MailCheck(void)
{
	MSG_FolderInfoMail *mailFolder = NULL;
	MSG_IMAPFolderInfoMail *imapFolderNotInView = NULL;
	time_t t;
	int counter = 0, total = 0;
	XPPtrArray panes;
	MSG_Pane *pane;
  
	time(&t);
	m_nextTime = t + m_interval;
/*
#ifdef XP_UNIX
	XP_StatStruct biffst;

	if (!XP_Stat(biffStatFile, &biffst, xpMailFolder) && biffst.st_size > 0)
	  MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NewMail);
	else
	  MSG_SetBiffStateAndUpdateFE(MSG_BIFF_NoMail);
 #endif
*/

	if (NET_IsOffline() || !m_prefs)
		return;
	if (!m_master)
		m_master = m_prefs->GetMasterForBiff();
	if (!m_master)
		m_master = MSG_InitializeMail(m_prefs);
	if (!m_master)
		return;		// Bogus

	// Later on we can pass the owner name and folder name if not just doing inboxes
	if (m_host)	// IMAP folder
		mailFolder = m_master->FindFolderForNikiBiff("INBOX" /* m_folderName */ , (const char*) m_host, NULL /* owner */ );
	else
		mailFolder = m_master->FindFolderForNikiBiff(m_folderName, NULL, NULL);
	if (!mailFolder)
	{
		if (!m_host)
			OldPOP3Biff();
		return;
	}
	if (mailFolder->GetGettingMail())
	{
		time(&t);
		m_nextTime = t + 30;	// try again in 30 seconds since we are busy
		return;
	}
	pane = MSG_Biff_Master::GetPane();
	if (!pane)
	{
		m_master->FindPanesReferringToFolder (mailFolder, &panes);
		total = panes.GetSize();
		m_pane = pane = NULL;
		for (counter = 0; counter < total; counter++)
		{
			pane = (MSG_Pane*) panes.GetAt(counter);
			if ((pane && (pane->GetPaneType() == MSG_FOLDERPANE)) || (pane && !m_pane)) // MSG_THREADPANE
				m_pane = pane;	// find folder pane, or last resort any pane
		}
	} else
		m_pane = pane;
	if (!m_pane)	// Currently only in use for UNIX, until we get a pane
	{
		imapFolderNotInView = mailFolder->GetIMAPFolderInfoMail();	// Folder is not opened in any pane, so just borrow one
		if (imapFolderNotInView && m_context && !imapFolderNotInView->GetGettingMail() && !imapFolderNotInView->IsLocked())
			imapFolderNotInView->MailCheck(m_context);
		return;
	}
	if (!mailFolder->GetGettingMail() && !mailFolder->IsLocked())
	{
		if ((m_host && m_getHeaders) || (!m_host && m_pop3GetsMail))	// POP3 or IMAP
			m_pane->GetMailForAFolder((MSG_FolderInfo*) mailFolder);
		else
		{
			imapFolderNotInView = mailFolder->GetIMAPFolderInfoMail();
			if (imapFolderNotInView)
				imapFolderNotInView->Biff(m_context);		// IMAP Folder, don't get mail
			else
				OldPOP3Biff();								// POP3, don't get mail
		}
	}
}




