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

#include "msgmdn.h"
#include "net.h"
#include "xp_time.h"
#include "prtime.h"
#include "prtypes.h"
#include "fe_proto.h"
#include "msg.h"
#include "msgfinfo.h"
#include "msgimap.h"
#include "msgpane.h"
#include "prefapi.h"
#include "intl_csi.h"
#include "msgurlq.h"
#include "gui.h"
#include "msgprefs.h"
#include "mkutils.h"
#include "mktcp.h"
#include "netutils.h"

extern "C"
{
	extern int MK_MSG_MDN_DISPLAYED;
	extern int MK_MSG_MDN_DISPATCHED;
	extern int MK_MSG_MDN_PROCESSED;
	extern int MK_MSG_MDN_DELETED;
	extern int MK_MSG_MDN_DENIED;
	extern int MK_MSG_MDN_FAILED;
	extern int MK_MSG_MDN_WISH_TO_SEND;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_DELIV_MAIL;
	extern int MK_MDN_DISPLAYED_RECEIPT;
	extern int MK_MDN_DISPATCHED_RECEIPT;
	extern int MK_MDN_PROCESSED_RECEIPT;
	extern int MK_MDN_DELETED_RECEIPT;
	extern int MK_MDN_DENIED_RECEIPT;
	extern int MK_MDN_FAILED_RECEIPT;
}

extern "C" char *mime_make_separator(const char *prefix);
extern "C" char *msg_generate_message_id();
extern "C" char *strip_continuations(char *original);

#define MH_ALLOC_COPY(dst, src) \
	do { dst = (char *) XP_ALLOC(src->length+1); *(dst+src->length)=0; \
		 if (src->length) XP_MEMCPY(dst, src->value, src->length); } while (0)
#define PUSH_N_FREE_STRING(p) \
	do { if (p) { status = WriteString(p); PR_smprintf_free(p); p=0; \
				   if (status<0) return status; } \
		 else { return MK_OUT_OF_MEMORY; } } while (0)

char DispositionTypes[7][16] = {
	"displayed",
	"dispatched",
	"processed",
	"deleted",
	"denied",
	"failed",
	""
};

MSG_ProcessMdnNeededState::MSG_ProcessMdnNeededState(
	EDisposeType intendedType,
	MSG_Pane *pane,
	MSG_FolderInfo *folder,
	uint32 key,
	MimeHeaders *srcHeaders,
	XP_Bool autoAction)
{
	XP_ASSERT (srcHeaders && pane);
	if (!srcHeaders || !pane) return;
	m_disposeType = intendedType;
	m_pane = pane;
	m_autoAction = autoAction;

	// *** no matter what happened with sending MDN report set the IMAP
	// MDNSent flag first.
	StoreImapMDNSentFlag(folder, key);

	m_returnPath = MimeHeaders_get (srcHeaders, HEADER_RETURN_PATH, FALSE,
									FALSE);
	m_dispositionNotificationTo = 
		MimeHeaders_get(srcHeaders, HEADER_DISPOSITION_NOTIFICATION_TO, FALSE,
						FALSE); 
	if (!m_dispositionNotificationTo)
		m_dispositionNotificationTo =
			MimeHeaders_get(srcHeaders, HEADER_RETURN_RECEIPT_TO, FALSE,
							FALSE); 
	m_date = MimeHeaders_get (srcHeaders, HEADER_DATE, FALSE, FALSE);
	m_to = MimeHeaders_get (srcHeaders, HEADER_TO, FALSE, FALSE);
	m_cc = MimeHeaders_get (srcHeaders, HEADER_CC, FALSE, FALSE);
	m_subject = MimeHeaders_get (srcHeaders, HEADER_SUBJECT, FALSE, FALSE);
	if (m_subject) strip_continuations(m_subject);
	m_messageId = MimeHeaders_get (srcHeaders, HEADER_MESSAGE_ID, FALSE,
								   FALSE);
	m_originalRecipient = MimeHeaders_get (srcHeaders,
										   HEADER_ORIGINAL_RECIPIENT, FALSE,
										   FALSE);

	m_all_headers = (char *) XP_ALLOC(srcHeaders->all_headers_fp + 1);
	*(m_all_headers+srcHeaders->all_headers_fp)=0;
	m_all_headers_size = srcHeaders->all_headers_fp;
	XP_MEMCPY(m_all_headers, srcHeaders->all_headers,
			  srcHeaders->all_headers_fp); 
	InitAndProcess();
}

