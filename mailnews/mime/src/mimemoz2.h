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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

////////////////////////////////////////////////////////////////
// Bridge routines for legacy mime code
////////////////////////////////////////////////////////////////

// Create bridge stream for libmime
void         *mime_bridge_create_stream(void *newStreamObj, const char *urlString,
                                        int   format_out );

// Destroy bridge stream for libmime
void          mime_bridge_destroy_stream(void *newStream);

// These are hooks into the libmime parsing functions...
int           mime_display_stream_write (void *stream, const char* buf, int size);
unsigned int  mime_display_stream_write_ready (void *stream);
void          mime_display_stream_complete (void *stream);
void          mime_display_stream_abort (void *stream, int status);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MIMEMOZ_H_ */

