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

/* msgcom.h --- prototypes for the mail/news reader module.
   Created: Jamie Zawinski <jwz@netscape.com>, 10-May-95.
 */

#ifndef _MSGCOM_H_
#define _MSGCOM_H_

#include "libmime.h"

#define SUBSCRIBE_USE_OLD_API

/* ===========================================================================
   This file defines all of the types and prototypes for communication
   between the msg library, which implements the mail and news applications,
   and the various front ends.

   Functions beginning with MSG_ are defined in the library, and are invoked
   by the front ends in response to user activity.

   Functions beginning with FE_ are defined by the front end, and are invoked
   by the message library to get things to happen on the screen.

   The main parts of this file are listed below:

   						COMMAND NUMBERS
   						CONSTANTS AND ENUMS
   						FLAGS AND MASKS
   						TYPES AND STRUCTS

   						INIT/CREATE
   						RANDOM CORE FUNCTIONS (to sort)
   						PREFERENCES
   						LIST CALLBACKS
   						HOSTS
   						SUBSCRIBE WINDOW
   						OFFLINE NEWS
   						OFFLINE IMAP
   						QUERIES
   						BIFF
   						OTHER INTERFACES
   						SECURE MAIL
   						COMPOSE WINDOW

   ===========================================================================
 */


#include "xp_mcom.h"
#include "xp_core.h"
#include "ntypes.h"
#include "msgtypes.h"





/* ===========================================================================
   						COMMAND NUMBERS
   ===========================================================================
 */

/* This enumerates all of the mail/news-specific commands (those which are
   not shared with the web browsers.  The front ends should invoke each of
   these menu items through the MSG_Command() function like so:

      MSG_Command (context, MSG_PostReply);

   This interface is used for selections of normal menu items, toolbar
   buttons, and keyboard equivalents.  Clicks in the scrolling list windows,
   drag-and-drop, and the "folders" menus are handled differently.

   Different items are meaningful in different sets of panes.  The comments
   indicate which:

		f:  Usable in folder panes (either mail or news)
		fn: Usable only in news folderpanes
		fm: Usable only in mail folderpanes
		t: Usable in thread panes
		tn: Usable only in news threadpanes
		tm: Usable only in mail threadpanes
		m: Usable in message panes
		mn: Usable only in news messagepanes
		mm: Usable only in mail messagepanes
		c: Usable in composition panes
		sub: Usable in the subscribe pane

    In general, an item which works on folders can either be called on
    the folder pane (in which case it effects the specified folders),
    or on a threadpane (in which case it effects the folder being
    displayed in the threadpane).  Similarly, items which work on
    messages can be either called on a threadpane or on a messagepane.

	Also, in general, commands useable in news folder pane work in the
	category pane as well.
 */

typedef enum
{
  /* FILE MENU
	 =========
   */
  MSG_GetNewMail,				/* fm: Gets new mail messages, and appends them
								   to the appropriate folders.  This is an
								   asynchronous operation; it will eventually
								   cause FE_ListChangeStarting and
								   FE_ListChangeFinished to be called on any
								   threadpanes displaying a folder which is
								   modified. */
  MSG_GetNextChunkMessages,		/* f,t Get the next N news messages, based on chunk 
								   size. */
  MSG_UpdateMessageCount,		/* f,t,m News only - Update the message counts */

  MSG_DeliverQueuedMessages,	/* f,t,m: Deliver messages waiting to be 
								   delivered in the queued folder. */

  MSG_OpenFolder,				/* fm: Prompt the user for the full pathname of
								   a mail file, and creates an entry for it
								   (i.e., calls MSG_CreateFolder). */

  MSG_NewFolder,				/* f,t,m: Prompt the user for a new folder or
								   subfolder name, and creates it (i.e., calls
								   MSG_CreateFolder) */

  MSG_CompressFolder,			/* fm: Causes the given folders to be
								   compressed. */

  MSG_CompressAllFolders,		/* fm: Causes all the folders to be
								   compressed. */

  MSG_DoRenameFolder,				/* fm: Prompt the user for a new folder or
								   subfolder name, and renames the selected
								   folder to have that name (i.e., calls
								   MSG_RenameFolder()). */

  MSG_AddNewsGroup,				/* fn: Prompts the user for a newsgroup name,
								 and adds it to the list of newsgroups (i.e.,
								 subscribes the user). */

  MSG_EmptyTrash,				/* fm: Causes the trash folder to be emptied,
								   and all the folders to be compressed. */

  MSG_Unsubscribe,				/* f, Unsubscribe to the selected newsgroup(s)
									Delete currently does this as well. */

  MSG_ManageMailAccount,		/* f: Prompts the user for mail account password
								   if needed and brings up the GURL web page */

  MSG_Print,				/* not a command - used for selectability only */

  MSG_NewNewsgroup,             /* ft: Prompts the user for a new newsgroup name */
                                /*     and brings up the NGURL web page */

  MSG_ModerateNewsgroup,		/* ft: brings up the MODURL web page for the */
								/*     selected group */

  MSG_NewCategory,              /* ft: Prompts the user for a new category name */
                                /*     and brings up the NGURL web page */

  /* VIEW/SORT MENUS
	 ===============
   */
  MSG_ReSort,					/* t: Re-apply the current sort. */
  MSG_SortBackward,				/* t: Reverse the order of the sort. */
  MSG_SortByDate,				/* t: Sort in chronological order. */
  MSG_SortBySubject,			/* t: Sort alphabetized by subject. */
  MSG_SortBySender,				/* t: Sort alphabetized by sender. */
  MSG_SortByMessageNumber,		/* t: Sort in order stored in mail folder, or
								   in numerical article number if news. */
  MSG_SortByThread,				/* t: Sort in threads. */
  MSG_SortByPriority,			/* t: Sort by priority - highest first. */
  MSG_SortByStatus,				/* t: Sort by message status - new first. */
  MSG_SortBySize,				/* t: Sort by size */
  MSG_SortByFlagged,			/* t: Sort by flag state */
  MSG_SortByUnread,				/* t: Sort by unread state */

  MSG_ViewAllThreads,			/* t: (except for killed threads) -default */
  MSG_ViewKilledThreads,			/* t: Show all incl. killed threads */
  MSG_ViewThreadsWithNew,		/* t: Show only threads with new messages */
  MSG_ViewWatchedThreadsWithNew,/* t: Show only watched thrds with new msgs */
  MSG_ViewNewOnly,				/* t: Show only new messages */

  MSG_Rot13Message,				/* m: Apply fancy rot13 encryption. */

  MSG_AttachmentsInline,		/* m: Display attachments in line. */
  MSG_AttachmentsAsLinks,		/* m: Display attachments as links */

  MSG_WrapLongLines,

  /* EDIT MENU
	 =========
   */
  MSG_Undo,						/* ftm: Undoes the last operation. */

  MSG_Redo,						/* ftm: Redoes the last undone operation. */

  MSG_DeleteMessage,			/* tm, mm: Causes the given messages to be
								   deleted. */

  MSG_DeleteFolder,				/* fm, tm: Causes the given folders to be
								   deleted. */

  MSG_CancelMessage,			/* tn, mn: Causes the given messages to be
								   cancelled, if possible. */

  MSG_DeleteMessageNoTrash,		/* tm, mm: Causes the given messages to be
								   deleted w/o getting copied to trash. */

  /* MESSAGE MENU
	 ============
   */
  MSG_EditAddressBook,			/* f,t,m,c: Bring up the address book. */
  MSG_EditAddress,				/* m: Bring up the address book entry for the
								   sender of this message. */
  MSG_AddSender,				/* t,m: Add the sender of this message. 
									to the address book */
  MSG_AddAll,					/* t,m: Add all recipients of this message. 
									to the address book */
  MSG_PostNew,					/* fn,tn: Post a new message to this
								   newsgroup. */
  MSG_PostReply,				/* tn,mn: Post a followup message to this
								   article. */
  MSG_PostAndMailReply,			/* tn,mn: Post a followup message to this
								   article, mailing a copy to the author. */
  MSG_MailNew,					/* f,t,m,c: Create a new mail composition. */
  MSG_ReplyToSender,			/* t,m: E-mail a reply to the sender of this
								   message. */
  MSG_ReplyToAll,				/* t,m: E-mail (& possibly post) a reply to
								   everyone who saw this message. */
  MSG_ForwardMessage,			/* t,m: Forward these messages to someone. */
  MSG_ForwardMessageQuoted,		/* t,m: Forward this message, quoting it
								   inline. */
  MSG_MarkMessagesRead,			/* t,m: Mark the message as read. */
  MSG_MarkMessagesUnread,		/* t,m: Mark the message as unread. */
  MSG_ToggleMessageRead,		/* t,m: Toggle whether the message has been
								   read. */
  MSG_MarkMessages,				/* tn: flag the passed messages,either for 
								   retrieval or just to mark them */
  MSG_UnmarkMessages,			/* tn: unflag passed messages */								   
  MSG_ToggleThreadKilled,		/* t: toggle the killed status of the message's
									thread */
  MSG_ToggleThreadWatched,		/* t: toggle the watched status of the message's
									thread */
  MSG_SaveMessagesAs,			/* t,m: Prompt the user for a filename, and
								   save the given messages into it.  */
  MSG_SaveMessagesAsAndDelete,	/* t,m: Prompt the user for a filename, and
								   save the given messages into it, and then
								   delete the messages.  */
  MSG_RetrieveMarkedMessages,	/* fn, tn Retrieve the messages marked for 
									retrieval */
  MSG_RetrieveSelectedMessages,	/* fn, tn Retrieve the selected messages for
									offline use. */
  MSG_OpenMessageAsDraft,       /* t,m: Open the selected message as a draft
								   message. Ready to send or further editing. */

  /* GO MENU
	 =======
   */
  MSG_MarkThreadRead,			/* t,m: Mark all messages in the same thread
								   as these as read.*/
  MSG_MarkAllRead,				/* t: Mark all messages in this folder as
								   read. */


  /* OPTIONS MENU
	 ============
   */
  MSG_ShowAllMessages,			/* t: Change the view to show all messages. */
  MSG_ShowOnlyUnreadMessages,	/* t: Change the view to show only unread
								   messages. */
  MSG_ShowMicroHeaders,			/* m: Change to show very compact headers. */
  MSG_ShowSomeHeaders,			/* m: Change to show interesting headers. */
  MSG_ShowAllHeaders,			/* m: Change to show all headers. */


  /* COMPOSITION FILE MENU
	 =====================
   */
  MSG_SendMessage,				/* c: Send the composition. */
  MSG_SendMessageLater,			/* c: Queue the composition to be sent
								   later. */
  MSG_Attach,					/* c: Bring up the attachment dialogs. */
  MSG_SaveDraft,				/* c: Save draft */
  MSG_SaveDraftThenClose,	    /* c: Save draft and then close the compose pane */
  MSG_SaveTemplate,				/* c: Save as template */

  /* COMPOSITION VIEW MENU
	 =====================
   */
  MSG_ShowFrom,					/* c: Toggle showing the From header. */
  MSG_ShowReplyTo,				/* c: Toggle showing the ReplyTo header. */
  MSG_ShowTo,					/* c: Toggle showing the To header. */
  MSG_ShowCC,					/* c: Toggle showing the Const Char header. */
  MSG_ShowBCC,					/* c: Toggle showing the BCC header. */
  MSG_ShowFCC,					/* c: Toggle showing the FCC header. */
  MSG_ShowPostTo,				/* c: Toggle showing the PostTo header. */
  MSG_ShowFollowupTo,			/* c: Toggle showing the FollowupTo header. */
  MSG_ShowSubject,				/* c: Toggle showing the Subject header. */
  MSG_ShowAttachments,			/* c: Toggle showing the Attachments header. */

  /* COMPOSITION OPTIONS MENU
	 ========================
   */
  MSG_SendFormattedText,		/* c: Toggle HTML mode. */
  MSG_AttachAsText,				/* c: ???   ###tw */

  MSG_SendEncrypted,
  MSG_SendSigned,


  /* SUBSCRIBE PANE
     ==============
   */
  MSG_ToggleSubscribed,			/* sub: Changes the subscribed state of the
								   given newsgroups.  */
  MSG_SetSubscribed,			/* sub: Sets the subscribed state of given
								   newsgroups / IMAP folders to Subscribed */
  MSG_ClearSubscribed,			/* sub: Sets the subscribed state of given
								   newsgroups / IMAP folders to Unsubscribed */
  MSG_FetchGroupList,			/* sub: Causes us to go to the newsserver and
								   re-fetch the entire list of newsgroups. */
  MSG_ExpandAll,				/* sub: Expands everything under the given
								   lines. */
  MSG_CollapseAll,				/* sub: Collapses everything under the given
								   lines. */
  MSG_ClearNew,					/* sub: Clears the list of new newsgroups. */
  MSG_CheckForNew,				/* sub: Checks the server for new
								   newsgroups. */
  /* CATEGORY PANE
     =============
  */
  MSG_MarkCategoryRead,			/* cat: Mark the passed categories read */
  MSG_MarkCategoryUnRead,		/* cat: Mark the passed categories unread */
  MSG_KillCategory				/* cat: kill/ignore/unsubscribe the category */

} MSG_CommandType;




/* ===========================================================================
   						CONSTANTS AND ENUMS
   ===========================================================================
 */

/* Several things want to know this (including libnet/mknews.c) */
#define NEWS_PORT 119
#define SECURE_NEWS_PORT 563   


#define MSG_MAXSUBJECTLENGTH	160
#define MSG_MAXAUTHORLENGTH		60
#define MSG_MAXGROUPNAMELENGTH	128

typedef enum
{
	MSG_Pop3 = 0,
	MSG_Imap4 = 1,
	MSG_MoveMail = 2,
	MSG_Inbox = 3
} MSG_SERVER_TYPE;

