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
   msg.h --- internal defs for the msg library
 */
#ifndef _MSG_H_
#define _MSG_H_

#include "xp.h"
#include "msgcom.h"
#include "msgnet.h"
#include "xpgetstr.h"

#ifdef XP_CPLUSPLUS
class MessageDBView;
class MSG_SendPart;
#endif

#define MANGLE_INTERNAL_ENVELOPE_LINES /* We always need to do this, for now */
#undef FIXED_SEPARATORS				   /* this doesn't work yet */
#define EMIT_CONTENT_LENGTH			   /* Experimental; and anyway, we only
										  emit it, we don't parse it, so this
										  is only the first step. */

/* hack - this string gets appended to the beginning of an attachment field
   during a forward quoted operation */

#define MSG_FORWARD_COOKIE  "$forward_quoted$"


/* The PRINTF macro is for debugging messages of unusual events (summary */
/* files out of date or invalid or the like.  It's so that as I use the mail */
/* to actually read my e-mail, I can look at the shell output whenever */
/* something unusual happens so I can get some clues as to what's going on. */
/* Please don't remove any PRINTF calls you see, and be sparing of adding */
/* any additional ones.  Thanks.  - Terry */

#ifdef DEBUG
#define PRINTF(msg) PR_LogPrint msg
#else
#define PRINTF(msg)
#endif


/* The Netscape-specific header fields that we use for storing our
   various bits of state in mail folders.
 */
#define X_MOZILLA_STATUS           "X-Mozilla-Status"
#define X_MOZILLA_STATUS_FORMAT    X_MOZILLA_STATUS ": %04.4x"
#define X_MOZILLA_STATUS_LEN      /*1234567890123456*/      16

#define X_MOZILLA_STATUS2		   "X-Mozilla-Status2"
#define X_MOZILLA_STATUS2_FORMAT   X_MOZILLA_STATUS2 ": %08.8x"
#define X_MOZILLA_STATUS2_LEN	  /*12345678901234567*/		17

#define X_MOZILLA_DRAFT_INFO	   "X-Mozilla-Draft-Info"
#define X_MOZILLA_DRAFT_INFO_LEN  /*12345678901234567890*/  20

#define X_MOZILLA_NEWSHOST         "X-Mozilla-News-Host"
#define X_MOZILLA_NEWSHOST_LEN    /*1234567890123456789*/   19

#define X_UIDL                     "X-UIDL"
#define X_UIDL_LEN                /*123456*/                 6

#define CONTENT_LENGTH             "Content-Length"
#define CONTENT_LENGTH_LEN        /*12345678901234*/        14

/* Provide a common means of detecting empty lines in a message. i.e. to detect the end of headers among other things...*/
#define EMPTY_MESSAGE_LINE(buf) (buf[0] == CR || buf[0] == LF || buf[0] == '\0')

typedef PRInt32 MsgChangeCookie;	/* used to unregister change notification */

/* The three ways the list of newsgroups can be pruned.
 */
typedef enum
{
  MSG_ShowAll,
  MSG_ShowSubscribed,
  MSG_ShowSubscribedWithArticles
} MSG_NEWSGROUP_DISPLAY_STYLE;

/* The three ways to deliver a message.
 */
typedef enum
{
  MSG_DeliverNow,
  MSG_QueueForLater,
  MSG_SaveAs,
  MSG_SaveAsDraft,
  MSG_SaveAsTemplate
} MSG_Deliver_Mode;


/* A little enum for things we'd like to learn lazily.
 * e.g. displaying recipients for this pane? Yes we are,
 *      no we're not, haven't figured it out yet
 */
typedef enum
{
	msg_No,
	msg_Yes,
	msg_DontKnow
} msg_YesNoDontKnow;

/* The MSG_REPLY_TYPE shares the same space as MSG_CommandType, to avoid
   possible weird errors, but is restricted to the `composition' commands
   (MSG_ReplyToSender through MSG_ForwardMessage.)
 */
typedef MSG_CommandType MSG_REPLY_TYPE;

/* The list of all message flags to not write to disk. */
#define MSG_FLAG_RUNTIME_ONLY   (MSG_FLAG_ELIDED)


/* ===========================================================================
   Structures.
   ===========================================================================
 */

/* Used for the various things that parse RFC822 headers...
 */
typedef struct message_header
{
  const char *value; /* The contents of a header (after ": ") */
  PRInt32 length;      /* The length of the data (it is not NULL-terminated.) */
} message_header;



XP_BEGIN_PROTOS

/* we'll need this for localized folder names */

extern int MK_MSG_INBOX_L10N_NAME;
extern int MK_MSG_OUTBOX_L10N_NAME; /* win16 variations are in allxpstr.h */
extern int MK_MSG_OUTBOX_L10N_NAME_OLD;
extern int MK_MSG_TRASH_L10N_NAME;
extern int MK_MSG_DRAFTS_L10N_NAME;
extern int MK_MSG_SENT_L10N_NAME;
extern int MK_MSG_TEMPLATES_L10N_NAME;

#define INBOX_FOLDER_NAME	MSG_GetSpecialFolderName(MK_MSG_INBOX_L10N_NAME)
#define QUEUE_FOLDER_NAME	MSG_GetSpecialFolderName(MK_MSG_OUTBOX_L10N_NAME)
#define QUEUE_FOLDER_NAME_OLD	MSG_GetSpecialFolderName(MK_MSG_OUTBOX_L10N_NAME_OLD)
#define TRASH_FOLDER_NAME	MSG_GetSpecialFolderName(MK_MSG_TRASH_L10N_NAME)
#define DRAFTS_FOLDER_NAME  MSG_GetSpecialFolderName(MK_MSG_DRAFTS_L10N_NAME)
#define SENT_FOLDER_NAME	MSG_GetSpecialFolderName(MK_MSG_SENT_L10N_NAME)
#define TEMPLATES_FOLDER_NAME	MSG_GetSpecialFolderName(MK_MSG_TEMPLATES_L10N_NAME)
#ifdef XP_OS2
#define INBOX_FOLDER_PRETTY_NAME       MSG_GetSpecialFolderPrettyName(MK_MSG_INBOX_L10N_NAME)
#define QUEUE_FOLDER_PRETTY_NAME       MSG_GetSpecialFolderPrettyName(MK_MSG_OUTBOX_L10N_NAME)
#define QUEUE_FOLDER_PRETTY_NAME_OLD   MSG_GetSpecialFolderPrettyName(MK_MSG_OUTBOX_L10N_NAME_OLD)
#define TRASH_FOLDER_PRETTY_NAME       MSG_GetSpecialFolderPrettyName(MK_MSG_TRASH_L10N_NAME)
#define DRAFTS_FOLDER_PRETTY_NAME	   MSG_GetSpecialFolderPrettyName(MK_MSG_DRAFTS_L10N_NAME)
#define SENT_FOLDER_PRETTY_NAME		   MSG_GetSpecialFolderPrettyName(MK_MSG_SENT_L10N_NAME)
#define TEMPLATES_FOLDER_PRETTY_NAME   MSG_GetSpecialFolderPrettyName(MK_MSG_TEMPLATES_L10N_NAME)
#endif

XP_END_PROTOS

#endif /* !_MSG_H_ */
