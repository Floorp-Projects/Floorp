/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsMimeTransition_h_
#define nsMimeTransition_h

#include "prtypes.h"
#include "plstr.h"
#include "net.h"

extern "C" int XP_FORWARDED_MESSAGE_ATTACHMENT;
extern "C" int MK_UNABLE_TO_OPEN_TMP_FILE;
extern "C" int MK_OUT_OF_MEMORY;
extern "C" int MK_MSG_XSENDER_INTERNAL;
extern "C" int MK_MSG_USER_WROTE;
extern "C" int MK_MSG_SHOW_ATTACHMENT_PANE;
extern "C" int MK_MSG_NO_HEADERS;
extern "C" int MK_MSG_LINK_TO_DOCUMENT;
extern "C" int MK_MSG_IN_MSG_X_USER_WROTE;
extern "C" int MK_MSG_DOCUMENT_INFO;
extern "C" int MK_MSG_ATTACHMENT;
extern "C" int MK_MSG_ADDBOOK_MOUSEOVER_TEXT;
extern "C" int MK_MIMEHTML_DISP_TO;
extern "C" int MK_MIMEHTML_DISP_SUBJECT;
extern "C" int MK_MIMEHTML_DISP_SENDER;
extern "C" int MK_MIMEHTML_DISP_RESENT_TO;
extern "C" int MK_MIMEHTML_DISP_RESENT_SENDER;
extern "C" int MK_MIMEHTML_DISP_RESENT_MESSAGE_ID;
extern "C" int MK_MIMEHTML_DISP_RESENT_FROM;
extern "C" int MK_MIMEHTML_DISP_RESENT_DATE;
extern "C" int MK_MIMEHTML_DISP_RESENT_COMMENTS;
extern "C" int MK_MIMEHTML_DISP_RESENT_CC;
extern "C" int MK_MIMEHTML_DISP_REPLY_TO;
extern "C" int MK_MIMEHTML_DISP_REFERENCES;
extern "C" int MK_MIMEHTML_DISP_ORGANIZATION;
extern "C" int MK_MIMEHTML_DISP_NEWSGROUPS;
extern "C" int MK_MIMEHTML_DISP_MESSAGE_ID;
extern "C" int MK_MIMEHTML_DISP_FROM;
extern "C" int MK_MIMEHTML_DISP_FOLLOWUP_TO;
extern "C" int MK_MIMEHTML_DISP_DATE;
extern "C" int MK_MIMEHTML_DISP_CC;
extern "C" int MK_MIMEHTML_DISP_BCC;

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

extern "C" char 
*XP_GetStringForHTML (int i, PRInt16 wincsid, char* english);

extern "C" char 
*XP_GetString (int i);

extern "C" NET_cinfo *
NET_cinfo_find_type (char *uri);

extern "C" NET_cinfo *
NET_cinfo_find_info_by_type (char *uri);

extern "C" int
nsScanForURLs(const char *input, int32 input_size,
              char *output, int output_size, XP_Bool urls_only);

extern "C" char *
nsMakeAbsoluteURL(char * absolute_url, char * relative_url);

#endif /* nsMimeTransition_h_ */