/* The font type which should be used for presentation of cited text.  The
   values are the same numeric values that are stored in the prefs db.
 */
typedef enum
{
  MSG_PlainFont = 0,
  MSG_BoldFont = 1,
  MSG_ItalicFont = 2,
  MSG_BoldItalicFont = 3
} MSG_FONT;

/* The font size which should be used for presentation of cited text.
 */
typedef enum
{
  MSG_NormalSize,
  MSG_Bigger,
  MSG_Smaller
} MSG_CITATION_SIZE;

typedef enum
{
  MSG_NotUsed,
  MSG_Checked,
  MSG_Unchecked
} MSG_COMMAND_CHECK_STATE;

/*	UndoStatus is returned by Undo and Redo to indicate whether the undo/redo
	action is complete (i.e., operation is synchronous and was successful), or 
	in progress (undo action kicked off a url), or it failed.
*/
typedef enum
{
	UndoError,			/* Should we remove the undo obj (?) Hmmm ...*/
	UndoComplete,		/* Synchronous undo status*/
	UndoInProgress,		/* Asynchronous undo status*/
	UndoFailed,			/* Should we remove the undo obj (?) Hmmm...*/
	UndoInterrupted 	 
} UndoStatus;

typedef enum
{
	UndoIdle,		/* Normal situation */
	UndoUndoing,		/* We are Undoing ...*/
	UndoRedoing 		/* We are Redoing ...*/
} UndoMgrState;

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

/* Message Compose Editor Type */
typedef enum
{
  MSG_DEFAULT = 0,
  MSG_PLAINTEXT_EDITOR,
  MSG_HTML_EDITOR
} MSG_EditorType;

/* What kind of change was made to a list.  Used in calls to
   FE_ListChangeStarting and FE_ListChangeFinished. */

typedef enum
{
  MSG_NotifyNone,				/* No change; this call is just being used
								   to potentially nest other sets of calls
								   inside it.  The "where" and "num" parameters
								   are unused. */
  MSG_NotifyInsertOrDelete,		/* Some lines have been inserted or deleted.
								   The "where" parameter will indicate
								   the first line that has been added or
								   removed; the "num" parameter will indicate
								   how many lines, and will be positive on
								   an insertion and negative on a deletion. */
  MSG_NotifyChanged,			/* Some lines have had their contents changed
								   (e.g., messages have been marked read
								   or something.)  "where" indicates the
								   first line with a change; "num" indicates
								   how many chaged. */
  MSG_NotifyScramble,			/* Everything changed.  Probably means we
								   resorted the folder.  We are still working
								   with the same set of items, or at least
								   have some overlap, but all the indices are
								   invalid.  The "where" and "num" parameters
								   are unused. */
  MSG_NotifyAll				/* Everything changed.  We're now not
								   displaying anything like what we were; we
								   probably opened a new folder or something.
								   The FE needs to forget anything it ever knew
								   about what was being displayed, and start
								   over.  The "where" and "num" parameters are
								   unused.  */
} MSG_NOTIFY_CODE;

/* How has a pane changed? Used in calls to FE_PaneChanged. If the
	value passed to FE_PaneChanged becomes meaningful, it should
	be documented here.
 */
typedef enum
{
	MSG_PaneDirectoriesChanged,		/* The list of directories has changed.  The 
										user has edited them by either reordering
										or deleting or changing the name.  The
										address book window will need to rebuild
										its directory list. */
    MSG_PaneNotifyMessageLoaded,     /* a new message has started to be loaded 
                                        into a message pane. */
    MSG_PaneNotifyFolderInfoChanged, /* the folder info for a thread pane has
                                        changed. */
    MSG_PaneNotifyFolderLoaded,      /* a folder has finished loading into a 
                                        thread pane */
    MSG_PaneNotifyFolderLoadedSync,  /* a folder has finished loading synchronously 
                                        into athread pane */
    MSG_PaneNotifyLastMessageDeleted,/* User has deleted the last message in
                                        a folder. Some FE's may want to close
                                        the message window. */
	MSG_PaneNotifyFolderDeleted,     /* User has deleted a folder. If it is
                                        open in a thread window, the FE
                                        should close the window */
	MSG_PaneNotifyMessageDeleted,    /* User has deleted a message. If it is
                                        open in a standalone msg window, the 
                                        FE should close the window */
    MSG_PanePastPasswordCheck,        /* Get New mail is past password check. Might
                                        be a good time to show progress window. */
	MSG_PaneProgressDone,			  /* Send this when you are a libmsg API that
										 usually runs a url in a progress pane but
										 you don't start a url. FE's, do something
										 reasonable (like take down progress pane)*/
    MSG_PaneNotifySelectNewFolder,    /* Sent when a new folder is created, and is
                                        supposed to be selected by the FE */

	MSG_PaneNotifyNewFolderFailed,    /* Sent when a new folder creation attempt
										fails */
	MSG_PaneNotifyIMAPClosed,         /* Sent to a folder or message pane when
										the IMAP connection with which it is
										associated is closing. */
    MSG_PaneNotifyCopyFinished,        /* Tell the FE that a message copy operation
									     has completed. They may have disabled
										 selection in order to prevent interruption */
	MSG_PaneChanged,				   /* Introduced for the 2 pane Address Book. Contents have changed through
										  another pane. This pane's data is no longer up to date */
	MSG_PaneClose					   /* Introduced for the 2 pane AB. This pane needs to be closed */
} MSG_PANE_CHANGED_NOTIFY_CODE;





/* ===========================================================================
   						FLAGS AND MASKS
   ===========================================================================
 */

/* Flags about a single message.  These values are used in the MSG_MessageLine
   struct and in a folder's mozilla-status line. The summary file database
   uses a different internal set of flags.
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


/* This set enumerates the header fields which may be displayed in the
   message composition window.
 */
typedef uint32 MSG_HEADER_SET;
#define MSG_FROM_HEADER_MASK				0x00000001
#define MSG_REPLY_TO_HEADER_MASK			0x00000002
#define MSG_TO_HEADER_MASK					0x00000004
#define MSG_CC_HEADER_MASK					0x00000008
#define MSG_BCC_HEADER_MASK					0x00000010
#define MSG_FCC_HEADER_MASK					0x00000020
#define MSG_NEWSGROUPS_HEADER_MASK			0x00000040
#define MSG_FOLLOWUP_TO_HEADER_MASK			0x00000080
#define MSG_SUBJECT_HEADER_MASK				0x00000100
#define MSG_ATTACHMENTS_HEADER_MASK			0x00000200

/* These next four are typically not ever displayed in the UI, but are still
   stored and used internally. */
#define MSG_ORGANIZATION_HEADER_MASK		0x00000400
#define MSG_REFERENCES_HEADER_MASK			0x00000800
#define MSG_OTHERRANDOMHEADERS_HEADER_MASK	0x00001000
#define MSG_NEWSPOSTURL_HEADER_MASK			0x00002000

#define MSG_PRIORITY_HEADER_MASK            0x00004000
#define MSG_NEWS_FCC_HEADER_MASK			0x00008000
#define MSG_MESSAGE_ENCODING_HEADER_MASK	0x00010000
#define MSG_CHARACTER_SET_HEADER_MASK		0x00020000
#define MSG_MESSAGE_ID_HEADER_MASK			0x00040000
#define MSG_NEWS_BCC_HEADER_MASK            0x00080000

/* This is also not exposed to the UI; it's used internally to help remember
   whether the original message had an HTML portion that we can quote. */
#define MSG_HTML_PART_HEADER_MASK			0x00100000

/* The "body=" pseudo-header (as in "mailto:me?body=hi+there") */
#define MSG_DEFAULTBODY_HEADER_MASK			0x00200000

#define MSG_X_TEMPLATE_HEADER_MASK          0x00400000

/* IMAP folders for posting */
#define MSG_IMAP_FOLDER_HEADER_MASK			0x02000000

typedef enum {
	MSG_RETURN_RECEIPT_BOOL_HEADER_MASK = 0,
	MSG_ENCRYPTED_BOOL_HEADER_MASK,
	MSG_SIGNED_BOOL_HEADER_MASK,
	MSG_UUENCODE_BINARY_BOOL_HEADER_MASK,
	MSG_ATTACH_VCARD_BOOL_HEADER_MASK,
	MSG_LAST_BOOL_HEADER_MASK			/* last boolean header mask; must be the last one 
										 * DON'T remove.
										 */
} MSG_BOOL_HEADER_SET;




/* ===========================================================================
   						TYPES AND STRUCTS
   ===========================================================================
 */


/* 
	These can be passed to MSG_Navigate and MSG_NavigateStatus
 */
typedef enum MSG_MotionType {
  MSG_FirstMessage,
  MSG_NextMessage,
  MSG_PreviousMessage,
  MSG_LastMessage,
  MSG_FirstUnreadMessage,
  MSG_NextUnreadMessage,
  MSG_PreviousUnreadMessage,
  MSG_LastUnreadMessage,
  MSG_NextUnreadThread,
  MSG_NextCategory,
  MSG_NextUnreadCategory,
  MSG_NextUnreadGroup,
  MSG_NextFolder,
  MSG_ReadMore,
  MSG_LaterMessage,
  MSG_Back,						/* t,m: Go back to theprevious visited message */
  MSG_Forward,					/* t,m: Go forward to the previous visited message. */
  MSG_FirstFlagged,
  MSG_NextFlagged,
  MSG_PreviousFlagged,
  MSG_FirstNew,
  MSG_EditUndo,
  MSG_EditRedo
} MSG_MotionType;


/* The different views a subscribepane supports. */
typedef enum MSG_SubscribeMode {
	MSG_SubscribeAll,
	MSG_SubscribeSearch,
	MSG_SubscribeNew
} MSG_SubscribeMode;


/* Backtrack state */
typedef enum MSG_BacktrackState {
	MSG_BacktrackIdle,
	MSG_BacktrackBackward,
	MSG_BacktrackForward
} MSG_BacktrackState;


/* INSTANCES of MSG_Prefs are used to communicate to the msglib what the values
   of various preferences are.  It's really a temporary hack; I hope a more
   general XP method of preferences evolves instead. */

#ifdef XP_CPLUSPLUS
class MSG_Prefs;
#else
typedef struct MSG_Prefs MSG_Prefs;
#endif


/* Instances of MSG_Master represent the entire universe of either mail or
   news. */

#ifdef XP_CPLUSPLUS
class MSG_Master;
#else
typedef struct MSG_Master MSG_Master;
#endif




/* The various types of panes available. */

typedef enum {
  MSG_ANYPANE,					
  MSG_PANE,						/* vanilla MSG_Pane, probably just a placeholder
									for MSG_ProgressPane */

  MSG_MAILINGLISTPANE,
  MSG_ADDRPANE,
  MSG_FOLDERPANE,
  MSG_THREADPANE,
  MSG_MESSAGEPANE,
  MSG_COMPOSITIONPANE,
  MSG_SEARCHPANE,
  MSG_SUBSCRIBEPANE,
  AB_CONTAINERPANE,
  AB_ABPANE,
  AB_MAILINGLISTPANE,
  AB_PERSONENTRYPANE
} MSG_PaneType;




/* MSG_ViewIndex is an index into the list of messages or folders or groups,
   where zero is the first one to show, one is the second, etc... */
typedef uint32 MSG_ViewIndex;
typedef MSG_ViewIndex MsgViewIndex; 

/* MSG_VIEWINDEXNONE is used to indicate an invalid or non-existent index. */
#ifdef XP_CPLUSPLUS
const MSG_ViewIndex MSG_VIEWINDEXNONE = 0xffffffff;
#else
#define MSG_VIEWINDEXNONE 0xffffffff
#endif

/* imap message flags */
typedef uint16 imapMessageFlagsType;


/* MessageKey is a unique ID for a particular message in a folder.  If you want
   a handle to a message that will remain valid even after resorting the folder
   or otherwise changing their indices, you want one of these rather than a
   MSG_ViewIndex. */
typedef uint32 MessageKey;
/* in the process of removing because of confusion with message-id string */
typedef uint32 MessageId;	

/* MSG_MESSAGEKEYNONE is used to indicate an invalid or non-existant message. */
#ifdef XP_CPLUSPLUS
const MessageId MSG_MESSAGEIDNONE = 0xffffffff;
const MessageKey MSG_MESSAGEKEYNONE = 0xffffffff;
#else
#define MSG_MESSAGEIDNONE 0xffffffff
#define MSG_MESSAGEKEYNONE 0xffffffff
#endif


/* Similarly, MSG_FolderInfo* is a unique ID for a particular folder. */

#ifdef XP_CPLUSPLUS
class MSG_FolderInfo;
#else
typedef struct MSG_FolderInfo *MSG_FolderInfo;
#endif


/* And MSG_GroupName is a unique ID for a group name in the subscribe pane. */

#ifdef XP_CPLUSPLUS
class MSG_GroupName;
#else
typedef struct MSG_GroupName *MSG_GroupName;
#endif


/* MSG_NewsHost represents a newsserver that we know about. */

#ifdef XP_CPLUSPLUS
class MSG_NewsHost;
#else
typedef struct MSG_NewsHost *MSG_NewsHost;
#endif


/* MSG_IMAPHost represents an imap server that we know about. */

#ifdef XP_CPLUSPLUS
class MSG_IMAPHost;
#else
typedef struct MSG_IMAPHost *MSG_IMAPHost;
#endif


/* MSG_Host represents a news or imap server that we know about. */

/* SUBSCRIBE_USE_OLD_API: REVISIT */
#ifdef XP_CPLUSPLUS
class MSG_Host;
#else
typedef struct MSG_Host *MSG_Host;
#endif /* XP_CPLUSPLUS */


