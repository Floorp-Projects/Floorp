/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "rosetta_mailnews.h"
#include "nsCRT.h"
#include "nsMsgCompFields.h"
#include "nsMsgCompFieldsFact.h"

/* this function will be used by the factory to generate an Message Compose Fields Object....*/
nsresult NS_NewMsgCompFields(nsIMsgCompFields** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgCompFields* pCompFields = new nsMsgCompFields();
		if (pCompFields)
			return pCompFields->QueryInterface(nsIMsgCompFields::GetIID(), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgCompFields, nsIMsgCompFields::GetIID());

nsMsgCompFields::nsMsgCompFields()
{
	PRInt16 i;
	PRBool bReturnReceiptOn = PR_FALSE;

	m_owner = NULL;
	for (i = 0; i < MAX_HEADERS; i ++)
		m_headers[i] = NULL;
	m_body = NULL;
	m_forwardurl = NULL;
	m_numforward = 0;
	m_maxforward = 0;
	for (i = 0; i < MSG_LAST_BOOL_HEADER_MASK; i ++)
		m_boolHeaders[i] = PR_FALSE;
	m_force_plain_text = PR_FALSE;
	m_multipart_alt = PR_FALSE;
	m_receiptType = 0;

	PREF_GetBoolPref("mail.request.return_receipt_on", &bReturnReceiptOn);
	PREF_GetIntPref("mail.request.return_receipt", &m_receiptType);
	SetReturnReceipt (bReturnReceiptOn, NULL);

	NS_INIT_REFCNT();
}


nsMsgCompFields::~nsMsgCompFields()
{
	PRInt16 i;
	for (i = 0; i < MAX_HEADERS; i ++)
		PR_FREEIF(m_headers[i]);

    PR_FREEIF(m_body);

	for (i = 0; i < m_numforward; i++)
		delete [] m_forwardurl[i];

	delete [] m_forwardurl;
}


nsresult nsMsgCompFields::Copy(nsIMsgCompFields* pMsgCompFields)
{
	nsMsgCompFields * pFields = (nsMsgCompFields*)pMsgCompFields;

	PRInt16 i;
	for (i = 0; i < MAX_HEADERS; i ++) {
		if (pFields->m_headers[i])
			m_headers[i] = nsCRT::strdup(pFields->m_headers[i]);
	}
    if (pFields->m_body)
		m_body = nsCRT::strdup(pFields->m_body);

	for (i = 0; i < pFields->m_numforward; i++)
		AddForwardURL(pFields->m_forwardurl[i]);

	for (i = 0; i < MSG_LAST_BOOL_HEADER_MASK; i ++)
        m_boolHeaders[i] = pFields->m_boolHeaders[i];

	m_receiptType = pFields->m_receiptType;

	return NS_OK;
}



nsresult nsMsgCompFields::SetHeader(PRInt32 header, const char *value, PRInt32 *_retval)
{
    int status = 0;

	/* Since colon is not a legal character in a newsgroup name under son-of-1036
	   we're assuming that such a header contains a URL, and we should parse it out
	   to infer the news server. */
	if (value && MSG_NEWSGROUPS_HEADER_MASK == header && PL_strchr(value, ':')) {
		status = ParseNewsgroupsForUrls (value);
		if (status == 0)
			return status; /* it was a news URL, and we snarfed it up */
		else {
			if (status == MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS) {
#ifdef UNREAD_CODE
				MSG_Pane *owner = GetOwner();
				if (owner)
					FE_Alert (owner->GetContext(), XP_GetString(status));
#endif
			}

			status = 0; /* It isn't a valid news URL, so treat it like a newsgroup
                           MSG_CompositionPane::SanityCheck will decide if it's a legal newsgroup name */
		}
	}

	int i = DecodeHeader(header);
	if (i >= 0) {
		char* old = m_headers[i]; /* Done with careful paranoia, in case the
								     value given is the old value (or worse,
								     a substring of the old value, as does
								     happen here and there.) */
		if (value != old) {
			if (value) {
				m_headers[i] = nsCRT::strdup(value);
				if (!m_headers[i]) 
				   status = MK_OUT_OF_MEMORY;
			} else 
				m_headers[i] = NULL;
			PR_FREEIF(old);
		}
	}

	if (_retval)
		*_retval = status;
	return NS_OK;
}

nsresult nsMsgCompFields::GetHeader(PRInt32 header, char **_retval)
{
	NS_PRECONDITION(nsnull != _retval, "nsnull ptr");

	*_retval = NS_CONST_CAST(char*, GetHeader(header));
	return NS_OK;
}

const char* nsMsgCompFields::GetHeader(PRInt32 header)
{
    int i = DecodeHeader(header);
    if (i >= 0) {
		return m_headers[i] ? m_headers[i] : "";
    }
    return NULL;
}

nsresult nsMsgCompFields::SetBoolHeader(PRInt32 header, PRBool bValue, PRInt32 *_retval)
{
	int status = 0;
	NS_ASSERTION ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK, "invalid header index");

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return NS_ERROR_FAILURE;

	m_boolHeaders[header] = bValue;

	if (_retval)
		*_retval = status;

	return NS_OK;
}

