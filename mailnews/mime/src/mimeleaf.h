/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MIMELEAF_H_
#define _MIMELEAF_H_

#include "mimeobj.h"
#include "modmimee.h"

/* MimeLeaf is the class for the objects representing all MIME types which
   are not containers for other MIME objects.  The implication of this is
   that they are MIME types which can have Content-Transfer-Encodings 
   applied to their data.  This class provides that service in its 
   parse_buffer() method:

     int (*parse_decoded_buffer) (const char *buf, PRInt32 size, MimeObject *obj)

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
  int (*parse_decoded_buffer) (const char *buf, PRInt32 size, MimeObject *obj);
  int (*close_decoder) (MimeObject *obj);
};

extern MimeLeafClass mimeLeafClass;

struct MimeLeaf {
  MimeObject object;		/* superclass variables */

  /* If we're doing Base64, Quoted-Printable, or UU decoding, this is the
	 state object for the decoder. */
  MimeDecoderData *decoder_data;
};

#endif /* _MIMELEAF_H_ */