/* used in MWContext to communicate IMAP stuff between libmsg and libnet */

#ifdef XP_CPLUSPLUS
class MSG_IMAPFolderInfoMail;
class TImapServerState;
class TNavigatorImapConnection;
#else
typedef struct MSG_IMAPFolderInfoMail *MSG_IMAPFolderInfoMail;
typedef struct TImapServerState *TImapServerState;
typedef struct TNavigatorImapConnection TNavigatorImapConnection;
#endif

struct MSG_AttachmentData
{
  char *url;			/* The URL to attach.
						   This should be 0 to signify "end of list".
						 */

  char *desired_type;	/* The type to which this document should be
						   converted.  Legal values are NULL, TEXT_PLAIN
						   and APPLICATION_POSTSCRIPT (which are macros
						   defined in net.h); other values are ignored.
						 */

  char *real_type;		/* The type of the URL if known, otherwise NULL.
						   For example, if you were attaching a temp file
						   which was known to contain HTML data, you would
						   pass in TEXT_HTML as the real_type, to override
						   whatever type the name of the tmp file might
						   otherwise indicate.
						 */
  char *real_encoding;	/* Goes along with real_type */

  char *real_name;		/* The original name of this document, which will
						   eventually show up in the Content-Disposition
						   header.  For example, if you had copied a
						   document to a tmp file, this would be the
						   original, human-readable name of the document.
						 */

  char *description;	/* If you put a string here, it will show up as
						   the Content-Description header.  This can be
						   any explanatory text; it's not a file name.
						 */

  char *x_mac_type, *x_mac_creator;
						/* Mac-specific data that should show up as optional parameters
						   to the content-type header.
						 */
};


/* This structure is the interface between compose.c and composew.c.
   When we have downloaded a URL to a tmp file for attaching, this
   represents everything we learned about it (and did to it) in the
   process. 
 */
/* Used by libmime -- mimedrft.c
 * Front end shouldn't use this structure.
 */
typedef struct MSG_AttachedFile
{
  char *orig_url;		/* Where it came from on the network (or even elsewhere
						   on the local disk.)
						 */
  char *file_name;		/* The tmp file in which the (possibly converted) data
						   now resides.
						*/
  char *type;			/* The type of the data in file_name (not necessarily
						   the same as the type of orig_url.)
						 */
  char *encoding;		/* Likewise, the encoding of the tmp file.
						   This will be set only if the original document had
						   an encoding already; we don't do base64 encoding and
						   so forth until it's time to assemble a full MIME
						   message of all parts.
						 */

  /* #### I'm not entirely sure where this data is going to come from...
   */
  char *description;					/* For Content-Description header */
  char *x_mac_type, *x_mac_creator;		/* mac-specific info */
  char *real_name;						/* The real name of the file. */

  /* Some statistics about the data that was written to the file, so that when
	 it comes time to compose a MIME message, we can make an informed decision
	 about what Content-Transfer-Encoding would be best for this attachment.
	 (If it's encoded already, we ignore this information and ship it as-is.)
   */
  uint32 size;
  uint32 unprintable_count;
  uint32 highbit_count;
  uint32 ctl_count;
  uint32 null_count;
  uint32 max_line_length;
  
  XP_Bool decrypted_p;	/* S/MIME -- when attaching a message that was
						   encrypted, it's necessary to decrypt it first
						   (since nobody but the original recipient can read
						   it -- if you forward it to someone in the raw, it
						   will be useless to them.)  This flag indicates
						   whether decryption occurred, so that libmsg can
						   issue appropriate warnings about doing a cleartext
						   forward of a message that was originally encrypted.
						 */
} MSG_AttachedFile;



/* This structure represents a single line in the folder pane.
 */
typedef struct MSG_FolderLine
{
  MSG_FolderInfo* id;
  const char* name;				/* The name of the folder to display. */
  const char* prettyName;		/* The pretty name to display */
  uint8 level;					/* How many parent folders we have. */
  uint32 flags;
  uint32 prefFlags;

  /* The below are used only if the icon type is MSG_NewsgroupIcon or
	 MSG_FolderIcon. */
  int32 unseen;					/* Number of unseen articles. (If negative,
								   then we don't know yet; display question
								   marks or something.*/
  int32 total;					/* Total number of articles. */
  uint16 numChildren;			/* How many immediate children of this folder
								   exist. */
  int32 deepUnseen;             /* Totals for this and child folders */
  int32 deepTotal; 
  int32 deletedBytes;			/* mail only - total size of deleted messages */
} MSG_FolderLine;


/* This structure represents a single line in the thread pane.
 */
typedef struct MSG_MessageLine
{
	MessageKey   threadId; 		/* (article # of thread - article could be
								   expired) */
	MessageKey	messageKey; 	/* news: article num, mail: mbox offset */
	char		subject[MSG_MAXSUBJECTLENGTH];
	char		author[MSG_MAXAUTHORLENGTH];
	time_t 		date;                         

	uint32		messageLines;	/* Number of lines in this message. */
	uint32		flags;
	MSG_PRIORITY priority;		/* message priority (mail only?) */
	uint16		numChildren;	/* for top-level threads */
	uint16		numNewChildren;	/* for top-level threads */
	int8		level;			/* indentation level */
} MSG_MessageLine;

/* This structure represents a single line in the subscribe pane.
 */

typedef struct MSG_GroupNameLine
{
	char name[MSG_MAXGROUPNAMELENGTH];
	int8 level;
	uint32 flags;
	int32 total;
} MSG_GroupNameLine;

/* This structure is used as an annotation about the font and size information
   included in a text string passed to us by the front end after the user has
   edited their message.
 */
typedef struct MSG_FontCode
{
  uint32 pos;
  uint32 length;
  MSG_FONT font;
  uint8 font_size;
  XP_Bool fixed_width_p;
} MSG_FontCode;



/* MSG_CompositionFields represents the desired initial state of the various
   fields in a composition.  */


#ifdef XP_CPLUSPLUS
class MSG_CompositionFields;
#else
typedef struct MSG_CompositionFields MSG_CompositionFields;
#endif


/* MSG_HTMLComposeAction lists what action to take with a message composed
   with HTML. */

typedef enum {
	MSG_HTMLAskUser,
	MSG_HTMLUseMultipartAlternative,
	MSG_HTMLConvertToPlaintext,
	MSG_HTMLSendAsHTML
} MSG_HTMLComposeAction;

/* MSG_RecipientList specifies a list of recipients to be displayed in the
   recipients dialog box.  It is terminated by an entry which has name set to
   NULL and value set to something negative. */
typedef struct {
	int32 value;
	const char* name;
} MSG_RecipientList;




/* Secure mail related
 */
typedef enum {
	certNone,
	certValid,
	certExpired,
	certRevoked
} MSG_CertStatus;

typedef struct {
	char *PrettyName;
	char *EmailAddress;
	MSG_CertStatus CertStatus;
} MSG_CertInfo;

typedef enum
{
	msgNoRecipients = 0,
	msgNoCerts = 1,
	msgHasCerts = 2,
	msgSomeCerts = 4
} MSG_SecurityLevel;

typedef enum
{
	crypto_RC4_40bit,
	crypto_RC4_128bit
} MSG_CryptoGrade;


typedef struct {
	/* DoFetchGroups() requests the front end do a
	   MSG_Command(MSG_FetchGroupList) on the subscribe pane, also doing
	   whatever side effects the front end usually does on such a request. */
	void (*DoFetchGroups)(MSG_Pane* pane, void* closure);

	/* FetchCompleted() tells the FE that a MSG_FetchGroupList or
	   MSG_CheckForNew command has completed. */
	void (*FetchCompleted)(MSG_Pane* pane, void* closure);
} MSG_SubscribeCallbacks;


typedef struct {
	/* AttachmentCount() tells the FE the number of attachments currently
	   known for the message being displayed in the given messagepane.  
	   finishedloading is TRUE iff we have finished loading the message.
	   If finishedloading is FALSE, then the FE must be prepared to have this
	   callback fire again soon, as more attachments may be found as
	   downloading proceeds. */
	void (*AttachmentCount)(MSG_Pane* messagepane, void* closure,
							int32 numattachments, XP_Bool finishedloading);


	/* UserWantsToSeeAttachments() gets called when the user clicks on the
	   little "show me attachment info" at the top of a mail message.  */
	void (*UserWantsToSeeAttachments)(MSG_Pane* messagepane, void* closure);
} MSG_MessagePaneCallbacks;


typedef struct {
	/* CreateAskHTMLDialog() tells the FE to bring up the dialog that lets the
	   user choose the disposition of this HTML composition.  If the user
	   chooses a format for HTML and clicks "Send", then the FE must call
	   MSG_SetHTMLAction() and return 0.  If the FE returns > 0, then
	   the message send is cancelled.  If  the user presses the "Recipients" 
	   button, then the the FE must call MSG_PutUpRecipientsDialog().  If the 
	   FE does not provide this CreateAskHTMLDialog() callback, or if this 
	   routine returns a negative number, then the back end will implement 
	it using HTML dialogs. */
	int (*CreateAskHTMLDialog)(MSG_Pane* composepane, void* closure);


	/* CreateRecipientsDialog() tells the FE to bring up the dialog that lets
	   the user choose whether the recipients of a message can deal with HTML.
	   The FE must notify the back end of the results by calling
	   MSG_ResultsRecipients().  If the FE does not provide this callback, or
	   if this routine returns a negative number, then the back end will
	   implement it using HTML dialogs.

	   The last two arguments specify the initial contents of the two scrolling
	   lists.  The given strings are to be displayed; the given ID's are to be
	   used in the call to MSG_ResultsRecipients().  (These ID's are also to be
	   used as a sort key so that the recipient list is always displayed sorted
	   in the right order.)  These structures are only valid for the length of
	   this call; the FE must copy any data it needs out of them before the
	   call returns. The void* pWnd parameter is used to pass the in parent window
	   of the RecipientsDialog.*/
	int (*CreateRecipientsDialog)(MSG_Pane* composepane, void* closure,
								  MSG_RecipientList* nohtml,
								  MSG_RecipientList* htmlok,void* pWnd);


} MSG_CompositionPaneCallbacks;





/* ===========================================================================
   							INIT / CREATE
   ===========================================================================
 */


XP_BEGIN_PROTOS

void MSG_InitMsgLib(void);  /* one-time initialization of libmsg */
void MSG_ShutdownMsgLib(void);      /* shut down LibMsg part of app. */

/* launch a url to compress mail folders and purge news dbs. */
XP_Bool MSG_CleanupNeeded(MSG_Master *master);
void MSG_CleanupFolders(MSG_Pane *pane); 

/* currently just to give db time to clean up */
void MSG_OnIdle(void);


int32 MSG_SetLibNeoCacheSize(int32 newCacheSize);

/* Initialize the mail/news universe.  A MSG_Master* object must be created
   before anything else can be done.  Only one MSG_Master* object can exist at
   any time.  */

extern MSG_Master* MSG_InitializeMail(MSG_Prefs* prefs);

/* Routines to create the various panes.  Those panes that require a true
   MWContext* take it as a parameter.  Any given thread pane is always
   associated with a particular folder pane; any given message pane is always
   associated with a particular thread pane.

   The entire creation process goes like this:

   - The FE decides to create a new pane.

     - The FE creates any necessary contexts and maybe some windows and stuff
   that it will associate with the pane.  

   - The FE calls MSG_Create*Pane() to create the pane object itself.  When
   creating a folderpane, the FE must also provide a pointer to a MSG_Prefs
   object that contains the preferences to be used for the folderpane.  The FE
   must be sure not to destroy that MSG_Prefs object as long as the folderpane
   exists. Any later change made to the MSG_Prefs will automatically be
   reflected in the folderpane and all related panes.  (Also note that when
   creating a folderpane, msglib uses the context type to determine whether
   this is for mail or news.)

   - The FE puts the resulting pane structure into its datastructures
   somewhere, and probably calls MSG_SetFEData() to assocatiate that
   datastructure with the pane.

*/

extern MSG_Pane* MSG_CreateFolderPane(MWContext* context,
									  MSG_Master* master);


extern MSG_Pane* MSG_CreateThreadPane(MWContext* context,
									  MSG_Master* master);

extern MSG_Pane* MSG_CreateMessagePane(MWContext* context,
									   MSG_Master* master);

extern int MSG_SetMessagePaneCallbacks(MSG_Pane* messagepane,
									   MSG_MessagePaneCallbacks* callbacks,
									   void* closure);

extern MSG_MessagePaneCallbacks*
MSG_GetMessagePaneCallbacks(MSG_Pane* messagepane, void** closure);


extern MSG_Pane* MSG_CreateCompositionPane(MWContext* context,
										   MWContext* old_context,
										   MSG_Prefs* prefs,
										   MSG_CompositionFields* fields,
										   MSG_Master* master);

extern int
MSG_SetCompositionPaneCallbacks(MSG_Pane* composepane,
								MSG_CompositionPaneCallbacks* callbacks,
								void* closure);

/* Typically, progress panes come down when you receive all connections complete,
	or you get a FE_PaneChanged MSG_PaneProgressDone, which gets sent when a
	command runs in a progress pane which doesn't launch a url.
*/
extern MSG_Pane* MSG_CreateProgressPane (MWContext *context,
										 MSG_Master *master,
										 MSG_Pane *parentPane);

/* WinFE (at least) has found that creating the composition pane in one swell
   foop is too much to handle.  They really want to create the pane pointer,
   but not start its initializing until some later point.  (The reason this is
   nasty is that composition pane initialization can sometimes happen in the
   background as we download attachments.)  So, if you don't want to call
   MSG_CreateCompositionPane(), you can instead call
   MSG_CreateCompositionPaneNoInit() and then soon call
   MSG_InitializeCompositionPane().  What fun. */

