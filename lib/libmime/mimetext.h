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

/* mimetext.h --- definition of the MimeInlineText class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#ifndef _MIMETEXT_H_
#define _MIMETEXT_H_

#include "mimeleaf.h"

/* The MimeInlineText class is the superclass of all handlers for the
   MIME text/ content types (which convert various text formats to HTML,
   in one form or another.)

   It provides two services:

     =  if ROT13 decoding is desired, the text will be rotated before
	    the `parse_line' method it called;

	 =  text will be converted from the message's charset to the "target"
        charset before the `parse_line' method is called.

   The contract with charset-conversion is that the converted data will
   be such that one may interpret any octets (8-bit bytes) in the data
   which are in the range of the ASCII characters (0-127) as ASCII
   characters.  It is explicitly legal, for example, to scan through
   the string for "<" and replace it with "&lt;", and to search for things
   that look like URLs and to wrap them with interesting HTML tags.

   The charset to which we convert will probably be UTF-8 (an encoding of
   the Unicode character set, with the feature that all octets with the
   high bit off have the same interpretations as ASCII.)

   #### NOTE: if it turns out that we use JIS (ISO-2022-JP) as the target
        encoding, then this is not quite true; it is safe to search for the
		low ASCII values (under hex 0x40, octal 0100, which is '@') but it
		is NOT safe to search for values higher than that -- they may be
		being used as the subsequent bytes in a multi-byte escape sequence.
		It's a nice coincidence that HTML's critical characters ("<", ">",
		and "&") have values under 0x40...
 */

typedef struct MimeInlineTextClass MimeInlineTextClass;
typedef struct MimeInlineText      MimeInlineText;

struct MimeInlineTextClass {
  MimeLeafClass   leaf;
  int (*rot13_line) (MimeObject *obj, char *line, int32 length);
  int (*convert_line_charset) (MimeObject *obj, char *line, int32 length);
};

extern MimeInlineTextClass mimeInlineTextClass;

struct MimeInlineText {
  MimeLeaf leaf;			/* superclass variables */
  char *charset;			/* The charset from the content-type of this
							   object, or the caller-specified overrides
							   or defaults.
							 */
  char *cbuffer;			/* Buffer used for charset conversion. */
  int32 cbuffer_size;

};

#endif /* _MIMETEXT_H_ */
