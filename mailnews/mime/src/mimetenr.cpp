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

#include "mimetenr.h"
#include "prlog.h"

/* All the magic for this class is in mimetric.c; since text/enriched and
   text/richtext are so similar, it was easiest to implement them in the
   same method (but this is a subclass anyway just for general goodness.)
 */

#define MIME_SUPERCLASS mimeInlineTextRichtextClass
MimeDefClass(MimeInlineTextEnriched, MimeInlineTextEnrichedClass,
			 mimeInlineTextEnrichedClass, &MIME_SUPERCLASS);

static int
MimeInlineTextEnrichedClassInitialize(MimeInlineTextEnrichedClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  MimeInlineTextRichtextClass *rclass = (MimeInlineTextRichtextClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  rclass->enriched_p = PR_TRUE;
  return 0;
}