extern MSG_Pane* MSG_CreateCompositionPaneNoInit(MWContext* context,
												 MSG_Prefs* prefs,
												 MSG_Master* master);

extern int MSG_InitializeCompositionPane(MSG_Pane* comppane, 
										 MWContext* old_context,
										 MSG_CompositionFields* fields);




extern MSG_Pane* MSG_CreateSearchPane (MWContext *context,
									   MSG_Master *master);

#ifdef SUBSCRIBE_USE_OLD_API
/* This routine is obsoleted; instead, use MSG_CreateSubscribePaneOnHost(). */
extern MSG_Pane* MSG_CreateSubscribePane(MWContext* context,
										 MSG_Master* master);

/* Bring up the subscribe UI on the given newshost.  If host is NULL,
   uses the default newshost. */
extern MSG_Pane* MSG_CreateSubscribePaneOnHost(MWContext* context,
											   MSG_Master* master,
											   MSG_NewsHost* host);
#endif /* SUBSCRIBE_USE_OLD_API */

/* Bring up the subscribe UI on the given news or imap host.
   If host is NULL, uses the default newshost. */
extern MSG_Pane* MSG_CreateSubscribePaneForHost(MWContext* context,
											   MSG_Master* master,
											   MSG_Host* host);


/* Tells the FEs to bring up the subscribe UI on the given news
   or imap host. */
extern XP_Bool FE_CreateSubscribePaneOnHost(MSG_Master* master,
											MWContext* parentContext,
											MSG_Host* host);

/* Message compositions sometimes (usually) get kicked off by the backend
   (like, the user clicks on a "mailto" link).  So, this call requests the FE
   to create a new context, and call MSG_CreateCompositionPane and bring up a
   composition window.  The given context and MSG_CompositionFields* are to be
   passed on to MSG_CreateCompositionPane(), and lets the backend know what the
   initial values of all the fields are to be.  The FE should then query the
   pane (using MSG_GetCompHeader() and MSG_GetCompBody()) so that it can find
   out what to display in the UI.

   The FE should bring up either the plaintext composition or the html
   composition window, depending on the user's preference.  One exception,
   though: the FE must first check MSG_GetForcePlainText().  If TRUE, then
   we are replying to a plaintext message, and the FE *must* bring up the
   plaintext composition window.

 */


extern MSG_Pane* FE_CreateCompositionPane(MWContext* old_context,
										  MSG_CompositionFields* fields,
										  const char* initialText,
										  MSG_EditorType editorType);




/* ===========================================================================
   						RANDOM CORE FUNCTIONS (to sort)
   ===========================================================================
 */


/* Due to addition of Java/JavaScript MailNews API, other things in addition
   to composition can be kicked off by the backend, so we provide the
   following FE functions to handle those cases.
*/

/* Ask the FE for the current master.  FE should create one if it doesn't exist */
extern MSG_Master* FE_GetMaster();

/* Routines to associate or get arbitrary FE-specific data from a pane. */

extern void MSG_SetFEData(MSG_Pane* pane, void* data);
extern void* MSG_GetFEData(MSG_Pane* pane);

/* Routines to to special things when Netscape mail is not being used */

extern XP_Bool FE_IsAltMailUsed(MWContext * context);

/* Routine to bring up the IMAP Subscription Upgrade dialog box	*/
/* Return value based on selection:  Automatic = 0, Custom = 1 */

extern int FE_PromptIMAPSubscriptionUpgrade (MWContext * context);


/*  This function is called by the backend to notify the frontend details about 
	new mail state so that the FE can notify the stand-alone biff that something
	has changed.
*/

#ifdef XP_WIN32		/* only supported on Win32 now */
typedef enum {
	MSG_IMAPHighWaterMark,			/* data is the IMAP UID highwater mark for the inbox */
} MSG_BiffInfoType;

#define FE_SetBiffInfoDefined		/*  this symbol used in the back-end to 
										know if FE_SetBiffInfo is defined */
extern uint32 FE_SetBiffInfo(MSG_BiffInfoType type, uint32 data);
#endif

/* run the url in the given pane. This will set the msg_pane member
   in url, interrupt the context, and then call FE_GetURL */
extern int MSG_GetURL(MSG_Pane *pane, URL_Struct* url);

/* routines to set and get the text type, setting true indicates that the
   text is HTML */

extern XP_Bool MSG_GetHTMLMarkup(MSG_Pane * composepane);
extern void MSG_SetHTMLMarkup(MSG_Pane * composepane,XP_Bool flag);

/* Returns the type of the given pane. */

extern MSG_PaneType MSG_GetPaneType(MSG_Pane* pane);


/* Finds a pane with the given type.  First, it looks for a pane which has the
   given context associated with it; failing that, it returns any pane it can
   find.
 */
extern MSG_Pane* MSG_FindPane(MWContext* context, MSG_PaneType type);

/* really find the pane of passed type with given context, NULL otherwise */
extern MSG_Pane *MSG_FindPaneOfContext (MWContext *context, MSG_PaneType type);

extern MSG_Pane *MSG_FindPaneOfType(MSG_Master *master, MSG_FolderInfo *id, MSG_PaneType type);

extern MSG_Pane *MSG_FindPaneFromUrl (MSG_Pane *pane, const char *url, MSG_PaneType type);

/* Returns the context associated with a given pane.  If this pane doesn't have
   a context (i.e., it's a threadpane), then it will return NULL. */

extern MWContext* MSG_GetContext(MSG_Pane* pane);


/* Returns the MSG_Prefs* object that is being used to determine the
   preferences for this pane. */

extern MSG_Prefs* MSG_GetPrefs(MSG_Pane* pane);


/* Returns the MSG_Prefs* object that is being used to determine the
   preferences for this master object. */

extern MSG_Prefs* MSG_GetPrefsForMaster(MSG_Master* master);


/* Returns the MSG_Master* object for the given pane. */

extern MSG_Master* MSG_GetMaster(MSG_Pane* pane);

extern XP_Bool MSG_DeliveryInProgress(MSG_Pane * composepane);


/* This is the front end's single interface to executing mail/news menu
   and toolbar commands.  See the comment above the definition of the
   MSG_CommandType enum.

   The indices and numindices arguments indicates which folders or threads are
   to be affected by this command.  For many commands, they'll be ignored.
   They should always be NULL if not specifying a folderpane or a threadpane.
 */
extern void MSG_Command (MSG_Pane* pane, MSG_CommandType command,
						 MSG_ViewIndex* indices, int32 numindices);


/* Before the front end displays any menu (each time), it should call this
   function for each command on that menu to determine how it should be
   displayed.

   selectable_p:	whether the user should be allowed to select this item;
					if this is FALSE, the item should be grayed out.

   selected_p:		if this command is a toggle or multiple-choice menu item,
					this will be MSG_Checked if the item should be `checked',
                    MSG_Unchecked if it should not be, and MSG_NotUsed if the
                    item is a `normal' menu item that never has a checkmark.

   display_string:	the string to put in the menu.  l10n handwave handwave.

   plural_p:		if it happens that you're not using display_string, then
					this tells you whether the string you display in the menu
					should be singular or plural ("Delete Message" versus
					"Delete Selected Messages".)  If you're using
					display_string, you don't need to look at this value.

   Returned value is negative if something went wrong.
 */
extern int MSG_CommandStatus (MSG_Pane* pane,
							  MSG_CommandType command,
							  MSG_ViewIndex* indices, int32 numindices,
							  XP_Bool *selectable_p,
							  MSG_COMMAND_CHECK_STATE *selected_p,
							  const char **display_string,
							  XP_Bool *plural_p);


/* Set the value of a toggle-style command to the given value.  Returns
   negative if something went wrong (like, you gave it a non-toggling
   command, or you passed in MSG_NotUsed for the check state.) */
extern int MSG_SetToggleStatus(MSG_Pane* pane, MSG_CommandType command,
							   MSG_ViewIndex* indices, int32 numindices,
							   MSG_COMMAND_CHECK_STATE value);

/* Queries whether the given toggle-style command is on or off. */
extern MSG_COMMAND_CHECK_STATE MSG_GetToggleStatus(MSG_Pane* pane,
												   MSG_CommandType command,
												   MSG_ViewIndex* indices,
												   int32 numindices);


/* Navigation commands */
extern int MSG_DataNavigate(MSG_Pane* pane, MSG_MotionType motion,
						MessageKey startId, MessageKey *resultId, 
						MessageKey *resultThreadId);

/* This routine will automatically expand the destination thread, if needs be. */
extern int MSG_ViewNavigate(MSG_Pane* pane, MSG_MotionType motion,
						MSG_ViewIndex startIndex, 
						MessageKey *resultId, MSG_ViewIndex *resultIndex, 
						MSG_ViewIndex *pThreadIndex,
						MSG_FolderInfo **folderInfo);

/* Indicates if navigation of the passed motion type is valid. */
extern int MSG_NavigateStatus (MSG_Pane* pane,
							  MSG_MotionType command,
							  MSG_ViewIndex index,
							  XP_Bool *selectable_p,
							  const char **display_string);

extern int MSG_MarkReadByDate (MSG_Pane* pane, time_t startDate, time_t endDate);

/* notification from Server Admin page via JavaScript that an imap folder has changed */
extern void MSG_IMAPFolderChangedNotification(const char *folder_url);

/* record the imap connection in the move state of the current context */
extern void MSG_StoreNavigatorIMAPConnectionInMoveState(MWContext *context, 
													    TNavigatorImapConnection *connection);

/* Determines whether we are currently actually showing the recipients
   in the "Sender" column of the display (because we are in the "sent"
   folder).  FE's should call this in their FE_UpdateToolbar to change
   the title of this column.*/

extern XP_Bool MSG_DisplayingRecipients(MSG_Pane* threadpane);




/* The msg library calls FE_ListChangeStarting() to tell the front end that the
   contents of the message or folder list are about to change.  It means that
   any MSG_ViewIndex values that the FE may be keeping around might become
   invalid.  The FE should adjust them, throw them out, or convert them into
   MessageKey or MSG_FolderInfo* values, instead.

   This call will be quickly followed by a matching call to
   FE_ListChangeFinished().

   The "asynchronous" flag will be TRUE if this change happened due to some
   completely asynchronous event, and FALSE if we are in the middle of a call
   to MSG_Command() or other calls from the FE that directly manipulate the
   msglib.  Some FE's (Mac) will choose to ignore these calls if asynchronous
   is FALSE, while other FE's (XFE) will likely ignore the parameter.

   The notify parameter describes what changed, along with the where and num
   parameters.  See the documentation in the declaration of MSG_NOTIFY_CODE,
   above. */

extern void FE_ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
								  MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
								  int32 num);


/* FE_ListChangeFinished() gets called only after a call to
   FE_ListChangeStarting().  It means that it is safe for the FE to convert
   back from MessageKey or MSG_FolderInfo* into MSG_ViewIndex.  It's also a good
   clue that the FE ought to redisplay the list.  If the folderpane is given,
   then the FE probably will want to regenerate any list of folders presented
   in menus. */

extern void FE_ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
								  MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
								  int32 num);

/*	The backend calls this when something changed about the pane other than the
	list. For example, if we load a different message into a message pane,
	we call FE_PaneChanged with a notify code of MSG_PaneNotifyMessageLoaded. Or, if
	the folder info for a folder displayed in a thread pane changes, 
	we call FE_PaneChanged with a notify code of MSG_PaneNotifyFolderInfoChanged.
*/
extern void FE_PaneChanged(MSG_Pane *pane, XP_Bool asynchronous, 
						   MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);

/* The front end calls this *after* it has handled the FE_PaneChanged notification.
   This function handles callbacks set up by back-end code to detect changes in the
   mail system.  Currently, it is used by the netscape.messaging.* classes
   to allow callbacks in the java classes when stuff in the mail backend changes.
*/
extern void MSG_HandlePaneChangedNotifications(MSG_Pane *pane, XP_Bool asynchronous, 
						   MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);

/* Declaration for backend pane change callback */
typedef void (*MSG_PANE_CHANGED_CALLBACK)(MSG_Pane *pane, XP_Bool asynchronous, 
						   MSG_PANE_CHANGED_NOTIFY_CODE, int32 value);

/* Registration function to register call backs.  Currently allows only one, but should
   be expanded to allow multiple observers */
extern void MSG_RegisterPaneChangedCallback(MSG_PANE_CHANGED_CALLBACK cb);

/* The front end can use this call to determine what the indentation depth is
   needed to display all the icons in the folder list.  The XFE uses this to
   dynamically resize the icon column. In true C style, the number returned is
   actually one bigger than the biggest depth the FE will ever get. */
extern int MSG_GetFolderMaxDepth(MSG_Pane* folderpane);


/* The front end uses these calls to determine what to display on particular
   lines in the outline lists.  They return TRUE on success, FALSE if the given
   line doesn't exist or if there are less than numlines worth of data to get.
   The given data pointer must point to an array of at least numlines
   elements. */

extern XP_Bool MSG_GetFolderLineByIndex(MSG_Pane* folderpane,
										MSG_ViewIndex firstline,
										int32 numlines,
										MSG_FolderLine* data);

/* get the flags for a folder w/o the whole MSG_FolderLine */
extern uint32 MSG_GetFolderFlags(MSG_FolderInfo *folder);

extern int32 MSG_GetFolderSizeOnDisk (MSG_FolderInfo *folder);
extern XP_Bool MSG_GetThreadLineByIndex(MSG_Pane* threadpane,
										MSG_ViewIndex firstline,
										int32 numlines,
										MSG_MessageLine* data);

extern int MSG_GetFolderLevelByIndex( MSG_Pane *folderpane,
									  MSG_ViewIndex line );

