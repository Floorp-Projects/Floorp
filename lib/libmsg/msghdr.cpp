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
#include "xp.h"
#include "msghdr.h"
#include "xp_time.h"
#include "msgdbapi.h"
#include "msgstrob.h"

/* ****************************************************************** */
						/** DBMessageHdr Class **/
/* ****************************************************************** */

#ifdef DEBUG_bienvenu
int32 DBMessageHdr::numMessageHdrs = 0;
#endif
DBMessageHdr::DBMessageHdr()
{
	m_level = 0;
	m_threadId = 0;
	m_flags = 0;
	m_messageKey = 0;
	m_date = 0;
	m_dbHeaderHandle = 0;
#ifdef DEBUG_bienvenu
	numMessageHdrs++;
#endif
}

DBMessageHdr::DBMessageHdr(MSG_HeaderHandle handle)
{
	MSG_DBHeaderExchange headerInfo;
	m_dbHeaderHandle = handle;
	if (handle)
	{
		MSG_HeaderHandle_GetHeaderInfo(handle, &headerInfo);
		m_threadId = headerInfo.m_threadId; 
		m_messageKey = headerInfo.m_messageKey; 	//news: article number, mail mbox offset
		m_date = headerInfo.m_date; 
		m_messageSize = headerInfo.m_messageSize;	// lines for news articles, bytes for mail messages
		m_flags = headerInfo.m_flags;
		m_level = headerInfo.m_level;
	}
	else
	{
		m_threadId = kIdNone;
		m_messageKey = kIdNone;
		m_date = 0;
		m_messageSize = 0;
		m_flags = 0;
		m_level = 0;
	}
#ifdef DEBUG_bienvenu
	numMessageHdrs++;
#endif
}

DBMessageHdr::~DBMessageHdr()
{
#ifdef DEBUG_bienvenu
	numMessageHdrs--;
#endif
	if (m_dbHeaderHandle)
		MSG_HeaderHandle_RemoveReference(m_dbHeaderHandle);
}


XP_Bool DBMessageHdr::InitFromXOverLine(char * xOverLine, MSG_DBHandle db)
{
	if (!ParseLine(xOverLine, this, db))
			return FALSE;
	return TRUE;
}

/* static method */
XP_Bool DBMessageHdr::ParseLine(char *line, DBMessageHdr *msgHdr, MSG_DBHandle db)
{
  uint32 lines;
  uint32 msgSize = 0;
  char *next = line;
#define GET_TOKEN()								\
  line = next;									\
  next = (line ? XP_STRCHR (line, '\t') : 0);	\
  if (next) *next++ = 0

  GET_TOKEN ();											/* message number */
  msgHdr->SetMessageKey (atol (line));
 
  if (msgHdr->GetMessageKey() == 0)					/* bogus xover data */
	return FALSE;

  GET_TOKEN ();											/* subject */
  if (line)
	msgHdr->SetSubject(line, db);

  GET_TOKEN ();											/* sender */
  if (line)
	msgHdr->SetAuthor(line, db);

  GET_TOKEN ();	
  if (line)
	  msgHdr->SetDate(XP_ParseTimeString (line, FALSE));										/* date */

  GET_TOKEN ();											/* message id */
   if (line)
		msgHdr->SetUnstrippedMessageId(line, db);

  GET_TOKEN ();											/* references */
  if (line)
	msgHdr->SetReferences(line, db);

  GET_TOKEN ();											/* bytes */
//  msgSize = (line) ? atol (line) : 0;
//  msgHdr->SetMessageSize(msgSize);
  /* ignored */

  GET_TOKEN ();											/* lines */
  lines = line ? atol (line) : 0;
  if (!msgSize)
	msgHdr->SetMessageSize(lines);
  GET_TOKEN ();											/* xref */
  return TRUE;
}

