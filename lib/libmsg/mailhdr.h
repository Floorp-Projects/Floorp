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
// mail message header - subclass of DBMessageHdr
#ifndef MAIL_HDR_H_
#define MAIL_HDR_H_

#include "msghdr.h"

class NewsMessageHdr;

class MailMessageHdr : public DBMessageHdr
{
public:
						/** Instance Methods **/
	MailMessageHdr();
	MailMessageHdr(MSG_HeaderHandle handle);
	~MailMessageHdr();

	virtual void		CopyFromMsgHdr(MailMessageHdr *msgHdr, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);
	virtual void		CopyFromMsgHdr(NewsMessageHdr *msgHdr, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);
	virtual void		CopyFromMsgHdr(DBMessageHdr *msgHdr, MSG_DBHandle srcDBHandle, MSG_DBHandle destDBHandle);

						/** Accessor Methods **/
	virtual	void		SetPriority(MSG_PRIORITY priority);
			void		SetPriority(const char *priority);
			MSG_PRIORITY GetPriority();
			void		SetByteLength(uint32 byteLength);
			uint32		GetByteLength() {return m_byteLength;}
			void		SetStatusOffset(uint16 statusOffset);
			uint16		GetStatusOffset();
			void		SetRecipients(const char *recipients, MSG_DBHandle db, XP_Bool rfc822 = TRUE);
			void		SetCCList(const char *cclist, MSG_DBHandle db);
			virtual int32 GetNumRecipients();
			virtual int32 GetNumCCRecipients();
			virtual void GetRecipients(XPStringObj &recipient, MSG_DBHandle dbHandle);
			virtual void GetCCList(XPStringObj &ccList, MSG_DBHandle dbHandle);
			virtual void GetFullRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle);
			virtual void GetFullCCRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle);
			virtual void GetNameOfRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle);
			virtual void GetMailboxOfRecipient(XPStringObj &recipient, int whichRecipient, MSG_DBHandle dbHandle);
			virtual void GetNameOfCCRecipient(XPStringObj & recipient, int whichRecipient, MSG_DBHandle dbHandle);
			virtual void GetMailboxOfCCRecipient(XPStringObj & recipient, int whichRecipient, MSG_DBHandle dbHandle);

			virtual uint32 GetLineCount() {return m_numLines;}
			virtual void	SetLineCount(uint32 lineCount) {m_numLines = lineCount;}

			int32 AddToOfflineMessage(const char *block, int32 length, MSG_DBHandle dbHandle); 
			int32 ReadFromOfflineMessage(char *block, int32 length, int32 offset, MSG_DBHandle dbHandle);
			void  PurgeOfflineMessage(MSG_DBHandle dbHandle);
			int32 GetOfflineMessageLength(MSG_DBHandle dbHandle); 
			uint32  WriteOfflineMessage(XP_File destinationFile,MSG_DBHandle dbHandle, XP_Bool needEnvelope = TRUE);	// returns bytes written

protected:
		
			
	unsigned char m_priority;
	uint16		m_statusOffset;	// carried over from Moz 2.0 summary files
	uint32		m_byteLength;   // ditto
	uint32		m_numLines;
};


#endif
