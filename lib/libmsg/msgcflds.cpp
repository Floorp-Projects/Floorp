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

#include "rosetta.h"
#include "msg.h"
#include "errcode.h"
#include "dberror.h"

#include "msgcflds.h"
#include "prefapi.h"
#include "ptrarray.h"

extern "C" {
    extern int MK_OUT_OF_MEMORY;
}

MSG_CompositionFields::MSG_CompositionFields()
{
	XP_Bool bReturnReceiptOn = FALSE;

	PREF_GetBoolPref("mail.request.return_receipt_on", &bReturnReceiptOn);
	PREF_GetIntPref("mail.request.return_receipt", &m_receiptType);
	SetReturnReceipt (bReturnReceiptOn);
}


MSG_CompositionFields::MSG_CompositionFields(MSG_CompositionFields* c)
{
	int i;
	for (i=0 ; i<sizeof(m_headers) / sizeof(char*) ; i++) {
		if (c->m_headers[i]) {
			m_headers[i] = XP_STRDUP(c->m_headers[i]);
		}
	}
    if (c->m_body) {
		m_body = XP_STRDUP(c->m_body);
    }
	for (i=0 ; i<c->m_numforward ; i++) {
		AddForwardURL(c->m_forwardurl[i]);
	}
    for (i=0; i<sizeof(m_boolHeaders)/sizeof(XP_Bool) ; i++) {
        m_boolHeaders[i] = c->m_boolHeaders[i];
    }
	m_receiptType = c->m_receiptType;
}



MSG_CompositionFields::~MSG_CompositionFields()
{
	int i;
	for (i=0 ; i<sizeof(m_headers) / sizeof(char*) ; i++) {
		FREEIF(m_headers[i]);
    }
    FREEIF(m_body);
	for (i=0 ; i<m_numforward ; i++) {
		delete [] m_forwardurl[i];
	}
	delete [] m_forwardurl;
}


int MSG_CompositionFields::SetNewsUrlHeader (const char *hostPort, XP_Bool xxx, const char *group)
{
	// Here's where we allow URLs in the newsgroups: header

	int status = -1; 
	if (hostPort && group) // must have a group
	{
		char *newsPostUrl = PR_smprintf ("%s://%s/", HG71654 "news", hostPort);
		if (newsPostUrl)
		{
			SetHeader (MSG_NEWSPOSTURL_HEADER_MASK, newsPostUrl);
			XP_FREE(newsPostUrl);
			status = 0; // we succeeded, no need to keep looking at this header
		}
		else
			status = MK_OUT_OF_MEMORY;
	}

	return status;
}


int MSG_CompositionFields::ParseNewsgroupsForUrls (const char *value)
{
	int status = 0;

	// Here we pull apart the comma-separated header value and look for news
	// URLs. We'll use the URL to set the newspost URL to determine the host
	msg_StringArray values (TRUE /*owns memory for strings*/);
	values.ImportTokenList (value);
	for (int i = 0; i < values.GetSize(); i++)
	{
		const char *singleValue = values.GetAt(i);
		if (NEWS_TYPE_URL == NET_URL_Type (singleValue))
		{
			char *hostPort, *group, *id, *data;
			XP_Bool xxx;
			if (0 == NET_parse_news_url (value, &hostPort, &xxx, &group, &id, &data))
			{
				status = SetNewsUrlHeader (hostPort, xxx, group);
				if (status == 0)
				{
					values.RemoveAt(i); // Remove the URL spec for this group
					values.Add (group); // Add in the plain old group name
				}
				FREEIF (hostPort);
				FREEIF (group);
				FREEIF (id);
				FREEIF (data);
			}
		}
	}
	char *newValue = values.ExportTokenList ();
	if (newValue)
	{
		status = SetHeader (MSG_NEWSGROUPS_HEADER_MASK, newValue);
		XP_FREE(newValue);
	}
	return status;
}


int
MSG_CompositionFields::SetHeader(MSG_HEADER_SET header, const char* value)
{
    int status = 0;

	// Since colon is not a legal character in a newsgroup name under son-of-1036
	// we're assuming that such a header contains a URL, and we should parse it out
	// to infer the news server.
	if (value && MSG_NEWSGROUPS_HEADER_MASK == header && XP_STRCHR(value, ':'))
		return ParseNewsgroupsForUrls (value);

	int i = DecodeHeader(header);
	if (i >= 0) 
	{
		char* old = m_headers[i]; // Done with careful paranoia, in case the
								  // value given is the old value (or worse,
								  // a substring of the old value, as does
								  // happen here and there.)
		if (value != old) 
		{
			if (value) 
			{
				m_headers[i] = XP_STRDUP(value);
				if (!m_headers[i]) 
				   status = MK_OUT_OF_MEMORY;
			} 
			else 
				m_headers[i] = NULL;
			FREEIF(old);
		}
	}

	return status;
}

extern "C"const char* MSG_GetCompFieldsHeader(MSG_CompositionFields *fields,
										   MSG_HEADER_SET header)
{
	return fields->GetHeader(header);
}

const char*
MSG_CompositionFields::GetHeader(MSG_HEADER_SET header)
{
    int i = DecodeHeader(header);
    if (i >= 0) {
		return m_headers[i] ? m_headers[i] : "";
    }
    return NULL;
}


