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

#ifndef _MsgHdr_H_
#define _MsgHdr_H_

const int kMaxSubject = 160;
const int kMaxAuthor = 160;
const int kMaxRecipient = 80;
const int kMaxMsgIdLen = 80;
const int kMaxReferenceLen = 10 * kMaxMsgIdLen;

typedef int32 MsgFlags;	// try to keep flags view is going to care about
						// in the first byte, so we can copy the flags over directly

const int32	kIsRead			= 0x1;	// same as MSG_FLAG_READ
const int32	kReplied		= 0x2;	// same as MSG_FLAG_REPLIED
const int32	kMsgMarked		= 0x4;	// same as MSG_FLAG_MARKED - (kMarked collides with IMAP)
const int32	kHasChildren	= 0x8;  // no equivalent (shares space with FLAG_EXPUNGED)
const int32	kIsThread		= 0x10; // !!shares space with MSG_FLAG_HAS_RE
const int32	kElided			= 0x20; // same as MSG_FLAG_ELIDED
const int32	kExpired		= 0x40; // same as MSG_FLAG_EXPIRED
const int32	kOffline		= 0x80; // this message has been downloaded
const int32	kWatched		= 0x100;
const int32	kSenderAuthed	= 0x200; // same as MSG_FLAG_SENDER_AUTHED
const int32	kExpunged		= 0x400;  // NOT same value as MSG_FLAG_EXPUNGED
const int32	kHasRe			= 0x800;  // Not same values as MSG_FLAG_HAS_RE
const int32	kForwarded		= 0x1000; // this message has been forward (mail only)
const int32	kIgnored		= 0x2000; // this message is ignored

const int32	kMDNNeeded		= 0x4000; // this message needs MDN report
const int32 kMDNSent		= 0x8000; // MDN report has been Sent

const int32 kNew			= 0x10000; // used for status, never stored in db.
const int32	kAdded			= 0x20000; // Just added to db - used in 
							  // notifications, never set in msgHdr.

const int32 kTemplate       = 0x40000; // this message is a template

const int32 kDirty			= 0x80000;
const int32 kPartial		= 0x100000;	// NOT same value as MSG_FLAG_PARTIAL
const int32 kIMAPdeleted	= 0x200000;	// same value as MSG_FLAG_IMAP_DELETED
const int32 kHasAttachment	= 0x10000000;	// message has attachments

const int32	kSameAsMSG_FLAG  = kHasAttachment|kIsRead|kMsgMarked|kExpired|kElided|kSenderAuthed|kReplied|kOffline|kForwarded|kWatched|kIMAPdeleted;
const int32	kMozillaSameAsMSG_FLAG  = kIsRead|kMsgMarked|kExpired|kElided|kSenderAuthed|kReplied|kOffline|kForwarded|kWatched|kIMAPdeleted;
const int32	kExtraFlags		= 0xFF;

struct MessageHdrStruct
{
	MessageKey   m_threadId; 
	MessageKey	m_messageKey; 	
	char		m_subject[kMaxSubject];
	char		m_author[kMaxAuthor];
	char		m_messageId[kMaxMsgIdLen];
	char		m_references[kMaxReferenceLen]; 
	char		m_recipients[kMaxRecipient];
	time_t 		m_date;                         
	uint32		m_messageSize;	// lines for news articles, bytes for mail messages
	uint32		m_flags;
	uint16		m_numChildren;		// for top-level threads
	uint16		m_numNewChildren;	// for top-level threads
	char		m_level;		// indentation level
	MSG_PRIORITY m_priority;
public:
	void SetSubject(const char * subject);
	void SetAuthor(const char * author);
	void SetMessageID(const char * msgID);
	void SetReferences(const char * referencesStr);
	void SetDate(const char * date);
	void SetLines(uint32 lines);
	void SetSize(uint32 size);
	const char *GetReference(const char *nextRef, char *reference);
	static void StripMessageId(const char *msgID, char *outMsgId, int msgIdLen);
};

#endif _MsgHdr_H_
