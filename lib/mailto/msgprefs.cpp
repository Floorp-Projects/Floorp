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

#include "msg.h"
#include "errcode.h"

#include "rosetta.h"
#include "msgprefs.h"
#include "msgprnot.h"
#include "prefapi.h"
#include "msgmast.h"
#include "proto.h" // for XP_FindSomeContext

extern "C" 
{
  #include "mkreg.h"
}

#include "ptrarray.h"

#ifdef XP_MAC
	#include "Memory.h"
	#include "Files.h"
	#include "ufilemgr.h"
	#include "uprefd.h"
#else
	extern "C" 
	{
		const char *FE_GetFolderDirectory(MWContext *c);
		void NET_SetMailRelayHost(char *);
		void NET_SetNewsHost(const char *);
	}
#endif

extern "C"
{
	extern int MK_MSG_MAIL_DIRECTORY_CHANGED;
	extern int MK_MSG_SENT_L10N_NAME;
	extern void MIME_ConformToStandard(XP_Bool conform_p);
	extern int MK_MSG_UNABLE_TO_SAVE_DRAFT;
	extern int MK_MSG_UNABLE_TO_SAVE_TEMPLATE;
}

// Notify listeners that something got changed. updateCode is an int16 instead of
// a MSG_PrefsNotify::NotifyCode because it was deemed a Bad Thing to include all of
// msgprnot.h just to pick up that definition.
void MSG_Prefs::Notify(int16 updateCode) 
{	
	int i;
	
	// if we are changing mail servers, trash cached path to imap databases
	// m_IMAPdirectory will be reset by next reload call
	if ((updateCode == MSG_PrefsNotify::MailServerType) || (updateCode == MSG_PrefsNotify::PopHost))
		FREEIF(m_IMAPdirectory);

	// Do the notification in such a way that deleted listeners don't
	// foul up the notification for everyone else. (m_notifying affects
	// the behavior of RemoveNotify so that it only NULLs deleted listeners.)
	m_notifying = TRUE;
	for (i = 0; i < m_numnotify ; i++) 
	{
	    if (m_notify[i])
	    	m_notify[i]->NotifyPrefsChange((MSG_PrefsNotify::NotifyCode) updateCode);
	}
	m_notifying = FALSE;
	
	// Clean up after nulled pointers.
	for(i=(m_numnotify-1);i >= 0; i--)
	{
		if (! m_notify[i])
		{
			if (i == (m_numnotify-1))
				// last element is null, keep decrementing the count
				m_numnotify--; 
			else
				// replace this null element with the last in the list
				m_notify[i] = m_notify[--m_numnotify];
		}
	}
}


char *headerPrefNames[] = 
{
	"mail.identity.reply_to",
	"mail.default_cc",
	"mail.default_fcc",
	"mail.imap_sentmail_path",
	"news.default_cc",
	"news.default_fcc",
	"news.imap_sentmail_path",
};

enum headerPrefIndices
{
	mail_identity_reply_to=0,
	mail_default_cc,
	mail_default_fcc,             // this is a mess don't change order
	mail_imap_sentmail_path,
	news_default_cc,
	news_default_fcc,             // this is a mess don't change order
	news_imap_sentmail_path,
	num_of_header_prefs
};

void MSG_Prefs::PlatformFileToURLPath(const char *platformFile, char **result)
{
	*result = NULL;

	char *tmp = XP_PlatformFileToURL(platformFile);
	XP_ASSERT(tmp && !strncmp(tmp, "file://", 7));
	if (tmp && !strncmp(tmp, "file://", 7))
		*result = XP_STRDUP(&(tmp[7]));

#ifdef XP_UNIX
	if ( *result && **result == '~' ) {
		char buf[1024];
		char* home_dir = getenv("HOME");
		char* tmp = (*result) + 1;

		while ( *tmp == '/' ) 
			tmp++;

		/* trim trailing slashes in home_dir */
		while ( (tmp = strrchr(home_dir, '/')) && tmp[1] == '\0' ) 
			*tmp = '\0';

		PR_snprintf(buf, sizeof(buf), "%s/%s", home_dir ? home_dir : "", tmp);
		XP_FREE(*result);
		*result = XP_STRDUP(buf);
	}
#endif

        FREEIF(tmp);
}

