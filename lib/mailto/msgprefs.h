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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _MsgPrefs_H_
#define _MsgPrefs_H_

#include "msgzap.h"
#include "rosetta.h"

/*
  mwelch 97 July:

  Before adding anything new to MSG_Prefs, consider whether your
  new preference can best be managed using the prefs API directly.
  If you are not getting the preference value often or otherwise
  have no need to cache the prefs value, you are probably better
  off calling the prefs API everywhere and leaving this object 
  alone.

  Having said that, if you want to add a new member variable and
  its access functions to MSG_Prefs, here is the way you want to
  do this, in order to keep the world happy. (Specific conditions,
  of course, may apply to your preference; the instructions below
  apply to most circumstances.)

  If a change in your preference does not need to trigger an immediate
  action:

  1. Put a call to PREF_Get{Int,Bool,Char,etc.)Pref in the Reload()
     method for your member variable and its corresponding JavaScript
	 preference name. This should be the only way by which your 
	 member variable is set.

  2. Your Get() accessor function must call Reload() before returning 
     the member value. This is the way by which you can be assured that
	 the most recently set preference value

  3. If you must have a Set() accessor, implement it in such a way
     that it calls PREF_Set{Int,Bool,Char,etc.}Pref, instead of
	 setting your member variable directly. This way, the prefs API 
	 (and the user's preferences.js file) is properly updated. The 
	 new value circulates back using Reload() and Get() as 
	 described above.

  If a change in your preference requires an immediate response (alert,
  change in behavior, etc):

  1. Implement the preference as described above.

  2. In MSG_PrefsChangeCallback, there is a top-level parse tree (looking
     for names beginning with "mail", "news", or "netw"). Find the clause
	 corresponding to your preference name and add a condition with code
	 that needs to run when your preference changes. Even if the code that
	 runs within MSG_PrefsChangeCallback always reloads on its own, you 
	 still want to leave a load call within Reload() in order to load 
	 the initial value at startup time.

  If you have a transaction that spans across several preference changes,
  be sure to tell the right people so that we can someday have calls to
  transactionalize preference changes. Currently, all we can do is respond
  to changes to individual preferences.
 */
class MSG_PrefsNotify;
class msg_StringArray;

int PR_CALLBACK MSG_PrefsChangeCallback(const char *prefName, void *data);
int PR_CALLBACK MSG_FccPrefChanged (const char *, void *msgPrefs);
int PR_CALLBACK MSG_MailServerTypeChanged (const char *, void *msgPrefs);
int PR_CALLBACK MSG_DraftsPrefChanged (const char *, void *msgPrefs);
int PR_CALLBACK MSG_TemplatesPrefChanged (const char *, void *msgPrefs);
int PR_CALLBACK MSG_UseImapSentmailPrefChanged (const char *prefName, void *msgPrefs);
int msg_FolderPrefChanged(const char *, void *msgPrefs, uint32 flag);

// Corresponds to pref("mailnews.nav_crosses_folders", 0)
// 0=do it, don't prompt 1=prompt, 2=don't do it, don't prompt
typedef enum {
  MSG_NCFDoIt = 0,
  MSG_NCFPrompt = 1,
  MSG_NCFDont = 2
} MSG_NCFValue;


#define MSG_IMAP_DELETE_MODEL_UPGRADE_FLAG		0x00000001 // upgraded to imap delete model?
#define MSG_IMAP_SPECIAL_RESERVED_UPGRADE_FLAG	0x00000002 // RESERVED.  DO NOT CHANGE THIS VALUE!
#define MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG			0x00000004 // upgraded to IMAP subscription?
#define MSG_IMAP_DEFAULT_HOST_UPGRADE_FLAG		0x00000008 // upgraded default host to new per host prefs
#define MSG_IMAP_CURRENT_START_FLAGS			( MSG_IMAP_DELETE_MODEL_UPGRADE_FLAG \
												| MSG_IMAP_SUBSCRIBE_UPGRADE_FLAG \
												| MSG_IMAP_DEFAULT_HOST_UPGRADE_FLAG)
class MSG_Prefs : public MSG_ZapIt 
{
public:

	MSG_Prefs(void);
	virtual ~MSG_Prefs();