extern int MSG_GetThreadLevelByIndex( MSG_Pane *threadpane,
									  MSG_ViewIndex line );

/* If the FE wants to find out info about a particular folder or thread by its
   id rather than by its index, it can use these calls. */

extern XP_Bool MSG_GetFolderLineById(MSG_Master* master,
									 MSG_FolderInfo* info,
									 MSG_FolderLine* data);
extern XP_Bool MSG_GetThreadLineById(MSG_Pane* pane, /* thread or message */
									 MessageKey id,
									 MSG_MessageLine* data);

/* returns the view index of the first message in the thread containing key */
extern MSG_ViewIndex MSG_ThreadIndexOfMsg(MSG_Pane* pane, MessageKey key);

/* Takes a time (most likely, the one returned in the MSG_MessageLine
   structure by MSG_GetThreadLine()) and formats it into a compact textual
   representation (like "Thursday" or "09:53:17" or "10/3 16:15"), based on
   how that date relates to the current time.  This is returned in a
   statically allocated buffer; the caller should copy it somewhere. */
extern const char* MSG_FormatDate(MSG_Pane* pane, time_t date);

/* If you don't have a pane and need to format a date ... */
extern const char* MSG_FormatDateFromContext(MWContext *context, time_t date);

/* Change the priority on a mail message */
extern XP_Bool MSG_SetPriority(MSG_Pane *pane, /* thread or message */
						   MessageKey key, 
						   MSG_PRIORITY priority);

/* The msg library calls this to get a temporary filename and type (suitable
   for passing to XP_FileOpen, etc) to be used for storing a temporary working
   copy of the given filename and type.  The returned file will usually be
   renamed back onto the given filename, using XP_FileRename().
 
   The returned filename will be freed using XP_FREE().  */

/* #### this should be replaced with a backup-version-hacking XP_FileOpen */
extern char* FE_GetTempFileFor(MWContext* context, const char* fname,
							   XP_FileType ftype, XP_FileType* rettype);




/* Delete the master object, meaning that we're all done with mail or news.
   It's not legal to do this unless all associated panes have already been
   destroyed. */

extern void MSG_DestroyMaster(MSG_Master* master);


/* A pane is going away. */

extern void MSG_DestroyPane(MSG_Pane* pane);



/* Load the given folder into the given threadpane. */

extern int MSG_LoadFolder(MSG_Pane* threadpane, MSG_FolderInfo* folder);

/* Load the given message into the given messagepane.  Also have to specify
   the folder that the pane is in. */

extern int MSG_LoadMessage(MSG_Pane* messagepane, MSG_FolderInfo* folder,
						   MessageKey id);

/* Open Draft Message into a compose window */
extern int MSG_OpenDraft (MSG_Pane* threadpane, MSG_FolderInfo* folder,
						  MessageKey id);
/* Draft -- backend helper function; FE don't use it */
extern void MSG_SetLineWidth(MSG_Pane* composepane, int width);

extern void MSG_AddBacktrackMessage(MSG_Pane* pane, MSG_FolderInfo* folder,
									MessageKey id);

extern void MSG_SetBacktrackState (MSG_Pane* pane, MSG_BacktrackState state);

extern MSG_BacktrackState MSG_GetBacktrackState (MSG_Pane* pane);


/* Set what action to take when the user sends an HTML message.  This
   is set by a pull-down in the compose window, or as a result of the
   AskHTMLDialog. */
extern int MSG_SetHTMLAction(MSG_Pane* composepane,
							 MSG_HTMLComposeAction action);

/* Gets the value set above. */
extern MSG_HTMLComposeAction MSG_GetHTMLAction(MSG_Pane* composepane);

/* Asks that the recipients dialog appear.  This should only happen as a
   result of the user pressing the "Recipients" button on the AskHTMLDialog.
   the void* parameter is used to pass in the parent window for the dialog */
extern int MSG_PutUpRecipientsDialog(MSG_Pane* composepane,void *pWnd);

/* For more details on this routine, see comments for
   MSG_CompositionPaneCallbacks. */
extern int MSG_ResultsRecipients(MSG_Pane* composepane,
								 XP_Bool cancelled,
								 int32* nohtml, /* List of IDs, terminated
												  with a negative entry. */
								 int32* htmlok /* Another list of IDs. */
								 );

extern void MSG_SetPostDeliveryActionInfo (MSG_Pane* pane, 
										   void *actionInfo);

/* Setting the preloaded attachments to a compose window. Drafts only */
extern int MSG_SetPreloadedAttachments ( MSG_Pane *composepane,
										 MWContext *context,
										 void *attachmentData,
										 void *attachments,
										 int attachments_count );

#ifdef XP_UNIX
/* This is how the XFE implements non-POP message delivery.  The given donefunc
   will be called when the incorporate actually finishes, which may be before
   or after this function returns. The boolean passed to the donefunc will be
   TRUE if the incorporate succeeds, and FALSE if it fails for any reason. */
extern void MSG_IncorporateFromFile(MSG_Pane* pane, XP_File infid,
									void (*donefunc)(void*, XP_Bool),
									void* doneclosure);
#endif /* XP_UNIX */

/* ===========================================================================
   							PREFERENCES
   ===========================================================================
 */


/* Create a new place to store mail/news preferences. */

extern MSG_Prefs* MSG_CreatePrefs(void);

/* Destroy a previously created MSG_Prefs* object.  it is illegal to call this
   unless any MSG_Master* or compositionpane objects associated with this prefs
   object have already been destroyed.*/

extern void MSG_DestroyPrefs(MSG_Prefs*);



/* This is the directory where mail folders are stored. */

extern void MSG_SetFolderDirectory(MSG_Prefs* prefs, const char* directory);
extern const char* MSG_GetFolderDirectory(MSG_Prefs* prefs);


extern void MSG_GetCitationStyle(MSG_Prefs* prefs,
								 MSG_FONT* font,
								 MSG_CITATION_SIZE* size,
								 const char** color);

/* Gets/Sets the current Queue Folder name, i.e. "Outbox" or "Unsent Messages."
 Don't free the return result. */
extern const char * MSG_GetQueueFolderName();
/* Should be called with a #define'd string, i.e. QUEUE_FOLDER_NAME or QUEUE_FOLDER_NAME_OLD */
extern const char * MSG_SetQueueFolderName(const char *newName);
/* Variation on XP_GetString for special folders, except this function 
	returns safer, longer-duration strings */
extern const char * MSG_GetSpecialFolderName(int whichOne);
#ifdef XP_OS2
extern const char * MSG_GetSpecialFolderPrettyName(int whichOne);
#endif

/* Whether to automatically quote original message when replying. */
extern XP_Bool MSG_GetAutoQuoteReply(MSG_Prefs* prefs);

/* Whether to show attachments as links instead of inline. */
extern XP_Bool MSG_GetNoInlineAttachments(MSG_Prefs* prefs);

/* there are going to be a set of per-folder preferences, including
	1 pane or 2
	Configured for off-line use
	I think these can be represented with bits. If wrong, we'll have
	to change this. They should be stored in the database, NeoFolderInfo obj,
	and cached in MSG_FolderInfo.
*/
#define MSG_FOLDER_PREF_OFFLINE 0x00000001
#define MSG_FOLDER_PREF_ONEPANE 0x00000002
#define MSG_FOLDER_PREF_SHOWIGNORED 0x00000004
#define MSG_FOLDER_PREF_IMAPMARKED 0x00000008 	/* server says this folder is 'special' */
#define MSG_FOLDER_PREF_IMAPUNMARKED 0x00000010 /* different from !marked? RFC says yes */
#define MSG_FOLDER_PREF_IMAPNOINFERIORS 0x00000020 /* cannot have children folders, pity */
#define MSG_FOLDER_PREF_IMAPNOSELECT 0x00000040 /* cannot hold messages, only other folders */
#define MSG_FOLDER_PREF_OFFLINEEVENTS 0x00000080 /* this folder has offline events to play back */
#define MSG_FOLDER_PREF_LSUB_DISCOVERED 0x00000100 /* this folder was discovered using LSUB */
#define MSG_FOLDER_PREF_CREATED_OFFLINE 0x00000200 /* this folder created offline, so don't delete it */
#define MSG_FOLDER_PREF_NAMESPACE_STRIPPED 0x00000400 /* this folder has had its personal namespace stripped off */
#define MSG_FOLDER_PREF_IMAP_ACL_READ 0x00000800 /* SELECT, CHECK, FETCH, PARTIAL, SEARCH, COPY from folder */
#define MSG_FOLDER_PREF_IMAP_ACL_STORE_SEEN 0x00001000 /* STORE SEEN flag */
#define MSG_FOLDER_PREF_IMAP_ACL_WRITE 0x00002000 /* STORE flags other than SEEN and DELETED */
#define MSG_FOLDER_PREF_IMAP_ACL_INSERT 0x00004000 /* APPEND, COPY into folder */
#define MSG_FOLDER_PREF_IMAP_ACL_POST 0x00008000 /* Can I send mail to the submission address for folder? */
#define MSG_FOLDER_PREF_IMAP_ACL_CREATE_SUBFOLDER 0x00010000 /* Can I CREATE a subfolder of this folder? */
#define MSG_FOLDER_PREF_IMAP_ACL_DELETE 0x00020000 /* STORE DELETED flag, perform EXPUNGE */
#define MSG_FOLDER_PREF_IMAP_ACL_ADMINISTER 0x00040000 /* perform SETACL */
#define MSG_FOLDER_PREF_IMAP_ACL_RETRIEVED 0x00080000 /* ACL info for this folder has been initialized */
#define MSG_FOLDER_PREF_FEVALID	0x40000000  /* prefs have actually been set  */	
#define MSG_FOLDER_PREF_CACHED  0x80000000  /* we've retrieved prefs from db */

extern void MSG_SetFolderPrefFlags(MSG_FolderInfo *info, int32 flag);
extern int32 MSG_GetFolderPrefFlags(MSG_FolderInfo *info);

/* allow FE's to remember default CSID's on a per-folder basis. It would be nice 
	if there were a type other than int16 for csid's!
 */
extern void MSG_SetFolderCSID(MSG_FolderInfo *info, int16 csid);
extern int16 MSG_GetFolderCSID(MSG_FolderInfo *info);

typedef enum MSG_AdminURLType
{
	MSG_AdminServer,
	MSG_AdminServerSideFilters,
	MSG_AdminFolder,
	MSG_AdminServerLists
} MSG_AdminURLType;

/* use this to run the url */
extern XP_Bool MSG_GetAdminUrlForFolder(MWContext *context, MSG_FolderInfo *folder, MSG_AdminURLType type);
/* use this to decide to show buttons and/or menut items */
extern XP_Bool MSG_HaveAdminUrlForFolder(MSG_FolderInfo *folder, MSG_AdminURLType type);

/* ===========================================================================
   							LIST CALLBACKS
   ===========================================================================
 */


/* The FE calls these in response to mouse gestures on the scrolling lists.
   Note that folderpanes, threadpanes, and subscribepanes all have scrolling
   lists; most of these calls are valid in any of them. */


/* Change the flippy state on this thread line, if possible.  The numchanged
   returns how many lines that were added to the view (if positive), or how
   many were removed (if negative).  If you don't care, pass in NULL. */
extern void MSG_ToggleExpansion (MSG_Pane* pane, MSG_ViewIndex line,
								 int32* numchanged);



/* Returns how many lines would get added or removed from the message pane if
   MSG_ToggleExpansion were to be called.  Returns positive if lines will be
   added; negative if lines will be removed. */
extern int32 MSG_ExpansionDelta(MSG_Pane* pane, MSG_ViewIndex line);



/* These are invoked by the items on the `Copy/Move Message Into' cascading
   menus, and by drag-and-drop.  The folder name should be the full pathname of
   the folder in URL (Unix) file name syntax.  This does not have to be an
   existing folder; it can be any file on the system.  It doesn't even have to
   exist yet.  (If you do want to use an existing folder, you can get the right
   string to pass using MSG_GetFolderName().)  */

typedef enum
{	MSG_Drag_Not_Allowed	= 0x00000000
,	MSG_Require_Copy		= 0x00000001
,	MSG_Require_Move		= 0x00000002
,	MSG_Default_Drag		= 0xFFFFFFFF
} MSG_DragEffect;

/* Status calls.
   Caller passes in requested action.  If it's a required action, the returned value
   will be the request value, or MSG_Drag_Not_Allowed.  If it's a default drag request,
   the returned value will show whether the drag should be interpreted as a move or copy. */
extern MSG_DragEffect MSG_DragMessagesStatus(MSG_Pane* pane, const MSG_ViewIndex* indices,
								int32 numindices, const char *folder, MSG_DragEffect request);
extern MSG_DragEffect MSG_DragMessagesIntoFolderStatus(MSG_Pane* pane,
								const MSG_ViewIndex* indices, int32 numindices,
								MSG_FolderInfo *folder, MSG_DragEffect request);
extern MSG_DragEffect MSG_DragFoldersIntoStatus(MSG_Pane *folderPane,
								const MSG_ViewIndex *indices, int32 numIndices,
								MSG_FolderInfo *destFolder, MSG_DragEffect request);

extern int MSG_CopyMessagesInto(MSG_Pane* pane, const MSG_ViewIndex* indices,
								int32 numindices, const char *folder);
extern int MSG_MoveMessagesInto(MSG_Pane* pane, const MSG_ViewIndex* indices,
								int32 numindices, const char *folder);

extern int MSG_CopyMessagesIntoFolder(MSG_Pane* pane, const MSG_ViewIndex* indices,
								int32 numindices, MSG_FolderInfo *folder);
extern int MSG_MoveMessagesIntoFolder(MSG_Pane* pane, const MSG_ViewIndex* indices,
								int32 numindices, MSG_FolderInfo *folder);