nsresult nsMsgCompFields::GetBoolHeader(PRInt32 header, PRBool *_retval)
{
	NS_PRECONDITION(nsnull != _retval, "nsnull ptr");

	*_retval = GetBoolHeader(header);
	return NS_OK;
}

PRBool nsMsgCompFields::GetBoolHeader(PRInt32 header)
{
	NS_ASSERTION ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK, "invalid header index");

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return PR_FALSE;

	return m_boolHeaders[header];
}

nsresult nsMsgCompFields::SetFrom(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_FROM_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetFrom(char **_retval)
{
	return GetHeader(MSG_FROM_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetReplyTo(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_REPLY_TO_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetReplyTo(char **_retval)
{
	return GetHeader(MSG_REPLY_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetTo(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_TO_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetTo(char **_retval)
{
	return GetHeader(MSG_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetCc(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_CC_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetCc(char **_retval)
{
	return GetHeader(MSG_CC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetBcc(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_BCC_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetBcc(char **_retval)
{
	return GetHeader(MSG_BCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetFcc(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_NEWS_FCC_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetFcc(char **_retval)
{
	return GetHeader(MSG_NEWS_FCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsFcc(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_NEWS_FCC_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetNewsFcc(char **_retval)
{
	return GetHeader(MSG_NEWS_FCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsBcc(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_NEWS_BCC_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetNewsBcc(char **_retval)
{
	return GetHeader(MSG_NEWS_BCC_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewsgroups(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_NEWSGROUPS_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetNewsgroups(char **_retval)
{
	return GetHeader(MSG_NEWSGROUPS_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetFollowupTo(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_FOLLOWUP_TO_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetFollowupTo(char **_retval)
{
	return GetHeader(MSG_FOLLOWUP_TO_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetSubject(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_SUBJECT_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetSubject(char **_retval)
{
	return GetHeader(MSG_SUBJECT_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetAttachments(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_ATTACHMENTS_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetAttachments(char **_retval)
{
	return GetHeader(MSG_ATTACHMENTS_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetOrganization(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_ORGANIZATION_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetOrganization(char **_retval)
{
	return GetHeader(MSG_ORGANIZATION_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetReferences(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_REFERENCES_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetReferences(char **_retval)
{
	return GetHeader(MSG_REFERENCES_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetOtherRandomHeaders(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetOtherRandomHeaders(char **_retval)
{
	return GetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetNewspostUrl(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_NEWSPOSTURL_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetNewspostUrl(char **_retval)
{
	return GetHeader(MSG_NEWSPOSTURL_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetDefaultBody(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_DEFAULTBODY_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetDefaultBody(char **_retval)
{
	return GetHeader(MSG_DEFAULTBODY_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetPriority(const char *value, PRInt32 *_retval)
{
	return SetHeader(nsMsgPriority_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetPriority(char **_retval)
{
	return GetHeader(nsMsgPriority_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetMessageEncoding(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetMessageEncoding(char **_retval)
{
	return GetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetCharacterSet(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_CHARACTER_SET_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetCharacterSet(char **_retval)
{
	return GetHeader(MSG_CHARACTER_SET_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetMessageId(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_MESSAGE_ID_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetMessageId(char **_retval)
{
	return GetHeader(MSG_MESSAGE_ID_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetHTMLPart(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_HTML_PART_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetHTMLPart(char **_retval)
{
	return GetHeader(MSG_HTML_PART_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetTemplateName(const char *value, PRInt32 *_retval)
{
	return SetHeader(MSG_X_TEMPLATE_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetTemplateName(char **_retval)
{
	return GetHeader(MSG_X_TEMPLATE_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetReturnReceipt(PRBool value, PRInt32 *_retval)
{
	return SetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetReturnReceipt(PRBool *_retval)
{
	return GetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, _retval);
}

nsresult nsMsgCompFields::SetAttachVCard(PRBool value, PRInt32 *_retval)
{
	return SetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK, value, _retval);
}

nsresult nsMsgCompFields::GetAttachVCard(PRBool *_retval)
{
	return GetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK, _retval);
}

HJ36954
{
	/* Here's where we allow URLs in the newsgroups: header */

	int status = -1; 
	if (hostPort && group) { /* must have a group */
		char *newsPostUrl = HJ57077
		if (newsPostUrl) {
			const char *existingHeader;
			GetHeader (MSG_NEWSPOSTURL_HEADER_MASK, (char **)&existingHeader);
			if (existingHeader && *existingHeader && nsCRT::strcasecmp(newsPostUrl, existingHeader))
				status = MK_MSG_CANT_POST_TO_MULTIPLE_NEWS_HOSTS; /* can only send to one news host at a time */
			else {
				SetHeader (MSG_NEWSPOSTURL_HEADER_MASK, newsPostUrl, NULL);
				status = 0; /* we succeeded, no need to keep looking at this header */
			}
			PR_Free(newsPostUrl);
		} else
			status = MK_OUT_OF_MEMORY;
	}

	return status;
}


PRInt16 nsMsgCompFields::ParseNewsgroupsForUrls (const char *value)
{
	int status = 0;

	/* Here we pull apart the comma-separated header value and look for news
	   URLs. We'll use the URL to set the newspost URL to determine the host */
#if 0 //JFD
	msg_StringArray values (PR_TRUE /*owns memory for strings*/);
	values.ImportTokenList (value);

	for (int i = 0; i < values.GetSize() && status == 0; i++) {
		const char *singleValue = values.GetAt(i);
		if (NEWS_TYPE_URL == NET_URL_Type (singleValue)) {
			char *hostPort, *group, *id, *data;
			HJ81279
			if (status == 0) {
				HJ78808
				if (status == 0) {
					values.RemoveAt(i);         /* Remove the URL spec for this group */
					values.InsertAt (i, group); /* Add in the plain old group name */
				}
				PR_FREEIF (hostPort);
				PR_FREEIF (group);
				PR_FREEIF (id);
				PR_FREEIF (data);
			}
		} else
			status = MK_MSG_INVALID_NEWS_HEADER;
	}

	if (status == 0) {
		char *newValue = values.ExportTokenList ();
		if (newValue) {
			status = SetHeader (MSG_NEWSGROUPS_HEADER_MASK, newValue);
			PR_Free(newValue);
		}
	}
#endif //JFD
	return status;
}

nsresult nsMsgCompFields::SetBody(const char *value, PRInt32 *_retval)
{
	long retval = 0;

    PR_FREEIF(m_body);
    if (value) {
		m_body = nsCRT::strdup(value);
		if (!m_body)
			retval = MK_OUT_OF_MEMORY;
    }
	if (_retval)
		*_retval = retval;

    return NS_OK;
}

nsresult nsMsgCompFields::GetBody(char **_retval)
{
	NS_PRECONDITION(nsnull != _retval, "nsnull ptr");

	*_retval = NS_CONST_CAST(char*, GetBody());
	return NS_OK;
}

const char* nsMsgCompFields::GetBody()
{
    return m_body ? m_body : "";
}


PRInt16 nsMsgCompFields::AppendBody(char* value)
{
    if (!value || !*value)
		return 0;
 
	if (!m_body) {
		return SetBody(value, NULL);
    } else {
		char* tmp = (char*) PR_Malloc(nsCRT::strlen(m_body) + nsCRT::strlen(value) + 1);
		if (tmp) {
			tmp = nsCRT::strdup(m_body);
			PL_strcat(tmp, value);
			PR_Free(m_body);
			m_body = tmp;
		} else {
			return MK_OUT_OF_MEMORY;
		}
    }
    return 0;
}
    
PRInt16 nsMsgCompFields::DecodeHeader(MSG_HEADER_SET header)
{
    int result;
 
	switch(header) {
    case MSG_FROM_HEADER_MASK				: result = 0;		break;
    case MSG_REPLY_TO_HEADER_MASK			: result = 1;		break;
    case MSG_TO_HEADER_MASK					: result = 2;		break;
    case MSG_CC_HEADER_MASK					: result = 3;		break;
    case MSG_BCC_HEADER_MASK				: result = 4;		break;
    case MSG_FCC_HEADER_MASK				: result = 5;		break;
    case MSG_NEWSGROUPS_HEADER_MASK			: result = 6;		break;
    case MSG_FOLLOWUP_TO_HEADER_MASK		: result = 7;		break;
    case MSG_SUBJECT_HEADER_MASK			: result = 8;		break;
    case MSG_ATTACHMENTS_HEADER_MASK		: result = 9;		break;
    case MSG_ORGANIZATION_HEADER_MASK		: result = 10;		break;
    case MSG_REFERENCES_HEADER_MASK			: result = 11;		break;
    case MSG_OTHERRANDOMHEADERS_HEADER_MASK	: result = 12;		break;
    case MSG_NEWSPOSTURL_HEADER_MASK		: result = 13;		break;
    case nsMsgPriority_HEADER_MASK			: result = 14;		break;
	case MSG_NEWS_FCC_HEADER_MASK			: result = 15;		break;
	case MSG_MESSAGE_ENCODING_HEADER_MASK	: result = 16;		break;
	case MSG_CHARACTER_SET_HEADER_MASK		: result = 17;		break;
	case MSG_MESSAGE_ID_HEADER_MASK			: result = 18;		break;
	case MSG_NEWS_BCC_HEADER_MASK			: result = 19;		break;
	case MSG_HTML_PART_HEADER_MASK			: result = 20;		break;
    case MSG_DEFAULTBODY_HEADER_MASK		: result = 21;		break;
	case MSG_X_TEMPLATE_HEADER_MASK			: result = 22;		break;
    default:
		NS_ASSERTION(0, "invalid header index");
		result = -1;
		break;
    }

    NS_ASSERTION(result < sizeof(m_headers) / sizeof(char*), "wrong result, review the code!");
    return result;
}

PRInt16 nsMsgCompFields::AddForwardURL(const char* url)
{
	NS_ASSERTION(url && *url, "empty url");
	if (!url || !*url)
		return -1;

	if (m_numforward >= m_maxforward) {
		m_maxforward += 10;
		char** tmp = new char* [m_maxforward];
		if (!tmp)
			return MK_OUT_OF_MEMORY;
		for (PRInt32 i=0 ; i<m_numforward ; i++) {
			tmp[i] = m_forwardurl[i];
		}
		delete [] m_forwardurl;
		m_forwardurl = tmp;
	}
	m_forwardurl[m_numforward] = new char[nsCRT::strlen(url) + 1];
	if (!m_forwardurl[m_numforward])
		return MK_OUT_OF_MEMORY;
	m_forwardurl[m_numforward] = nsCRT::strdup(url);
	m_numforward++;
	return 0;
}

PRInt32 nsMsgCompFields::GetNumForwardURL()
{
	return m_numforward;
}

const char* nsMsgCompFields::GetForwardURL(PRInt32 which)
{
	NS_ASSERTION(which >= 0 && which < m_numforward, "parameter out of range");
	if (which >= 0 && which < m_numforward) {
		return m_forwardurl[which];
	}
	return NULL;
}

