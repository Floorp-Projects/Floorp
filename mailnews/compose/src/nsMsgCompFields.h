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

/*JFD #include "msgzap.h" */
#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "net.h" /* should be defined into msgCore.h? */
#include "intl_csi.h"
#include "msgcom.h"
#include "nsMsgHeaderMasks.h"

#include "MsgCompGlue.h"

#include "nsIMsgCompFields.h"


/* Note that all the "Get" methods never return NULL (except in case of serious
   error, like an illegal parameter); rather, they return "" if things were set
   to NULL.  This makes it real handy for the callers. */

class nsMsgCompFields : public nsIMsgCompFields, public MSG_ZapIt {
public:
	nsMsgCompFields();
	virtual ~nsMsgCompFields();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	/* this is just for testing purpose, must be removed before shipping */
	NS_IMETHOD Test() {printf("nsMsgCompField: Test Succesfull\n"); return NS_OK;}

	NS_IMETHOD Copy(const nsIMsgCompFields* pMsgCompFields);

	NS_IMETHOD SetHeader(PRInt32 header, const char *value, PRInt32 *_retval);
	NS_IMETHOD GetHeader(PRInt32 header, char **_retval);

	NS_IMETHOD SetBoolHeader(PRInt32 header, PRBool bValue, PRInt32 *_retval);
	NS_IMETHOD GetBoolHeader(PRInt32 header, PRBool *_retval);


	/* Convenience routines to get and set header's value... */
	NS_IMETHOD SetFrom(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetFrom(char **_retval);

	NS_IMETHOD SetReplyTo(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetReplyTo(char **_retval);

	NS_IMETHOD SetTo(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetTo(char **_retval);

	NS_IMETHOD SetCc(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetCc(char **_retval);

	NS_IMETHOD SetBcc(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetBcc(char **_retval);

	NS_IMETHOD SetFcc(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetFcc(char **_retval);

	NS_IMETHOD SetNewsFcc(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetNewsFcc(char **_retval);

	NS_IMETHOD SetNewsBcc(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetNewsBcc(char **_retval);

	NS_IMETHOD SetNewsgroups(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetNewsgroups(char **_retval);

	NS_IMETHOD SetFollowupTo(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetFollowupTo(char **_retval);

	NS_IMETHOD SetSubject(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetSubject(char **_retval);

	NS_IMETHOD SetAttachments(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetAttachments(char **_retval);

	NS_IMETHOD SetOrganization(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetOrganization(char **_retval);

	NS_IMETHOD SetReferences(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetReferences(char **_retval);

	NS_IMETHOD SetOtherRandomHeaders(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetOtherRandomHeaders(char **_retval);

	NS_IMETHOD SetNewspostUrl(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetNewspostUrl(char **_retval);

	NS_IMETHOD SetDefaultBody(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetDefaultBody(char **_retval);

	NS_IMETHOD SetPriority(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetPriority(char **_retval);

	NS_IMETHOD SetMessageEncoding(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetMessageEncoding(char **_retval);

	NS_IMETHOD SetCharacterSet(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetCharacterSet(char **_retval);

	NS_IMETHOD SetMessageId(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetMessageId(char **_retval);

	NS_IMETHOD SetHTMLPart(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetHTMLPart(char **_retval);

	NS_IMETHOD SetTemplateName(const char *value, PRInt32 *_retval);
	NS_IMETHOD GetTemplateName(char **_retval);

	NS_IMETHOD SetReturnReceipt(PRBool value, PRInt32 *_retval);
	NS_IMETHOD GetReturnReceipt(PRBool *_retval);

	NS_IMETHOD SetAttachVCard(PRBool value, PRInt32 *_retval);
	NS_IMETHOD GetAttachVCard(PRBool *_retval);

	int SetBody(const char*);
	const char* GetBody();

	int AppendBody(const char*);

	// When forwarding a bunch of messages, we can have a bunch of
	// "forward url's" instead of an attachment.

	PRInt16 AddForwardURL(const char*);

	PRInt32 GetNumForwardURL();
	const char* GetForwardURL(PRInt32 which);

	PRInt32 GetReturnReceiptType() { return m_receiptType; };
	void SetReturnReceiptType(PRInt32 type) {m_receiptType = type;};

	void SetOwner(MSG_Pane *pane) {
		m_owner = pane;
	}

	MSG_Pane * GetOwner() { return m_owner; }


	void SetForcePlainText(PRBool value) {m_force_plain_text = value;}
	PRBool GetForcePlainText() {return m_force_plain_text;}
	void SetUseMultipartAlternative(PRBool value) {m_multipart_alt = value;}
	PRBool GetUseMultipartAlternative() {return m_multipart_alt;}

protected:
	PRInt16 DecodeHeader(MSG_HEADER_SET header);

	// These methods allow news URLs in the newsgroups header
	HJ30181
	PRInt16 ParseNewsgroupsForUrls (const char *value);

	#define MAX_HEADERS		32
	MSG_Pane*	m_owner;
	char*		m_headers[MAX_HEADERS];
	char*		m_body;
	char**		m_forwardurl;
	PRInt32		m_numforward;
	PRInt32		m_maxforward;
	PRBool		m_boolHeaders[MSG_LAST_BOOL_HEADER_MASK];
	PRBool		m_force_plain_text;
	PRBool		m_multipart_alt;
	PRInt32		m_receiptType; /* 0:None 1:DSN 2:MDN 3:BOTH */
};


#endif /* _MsgCompFields_H_ */
