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
#include "mailhdr.h"
#include "maildb.h"
#include "newshdr.h"
#include "imapoff.h"
#include "msg_srch.h" // for MSG_GetPriorityFromString
#include "msgdbapi.h"
#include "msgstrob.h"

/* ****************************************************************** */
						/** MailMessageHdr Class **/
/* ****************************************************************** */
MailMessageHdr::MailMessageHdr()
{
	m_priority = 0;
	m_byteLength = 0;
	m_statusOffset = 0;
	m_numLines = 0;
	if (!m_dbHeaderHandle)
		m_dbHeaderHandle = GetNewMailHeaderHandle();
}

MailMessageHdr::MailMessageHdr(MSG_HeaderHandle handle) : DBMessageHdr(handle)
{
	m_priority = 0;
	m_byteLength = 0;
	m_statusOffset = 0;
	m_numLines = 0;
}

MailMessageHdr::~MailMessageHdr()
{
}

MSG_PRIORITY MailMessageHdr::GetPriority()
{
	m_priority = MSG_HeaderHandle_GetPriority(GetHandle());
	switch (m_priority) {
		case MSG_PriorityNotSet:
		case MSG_NoPriority:
			return MSG_NormalPriority;
		default:
			return (MSG_PRIORITY) m_priority;
	}
}

void MailMessageHdr::CopyFromMsgHdr(MailMessageHdr *msgHdr, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle)
{
	MSG_HeaderHandle_CopyFromMailMsgHdr(m_dbHeaderHandle, msgHdr->m_dbHeaderHandle, srcDBHandle, destDBHandle);
}

void MailMessageHdr::CopyFromMsgHdr(NewsMessageHdr *msgHdr, MSG_DBHandle srcDB, MSG_DBHandle destDB)
{
	DBMessageHdr::CopyFromDBMsgHdr(msgHdr, srcDB, destDB);
}

void MailMessageHdr::CopyFromMsgHdr(DBMessageHdr *msgHdr, MSG_DBHandle srcDB, MSG_DBHandle destDB)
{
	DBMessageHdr::CopyFromDBMsgHdr(msgHdr, srcDB, destDB);
}

void MailMessageHdr::SetPriority(MSG_PRIORITY priority)
{
	if (priority < 0 || priority > MSG_HighestPriority)
	{
		priority = MSG_NoPriority;
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
	}
	m_priority = (unsigned char) priority;
	MSG_HeaderHandle_SetPriority(GetHandle(), (MSG_PRIORITY) m_priority);
}

void		MailMessageHdr::SetPriority(const char *priority)
{
	m_priority = MSG_GetPriorityFromString(priority);
}


void MailMessageHdr::SetByteLength(uint32 byteLength)
{
	m_byteLength = byteLength;
	MSG_HeaderHandle_SetByteLength(GetHandle(), byteLength);
}

void MailMessageHdr::SetStatusOffset(uint16 statusOffset)
{
	m_statusOffset = statusOffset;
	MSG_HeaderHandle_SetStatusOffset(GetHandle(), statusOffset);
}

uint16 MailMessageHdr::GetStatusOffset()
{
	return MSG_HeaderHandle_GetStatusOffset(GetHandle());
}

void MailMessageHdr::SetRecipients(const char *recipients, MSG_DBHandle dbHandle, XP_Bool rfc822)
{
	MSG_HeaderHandle_SetRecipients(m_dbHeaderHandle, recipients, dbHandle, rfc822);
}


int32		MailMessageHdr::GetNumRecipients()
{
	return MSG_HeaderHandle_GetNumRecipients(m_dbHeaderHandle);
}

int32		MailMessageHdr::GetNumCCRecipients()
{
	return MSG_HeaderHandle_GetNumCCRecipients(m_dbHeaderHandle);
}

void MailMessageHdr::GetRecipients(XPStringObj &recipient, MSG_DBHandle dbHandle)
{
	char	*addressList;
	MSG_HeaderHandle_GenerateAddressList(m_dbHeaderHandle, dbHandle, &addressList);

	recipient.SetStrPtr(addressList);
}

void MailMessageHdr::GetCCList(XPStringObj &ccList, MSG_DBHandle dbHandle)
{
	char	*addressList;
	MSG_HeaderHandle_GenerateCCAddressList(m_dbHeaderHandle, dbHandle, &addressList);

	ccList.SetStrPtr(addressList);
}

void MailMessageHdr::GetFullRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	char	*address;
	MSG_HeaderHandle_GetFullRecipient(m_dbHeaderHandle, dbHandle, whichRecipient, &address);
	recipient.SetStrPtr(address);
}

void MailMessageHdr::GetFullCCRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	char	*address;
	MSG_HeaderHandle_GetFullCCRecipient(m_dbHeaderHandle, dbHandle, whichRecipient, &address);
	recipient.SetStrPtr(address);
}

void MailMessageHdr::GetNameOfRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	XPStringObj	fullRecipient;

	if (GetNumRecipients() != 0)
	{
		GetFullRecipient(fullRecipient, whichRecipient, dbHandle);
		char *name = MSG_ExtractRFC822AddressNames(fullRecipient);
		if (name != NULL)
		{
			recipient = name;
			XP_FREE(name);
			return;
		}
	}
	recipient = "";
}

