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

#ifndef _MIMEMREL_H_
#define _MIMEMREL_H_

#include "mimemult.h"
#include "plhash.h"
#include "prio.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"


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
	
	nsFileSpec          *file_buffer_spec;		/* The nsFileSpec of a temp file used when we
								                               run out of room in the head_buffer. */
	nsInputFileStream   *input_file_stream;		/* A stream to it. */
	nsOutputFileStream  *output_file_stream;	/* A stream to it. */

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
