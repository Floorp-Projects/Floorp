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

#ifndef _NNTPCore_H__
#define _NNTPCore_H__

#define NEWS_MSGS_URL       "chrome://messenger/locale/news.properties"

// The following string constants are protocol strings. I'm defining them as macros here
// so I don't have to sprinkle all of the strings throughout the protocol. 
#define NNTP_CMD_LIST_EXTENSIONS		"LIST EXTENSIONS" CRLF
#define NNTP_CMD_MODE_READER			"MODE READER" CRLF
#define NNTP_CMD_LIST_SEARCHES			"LIST SEARCHES" CRLF
#define NNTP_CMD_LIST_SEARCH_FIELDS 		"LIST SRCHFIELDS" CRLF
#define NNTP_CMD_GET_PROPERTIES     		"GET" CRLF
#define NNTP_CMD_LIST_SUBSCRIPTIONS 		"LIST SUBSCRIPTIONS" CRLF
#define NNTP_CMD_POST				"POST" CRLF
#define NNTP_CMD_QUIT				"QUIT" CRLF

// end of protocol strings

#define MK_NNTP_RESPONSE_HELP 100

#define MK_NNTP_RESPONSE_POSTING_ALLOWED 200
#define MK_NNTP_RESPONSE_POSTING_DENIED 201

#define MK_NNTP_RESPONSE_DISCONTINUED 400

#define MK_NNTP_RESPONSE_COMMAND_UNKNOWN 500
#define MK_NNTP_RESPONSE_SYNTAX_ERROR 501
#define MK_NNTP_RESPONSE_PERMISSION_DENIED 502
#define MK_NNTP_RESPONSE_SERVER_ERROR 503

#define MK_NNTP_RESPONSE_ARTICLE_BOTH 220
#define MK_NNTP_RESPONSE_ARTICLE_HEAD 221
#define MK_NNTP_RESPONSE_ARTICLE_BODY 222
#define MK_NNTP_RESPONSE_ARTICLE_NONE 223
#define MK_NNTP_RESPONSE_ARTICLE_NO_GROUP 412
#define MK_NNTP_RESPONSE_ARTICLE_NO_CURRENT 420
#define MK_NNTP_RESPONSE_ARTICLE_NONEXIST 423
#define MK_NNTP_RESPONSE_ARTICLE_NOTFOUND 430

#define MK_NNTP_RESPONSE_GROUP_SELECTED   211
#define MK_NNTP_RESPONSE_GROUP_NO_GROUP   411

#define MK_NNTP_RESPONSE_IHAVE_OK         235
#define MK_NNTP_RESPONSE_IHAVE_ARTICLE    335
#define MK_NNTP_RESPONSE_IHAVE_NOT_WANTED 435
#define MK_NNTP_RESPONSE_IHAVE_FAILED     436
#define MK_NNTP_RESPONSE_IHAVE_REJECTED   437

#define MK_NNTP_RESPONSE_LAST_OK          223
#define MK_NNTP_RESPONSE_LAST_NO_GROUP    412
#define MK_NNTP_RESPONSE_LAST_NO_CURRENT  420
#define MK_NNTP_RESPONSE_LAST_NO_ARTICLE  422

#define MK_NNTP_RESPONSE_LIST_OK          215

#define MK_NNTP_RESPONSE_NEWGROUPS_OK     231

#define MK_NNTP_RESPONSE_NEWNEWS_OK       230

#define MK_NNTP_RESPONSE_NEXT_OK          223
#define MK_NNTP_RESPONSE_NEXT_NO_GROUP    412
#define MK_NNTP_RESPONSE_NEXT_NO_CURRENT  420
#define MK_NNTP_RESPONSE_NEXT_NO_ARTICLE  421

#define MK_NNTP_RESPONSE_POST_OK          240
#define MK_NNTP_RESPONSE_POST_SEND_NOW    340
#define MK_NNTP_RESPONSE_POST_DENIED      440
#define MK_NNTP_RESPONSE_POST_FAILED      441

#define MK_NNTP_RESPONSE_QUIT_OK          205

#define MK_NNTP_RESPONSE_SLAVE_OK         202

#define MK_NNTP_RESPONSE_CHECK_NO_ARTICLE 238
#define MK_NNTP_RESPONSE_CHECK_NO_ACCEPT  400
#define MK_NNTP_RESPONSE_CHECK_LATER      431
#define MK_NNTP_RESPONSE_CHECK_DONT_SEND  438
#define MK_NNTP_RESPONSE_CHECK_DENIED     480
#define MK_NNTP_RESPONSE_CHECK_ERROR      500

