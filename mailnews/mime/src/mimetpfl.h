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