static XP_Bool ShouldSavePrefAsBinaryAlias(const char* prefname)
{
#ifdef XP_MAC
	// Return TRUE if path names are saved as binary alias prefs (the default).
	// Return FALSE for the URL cases, now saved as a string (new in Nova). These are IMAP
	// fccs, and all drafts and all templates.
	if (XP_STRCMP(prefname, "mail.imap_sentmail_path") == 0)
		return FALSE; // URL
	else if (XP_STRCMP(prefname, "news.imap_sentmail_path") == 0)
		return FALSE; // URL
	else if (XP_STRCMP(prefname, "mail.default_drafts") == 0)
		return FALSE; // URL
	else if (XP_STRCMP(prefname, "mail.default_templates") == 0)
		return FALSE; // URL
	return TRUE;
#else
	return FALSE;
#endif // XP_MAC
}

int MSG_Prefs::GetXPDirPathPref(const char *prefName, XP_Bool /*expectFile*/, char **result)
{
	int returnVal = PREF_NOERROR;
	char *tmp = NULL;
	*result = NULL; // in case we fail
	if (ShouldSavePrefAsBinaryAlias(prefName))
	{
		returnVal = PREF_CopyPathPref(prefName, &tmp);
		if (returnVal == PREF_NOERROR)
			*result = tmp;
		return returnVal;
	}
	returnVal = PREF_CopyCharPref(prefName, &tmp); // paths are strings elsewhere
	if (returnVal == PREF_NOERROR)
	{
		// Convert pathname to an xp path.
		if (XP_STRLEN(tmp) > 0 && NET_URL_Type(tmp) != IMAP_TYPE_URL &&
			NET_URL_Type(tmp) != MAILBOX_TYPE_URL)
		{
			PlatformFileToURLPath(tmp,result);
			XP_FREEIF(tmp);
		}
		else
			*result = tmp;
	}
	return returnVal;
}

int MSG_Prefs::SetXPMailFilePref(const char* /*prefName*/, char *xpPath)
{
	int returnVal = PREF_NOERROR;

	if (NET_URL_Type(xpPath) != IMAP_TYPE_URL)
	{
		// ## mwelch 4.0b2 hack! Create the mail file if it doesn't already exist.
		//           Mail parent folder was created at app startup, so it is
		//           safe to assume that the parent directory exists.
		char *platformPath = WH_FileName(xpPath, xpMailFolder);
		XP_File fp = XP_FileOpen(xpPath, xpMailFolder, XP_FILE_APPEND_BIN);
		if (fp) 
			XP_FileClose(fp);

		XP_FREEIF(platformPath);
	}
	return returnVal;
} // MSG_Prefs::SetXPMailFilePref

void MSG_Prefs::SetMailNewsProfileAgeFlag(int32 flag, XP_Bool set /* = TRUE */)
{
	// each 'trick' uses this function to register that it is done, but we do not
	// want one trick to erase the completion of another.
	int32 currentAge = 0;
	PREF_GetIntPref("mailnews.profile_age",&currentAge);
	if (set)
	{
		if (!(currentAge & flag))
			PREF_SetIntPref("mailnews.profile_age",(currentAge | flag));
	}
	else
	{
		if (currentAge & flag)
			PREF_SetIntPref("mailnews.profile_age",(currentAge | ~flag));
	}
	m_dirty = TRUE;
}

int32 MSG_Prefs::GetStartingMailNewsProfileAge()
{
	Reload();
	return m_startingMailNewsProfileAge;
}

