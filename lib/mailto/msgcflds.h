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

#ifndef _MsgCFlds_H_
#define _MsgCFlds_H_

#include "rosetta.h"
#include "msgzap.h"


// Note that all the "Get" methods never return NULL (except in case of serious
// error, like an illegal parameter); rather, they return "" if things were set
// to NULL.  This makes it real handy for the callers.

class MSG_CompositionFields : public MSG_ZapIt {
public:
	MSG_CompositionFields();
	MSG_CompositionFields(MSG_CompositionFields*); // Makes a copy.
	virtual ~MSG_CompositionFields();

	int SetHeader(MSG_HEADER_SET header, const char* value);
	const char* GetHeader(MSG_HEADER_SET header);

	int SetBoolHeader(MSG_BOOL_HEADER_SET header, XP_Bool bValue);
	XP_Bool GetBoolHeader(MSG_BOOL_HEADER_SET header);

	int SetBody(const char*);
	const char* GetBody();

	int AppendBody(const char*);

	// When forwarding a bunch of messages, we can have a bunch of
	// "forward url's" instead of an attachment.

	int AddForwardURL(const char*);

	int32 GetNumForwardURL();
	const char* GetForwardURL(int32 which);

	int32 GetReturnReceiptType() { return m_receiptType; };
	void SetReturnReceiptType(int32 type) {m_receiptType = type;};


	// Convenience routines...

	int SetFrom(const char* value) {
		return SetHeader(MSG_FROM_HEADER_MASK, value);
	}
	const char* GetFrom() {
		return GetHeader(MSG_FROM_HEADER_MASK);
	}

	int SetReplyTo(const char* value) {
		return SetHeader(MSG_REPLY_TO_HEADER_MASK, value);
	}
	const char* GetReplyTo() {
		return GetHeader(MSG_REPLY_TO_HEADER_MASK);
	}

	int SetTo(const char* value) {
		return SetHeader(MSG_TO_HEADER_MASK, value);
	}
	const char* GetTo() {
		return GetHeader(MSG_TO_HEADER_MASK);
	}

	int SetCc(const char* value) {
		return SetHeader(MSG_CC_HEADER_MASK, value);
	}
	const char* GetCc() {
		return GetHeader(MSG_CC_HEADER_MASK);
	}

	int SetBcc(const char* value) {
		return SetHeader(MSG_BCC_HEADER_MASK, value);
	}
	const char* GetBcc() {
		return GetHeader(MSG_BCC_HEADER_MASK);
	}

	int SetFcc(const char* value) {
		return SetHeader(MSG_FCC_HEADER_MASK, value);
	}
	const char* GetFcc() {
		return GetHeader(MSG_FCC_HEADER_MASK);
	}

	int SetNewsFcc(const char* value) {
		return SetHeader(MSG_NEWS_FCC_HEADER_MASK, value);
	}
	const char* GetNewsFcc() {
		return GetHeader(MSG_NEWS_FCC_HEADER_MASK);
	}
	int SetNewsBcc(const char* value) {
		return SetHeader(MSG_NEWS_BCC_HEADER_MASK, value);
	}
	const char* GetNewsBcc() {
		return GetHeader(MSG_NEWS_BCC_HEADER_MASK);
	}

	int SetNewsgroups(const char* value) {
		return SetHeader(MSG_NEWSGROUPS_HEADER_MASK, value);
	}
	const char* GetNewsgroups() {
		return GetHeader(MSG_NEWSGROUPS_HEADER_MASK);
	}

	int SetFollowupTo(const char* value) {
		return SetHeader(MSG_FOLLOWUP_TO_HEADER_MASK, value);
	}
	const char* GetFollowupTo() {
		return GetHeader(MSG_FOLLOWUP_TO_HEADER_MASK);
	}

	int SetSubject(const char* value) {
		return SetHeader(MSG_SUBJECT_HEADER_MASK, value);
	}
	const char* GetSubject() {
		return GetHeader(MSG_SUBJECT_HEADER_MASK);
	}

	int SetAttachments(const char* value) {
		return SetHeader(MSG_ATTACHMENTS_HEADER_MASK, value);
	}
	const char* GetAttachments() {
		return GetHeader(MSG_ATTACHMENTS_HEADER_MASK);
	}

	int SetOrganization(const char* value) {
		return SetHeader(MSG_ORGANIZATION_HEADER_MASK, value);
	}
	const char* GetOrganization() {
		return GetHeader(MSG_ORGANIZATION_HEADER_MASK);
	}

