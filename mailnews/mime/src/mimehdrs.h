/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _MIMEHDRS_H_
#define _MIMEHDRS_H_

#include "modlmime.h"

/* This file defines the interface to message-header parsing and formatting
   code, including conversion to HTML. */

/* Other structs defined later in this file.
 */

/* Creation and destruction.
 */
extern MimeHeaders *MimeHeaders_new (void);
//extern void MimeHeaders_free (MimeHeaders *);
//extern MimeHeaders *MimeHeaders_copy (MimeHeaders *);


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


/* Some crypto-related HTML-generated utility routines.
 * XXX This may not be needed. XXX 
 */
extern char *MimeHeaders_open_crypto_stamp(void);
extern char *MimeHeaders_finish_open_crypto_stamp(void);
extern char *MimeHeaders_close_crypto_stamp(void);
extern char *MimeHeaders_make_crypto_stamp(PRBool encrypted_p,

   PRBool signed_p,

   PRBool good_p,

   PRBool unverified_p,

   PRBool close_parent_stamp_p,

   const char *stamp_url);

/* Does all the heuristic silliness to find the filename in the given headers.
 */
extern char *MimeHeaders_get_name(MimeHeaders *hdrs, MimeDisplayOptions *opt);

extern char *mime_decode_filename(char *name, const char* charset,
                                  MimeDisplayOptions *opt);

#endif /* _MIMEHDRS_H_ */
