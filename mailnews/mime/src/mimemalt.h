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

#ifndef _MIMEMALT_H_
#define _MIMEMALT_H_

#include "mimemult.h"
#include "mimepbuf.h"

/* The MimeMultipartAlternative class implements the multipart/alternative
   MIME container, which displays only one (the `best') of a set of enclosed
   documents.
 */

typedef struct MimeMultipartAlternativeClass MimeMultipartAlternativeClass;
typedef struct MimeMultipartAlternative      MimeMultipartAlternative;

struct MimeMultipartAlternativeClass {
  MimeMultipartClass multipart;
};

extern "C" MimeMultipartAlternativeClass mimeMultipartAlternativeClass;

struct MimeMultipartAlternative {
  MimeMultipart multipart;			/* superclass variables */

  MimeHeaders *buffered_hdrs;		/* The headers of the currently-pending
									   part. */
  MimePartBufferData *part_buffer;	/* The data of the current-pending part
									   (see mimepbuf.h) */
};

#endif /* _MIMEMALT_H_ */