// It would be nice to get rid of this in favor of the routine above that takes
// a DBMessageHdr.
/* static method */
XP_Bool DBMessageHdr::ParseLine(char *line, 	MessageHdrStruct *msgHdr)
{
  uint32 lines;
  char *next = line;
#define GET_TOKEN()								\
  line = next;									\
  next = (line ? XP_STRCHR (line, '\t') : 0);	\
  if (next) *next++ = 0

  GET_TOKEN ();											/* message number */
  msgHdr->m_messageKey = atol (line);
 
  if (msgHdr->m_messageKey == 0)					/* bogus xover data */
	return FALSE;

  GET_TOKEN ();											/* subject */
  if (line)
	msgHdr->SetSubject(line);

  GET_TOKEN ();											/* sender */
  if (line)
	msgHdr->SetAuthor(line);

  GET_TOKEN ();	
  if (line)
	  msgHdr->SetDate(line);										/* date */

  GET_TOKEN ();											/* message id */
   if (line)
		msgHdr->SetMessageID(line);

  GET_TOKEN ();											/* references */
  if (line)
	msgHdr->SetReferences(line);

  GET_TOKEN ();											/* bytes */
  /* ignored */

  GET_TOKEN ();											/* lines */
  lines = line ? atol (line) : 0;
  msgHdr->SetLines(lines);
  GET_TOKEN ();											/* xref */
  return TRUE;
}

void DBMessageHdr::CopyToMessageHdr(MessageHdrStruct *msgHdr, MSG_DBHandle db /* = NULL */)
{
	MSG_HeaderHandle_CopyToMessageHdr(m_dbHeaderHandle, msgHdr, db);
}

void DBMessageHdr::CopyToShortMessageHdr(MSG_MessageLine *msgHdr, MSG_DBHandle db /* = NULL */)
{
	MSG_HeaderHandle_CopyToShortMessageHdr(m_dbHeaderHandle, msgHdr, db);
	// msgHdr should be set to zeroes by caller
}

void DBMessageHdr::CopyDBFlagsToMsgHdrFlags(uint32 *pflags)
{
	uint32 dbFlags = GetFlags();
	MessageDB::ConvertDBFlagsToPublicFlags(&dbFlags);
	*pflags = dbFlags;
}

XP_Bool		DBMessageHdr::SetThreadId(MessageKey threadId)
{
	m_threadId = threadId;
	MSG_HeaderHandle_SetThreadID(m_dbHeaderHandle, threadId);
	return TRUE;
}
XP_Bool		DBMessageHdr::SetMessageKey(MessageKey messageKey)
{
	m_messageKey = messageKey;
	MSG_HeaderHandle_SetMessageKey(m_dbHeaderHandle, messageKey);
	// DMB TODO
//	XP_ASSERT(messageKey != 0);	// does this happen? should I set messageKey to 1 for db?
	return TRUE;
}

// these could all be virtual inlines
MSG_PRIORITY DBMessageHdr::GetPriority() {return MSG_NoPriority;}
void		DBMessageHdr::SetPriority(const char * /*priority*/) {}
void		DBMessageHdr::SetPriority(MSG_PRIORITY /*priority*/) {}
int32		DBMessageHdr::GetNumRecipients() {return 0;}
int32		DBMessageHdr::GetNumCCRecipients() {return 0;}
void		DBMessageHdr::GetCCList(XPStringObj & /*ccList*/ ,MSG_DBHandle  /*db*/) {}
void		DBMessageHdr::GetRecipients(XPStringObj & /*recipient*/, MSG_DBHandle  /*db*/) {}
void		DBMessageHdr::GetNameOfRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) {}
void		DBMessageHdr::GetMailboxOfRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) {}
void		DBMessageHdr::GetNameOfCCRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) {}
void		DBMessageHdr::GetMailboxOfCCRecipient(XPStringObj & /*recipient*/, int /*whichRecipient*/, MSG_DBHandle  /*db*/) {}
uint32		DBMessageHdr::GetByteLength() {return MSG_HeaderHandle_GetByteLength(GetHandle());}
uint32		DBMessageHdr::GetLineCount() {return MSG_HeaderHandle_GetLineCount(GetHandle());}
uint32		DBMessageHdr::GetMessageSize() {return MSG_HeaderHandle_GetMessageSize(GetHandle());}

