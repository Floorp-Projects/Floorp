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

/* mimeebod.h --- definition of the MimeExternalBody class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#ifndef _MIMEEBOD_H_
#define _MIMEEBOD_H_

#include "mimeobj.h"

/* The MimeExternalBody class implements the message/external-body MIME type.
   (This is not to be confused with MimeExternalObject, which implements the
   handler for application/octet-stream and other types with no more specific
   handlers.)
 */

typedef struct MimeExternalBodyClass MimeExternalBodyClass;
typedef struct MimeExternalBody      MimeExternalBody;

struct MimeExternalBodyClass {
  MimeObjectClass object;
};

extern MimeExternalBodyClass mimeExternalBodyClass;

struct MimeExternalBody {
  MimeObject object;			/* superclass variables */
  MimeHeaders *hdrs;			/* headers within this external-body, which
								   describe the network data which this body
								   is a pointer to. */
  char *body;					/* The "phantom body" of this link. */
};

#endif /* _MIMEEBOD_H_ */
