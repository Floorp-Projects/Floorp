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

/* mimetpla.c --- definition of the MimeInlineTextPlain class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "mimetpla.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextPlain, MimeInlineTextPlainClass,
			 mimeInlineTextPlainClass, &MIME_SUPERCLASS);

static int MimeInlineTextPlain_parse_begin (MimeObject *);
static int MimeInlineTextPlain_parse_line (char *, int32, MimeObject *);
static int MimeInlineTextPlain_parse_eof (MimeObject *, XP_Bool);

static int
MimeInlineTextPlainClassInitialize(MimeInlineTextPlainClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;
  XP_ASSERT(!oclass->class_initialized);
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
	  s = XP_STRDUP(strs[(obj->options->variable_width_plaintext_p ? 1 : 0) +
						(obj->options->wrap_long_lines_p ? 2 : 0)]);
	  if (!s) return MK_OUT_OF_MEMORY;
	  status = MimeObject_write(obj, s, XP_STRLEN(s), FALSE);
	  XP_FREE(s);
	  if (status < 0) return status;

	  /* text/plain objects always have separators before and after them.
		 Note that this is not the case for text/enriched objects. */
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}

static int
MimeInlineTextPlain_parse_eof (MimeObject *obj, XP_Bool abort_p)
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
	  status = MimeObject_write(obj, s, XP_STRLEN(s), FALSE);
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
MimeInlineTextPlain_parse_line (char *line, int32 length, MimeObject *obj)
{
  int status;

  XP_ASSERT(length > 0);
  if (length <= 0) return 0;

  status = MimeObject_grow_obuffer (obj, length * 2 + 40);
  if (status < 0) return status;

  /* Copy `line' to `out', quoting HTML along the way.
	 Note: this function does no charset conversion; that has already
	 been done.
   */
  *obj->obuffer = 0;
  status = NET_ScanForURLs (
#ifndef MOZILLA_30
							(obj->options ? obj->options->pane : 0),
#endif /* !MOZILLA_30 */
							line, length, obj->obuffer, obj->obuffer_size - 10,
							(obj->options ?
							 obj->options->dont_touch_citations_p : FALSE));
  if (status < 0) return status;
  XP_ASSERT(*line == 0 || *obj->obuffer);
  return MimeObject_write(obj, obj->obuffer, XP_STRLEN(obj->obuffer), TRUE);
}
