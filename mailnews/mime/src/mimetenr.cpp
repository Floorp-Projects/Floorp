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