void	DBMessageHdr::PurgeArticle() {}

// This is the key used in the database, which is almost always the same
// as the m_messageKey, except for the first message in a mailbox,
// which has a m_messageKey of 0, but a non-zero fID in the database.
MessageKey DBMessageHdr::GetMessageKey()
{	   
	m_messageKey = MSG_HeaderHandle_GetMessageKey(GetHandle());
	return m_messageKey;
	// DMB TODO	- do we need to play the 0 key/id problem?
}

void DBMessageHdr::SetMessageSize(uint32 messageSize)
{
	m_messageSize = messageSize;
	MSG_HeaderHandle_SetMessageSize(m_dbHeaderHandle, messageSize);
}

XP_Bool		DBMessageHdr::SetSubject(const char * subject, MSG_DBHandle db)
{
	MSG_HeaderHandle_SetSubject(m_dbHeaderHandle, subject, db);
	return TRUE;
}

XP_Bool		DBMessageHdr::SetAuthor(const char * author,
									 MSG_DBHandle db)
{
	MSG_HeaderHandle_SetAuthor(m_dbHeaderHandle, author, db);
	return TRUE;
}

// This assumes the caller has done the stripping off of '<' and '>'
XP_Bool		DBMessageHdr::SetMessageId(const char * messageId,
										MSG_DBHandle db)
{
//	XP_ASSERT(messageId[0] != '<'); Nelson Bolyard hits this assert because of some spam, so we can't leave it in.

	MSG_HeaderHandle_SetMessageId(m_dbHeaderHandle, messageId, db);
	return TRUE;
}

XP_Bool		DBMessageHdr::SetUnstrippedMessageId(char * messageId,
										MSG_DBHandle db)
{
	if (messageId[0] == '<')
		messageId++;

	char * lastChar = messageId + strlen(messageId) -1;
	if (*lastChar == '>')
		*lastChar = '\0';
	MSG_HeaderHandle_SetMessageId(m_dbHeaderHandle, messageId, db);
	return TRUE;
}

XP_Bool		DBMessageHdr::SetReferences(const char * references, MSG_DBHandle db)
{
	MSG_HeaderHandle_SetReferences(m_dbHeaderHandle, references, db);
	return TRUE;
}

void	DBMessageHdr::SetFlags(uint32 flags)
{
	m_flags = flags;
	MSG_HeaderHandle_SetFlags(m_dbHeaderHandle, flags);
}

uint32		DBMessageHdr::OrFlags(uint32 flags)
{
	return MSG_HeaderHandle_OrFlags(m_dbHeaderHandle, flags);
}

uint32		DBMessageHdr::AndFlags(uint32 flags)
{
	return MSG_HeaderHandle_AndFlags(m_dbHeaderHandle, flags);
}


void DBMessageHdr::SetDate(time_t date)
{
	m_date = date;
	MSG_HeaderHandle_SetDate(m_dbHeaderHandle, date);
}

void DBMessageHdr::SetLevel(char level)
{
	m_level = level;
	MSG_HeaderHandle_SetLevel(m_dbHeaderHandle, level);
}


char DBMessageHdr::GetLevel()
{
	return m_level;
}

uint32	DBMessageHdr::GetFlags()
{
	return (m_flags = MSG_HeaderHandle_GetFlags(m_dbHeaderHandle));
}

uint32 DBMessageHdr::GetMozillaStatusFlags()
{
	uint32 flags = 0;
	uint32 priority = GetPriority();

	// copy over flags that are the same
	flags |= (kMozillaSameAsMSG_FLAG & m_flags); 
	if (m_flags & kExpunged)	// is this needed?
		flags |= MSG_FLAG_EXPUNGED;
	if (m_flags & kHasRe)
		flags |= MSG_FLAG_HAS_RE;
	if (m_flags & kPartial)
		flags |= MSG_FLAG_PARTIAL;
	if (priority != 0)
	{
		flags |= (priority << 13);	// use the upper three bits for priority.
	}
	// Disaster will ensue if we use more than 16 bits!  If somebody adds a bad bit,
	// make sure we don't corrupt the user's mail folder by writing out more than
	// four characters. 
	uint32 legalFlags = flags & 0x0000FFFF;
	XP_ASSERT(flags==legalFlags);
	return legalFlags;
}

