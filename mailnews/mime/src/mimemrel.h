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

#ifndef _MIMEMREL_H_
#define _MIMEMREL_H_

#include "mimemult.h"
#include "plhash.h"
#include "prio.h"

/* The MimeMultipartRelated class implements the multipart/related MIME 
   container, which allows `sibling' sub-parts to refer to each other.
 */

typedef struct MimeMultipartRelatedClass MimeMultipartRelatedClass;
typedef struct MimeMultipartRelated      MimeMultipartRelated;

struct MimeMultipartRelatedClass {
	MimeMultipartClass multipart;
};

extern "C" MimeMultipartRelatedClass mimeMultipartRelatedClass;

struct MimeMultipartRelated {
	MimeMultipart multipart;	/* superclass variables */

	char* base_url;				/* Base URL (if any) for the whole
								   multipart/related. */

	char* head_buffer;			/* Buffer used to remember the text/html 'head'
								   part. */
	PRInt32 head_buffer_fp;		/* Active length. */
	PRInt32 head_buffer_size;		/* How big it is. */
	
	char *file_buffer_name;		/* The name of a temp file used when we
								   run out of room in the head_buffer. */
	PRFileDesc *file_stream;		/* A stream to it. */

	MimeHeaders* buffered_hdrs;	/* The headers of the 'head' part. */

	PRBool head_loaded;		/* Whether we've already passed the 'head'
								   part. */
	MimeObject* headobj;		/* The actual text/html head object. */

	PLHashTable		*hash;

	int (*real_output_fn) (char *buf, PRInt32 size, void *stream_closure);
	void* real_output_closure;

	char* curtag;
	PRInt32 curtag_max;
	PRInt32 curtag_length;



};

#endif /* _MIMEMREL_H_ */