void MSG_Prefs::Reload()
{
	if (m_dirty)
	{
		// Temp vars we need to convey pref values.
		int32 intPref;
		char *strPref;
		int prefError = PREF_NOERROR;

		// Load in boolean prefs.
		PREF_GetBoolPref("mail.fixed_width_messages", &m_plainText);
		PREF_GetBoolPref("mail.auto_quote", &m_autoQuote);
		PREF_GetBoolPref("news.show_pretty_names", &m_showPrettyNames);
		PREF_GetBoolPref("news.notify.on", &m_newsNotifyOn);
		PREF_GetBoolPref("mail.cc_self", &m_mailBccSelf);
		PREF_GetBoolPref("news.cc_self", &m_newsBccSelf);
		PREF_GetBoolPref("mail.wrap_long_lines", &m_wraplonglines);
		HG63256
		
		PREF_GetBoolPref("mail.inline_attachments", &m_noinline);
		PREF_GetBoolPref("mail.prompt_purge_threshhold", &m_purgeThreshholdEnabled); //Ask about compacting folders
		m_noinline = !m_noinline;
		
		// Load in int/enum prefs.
		intPref = (int) MSG_ItalicFont; // make italic if pref call fails
		PREF_GetIntPref("mail.quoted_style", &intPref);
		m_citationFont = (MSG_FONT) intPref;
		
		intPref = (int) MSG_NormalSize; // make normal size if pref call fails
		PREF_GetIntPref("mail.quoted_size", &intPref);
		m_citationFontSize = (MSG_CITATION_SIZE) intPref;
		
		intPref = 1;
		PREF_GetIntPref("mailnews.nav_crosses_folders", &intPref);
		m_navCrossesFolders = intPref;

		PREF_GetIntPref("mailnews.profile_age",&m_startingMailNewsProfileAge);

		intPref = 1; // default in case we fail
		prefError = PREF_GetIntPref("mail.show_headers", &intPref);
		switch (intPref)
		{
		case 0: m_headerstyle = MSG_ShowMicroHeaders; break;
		case 1: m_headerstyle = MSG_ShowSomeHeaders; break;
		case 2: m_headerstyle = MSG_ShowAllHeaders; break;
		default:
			XP_ASSERT(FALSE);
			break;
		}

		intPref = 0; // if no pref is set, return 0 - let the backend set the default port
		HG87635


		// Server preference.
		// ### mwelch This used to use mail.use_imap, but we're switching
		//            to mail.server_type as of 4.0b2.
		intPref = 0; // pop by default
		prefError = PREF_GetIntPref("mail.server_type", &intPref);
		m_mailInputType = intPref;
		m_mailServerIsIMAP = (m_mailInputType == 1);

		PREF_GetIntPref("mail.purge_threshhold", &m_purgeThreshhold);
		// Get string prefs.

		if (!m_freezeMailDirectory)
		{
			// m_localMailDirectory
			strPref = m_localMailDirectory;
			m_localMailDirectory = NULL;

			// Get the m_localMailDirectory pref. Passing in TRUE (expectFile) since the flag
			// tells the Mac-specific code to make use of the name (usually "\pMail") in the FSSpec
			// as well as the parent directory ID.
			GetXPDirPathPref("mail.directory", TRUE, &m_localMailDirectory);

#if defined (XP_MAC)
			if (!m_localMailDirectory || !*m_localMailDirectory)
			{
				Assert_(FALSE); // can this happen?  If not, remove this.
				char *newDirURL = NULL;
				// ### mwelch This is a hack, because the MacFE
				//            doesn't set the mail root directory by default.
				FSSpec mailFolder = CPrefs::GetFolderSpec(CPrefs::MailFolder);
				newDirURL = CFileMgr::EncodedPathNameFromFSSpec(mailFolder, true);
				m_localMailDirectory = newDirURL;
			}
#endif
		
			// It is still possible to have a NULL directory at this
			// point in the code. This is because the WinFE sets the default
			// mail directory preference after creating the prefs object.
			
			if (m_localMailDirectory)
			{
				// by arbitrary convention, the directory shouldn't have 
				// a trailing slash
				int len = XP_STRLEN(m_localMailDirectory);
				if (len && m_localMailDirectory[len-1] == '/')
					m_localMailDirectory[len-1] = '\0';
				
#if !defined(XP_MAC) && !defined(XP_WIN) && !defined(XP_OS2)
				if (!strPref || strcmp(m_localMailDirectory, strPref))
				{
					// Create the directory if it doesn't exist (Unix only)
					XP_StatStruct dirStat;
					if (-1 == XP_Stat(m_localMailDirectory, &dirStat, xpMailFolder))
						XP_MakeDirectory (m_localMailDirectory, xpMailFolder);
				}
#endif
			}
			if (strPref) XP_FREE(strPref);
		}
		HG93653
		char onlineDir[256];
		onlineDir[0] = '\0';
		int stringSize = 256;
		PREF_GetCharPref("mail.imap.server_sub_directory", 
						 onlineDir, &stringSize);
		if ( *onlineDir && (*(onlineDir + XP_STRLEN(onlineDir) - 1) != '/') )
			XP_STRCAT(onlineDir, "/");
		if ((!m_OnlineImapSubDir) || 
			((m_OnlineImapSubDir) &&
			 XP_STRCMP(onlineDir, m_OnlineImapSubDir)))
		{
			FREEIF(m_OnlineImapSubDir);
			m_OnlineImapSubDir = XP_STRDUP(onlineDir);
			//if (XP_STRCMP(m_OnlineImapSubDir,""))	// only set it if it is not empty
			//	IMAP_SetNamespacesFromPrefs(GetPopHost(), m_OnlineImapSubDir, "", "");
		}

		PREF_GetBoolPref("mailnews.searchServer", &m_searchServer);

		PREF_GetBoolPref("mailnews.searchSubFolders", &m_searchSubFolders);

		PREF_GetBoolPref("mailnews.confirm.moveFoldersToTrash", &m_confirmMoveFoldersToTrash);

		FREEIF(m_customHeaders);
		PREF_CopyCharPref("mailnews.customHeaders",&m_customHeaders);  

		FREEIF(m_citationColor);
		PREF_CopyCharPref("mail.citation_color", &m_citationColor);
		
		FREEIF(m_popHost);
		PREF_CopyCharPref("network.hosts.pop_server", &m_popHost);
		
		PREF_GetBoolPref("mail.imap.delete_is_move_to_trash", &m_ImapDeleteMoveToTrash);
		
		// Set the smtp and news (nntp) hosts.
		strPref = NULL;
		prefError = PREF_CopyCharPref("network.hosts.smtp_server", &strPref);
		if (prefError == PREF_NOERROR)
			NET_SetMailRelayHost(strPref);
		XP_FREEIF(strPref);

		for(int i=0;i<(int) num_of_header_prefs ;i++)
		{
			strPref = NULL;
			// Only look at the path if the "use it" bool flag is set!
			// Bug #45449 jrm
			XP_Bool doingFccPath = TRUE, wantsFccPath = FALSE;
			if (i == mail_default_fcc || i == mail_imap_sentmail_path)
#ifdef XP_MAC
				PREF_GetBoolPref("mail.use_fcc", &wantsFccPath);
#else
			    wantsFccPath = TRUE;
#endif
			else if (i == news_default_fcc || i == news_imap_sentmail_path)
#ifdef XP_MAC
				PREF_GetBoolPref("news.use_fcc", &wantsFccPath);
#else
			    wantsFccPath = TRUE;
#endif
			else
				doingFccPath = FALSE;
			if (doingFccPath && wantsFccPath)
			{
				prefError = GetXPDirPathPref(headerPrefNames[i], TRUE, &strPref);
				if ((prefError != PREF_NOERROR) || (!strPref) || (!*strPref) ||
					(m_localMailDirectory && !XP_STRCMP(strPref, m_localMailDirectory)))
				{
					// Take the directory preference and add "Sent" to it.
					char *sent = XP_GetString(MK_MSG_SENT_L10N_NAME);
					XP_FREEIF(strPref);

					if (m_localMailDirectory && *m_localMailDirectory)
						strPref = PR_smprintf("%s/%s", m_localMailDirectory, sent);

#ifdef XP_MAC
					// Still may need to ensure the file exists.
					SetXPMailFilePref(headerPrefNames[i], strPref);
#endif
				}
			}
			else if (!doingFccPath)
				PREF_CopyCharPref(headerPrefNames[i], &strPref);
			FREEIF(m_defaultHeaders[i]);
			m_defaultHeaders[i] = strPref;
		}

		// Collect all the email addresses which specify the user. We'll need to know them
		// when checking the Reply recipients, or doing MDN receipts
		if (PREF_NOERROR == PREF_CopyCharPref ("mail.identity.useremail.aliases", &strPref))
		{
			if (*strPref) // default is empty string. don't create an array for that
			{
				if (!m_emailAliases)
					m_emailAliases = new msg_StringArray (TRUE /*ownsMemory*/);
				if (m_emailAliases)
				{
					m_emailAliases->RemoveAll();
					m_emailAliases->ImportTokenList (strPref);
				}
			}
			XP_FREE (strPref);
		}

		// Collect the email addresses which can't be considered aliases for this user.
		// This is intended to keep the email aliases feature from defeating the reply-to
		// header in messages
		char *replyTo = m_defaultHeaders[mail_identity_reply_to];
		if (replyTo && *replyTo)
		{
			if (!m_emailAliasesNot)
				m_emailAliasesNot = new msg_StringArray (TRUE);
			if (m_emailAliasesNot)
			{
				char *addresses = NULL;
				int num = 0;
				if (0 != (num = MSG_ParseRFC822Addresses (replyTo, NULL, &addresses)))
				{
					// We're ignoring the name of the reply-to header, since the
					// actual address is all we care about for the alias calculation
					m_emailAliasesNot->RemoveAll();
					for (int i = 0; i < num; i++)
					{
						m_emailAliasesNot->Add (addresses);
						addresses += XP_STRLEN (addresses) + 1;
					}
				}
			}
		}


	}
	m_dirty = FALSE;
}