MessageId	DBMessageHdr::GetArticleNum()
{
//	XP_ASSERT(fID == m_messageKey || m_messageKey == 0);
	// DMB TODO ???
	return m_messageKey;
//	return fID;
}

MessageId	DBMessageHdr::GetThreadId()
{
	return m_threadId;
}

void DBMessageHdr::CopyFromDBMsgHdr(DBMessageHdr *oldHdr, MSG_DBHandle srcDB, MSG_DBHandle destDB /*= NULL*/)
{
	MSG_HeaderHandle_CopyFromMsgHdr(m_dbHeaderHandle,  oldHdr->GetHandle(), srcDB, destDB);
}


XP_Bool	DBMessageHdr::GetMessageId(XPStringObj &messageId, MSG_DBHandle db /* = NULL */)
{
	char	*messageIdStr;

	XP_Bool ret = MSG_HeaderHandle_GetMessageId(m_dbHeaderHandle, &messageIdStr, db);
	messageId.SetStrPtr(messageIdStr);
	return ret;
}

XP_Bool DBMessageHdr::GetSubject(XPStringObj &subject, XP_Bool withRe, MSG_DBHandle db)
{
	char *subjectStr;
	XP_Bool ret = MSG_HeaderHandle_GetSubject(m_dbHeaderHandle, &subjectStr, withRe, db);
	subject.SetStrPtr(subjectStr);
	return ret;
}

XP_Bool DBMessageHdr::GetAuthor(XPStringObj &author, MSG_DBHandle db)
{
	char *authorStr;
	XP_Bool ret = MSG_HeaderHandle_GetAuthor(m_dbHeaderHandle, &authorStr, db);
	author.SetStrPtr(authorStr);
	return ret;
}

XP_Bool DBMessageHdr::GetRFC822Author(XPStringObj &author, MSG_DBHandle db)
{
	XP_Bool ret = GetAuthor(author, db);
	if (ret)
	{
		char *rfc822Sender;
		rfc822Sender = MSG_ExtractRFC822AddressName (author);
		if (rfc822Sender) 
		{
			author = rfc822Sender;
			FREEIF(rfc822Sender);
		}
		return TRUE;
	}
	return FALSE;
}

// copy the subject back into the passed buffer.
void DBMessageHdr::GetSubject(char *outSubject, int subjectLen, XP_Bool withRe, MSG_DBHandle db )
{
	XPStringObj subjectStr;

	GetSubject(subjectStr, withRe, db);
	*outSubject = '\0';
	if ((const char *) subjectStr != NULL)
	{
		// Fix problem of going one over the real length of the array
		// for source strings that are longer than the destination.
		if (subjectLen <= (int) XP_STRLEN(subjectStr))
			subjectLen --;

		XP_STRNCAT_SAFE(outSubject, subjectStr, subjectLen);
	}
#ifdef DEBUG_bienvenu
	else
		XP_ASSERT(FALSE);
#endif
}
// copy the messageid back into the passed buffer.
void DBMessageHdr::GetMessageId(char *outMessageId, int messageIdLen, MSG_DBHandle db)
{
	XPStringObj messageIdStr;
	GetMessageId(messageIdStr, db);
	if ((const char *) messageIdStr != NULL)
		XP_STRNCPY_SAFE(outMessageId, messageIdStr, messageIdLen);
#ifdef DEBUG_bienvenu
	else
		XP_ASSERT(FALSE);
#endif
}
// copy the author back into the passed buffer.
void DBMessageHdr::GetAuthor(char *outAuthor, int authorLen, MSG_DBHandle db /* = NULL */)
{
	XPStringObj authorStr;
	GetAuthor(authorStr, db);
	if ((const char* ) authorStr != NULL)
		XP_STRNCPY_SAFE(outAuthor, authorStr, authorLen);
#ifdef DEBUG_bienvenu
	else
		XP_ASSERT(FALSE);
#endif
}
// copy the author back into the passed buffer.
void DBMessageHdr::GetRFC822Author(char *outAuthor, int authorLen, MSG_DBHandle db /* = NULL */)
{
	XPStringObj authorStr;
	GetAuthor(authorStr, db);
	if ((const char *) authorStr != NULL)
	{
		char *rfc822Sender;
		rfc822Sender = MSG_ExtractRFC822AddressName (authorStr);
		if (rfc822Sender) 
		{
			XP_STRNCPY_SAFE(outAuthor, rfc822Sender, authorLen);
			FREEIF(rfc822Sender);
		}
	}
#ifdef DEBUG_bienvenu1
	else
		XP_ASSERT(FALSE);
#endif
}