MSG_ProcessMdnNeededState::MSG_ProcessMdnNeededState(
	EDisposeType intendedType,
	MSG_Pane *pane,
	MSG_FolderInfo *folder,
	uint32 key,
	struct message_header *returnPath,
	struct message_header *dnt,
	struct message_header *to,
	struct message_header *cc,
	struct message_header *subject,
	struct message_header *date,
	struct message_header *originalRecipient,
	struct message_header *messageId,
	char *allHeaders,
	int32 allHeadersSize,
	XP_Bool autoAction)
{
	XP_ASSERT (pane);
	if (!pane) return;
	m_disposeType = intendedType;
	m_pane = pane;
	m_autoAction = autoAction;

	// *** no matter what happened with sending MDN report set the IMAP
	// MDNSent flag first.
	StoreImapMDNSentFlag(folder, key);

	MH_ALLOC_COPY (m_returnPath, returnPath);
	MH_ALLOC_COPY (m_dispositionNotificationTo, dnt);
	MH_ALLOC_COPY (m_to, to);
	MH_ALLOC_COPY (m_cc, cc);
	MH_ALLOC_COPY (m_date, date);
	MH_ALLOC_COPY (m_subject, subject);
	MH_ALLOC_COPY (m_originalRecipient, originalRecipient);
	MH_ALLOC_COPY (m_messageId, messageId);

	m_all_headers = (char *) XP_ALLOC (allHeadersSize+1);
	*(m_all_headers+allHeadersSize) = 0;
	m_all_headers_size = allHeadersSize;
	XP_MEMCPY(m_all_headers, allHeaders, allHeadersSize);
	InitAndProcess();
}

void MSG_ProcessMdnNeededState::InitAndProcess()
{
	XP_Bool mdnEnabled = FALSE;

	m_outFile = 0;
	m_csid = 0;
	m_msgFileName = 0;
	m_mimeSeparator = 0;
	m_autoSend = FALSE;
	m_reallySendMdn = FALSE;
	
	PREF_GetBoolPref("mail.mdn.report.enabled", &mdnEnabled);

	if (m_dispositionNotificationTo &&
		mdnEnabled && 
		ProcessSendMode() &&
		ValidateReturnPath())
		CreateMdnMsg();
}

int32 MSG_ProcessMdnNeededState::OutputAllHeaders()
{	
	XP_ASSERT(m_all_headers && m_all_headers_size);

    // **** this is disgusting; I don't have a better way to deal with it 
	char *buf = m_all_headers, *buf_end =
		m_all_headers+m_all_headers_size;
	char *start = buf, *end = buf;
	int32 count = 0, ret = 0;
	
	while (buf < buf_end)
	{
		switch (*buf)
		{
		case 0:
			if (*(buf+1) == LF)
			{
				// *buf = CR;
				end = buf;
			}
			else if (*(buf+1) == 0)
			{
				// the case of message id
				*buf = '>';
			}
			break;
		case CR:
			end = buf;
			*buf = 0;
			break;
		case LF:
			if (buf > start && *(buf-1) == 0)
			{
				start = buf + 1;
				end = start;
			}
			else
			{
				end = buf;
			}
			break;
		}
		buf++;
		
		if (end > start && (*end == LF || !*end))
		{
			// strip out private X-Mozilla-Status header & X-Mozilla-Draft-Info
			if (!XP_STRNCASECMP(start, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN) ||
				!XP_STRNCASECMP(start, X_MOZILLA_DRAFT_INFO, X_MOZILLA_DRAFT_INFO_LEN))
			{ 
				// make sure we are also copying the last null terminated char
				// XP_MEMCPY(start, end+2, (buf_end+1) - (end+2));
				// buf_end -= (end+2 - start);
				// m_all_headers_size -= (end+2 - start);
				if (*end == LF)
					start = end+1;
				else
					start = end+2;
			}
			else
			{
				XP_Bool endIsLF = *end == LF;
				if (endIsLF)
					*end = 0;
				char *wrapped_string = (char *) 
					XP_WordWrap(m_csid, (unsigned char *) start, 72, FALSE);
				if (wrapped_string)
				{
					ret = WriteString(wrapped_string);
					if (ret < 0) return ret;
					count += ret;
					XP_FREEIF(wrapped_string);
					ret = WriteString(CRLF);
					if (ret < 0) return ret;
					count += ret;
				}
				if (endIsLF)
					start = end+1;
				else 
					start = end+2;
			}
			buf = start;
			end = start;
		}
	}
	return count;
}