	int SetReferences(const char* value) {
		return SetHeader(MSG_REFERENCES_HEADER_MASK, value);
	}
	const char* GetReferences() {
		return GetHeader(MSG_REFERENCES_HEADER_MASK);
	}

	int SetOtherRandomHeaders(const char* value) {
		return SetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, value);
	}
	const char* GetOtherRandomHeaders() {
		return GetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK);
	}

	int SetNewspostUrl(const char* value) {
		return SetHeader(MSG_NEWSPOSTURL_HEADER_MASK, value);
	}
	const char* GetNewspostUrl() {
		return GetHeader(MSG_NEWSPOSTURL_HEADER_MASK);
	}

	int SetDefaultBody(const char* value) {
		return SetHeader(MSG_DEFAULTBODY_HEADER_MASK, value);
	}
	const char* GetDefaultBody() {
		return GetHeader(MSG_DEFAULTBODY_HEADER_MASK);
	}

    int SetPriority(const char* value) {
	    return SetHeader(MSG_PRIORITY_HEADER_MASK, value);
    }

    const char* GetPriority() {
	    return GetHeader(MSG_PRIORITY_HEADER_MASK);
    }

	int SetMessageEncoding(const char*  value) {
		return SetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, value);
	}

	const char* GetMessageEncoding() {
		return GetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK);
	}

	int SetCharacterSet(const char*  value) {
		return SetHeader (MSG_CHARACTER_SET_HEADER_MASK, value);
	}

	const char* GetCharacterSet() {
		return GetHeader(MSG_CHARACTER_SET_HEADER_MASK);
	}

	int SetMessageId(const char* value) {
		return SetHeader (MSG_MESSAGE_ID_HEADER_MASK, value);
	}

	const char* GetMessageId() {
		return GetHeader(MSG_MESSAGE_ID_HEADER_MASK);
	}

	int SetHTMLPart(const char* value) {
		return SetHeader(MSG_HTML_PART_HEADER_MASK, value);
	}

	const char* GetHTMLPart() {
		return GetHeader(MSG_HTML_PART_HEADER_MASK);
	}

	int SetTemplateName(const char* value) {
		return SetHeader(MSG_X_TEMPLATE_HEADER_MASK, value);
	}

	const char* GetTemplateName() {
		return GetHeader(MSG_X_TEMPLATE_HEADER_MASK);
	}

	// Bool headers

	int SetReturnReceipt(XP_Bool value) {
		return SetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, value);
	}

	XP_Bool GetReturnReceipt() {
		return GetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK);
	}

HG87266

	int SetSigned(XP_Bool value) {
		return SetBoolHeader(MSG_SIGNED_BOOL_HEADER_MASK, value);
	}

	XP_Bool GetSigned() {
		return GetBoolHeader(MSG_SIGNED_BOOL_HEADER_MASK);
	}

	int SetAttachVCard(XP_Bool value) {
		return SetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK, value);
	}

	XP_Bool GetAttachVCard() {
		return GetBoolHeader(MSG_ATTACH_VCARD_BOOL_HEADER_MASK);
	}


	void SetOwner(MSG_Pane *pane) {
		m_owner = pane;
	}

	MSG_Pane * GetOwner() { return m_owner; }


	void SetForcePlainText(XP_Bool value) {m_force_plain_text = value;}
	XP_Bool GetForcePlainText() {return m_force_plain_text;}
	void SetUseMultipartAlternative(XP_Bool value) {m_multipart_alt = value;}
	XP_Bool GetUseMultipartAlternative() {return m_multipart_alt;}

protected:
	int DecodeHeader(MSG_HEADER_SET header);

	// These methods allow news URLs in the newsgroups header
	int SetNewsUrlHeader (const char *hostPort, XP_Bool xxx, const char *group);
	int ParseNewsgroupsForUrls (const char *value);

	MSG_Pane *m_owner;
	char* m_headers[32];
	char* m_body;
	char** m_forwardurl;
	int32 m_numforward;
	int32 m_maxforward;
	XP_Bool m_boolHeaders[MSG_LAST_BOOL_HEADER_MASK];
	XP_Bool m_force_plain_text;
	XP_Bool m_multipart_alt;
	int32 m_receiptType; /* 0:None 1:DSN 2:MDN 3:BOTH */
};


#endif /* _MsgCFlds_H_ */
