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
#ifndef MSGMDN_H
#define MSGMDN_H

#include "xp.h"
#include "msgzap.h"
#include "libmime.h"

class MSG_Pane;
class MSG_FolderInfo;
struct message_header;

#define MDN_NOT_IN_TO_CC          ((int) 0x0001)
#define MDN_OUTSIDE_DOMAIN        ((int) 0x0002)

#define HEADER_RETURN_PATH					"Return-Path"
#define HEADER_DISPOSITION_NOTIFICATION_TO	"Disposition-Notification-To"
#define HEADER_APPARENTLY_TO				"Apparently-To"
#define HEADER_ORIGINAL_RECIPIENT			"Original-Recipient"
#define HEADER_REPORTING_UA                 "Reporting-UA"
#define HEADER_MDN_GATEWAY                  "MDN-Gateway"
#define HEADER_FINAL_RECIPIENT              "Final-Recipient"
#define HEADER_DISPOSITION                  "Disposition"
#define HEADER_ORIGINAL_MESSAGE_ID          "Original-Message-ID"
#define HEADER_FAILURE                      "Failure"
#define HEADER_ERROR                        "Error"
#define HEADER_WARNING                      "Warning"
#define HEADER_RETURN_RECEIPT_TO            "Return-Receipt-To"


class MSG_ProcessMdnNeededState : public MSG_ZapIt
{
public:
	enum EDisposeType {
		eDisplayed = 0x0,
		eDispatched,
		eProcessed,
		eDeleted,
		eDenied,
		eFailed
	};

public:
	MSG_ProcessMdnNeededState (EDisposeType intendedType,
							   MSG_Pane *pane,
							   MSG_FolderInfo *folder,
							   uint32 key,
							   MimeHeaders *srcHeader,
							   XP_Bool autoAction = FALSE);

	MSG_ProcessMdnNeededState (EDisposeType intendedType,
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
							   XP_Bool autoAction = TRUE);

	virtual ~MSG_ProcessMdnNeededState ();
	static void PostSendMdn(URL_Struct *url, int status, MWContext *context);
	
protected:

    XP_Bool ProcessSendMode();		// this should be called prior to
								// ValidateReturnPath();
	XP_Bool ValidateReturnPath();
	XP_Bool MailAddrMatch(const char *addr1, const char *addr2);
	XP_Bool NotInToOrCc();

	void StoreImapMDNSentFlag(MSG_FolderInfo *folder, uint32 key);

	void CreateMdnMsg();
	int32 CreateFirstPart();
	int32 CreateSecondPart();
	int32 CreateThirdPart();
	void DoSendMdn();

	// helper fucntion
	void InitAndProcess();
	int32 OutputAllHeaders();
	int32 WriteString(const char *str);

protected:
	EDisposeType m_disposeType;
	MSG_Pane *m_pane;
	XP_File m_outFile;
	int16 m_csid;
	char *m_msgFileName;
	char *m_mimeSeparator;
	XP_Bool m_reallySendMdn;	/* really send mdn? */
	XP_Bool m_autoSend;			/* automatic vs manual send mode */
	XP_Bool m_autoAction;		/* automatic vs manual action */
	char *m_returnPath;
	char *m_dispositionNotificationTo;
	char *m_date;
	char *m_to;
	char *m_cc;
	char *m_subject;
	char *m_messageId;
	char *m_originalRecipient;
	char *m_all_headers;
	int32 m_all_headers_size;
};

#endif
