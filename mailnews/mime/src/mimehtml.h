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

/* 
 * The purpose of this header file is to centralize all of the
 * generated HTML/CSS for libmime. 
 */
#ifndef _MIMEHTML_H_
#define _MIMEHTML_H_

/* 
 * HTML in mimehdrs.c 
 */
#define MHTML_BLOCKQUOTE_BEGIN   "<BLOCKQUOTE TYPE=CITE>"
#define MHTML_STYLE_IMPORT       "<HTML><HEAD><LINK REL=\"stylesheet\" HREF=\"resource:/res/mail.css\">"
#define MHTML_JS_IMPORT          "<SCRIPT LANGUAGE=\"JavaScript1.2\" SRC=\"resource:/res/mail.js\"></SCRIPT></HEAD>"
#define MHTML_HEADER_TABLE       "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define MHTML_TABLE_BEGIN        "<TABLE><TR><TD>"
#define MHTML_TABLE_COLUMN_BEGIN "<TR><TD><B><I>"
#define MHTML_TABLE_COLUMN_END   "</I></B></TD></TR>"
#define MHTML_TABLE_END          "</TABLE>"
#define MHTML_COL_AND_TABLE_END  "</td></tr></table>"
#define MHTML_TITLE              "<TITLE>%s</TITLE>\n"

#define MHTML_CLIP_START         "</TD><TD VALIGN=TOP><DIV "
#ifndef XP_MAC
#define MHTML_CLIP_DIM           "CLIP=0,0,30,30 "
#else
#define MHTML_CLIP_DIM           "CLIP=0,0,48,30 "
#endif
#define MHTML_NOATTACH_ID        "ID=noattach%ld>"
#define MHTML_ENDBEGIN_DIV       "</DIV><DIV LOCKED "
#define MHTML_ATTACH_ID          "ID=attach%ld style=\"display: 'none';\" >"
#define MHTML_DISP_ICON_LINK     "<a href=javascript:toggleAttachments(); onMouseOver=\"window.status='%s'; return true\"; onMouseOut=\"window.status=''; return true\" PRIVATE>"
#define MHTML_ATTACH_ICON        "<IMG SRC=resource:/res/network/gopher-text.gif BORDER=0>"
#define MHTML_CLIP_END           "</a></DIV>"

#define HEADER_START_JUNK        "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK       ": </TH><TD>"
#define HEADER_END_JUNK          "</TD></TR>"

/* 
 * HTML in mimemult.c 
 */



#endif /* _MIMEHTML_H_ */

