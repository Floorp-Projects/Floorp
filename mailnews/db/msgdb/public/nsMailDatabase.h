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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsMailDatabase_H_
#define _nsMailDatabase_H_

// This is the subclass of nsMsgDatabase that handles local mail messages.
class nsOfflineImapOperation;

class nsMailDatabase : public nsMsgDatabase
{
public:
			nsMailDatabase();
	virtual ~nsMailDatabase();
	static nsresult			Open(const char * dbName, PRBool create, nsMailDatabase** pMessageDB);

	static  nsresult		CloneInvalidDBInfoIntoNewDB(const char * pathName, nsMailDatabase** pMailDB);

	virtual nsresult		OnNewPath (const char *newPath);

	virtual nsresult		DeleteMessages(IDArray &messageKeys, ChangeListener *instigator);

	virtual int				GetCurVersion() {return kMailDBVersion;}
	static  nsresult		SetFolderInfoValid(const char* pathname, int num, int numunread);
	virtual const char		*GetFolderName() {return m_folderName;}
	virtual nsMailDatabase	*GetMailDB() {return this;}
			MSG_Master		*GetMaster() {return m_master;}
			void			SetMaster(MSG_Master *master) {m_master = master;}

	virtual MSG_FolderInfo *GetFolderInfo();
	
	// for offline imap queued operations
	// these are in the base mail class (presumably) because offline moves between online and offline
	// folders can cause these operations to be stored in local mail folders.
	nsresult				ListAllOfflineOpIds(IDArray &outputIds);
	int						ListAllOfflineDeletes(IDArray &outputIds);
	nsresult				GetOfflineOpForKey(MessageKey opKey, PRBool create, nsOfflineImapOperation **);
	nsresult				AddOfflineOp(nsOfflineImapOperation *op);
	nsresult				DeleteOfflineOp(MessageKey opKey);
	nsresult				SetSourceMailbox(nsOfflineImapOperation *op, const char *mailbox, MessageKey key);
	
	virtual nsresult		SetSummaryValid(PRBool valid = TRUE);
	
	nsresult 				GetIdsWithNoBodies (IDArray &bodylessIds);


protected:
	virtual void			SetHdrFlag(nsMsgHdr *, PRBool bSet, MsgFlags flag);
	virtual void			UpdateFolderFlag(nsMsgHdr *msgHdr, PRBool bSet, 
									 MsgFlags flag, PRFileDesc *fid);
	virtual void			SetReparse(PRBool reparse);

	XP_Bool			m_reparse;
	char			*m_folderName;
	PRFileDesc		*m_folderFile;	/* this is a cache for loops which want file left open */
	MSG_Master		*m_master;
};

#endif