int PR_CALLBACK MSG_PrefsChangeCallback(const char * prefName, void *data)
{
	MSG_Prefs *prefs = (MSG_Prefs *) data;

	if (prefs)
		prefs->m_dirty = TRUE;

	// Depending on the preference being changed,
	// notify listeners as to the change.

	// Default headers first, since we have easy access to them.
	for (int i=0;i<(int) num_of_header_prefs;i++)
	{
		if (!XP_STRCMP(prefName, headerPrefNames[i]))
		{
			prefs->Notify(MSG_PrefsNotify::DefaultHeader);
			return PREF_NOERROR;
		}
	}

	if (!XP_STRNCMP(prefName, "netw", 4))
	{
    	// cause smtp and news host to be set to new values immediately
        if (prefs) prefs->Reload(); 
	}
	else if (!XP_STRNCMP(prefName, "mail", 4))
	{
		if (0) ;
	}

	return PREF_NOERROR;
}

MSG_Prefs::MSG_Prefs()
{
	m_citationFont = MSG_PlainFont;
	m_citationFontSize = MSG_Bigger;
	m_plainText = TRUE;
	m_mailServerIsIMAP = FALSE;
	HG98298
	m_ImapDeleteMoveToTrash = TRUE;
	m_IMAPdirectory = NULL;
	m_headerstyle = MSG_ShowSomeHeaders;
	m_masterForBiff = NULL;
	m_notifying = FALSE;

	// Set up dirty flag.
	m_dirty = TRUE;
	m_freezeMailDirectory = FALSE;

	m_emailAliases = NULL;
	m_emailAliasesNot = NULL;

	// Set up prefs callback.
	PREF_RegisterCallback("mail.", &MSG_PrefsChangeCallback, this);
	PREF_RegisterCallback("news.", &MSG_PrefsChangeCallback, this);
	PREF_RegisterCallback("mailnews.", &MSG_PrefsChangeCallback, this);
	PREF_RegisterCallback("network.hosts.", &MSG_PrefsChangeCallback, this);

	// Load pref values from the prefs api.
	// (We can rely on the string members being NULL initially 
	// since we derive from MSG_Zap.)
	Reload();


	// we are only interested in what this prefs value was at startup so initialize it
	// here rather than in Reload()
	m_startingMailNewsProfileAge = 0;
	PREF_GetIntPref("mailnews.profile_age",&m_startingMailNewsProfileAge);
	HG92734
}

