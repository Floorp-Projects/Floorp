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

#ifndef _MIMEUNTY_H_
#define _MIMEUNTY_H_

#include "mimecont.h"

/* The MimeUntypedText class is used for untyped message contents, that is, 
   it is the class used for the body of a message/rfc822 object which had
   *no* Content-Type header, as opposed to an unrecognized content-type.
   Such a message, technically, does not contain MIME data (it follows only
   RFC 822, not RFC 1521.)

   This is a container class, and the reason for that is that it loosely
   parses the body of the message looking for ``sub-parts'' and then
   creates appropriate containers for them.

   More specifically, it looks for uuencoded data.  It may do more than that
   some day.

   Basically, the algorithm followed is:

     if line is "begin 644 foo.gif"
       if there is an open sub-part, close it
	   add a sub-part with type: image/gif; encoding: x-uue
	   hand this line to it
	   and hand subsequent lines to that subpart
	 else if there is an open uuencoded sub-part, and line is "end"
	   hand this line to it
	   close off the uuencoded sub-part
     else if there is an open sub-part
	   hand this line to it
     else
       open a text/plain subpart
	   hand this line to it

   Adding other types than uuencode to this (for example, PGP) would be 
   pretty straightforward.
 */

typedef struct MimeUntypedTextClass MimeUntypedTextClass;
typedef struct MimeUntypedText      MimeUntypedText;

struct MimeUntypedTextClass {
  MimeContainerClass container;
};

extern MimeUntypedTextClass mimeUntypedTextClass;

typedef enum {
  MimeUntypedTextSubpartTypeText,	/* text/plain */
  MimeUntypedTextSubpartTypeUUE,	/* uuencoded data */
  MimeUntypedTextSubpartTypeBinhex	/* Mac BinHex data */
} MimeUntypedTextSubpartType;

struct MimeUntypedText {
  MimeContainer container;			/* superclass variables */
  MimeObject *open_subpart;			/* The part still-being-parsed */
  MimeUntypedTextSubpartType type;	/* What kind of type it is */
  MimeHeaders *open_hdrs;			/* The faked-up headers describing it */
};

#endif /* _MIMEUNTY_H_ */
