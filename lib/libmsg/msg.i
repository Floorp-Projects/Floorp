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
#ifndef MSG_I
#define MSG_I

#include "resdef.h"

/*******************************************************************
   Syntax:  ResDef(Token, Id, String)

   Note:  Please change Id based on IDS_MSG_BASE

********************************************************************/

BEGIN_STR (MSG_strings)


/* Search attribute names */
/* WARNING -- order must match MSG_SearchAttribute enum */
ResDef(XP_SEARCH_SENDER,         XP_MSG_BASE + 648, "from")
ResDef(XP_SEARCH_SUBJECT,        XP_MSG_BASE + 649, "subject")
ResDef(XP_SEARCH_BODY,           XP_MSG_BASE + 650, "body")
ResDef(XP_SEARCH_DATE,           XP_MSG_BASE + 651, "date")
ResDef(XP_SEARCH_PRIORITY,       XP_MSG_BASE + 652, "priority")
ResDef(XP_SEARCH_STATUS,         XP_MSG_BASE + 653, "status")
ResDef(XP_SEARCH_TO,             XP_MSG_BASE + 654, "to")
ResDef(XP_SEARCH_CC,             XP_MSG_BASE + 655, "CC")
ResDef(XP_SEARCH_TO_OR_CC,       XP_MSG_BASE + 656, "to or CC")
ResDef(XP_SEARCH_COMMON_NAME,    XP_MSG_BASE + 657, "name")
ResDef(XP_SEARCH_EMAIL_ADDRESS,  XP_MSG_BASE + 658, "e-mail address")	
ResDef(XP_SEARCH_PHONE_NUMBER,   XP_MSG_BASE + 659, "phone number")
ResDef(XP_SEARCH_ORG,            XP_MSG_BASE + 660, "organization")
ResDef(XP_SEARCH_ORG_UNIT,       XP_MSG_BASE + 661, "organizational unit")
ResDef(XP_SEARCH_LOCALITY,       XP_MSG_BASE + 662, "locality")
ResDef(XP_SEARCH_STREET_ADDRESS, XP_MSG_BASE + 663, "street address")

/* Search operator names */
/* WARNING -- order must match MSG_SearchOperator enum */
ResDef(XP_SEARCH_CONTAINS,       XP_MSG_BASE + 664, "contains")
ResDef(XP_SEARCH_DOESNT_CONTAIN, XP_MSG_BASE + 665, "doesn't contain")
ResDef(XP_SEARCH_IS,             XP_MSG_BASE + 666, "is")
ResDef(XP_SEARCH_ISNT,           XP_MSG_BASE + 667, "isn't")
ResDef(XP_SEARCH_IS_EMPTY,       XP_MSG_BASE + 668, "is empty")
ResDef(XP_SEARCH_IS_BEFORE,      XP_MSG_BASE + 669, "is before")
ResDef(XP_SEARCH_IS_AFTER,       XP_MSG_BASE + 670, "is after")
ResDef(XP_SEARCH_IS_HIGHER,      XP_MSG_BASE + 671, "is higher than")
ResDef(XP_SEARCH_IS_LOWER,       XP_MSG_BASE + 672, "is lower than")
ResDef(XP_SEARCH_SOUNDS_LIKE,    XP_MSG_BASE + 673, "sounds like")
ResDef(XP_BEGINS_WITH,           XP_MSG_BASE + 674, "begins with")


/* Message priority names */
ResDef(XP_PRIORITY_NONE,         XP_MSG_BASE + 675, "none")
ResDef(XP_PRIORITY_LOWEST,       XP_MSG_BASE + 676, "lowest")
ResDef(XP_PRIORITY_LOW,          XP_MSG_BASE + 677, "low")
ResDef(XP_PRIORITY_NORMAL,       XP_MSG_BASE + 678, "normal")
ResDef(XP_PRIORITY_HIGH,         XP_MSG_BASE + 679, "high")
ResDef(XP_PRIORITY_HIGHEST,      XP_MSG_BASE + 680, "highest")