int 
MSG_CompositionFields::SetBoolHeader(MSG_BOOL_HEADER_SET header, XP_Bool bValue)
{
	int status = 0;
	XP_ASSERT ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK);

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return -1;

	m_boolHeaders[header] = bValue;
	return status;
}


XP_Bool
MSG_CompositionFields::GetBoolHeader(MSG_BOOL_HEADER_SET header)
{
	XP_ASSERT ((int) header >= (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK &&
			   (int) header < (int) MSG_LAST_BOOL_HEADER_MASK);

	if ( (int) header < (int) MSG_RETURN_RECEIPT_BOOL_HEADER_MASK ||
		 (int) header >= (int) MSG_LAST_BOOL_HEADER_MASK )
		 return FALSE;

	return m_boolHeaders[header];
}


int
MSG_CompositionFields::SetBody(const char* value)
{
    FREEIF(m_body);
    if (value) {
		m_body = XP_STRDUP(value);
		if (!m_body) return MK_OUT_OF_MEMORY;
    }
    return 0;
}


const char* 
MSG_CompositionFields::GetBody()
{
    return m_body ? m_body : "";
}


int
MSG_CompositionFields::AppendBody(const char* value)
{
    if (!value || !*value) return 0;
    if (!m_body) {
		return SetBody(value);
    } else {
		char* tmp = (char*) XP_ALLOC(XP_STRLEN(m_body) + XP_STRLEN(value) + 1);
		if (tmp) {
			XP_STRCPY(tmp, m_body);
			XP_STRCAT(tmp, value);
			XP_FREE(m_body);
			m_body = tmp;
		} else {
			return MK_OUT_OF_MEMORY;
		}
    }
    return 0;
}

    
int
MSG_CompositionFields::DecodeHeader(MSG_HEADER_SET header)
{
    int result;
    switch(header) {
    case MSG_FROM_HEADER_MASK:
		result = 0;
		break;
    case MSG_REPLY_TO_HEADER_MASK:
		result = 1;
		break;
    case MSG_TO_HEADER_MASK:
		result = 2;
		break;
    case MSG_CC_HEADER_MASK:
		result = 3;
		break;
    case MSG_BCC_HEADER_MASK:
		result = 4;
		break;
    case MSG_FCC_HEADER_MASK:
		result = 5;
		break;
    case MSG_NEWSGROUPS_HEADER_MASK:
		result = 6;
		break;
    case MSG_FOLLOWUP_TO_HEADER_MASK:
		result = 7;
		break;
    case MSG_SUBJECT_HEADER_MASK:
		result = 8;
		break;
    case MSG_ATTACHMENTS_HEADER_MASK:
		result = 9;
		break;
    case MSG_ORGANIZATION_HEADER_MASK:
		result = 10;
		break;
    case MSG_REFERENCES_HEADER_MASK:
		result = 11;
		break;
    case MSG_OTHERRANDOMHEADERS_HEADER_MASK:
		result = 12;
		break;
    case MSG_NEWSPOSTURL_HEADER_MASK:
		result = 13;
		break;
    case MSG_PRIORITY_HEADER_MASK:
		result = 14;
		break;
	case MSG_NEWS_FCC_HEADER_MASK:
		result = 15;
		break;
	case MSG_MESSAGE_ENCODING_HEADER_MASK:
		result = 16;
		break;
	case MSG_CHARACTER_SET_HEADER_MASK:
		result = 17;
		break;
	case MSG_MESSAGE_ID_HEADER_MASK:
		result = 18;
		break;
	case MSG_NEWS_BCC_HEADER_MASK:
		result = 19;
		break;
	case MSG_HTML_PART_HEADER_MASK:
		result = 20;
		break;
    case MSG_DEFAULTBODY_HEADER_MASK:
		result = 21;
		break;
	case MSG_X_TEMPLATE_HEADER_MASK:
		result = 22;
		break;
    default:
		XP_ASSERT(0);
		result = -1;
		break;
    }
    XP_ASSERT(result < sizeof(m_headers) / sizeof(char*));
    return result;
}

int
MSG_CompositionFields::AddForwardURL(const char* url)
{
	XP_ASSERT(url && *url);
	if (!url || !*url) return -1;
	if (m_numforward >= m_maxforward) {
		m_maxforward += 10;
		char** tmp = new char* [m_maxforward];
		if (!tmp) return MK_OUT_OF_MEMORY;
		for (int32 i=0 ; i<m_numforward ; i++) {
			tmp[i] = m_forwardurl[i];
		}
		delete [] m_forwardurl;
		m_forwardurl = tmp;
	}
	m_forwardurl[m_numforward] = new char[XP_STRLEN(url) + 1];
	if (!m_forwardurl[m_numforward]) return MK_OUT_OF_MEMORY;
	XP_STRCPY(m_forwardurl[m_numforward], url);
	m_numforward++;
	return 0;
}

int32
MSG_CompositionFields::GetNumForwardURL()
{
	return m_numforward;
}

const char*
MSG_CompositionFields::GetForwardURL(int32 which)
{
	XP_ASSERT(which >= 0 && which < m_numforward);
	if (which >= 0 && which < m_numforward) {
		return m_forwardurl[which];
	}
	return NULL;
}
