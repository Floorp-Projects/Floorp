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

/* mimemapl.h --- definition of the MimeMultipartAppleDouble class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMEMAPL_H_
#define _MIMEMAPL_H_

#include "mimemult.h"

/* The MimeMultipartAppleDouble class implements the multipart/appledouble
   MIME container, which provides a method of encapsulating and reconstructing
   a two-forked Macintosh file.
 */

typedef struct MimeMultipartAppleDoubleClass MimeMultipartAppleDoubleClass;
typedef struct MimeMultipartAppleDouble      MimeMultipartAppleDouble;

struct MimeMultipartAppleDoubleClass {
  MimeMultipartClass multipart;
};

extern MimeMultipartAppleDoubleClass mimeMultipartAppleDoubleClass;

struct MimeMultipartAppleDouble {
  MimeMultipart multipart;
};

#endif /* _MIMEMAPL_H_ */
