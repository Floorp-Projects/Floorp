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

#ifndef _MsgMast_H_
#define _MsgMast_H_

#include "prlong.h"
#include "msgprnot.h"
#include "errcode.h"
class MSG_Prefs;
class MSG_Pane;
class MSG_ThreadPane;
class MSG_FolderInfo;
class MSG_FolderInfoMail;
class MSG_FolderInfoContainer;
class MSG_IMAPFolderInfoContainer;
class MSG_FolderInfoNews;
class ParseMailboxState;
class msg_HostTable;
class MSG_NewsHost;
class ListNewsGroupState;
class XPPtrArray;
struct MSG_PurgeInfo;
class MSG_FolderCache;
class MSG_CachedConnectionList;
class MSG_IMAPHostTable;

typedef void * pw_ptr;

class MSG_Master : public MSG_PrefsNotify {
public:
	MSG_Master(MSG_Prefs* prefs);
	virtual ~MSG_Master();

	virtual void NotifyPrefsChange(NotifyCode code);
	
	MSG_FolderInfo *GetFolderTree();
	MSG_FolderInfoMail* GetLocalMailFolderTree();
	MSG_IMAPFolderInfoContainer* GetImapMailFolderTree();
	XP_Bool FolderTreeExists ();

	void BroadcastFolderChanged(MSG_FolderInfo *folder);
	void BroadcastFolderDeleted (MSG_FolderInfo *folder);
	void BroadcastFolderAdded (MSG_FolderInfo *folder, MSG_Pane *instigator = NULL);
	void BroadcastFolderKeysAreInvalid (MSG_FolderInfo *folder);

	MSG_Prefs* GetPrefs() {return m_prefs;}

	void SetFirstPane(MSG_Pane* pane);
	MSG_Pane* GetFirstPane();

	int32 GetFolderChildren(MSG_FolderInfo* folder, MSG_FolderInfo** result,
							int32 resultsize);
	
	int32 GetFoldersWithFlag(uint32 flags, MSG_FolderInfo** result,
							 int32 resultsize);

	XP_Bool GetFolderLineById(MSG_FolderInfo* info, MSG_FolderLine* data);
	MSG_Pane *FindPaneOfType(MSG_FolderInfo *id, MSG_PaneType type) ;
	MSG_Pane *FindPaneOfType(const char *url, MSG_PaneType type) ;
	
	MSG_Pane *FindFirstPaneOfType(MSG_PaneType type);
	MSG_Pane *FindNextPaneOfType(MSG_Pane *startHere, MSG_PaneType type);

	MSG_IMAPFolderInfoMail *FindImapMailFolder(const char* onlineServerName);		// As imap would say, BAD
	MSG_IMAPFolderInfoMail *FindImapMailFolder(const char *hostName, const char* onlineServerName, const char *owner, XP_Bool createIfMissing);

	MSG_IMAPFolderInfoMail *FindImapMailFolderFromPath(const char *pathName);
	MSG_FolderInfoMail* FindMagicMailFolder(int flag, XP_Bool createIfMissing);
	MSG_FolderInfoMail* FindFolderForNikiBiff(const char *path, const char *host, const char *owner);
	MSG_FolderInfoMail *FindMailFolder(const char* pathname,
									   XP_Bool createIfMissing);
	MSG_FolderInfoNews *FindNewsFolder(const char *host_and_port,
									   const char *newsGroupName,
									   XP_Bool secureP,
									   XP_Bool autoSubscribe = TRUE);
	MSG_FolderInfoNews *FindNewsFolder(MSG_NewsHost* host,
									   const char *newsGroupName,
									   XP_Bool autoSubscribe = TRUE);
	MSG_ThreadPane*		FindThreadPaneNamed(const char *pathname);
	void				FindPanesReferringToFolder (MSG_FolderInfo *folder, XPPtrArray *outArray);
	MSG_FolderInfo		*FindNextFolder(MSG_FolderInfo *startFolder);
	MSG_FolderInfo		*FindNextFolderWithUnread(MSG_FolderInfo *startFolder);

