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

/* mimetric.h --- definition of the MimeInlineTextRichtext class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
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
  XP_Bool enriched_p;	/* Whether we should act like text/enriched instead. */
};

extern MimeInlineTextRichtextClass mimeInlineTextRichtextClass;

struct MimeInlineTextRichtext {
  MimeInlineText text;
};


extern int
MimeRichtextConvert (char *line, int32 length,
					 int (*output_fn) (char *buf, int32 size, void *closure),
					 void *closure,
					 char **obufferP,
					 int32 *obuffer_sizeP,
					 XP_Bool enriched_p);

#endif /* _MIMETRIC_H_ */
