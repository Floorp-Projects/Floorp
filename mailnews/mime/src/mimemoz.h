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

#ifndef _MIMEMOZ_H_
#define _MIMEMOZ_H_

#include "xp.h"
#include "prtypes.h"

/* Mozilla-specific interfaces
 */

/* Given a URL, this might return a better suggested name to save it as.

   When you have a URL, you can sometimes get a suggested name from
   URL_s->content_name, but if you're saving a URL to disk before the
   URL_Struct has been filled in by netlib, you don't have that yet.

   So if you're about to prompt for a file name *before* you call FE_GetURL
   with a format_out of FO_SAVE_AS, call this function first to see if it
   can offer you advice about what the suggested name for that URL should be.

   (This works by looking in a cache of recently-displayed MIME objects, and
   seeing if this URL matches.  If it does, the remembered content-name will
   be used.)
 */
extern char *MimeGuessURLContentName(MWContext *context, const char *url);

/* Given a URL, return the content type for the mime part, if the passed context
   recently parsed a message containing the part specified by the URL.
   This is used to figure out if we need to open the url in a browser window,
   or if we're just going to do a save as, anyay.
*/
extern char *MimeGetURLContentType(MWContext *context, const char *url);


/* Determines whether the given context is currently showing a text/html
   message.  (Used by libmsg to determine if replys should bring up the
   text/html editor. */

extern PRBool MimeShowingTextHtml(MWContext* context);



/* Yeech, hack...  Determine the URL to use to save just the HTML part of the
   currently-displayed message to disk.  If the current message doesn't have
   a text/html part, returns NULL.  Otherwise, the caller must free the
   returned string using PR_Free(). */

extern char* MimeGetHtmlPartURL(MWContext* context);


/* Return how many attachments are in the currently-displayed message. */
extern int MimeGetAttachmentCount(MWContext* context);

/* Returns what attachments are being viewed  in the currently-displayed
   message.  The returned data must be free'd using
   MimeFreeAttachmentList(). */
extern int MimeGetAttachmentList(MWContext* context,
								 MSG_AttachmentData** data);

extern void MimeFreeAttachmentList(MSG_AttachmentData* data);


/* Call this when destroying a context; this frees up some memory.
 */
extern void MimeDestroyContextData(MWContext *context);

#ifdef MOZ_SECURITY
HG10034
#endif

/* Used only by libnet, this indicates that the user bonked on the "show me
   details about attachments" button. */

extern int MIME_DisplayAttachmentPane(MWContext* context);

/* Used by libmsg, libmime, libi18n to strip out line continuations of a
   message header. It does *NOT* allocate new buffer. It returns the
   origial. */

extern char * MIME_StripContinuations(char *original);

/*
 * This is currently the primary interface for stream converters
 */
NET_StreamClass *
MIME_MessageConverter (int format_out, void *closure,
                       URL_Struct *url, MWContext *context);


#endif /* _MIMEMOZ_H_ */