/* Message status names */
ResDef(XP_STATUS_READ,           XP_MSG_BASE + 681, "read")
ResDef(XP_STATUS_REPLIED,        XP_MSG_BASE + 682, "replied")

ResDef(MK_MSG_RENAME_FOLDER,     XP_MSG_BASE + 683, "Rename selected folder")


/* 684 unused */

ResDef(XP_FILTER_MOVE_TO_FOLDER, XP_MSG_BASE + 685,	 "Move to folder")
ResDef(XP_FILTER_CHANGE_PRIORITY,XP_MSG_BASE + 686,  "Change priority")
ResDef(XP_FILTER_DELETE,		 XP_MSG_BASE + 687,  "Delete")
ResDef(XP_FILTER_MARK_READ,		 XP_MSG_BASE + 688,	 "Mark read")
ResDef(XP_FILTER_KILL_THREAD,	 XP_MSG_BASE + 689,	 "Kill thread")
ResDef(XP_FILTER_WATCH_THREAD,	 XP_MSG_BASE + 690,	 "Watch thread")	  


/* 691, 692 unused */

ResDef (XP_SEARCH_DISPLAY_PARTIAL, XP_MSG_BASE + 693, "Display items found so far?")
ResDef (XP_LDAP_SIZELIMIT_EXCEEDED, XP_MSG_BASE + 694, "More than %d items were found. ")

ResDef (MK_MSG_READ_MORE, XP_MSG_BASE + 700, "Read More" )
ResDef (MK_MSG_NEXTUNREAD_THREAD, XP_MSG_BASE + 701, "Next Unread Thread" )
ResDef (MK_MSG_NEXTUNREAD_CATEGORY, XP_MSG_BASE + 702, "Next Unread Category" )
ResDef (MK_MSG_NEXTUNREAD_GROUP, XP_MSG_BASE + 703, "Next Unread Group" )

ResDef (MK_MSG_BY_PRIORITY,		XP_MSG_BASE + 704, "By Priority" )

ResDef (MK_MSG_FIND,			XP_MSG_BASE + 705, "Find..." )
ResDef (MK_MSG_BY_TYPE,			XP_MSG_BASE + 706, "By Type" )
ResDef (MK_MSG_BY_NAME,			XP_MSG_BASE + 707, "By Name" )
ResDef (MK_MSG_BY_NICKNAME,		XP_MSG_BASE + 708, "By Nickname" )
ResDef (MK_MSG_BY_EMAILADDRESS,	XP_MSG_BASE + 709, "By Email Address" )
ResDef (MK_MSG_BY_COMPANY,		XP_MSG_BASE + 710, "By Company" )
ResDef (MK_MSG_BY_LOCALITY,		XP_MSG_BASE + 711, "By Locality" )
ResDef (MK_MSG_SORT_DESCENDING,	XP_MSG_BASE + 712, "Descending" )
ResDef (MK_MSG_ADD_NAME,		XP_MSG_BASE + 713, "Add Name..." )
ResDef (MK_MSG_ADD_LIST,		XP_MSG_BASE + 714, "Add List..." )
ResDef (MK_MSG_PROPERTIES,		XP_MSG_BASE + 715, "Properties..." )

ResDef (MK_MSG_SEARCH_STATUS,   XP_MSG_BASE + 716, "Searching %s..." )
ResDef (MK_MSG_NEED_FULLNAME,   XP_MSG_BASE + 717, "You must enter a full name." )
ResDef (MK_MSG_NEED_GIVENNAME,  XP_MSG_BASE + 718, "You must enter a first name." )
ResDef (MK_MSG_REPARSE_FOLDER,	XP_MSG_BASE + 719, "Building summary file for %s...")

ResDef( MK_MSG_ALIVE_THREADS,	XP_MSG_BASE + 720, "View all threads")
ResDef( MK_MSG_KILLED_THREADS,	XP_MSG_BASE + 721, "View killed threads")
ResDef( MK_MSG_WATCHED_THREADS, XP_MSG_BASE + 722, "View watched threads with new messages")
ResDef( MK_MSG_THREADS_WITH_NEW, XP_MSG_BASE + 723, "View threads with new messages")

