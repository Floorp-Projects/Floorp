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
