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

#include "mimetpla.h"
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMimeTransition.h"
#include "nsMimeURLUtils.h"
#include "nsCRT.h"
#include "nsMimeStringResources.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextPlain, MimeInlineTextPlainClass,
			 mimeInlineTextPlainClass, &MIME_SUPERCLASS);

static int MimeInlineTextPlain_parse_begin (MimeObject *);
static int MimeInlineTextPlain_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextPlain_parse_eof (MimeObject *, PRBool);

static int
MimeInlineTextPlainClassInitialize(MimeInlineTextPlainClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "class not initialized");
  oclass->parse_begin = MimeInlineTextPlain_parse_begin;
  oclass->parse_line  = MimeInlineTextPlain_parse_line;
  oclass->parse_eof   = MimeInlineTextPlain_parse_eof;
  return 0;
}

static int
MimeInlineTextPlain_parse_begin (MimeObject *obj)
{
  int status = 0;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
	  char* strs[4];
	  char* s;
	  strs[0] = "<PRE>";
	  strs[1] = "<PRE VARIABLE>";
	  strs[2] = "<PRE WRAP>";
	  strs[3] = "<PRE VARIABLE WRAP>";
	  s = nsCRT::strdup(strs[(obj->options->variable_width_plaintext_p ? 1 : 0) +
						(obj->options->wrap_long_lines_p ? 2 : 0)]);
	  if (!s) return MIME_OUT_OF_MEMORY;
	  status = MimeObject_write(obj, s, nsCRT::strlen(s), PR_FALSE);
	  PR_Free(s);
	  if (status < 0) return status;

	  /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects. */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}

static int
MimeInlineTextPlain_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  if (obj->closed_p) return 0;
  
  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn &&
	  !abort_p)
	{
	  char s[] = "</PRE>";
	  status = MimeObject_write(obj, s, nsCRT::strlen(s), PR_FALSE);
	  if (status < 0) return status;

	  /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects.
	   */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}


static int
MimeInlineTextPlain_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  int status;

  NS_ASSERTION(length > 0, "zero length");
  if (length <= 0) return 0;

  status = MimeObject_grow_obuffer (obj, (length * 2) + 40);
  if (status < 0) return status;

  /* Copy `line' to `out', quoting HTML along the way.
	 Note: this function does no charset conversion; that has already
	 been done.
   */
  *obj->obuffer = 0;

  nsMimeURLUtils myUtil;

  // If we have been told not to mess with this text, then don't do this search!
  PRBool skipScanning = (obj->options && obj->options->force_user_charset);
    
  if (!skipScanning)
    status = myUtil.ScanForURLs(line, length, obj->obuffer, obj->obuffer_size - 10,
							                (obj->options ?
							                obj->options->dont_touch_citations_p : PR_FALSE));
  else
  {
    nsCRT::memcpy(obj->obuffer, line, length);
    obj->obuffer[length] = '\0';
    status = NS_OK;
  }

  if (status != NS_OK)
    return status;

  NS_ASSERTION(*line == 0 || *obj->obuffer, "have line or buffer");
  return MimeObject_write(obj, obj->obuffer, nsCRT::strlen(obj->obuffer), PR_TRUE);
}