ResDef (XP_LDAP_DIALOG_TITLE,      XP_MSG_BASE + 724, "LDAP Directory Entry")
ResDef (XP_LDAP_OPEN_ENTRY_FAILED, XP_MSG_BASE + 725, "Failed to open entry for %s due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_OPEN_SERVER_FAILED,XP_MSG_BASE + 726, "Failed to open a connection to '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_BIND_FAILED,       XP_MSG_BASE + 727, "Failed to bind to '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_SEARCH_FAILED,     XP_MSG_BASE + 728, "Failed to search '%s' due to LDAP error '%s' (0x%02X)")
ResDef (XP_LDAP_MODIFY_FAILED,     XP_MSG_BASE + 729, "Failed to modify entry on '%s' due to LDAP error '%s' (0x%02X)")
ResDef (MK_SEARCH_HITS_COUNTER,    XP_MSG_BASE + 730, "Found %d matches")
ResDef( MK_MSG_UNSUBSCRIBE_GROUP,  XP_MSG_BASE + 731, "Are you sure you want to unsubscribe to %s?")

ResDef (MK_HDR_DOWNLOAD_COUNT,	   XP_MSG_BASE + 732, "Received %ld of %ld headers")

ResDef (MK_MSG_MARK_SEL_FOR_RETRIEVAL, XP_MSG_BASE + 733, "Mark Selected for Retrieval")
ResDef (MK_MSG_DOWNLOAD_COUNT,	   XP_MSG_BASE + 734, "Received %ld of %ld messages")

ResDef(XP_MAIL_SEARCHING,	XP_MSG_BASE + 735,
"Mail: Searching mail folders..." )

ResDef(MK_MSG_IGNORE_THREAD, XP_MSG_BASE + 736,
"Ignore Thread")

ResDef(MK_MSG_IGNORE_THREADS, XP_MSG_BASE + 737,
"Ignore Threads")

ResDef(MK_MSG_WATCH_THREAD, XP_MSG_BASE + 738,
"Watch Thread")

ResDef(MK_MSG_WATCH_THREADS, XP_MSG_BASE + 739,
"Watch Threads")


/* #define MK_MIME_NO_SUBJECT				-268  	*/
ResDef(MK_MIME_NO_SUBJECT,				-268,  /* Subject: empty */
"No subject was specified.")


ResDef(MK_MSG_ASSEMBLING_MSG,		XP_MSG_BASE + 407,
 "Assembling message...")

ResDef(MK_MSG_ASSEMB_DONE_MSG,		XP_MSG_BASE + 408,
"Assembling message...Done")

ResDef(MK_MSG_LOAD_ATTACHMNT,		XP_MSG_BASE + 409,
 "Loading attachment...")

ResDef(MK_MSG_LOAD_ATTACHMNTS,		XP_MSG_BASE + 410,
 "Loading attachments...")

ResDef(MK_MSG_DELIVERING_MAIL,		XP_MSG_BASE + 411,
 "Delivering mail...")

ResDef(MK_MSG_DELIV_MAIL,			XP_MSG_BASE + 412,
 "Delivering mail...")

ResDef(MK_MSG_DELIV_MAIL_DONE,		XP_MSG_BASE + 413,
 "Delivering mail...Done")

ResDef(MK_MSG_DELIV_NEWS,			XP_MSG_BASE + 414,
 "Delivering news...")

ResDef(MK_MSG_DELIV_NEWS_DONE,		XP_MSG_BASE + 415,
 "Delivering news...Done")

ResDef(MK_MSG_QUEUEING,				XP_MSG_BASE + 416,
 "Queueing for later delivery...")

ResDef(MK_MSG_WRITING_TO_FCC,		XP_MSG_BASE + 417,
 "Writing to FCC file...")

