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

/* mimemdig.h --- definition of the MimeMultipartDigest class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMEMDIG_H_
#define _MIMEMDIG_H_

#include "mimemult.h"

/* The MimeMultipartDigest class implements the multipart/digest MIME 
   container, which is just like multipart/mixed, except that the default
   type (for parts with no type explicitly specified) is message/rfc822
   instead of text/plain.
 */

typedef struct MimeMultipartDigestClass MimeMultipartDigestClass;
typedef struct MimeMultipartDigest      MimeMultipartDigest;

struct MimeMultipartDigestClass {
  MimeMultipartClass multipart;
};

extern MimeMultipartDigestClass mimeMultipartDigestClass;

struct MimeMultipartDigest {
  MimeMultipart multipart;
};

#endif /* _MIMEMDIG_H_ */
