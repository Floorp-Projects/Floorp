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

/* mimemmix.h --- definition of the MimeMultipartMixed class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMEMMIX_H_
#define _MIMEMMIX_H_

#include "mimemult.h"

/* The MimeMultipartMixed class implements the multipart/mixed MIME container,
   and is also used for any and all otherwise-unrecognised subparts of
   multipart/.
 */

typedef struct MimeMultipartMixedClass MimeMultipartMixedClass;
typedef struct MimeMultipartMixed      MimeMultipartMixed;

struct MimeMultipartMixedClass {
  MimeMultipartClass multipart;
};

extern MimeMultipartMixedClass mimeMultipartMixedClass;

struct MimeMultipartMixed {
  MimeMultipart multipart;
};

#endif /* _MIMEMMIX_H_ */