MSG_Prefs::~MSG_Prefs() 
{
	XP_ASSERT(m_numnotify == 0);
	FREEIF(m_localMailDirectory);
	FREEIF(m_citationColor);
	FREEIF(m_popHost);
	for (int i=0 ; i<sizeof(m_defaultHeaders)/sizeof(char*) ; i++) 
		FREEIF(m_defaultHeaders[i]);
	XP_FREEIF(m_IMAPdirectory);

	delete m_emailAliases;
	m_emailAliases = NULL;
	delete m_emailAliasesNot;
	m_emailAliasesNot = NULL;

}



void MSG_Prefs::AddNotify(MSG_PrefsNotify* notify)
{
	MSG_PrefsNotify** tmp = m_notify;
	m_notify = new MSG_PrefsNotify* [m_numnotify + 1];
	for (int i=0 ; i<m_numnotify ; i++) {
		XP_ASSERT(tmp[i] != notify);
		m_notify[i] = tmp[i];
	}
	m_notify[m_numnotify++] = notify;
	delete [] tmp;  // not sure why this wasn't here before.
					// Could have used XP_PtrArray...
}


void MSG_Prefs::RemoveNotify(MSG_PrefsNotify* notify)
{
	int i = 0;

	if (m_notifying)
	{
		// We're in the process of notifying listeners, so we
		// can't shuffle pointers around.
		// Just null out the pointer in question, Notify will
		// clean up after us.
		for (; i<m_numnotify; i++)
		{
			if (m_notify[i] == notify)
				m_notify[i] = NULL;
		}
		return;
	}

	// If we're not notifying listeners, just replace the
	// dead element with the last in the list, then decrement
	// the listener count.
	for (; i<m_numnotify ; i++) 
	{
		if (m_notify[i] == notify) 
		{
			m_notify[i] = m_notify[--m_numnotify];
			return;
		}
	}
	XP_ASSERT(0);
}

