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
