/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __mimedisp_h
#define __mimedisp_h

/* mimedisp.h: Interface between netlib and the toplevel message/rfc822 parser:
   MIME_MessageConverter()
 */

struct mime_stream_data {           /* This struct is the state we pass around
                                       amongst the various stream functions
                                       used by MIME_MessageConverter().
                                     */

  URL_Struct *url;                  /* The URL this is all coming from. */
  int format_out;
  MWContext *context;
  NET_StreamClass *stream;          /* The stream to which we write output */
  NET_StreamClass *istream;   /* The stream we're writing out image data,
                                                                  if any. */
  MimeObject *obj;                  /* The root parser object */
  MimeDisplayOptions *options;      /* Data for communicating with libmime.a */

  /* These are used by FO_QUOTE_HTML_MESSAGE stuff only: */
  int16 lastcsid;                   /* csid corresponding to above. */
  int16 outcsid;                    /* csid passed to EDT_PasteQuoteINTL */

#ifndef MOZILLA_30
  uint8 rand_buf[6];                /* Random number used in the MATCH
                                       attribute of the ILAYER tag
                                       pair that encapsulates a
                                       text/html part.  (The
                                       attributes must match on the
                                       ILAYER and the closing
                                       /ILAYER.)  This is used to
                                       prevent stray layer tags (or
                                       maliciously placed ones) inside
                                       an email message allowing the
                                       message to escape from its
                                       encapsulated environment. */
#endif /* MOZILLA_30 */
    
#ifdef DEBUG_terry
    XP_File logit;              /* Temp file to put generated HTML into. */
#endif
};


struct MimeDisplayData {            /* This struct is what we hang off of
                                       MWContext->mime_data, to remember info
                                       about the last MIME object we've
                                       parsed and displayed.  See
                                       MimeGuessURLContentName() below.
                                     */
  MimeObject *last_parsed_object;
  char *last_parsed_url;

#ifdef LOCK_LAST_CACHED_MESSAGE
  char *previous_locked_url;
#endif /* LOCK_LAST_CACHED_MESSAGE */

#ifndef MOZILLA_30
  MSG_Pane* last_pane;
#endif /* MOZILLA_30 */
};

#endif