XP_Bool MSG_Prefs::GetSearchServer()
{
	Reload();
	return m_searchServer;
}

XP_Bool MSG_Prefs::GetSearchSubFolders()
{
	Reload();
	return m_searchSubFolders;
}

int32 MSG_Prefs::GetNumCustomHeaders()
{
	Reload();
	// determine number of custom headers in the preference so far....
	int count = 0;
	
	if (!m_customHeaders)
		return 0;

	char * buffer = XP_STRDUP(m_customHeaders);
	char * marker = buffer;

	while (XP_STRTOK_R(nil, ":, ", &buffer))
		count++;
	XP_FREEIF(marker);
	return count;
}


// caller must use XP_FREE to deallocate the header string this returns.
char * MSG_Prefs::GetNthCustomHeader(int offset)
{
	Reload();
	char * temp = NULL;
	if (offset < 0)
		return temp;

	if (!m_customHeaders)
		return NULL;

	char * buffer = XP_STRDUP(m_customHeaders);
	char * marker = buffer;

	for (int count = 0; count <= offset; count++)
	{
		temp = XP_STRTOK_R(nil,",: ",&buffer);
		if (!temp)
			break;
	}

	// temp now points to the token string
	if (temp)
		temp = XP_STRDUP(temp); // make a copy of the string  // caller must deallocate the space

	XP_FREEIF(marker);      // free our copy of the buffer...
	return temp;
}


