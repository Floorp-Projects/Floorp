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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef MailNewsTypes_h__
#define MailNewsTypes_h__

#include "msgCore.h"
#include "prtypes.h"

/* MessageKey is a unique ID for a particular message in a folder.  If you want
   a handle to a message that will remain valid even after resorting the folder
   or otherwise changing their indices, you want one of these rather than a
   MSG_ViewIndex. MessageKeys don't survive local mail folder compression, however.
 */
typedef PRUint32 MessageKey;
const MessageKey MSG_MESSAGEKEYNONE = 0xffffffff;

/* XP_Index
 *
 * A generic index type from which other index types are derived.  All XP_Index
 * derived types are zero based.
 *
 * The following index types are currently supported:
 *  - MSG_ViewIndex - an index into the list of messages or folders or groups,
 *    where zero is the first one to show, one is the second, etc...
 *  - AB_SelectionIndex
 *  - AB_NameCompletionIndex
 */

/* I'm not sure it's correct to carry these over into Mozilla, but we can always take them out */
typedef enum _XP_IndexType
{
	kViewIndex = 0,
	kSelectionIndex,
	kNameCompletionIndex
} XP_IndexType;

typedef uint32 XP_Index;
typedef XP_Index MSG_ViewIndex;
typedef MSG_ViewIndex MsgViewIndex; 

/* XP_INDEXNONE and MSG_VIEWINDEXNONE are used to indicate an invalid or
 * non-existent index.
 */
const XP_Index      XP_INDEXNONE      = 0xFFFFFFFF;
const MSG_ViewIndex MSG_VIEWINDEXNONE = 0xFFFFFFFF;

/* Message priorities as determined by X-Priority hdr, or Priority header? */
typedef enum
{
  MSG_PriorityNotSet = 0,
  MSG_NoPriority = 1,
  MSG_LowestPriority,
  MSG_LowPriority,
  MSG_NormalPriority,
  MSG_HighPriority,
  MSG_HighestPriority
} MSG_PRIORITY;


/* Flags about a single message.  These values are used in the MSG_MessageLine
   struct and in a folder's mozilla-status line. The summary file database
   should use the same set of flags..
*/

#define MSG_FLAG_READ     0x0001    /* has been read */
#define MSG_FLAG_REPLIED  0x0002    /* a reply has been successfully sent */
#define MSG_FLAG_MARKED   0x0004    /* the user-provided mark */
#define MSG_FLAG_EXPUNGED 0x0008    /* already gone (when folder not
                                       compacted.)  Since actually
                                       removing a message from a
                                       folder is a semi-expensive
                                       operation, we tend to delay it;
                                       messages with this bit set will
                                       be removed the next time folder
                                       compaction is done.  Once this
                                       bit is set, it never gets
                                       un-set.  */
#define MSG_FLAG_HAS_RE   0x0010    /* whether subject has "Re:" on
                                       the front.  The folder summary
                                       uniquifies all of the strings
                                       in it, and to help this, any
                                       string which begins with "Re:"
                                       has that stripped first.  This
                                       bit is then set, so that when
                                       presenting the message, we know
                                       to put it back (since the "Re:"
                                       is not itself stored in the
                                       file.)  */
#define MSG_FLAG_ELIDED   0x0020    /* Whether the children of this
                                       sub-thread are folded in the
                                       display.  */
#define MSG_FLAG_EXPIRED  0x0040    /* If this flag is set, then this
                                       is not a "real" message, but is
                                       a dummy container representing
                                       an expired parent in a thread.  */
#define MSG_FLAG_OFFLINE  0x0080	/* db has offline news or imap article
											 */
#define MSG_FLAG_WATCHED  0x0100    /* If set, then this thread is watched (in
									   3.0, this was MSG_FLAG_UPDATING).*/
#define MSG_FLAG_SENDER_AUTHED    0x0200    /* If set, then this message's sender
                                       has been authenticated when sending this msg. */
#define MSG_FLAG_PARTIAL  0x0400    /* If set, then this message's body is
                                       only the first ten lines or so of the
                                       message, and we need to add a link to
                                       let the user download the rest of it
                                       from the POP server. */
#define MSG_FLAG_QUEUED   0x0800    /* If set, this message is queued for
                                       delivery.  This only ever gets set on
                                       messages in the queue folder, but is
                                       used to protect against the case of
                                       other messages having made their way
                                       in there somehow -- if some other
                                       program put a message in the queue, we
                                       don't want to later deliver it! */
#define MSG_FLAG_FORWARDED  0x1000  /* this message has been forwarded */
#define MSG_FLAG_PRIORITIES 0xE000	/* These are used to remember the message
									   priority in the mozilla status flags
									   so we can regenerate a priority after a 
									   rule (or user) has changed it. They are
									   not returned in MSG_MessageLine.flags, 
									   just in mozilla-status, so if you need
									   more non-persistent flags, you could 
									   share these bits. But it would be wrong.
									.  */

#define MSG_FLAG_NEW		0x10000	/* This msg is new since the last time
									   the folder was closed.
									   */
#define MSG_FLAG_IGNORED	0x40000 /* the thread is ignored */


#define MSG_FLAG_IMAP_DELETED	0x200000 /* message is marked deleted on the server */

#define MSG_FLAG_MDN_REPORT_NEEDED 0x400000 /* This msg required to send an MDN 
											 * to the sender of the message
											 */
#define MSG_FLAG_MDN_REPORT_SENT   0x800000 /* An MDN report message has been
											 * sent for this message. No more
											 * MDN report should be sent to the
											 * sender
											 */
#define MSG_FLAG_TEMPLATE       0x1000000	/* this message is a template */
#define MSG_FLAG_ATTACHMENT		0x10000000	/* this message has files attached to it */

#endif