ResDef(MK_MSG_QUEUED,				XP_MSG_BASE + 418,
 "Queued for later delivery.")

ResDef(MK_MSG_MSG_COMPOSITION,		XP_MSG_BASE + 419,
 "Message Composition")

/* MK_MSG_UNKNOWN_ERROR unused		XP_MSG_BASE + 420, */


ResDef(MK_MSG_KBYTES_WASTED,		XP_MSG_BASE + 421,
 "%s %ldK bytes wasted (%ld%%)")

ResDef(MK_MSG_LOADED_MESSAGES,		XP_MSG_BASE + 422,
 "%s loaded %ld messages (%ld%%)")

ResDef(MK_MSG_OPEN_FOLDER,			XP_MSG_BASE + 423,
 "Add Folder")

ResDef(MK_MSG_OPEN_FOLDER2,			XP_MSG_BASE + 424,
 "Add Folder...")

ResDef(MK_MSG_ENTER_FOLDERNAME,		XP_MSG_BASE + 425,
 "Enter the name for your new folder.")

ResDef(MK_MSG_SAVE_MESSAGE_AS,		XP_MSG_BASE + 426,
 "Save Message As")

ResDef(MK_MSG_SAVE_MESSAGES_AS,		XP_MSG_BASE + 427,
 "Save Messages As")

ResDef(MK_MSG_GET_NEW_MAIL,			XP_MSG_BASE + 428,
 "Get New Mail")

ResDef(MK_MSG_DELIV_NEW_MSGS,		XP_MSG_BASE + 429,
 "Send Messages in Outbox")

ResDef(MK_MSG_NEW_FOLDER,			XP_MSG_BASE + 430,
 "New Folder...")

ResDef(MK_MSG_COMPRESS_FOLDER,		XP_MSG_BASE + 431,
 "Compress This Folder")

ResDef(MK_MSG_COMPRESS_ALL_FOLDER,	XP_MSG_BASE + 432,
 "Compress All Folders")

ResDef(MK_MSG_OPEN_NEWS_HOST_ETC,		XP_MSG_BASE + 433,
 "Open News Host...")

ResDef(MK_MSG_EMPTY_TRASH_FOLDER,	XP_MSG_BASE + 434,
 "Empty Trash Folder")

ResDef(MK_MSG_PRINT,				XP_MSG_BASE + 435,
 "Print...")

ResDef(MK_MSG_UNDO,					XP_MSG_BASE + 436,
 "Undo")

ResDef(MK_MSG_REDO,					XP_MSG_BASE + 437,
 "Redo")

ResDef(MK_MSG_DELETE_SEL_MSGS,		XP_MSG_BASE + 438,
 "Delete Selected Messages")

ResDef(MK_MSG_DELETE_MESSAGE,		XP_MSG_BASE + 439,
 "Delete Message")

ResDef(MK_MSG_DELETE_FOLDER,		XP_MSG_BASE + 440,
 "Delete Folder")

ResDef(MK_MSG_CANCEL_MESSAGE,		XP_MSG_BASE + 441,
 "Cancel Message")

ResDef(MK_MSG_RMV_NEWS_HOST,		XP_MSG_BASE + 442,
 "Remove News Host")

ResDef(MK_MSG_SUBSCRIBE,			XP_MSG_BASE + 443,
 "Subscribe")

ResDef(MK_MSG_UNSUBSCRIBE,			XP_MSG_BASE + 444,
 "Unsubscribe")

ResDef(MK_MSG_SELECT_THREAD,		XP_MSG_BASE + 445,
 "Select Thread")

ResDef(MK_MSG_SELECT_FLAGGED_MSG,	XP_MSG_BASE + 446,
 "Select Flagged Messages")

ResDef(MK_MSG_SELECT_ALL,			XP_MSG_BASE + 447,
 "Select All Messages")

ResDef(MK_MSG_DESELECT_ALL_MSG,		XP_MSG_BASE + 448,
 "Deselect All Messages")