MSG_ProcessMdnNeededState::~MSG_ProcessMdnNeededState()
{
	if (m_outFile)
	{
		XP_FileClose(m_outFile);
		m_outFile = 0;
	}

	if (m_msgFileName)
	{
		XP_FileRemove(m_msgFileName, xpFileToPost);
		XP_FREEIF(m_msgFileName);
	}

	XP_FREEIF(m_mimeSeparator);
	XP_FREEIF(m_returnPath);
	XP_FREEIF(m_dispositionNotificationTo);
	XP_FREEIF(m_to);
	XP_FREEIF(m_cc);
	XP_FREEIF(m_subject);
	XP_FREEIF(m_date);
	XP_FREEIF(m_originalRecipient);
	XP_FREEIF(m_messageId);
	XP_FREEIF(m_all_headers);
}

int32
MSG_ProcessMdnNeededState::WriteString( const char *str )
{
	XP_ASSERT (str);
	if (!str) return 0;

	int32 len = XP_STRLEN(str);
	
	return XP_FileWrite(str, len, m_outFile);
}

	
XP_Bool
MSG_ProcessMdnNeededState::MailAddrMatch( const char *addr1, const char *addr2)
{
	XP_Bool isMatched = TRUE;
	const char *atSign1 = NULL, *atSign2 = NULL;
	const char *lt = NULL, *local1 = NULL, *local2 = NULL;
	const char *end1 = NULL, *end2 = NULL;

	if (!addr1 || !addr2)
		return FALSE;

	lt = XP_STRCHR(addr1, '<');
	if (!lt)
		local1 = addr1;
	else
		local1 = lt+1;

	lt = XP_STRCHR(addr2, '<');
	if (!lt)
		local2 = addr2;
	else
		local2 = lt+1;

	end1 = XP_STRCHR(local1, '>');
	if (!end1)
		end1 = addr1 + XP_STRLEN(addr1);

	end2 = XP_STRCHR(local2, '>');
	if (!end2)
		end2 = addr2 + XP_STRLEN(addr2);

	atSign1 = XP_STRCHR(local1, '@');
	atSign2 = XP_STRCHR(local2, '@');
	if (!atSign1 || !atSign2 ||	  // ill formed addr-spec
		(atSign1 - local1) != (atSign2 - local2))
	{
		isMatched = FALSE;
	}
	else if (XP_STRNCMP(local1, local2, (atSign1-local1)))
	{	// case sensitive compare for local part
		isMatched = FALSE;
	}
	else if ((end1 - atSign1) != (end2 - atSign2) ||
		XP_STRNCASECMP(atSign1, atSign2, (end1 - atSign1)))
	{	// case insensitive compare for domain part
		isMatched = FALSE;
	}
	return isMatched;
}

XP_Bool
MSG_ProcessMdnNeededState::ValidateReturnPath()
{
	// ValidateReturnPath applies to Automatic Send Mode only
	// if we were in manual mode by pass this check
	if (!m_autoSend)
		return m_reallySendMdn;
	
	if (!m_returnPath || !m_dispositionNotificationTo ||
		!*m_returnPath || !*m_dispositionNotificationTo)
	{
		m_autoSend = FALSE;
		goto done;
	}
	
	m_autoSend = MailAddrMatch(m_returnPath, m_dispositionNotificationTo);

done:
	return m_reallySendMdn;
}

