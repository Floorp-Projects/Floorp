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

/* mimeeobj.h --- definition of the MimeExternalObject class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#ifndef _MIMEEOBJ_H_
#define _MIMEEOBJ_H_

#include "mimeleaf.h"

/* The MimeExternalObject class represents MIME parts which contain data
   which cannot be displayed inline -- application/octet-stream and any
   other type that is not otherwise specially handled.  (This is not to
   be confused with MimeExternalBody, which is the handler for the 
   message/external-object MIME type only.)
 */

typedef struct MimeExternalObjectClass MimeExternalObjectClass;
typedef struct MimeExternalObject      MimeExternalObject;

struct MimeExternalObjectClass {
  MimeLeafClass leaf;
};

extern MimeExternalObjectClass mimeExternalObjectClass;

struct MimeExternalObject {
  MimeLeaf leaf;
};

#endif /* _MIMEEOBJ_H_ */