ResDef(MK_MSG_FLAG_MESSAGE,			XP_MSG_BASE + 449,
 "Flag Message")

ResDef(MK_MSG_UNFLAG_MESSAGE,		XP_MSG_BASE + 450,
 "Unflag Message")

ResDef(MK_MSG_AGAIN,				XP_MSG_BASE + 451,
 "Again")

ResDef(MK_MSG_THREAD_MESSAGES,		XP_MSG_BASE + 452,
 "Thread Messages")

ResDef(MK_MSG_BY_DATE,				XP_MSG_BASE + 453,
 "By Date")

ResDef(MK_MSG_BY_SENDER,			XP_MSG_BASE + 454,
 "By Sender")

ResDef(MK_MSG_BY_SUBJECT,			XP_MSG_BASE + 455,
 "By Subject")

ResDef(MK_MSG_BY_MESSAGE_NB,		XP_MSG_BASE + 456,
 "By Message Number")

ResDef(MK_MSG_UNSCRAMBLE,			XP_MSG_BASE + 457,
 "Unscramble (Rot13)")

ResDef(MK_MSG_ADD_FROM_NEW_MSG,		XP_MSG_BASE + 458,
 "Add from Newest Messages")

ResDef(MK_MSG_ADD_FROM_OLD_MSG,		XP_MSG_BASE + 459,
 "Add from Oldest Messages")

ResDef(MK_MSG_GET_MORE_MSGS,		XP_MSG_BASE + 460,
 "Get More Messages")

ResDef(MK_MSG_GET_ALL_MSGS,			XP_MSG_BASE + 461,
 "Get All Messages")

ResDef(MK_MSG_ADDRESS_BOOK,			XP_MSG_BASE + 462,
 "Address Book")

ResDef(MK_MSG_VIEW_ADDR_BK_ENTRY,	XP_MSG_BASE + 463,
 "View Address Book Entry")

ResDef(MK_MSG_ADD_TO_ADDR_BOOK,		XP_MSG_BASE + 464,
 "Add to Address Book")

ResDef(MK_MSG_NEW_NEWS_MESSAGE,		XP_MSG_BASE + 465,
 "New News Message")

ResDef(MK_MSG_POST_REPLY,			XP_MSG_BASE + 466,
 "Post Reply")

ResDef(MK_MSG_POST_MAIL_REPLY,		XP_MSG_BASE + 467,
 "Post And Mail Reply")

ResDef(MK_MSG_NEW_MAIL_MESSAGE,		XP_MSG_BASE + 468,
 "New Mail Message")

ResDef(MK_MSG_REPLY,				XP_MSG_BASE + 469,
 "Reply")

ResDef(MK_MSG_REPLY_TO_ALL,			XP_MSG_BASE + 470,
 "Reply To All")

ResDef(MK_MSG_FWD_SEL_MESSAGES,		XP_MSG_BASE + 471,
 "Forward Selected Messages")

ResDef(MK_MSG_FORWARD,				XP_MSG_BASE + 472,
 "Forward")

ResDef(MK_MSG_MARK_SEL_AS_READ,		XP_MSG_BASE + 473,
 "Mark Selected as Read")

ResDef(MK_MSG_MARK_AS_READ,			XP_MSG_BASE + 474,
 "Mark as Read")

ResDef(MK_MSG_MARK_SEL_AS_UNREAD,	XP_MSG_BASE + 475,
 "Mark Selected as Unread")

ResDef(MK_MSG_MARK_AS_UNREAD,		XP_MSG_BASE + 476,
 "Mark as Unread")

ResDef(MK_MSG_UNFLAG_ALL_MSGS,		XP_MSG_BASE + 477,
 "Unflag All Messages")

ResDef(MK_MSG_COPY_SEL_MSGS,		XP_MSG_BASE + 478,
 "Copy Selected Messages")

ResDef(MK_MSG_COPY_ONE,				XP_MSG_BASE + 479,
 "Copy")

ResDef(MK_MSG_MOVE_SEL_MSG,			XP_MSG_BASE + 480,
 "Move Selected Messages")

