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

#ifndef _MsgCompFields_H_
#define _MsgCompFields_H_

#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "net.h" /* should be defined into msgCore.h? */
//#include "intl_csi.h"
#include "msgcom.h"
#include "nsMsgHeaderMasks.h"

#include "nsMsgTransition.h"

#include "nsIMsgCompFields.h"


/* Note that all the "Get" methods never return NULL (except in case of serious
   error, like an illegal parameter); rather, they return "" if things were set
   to NULL.  This makes it real handy for the callers. */

class nsMsgCompFields : public nsIMsgCompFields, public nsMsgZapIt {
public:
	nsMsgCompFields();
	virtual ~nsMsgCompFields();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	NS_IMETHOD Copy(nsIMsgCompFields* pMsgCompFields);

	NS_IMETHOD SetAsciiHeader(PRInt32 header, const char *value);
	const char* GetHeader(PRInt32 header); //just return the address of the internal header variable, don't dispose it

	NS_IMETHOD SetHeader(PRInt32 header, const PRUnichar *value);
	NS_IMETHOD GetHeader(PRInt32 header, PRUnichar **_retval); //Will return a copy of the header, must be free using PR_Free() 

	NS_IMETHOD SetBoolHeader(PRInt32 header, PRBool bValue);
	NS_IMETHOD GetBoolHeader(PRInt32 header, PRBool *_retval);
	PRBool GetBoolHeader(PRInt32 header);

	/* Convenience routines to get and set header's value...
	
		IMPORTANT:
		all routines NS_IMETHOD GetXxx(char **_retval) will allocate a string that must be free later using free()
		all routines const char* GetXxx(void) will return a pointer to the header, please don't free it.
		
		accessor and mutator that are scriptable use Unicode only.
	*/

	NS_IMETHOD SetFrom(const PRUnichar *value);
	NS_IMETHOD GetFrom(PRUnichar **_retval);
	NS_IMETHOD SetFrom(const char *value) {return SetAsciiHeader(MSG_FROM_HEADER_MASK, value);}
	const char* GetFrom(void) {return GetHeader(MSG_FROM_HEADER_MASK);}

	NS_IMETHOD SetReplyTo(const PRUnichar *value);
	NS_IMETHOD GetReplyTo(PRUnichar **_retval);
	NS_IMETHOD SetReplyTo(const char *value) {return SetAsciiHeader(MSG_REPLY_TO_HEADER_MASK, value);}
	const char* GetReplyTo() {return GetHeader(MSG_REPLY_TO_HEADER_MASK);}

	NS_IMETHOD SetTo(const PRUnichar *value);
	NS_IMETHOD GetTo(PRUnichar **_retval);
	NS_IMETHOD SetTo(const char *value) {return SetAsciiHeader(MSG_TO_HEADER_MASK, value);}
	const char* GetTo() {return GetHeader(MSG_TO_HEADER_MASK);}

	NS_IMETHOD SetCc(const PRUnichar *value);
	NS_IMETHOD GetCc(PRUnichar **_retval);
	NS_IMETHOD SetCc(const char *value) {return SetAsciiHeader(MSG_CC_HEADER_MASK, value);}
	const char* GetCc() {return GetHeader(MSG_CC_HEADER_MASK);}

	NS_IMETHOD SetBcc(const PRUnichar *value);
	NS_IMETHOD GetBcc(PRUnichar **_retval);
	NS_IMETHOD SetBcc(const char *value) {return SetAsciiHeader(MSG_BCC_HEADER_MASK, value);}
	const char* GetBcc() {return GetHeader(MSG_BCC_HEADER_MASK);}

	NS_IMETHOD SetFcc(const PRUnichar *value);
	NS_IMETHOD GetFcc(PRUnichar **_retval);
	NS_IMETHOD SetFcc(const char *value) {return SetAsciiHeader(MSG_FCC_HEADER_MASK, value);}
	const char* GetFcc() {return GetHeader(MSG_FCC_HEADER_MASK);}

	NS_IMETHOD SetNewsFcc(const PRUnichar *value);
	NS_IMETHOD GetNewsFcc(PRUnichar **_retval);
	NS_IMETHOD SetNewsFcc(const char *value) {return SetAsciiHeader(MSG_NEWS_FCC_HEADER_MASK, value);}
	const char* GetNewsFcc() {return GetHeader(MSG_NEWS_FCC_HEADER_MASK);}

