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

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsMimeTransition.h"
#ifndef XP_MAC
	#include "nsTextFragment.h"
#endif
#include "msgCore.h"
#include "mimebuf.h"

extern "C" int XP_FORWARDED_MESSAGE_ATTACHMENT = 100;
extern "C" int MK_UNABLE_TO_OPEN_TMP_FILE = 101;
extern "C" int MK_OUT_OF_MEMORY = 102;
extern "C" int MK_MSG_XSENDER_INTERNAL = 103;
extern "C" int MK_MSG_USER_WROTE = 104;
extern "C" int MK_MSG_SHOW_ATTACHMENT_PANE = 105;
extern "C" int MK_MSG_NO_HEADERS = 106;
extern "C" int MK_MSG_LINK_TO_DOCUMENT = 107;
extern "C" int MK_MSG_IN_MSG_X_USER_WROTE = 108;
extern "C" int MK_MSG_DOCUMENT_INFO = 109;
extern "C" int MK_MSG_ATTACHMENT = 110;
extern "C" int MK_MSG_ADDBOOK_MOUSEOVER_TEXT = 111;
extern "C" int MK_MIMEHTML_DISP_TO = 112;
extern "C" int MK_MIMEHTML_DISP_SUBJECT = 113;
extern "C" int MK_MIMEHTML_DISP_SENDER = 114;
extern "C" int MK_MIMEHTML_DISP_RESENT_TO = 115;
extern "C" int MK_MIMEHTML_DISP_RESENT_SENDER = 116;
extern "C" int MK_MIMEHTML_DISP_RESENT_MESSAGE_ID = 117;
extern "C" int MK_MIMEHTML_DISP_RESENT_FROM = 118;
extern "C" int MK_MIMEHTML_DISP_RESENT_DATE = 119;
extern "C" int MK_MIMEHTML_DISP_RESENT_COMMENTS = 120;
extern "C" int MK_MIMEHTML_DISP_RESENT_CC = 121;
extern "C" int MK_MIMEHTML_DISP_REPLY_TO = 122;
extern "C" int MK_MIMEHTML_DISP_REFERENCES = 123;
extern "C" int MK_MIMEHTML_DISP_ORGANIZATION = 124;
extern "C" int MK_MIMEHTML_DISP_NEWSGROUPS = 125;
extern "C" int MK_MIMEHTML_DISP_MESSAGE_ID = 126;
extern "C" int MK_MIMEHTML_DISP_FROM = 127;
extern "C" int MK_MIMEHTML_DISP_FOLLOWUP_TO = 128;
extern "C" int MK_MIMEHTML_DISP_DATE = 129;
extern "C" int MK_MIMEHTML_DISP_CC = 130;
extern "C" int MK_MIMEHTML_DISP_BCC = 131;

/* 
 * This is a transitional file that will help with the various message
 * ID definitions and the functions to retrieve the error string
 */

extern "C" char 
*XP_GetStringForHTML (int i, PRInt16 wincsid, char* english)
{
  return english; 
}

extern "C" char 
*XP_GetString (int i)
{
  return PL_strdup("Error condition");
}


extern "C" NET_cinfo *
NET_cinfo_find_type (char *uri)
{
  return NULL;  
}

extern "C" NET_cinfo *
NET_cinfo_find_info_by_type (char *uri)
{
  return NULL;  
}