XP_Bool
MSG_ProcessMdnNeededState::NotInToOrCc()
{
	XP_ASSERT(m_pane);
	msg_StringArray to_cc_list (TRUE);
	MSG_Prefs *prefs = m_pane->GetPrefs();
	XP_ASSERT(prefs);
	char *reply_to = NULL;
	char *to = m_to ? MSG_ExtractRFC822AddressMailboxes(m_to) : (char *)NULL;
	char *cc = m_cc ? MSG_ExtractRFC822AddressMailboxes(m_cc) : (char *)NULL;
	const char *usr_addr = FE_UsersMailAddress();
	int i, size;
	XP_Bool bRet = FALSE;
	
	// start with a simple check
	if (XP_STRCASESTR(m_to, usr_addr) || XP_STRCASESTR(m_cc, usr_addr))
		goto done;

	PREF_CopyCharPref("mail.identity.reply_to", &reply_to);

	to_cc_list.ImportTokenList(to);
	to_cc_list.ImportTokenList(cc);

	for (i=0, size=to_cc_list.GetSize(); i < size; i++)
	{
		const char *addr = to_cc_list.GetAt(i);

		if (prefs->IsEmailAddressAnAliasForMe(addr) ||
			(reply_to && XP_STRCASESTR(reply_to, addr)))
			goto done;
	}
	bRet = TRUE;

done:
	XP_FREEIF(to);
	XP_FREEIF(cc);
	XP_FREEIF(reply_to);
	return bRet;
}

XP_Bool
MSG_ProcessMdnNeededState::ProcessSendMode()
{
	const char *user_addr = FE_UsersMailAddress();
	const char *localDomain = NULL;
	char *prefDomain = NULL;
	int miscState = 0;
	int32 intPref;

	XP_ASSERT(user_addr);
	if (!user_addr) 
		return m_reallySendMdn;

	PREF_CopyCharPref("mail.identity.defaultdomain", &prefDomain);
	
	
	localDomain = XP_STRCHR(user_addr, '@');

	if (prefDomain && *prefDomain)
	{
		if (!XP_STRCASESTR(m_dispositionNotificationTo, prefDomain))
			miscState |= MDN_OUTSIDE_DOMAIN;
		XP_FREEIF(prefDomain);
	}
	else if (localDomain)
	{
		localDomain++; // advance after @ sign
		if (!XP_STRCASESTR(m_dispositionNotificationTo, localDomain))
			miscState |= MDN_OUTSIDE_DOMAIN;
	}
	if (NotInToOrCc())
	{
		miscState |= MDN_NOT_IN_TO_CC;
	}
	m_reallySendMdn = TRUE;
	// *********
	// How are we gona deal with the auto forwarding issues? Some server
	// didn't bother to add addition header or modify existing header to the
	// message when forwarding. They simply copy the exact same message to
	// another user's mailbox. Some change To: to Apparently-To:
	// *********
	// starting from lowest denominator to highest
	if (!miscState)
	{	// under normal situation: recipient is in to and cc
		// list, sender is from the same domain
		intPref = 0;
		PREF_GetIntPref("mail.mdn.report.other", &intPref);
		switch (intPref)
		{
		default:
		case 0:
			m_reallySendMdn = FALSE;
			break;
		case 1:
			m_autoSend = TRUE;
			break;
		case 2:
			m_autoSend = FALSE;
			break;
		case 3:
			m_autoSend = TRUE;
			m_disposeType = eDenied;
			break;
		}
#ifdef CHECK_SENDER_AND_USER_ARE_SAME
		// original sender is same as current user; doesn't make sense
		if (m_reallySendMdn && 
			MailAddrMatch(m_dispositionNotificationTo, user_addr))
			m_reallySendMdn = FALSE;
#endif
	}
	else if (miscState == (MDN_OUTSIDE_DOMAIN | MDN_NOT_IN_TO_CC))
	{
		int32 intPref2 = 0;
		intPref = 0;
		PREF_GetIntPref("mail.mdn.report.outside_domain", &intPref);
		PREF_GetIntPref("mail.mdn.report.not_in_to_cc", &intPref2);
		if (intPref != intPref2)
		{
			m_autoSend = FALSE;	// ambiguous; always ask
		}
		else
		{
			switch (intPref)
			{
			default:
			case 0:
				m_reallySendMdn = FALSE;
				break;
			case 1:
				m_autoSend = TRUE;
				break;
			case 2:
				m_autoSend = FALSE;
				break;
			}
		}
	}
	else if (miscState & MDN_OUTSIDE_DOMAIN)
	{
		intPref = 0;          // reset int pref to 0
		PREF_GetIntPref("mail.mdn.report.outside_domain", &intPref);
		switch (intPref)
		{
		default:
		case 0:
			m_reallySendMdn = FALSE;
			break;
		case 1:
			m_autoSend = TRUE;
			break;
		case 2:
			m_autoSend = FALSE;
			break;
		}
	}
	else if (miscState & MDN_NOT_IN_TO_CC)
	{
		intPref =0;			  // reset to 0
		PREF_GetIntPref("mail.mdn.report.not_in_to_cc", &intPref);
		switch (intPref)
		{
		case 0:
		default:
			m_reallySendMdn = FALSE;
			break;
		case 1:
			m_autoSend = TRUE;
			break;
		case 2:
			m_autoSend = FALSE;
			break;
		}
	}

	return m_reallySendMdn;
}

