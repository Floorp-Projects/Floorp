/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _msgHeaderMasks_h_
#define _msgHeaderMasks_h_

DO NOT USE ANYMORE!!!
/* This set enumerates the header fields which may be displayed in the
   message composition window.
 */
#define MSG_FROM_HEADER_MASK                0x00000001
#define MSG_REPLY_TO_HEADER_MASK            0x00000002
#define MSG_TO_HEADER_MASK					        0x00000004
#define MSG_CC_HEADER_MASK					        0x00000008
#define MSG_BCC_HEADER_MASK					        0x00000010
#define MSG_FCC_HEADER_MASK					        0x00000020
#define MSG_NEWSGROUPS_HEADER_MASK          0x00000040
#define MSG_FOLLOWUP_TO_HEADER_MASK         0x00000080
#define MSG_SUBJECT_HEADER_MASK             0x00000100
#define MSG_ATTACHMENTS_HEADER_MASK         0x00000200

/* These next four are typically not ever displayed in the UI, but are still
   stored and used internally. */
#define MSG_ORGANIZATION_HEADER_MASK		    0x00000400
#define MSG_REFERENCES_HEADER_MASK			    0x00000800
#define MSG_OTHERRANDOMHEADERS_HEADER_MASK  0x00001000
#define MSG_NEWSPOSTURL_HEADER_MASK			    0x00002000

#define MSG_PRIORITY_HEADER_MASK			      0x00004000
//#define MSG_NEWS_FCC_HEADER_MASK			0x00008000
//#define MSG_MESSAGE_ENCODING_HEADER_MASK	0x00010000
#define MSG_CHARACTER_SET_HEADER_MASK		    0x00008000
#define MSG_MESSAGE_ID_HEADER_MASK			    0x00010000
//#define MSG_NEWS_BCC_HEADER_MASK            0x00080000

/* This is also not exposed to the UI; it's used internally to help remember
   whether the original message had an HTML portion that we can quote. */
//#define MSG_HTML_PART_HEADER_MASK			0x00100000

/* The "body=" pseudo-header (as in "mailto:me?body=hi+there") */
//#define MSG_DEFAULTBODY_HEADER_MASK			0x00200000

#define MSG_X_TEMPLATE_HEADER_MASK          0x00020000

#define MSG_FCC2_HEADER_MASK                0x00400000

/* IMAP folders for posting */
//#define MSG_IMAP_FOLDER_HEADER_MASK			0x02000000


#endif
