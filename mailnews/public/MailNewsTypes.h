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


/* MessageKey is a unique ID for a particular message in a folder.  If you want
   a handle to a message that will remain valid even after resorting the folder
   or otherwise changing their indices, you want one of these rather than a
   MSG_ViewIndex.
 */
typedef PRUInt32 MessageKey;
const MessageKey MSG_MESSAGEKEYNONE = 0xffffffff;

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

/* Flags about a folder or a newsgroup.  Used in the MSG_FolderLine struct;
   also used internally in libmsg (the `flags' slot in MSG_Folder).  Note that
   these don't have anything to do with the above MSG_FLAG flags; they belong
   to different objects entirely.  */

    /* These flags say what kind of folder this is:
       mail or news, directory or leaf.
     */
#define MSG_FOLDER_FLAG_NEWSGROUP   0x0001  /* The type of this folder. */
#define MSG_FOLDER_FLAG_NEWS_HOST   0x0002  /* Exactly one of these three */
#define MSG_FOLDER_FLAG_MAIL        0x0004  /* flags will be set. */

#define MSG_FOLDER_FLAG_DIRECTORY   0x0008  /* Whether this is a directory:
                                               NEWS_HOSTs are always
                                               directories; NEWS_GROUPs can be
                                               directories if we are in ``show
                                               all groups'' mode; MAIL folders
                                               will have this bit if they are
                                               really directories, not files.
                                               (Note that directories may have
                                               zero children.) */

#define MSG_FOLDER_FLAG_ELIDED      0x0010  /* Whether the children of this
                                               folder are currently hidden in
                                               the listing.  This will only
                                               be present if the DIRECTORY
                                               bit is on. */

    /* These flags only occur in folders which have
       the MSG_FOLDER_FLAG_NEWSGROUP bit set, and do
       not have the MSG_FOLDER_FLAG_DIRECTORY or
       MSG_FOLDER_FLAG_ELIDED bits set.
     */

#define MSG_FOLDER_FLAG_MODERATED   0x0020  /* Whether this folder represents
                                               a moderated newsgroup. */
#define MSG_FOLDER_FLAG_SUBSCRIBED  0x0040  /* Whether this folder represents
                                               a subscribed newsgroup. */
#define MSG_FOLDER_FLAG_NEW_GROUP   0x0080  /* A newsgroup which has just
                                               been added by the `Check
                                               New Groups' command. */


    /* These flags only occur in folders which have
       the MSG_FOLDER_FLAG_MAIL bit set, and do
       not have the MSG_FOLDER_FLAG_DIRECTORY or
       MSG_FOLDER_FLAG_ELIDED bits set.

	   The numeric order of these flags is important;
	   folders with these flags on get displayed first,
	   in reverse numeric order, before folders that have
	   none of these flags on.  (Note that if a folder is,
	   say, *both* inbox and sentmail, then its numeric value
	   will be even bigger, and so will bubble up to where the
	   inbox generally is.  What a hack!)
     */

#define MSG_FOLDER_FLAG_TRASH       0x0100  /* Whether this is the trash
                                               folder. */
#define MSG_FOLDER_FLAG_SENTMAIL	0x0200	/* Whether this is a folder that
											   sent mail gets delivered to.
											   This particular magic flag is
											   used only during sorting of
											   folders; we generally don't care
											   otherwise. */
#define MSG_FOLDER_FLAG_DRAFTS      0x0400	/* Whether this is the folder in
                                               which unfinised, unsent messages
                                               are saved for later editing. */
#define MSG_FOLDER_FLAG_QUEUE       0x0800  /* Whether this is the folder in
                                               which messages are queued for
                                               later delivery. */
#define MSG_FOLDER_FLAG_INBOX       0x1000  /* Whether this is the primary
                                               inbox folder. */
#define MSG_FOLDER_FLAG_IMAPBOX		0x2000	/* Whether this folder on online
											   IMAP */

#define MSG_FOLDER_FLAG_CAT_CONTAINER 0x4000 /* This group contains categories */

#define MSG_FOLDER_FLAG_PROFILE_GROUP 0x8000 /* This is a virtual newsgroup */

#define MSG_FOLDER_FLAG_CATEGORY	0x10000  /* this is a category */

#define MSG_FOLDER_FLAG_GOT_NEW		0x20000		/* folder got new msgs */

#define MSG_FOLDER_FLAG_IMAP_SERVER	0x40000		/* folder is an IMAP server */

#define MSG_FOLDER_FLAG_IMAP_PERSONAL	0x80000		/* folder is an IMAP personal folder */

#define MSG_FOLDER_FLAG_IMAP_PUBLIC		0x100000		/* folder is an IMAP public folder */

#define MSG_FOLDER_FLAG_IMAP_OTHER_USER	0x200000		/* folder is another user's IMAP folder */
														/* Think of it like a folder that someone would share. */
#define MSG_FOLDER_FLAG_TEMPLATES		0x400000	/* Whether this is the template folder */

#define MSG_FOLDER_FLAG_PERSONAL_SHARED	0x800000	/* This folder is one of your personal folders that
								`					   is shared with other users */

#define MSG_FOLDER_FLAG_IMAP_NOSELECT	0x1000000	/* This folder is an IMAP \Noselect folder */


/* Flags in the subscribe pane (used inside of MSG_GroupNameLine).  Where
   the flags overlap with the MSG_FOLDER_FLAG_* flags, it has the same value,
   to reduce the chance of someone using the wrong constant. */

#define MSG_GROUPNAME_FLAG_ELIDED		0x0010  /* Whether the children of this
												   group are currently hidden
												   in the listing.  This will
												   only be present if it has
												   any children. */

#define MSG_GROUPNAME_FLAG_MODERATED	0x0020  /* Whether this folder
												   represents a moderated
												   newsgroup. */
#define MSG_GROUPNAME_FLAG_SUBSCRIBED	0x0040  /* Whether this folder
												   represents a subscribed
												   newsgroup. */
#define MSG_GROUPNAME_FLAG_NEW_GROUP	0x0080  /* A newsgroup which has just
												   been added by the `Check
												   New Groups' command. */
#define MSG_GROUPNAME_FLAG_HASCHILDREN	0x40000 /* Whether there are children
												  of this group.  Whether those
												  chilren are visible in this
												  list is determined by the
												  above "ELIDED" flag. 
												  Setting this to the same value
												  as a MSG_FOLDER_FLAG_* IMAP server,
												  since an IMAP _server_ will never
												  appear in the subscribe pane.  */
#define MSG_GROUPNAME_FLAG_IMAP_PERSONAL	0x80000		/* folder is an IMAP personal folder */

#define MSG_GROUPNAME_FLAG_IMAP_PUBLIC		0x100000		/* folder is an IMAP public folder */

#define MSG_GROUPNAME_FLAG_IMAP_OTHER_USER	0x200000		/* folder is another user's IMAP folder */

#define MSG_GROUPNAME_FLAG_IMAP_NOSELECT	0x400000		/* A \NoSelect IMAP folder */

#define MSG_GROUPNAME_FLAG_PERSONAL_SHARED	0x800000	/* whether or not this folder is one of your personal folders that
								`					       is shared with other users */


