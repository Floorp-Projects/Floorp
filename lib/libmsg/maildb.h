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
// mail message header database - subclass of MessageDB
#ifndef MAIL_DB_H_
#define MAIL_DB_H_

#include "msgdb.h"
#include "errcode.h"

class DBThreadMessageHdr;
class MailMessageHdr;
class DBOfflineImapOperation;

// this is the version number for the mail db.
// If the file format changes, we just reparse the
// mail folder. The news database is not so lucky and
// since they share DBMessageHdr as a base class for the headers,
// we need to stop changing DBMessageHdr. Another way of putting
// this is the newsdb has its own version, but they both need
// to be changed if DBMessageHdr changes!
const int kMailDBVersion = 12;

// mail message database

class MailDB : public MessageDB
{
public:
			MailDB();
	virtual ~MailDB();
	static MsgERR			Open(const char * dbName, XP_Bool create, 
								MailDB** pMessageDB,
								XP_Bool upgrading = FALSE);

	static  MsgERR			CloneInvalidDBInfoIntoNewDB(const char * pathName, MailDB** pMailDB);

	virtual MsgERR			OnNewPath (const char *newPath);

	virtual MailMessageHdr	*GetMailHdrForKey(MessageKey messageKey);
	virtual MsgERR			DeleteMessages(IDArray &messageKeys, ChangeListener *instigator);

	virtual int				GetCurVersion() {return kMailDBVersion;}
	static  MsgERR	SetFolderInfoValid(const char* pathname, int num, int numunread);
	virtual const char	*GetFolderName() {return m_folderName;}
	virtual MailDB		*GetMailDB() {return this;}
			MSG_Master	*GetMaster() {return m_master;}
			void		SetMaster(MSG_Master *master) {m_master = master;}

	virtual MSG_FolderInfo *GetFolderInfo();
	
	// for offline imap queued operations
	MsgERR						ListAllOfflineOpIds(IDArray &outputIds);
	int							ListAllOfflineDeletes(IDArray &outputIds);
	DBOfflineImapOperation		*GetOfflineOpForKey(MessageKey opKey, XP_Bool create);
	MsgERR						AddOfflineOp(DBOfflineImapOperation *op);
	MsgERR						DeleteOfflineOp(MessageKey opKey);
	MsgERR						SetSourceMailbox(DBOfflineImapOperation *op, const char *mailbox, MessageKey key);
	
	virtual MsgERR		SetSummaryValid(XP_Bool valid = TRUE);
	
	MsgERR 				GetIdsWithNoBodies (IDArray &bodylessIds);


protected:
	virtual void	SetHdrFlag(DBMessageHdr *, XP_Bool bSet, MsgFlags flag);
	virtual void	UpdateFolderFlag(MailMessageHdr *msgHdr, XP_Bool bSet, 
									 MsgFlags flag, XP_File *fid);
	virtual void	SetReparse(XP_Bool reparse);

	XP_Bool			m_reparse;
	char			*m_folderName;
	XP_File			m_folderFile;	/* this is a cache for loops which want file left open */
	MSG_Master		*m_master;
};


class ImapMailDB : public MailDB
{
public:
	ImapMailDB();
	virtual ~ImapMailDB();
	
	static MsgERR		Open(const char * dbName, XP_Bool create, 
						     MailDB** pMessageDB, 
						     MSG_Master* mailMaster,
						     XP_Bool *dbWasCreated);
	
	virtual MsgERR		SetSummaryValid(XP_Bool valid = TRUE);
	
	virtual MSG_FolderInfo *GetFolderInfo();
protected:
	// IMAP does not set local file flags, override does nothing
	virtual void	UpdateFolderFlag(MailMessageHdr *msgHdr, XP_Bool bSet, 
									 MsgFlags flag, XP_File *fid);
};


#endif
