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

#ifndef _MIMETRIC_H_
#define _MIMETRIC_H_

#include "mimetext.h"

/* The MimeInlineTextRichtext class implements the (obsolete and deprecated)
   text/richtext MIME content type, as defined in RFC 1341, and also the
   text/enriched MIME content type, as defined in RFC 1563.
 */

typedef struct MimeInlineTextRichtextClass MimeInlineTextRichtextClass;
typedef struct MimeInlineTextRichtext      MimeInlineTextRichtext;

struct MimeInlineTextRichtextClass {
  MimeInlineTextClass text;
  PRBool enriched_p;	/* Whether we should act like text/enriched instead. */
};

extern MimeInlineTextRichtextClass mimeInlineTextRichtextClass;

struct MimeInlineTextRichtext {
  MimeInlineText text;
};


extern int
MimeRichtextConvert (char *line, PRInt32 length,
					 int (*output_fn) (char *buf, PRInt32 size, void *closure),
					 void *closure,
					 char **obufferP,
					 PRInt32 *obuffer_sizeP,
					 PRBool enriched_p);

#endif /* _MIMETRIC_H_ */