void
MSG_ProcessMdnNeededState::CreateMdnMsg()
{
	int32 status = 0;
	
	if (!m_autoSend)
		m_reallySendMdn = 
			FE_Confirm(m_pane->GetContext(),
			   XP_GetString(MK_MSG_MDN_WISH_TO_SEND));

	if (!m_reallySendMdn)
		return;
	m_msgFileName = WH_TempName (xpFileToPost, "mdnmsg");
	if (!m_msgFileName)
		return;
	m_outFile = XP_FileOpen (m_msgFileName, xpFileToPost, XP_FILE_WRITE_BIN);
	if (!m_outFile) goto done;
	status = CreateFirstPart();
	if (status < 0) goto done;
	status = CreateSecondPart();
	if (status < 0) goto done;
	status = CreateThirdPart();
	
done:
	if (m_outFile)
	{
		XP_FileClose(m_outFile);
		m_outFile = 0;
	}
	if (status < 0)
	{
		// may want post out error message
		XP_FileRemove(m_msgFileName, xpFileToPost);
		XP_FREEIF(m_msgFileName);
	}
	else
	{
		DoSendMdn();
	}
}

int32
MSG_ProcessMdnNeededState::CreateFirstPart()
{
	int gmtoffset = XP_LocalZoneOffset();
	char *convbuf = NULL, *tmpBuffer = NULL;
	int16 win_csid;
	char *parm = NULL;
	char *firstPart = NULL;
	int formatId = MK_MSG_MDN_DISPLAYED;
	int32 status = 0;
	char *receipt_string = NULL;
	char *wrapped_string = NULL;

	XP_ASSERT(m_outFile);

	if (!m_mimeSeparator)
		m_mimeSeparator = mime_make_separator("mdn");
	if (!m_mimeSeparator)
		return MK_OUT_OF_MEMORY;

	tmpBuffer = (char *) XP_ALLOC(256);

	if (!tmpBuffer)
		return MK_OUT_OF_MEMORY;

	win_csid = INTL_DefaultWinCharSetID(m_pane->GetContext());
	m_csid = INTL_DefaultMailCharSetID(win_csid);

#ifndef NSPR20
    PRTime now;
    PR_ExplodeTime(&now, PR_Now());
#else
    PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
#endif /* NSPR20 */

	/* Use PR_FormatTimeUSEnglish() to format the date in US English format,
	   then figure out what our local GMT offset is, and append it (since
	   PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
	   per RFC 1123 (superceding RFC 822.)
	 */
	PR_FormatTimeUSEnglish(tmpBuffer, 100,
						   "Date: %a, %d %b %Y %H:%M:%S ",
						   &now);

	PR_snprintf(tmpBuffer + XP_STRLEN(tmpBuffer), 100,
				"%c%02d%02d" CRLF,
				(gmtoffset >= 0 ? '+' : '-'),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) / 60),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) % 60));

	status = WriteString(tmpBuffer);
	XP_FREEIF(tmpBuffer);
	if (status < 0) return status;

	convbuf = IntlEncodeMimePartIIStr((char *) FE_UsersMailAddress(),
									  m_csid, TRUE);

	parm = PR_smprintf("From: %s" CRLF, convbuf ? convbuf : FE_UsersMailAddress());
	PUSH_N_FREE_STRING (parm);
			
	XP_FREEIF(convbuf);

	parm = msg_generate_message_id();
	tmpBuffer = PR_smprintf("Message-ID: %s" CRLF, parm);
	PUSH_N_FREE_STRING(tmpBuffer);
	
	XP_FREEIF(parm);

	receipt_string = XP_GetString(MK_MDN_DISPLAYED_RECEIPT + (int)
								  m_disposeType);

	parm = PR_smprintf ("%s - %s", (receipt_string ? receipt_string :
						"Return Receipt"), (m_subject ? m_subject
						: ""));
	convbuf = 
		IntlEncodeMimePartIIStr(parm ? parm : "Return Receipt",
								m_csid, TRUE);
	tmpBuffer = PR_smprintf("Subject: %s" CRLF, (convbuf ? convbuf : (parm ? parm :
		                            "Return Receipt")));
	PUSH_N_FREE_STRING(tmpBuffer);
	if (parm)
	{
		PR_smprintf_free(parm);
		parm = 0;
	}
	XP_FREEIF(convbuf);

	convbuf = IntlEncodeMimePartIIStr(m_dispositionNotificationTo, m_csid,
									  TRUE); 
	tmpBuffer = PR_smprintf("To: %s" CRLF, convbuf ? convbuf :	m_dispositionNotificationTo); 
	PUSH_N_FREE_STRING(tmpBuffer);

	XP_FREEIF(convbuf);

	// *** This is not in the spec. I am adding this so we could do
	// threading
	if (*m_messageId == '<')
		tmpBuffer = PR_smprintf("References: %s" CRLF, m_messageId);
	else
		tmpBuffer = PR_smprintf("References: <%s>" CRLF, m_messageId);
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("%s" CRLF, "MIME-Version: 1.0");
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("Content-Type: multipart/report; \
report-type=disposition-notification;\r\n\tboundary=\"%s\"" CRLF CRLF,
				m_mimeSeparator); 
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator);
	PUSH_N_FREE_STRING(tmpBuffer);

	char charset[30];
	INTL_CharSetIDToName(m_csid, charset);

	tmpBuffer = PR_smprintf("Content-Type: text/plain; charset=%s" CRLF, charset);
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("Content-Transfer-Encoding: %s" CRLF CRLF, ENCODING_7BIT);
	PUSH_N_FREE_STRING(tmpBuffer);

	formatId = MK_MSG_MDN_DISPLAYED + ((int) m_disposeType - (int)
									   eDisplayed); 

	firstPart = XP_STRDUP(XP_GetString(formatId));
	convbuf = IntlEncodeMimePartIIStr(firstPart, m_csid, TRUE);
	wrapped_string = (char *) XP_WordWrap(m_csid, (unsigned char *)
										  (convbuf ? convbuf : firstPart),
										  72, FALSE);
	tmpBuffer = PR_smprintf("%s" CRLF CRLF, wrapped_string ? wrapped_string : firstPart);
	PUSH_N_FREE_STRING(tmpBuffer);

	XP_FREEIF(firstPart);
	XP_FREEIF(convbuf);
	XP_FREEIF(wrapped_string);

	return status;
}