extern int MSG_MoveFoldersInto (MSG_Pane *folderPane, const MSG_ViewIndex *indices,
							   int32 numIndices, MSG_FolderInfo *destFolder);

/* ===========================================================================
   								HOSTS
   ===========================================================================
 */

/* These operations on hosts are mostly used for the subscribe pane. */

/* Returns the number of known newshosts.  If the result pointer is not NULL,
   fills it in with the list of newshosts (providing up to resultsize entries.)
 */
extern int32 MSG_GetNewsHosts(MSG_Master* master,
							  MSG_NewsHost** result,
							  int32 resultsize);

/* Same as above for imap hosts */
extern int32 MSG_GetIMAPHosts(MSG_Master* master,
							  MSG_IMAPHost** result,
							  int32 resultsize);

/* Same as above for imap hosts that support subscription */
extern int32 MSG_GetSubscribingIMAPHosts(MSG_Master* master,
							  MSG_IMAPHost** result,
							  int32 resultsize);

/* Same as above for news hosts + imap hosts that support subscription */
extern int32 MSG_GetSubscribingHosts(MSG_Master* master,
							  MSG_Host** result,
							  int32 resultsize);

/* Get the host type */
extern XP_Bool MSG_IsNewsHost(MSG_Host* host);
extern XP_Bool MSG_IsIMAPHost(MSG_Host* host);

/* Returns a pointer to the associated MSG_Host object for this MSG_NewsHost */
extern MSG_Host* MSG_GetMSGHostFromNewsHost(MSG_NewsHost *newsHost);
/* Returns a pointer to the associated MSG_Host object for this MSG_IMAPHost */
extern MSG_Host* MSG_GetMSGHostFromIMAPHost(MSG_IMAPHost *imapHost);
/* Returns a pointer to the associated MSG_NewsHost object for this MSG_Host,
   if it is a MSG_NewsHost.  Otherwise, returns NULL. */
extern MSG_NewsHost* MSG_GetNewsHostFromMSGHost(MSG_Host *host);
/* Returns a pointer to the associated MSG_IMAPHost object for this MSG_Host,
   if it is a MSG_IMAPHost.  Otherwise, returns NULL. */
extern MSG_IMAPHost* MSG_GetIMAPHostFromMSGHost(MSG_Host *host);



/* this should be in msgnet.h, but winfe uses it - this is dangerous in 
   a multiple host world.
*/
extern XP_Bool MSG_GetMailServerIsIMAP4(MSG_Prefs* prefs);

/* Creates a new newshost.  If the given info matches an existing newshost,
   then returns that newshost. */

extern MSG_NewsHost* MSG_CreateNewsHost(MSG_Master* master,
										const char* hostname,
										XP_Bool isSecure,
										int32 port	/* If 0, use default */
										);


/* Gets the default newshost.  Could be NULL, if the user has managed to
   configure himself that way. */
extern MSG_NewsHost* MSG_GetDefaultNewsHost(MSG_Master* master);



/* Deletes a newshost.  This deletes everything known about this hosts -- the
   newsrc files, the databases, everything.  The user had better have confirmed
   this operation before making this call. */

extern int MSG_DeleteNewsHost(MSG_Master* master, MSG_NewsHost* host);


/* IMAP Hosts API's */
/* Creates a new imap host.  If the given info matches an existing imap host,
   then returns that imap host. */

extern MSG_IMAPHost* MSG_CreateIMAPHost(MSG_Master* master,
										const char* hostname,
										XP_Bool isSecure,
										const char *userName,
										XP_Bool checkNewMail,
										int	biffInterval,
										XP_Bool rememberPassword,
										XP_Bool usingSubscription,
										XP_Bool overrideNamespaces,
										const char *personalOnlineDir,
										const char *publicOnlineDir,
										const char *otherUsersOnlineDir
										);


/* Gets the default imap host.  Could be NULL, if the user has managed to
   configure himself that way. */
extern MSG_IMAPHost* MSG_GetDefaultIMAPHost(MSG_Master* master);


/* Deletes an imap host.  This deletes everything known about this hosts -- the
   preferences, the databases, everything.  The user had better have confirmed
   this operation before making this call. */

extern int MSG_DeleteIMAPHost(MSG_Master* master, MSG_IMAPHost* host);

/* Get info about a host. */

#ifdef SUBSCRIBE_USE_OLD_API	/* these are being obsoleted */
extern const char* MSG_GetNewsHostUIName(MSG_NewsHost* host);
extern const char* MSG_GetNewsHostName(MSG_NewsHost* host);
extern XP_Bool MSG_IsNewsHostSecure(MSG_NewsHost* host);
extern int32 MSG_GetNewsHostPort(MSG_NewsHost* host);
#endif /* SUBSCRIBE_USE_OLD_API */

extern const char* MSG_GetHostUIName(MSG_Host* host);
extern const char* MSG_GetHostName(MSG_Host* host);
extern XP_Bool MSG_IsHostSecure(MSG_Host* host);
extern int32 MSG_GetHostPort(MSG_Host* host);
extern void MSG_SetNewsHostPostingAllowed (MSG_NewsHost *host, XP_Bool allowed);
extern XP_Bool MSG_GetNewsHostPushAuth (MSG_NewsHost *host);
extern void MSG_SetNewsHostPushAuth (MSG_NewsHost *host, XP_Bool pushAuth);



/* ===========================================================================
   							SUBSCRIBE WINDOW
   ===========================================================================
 */


/* Set list of callback routines that the subscribe pane can call to the
   FE. */
extern int MSG_SubscribeSetCallbacks(MSG_Pane* subscribepane,
									 MSG_SubscribeCallbacks* callbacks,
									 void* callbackclosure);


/* The cancel button was hit.  Forget any subscriptions or unsubscriptions that
   had been requested.  (This call is usually quickly followed by a call to
   MSG_DestroyPane). */
extern int MSG_SubscribeCancel(MSG_Pane* subscribepane);

/* The OK button was hit.  Commit any subscriptions or unsubscriptions to the
   IMAP server, if there are any changes.  If the changes are on a news server,
   the commit will happen in the destructor, just like they used to (ack). 
   This needs to change for IMAP because we do network I/O. */
extern int MSG_SubscribeCommit(MSG_Pane* subscribepane);

/* Get which newshost we're currently viewing. */
#ifdef SUBSCRIBE_USE_OLD_API
extern MSG_NewsHost* MSG_SubscribeGetNewsHost(MSG_Pane* subscribepane);
#endif /* SUBSCRIBE_USE_OLD_API */

/* Get which news or imap host we're currently viewing. */
extern MSG_Host* MSG_SubscribeGetHost(MSG_Pane* subscribepane);

/* Set our view to the given newshost. */
#ifdef SUBSCRIBE_USE_OLD_API
extern int MSG_SubscribeSetNewsHost(MSG_Pane* subscribepane,
									MSG_NewsHost* host);
#endif /* SUBSCRIBE_USE_OLD_API */

/* Set our view to the given news or imap host. */
extern int MSG_SubscribeSetHost(MSG_Pane* subscribepane,
									MSG_Host* host);

/* Gets the mode of the pane. */
extern MSG_SubscribeMode MSG_SubscribeGetMode(MSG_Pane* subscribepane);

/* Sets the mode of the pane */
extern int MSG_SubscribeSetMode(MSG_Pane* subscribepane,
								MSG_SubscribeMode mode);


/* Finds the first newsgroup whose name matches the given string.  Expansions
   will automatically be done as necessary.  Returns the index that matches;
   the FE should then scroll to and select that index.   The pane must be
   in the "MSG_SubscribeAll" mode. */
extern MSG_ViewIndex MSG_SubscribeFindFirst(MSG_Pane* subscribepane,
											const char* str);


/* Finds all the newsgroups that have the given string as a substring, and
   changes the view to show only those groups.  The pane must be in the
   "MSG_SubscribeSearch" mode. */
extern int MSG_SubscribeFindAll(MSG_Pane* subscribepane, const char* str);


/* Get information to display on these lines in the subscription pane
   outline list.  Returns TRUE on success, FALSE if the given
   line doesn't exist or if there are less than numlines worth of data to get.
   The given data pointer must point to an array of at least numlines
   elements. */

extern XP_Bool MSG_GetGroupNameLineByIndex(MSG_Pane* subscribepane,
										   MSG_ViewIndex firstline,
										   int32 numlines,
										   MSG_GroupNameLine* data);



/* ===========================================================================
   								OFFLINE NEWS
   ===========================================================================
 */

typedef enum MSG_PurgeByPreferences
{
	MSG_PurgeNone,
	MSG_PurgeByAge,
	MSG_PurgeByNumHeaders
} MSG_PurgeByPreferences;


extern int MSG_SetOfflineRetrievalInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool		useDefaults,
	XP_Bool		byReadness,
	XP_Bool		unreadOnly,
	XP_Bool		byDate,
	int32		daysOld);

extern int MSG_SetArticlePurgingInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool			useDefaults,
	MSG_PurgeByPreferences	purgeBy,
	int32			daysToKeep);

extern int MSG_SetHeaderPurgingInfo(
	MSG_FolderInfo *newsGroup, 
	XP_Bool	useDefaults,
	MSG_PurgeByPreferences	purgeBy,
	XP_Bool	unreadOnly,
	int32 daysToKeep,
	int32 numHeadersToKeep
);

extern void MSG_GetOfflineRetrievalInfo(
	MSG_FolderInfo *newsGroup,
	XP_Bool		*pUseDefaults,
	XP_Bool		*pByReadness,
	XP_Bool		*pUnreadOnly,
	XP_Bool		*pByDate,
	int32		*pDaysOld);

extern void MSG_GetArticlePurgingInfo(MSG_FolderInfo *newsGroup,
	XP_Bool			*pUseDefaults,
	MSG_PurgeByPreferences	*pPurgeBy,
	int32 *pDaysToKeep);

extern void MSG_GetHeaderPurgingInfo(MSG_FolderInfo *newsGroup,
	XP_Bool *pUseDefaults,
	MSG_PurgeByPreferences	*pPurgeBy,
	XP_Bool	*pUnreadOnly,
	int32 *pDaysToKeep,
	int32 *pNumHeadersToKeep);



/* run the url to download mail and news articles for offline use
  for those folders so configured.
*/
extern int MSG_DownloadForOffline(MSG_Master *master, MSG_Pane *pane);
extern int MSG_GoOffline(MSG_Master *master, MSG_Pane *pane, XP_Bool downloadDiscussions, XP_Bool getNewMail, XP_Bool sendOutbox);
extern int MSG_DownloadFolderForOffline(MSG_Master *master, MSG_Pane *pane, MSG_FolderInfo *folder);


/* ===========================================================================
   									QUERIES
   ===========================================================================
 */

/* Get the filename for the folder on the given line.  This string is perfect
   for passing to calls to MSG_MoveMessagesInto() and friends.  String is 
   allocated with XP_STRDUP so client needs to free. This should only be used
   for mail folders, it appears from the code.
*/
extern const char* MSG_GetFolderName(MSG_Pane* folderpane, MSG_ViewIndex line);


/* return the full folder name from a folder id. String is not allocated so caller
   should duplicate it. For mail boxes, this returns the full folder path. This is
   currently used for filters to remember a full folder path.
 */
extern const char *MSG_GetFolderNameFromID(MSG_FolderInfo *);


/* Get how many lines are in the list for this folderpane or threadpane or
   subscribepane. */
extern int32 MSG_GetNumLines(MSG_Pane* pane);


/* Convert a MSG_FolderInfo* into a MSG_ViewIndex.  Returns MSG_VIEWINDEXNONE if
   the given MSG_FolderInfo* is not being displayed anywhere. */

extern MSG_ViewIndex MSG_GetFolderIndex(MSG_Pane* folderpane,
									   MSG_FolderInfo* info);


/* This routine should replace the above routine when people port over to it.
	If expand is TRUE, we will expand the folderPane enough to
	expose the passed in folder info. Otherwise, if the folder is collapsed,
	we return MSG_ViewIndexNone.
*/
extern MSG_ViewIndex MSG_GetFolderIndexForInfo(MSG_Pane *folderpane, 
												MSG_FolderInfo *info,
												XP_Bool expand);

/* Converts a MSG_ViewIndex into a MSG_FolderInfo*. */

extern MSG_FolderInfo* MSG_GetFolderInfo(MSG_Pane* folderpane,
										 MSG_ViewIndex index);

extern MSG_FolderInfo* MSG_GetFolderInfoFromURL(MSG_Master* master, const char *url);

/* returns folder info of host owning this folder*/
MSG_FolderInfo* GetHostFolderInfo(MSG_FolderInfo* info); 

extern MSG_FolderInfo* MSG_GetThreadFolder(MSG_Pane *threadpane);

extern MSG_FolderInfo* MSG_GetCategoryContainerForCategory(MSG_FolderInfo *category);

#ifdef SUBSCRIBE_USE_OLD_API
/* Finds the newshost that contains the given folder.  If the given folder is
   not a newsgroup, returns NULL. */
extern MSG_NewsHost* MSG_GetNewsHostForFolder(MSG_FolderInfo* folder);

/* Converts a MSG_ViewIndex into a MSG_NewsHost */
extern MSG_NewsHost *MSG_GetNewsHostFromIndex (MSG_Pane *folderPane,
											   MSG_ViewIndex index);
#endif	/* SUBSCRIBE_USE_OLD_API */

/* Finds the host that contains the given folder.  If the given folder is
   not a newsgroup or IMAP folder, returns NULL. */
extern MSG_Host *MSG_GetHostForFolder(MSG_FolderInfo* folder);

/* Converts a MSG_ViewIndex into a MSG_Host */
extern MSG_Host *MSG_GetHostFromIndex (MSG_Pane *folderPane,
									   MSG_ViewIndex index);