ResDef(MK_MSG_MOVE_ONE,				XP_MSG_BASE + 481,
 "Move")

ResDef(MK_MSG_SAVE_SEL_MSGS,		XP_MSG_BASE + 482,
 "Save Selected Messages As...")

ResDef(MK_MSG_SAVE_AS,				XP_MSG_BASE + 483,
 "Save As...")

ResDef(MK_MSG_MOVE_SEL_MSG_TO,		XP_MSG_BASE + 484,
 "Move Selected Messages To...")

ResDef(MK_MSG_MOVE_MSG_TO,			XP_MSG_BASE + 485,
 "Move This Message To...")

ResDef(MK_MSG_FIRST_MSG,			XP_MSG_BASE + 486,
 "First Message")

ResDef(MK_MSG_NEXT_MSG,				XP_MSG_BASE + 487,
 "Next Message")

ResDef(MK_MSG_PREV_MSG,				XP_MSG_BASE + 488,
 "Previous Message")

ResDef(MK_MSG_LAST_MSG,				XP_MSG_BASE + 489,
 "Last Message")

ResDef(MK_MSG_FIRST_UNREAD,			XP_MSG_BASE + 490,
 "First Unread")

ResDef(MK_MSG_NEXT_UNREAD,			XP_MSG_BASE + 491,
 "Next Unread")

ResDef(MK_MSG_PREV_UNREAD,			XP_MSG_BASE + 492,
 "Previous Unread")

ResDef(MK_MSG_LAST_UNREAD,			XP_MSG_BASE + 493,
 "Last Unread")

ResDef(MK_MSG_FIRST_FLAGGED,		XP_MSG_BASE + 494,
 "First Flagged")

ResDef(MK_MSG_NEXT_FLAGGED,			XP_MSG_BASE + 495,
 "Next Flagged")

ResDef(MK_MSG_PREV_FLAGGED,			XP_MSG_BASE + 496,
 "Previous Flagged")

ResDef(MK_MSG_LAST_FLAGGED,			XP_MSG_BASE + 497,
 "Last Flagged")

ResDef(MK_MSG_MARK_SEL_READ,		XP_MSG_BASE + 498,
 "Mark Selected Threads Read")

ResDef(MK_MSG_MARK_THREAD_READ,		XP_MSG_BASE + 499,
 "Mark Thread Read")

ResDef(MK_MSG_MARK_NEWSGRP_READ,	XP_MSG_BASE + 500,
 "Mark Newsgroup Read")

ResDef(MK_MSG_SHOW_SUB_NEWSGRP,		XP_MSG_BASE + 501,
 "Show Subscribed Newsgroups")

ResDef(MK_MSG_SHOW_ACT_NEWSGRP,		XP_MSG_BASE + 502,
 "Show Active Newsgroups")

ResDef(MK_MSG_SHOW_ALL_NEWSGRP,		XP_MSG_BASE + 503,
 "Show All Newsgroups")

ResDef(MK_MSG_CHECK_FOR_NEW_GRP,	XP_MSG_BASE + 504,
 "Check for New Groups")

ResDef(MK_MSG_SHOW_ALL_MSGS,		XP_MSG_BASE + 505,
 "Show All Messages")

ResDef(MK_MSG_SHOW_UNREAD_ONLY,		XP_MSG_BASE + 506,
 "Show Only Unread Messages")

/* This #$%^S!!
   MK_MSG_SHOW_MICRO_HEADERS and MK_MSG_SHOW_SOME_HEADERS are way at the end.
 */

ResDef(MK_MSG_SHOW_ALL_HEADERS,		XP_MSG_BASE + 507,
 "All")

ResDef(MK_MSG_QUOTE_MESSAGE,		XP_MSG_BASE + 508,
 "Include Original Text")

ResDef(MK_MSG_FROM,					XP_MSG_BASE + 509,
 "From")

ResDef(MK_MSG_REPLY_TO,				XP_MSG_BASE + 510,
 "Reply To")