int32
MSG_ProcessMdnNeededState::CreateSecondPart()
{
	char *tmpBuffer = NULL;
	char *convbuf = NULL;
	int32 status = 0;
	
	tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator);
	PUSH_N_FREE_STRING(tmpBuffer);
	
	tmpBuffer = PR_smprintf("%s" CRLF, "Content-Type: message/disposition-notification");
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("%s" CRLF, "Content-Disposition: inline");
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("Content-Transfer-Encoding: %s" CRLF CRLF, ENCODING_7BIT);
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("Reporting-UA: %s; %s %s" CRLF,
				NET_HostName(), XP_AppCodeName, XP_AppVersion);
	PUSH_N_FREE_STRING(tmpBuffer);

	if (m_originalRecipient && *m_originalRecipient)
	{
		tmpBuffer = PR_smprintf("Original-Recipient: %s" CRLF, m_originalRecipient);
		PUSH_N_FREE_STRING(tmpBuffer);
	}

	convbuf = IntlEncodeMimePartIIStr((char *) FE_UsersMailAddress(), m_csid,
									  TRUE); 
	tmpBuffer = PR_smprintf("Final-Recipient: rfc822;%s" CRLF, convbuf ? convbuf :
				FE_UsersMailAddress()); 
	PUSH_N_FREE_STRING(tmpBuffer);

	XP_FREEIF (convbuf);

	if (*m_messageId == '<')
		tmpBuffer = PR_smprintf("Original-Message-ID: %s" CRLF, m_messageId);
	else
		tmpBuffer = PR_smprintf("Original-Message-ID: <%s>" CRLF, m_messageId);
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("Disposition: %s/%s; %s" CRLF CRLF,
				(m_autoAction ? "automatic-action" : "manual-action"),
				(m_autoSend ? "MDN-sent-automatically" : "MDN-sent-manually"),
				DispositionTypes[(int) m_disposeType]);
	PUSH_N_FREE_STRING(tmpBuffer);
	
	return status;
}