	ParseMailboxState	*GetParseMailboxState(const char *folderName);
	ParseMailboxState	*GetMailboxParseState() {return m_parseMailboxState;}
	void				SetParseMailboxState(ParseMailboxState *state)
							{m_parseMailboxState = state;}
	void				ClearParseMailboxState();
	void				ClearParseMailboxState(const char *folderName);

	// Get News Header helper routines
	ListNewsGroupState *GetListNewsGroupState(MSG_NewsHost* host,
											  const char *newsGroupName);
	void				ClearListNewsGroupState(MSG_NewsHost* host,
												const char *newsGroupName);

	MSG_FolderInfo*		GetFolderInfo(const char *url, XP_Bool createIfMissing);
	int					GetMessageLineForURL(const char *url, MSG_MessageLine *msgLine);
	MSG_NewsHost* FindHost(const char* name, XP_Bool isSecure,
						   int32 port); // If port is negative, then we will
										// look for a ":portnum" as part of the
										// name.  Otherwise, there had better
										// not be a colon in the name.
	MSG_NewsHost* GetDefaultNewsHost();
	MSG_FolderInfoNews * AddNewsGroup(const char *groupURL);
	MSG_FolderInfoNews * AddProfileNewsgroup(MSG_NewsHost* host,
											 const char *group);
	MSG_NewsHost *		AddNewsHost(const char* hostname, XP_Bool isSecure, int32 port);

	msg_HostTable* GetHostTable() {return m_hosttable;}
	MSG_IMAPHostTable	*GetIMAPHostTable(); 
	MSG_IMAPHost		*AddIMAPHost(const char* hostname,
									  XP_Bool isSecure, 
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespace,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir,
										XP_Bool writePrefs = TRUE);

	MSG_IMAPHost *GetIMAPHost(const char *hostName);
	MSG_FolderInfoContainer *GetImapMailFolderTreeForHost(const char *hostName);
	int32 GetSubscribingHosts(MSG_Host** result, int32 resultsize);
	int32 GetSubscribingIMAPHosts(MSG_IMAPHost** result, int32 resultsize);

	XP_Bool				CleanupNeeded();
	void				CleanupFolders(MSG_Pane *pane); 
	int					SynchronizeOffline(MSG_Pane *pane, XP_Bool bDownloadDiscussions, XP_Bool bGetNewMail, XP_Bool bSendOutbox, XP_Bool bGetDirectories, XP_Bool goOffline);
	int					DownloadForOffline(MSG_Pane *pane, MSG_FolderInfo *folder = NULL);
	void				ClearFolderGotNewBit();

	XP_Bool AcquireRulesSemaphore (void*);
	XP_Bool ReleaseRulesSemaphore (void*);

	XP_Bool	AcquireEditHeadersSemaphore (void *);
	XP_Bool	ReleaseEditHeadersSemaphore (void *);

	// Non-standard implementation but to lure the customers ...
	XP_Bool	IsUserAuthenticated() {return m_userAuthenticated;}
	void	SetUserAuthenticated(XP_Bool bAuthenticated) { m_userAuthenticated = bAuthenticated; }
	XP_Bool	AreLocalFoldersAuthenticated() {return m_localFoldersAuthenticated;}
	void	SetLocalFoldersAuthenticated() {m_localFoldersAuthenticated = TRUE;}

	void	SetMailAccountURL(const char* urlString);
	const char* GetMailAccountURL() { return m_mailAccountUrl; }

	TNavigatorImapConnection* UnCacheImapConnection(const char *host, const char *folderName);
	XP_Bool 				  TryToCacheImapConnection(TNavigatorImapConnection *connection, const char *host, const char *folderName);
	void						CloseCachedImapConnections();
	XP_Bool						HasCachedConnection(MSG_IMAPFolderInfoMail *folder);
	void						ImapFolderClosed(MSG_FolderInfo *folder);

