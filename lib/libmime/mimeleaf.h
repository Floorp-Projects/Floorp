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

/* mimeleaf.h --- definition of the MimeLeaf class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMELEAF_H_
#define _MIMELEAF_H_

#include "mimeobj.h"
#include "mimeenc.h"

/* MimeLeaf is the class for the objects representing all MIME types which
   are not containers for other MIME objects.  The implication of this is
   that they are MIME types which can have Content-Transfer-Encodings 
   applied to their data.  This class provides that service in its 
   parse_buffer() method:

     int (*parse_decoded_buffer) (char *buf, int32 size, MimeObject *obj)

   The `parse_buffer' method of MimeLeaf passes each block of data through
   the appropriate decoder (if any) and then calls `parse_decoded_buffer'
   on each block (not line) of output.

   The default `parse_decoded_buffer' method of MimeLeaf line-buffers the
   now-decoded data, handing each line to the `parse_line' method in turn.
   If different behavior is desired (for example, if a class wants access
   to the decoded data before it is line-buffered) the `parse_decoded_buffer'
   method should be overridden.  (MimeExternalObject does this.)
 */

typedef struct MimeLeafClass MimeLeafClass;
typedef struct MimeLeaf      MimeLeaf;

struct MimeLeafClass {
  MimeObjectClass object;
  /* This is the callback that is handed to the decoder. */
  int (*parse_decoded_buffer) (char *buf, int32 size, MimeObject *obj);
};

extern MimeLeafClass mimeLeafClass;

struct MimeLeaf {
  MimeObject object;		/* superclass variables */

  /* If we're doing Base64, Quoted-Printable, or UU decoding, this is the
	 state object for the decoder. */
  MimeDecoderData *decoder_data;
};

#endif /* _MIMELEAF_H_ */
