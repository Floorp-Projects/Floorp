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
#include "msgutils.h"
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

/* This string gets appended to the beginning of an attachment field
   during a forward quoted operation */

#define MSG_FORWARD_COOKIE  "$forward_quoted$"


/* The PRINTF macro is for debugging messages of unusual events (summary */
/* files out of date or invalid or the like.  It's so that as I use the mail */
/* to actually read my e-mail, I can look at the shell output whenever */
/* something unusual happens so I can get some clues as to what's going on. */
/* Please don't remove any PRINTF calls you see, and be sparing of adding */
/* any additional ones.  Thanks.  - Terry */

#ifdef DEBUG
#define PRINTF(msg) XP_Trace msg
#else
#define PRINTF(msg)
#endif

#ifdef FREEIF
#undef FREEIF
#endif
#define FREEIF(obj) do { if (obj) { XP_FREE (obj); obj = 0; }} while (0)

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

typedef int32 MsgChangeCookie;	/* used to unregister change notification */

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
  int32 length;      /* The length of the data (it is not NULL-terminated.) */
} message_header;



/* Argument to msg_NewsgroupNameMapper() */
typedef int (*msg_SubscribedGroupNameMapper) (MWContext *context,
                                              const char *name,
                                              void *closure);

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


int ConvertMsgErrToMKErr(uint32 err); /* this routine might live - the rest are
										probably already dead */


/* ===========================================================================
   Redisplay-related stuff
   ===========================================================================
 */

/* Clear out the message display (make Layout be displaying no document.) */
extern void msg_ClearMessageArea (MWContext *context);


/* Returns a line suitable for using as the envelope line in a BSD
   mail folder. The returned string is stored in a static, and so
   should be used before msg_GetDummyEnvelope is called again. */
extern char * msg_GetDummyEnvelope(void);

/* Returns TRUE if the buffer looks like a valid envelope.
   This test is somewhat more restrictive than XP_STRNCMP(buf, "From ", 5).
 */
extern XP_Bool msg_IsEnvelopeLine(const char *buf, int32 buf_size);


/* ===========================================================================
   Utilities specific to mail folders and their Folders and ThreadEntries.
   ===========================================================================
 */

/* returns the name of a magic folder - returns a new string. */
extern char *msg_MagicFolderName(MSG_Prefs* prefs, uint32 flag, int *pStatus);


/* Get the current fcc folder name, so we can sort them up at the top of the
   other folder lists with the other magic folders. */
const char* msg_GetDefaultFcc(XP_Bool news_p);


/* Reads the first few bytes of the file and returns FALSE if it doesn't
   seem to be a mail folder.  (Empty and nonexistent files return TRUE.)
   If it doesn't seem to be one, the user is asked whether it should be
   written to anyway, and their answer is returned.
 */
extern XP_Bool msg_ConfirmMailFile (MWContext *context, const char *file_name);

/* ===========================================================================
   The content-type converters for the MIME types.  These are provided by
   compose.c, but are registered by netlib rather than msglib, for some
   destined-to-be-mysterious reason.
   ===========================================================================
 */
extern NET_StreamClass *MIME_MessageConverter (int format_out, void *closure,
                                               URL_Struct *url,
                                               MWContext *context);

extern NET_StreamClass *MIME_RichtextConverter (int format_out, void *data_obj,
                                                URL_Struct *url,
                                                MWContext *context);

extern NET_StreamClass *MIME_EnrichedTextConverter (int format_out,
                                                    void *data_obj,
                                                    URL_Struct *url,
                                                    MWContext *context);

extern NET_StreamClass *MIME_ToDraftConverter (int format_out, void *closure,
                                               URL_Struct *url,
                                               MWContext *context);

extern NET_StreamClass *MIME_VCardConverter (int format_out, void *data_obj,
                                                URL_Struct *url,
                                                MWContext *context);

extern NET_StreamClass *MIME_JulianConverter (int format_out, void *data_obj,
                                              URL_Struct *url,
                                              MWContext *context);


/* This nastiness is how msg_SaveSelectedNewsMessages() works. */
extern NET_StreamClass *msg_MakeAppendToFolderStream (int format_out,
                                                       void *closure,
                                                       URL_Struct *url,
                                                       MWContext *);

/* This probably should be in mime.h, except that mime.h should mostly be
   private to libmsg.  So it's here. */
extern void
msg_StartMessageDeliveryWithAttachments (MSG_Pane *pane,
										 void      *fe_data,
										 MSG_CompositionFields *fields,
										 XP_Bool digest_p,
										 XP_Bool dont_deliver_p,
										 MSG_Deliver_Mode deliver_mode,
										 const char *attachment1_type,
										 const char *attachment1_body,
										 uint32 attachment1_body_length,
										 const struct MSG_AttachedFile *attachments,
										 void *mimeRelatedPart,
										 void (*message_delivery_done_callback)
										 (MWContext *context, 
										  void *fe_data,
										  int status,
										  const char *error_message));

extern int
msg_DownloadAttachments (MSG_Pane *pane,
						 void *fe_data,
						 const struct MSG_AttachmentData *attachments,
						 void (*attachments_done_callback)
						      (MWContext *context, 
							   void *fe_data,
							   int status, const char *error_message,
							   struct MSG_AttachedFile *attachments));


extern 
int msg_DoFCC (MSG_Pane *pane,
			   const char *input_file,  XP_FileType input_file_type,
			   const char *output_file, XP_FileType output_file_type,
			   const char *bcc_header_value,
			   const char *fcc_header_value);

extern char* msg_generate_message_id (void);

#ifdef XP_UNIX
extern int msg_DeliverMessageExternally(MWContext *, const char *msg_file);
#endif /* XP_UNIX */


XP_END_PROTOS





#endif /* !_MSG_H_ */