#ifdef XP_UNIX
void MSG_Prefs::SetFolderDirectory(const char* d)
{
	PREF_SetCharPref("mail.directory", d);
	m_dirty = TRUE;
}
#else
void MSG_Prefs::SetFolderDirectory(const char*)
{
}
#endif


const char *MSG_Prefs::GetFolderDirectory()
{
	Reload();
	return m_localMailDirectory;
}

const char *MSG_Prefs::GetIMAPFolderDirectory()
{
	Reload();
	if (!m_IMAPdirectory)
	{
		char *machinePathName = 0;
		if (m_popHost)
		{
			// see if there's a :port in the server name.  If so, strip it off when
			// creating the server directory.
			char *server = XP_STRDUP(m_popHost);
			char *port = 0;
			if (server)
			{
				port = XP_STRCHR(server,':');
				if (port)
				*port = 0;
				machinePathName = WH_FileName(server, xpImapServerDirectory);
				XP_FREE(server);
			}	
		}
		if (machinePathName)
		{
			char *imapUrl = XP_PlatformFileToURL (machinePathName);
			if (imapUrl)
			{
				m_IMAPdirectory = XP_STRDUP(imapUrl + XP_STRLEN("file://"));
				XP_FREE(imapUrl);
			}
			XP_FREE (machinePathName);
		}
	}
	return m_IMAPdirectory;
}


void MSG_Prefs::GetCitationStyle(MSG_FONT* f, MSG_CITATION_SIZE* s, const char** c)
{
	Reload();
	if (f) *f = m_citationFont;
	if (s) *s = m_citationFontSize;
	if (c) *c = m_citationColor;
}


const char* MSG_Prefs::GetPopHost() 
{
	Reload();
	return m_popHost;
}

XP_Bool MSG_Prefs::IMAPMessageDeleteIsMoveToTrash()
{
	Reload();
	return m_ImapDeleteMoveToTrash;
}

/*
const char *MSG_Prefs::GetOnlineImapSubDir()
{
	Reload();
	return m_OnlineImapSubDir;
}
*/

XP_Bool MSG_Prefs::GetMailServerIsIMAP4()
{
	Reload();
	return m_mailServerIsIMAP;
}

HG87637

const char *MSG_Prefs::GetCopyToSentMailFolderPath()
{
	return GetDefaultHeaderContents(MSG_FCC_HEADER_MASK);
}

MSG_CommandType MSG_Prefs::GetHeaderStyle()
{
	Reload();
	return m_headerstyle;
}

XP_Bool MSG_Prefs::GetNoInlineAttachments()
{
	Reload();
	return m_noinline;
}

XP_Bool MSG_Prefs::GetWrapLongLines()
{
	Reload();
	return m_wraplonglines;
}

XP_Bool MSG_Prefs::GetAutoQuoteReply() 
{
	Reload();
	return m_autoQuote;
}

const char* MSG_Prefs::GetDefaultHeaderContents(MSG_HEADER_SET header) 
{
	Reload();
	int i = ConvertHeaderSetToSubscript(header);
	if (i < 0) 
		return NULL;
	return m_defaultHeaders[i];
}

XP_Bool MSG_Prefs::GetDefaultBccSelf(XP_Bool newsBcc)
{
	Reload();
	return newsBcc ? m_newsBccSelf : m_mailBccSelf;
}

int32 MSG_Prefs::GetPurgeThreshhold()
{
    Reload();
	return m_purgeThreshhold;
}

XP_Bool MSG_Prefs::GetPurgeThreshholdEnabled()
{
    Reload();
	return m_purgeThreshholdEnabled;
}

XP_Bool MSG_Prefs::GetShowPrettyNames()
{
    Reload();
	return m_showPrettyNames;
}

HG92435

XP_Bool MSG_Prefs::GetNewsNotifyOn()
{
    Reload();
	return m_newsNotifyOn;
}

