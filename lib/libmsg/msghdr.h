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

#ifndef _MSGHDR_H
#define _MSGHDR_H


#include "msgdb.h"

class XPStringObj;

#include "msgdbtyp.h"

class DBMessageHdr
{
	friend class MessageDB;
	friend class NewsGroupDB;
public:
						/** Instance Methods **/
						DBMessageHdr();
						DBMessageHdr(MSG_HeaderHandle handle);
	virtual				~DBMessageHdr();
	virtual XP_Bool		InitFromXOverLine(char * xOverLine, MSG_DBHandle db);

			void		CopyToMessageHdr(MessageHdrStruct *msgHdr,MSG_DBHandle db = NULL);	// helper function to publish header info
			void		CopyToShortMessageHdr(MSG_MessageLine *msgHdr, MSG_DBHandle db /* = NULL */);	// helper function to publish header info
	virtual void		CopyFromDBMsgHdr(DBMessageHdr *msgHdr, MSG_DBHandle srcDB, MSG_DBHandle destDB);

	static XP_Bool		ParseLine(char *line, DBMessageHdr *msgHdr, MSG_DBHandle db);
	static XP_Bool		ParseLine(char *line, MessageHdrStruct *msgHdr);

	virtual XP_Bool		RefersTo(DBMessageHdr *hdr);
	virtual XP_Bool		IsReferredTo(MSG_DBHandle db);
// set and get methods
			XP_Bool		SetThreadId(MessageId threadId);
			XP_Bool		SetMessageKey(MessageId articleNumber);
			XP_Bool		SetSubject(const char * subject, MSG_DBHandle db);
			XP_Bool		SetAuthor(const char * author, MSG_DBHandle db);
			XP_Bool		SetMessageId(const char * messageId, MSG_DBHandle db);
			XP_Bool		SetUnstrippedMessageId(char * messageId, MSG_DBHandle db);
			XP_Bool		SetReferences(const char * references, MSG_DBHandle db);
			void		SetMessageSize(uint32 messageSize);
			void		SetLevel(char level);
			void		SetDate(time_t date);
			// flag handling routines
			void		SetFlags(uint32 flags);
			uint32		GetFlags();
			uint32		OrFlags(uint32 flags);
			uint32		AndFlags(uint32 flags);
			uint32		GetMozillaStatusFlags();
	virtual	void		CopyDBFlagsToMsgHdrFlags(uint32 *pflags);

			char		GetLevel();
			time_t		GetDate() {return m_date;}

			MessageKey	GetArticleNum();
			MessageKey  GetMessageKey();
			MessageKey	GetThreadId();

			// this is almost always the m_messageKey, except for the first message.
			virtual uint32 GetMessageOffset() {return (m_messageKey < 5) ? 0 : m_messageKey;}

	virtual MSG_PRIORITY GetPriority() ;
	/* does news have priority? By default, we ignore it. */
	virtual void		SetPriority(const char * /*priority*/);
	virtual void		SetPriority(MSG_PRIORITY /*priority*/);
	// this one should go away... DMB
	virtual uint32		GetMessageSize();
	void		GetSubject(char *subject, int subjectLen, XP_Bool withRe /* = FALSE */, MSG_DBHandle db );
	XP_Bool		GetSubject(XPStringObj &subject, XP_Bool withRe /* = FALSE */, MSG_DBHandle db );
	void		GetMessageId(char *outMessageId, int messageIdLen, MSG_DBHandle db);
	void		GetAuthor(char *outAuthor, int authorLen, MSG_DBHandle db = NULL);
	void		GetRFC822Author(char *outAuthor, int authorLen, MSG_DBHandle db );
	XP_Bool		GetAuthor(XPStringObj &subject, MSG_DBHandle db);
	XP_Bool		GetRFC822Author(XPStringObj &subject, MSG_DBHandle db);
	void		GetMailboxOfAuthor(char *outAuthor, int authorLen, MSG_DBHandle db );
	XP_Bool		GetMessageId(XPStringObj &messageId, MSG_DBHandle db );
	int32		GetNumReferences();
	const char *GetReference(const char *nextRef, char *reference);
	const char *GetPrevReference(const char *prefRef, char *reference);
	// These are all virtual methods so callers don't need to downcast
	// to mailhdrs.
	virtual int32		GetNumRecipients();
	virtual int32		GetNumCCRecipients();
	virtual void		GetCCList(XPStringObj & /*ccList*/ ,MSG_DBHandle  /*db*/);
	virtual void		GetRecipients(XPStringObj & /*recipient*/, MSG_DBHandle  /*db*/);
	virtual void		GetNameOfRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) ;
	virtual void		GetMailboxOfRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/);
	virtual void		GetNameOfCCRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) ;
	virtual void		GetMailboxOfCCRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/);
	virtual	uint32		GetByteLength();
	virtual uint32		GetLineCount();
	virtual void		PurgeArticle();
	virtual void		PurgeOfflineMessage(MSG_DBHandle db);
	MSG_HeaderHandle GetHandle() {return m_dbHeaderHandle;}
			void		SetHandle(MSG_HeaderHandle headerHandle) {m_dbHeaderHandle = headerHandle;}

#ifdef DEBUG_bienvenu
	static int32 numMessageHdrs;
#endif
protected:
	MSG_HeaderHandle	m_dbHeaderHandle;
	MessageId   m_threadId; 
	MessageId	m_messageKey; 	//news: article number, mail mbox offset
	time_t 		m_date;                         
	uint32		m_messageSize;	// lines for news articles, bytes for mail messages
	uint32		m_flags;
	char		m_level;		// indentation level
};


#endif