	MsgERR CreateMailFolder (MSG_Pane *invokingPane, MSG_FolderInfo *parent, const char *childName);
	MSG_PurgeInfo			*GetPurgeHdrInfo();
	MSG_PurgeInfo			*GetPurgeArtInfo();
	
	void		ImapGoOnline(MSG_Pane *paneForUrls);

	XP_Bool IsHTMLOK(MSG_FolderInfo* group);
	int SetIsHTMLOK(MSG_FolderInfo* group, MWContext* context, XP_Bool value);

	XP_Bool InitFolderFromCache (MSG_FolderInfo *folder);

	static void PostCreateImapFolderUrlExitFunc (URL_Struct *URL_s, int status, MWContext *window_id);

	XP_Bool	IsCachePasswordProtected();
	XP_Bool IsCollabraEnabled() { return m_collabraEnabled; };
	XP_Bool TryUpgradeToIMAPSubscription(MWContext *context, XP_Bool forceBringUpSubscribeUI, 
		MSG_Pane *paneToAssociate, MSG_IMAPHost *hostBeingOpened);
	void	SetUpgradingToIMAPSubscription(XP_Bool upgrading) { m_upgradingToIMAPSubscription = upgrading; }
	XP_Bool	GetUpgradingToIMAPSubscription() { return m_upgradingToIMAPSubscription; }
	static void PostImapSubscriptionUpgradeExitFunc (URL_Struct *URL_s, int status, MWContext *context);
	static void CancelIMAPProgressCallback(void *closure);
	void	SetIMAPSubscriptionUpgradedPrefs();	// call upon upgrade success, to write out pref.
	XP_Bool	ShowIMAPProgressWindow();
	void	PostIMAPUpgradeFolderLoad(int status);

	XP_Bool	GetPlayingBackOfflineOps() {return m_playingBackOfflineOps;}
	void	SetPlayingBackOfflineOps(XP_Bool playingBackOfflineOps) {m_playingBackOfflineOps = playingBackOfflineOps;}

	char *GetArbitraryHeadersForHostFromFilters(const char *hostName);
	char *GetArbitraryHeadersForHostFromMDN(const char *hostName);
	int PromptForHostPassword(MWContext *context, MSG_FolderInfo *folder);

protected:
	XP_Bool ShouldUpgradeToIMAPSubscription();

private:
	MSG_Prefs* m_prefs;
	MSG_Pane* m_firstpane;

	MSG_FolderInfo* m_folderTree; // Master list of all folders.
	MSG_FolderInfoMail* m_mailfolders; // List of local mail folders.
	MSG_IMAPFolderInfoContainer* m_imapfolders; // List of imap mail folders.

	// get mailbox header handling state - only one ever...
	// This means we can only be parsing one at a time.
	ParseMailboxState		*m_parseMailboxState;
	XP_Bool				m_IMAPfoldersBuilt;
	msg_HostTable* m_hosttable;
	MSG_IMAPHostTable	*m_imapHostTable;

	MSG_CachedConnectionList	*m_cachedConnectionList;

	void *m_editHeadersSemaphoreHolder;
	void *m_rulesSemaphoreHolder;
	// This is a non-standard implementation. But....
	XP_Bool m_userAuthenticated;

	MSG_PurgeInfo	*m_purgeHdrPref;
	MSG_PurgeInfo	*m_purgeArtPref;
	int32			m_purgeThreshhold;
	char *m_mailAccountUrl;

	MSG_FolderCache *m_folderCache;

	// whether or not Collabra is enabled, from the prefs and/or Mission Control
	// default is true
	XP_Bool m_collabraEnabled;
	XP_Bool m_upgradingToIMAPSubscription;
	XP_Bool m_playingBackOfflineOps;
	XP_Bool	m_passwordProtectCache;
	XP_Bool m_localFoldersAuthenticated;

	// For the IMAP subscription upgrade
	pw_ptr		m_progressWindow;
	MSG_ThreadPane	*m_imapUpgradeBeginPane;
};


#endif /* _MsgMast_H_ */