void MSG_Prefs::SetNewsNotifyOn(XP_Bool notify)
{
	PREF_SetBoolPref("news.notify.on", notify);
}

MSG_NCFValue MSG_Prefs::GetNavCrossesFolders()
{
    Reload();
	return (MSG_NCFValue) m_navCrossesFolders;
}

void MSG_Prefs::SetNavCrossesFolders(MSG_NCFValue cross)
{
	PREF_SetIntPref("mailnews.nav_crosses_folders", (int32) cross);
}

MSG_Master *MSG_Prefs::GetMasterForBiff()
{
    Reload();
	return m_masterForBiff;
}

void MSG_Prefs::SetMasterForBiff(MSG_Master *mstr)
{
	// Since we're dealing with a mail master, this by
	// definition is a temporary preference. So, just
	// set the member in this case.
	m_masterForBiff = mstr;
}


XP_Bool MSG_Prefs::CopyStringIfChanged(char** str, const char* value) 
{
	if (*str == NULL && value == NULL) return FALSE;
	if (*str && value && XP_STRCMP(*str, value) == 0) return FALSE;

	if (*str)
		delete [] *str;
	if (value) 
	{
		*str = new char[XP_STRLEN(value) + 1];
		if (*str) 
			XP_STRCPY(*str, value);
	} 
	else 
		*str = NULL;
	return TRUE;
}


int MSG_Prefs::ConvertHeaderSetToSubscript(MSG_HEADER_SET header) 
{
	switch (header) 
	{
		case MSG_REPLY_TO_HEADER_MASK:
			return 0;
		case MSG_BCC_HEADER_MASK:
			return 1;
		case MSG_FCC_HEADER_MASK:
		{
			XP_Bool use_imap_sentmail = FALSE;
			PREF_GetBoolPref("mail.use_imap_sentmail", &use_imap_sentmail);
			if (use_imap_sentmail)
				return 3;
			else
				return 2;
		}
		case MSG_NEWS_BCC_HEADER_MASK:
			return 4;
		case MSG_NEWS_FCC_HEADER_MASK:
		{
			XP_Bool use_imap_sentmail = FALSE;
			PREF_GetBoolPref("news.use_imap_sentmail", &use_imap_sentmail);
			if (use_imap_sentmail)
				return 6;
			else
				return 5;
		}
		default:
			XP_ASSERT (0);
			return -1;
	}
}


msg_StringArray *MSG_Prefs::m_emailAliases = NULL;
msg_StringArray *MSG_Prefs::m_emailAliasesNot = NULL;

/*static*/ XP_Bool MSG_Prefs::IsEmailAddressAnAliasForMe (const char *addr)
{
	// Does the address match one of the user's alias expressions (e.g. foo@*.netscape.com)

	if (m_emailAliasesNot)
	{
		// Some addresses can't be considered aliases. Right now, this is just used
		// for the Reply-To address, so that if you Reply All, your Reply-To address 
		// will be included, or if you send yourself an MDN request, you'll get one back
		//
		// NB: this list is guaranteed to hold only addresses, not names, so we can strcmp
		// it without parsing it (here)
		for (int i = 0; i < m_emailAliasesNot->GetSize(); i++)
			if (!strcasecomp (addr, m_emailAliasesNot->GetAt(i)))
				return FALSE;
	}

	if (m_emailAliases) // master/prefs may not have been initialized yet
	{
		for (int i = 0; i < m_emailAliases->GetSize(); i++)
		{
			char *alias = (char*) m_emailAliases->GetAt(i); //Hacky cast: regexp API isn't const

			if (VALID_SXP == NET_RegExpValid (alias))
			{
				// The alias is a regular expression, so send it into the regexp evaluator
				if (!NET_RegExpMatch ((char*) addr, alias, FALSE /*case sensitive*/))
					return TRUE;
			}
			else
			{
				// The alias is not a regular expression, so just use a string compare
				if (!strcasecomp (addr, alias))
					return TRUE;
			}
		}
	}
	return FALSE;
}