	void AddNotify(MSG_PrefsNotify* notify);
	void RemoveNotify(MSG_PrefsNotify* notify);

	void        SetFolderDirectory(const char* directory);
	const char* GetFolderDirectory();
	const char* GetIMAPFolderDirectory();

	// This preference is used to help decide when to perform one time tricks, based
	// on the age of the current user profile.  

	// The theory is:  On a fresh install (actually, the creation of a new user profile),
	// we should never do any one-time upgrade tricks.
	// On an install over a previous verison, we should run each of the upgrade tricks that
	// haven't already been run, for each old profile.

	// The first such example is dealing with the introduction of the imap delete messages
	// model in 4.02.  We wanted the imap delete model to be the default out of the box but 
	// we also wanted a painless upgrade for existing users who use the delete to trash model.
	// The solution is to make the imap delete model the default but if you haven't run the
	// IMAP delete model upgrade and you have a trash folder, then change the preference to the
	// trash model.  In either case, the profile age is now changed to reflect that we have made
	// this upgrade, by setting the appropriate bit. 

	// We will do this in a bitwise manner, so that individual one-time tricks do not depend
	// on the order in which we check for them.

	// This preference should ALWAYS be written to the prefs.js file, unless the value is 0.
	// A value of 0 indicates an upgrade from Communicator 4.0, in which no upgrades have ever
	// been run.

	// The value written out to the prefs should be the sum of all trick flags which have been run.
	// On a fresh install, we automatically write out the following number:
	// {sum of all flags used for an upgrade trick}
	// We do this in config.js, in modules\libpref\src\init

	// The default value (in all.js) should ALWAYS be 0.  This is because if the preference isn't
	// present in the prefs.js file, then it means the value is implicitly 0 (because we are
	// upgrading from 4.0, and none of the one-time tricks have been run).

	// So, when a new flag is added here, you should also modify "mailnews.profile_age" in config.js
	// to be the following number:
	// {sum of all flags used for an upgrade trick}

	// The Flags
	// see above for #definitions
	// 0x00000001 (reserved for the imap delete model trick)
	// 0x00000004 (reserved for migrating to IMAP subscription)
	// 0x00000008 MSG_IMAP_DEFAULT_HOST_UPGRADE_FLAG (upgraded default host to new per host prefs)
	//
	//
	// *(The special reserved upgrade flag is purposely not used for an upgrade anywhere, except in calculating
	//   the default value for all.js.  This is because we always want the value of "mailnews.profile_age"
	//   written out to the prefs file -- it can NEVER take on the "default" value, or else it will be
	//   removed from prefs.js, and this will defeat the purpose of writing out the "current" profile age.
	//   If we were to ever upgrade, it would find the preference not written out, and assume that it is
	//   up-to-date.
	//   To ensure that it is always written out to the file, in the creation of the MSG_Master we will 
	//   un-set this flag each time, so that on a fresh install, the preference will get written out.

	// current total = 1+4+8 = 13 (see #define MSG_IMAP_CURRENT_START_FLAGS above)

	int32		GetStartingMailNewsProfileAge();
	void		SetMailNewsProfileAgeFlag(int32 flag, XP_Bool set = TRUE);  // call this when we perform an upgrade, to set that upgrade bit

	HG97760
	XP_Bool         GetMailServerIsIMAP4();
	MSG_CommandType GetHeaderStyle();
	XP_Bool         GetNoInlineAttachments();
	XP_Bool         GetWrapLongLines();
	XP_Bool         GetDefaultBccSelf(XP_Bool newsBcc);
	XP_Bool         GetAutoQuoteReply();
	const char *    GetDefaultHeaderContents(MSG_HEADER_SET header);
	const char *    GetCopyToSentMailFolderPath();
	//const char *    GetOnlineImapSubDir(); // online subdir for imap folders
	int32           GetPurgeThreshhold();
	XP_Bool         GetPurgeThreshholdEnabled();
	void            GetCitationStyle(MSG_FONT* font, MSG_CITATION_SIZE* size, const char** color);
	const char *    GetPopHost();
	XP_Bool         IMAPMessageDeleteIsMoveToTrash();
	XP_Bool			GetSearchSubFolders();
	XP_Bool			GetSearchServer();