/* Gets the MSG_FolderInfo object associated with a given MSG_Host. */
extern MSG_FolderInfo *MSG_GetFolderInfoForHost(MSG_Host *host);

/* given a url containing a message id, fill in the message line info for the msg.
	For news urls, the news group will need to be open!
 */
extern int MSG_GetMessageLineForURL(MSG_Master* master, const char *url, MSG_MessageLine *msgLine);

/* This routine should replace the MSG_GetMessageIndex when people port over to
	it. If expand is TRUE, we will expand the folderPane enough to
	expose the passed in folder info. Otherwise, if the folder is collapsed,
	we return MSG_ViewIndexNone.
*/
extern MSG_ViewIndex MSG_GetMessageIndexForKey(MSG_Pane* threadpane, 
											   MessageKey key,
											   XP_Bool expand);

/* Look up the MessageKey based on the message ID */

extern int MSG_GetKeyFromMessageId (MSG_Pane *message_pane, 
                             const char *message_id,
                             MessageKey *outKey);

/* Converts a MSG_ViewIndex into a MessageKey. */

extern MessageKey MSG_GetMessageKey(MSG_Pane* threadpane, MSG_ViewIndex index);


/* Getting a message key from the undo manager for dsiplay */

extern XP_Bool MSG_GetUndoMessageKey (MSG_Pane* pane, 
									  MSG_FolderInfo** pFolderInfo,
									  MessageKey* pKey);

/* Get current undo operation status */
extern UndoStatus MSG_GetUndoStatus (MSG_Pane* pane);

/* Get Undo Manager State */
extern UndoMgrState MSG_GetUndoState(MSG_Pane* pane);

/* Given a MSG_FolderInfo*, returns the number of children folders.  If the
   result pointer is not NULL, fills it in with the list of children (providing
   up to resultsize entries.)  If the given MSG_FolderInfo* is NULL, then
   returns the list of top-level folders. */

extern int32 MSG_GetFolderChildren(MSG_Master* master,
								   MSG_FolderInfo* folder,
								   MSG_FolderInfo** result,
								   int32 resultsize);

/* Returns the container of all local (off-line) mail folders. */

extern MSG_FolderInfo* MSG_GetLocalMailTree(MSG_Master* master); 

/* Returns the number of mail folders (both local and IMAP).  If this number is
   greater than zero, an array of pointers to the MSG_FolderInfos is allocated
   and returned in result.  The caller is responsible for freeing the array.
   This function is not recursive, it only returns the top level mail folders. */
extern int32 MSG_ListMailFolders(MSG_Master *master, MSG_FolderInfo ***result);

/* Given a flag or set of flags, returns the number of folders that have
   that flag set.  If the result pointer is not NULL, fills it in with the
   list of folders (providing up to resultsize entries).  */

extern int32 MSG_GetFoldersWithFlag(MSG_Master* master,
									uint32 flags,
									MSG_FolderInfo** result,
									int32 resultsize);




/* Returns what folder a threadpane is viewing (NULL if not viewing
   anything.) */

extern MSG_FolderInfo* MSG_GetCurFolder(MSG_Pane* threadpane);

/* Call this from the new Create Folder UI with the dropdown list.
   The MSG_Command will go away when everyone uses this.
   This should not be called anymore - keeping it for a smooth transition to
   MSG_CreateMailFolderWithPane  (below) */
extern int MSG_CreateMailFolder (MSG_Master *master, MSG_FolderInfo *parent,
								 const char *childName);

/* Call this from the new Create Folder UI with the dropdown list.
   The MSG_Command will go away when everyone uses this.
   Call with invokingPane set to the pane with which to run any IMAP
   Create-Folder URLs.  (probably the pane from which it was invoked,
   or a progress pane) */
extern int MSG_CreateMailFolderWithPane (MSG_Pane *invokingPane, MSG_Master *master,
										 MSG_FolderInfo *parent, const char *childName);



/* FEs should call this to determine what folder we should suggest as the parent folder,
   when creating a new folder.
   current is the MSG_FolderInfo of the currently selected folder or host. 
   Returns the MSG_FolderInfo for the suggested parent, given the currently selected folder or host. 
   Returns NULL if current is NULL. 
*/ 
extern MSG_FolderInfo *MSG_SuggestNewFolderParent(MSG_FolderInfo *current); 

/* Call this from the new folder properties UI */
extern int MSG_RenameMailFolder (MSG_Pane *folderPane, MSG_FolderInfo *folder,
								 const char *newName);

/* Returns what folder and message a messagepane is currently displaying.
   (Sets the folder to NULL and the key to MSG_MESSAGEKEYNONE if the
   messagepane is currently displaying nothing.) */

extern void MSG_GetCurMessage(MSG_Pane* messagepane,
							  MSG_FolderInfo** folder,
							  MessageKey* key, MSG_ViewIndex *index);


/* Returns what attachments are being viewed in the messagepane, and whether
   we're certain it's the entire list.  (If the data is still being downloaded,
   then there could still be attachments that we don't know about.)

   The returned data must be free'd using MSG_FreeAttachmentList(). */

extern int MSG_GetViewedAttachments(MSG_Pane* messagepane,
									MSG_AttachmentData** data,
									XP_Bool* iscomplete);


/* Frees the attachemnt data returned by MSG_GetViewedAttachments(). */

extern void MSG_FreeAttachmentList(MSG_Pane* messagepane,
								   MSG_AttachmentData* data);




/* These return NULL if they fail. Caller must call NET_FreeURLStruct */ 
extern URL_Struct*  MSG_ConstructUrlForPane(MSG_Pane *pane);
extern URL_Struct*  MSG_ConstructUrlForMessage(MSG_Pane *pane, MessageKey key);
extern URL_Struct*	MSG_ConstructUrlForFolder(MSG_Pane *pane, MSG_FolderInfo *folder);
/* Returns whether the user has asked to rot13 messages displayed in
   this pane.  (Used by libmime.) */
extern XP_Bool MSG_ShouldRot13Message(MSG_Pane* messagepane);


/* ===========================================================================
   								BIFF
   ===========================================================================
 */

/* biff is the unixy name for "Check for New Mail".  It comes from the unix
   command of the same name; the author of that command named it after his dog,
   who would bark at him whenever he had new mail...  */

/* The different biff states we can be in:  */

typedef enum
{
  MSG_BIFF_NewMail,				/* User has new mail waiting. */
  MSG_BIFF_NoMail,				/* No new mail is waiting. */
  MSG_BIFF_Unknown				/* We dunno whether there is new mail. */
} MSG_BIFF_STATE;


/* Register and unregister biff callback functions */
typedef void (*MSG_BIFF_CALLBACK)(MSG_BIFF_STATE oldState, MSG_BIFF_STATE newState);

extern void MSG_RegisterBiffCallback( MSG_BIFF_CALLBACK cb );

extern void MSG_UnregisterBiffCallback();

/* Get and set the current biff state */

extern MSG_BIFF_STATE MSG_GetBiffState();

extern void MSG_SetBiffStateAndUpdateFE(MSG_BIFF_STATE newState);


/* Set the preference of how often to run biff.  If zero is passed in, then
   never check. */

extern void MSG_SetBiffInterval(int32 seconds);



#ifdef XP_UNIX
/* Set the file to stat, instead of using pop3.  This is for the Unix movemail
   nonsense.  If the filename is NULL (the default), then use pop3. */
extern void MSG_SetBiffStatFile(const char* filename);
#endif



/* Initialize the biff context.  Note that biff contexts exist entirely
   independent of mail contexts; it's up to the FE to decide what order they
   get created and stuff. */

extern int MSG_BiffInit(MWContext* context, MSG_Prefs* prefs);


/* The biff context is about to go away.  The FE must call this first to
   clean up. */

extern int MSG_BiffCleanupContext(MWContext* context);


/* Causes a biff check to occur immediately.  This gets caused
   automatically by MSG_SetBiffInterval or whenever libmsg gets new mail. */

extern void MSG_BiffCheckNow(MWContext* context);


/* Tell the FE to render in all the right places this latest knowledge as to
   whether we have new mail waiting. */
extern void FE_UpdateBiff(MSG_BIFF_STATE state);



/* ===========================================================================
   						OTHER INTERFACES
   ===========================================================================
 */

/* Certain URLs must always be displayed in certain types of windows:
   for example, all "mailbox:" URLs must go in the mail window, and
   all "http:" URLs must go in a browser window.  These predicates
   look at the address and say which window type is required.
 */
extern XP_Bool MSG_RequiresMailWindow (const char *url);
extern XP_Bool MSG_RequiresNewsWindow (const char *url);
extern XP_Bool MSG_RequiresBrowserWindow (const char *url);
extern XP_Bool MSG_RequiresComposeWindow (const char *url);

/* If this URL requires a particular kind of window, and this is not
   that kind of window, then we need to find or create one.
 */
extern XP_Bool MSG_NewWindowRequired (MWContext *context, const char *url);

/* If this URL requires a particular kind of window, and this is not
   that kind of window, then we need to find or create one.
   This routine takes a URL_Struct, which allows it to be smarter than
   the above routine.
 */
extern XP_Bool MSG_NewWindowRequiredForURL (MWContext *context, URL_Struct *urlStruct);

/* If we're in a mail window, and clicking on a link which will itself
   require a mail window, then don't allow this to show up in a different
   window - since there can be only one mail window.
 */
extern XP_Bool MSG_NewWindowProhibited (MWContext *context, const char *url);


/* When the front end loads a url, it needs to know what kind of pane the url
	should be loaded into.
*/
extern MSG_PaneType MSG_PaneTypeForURL(const char *url);

/* Returns the number of bytes available on the disk where the given directory
   is - this is so we know whether there is room to incorporate new mail.  */
extern uint32 FE_DiskSpaceAvailable (MWContext* context, const char* dir);


/* ===========================================================================
   							SECURE MAIL
   ===========================================================================
 */

extern MSG_SecurityLevel MSG_GetMessageSecurityLevel(MSG_Pane *pPane);
extern const XP_List *MSG_GetRecipientsWithNoCerts(MSG_Pane *pPane);
extern XP_Bool MSG_ShouldEncryptMessage(MSG_Pane *pPane);
extern XP_Bool MSG_ShouldSignMessage(MSG_Pane *pPane);


/* FEs call this any time the set of recipients on the compose window
   changes.  They should make the state (and sensitivity) of the "sign"
   and "encrypt" checkboxes, and the state of the "security" button,
   correspond to the returned boolean values.

   Maybe this function doesn't really belong here, but it's as good a
   place as any...
  */
/* (This is maybe not the most appropriate header file for this proto.) */
extern void SEC_GetMessageCryptoViability(const char *from,
										  const char *reply_to,
										  const char *to,
										  const char *cc,
										  const char *bcc,
										  const char *newsgroups,
										  const char *followup_to,
										  XP_Bool *signing_possible_return,
										  XP_Bool *encryption_possible_return);


/* Returns TRUE if the user has selected the preference that says that new
   mail messages should be encrypted by default. 
 */
extern XP_Bool MSG_GetMailEncryptionPreference(void);

/* Returns TRUE if the user has selected the preference that says that new
   mail messages should be cryptographically signed by default. 
 */
extern XP_Bool MSG_GetMailSigningPreference(void);

/* Returns TRUE if the user has selected the preference that says that new
   news messages should be cryptographically signed by default. 
 */
extern XP_Bool MSG_GetNewsSigningPreference(void);




/* ===========================================================================
   						COMPOSE WINDOW
   ===========================================================================
 */

/* This is how the `mailto' parser asks the message library to create a
   message composition window.  That window has its own context.  The
   `old_context' arg is the context from which the mailto: URL was loaded.
   It may be NULL.

   Any of the fields may be 0 or "".  Some of them (From, BCC, Organization,
   etc) will be given default values if none is provided.

   This returns the new composition pane.
 */
extern MSG_Pane* MSG_ComposeMessage(MWContext *old_context,
									const char *from,
									const char *reply_to,
									const char *to,
									const char *cc,
									const char *bcc,
									const char *fcc,
									const char *newsgroups,
									const char *followup_to,
									const char *organization,
									const char *subject,
									const char *references,
									const char *other_random_headers,
									const char *priority,
									const char *attachment,
									const char *newspost_url,
									const char *body,
									XP_Bool encrypt_p,
									XP_Bool sign_p,
									XP_Bool force_plain_text,
									const char* html_part);

extern MSG_CompositionFields* MSG_CreateCompositionFields(
									const char *from,
									const char *reply_to,
									const char *to,
									const char *cc,
									const char *bcc,
									const char *fcc,
									const char *newsgroups,
									const char *followup_to,
									const char *organization,
									const char *subject,
									const char *references,
									const char *other_random_headers,
									const char *priority,
									const char *attachment,
									const char *newspost_url,
									XP_Bool encrypt_p,
									XP_Bool sign_p);

extern void MSG_DestroyCompositionFields(MSG_CompositionFields *fields);

/* Tell the FE that something has changed in the composition (like, we
   finished quoting or something) so that it needs to update the status of
   toolbars and stuff.  This call should go away.  ###tw */
extern void FE_UpdateCompToolbar(MSG_Pane* comppane);


/* Tell the FE that we're all done with the given context (which was associated
   with a composition pane that we're destroying).  Presumably, the FE will
   then destroy the context. */
extern void FE_DestroyMailCompositionContext(MWContext* context);



/* Determines whether this is a mail composition that ought to have a "quote
   message" operation done at startup.  If so, the FE must call
   MSG_QuoteMessage and handle it, and when done call MSG_SetInitialState(). */

extern XP_Bool MSG_ShouldAutoQuote(MSG_Pane* comppane);


