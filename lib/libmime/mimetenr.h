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

/* mimetenr.h --- definition of the MimeInlineTextEnriched class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMETENR_H_
#define _MIMETENR_H_

#include "mimetric.h"

/* The MimeInlineTextEnriched class implements the text/enriched MIME content
   type, as defined in RFC 1563.  It does this largely by virtue of being a
   subclass of the MimeInlineTextRichtext class.
 */

typedef struct MimeInlineTextEnrichedClass MimeInlineTextEnrichedClass;
typedef struct MimeInlineTextEnriched      MimeInlineTextEnriched;

struct MimeInlineTextEnrichedClass {
  MimeInlineTextRichtextClass text;
};

extern MimeInlineTextEnrichedClass mimeInlineTextEnrichedClass;

struct MimeInlineTextEnriched {
  MimeInlineTextRichtext richtext;
};

#endif /* _MIMETENR_H_ */
