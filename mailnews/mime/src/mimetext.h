/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  int (*rot13_line) (MimeObject *obj, char *line, PRInt32 length);
  int (*convert_line_charset) (MimeObject *obj, char *line, PRInt32 length);
};

extern MimeInlineTextClass mimeInlineTextClass;

#define DAM_MAX_BUFFER_SIZE 8*1024      
#define DAM_MAX_LINES  1024

struct MimeInlineText {
  MimeLeaf leaf;			/* superclass variables */
  char *charset;			/* The charset from the content-type of this
							           object, or the caller-specified overrides
							           or defaults. */
  PRBool charsetOverridable; 
  char *cbuffer;			/* Buffer used for charset conversion. */
  PRInt32 cbuffer_size;
  
  nsCOMPtr<nsIUnicodeDecoder> inputDecoder;
  nsCOMPtr<nsIUnicodeEncoder> utf8Encoder;

  PRBool  inputAutodetect;
  PRBool  initializeCharset;
  PRInt32 lastLineInDam;
  PRInt32 curDamOffset;
  char *lineDamBuffer;
  char **lineDamPtrs;
};

#endif /* _MIMETEXT_H_ */