/* Get and set the various things that a user may have typed.  These are all
   initialized automatically by the constructor from the MSG_CompositionFields*
   object.  The FE needs to done setting everything before doing sanity checks
   or sending the message.

   MSG_SetCompHeader should not be made with the header
   MSG_ATTACHMENTS_HEADER_MASK; instead, call MSG_SetAttachmentList().
   However, the FE may call MSG_GetCompHeader() with
   MSG_ATTACHMENTS_HEADER_MASK to find the summary line to display. */

extern const char* MSG_GetCompHeader(MSG_Pane* comppane,
									 MSG_HEADER_SET header);
extern int MSG_SetCompHeader(MSG_Pane* comppane,
							 MSG_HEADER_SET header,
							 const char* value);
extern const char* MSG_GetCompFieldsHeader(MSG_CompositionFields *fields,
										   MSG_HEADER_SET header);
extern int MSG_SetCompFieldsHeader(MSG_CompositionFields *fields,
								   MSG_HEADER_SET header,
								   const char* value);
extern void MSG_SetCompFieldsReceiptType(MSG_CompositionFields *fields,
										int32 type);
extern int32 MSG_GetCompFieldsReceiptType(MSG_CompositionFields *fields);

extern XP_Bool MSG_GetCompBoolHeader(MSG_Pane* comppane,
									 MSG_BOOL_HEADER_SET header);
extern int MSG_SetCompBoolHeader(MSG_Pane* comppane,
								 MSG_BOOL_HEADER_SET header,
								 XP_Bool value);
extern XP_Bool MSG_GetCompFieldsBoolHeader(MSG_CompositionFields *fields,
										   MSG_BOOL_HEADER_SET header);
extern int MSG_SetCompFieldsBoolHeader(MSG_CompositionFields *fields,
									   MSG_BOOL_HEADER_SET header,
									   XP_Bool value);

/* Get whether we should force plain-text composition by default. */
extern XP_Bool MSG_GetForcePlainText(MSG_CompositionFields* fields);


extern const char* MSG_GetCompBody(MSG_Pane* comppane);
extern int MSG_SetCompBody(MSG_Pane* comppane, const char* body);

/* These following four functions are provided to make implementing the
   grid-based compose window a bit easier.  A new type definition
   (MSG_HeaderEntry) is introduced which is used to store individual 
   address entries. -- jevering  */

typedef struct 
{
  MSG_HEADER_SET header_type;
  char * header_value;
} MSG_HeaderEntry;

/* MSG_ExplodeHeaderField() parses a single header entry and returns
   a list of MSG_HeaderEntry structures.  The return value is the
   number of items in the return list or -1 for an error.  The return 
   list as well as its contents should be freed by the caller. */
    
extern int  MSG_ExplodeHeaderField( MSG_HEADER_SET msg_header, 
	                                const char * field,
	                                MSG_HeaderEntry **return_list);

/* This function retrieve the contents of the current compose window
   headers and parses them into a MSG_HeaderEntry list with one
   address per line.  Call this function when constructing the compose
   window in order to initialize the grid-widget.  This function returns
   the number of items in the list or -1 indicating an error.  The
   return list should be freed by the caller. */

extern int MSG_RetrieveStandardHeaders(MSG_Pane * pane, 
                                    MSG_HeaderEntry ** return_list);

/* This function accepts a list of single-entry address items and
   condenses it down to a list of comma separated addressed.  The 
   order of occurance is maintained on a MSG_HEADER_SET basis.
   The return value indicated the size of the list. A return value
   of -1 indicates that an error has occured.  The returned list
   as well as its contents should be freed by the caller. */

extern int MSG_CompressHeaderEntries(MSG_HeaderEntry * in_list, 
                                    int list_size,
                                    MSG_HeaderEntry ** return_list);

/* call this function with the list returned by MSG_CompressHeaderEntries().
   pane - message pane, count - number of entries in list.
   The previous headers are cleared and the contents of the new list
   is used.  The memory is freed as well. */

extern void MSG_SetHeaderEntries(MSG_Pane * pane,
                                 MSG_HeaderEntry * in_list,
                                 int count);

/* Clears all compose window headers (except the attachment list). */

extern void MSG_ClearComposeHeaders(MSG_Pane * pane);

/* Begin a "quote message" operation.  This is complicated, because this is an
   asynchronous operation.  (Getting the message to quote can be a network
   operation).  So, the FE must provide a callback routine that inserts the
   quoted data, piecemeal.  The callback will get called with a NULL string
   when things are all done (or if things failed or were cancelled.)  The
   callback routine should return a negative value on error.

   Actually, if we're an HTML composition, we call the editor functions
   directly, and ignore the given func().  Pretty rude, huh.
   */
#ifdef XP_OS2
typedef int (*PNSQMFN)(void *, const char *);

extern void MSG_QuoteMessage(MSG_Pane* comppane,
							 PNSQMFN func,
							 void* closure);
#else
extern void MSG_QuoteMessage(MSG_Pane* comppane,
							 int (*func)(void* closure, const char* data),
							 void* closure);
#endif

/* The FE is about to call MSG_SendComposition to cause things to be sent, but
   first, it wants to make sure that the user isn't trying to send something
   blatently stupid (like, no subject, or no body, or no "To:", or whatever).
   So, there's various sanity checks for it to make, and the FE can then take
   appropriate actions if the check failed.  Each sanity check returns an
   error code describing the problem.  The FE can pop up the string for that
   error code in a FE_Confirm() call.  If FE_Confirm() returns TRUE, then the
   user doesn't care and wants to send the message anyway, so the FE can just
   call MSG_SanityCheck() again and pass the given errorcode as the skippast
   parameter.  libmsg will skip that sanity check, and all the previous ones,
   and confirm that the other things work.

   It's up to the FE how to handle each error code; the FE_Confirm() method
   is just the suggested default.  In particular, the FE should prompt the
   user for the subject if we return MK_MSG_MISSING_SUBJECT.

   The current sanity checks are:

   MK_MSG_MISSING_SUBJECT:  There is no subject line.
   MK_MSG_EMPTY_MESSAGE:	There is no real body, and no attachments.
   MK_MSG_DOUBLE_INCLUDE:	The original message was both quoted and attached.
   MK_MSG_MIXED_SECURITY:	Some recipients have certificates; some not.

   */

extern int MSG_SanityCheck(MSG_Pane* comppane, int skippast);

/* Get the URL associated with this context (the "X-Url" field.) */
extern const char* MSG_GetAssociatedURL(MSG_Pane* comppane);


/* This is completely foul, but the FE needs to call this from within
   FE_AllConnectionsComplete() when the context->type is
   MWContextMessageComposition.
 */
extern void MSG_MailCompositionAllConnectionsComplete (MSG_Pane* pane);


/*  */


/* Bring up the dialog box that presents the user with the list of domains that
   have been marked for HTML, and let the user edit them. */

extern int MSG_DisplayHTMLDomainsDialog(MWContext* context);


/* Returns whether the given folderinfo represents a newsgroup where
   HTML postings are OK.  This is to be used in the property dialog
   for that newsgroup.  This call should only be done on newsgroups. */

extern XP_Bool MSG_IsHTMLOK(MSG_Master* master, MSG_FolderInfo* group);

/* Sets whether the given newsgroup can have HTML.  This can potentially
   pop up a confirmation window, so we ask for a MWContext* to use for that
   (yick).  The folderinfo provided must represent a newsgroup.  This is to
   be used in the property dialog for that newsgroup. */

extern int MSG_SetIsHTMLOK(MSG_Master* master, MSG_FolderInfo* group,
						   MWContext* context, XP_Bool value);



/* Utility function that prefixes each line with "> " (for Paste As Quote). */
extern char *MSG_ConvertToQuotation (const char *string);

/* Paste the given plaintext into the running HTML editor as a quotation.
   This can only be used if the given composition pane is in HTML mode. */
extern int MSG_PastePlaintextQuotation(MSG_Pane* comppane, const char* string);


/* The user has just finished editing a given header (the field has lost focus
   or something like that.)  Find out if some magic change needs to be made
   (e.g., the user typed a nickname from the addressbook and we want to replace
   it with the specified address.)  If so, a new value to be displayed is
   returned.  The returned string, if any, must be freed using XP_FREE().  This
   should be called before calling MSG_SetCompHeader(). */

extern char* MSG_UpdateHeaderContents(MSG_Pane* comppane,
									  MSG_HEADER_SET header,
									  const char* value);


/* If the sanity check return MK_MSG_MIXED_SECURITY, then the user may choose
   to remove non-secure people from the list.  The FE can do so by calling
   this routine.  It will then have to call MSG_GetCompHeader a few times
   to get and redisplay the new values for the To, Cc, and Bcc fields. */

extern void MSG_RemoveNoCertRecipients(MSG_Pane* comppane);

/* Inform the backend what is to be attached.  The MSG_AttachmentData structure
   is defined in mime.h.  The "list" parameter is an array of these fields,
   terminated by one which has a NULL url field.  In each entry, all that you
   have to fill in is "url" and probably "desired_type"; NULL is generally fine
   for all the other fields (but you can fill them in too).  See mime.h for all
   the spiffy details.

   Note that this call copies (sigh) all the data into libmsg; it is up to the
   caller to free any data it had to allocate to create the MSG_AttachmentData
   structure. */

extern int MSG_SetAttachmentList(MSG_Pane* comppane,
								  struct MSG_AttachmentData* list);


/* Get what msglib's idea of what the current attachments are.  DO NOT MODIFY
   THE DATA RETURNED; you can look, but don't touch.  If you want to change
   the data, you have to copy out the whole structure somewhere, make your
   changes, call MSG_SetAttachmentList() with your new data, and then free
   your copy.  If there are no attachments, this routine always returns
   NULL. */
extern const struct MSG_AttachmentData*
MSG_GetAttachmentList(MSG_Pane* comppane);




/* The FE should call this in response to a `Show Interesting Headers' command,
   and when first initializing the composition window..  It returns the set of
   headers that ought to be displayed (and checked in the "view" menu.)
 */
extern MSG_HEADER_SET MSG_GetInterestingHeaders(MSG_Pane* comppane);



/* This function creates a new, blank mail message composition window.  It
   causes calls to FE_CreateCompositionPane, which should drive the creation
   of the window.
 */

MSG_Pane* MSG_Mail(MWContext *old_context);

/* Convenience function to start composing a mail message from a web
   browser window - the currently-loaded document will be the default
   attachment should the user choose to attach it.  The context may
   be of any type, or NULL.  Returns the new message composition pane.
 
   This is going away, you should call MSG_MailDocumentURL with NULL
   for the second argument.
*/
extern MSG_Pane* MSG_MailDocument(MWContext *context);
/* 
   Another version of MSG_MailDocument where pAttachmentURL explicitly gives the URL to attach.
   MSG_MailDocument() is still there for backwards compatability.
 */
extern MSG_Pane* MSG_MailDocumentURL(MWContext *context,const char *pAttachmentURL);

/* These routines were in mime.h, but defined in libmsg...*/

/* Given a string which contains a list of RFC822 addresses, parses it into
   their component names and mailboxes.

   The returned value is the number of addresses, or a negative error code;
   the names and addresses are returned into the provided pointers as
   consecutive null-terminated strings.  It is up to the caller to free them.
   Note that some of the strings may be zero-length.

   Either of the provided pointers may be NULL if the caller is not interested
   in those components.
 */
extern int MSG_ParseRFC822Addresses (const char *line,
									 char **names, char **addresses);


/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `mailbox' portions.
 */
extern char *MSG_ExtractRFC822AddressMailboxes (const char *line);


/* Given a string which contains a list of RFC822 addresses, returns a
   comma-seperated list of just the `user name' portions.  If any of
   the addresses doesn't have a name, then the mailbox is used instead.
 */
extern char *MSG_ExtractRFC822AddressNames (const char *line);

/* Like MSG_ExtractRFC822AddressNames(), but only returns the first name
   in the list, if there is more than one. 
 */
extern char *MSG_ExtractRFC822AddressName (const char *line);

/* Given a string which contains a list of RFC822 addresses, returns a new
   string with the same data, but inserts missing commas, parses and reformats
   it, and wraps long lines with newline-tab.
 */
extern char * MSG_ReformatRFC822Addresses (const char *line);

/* Returns a copy of ADDRS which may have had some addresses removed.
   Addresses are removed if they are already in either ADDRS or OTHER_ADDRS.
   (If OTHER_ADDRS contain addresses which are not in ADDRS, they are not
   added.  That argument is for passing in addresses that were already
   mentioned in other header fields.)

   Addresses are considered to be the same if they contain the same mailbox
   part (case-insensitive.)  Real names and other comments are not compared.
 */
extern char *MSG_RemoveDuplicateAddresses (const char *addrs,
										   const char *other_addrs);


/* Given an e-mail address and a person's name, cons them together into a
   single string of the form "name <address>", doing all the necessary quoting.
   A new string is returned, which you must free when you're done with it.
 */
extern char *MSG_MakeFullAddress (const char* name, const char* addr);

/* MSG_ParseRFC822Addresses returns quoted parsable addresses
   This function removes the quoting if you want to show the
   names to users. e.g. summary file, address book
 */
extern int MSG_UnquotePhraseOrAddr (char *line, char** lineout);


/* Returns the address book context, creating it if necessary.  A mail pane is
   passed in, on the unlikely chance that it might be useful for the FE.  If
   "viewnow" is TRUE, then present the address book window to the user;
   otherwise, don't (unless it is already visible).*/

extern MWContext* FE_GetAddressBookContext(MSG_Pane* pane, XP_Bool viewnow);

/* Notify the address book panes that the list of directory servers has changed 
	This is only called from the address book. */

extern int MSG_NotifyChangeDirectoryServers();

XP_END_PROTOS


#endif /* _MSGCOM_H_ */