#define MK_NNTP_RESPONSE_XHDR_OK          221
#define MK_NNTP_RESPONSE_XHDR_NO_GROUP    412
#define MK_NNTP_RESPONSE_XHDR_NO_CURRENT  420
#define MK_NNTP_RESPONSE_XHDR_NO_ARTICLE  430
#define MK_NNTP_RESPONSE_XHDR_DENIED      502

#define MK_NNTP_RESPONSE_XOVER_OK         224
#define MK_NNTP_RESPONSE_XOVER_NO_GROUP   412
#define MK_NNTP_RESPONSE_XOVER_NO_CURRENT 420
#define MK_NNTP_RESPONSE_XOVER_DENIED     502

#define MK_NNTP_RESPONSE_XPAT_OK          221
#define MK_NNTP_RESPONSE_XPAT_NO_ARTICLE  430
#define MK_NNTP_RESPONSE_XPAT_DENIED      502

#define MK_NNTP_RESPONSE_AUTHINFO_OK      281
#define MK_NNTP_RESPONSE_AUTHINFO_CONT    381
#define MK_NNTP_RESPONSE_AUTHINFO_REQUIRE 480
#define MK_NNTP_RESPONSE_AUTHINFO_REJECT  482
#define MK_NNTP_RESPONSE_AUTHINFO_DENIED  502

#define MK_NNTP_RESPONSE_

#define MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK      250
#define MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_CONT    350
#define MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_REQUIRE 450
#define MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_REJECT  452

#define MK_NNTP_RESPONSE_TYPE_INFO    1
#define MK_NNTP_RESPONSE_TYPE_OK      2
#define MK_NNTP_RESPONSE_TYPE_CONT    3
#define MK_NNTP_RESPONSE_TYPE_CANNOT  4
#define MK_NNTP_RESPONSE_TYPE_ERROR   5

#define MK_NNTP_RESPONSE_TYPE(x) (x/100)

// the following used to be defined in allxpstr.h. Until we find a new values for these, 
// I'm defining them here because I don't want to link against xplib.lib...(mscott)

#define MK_DATA_LOADED		1
#define MK_EMPTY_NEWS_LIST	-227
#define MK_INTERRUPTED		-201
#define MK_MALFORMED_URL_ERROR	-209
#define MK_NEWS_ERROR_FMT		-430
#define MK_NNTP_CANCEL_CONFIRM	-426
#define MK_NNTP_CANCEL_DISALLOWED -427
#define MK_NNTP_NOT_CANCELLED    -429
#define MK_OUT_OF_MEMORY		 -207
#define XP_CONFIRM_SAVE_NEWSGROUPS			-1
#define XP_HTML_ARTICLE_EXPIRED				-1
#define XP_HTML_NEWS_ERROR					-1
#define XP_PROGRESS_READ_NEWSGROUPINFO		 1
#define XP_PROGRESS_RECEIVE_ARTICLE			 1
#define XP_PROGRESS_RECEIVE_LISTARTICLES	 1
#define XP_PROGRESS_RECEIVE_NEWSGROUP		 1
#define XP_PROGRESS_SORT_ARTICLES		     1
#define XP_PROGRESS_READ_NEWSGROUP_COUNTS	 1
#define XP_THERMO_PERCENT_FORM				 1
#define XP_PROMPT_ENTER_USERNAME			 1
#define MK_BAD_NNTP_CONNECTION			  -216
#define MK_NNTP_AUTH_FAILED			      -260
#define MK_NNTP_ERROR_MESSAGE		      -304
#define MK_NNTP_NEWSGROUP_SCAN_ERROR	  -305
#define MK_NNTP_SERVER_ERROR		      -217
#define MK_NNTP_SERVER_NOT_CONFIGURED     -307
#define MK_TCP_READ_ERROR				  -252
#define MK_TCP_WRITE_ERROR				  -236
#define MK_NNTP_CANCEL_ERROR			  -428
#define XP_CONNECT_NEWS_HOST_CONTACTED_WAITING_FOR_REPLY  1
#define XP_PLEASE_ENTER_A_PASSWORD_FOR_NEWS_SERVER_ACCESS 1
#define XP_GARBAGE_COLLECTING							  1
#define XP_MESSAGE_SENT_WAITING_NEWS_REPLY				  1
#define MK_MSG_DELIV_NEWS								  1
#define MK_MSG_COLLABRA_DISABLED						  1
#define MK_MSG_EXPIRE_NEWS_ARTICLES					      1
#define MK_MSG_HTML_IMAP_NO_CACHED_BODY					  1
#define MK_MSG_CANT_MOVE_FOLDER							  1

#endif /* NNTPCore_H__ */