	NS_IMETHOD SetNewsBcc(const PRUnichar *value);
	NS_IMETHOD GetNewsBcc(PRUnichar **_retval);
	NS_IMETHOD SetNewsBcc(const char *value) {return SetAsciiHeader(MSG_NEWS_BCC_HEADER_MASK, value);}
	const char* GetNewsBcc() {return GetHeader(MSG_NEWS_BCC_HEADER_MASK);}

	NS_IMETHOD SetNewsgroups(const PRUnichar *value);
	NS_IMETHOD GetNewsgroups(PRUnichar **_retval);
	NS_IMETHOD SetNewsgroups(const char *value) {return SetAsciiHeader(MSG_NEWSGROUPS_HEADER_MASK, value);}
	const char* GetNewsgroups() {return GetHeader(MSG_NEWSGROUPS_HEADER_MASK);}

	NS_IMETHOD SetFollowupTo(const PRUnichar *value);
	NS_IMETHOD GetFollowupTo(PRUnichar **_retval);
	NS_IMETHOD SetFollowupTo(const char *value) {return SetAsciiHeader(MSG_FOLLOWUP_TO_HEADER_MASK, value);}
	const char* GetFollowupTo() {return GetHeader(MSG_FOLLOWUP_TO_HEADER_MASK);}

	NS_IMETHOD SetSubject(const PRUnichar *value);
	NS_IMETHOD GetSubject(PRUnichar **_retval);
	NS_IMETHOD SetSubject(const char *value) {return SetAsciiHeader(MSG_SUBJECT_HEADER_MASK, value);}
	const char* GetSubject() {return GetHeader(MSG_SUBJECT_HEADER_MASK);}

	NS_IMETHOD SetAttachments(const PRUnichar *value);
	NS_IMETHOD GetAttachments(PRUnichar **_retval);
	NS_IMETHOD SetAttachments(const char *value) {return SetAsciiHeader(MSG_ATTACHMENTS_HEADER_MASK, value);}
	const char* GetAttachments() {return GetHeader(MSG_ATTACHMENTS_HEADER_MASK);}

	NS_IMETHOD SetOrganization(const PRUnichar *value);
	NS_IMETHOD GetOrganization(PRUnichar **_retval);
	NS_IMETHOD SetOrganization(const char *value) {return SetAsciiHeader(MSG_ORGANIZATION_HEADER_MASK, value);}
	const char* GetOrganization() {return GetHeader(MSG_ORGANIZATION_HEADER_MASK);}

	NS_IMETHOD SetReferences(const PRUnichar *value);
	NS_IMETHOD GetReferences(PRUnichar **_retval);
	NS_IMETHOD SetReferences(const char *value) {return SetAsciiHeader(MSG_REFERENCES_HEADER_MASK, value);}
	const char* GetReferences() {return GetHeader(MSG_REFERENCES_HEADER_MASK);}

