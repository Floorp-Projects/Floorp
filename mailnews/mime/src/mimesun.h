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

#ifndef _MIMESUN_H_
#define _MIMESUN_H_

#include "mimemult.h"

/* MimeSunAttachment is the class for X-Sun-Attachment message contents, which
   is the Content-Type assigned by that pile of garbage called MailTool.  This
   is not a MIME type per se, but it's very similar to multipart/mixed, so it's
   easy to parse.  Lots of people use MailTool, so what the hell.

   The format is this:

    = Content-Type is X-Sun-Attachment
	= parts are separated by lines of exactly ten dashes
	= just after the dashes comes a block of headers, including:

      X-Sun-Data-Type: (manditory)
			Values are Text, Postscript, Scribe, SGML, TeX, Troff, DVI,
			and Message.

      X-Sun-Encoding-Info: (optional)
	  		Ordered, comma-separated values, including Compress and Uuencode.

      X-Sun-Data-Name: (optional)
	  		File name, maybe.

      X-Sun-Data-Description: (optional)
			Longer text.

      X-Sun-Content-Lines: (manditory, unless Length is present)
			Number of lines in the body, not counting headers and the blank
			line that follows them.

      X-Sun-Content-Length: (manditory, unless Lines is present)
	  		Bytes, presumably using Unix line terminators.
 */

typedef struct MimeSunAttachmentClass MimeSunAttachmentClass;
typedef struct MimeSunAttachment      MimeSunAttachment;

struct MimeSunAttachmentClass {
  MimeMultipartClass multipart;
};

extern MimeSunAttachmentClass mimeSunAttachmentClass;

struct MimeSunAttachment {
  MimeMultipart multipart;
};

#endif /* _MIMESUN_H_ */