void DBMessageHdr::GetMailboxOfAuthor(char *outAuthor, int authorLen, MSG_DBHandle db /* = NULL */)
{
	XPStringObj authorStr;
	GetAuthor(authorStr, db);
	if ((const char *) authorStr != NULL)
	{
		char *rfc822Sender;
		rfc822Sender = MSG_ExtractRFC822AddressMailboxes (authorStr);
		if (rfc822Sender) 
		{
			XP_STRNCPY_SAFE(outAuthor, rfc822Sender, authorLen);
			FREEIF(rfc822Sender);
		}
	}
#ifdef DEBUG_bienvenu
	else
		XP_ASSERT(FALSE);
#endif
}

int32		DBMessageHdr::GetNumReferences()
{
	return MSG_HeaderHandle_GetNumReferences(GetHandle());
}


// get the next <> delimited reference from nextRef and copy it into reference,
// which is a pointer to a buffer at least kMaxMsgIdLen long.
const char * DBMessageHdr::GetReference(const char *nextRef, char *reference)
{
	const char *ptr = nextRef;

	while ((*ptr == '<' || *ptr == ' ') && *ptr)
		ptr++;

	for (int i = 0; *ptr && *ptr != '>' && i < kMaxMsgIdLen; i++)
		*reference++ = *ptr++;

	if (*ptr == '>')
		ptr++;
	*reference = '\0';
	return ptr;
}
// Get previous <> delimited reference - used to go backwards through the
// reference string. Caller will need to make sure that prevRef is not before
// the start of the reference string when we return.
const char *DBMessageHdr::GetPrevReference(const char *prevRef, char *reference)
{
	const char *ptr = prevRef;

	while ((*ptr == '>' || *ptr == ' ') && *ptr)
		ptr--;

	// scan back to '<'
	for (int i = 0; *ptr && *ptr != '<' ; i++)
		ptr--;

	GetReference(ptr, reference);
	if (*ptr == '<')
		ptr--;
	return ptr;
}

// If the m_messageIdID string in the hash table has a ref count
// greater than 1, the presumption is another header must
// refer to it.
XP_Bool DBMessageHdr::IsReferredTo(MSG_DBHandle db)
{
	XP_Bool isReferredTo = FALSE;
	// DMB TODO
#if 0
	if (m_messageIdID != 0)
	{
		HashString *hashString = HashString::GetHashString(m_messageIdID, kMessageIdStringID, db);
		if (hashString)
		{
			if (hashString->GetRefCount() > 1)
				isReferredTo = TRUE;
			hashString->unrefer();
		}
	}
#endif
	return isReferredTo;
}

// Does this hdr refer to passed in hdr? I.e., should we be a child
// of the passed in hdr?
XP_Bool		DBMessageHdr::RefersTo(DBMessageHdr *hdr)
{
	// if the hash string id of the passed in hdr is equal to one of the
	// hash string id's in our reference id array, then we refer to hdr.
	// DMB TODO
	return FALSE;

}


void DBMessageHdr::PurgeOfflineMessage(MSG_DBHandle  /* db */)
{
	// ### dmb could move the news and imap overrides into here
}


