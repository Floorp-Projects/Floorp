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
#ifndef _MIMESIGNED_H_
#define _MIMESIGNED_H_

#include "mimetext.h"

/*
 * These functions are the public interface for this content type
 * handler and will be called in by the mime component.
 */
#define      SIGNED_CONTENT_TYPE  "application/x-pkcs7-signature"


/* The MimeInlineTextSIGNEDStub class implements the "application/x-pkcs7-mime"2
   MIME content types.
 */

typedef struct MimeInlineTextSIGNEDStubClass MimeInlineTextSIGNEDStubClass;
typedef struct MimeInlineTextSIGNEDStub      MimeInlineTextSIGNEDStub;

struct MimeInlineTextSIGNEDStubClass {
    MimeInlineTextClass text;
    char* buffer;
    PRInt32 bufferlen;
    PRInt32 buffermax;
};

extern MimeInlineTextSIGNEDStubClass mimeInlineTextSIGNEDStubClass;

struct MimeInlineTextSIGNEDStub {
  MimeInlineText text;
};

#endif /* _MIMESIGNED_H_ */