	// Queries for things that are derived off of the preferences.
	int32			GetNumCustomHeaders();
	char *			GetNthCustomHeader(int offset); // caller must use XP_FREEIF on the character string returned


	// Get the full pathname of the folder implementing the given magic type.
	// The result must be free'd with XP_FREE().
	char *          MagicFolderName(uint32 flag, int *pStatus = 0);

	//    I believe this should be a global preference and not per server
	XP_Bool         GetShowPrettyNames();

	XP_Bool         GetNewsNotifyOn();
	void            SetNewsNotifyOn(XP_Bool notifyOn);

	MSG_NCFValue    GetNavCrossesFolders();
	void            SetNavCrossesFolders(MSG_NCFValue navCrossesFoldersOn);

	void            SetMasterForBiff(MSG_Master *masterForBiff);
	MSG_Master *    GetMasterForBiff();

	static XP_Bool	IsEmailAddressAnAliasForMe (const char *addr);

	static int GetXPDirPathPref(const char *prefName, XP_Bool expectFile, char ** result);
	static int SetXPMailFilePref(const char *prefName, char * xpPath);
	static void PlatformFileToURLPath(const char *src, char **dest);
	HG82309

	XP_Bool			GetConfirmMoveFoldersToTrash() { Reload(); return m_confirmMoveFoldersToTrash; }

protected:

	XP_Bool m_dirty; // need to reload prefs at next Get() call

	// m_freezeMailDirectory, if TRUE, prevents (directory) from
	// getting a new value. We set this TRUE in order to ensure that
	// (directory) will only get one meaningful value within a session.
	XP_Bool m_freezeMailDirectory;

    void Reload(void); // reload if dirty flag is set
    void Notify(int16 updateCode);

	// callback when javascript prefs value(s) change
	friend int PR_CALLBACK MSG_PrefsChangeCallback(const char *prefName, void *data);
	friend int msg_FolderPrefChanged (const char *, void *msgPrefs, uint32 flag);

	XP_Bool CopyStringIfChanged(char** str, const char* newvalue);

	int ConvertHeaderSetToSubscript(MSG_HEADER_SET header);

	HG72142

	MSG_Master *m_masterForBiff;

	// Notification prefs (is this all obsolete? or should it be?)
	MSG_PrefsNotify **m_notify;
	int               m_numnotify;
	XP_Bool           m_notifying; // Are we in the middle of notifying listeners?

	// IMAP prefs
	char *   m_IMAPdirectory;
	char *   m_OnlineImapSubDir;
	XP_Bool  m_ImapDeleteMoveToTrash;
	XP_Bool  m_mailServerIsIMAP;
	HG29866

	// Search and Filter prefs
	char *   m_customHeaders;  // arbitrary headers list.
	int32    m_numberCustomHeaders;

	XP_Bool  m_searchSubFolders;
	XP_Bool  m_searchServer;  // when in online mode, search server or search locally (if false)

	// Quoting prefs
	XP_Bool           m_autoQuote;
	MSG_FONT          m_citationFont;
	MSG_CITATION_SIZE m_citationFontSize;
	char *            m_citationColor;

	char *  m_localMailDirectory;
	XP_Bool m_plainText;
	char *  m_popHost;
	char *  m_defaultHeaders[7];
	XP_Bool m_mailBccSelf;
	XP_Bool m_newsBccSelf;
	long    m_mailInputType;
	MSG_CommandType m_headerstyle;
	XP_Bool m_noinline;
	XP_Bool m_wraplonglines;
	XP_Bool m_showPrettyNames;
	XP_Bool m_purgeThreshholdEnabled;
	int32	m_purgeThreshhold;
	XP_Bool m_newsNotifyOn;
	int32	m_navCrossesFolders;
	int32   m_startingMailNewsProfileAge;
	XP_Bool m_confirmMoveFoldersToTrash;

	static msg_StringArray *m_emailAliases;
	static msg_StringArray *m_emailAliasesNot;

	char *	m_draftsName;
	char *	m_sentName;
	char *	m_templatesName;
};

const char * msg_DefaultFolderName(uint32 flag);

#endif /* _MsgPrefs_H_ */