void MailMessageHdr::GetMailboxOfRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	XPStringObj	fullRecipient;

	if (GetNumRecipients() != 0)
	{
		GetFullRecipient(fullRecipient, whichRecipient, dbHandle);
		char *mailbox = MSG_ExtractRFC822AddressMailboxes(fullRecipient);
		if (mailbox != NULL)
		{
			recipient = mailbox;
			XP_FREE(mailbox);
			return;
		}
	}
	recipient = "";
}

void MailMessageHdr::GetNameOfCCRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	XPStringObj	fullRecipient;

	if (GetNumCCRecipients() != 0)
	{
		GetFullCCRecipient(fullRecipient, whichRecipient, dbHandle);
		char *name = MSG_ExtractRFC822AddressNames(fullRecipient);
		if (name != NULL)
		{
			recipient = name;
			XP_FREE(name);
			return;
		}
	}
	recipient = "";
}

void MailMessageHdr::GetMailboxOfCCRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle)
{
	XPStringObj	fullRecipient;

	if (GetNumCCRecipients() != 0)
	{
		GetFullCCRecipient(fullRecipient, whichRecipient, dbHandle);
		char *mailbox = MSG_ExtractRFC822AddressMailboxes(fullRecipient);
		if (mailbox != NULL)
		{
			recipient = mailbox;
			XP_FREE(mailbox);
			return;
		}
	}
	recipient = "";
}


void MailMessageHdr::SetCCList(const char *cclist, MSG_DBHandle dbHandle)
{
	MSG_HeaderHandle_SetCCRecipients(m_dbHeaderHandle, cclist, dbHandle);
}

int32 MailMessageHdr::AddToOfflineMessage(const char *block, int32 length, MSG_DBHandle dbHandle)
{
	return MSG_HeaderHandle_AddToOfflineMessage(m_dbHeaderHandle, block, length, dbHandle);
}

int32 MailMessageHdr::ReadFromOfflineMessage(char *block, int32 length, int32 offset, MSG_DBHandle dbHandle)
{
	return MSG_HeaderHandle_ReadFromOfflineMessage(m_dbHeaderHandle, block, length, offset, dbHandle);
}

void MailMessageHdr::PurgeOfflineMessage(MSG_DBHandle dbHandle)
{
	MSG_HeaderHandle_PurgeOfflineMessage(m_dbHeaderHandle, dbHandle);
}


uint32  MailMessageHdr::WriteOfflineMessage(XP_File	destinationFile,
											MSG_DBHandle dbHandle,
											XP_Bool needEnvelope)
{
	uint32 bytesAttempted = 0;
	uint32 filedBytes = 0;
	XP_FileSeek(destinationFile, 0, SEEK_END);
	
	// if writing to a local mail folder
	// this message needs a Berkely envlope
	if (needEnvelope)
	{
		char *envelope = msg_GetDummyEnvelope();  // not allocated, do not free
		if (envelope)
		{
			bytesAttempted += XP_STRLEN(envelope);
			filedBytes += XP_FileWrite(envelope, XP_STRLEN(envelope), destinationFile);
		}
	}
	
	if (filedBytes == bytesAttempted)
	{
		// this message needs an X-MOZILLA_STATUS line
		char status_buf[X_MOZILLA_STATUS_LEN + 10];
		PR_snprintf(status_buf, X_MOZILLA_STATUS_LEN + 10,
						X_MOZILLA_STATUS_FORMAT LINEBREAK, GetFlags());
		bytesAttempted += XP_STRLEN(status_buf);
		filedBytes += XP_FileWrite(status_buf, XP_STRLEN(status_buf), destinationFile);

		uint32 dbFlags = GetFlags();
		char *status2_buf = NULL;
		MessageDB::ConvertDBFlagsToPublicFlags(&dbFlags);
		dbFlags &= (MSG_FLAG_MDN_REPORT_NEEDED | MSG_FLAG_MDN_REPORT_SENT | MSG_FLAG_TEMPLATE);
		status2_buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT LINEBREAK, dbFlags);
		if (status2_buf)
		{
			bytesAttempted += XP_STRLEN(status2_buf);
			filedBytes += XP_FileWrite(status2_buf, XP_STRLEN(status2_buf), destinationFile);
			XP_FREEIF(status2_buf);
		}

		int32 bytesWritten = MSG_HeaderHandle_WriteOfflineMessageBody(m_dbHeaderHandle, dbHandle, destinationFile);
		//DMB TODO - not quite right...do we need to know how many bytes we tried to write?
		filedBytes += bytesWritten;
		bytesAttempted += bytesWritten;
	}		
	return (filedBytes == bytesAttempted) ? filedBytes : 0;
}

int32 MailMessageHdr::GetOfflineMessageLength(MSG_DBHandle dbHandle) 
{
	return MSG_HeaderHandle_GetOfflineMessageLength(m_dbHeaderHandle, dbHandle);
}

