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

#ifndef _MIMEVCRD_H_
#define _MIMEVCRD_H_

#include "mimetext.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"

/* The MimeInlineTextHTML class implements the text/x-vcard and (maybe?
   someday?) the application/directory MIME content types.
 */

typedef struct MimeInlineTextVCardClass MimeInlineTextVCardClass;
typedef struct MimeInlineTextVCard      MimeInlineTextVCard;

struct MimeInlineTextVCardClass {
  MimeInlineTextClass         text;
  char                        *vCardString;
};

extern MimeInlineTextVCardClass mimeInlineTextVCardClass;

struct MimeInlineTextVCard {
  MimeInlineText text;
};

extern "C" char *
VCardGetStringByID(PRInt32 aMsgId);

#endif /* _MIMEVCRD_H_ */
