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

#ifndef _MIMETRIC_H_
#define _MIMETRIC_H_

#include "mimetext.h"

/* The MimeInlineTextRichtext class implements the (obsolete and deprecated)
   text/richtext MIME content type, as defined in RFC 1341, and also the
   text/enriched MIME content type, as defined in RFC 1563.
 */

typedef struct MimeInlineTextRichtextClass MimeInlineTextRichtextClass;
typedef struct MimeInlineTextRichtext      MimeInlineTextRichtext;

struct MimeInlineTextRichtextClass {
  MimeInlineTextClass text;
  PRBool enriched_p;	/* Whether we should act like text/enriched instead. */
};

extern MimeInlineTextRichtextClass mimeInlineTextRichtextClass;

struct MimeInlineTextRichtext {
  MimeInlineText text;
};


extern int
MimeRichtextConvert (char *line, PRInt32 length,
					 int (*output_fn) (char *buf, PRInt32 size, void *closure),
					 void *closure,
					 char **obufferP,
					 PRInt32 *obuffer_sizeP,
					 PRBool enriched_p);

#endif /* _MIMETRIC_H_ */