int32
MSG_ProcessMdnNeededState::CreateThirdPart()
{
	char *tmpBuffer = NULL;
	int32 status = 0;

	tmpBuffer = PR_smprintf("--%s" CRLF, m_mimeSeparator);
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("%s" CRLF, "Content-Type: text/rfc822-headers");
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("%s" CRLF, "Content-Transfer-Encoding: 7bit");
	PUSH_N_FREE_STRING(tmpBuffer);

	tmpBuffer = PR_smprintf("%s" CRLF CRLF, "Content-Disposition: inline");
	PUSH_N_FREE_STRING(tmpBuffer);
   
	status = OutputAllHeaders();

	if (status < 0) return status;

	status = WriteString(CRLF);
	if (status < 0) return status;

	tmpBuffer = PR_smprintf("--%s--" CRLF, m_mimeSeparator);
	PUSH_N_FREE_STRING(tmpBuffer);

	return status;
}

void
MSG_ProcessMdnNeededState::DoSendMdn()
{
	XP_ASSERT(m_dispositionNotificationTo);
	
	if (!m_dispositionNotificationTo) return; // MK_OUT_OF_MEMORY

	char *tmpBuffer =			// 9 = mailto: + null + 1 extra
		(char *) XP_ALLOC(XP_STRLEN(m_dispositionNotificationTo) + 9);

	if (!tmpBuffer) return;		// MK_OUT_OF_MEMORY

	URL_Struct *url = NULL;

	FE_Progress (m_pane->GetContext(), XP_GetString(MK_MSG_DELIV_MAIL));

	XP_STRCPY(tmpBuffer, "mailto:");
	
	XP_STRCAT(tmpBuffer, m_dispositionNotificationTo);

	url = NET_CreateURLStruct (tmpBuffer, NET_DONT_RELOAD);
	if (url)
	{
		url->post_data = XP_STRDUP(m_msgFileName);
		url->post_data_size = XP_STRLEN(m_msgFileName);
		url->post_data_is_file = TRUE;
		url->method = URL_POST_METHOD;
		url->fe_data = this;
		url->internal_url = TRUE;
		url->msg_pane = m_pane;
			// clear m_msgFileName to prevent removing temp file too early
		XP_FREEIF(m_msgFileName);
		// Set sending MDN in progress so the smtp state machine can null out
		// the address for MAIL FROM
		m_pane->SetSendingMDNInProgress(TRUE);

		MSG_UrlQueue::AddUrlToPane (url,
									MSG_ProcessMdnNeededState::PostSendMdn,
									m_pane,
									TRUE);
	}
	XP_FREEIF(tmpBuffer);
}

/* static */ void
MSG_ProcessMdnNeededState::PostSendMdn(URL_Struct *url, int status, MWContext
									   *context)
{
	if (url->msg_pane)
		url->msg_pane->SetSendingMDNInProgress(FALSE);

	if (status < 0)
	{
		char *error_msg = NET_ExplainErrorDetails(status, 0, 0, 0, 0);
		if (error_msg)
			FE_Alert(context, error_msg);
		XP_FREEIF(error_msg);
	}
	if (url->post_data)
	{
		XP_FileRemove(url->post_data, xpFileToPost);
		XP_FREEIF(url->post_data);
		url->post_data_size = 0;
	}
	NET_FreeURLStruct(url);
}

void
MSG_ProcessMdnNeededState::StoreImapMDNSentFlag(MSG_FolderInfo *folder,
												MessageKey key)
{
	if (!folder || key == MSG_MESSAGEKEYNONE)
		return;
	if (folder->GetType() == FOLDER_IMAPMAIL)
	{
		MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
		if (imapFolder)
		{
			MSG_UrlQueue *queue = MSG_UrlQueue::FindQueue(m_pane);

 			IDArray idArray;
			idArray.Add(key);

			imapFolder->StoreImapFlags(m_pane,
									   kImapMsgMDNSentFlag,
									   TRUE,
									   idArray,
									   queue);
		}
	}
}