ResDef(MK_MSG_MAIL_TO,				XP_MSG_BASE + 511,
 "Mail To")

ResDef(MK_MSG_MAIL_CC,				XP_MSG_BASE + 512,
 "Mail CC")

ResDef(MK_MSG_MAIL_BCC,				XP_MSG_BASE + 513,
 "Mail BCC")

ResDef(MK_MSG_FILE_CC,				XP_MSG_BASE + 514,
 "File CC")

ResDef(MK_MSG_POST_TO,				XP_MSG_BASE + 515,
 "Newsgroups")

ResDef(MK_MSG_FOLLOWUPS_TO,			XP_MSG_BASE + 516,
 "Followups To")

ResDef(MK_MSG_SUBJECT,				XP_MSG_BASE + 517,
 "Subject")

ResDef(MK_MSG_ATTACHMENT,			XP_MSG_BASE + 518,
 "Attachment")

ResDef(MK_MSG_SEND_FORMATTED_TEXT,	XP_MSG_BASE + 519,
 "Send Formatted Text")

ResDef(MK_MSG_Q4_LATER_DELIVERY,	XP_MSG_BASE + 520,
 "Queue For Later Delivery")

ResDef(MK_MSG_ATTACH_AS_TEXT,		XP_MSG_BASE + 521,
 "Attach As Text")

ResDef(MK_MSG_FLAG_MESSAGES,		XP_MSG_BASE + 522,
 "Flag Messages")

ResDef(MK_MSG_UNFLAG_MESSAGES,		XP_MSG_BASE + 523,
 "Unflag Messages")

ResDef(MK_MSG_SORT_BACKWARD,		XP_MSG_BASE + 524,
 "Ascending")

ResDef(MK_MSG_PARTIAL_MSG_FMT_1,	XP_MSG_BASE + 525,
 "<P><CENTER>\n<TABLE BORDER CELLSPACING=5 CELLPADDING=10 WIDTH=\04280%%\042>\n\
<TR><TD ALIGN=CENTER><FONT SIZE=\042+1\042>Truncated!</FONT><HR>\n")

ResDef(MK_MSG_PARTIAL_MSG_FMT_2,	XP_MSG_BASE + 526,
 "<B>This message exceeded the Maximum Message Size set in Preferences,\n\
so we have only downloaded the first few lines from the mail server.<P>Click <A HREF=\042")

ResDef(MK_MSG_PARTIAL_MSG_FMT_3,	XP_MSG_BASE + 527,
 "\042>here</A> to download the rest of the message.</B></TD></TR></TABLE></CENTER>\n")

ResDef(MK_MSG_NO_HEADERS,			XP_MSG_BASE + 528,
 "(no headers)")

ResDef(MK_MSG_UNSPECIFIED,			XP_MSG_BASE + 529,
 "(unspecified)")

ResDef(MK_MSG_MIME_MAC_FILE,		XP_MSG_BASE + 530,
 "Macintosh File")

ResDef(MK_MSG_DIR_DOESNT_EXIST,		XP_MSG_BASE + 531,
 "The directory %s does not exist.  Mail will not\nwork without it.\n\nCreate it now?")

ResDef(MK_MSG_SAVE_DECODED_AS,		XP_MSG_BASE + 532,
 "Save decoded file as:")

ResDef(MK_MSG_FILE_HAS_CHANGED,		XP_MSG_BASE + 533,
 "The file %s has been changed by some other program!\nOverwrite it?")

ResDef(MK_MSG_OPEN_NEWS_HOST,		XP_MSG_BASE + 534,
 "Open News Host")

ResDef(MK_MSG_ANNOUNCE_NEWSGRP,		XP_MSG_BASE + 535,
 "news.announce.newusers")

ResDef(MK_MSG_QUESTIONS_NEWSGRP,	XP_MSG_BASE + 536,
 "news.newusers.questions")

ResDef(MK_MSG_ANSWERS_NEWSGRP,		XP_MSG_BASE + 537,
 "news.answers")

