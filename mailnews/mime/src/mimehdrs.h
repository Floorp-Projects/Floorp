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

#ifndef _MIMEHDRS_H_
#define _MIMEHDRS_H_

// RICHIE
#include "modlmime.h"

/* This file defines the interface to message-header parsing and formatting
   code, including conversion to HTML.
 */



/* Other structs defined later in this file.
 */

/* Creation and destruction.
 */
extern MimeHeaders *MimeHeaders_new (void);
extern void MimeHeaders_free (MimeHeaders *);
extern MimeHeaders *MimeHeaders_copy (MimeHeaders *);


/* Feed this method the raw data from which you would like a header
   block to be parsed, one line at a time.  Feed it a blank line when
   you're done.  Returns negative on allocation-related failure.
 */
extern int MimeHeaders_parse_line (const char *buffer, PRInt32 size,
								   MimeHeaders *hdrs);


/* Converts a MimeHeaders object into HTML, by writing to the provided
   output function.
 */
extern int MimeHeaders_write_headers_html (MimeHeaders *hdrs,
										   MimeDisplayOptions *opt, 
                       PRBool             attachment);

/*
 * Writes all headers to the mime emitter.
 */
extern int 
MimeHeaders_write_all_headers (MimeHeaders *, MimeDisplayOptions *, PRBool);

/* Writes the headers as text/plain.
   This writes out a blank line after the headers, unless
   dont_write_content_type is true, in which case the header-block
   is not closed off, and none of the Content- headers are written.
 */
extern int MimeHeaders_write_raw_headers (MimeHeaders *hdrs,
										  MimeDisplayOptions *opt,
										  PRBool dont_write_content_type);


/* For drawing the tables that represent objects that can't be displayed
   inline. */
extern int MimeHeaders_write_attachment_box(MimeHeaders *hdrs,
											MimeDisplayOptions *opt,
											const char *content_type,
											const char *encoding,
											const char *name,
											const char *name_url,
											const char *body);

#ifdef MOZ_SECURITY
HG77761
#endif

/* Does all the heuristic silliness to find the filename in the given headers.
 */
extern char *MimeHeaders_get_name(MimeHeaders *hdrs);

extern char *mime_decode_filename(char *name);

#endif /* _MIMEHDRS_H_ */