	NS_IMETHOD SetOtherRandomHeaders(const PRUnichar *value);
	NS_IMETHOD GetOtherRandomHeaders(PRUnichar **_retval);
	NS_IMETHOD SetOtherRandomHeaders(const char *value) {return SetAsciiHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK, value);}
	const char* GetOtherRandomHeaders() {return GetHeader(MSG_OTHERRANDOMHEADERS_HEADER_MASK);}

	NS_IMETHOD SetNewspostUrl(const PRUnichar *value);
	NS_IMETHOD GetNewspostUrl(PRUnichar **_retval);
	NS_IMETHOD SetNewspostUrl(const char *value) {return SetAsciiHeader(MSG_NEWSPOSTURL_HEADER_MASK, value);}
	const char* GetNewspostUrl() {return GetHeader(MSG_NEWSPOSTURL_HEADER_MASK);}

	NS_IMETHOD SetDefaultBody(const PRUnichar *value);
	NS_IMETHOD GetDefaultBody(PRUnichar **_retval);
	NS_IMETHOD SetDefaultBody(const char *value) {return SetAsciiHeader(MSG_DEFAULTBODY_HEADER_MASK, value);}
	const char* GetDefaultBody() {return GetHeader(MSG_DEFAULTBODY_HEADER_MASK);}

	NS_IMETHOD SetPriority(const PRUnichar *value);
	NS_IMETHOD GetPriority(PRUnichar **_retval);
	NS_IMETHOD SetPriority(const char *value) {return SetAsciiHeader(MSG_PRIORITY_HEADER_MASK, value);}
	const char* GetPriority() {return GetHeader(MSG_PRIORITY_HEADER_MASK);}

	NS_IMETHOD SetMessageEncoding(const PRUnichar *value);
	NS_IMETHOD GetMessageEncoding(PRUnichar **_retval);
	NS_IMETHOD SetMessageEncoding(const char *value) {return SetAsciiHeader(MSG_MESSAGE_ENCODING_HEADER_MASK, (const char *)value);}
	const char* GetMessageEncoding() {return GetHeader(MSG_MESSAGE_ENCODING_HEADER_MASK);}

	NS_IMETHOD SetCharacterSet(const PRUnichar *value);
	NS_IMETHOD GetCharacterSet(PRUnichar **_retval);
	NS_IMETHOD SetCharacterSet(const char *value) {return SetAsciiHeader(MSG_CHARACTER_SET_HEADER_MASK, (const char *)value);}
	const char* GetCharacterSet() {return GetHeader(MSG_CHARACTER_SET_HEADER_MASK);}

	NS_IMETHOD SetMessageId(const PRUnichar *value);
	NS_IMETHOD GetMessageId(PRUnichar **_retval);
	NS_IMETHOD SetMessageId(const char *value) {return SetAsciiHeader(MSG_MESSAGE_ID_HEADER_MASK, value);}
	const char* GetMessageId() {return GetHeader(MSG_MESSAGE_ID_HEADER_MASK);}

	NS_IMETHOD SetHTMLPart(const PRUnichar *value);
	NS_IMETHOD GetHTMLPart(PRUnichar **_retval);
	NS_IMETHOD SetHTMLPart(const char *value) {return SetAsciiHeader(MSG_HTML_PART_HEADER_MASK, value);}
	const char* GetHTMLPart() {return GetHeader(MSG_HTML_PART_HEADER_MASK);}

	NS_IMETHOD SetTemplateName(const PRUnichar *value);
	NS_IMETHOD GetTemplateName(PRUnichar **_retval);
	NS_IMETHOD SetTemplateName(const char *value) {return SetAsciiHeader(MSG_X_TEMPLATE_HEADER_MASK, value);}
	const char* GetTemplateName() {return GetHeader(MSG_X_TEMPLATE_HEADER_MASK);}

	NS_IMETHOD SetReturnReceipt(PRBool value);
	NS_IMETHOD GetReturnReceipt(PRBool *_retval);
	PRBool GetReturnReceipt() {return GetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK);}

	NS_IMETHOD SetAttachVCard(PRBool value);
	NS_IMETHOD GetAttachVCard(PRBool *_retval);
	PRBool GetAttachVCard() {return GetBoolHeader(MSG_RETURN_RECEIPT_BOOL_HEADER_MASK);}

	NS_IMETHOD SetUUEncodeAttachments(PRBool value);
	NS_IMETHOD GetUUEncodeAttachments(PRBool *_retval);
	PRBool GetUUEncodeAttachments() {return GetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK);}


	NS_IMETHOD SetBody(const PRUnichar *value);
	NS_IMETHOD GetBody(PRUnichar **_retval);
	NS_IMETHOD SetBody(const char *value);
	const char* GetBody();

	nsresult AppendBody(char*);

	// When forwarding a bunch of messages, we can have a bunch of
	// "forward url's" instead of an attachment.

	nsresult AddForwardURL(const char*);

	PRInt32 GetNumForwardURL();
	const char* GetForwardURL(PRInt32 which);

	PRInt32 GetReturnReceiptType() { return m_receiptType; };
	void SetReturnReceiptType(PRInt32 type) {m_receiptType = type;};

	NS_IMETHOD  SetTheForcePlainText(PRBool value);
	NS_IMETHOD  GetTheForcePlainText(PRBool *_retval);
	void        SetForcePlainText(PRBool value) {m_force_plain_text = value;}
	PRBool      GetForcePlainText() {return m_force_plain_text;}

	void SetUseMultipartAlternative(PRBool value) {m_multipart_alt = value;}
	PRBool GetUseMultipartAlternative() {return m_multipart_alt;}

protected:
	nsresult DecodeHeader(MSG_HEADER_SET header);

	// These methods allow news URLs in the newsgroups header
	HJ30181

	#define MAX_HEADERS		32
	char*		m_headers[MAX_HEADERS];
	char*		m_body;
	char**		m_forwardurl;
	PRInt32		m_numforward;
	PRInt32		m_maxforward;
	PRBool		m_boolHeaders[MSG_LAST_BOOL_HEADER_MASK];
	PRBool		m_force_plain_text;
	PRBool		m_multipart_alt;
	PRInt32		m_receiptType; /* 0:None 1:DSN 2:MDN 3:BOTH */
	nsString	m_internalCharSet;

};


#endif /* _MsgCompFields_H_ */