ResDef(MK_MSG_COMPRESS_FOLDER_ETC,		XP_MSG_BASE + 538,
 "Mail: Compressing folder %s...")

ResDef(MK_MSG_COMPRESS_FOLDER_DONE,	XP_MSG_BASE + 539,
 "Mail: Compressing folder %s...Done")

ResDef(MK_MSG_CANT_OPEN,			XP_MSG_BASE + 540,
 "Can't open %s.")

ResDef(MK_MSG_SAVE_ATTACH_AS,		XP_MSG_BASE + 541,
 "Save attachment in BinHex file as:")

ResDef(MK_MSG_ADD_NEWS_GROUP,		XP_MSG_BASE + 574,	"Add Newsgroup...")
ResDef(MK_MSG_FIND_AGAIN,			XP_MSG_BASE + 575, 	"Find Again")
ResDef(MK_MSG_SEND,					XP_MSG_BASE + 576, 	"Send")
ResDef(MK_MSG_SEND_LATER,			XP_MSG_BASE + 577, 	"Send Later")
ResDef(MK_MSG_ATTACH_ETC,			XP_MSG_BASE + 578, 	"Attach...")
ResDef(MK_MSG_ATTACHMENTSINLINE,	XP_MSG_BASE + 579, 	"Attachments Inline")
ResDef(MK_MSG_ATTACHMENTSASLINKS,	XP_MSG_BASE + 580, 	"Attachments as Links")
ResDef(MK_MSG_FORWARD_QUOTED,   	XP_MSG_BASE + 581,  "Forward Quoted")
ResDef(MK_MSG_REMOVE_HOST_CONFIRM,      XP_MSG_BASE + 582,
       "Are you sure you want to remove the news host %s\n\
and all of the newsgroups in it?")

ResDef(MK_MSG_ALL_FIELDS, 		XP_MSG_BASE + 583, "All Fields")

ResDef(MK_MSG_BOGUS_QUEUE_MSG_1,	XP_MSG_BASE + 584,
       "The `Outbox' folder contains a message which is not\n\
scheduled for delivery!")

ResDef(MK_MSG_BOGUS_QUEUE_MSG_N,	XP_MSG_BASE + 585,
       "The `Outbox' folder contains %d messages which are not\n\
scheduled for delivery!")

ResDef(MK_MSG_BOGUS_QUEUE_REASON,	XP_MSG_BASE + 586,
       "\n\nThis probably means that some program other than\n\
Netscape has added messages to this folder.\n")

ResDef(MK_MSG_WHY_QUEUE_SPECIAL,	XP_MSG_BASE + 587,
       "The `Outbox' folder is special; it is only for holding\n\
messages which have been deferred for later delivery.")

ResDef(MK_MSG_NOT_AS_SENT_FOLDER,	XP_MSG_BASE + 588,
       "\nTherefore, you can't use it as your `Sent' folder.\n\
\n\
Please verify that your outgoing messages destination is correct\n\
in your Mail & News preferences.")
ResDef(MK_MSG_QUEUED_DELIVERY_FAILED,	XP_MSG_BASE + 589,
       "An error occurred delivering deferred mail.\n\n\
%s\n\
Continue delivery of any remaining deferred messages ?")

ResDef(XP_PASSWORD_FOR_POP3_USER,	XP_MSG_BASE + 590,
       "Password for POP3 user %.100s@%.100s:")


/* Whoever decided all these numbers should be consecutive should be shot. */
ResDef(MK_MSG_SHOW_MICRO_HEADERS,		XP_MSG_BASE + 630,
 "Brief")
ResDef(MK_MSG_SHOW_SOME_HEADERS,		XP_MSG_BASE + 631,
 "Normal")

ResDef(MK_MSG_DELETE_FOLDER_MESSAGES,	XP_MSG_BASE + 632,
 "There are still messages in folder '%s'.\n\
Are you sure you still want to delete this folder?")


END_STR (MSG_strings)

#endif




