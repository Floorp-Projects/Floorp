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

#ifndef _MIMETPFL_H_
#define _MIMETPFL_H_

#include "mimetext.h"

/* The MimeInlineTextPlainFlowed class implements the
   text/plain MIME content type for the special case of a supplied
   format=flowed. See
   ftp://ftp.ietf.org/internet-drafts/draft-gellens-format-06.txt for
   more information. 
 */

typedef struct MimeInlineTextPlainFlowedClass MimeInlineTextPlainFlowedClass;
typedef struct MimeInlineTextPlainFlowed      MimeInlineTextPlainFlowed;

struct MimeInlineTextPlainFlowedClass {
  MimeInlineTextClass text;
};

extern MimeInlineTextPlainFlowedClass mimeInlineTextPlainFlowedClass;

struct MimeInlineTextPlainFlowed {
  MimeInlineText text;
};

/** Made to contain information to be kept during the while message.
 * Will be placed in the obj->obuff. Is it safe there or could
 * someone or something tamper with that buffer between visits to
 * MimeInlineTextPlainFlowed functions???? XXX
 */
struct MimeInlineTextPlainFlowedExData {
  struct MimeObject *ownerobj; /* The owner of this struct */
  uint32 inflow; /* If we currently are in flow */
  uint32 quotelevel; /* How deep is your love, uhr, quotelevel I meen. */
  struct MimeInlineTextPlainFlowedExData *next;
};

#endif /* _MIMETPFL_H_ */
