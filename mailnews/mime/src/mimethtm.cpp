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

#include "mimethtm.h"

#include "prmem.h"
#include "plstr.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextHTML, MimeInlineTextHTMLClass,
			 mimeInlineTextHTMLClass, &MIME_SUPERCLASS);

static int MimeInlineTextHTML_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextHTML_parse_eof (MimeObject *, PRBool);
static int MimeInlineTextHTML_parse_begin (MimeObject *obj);

static int
MimeInlineTextHTMLClassInitialize(MimeInlineTextHTMLClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextHTML_parse_begin;
  oclass->parse_line  = MimeInlineTextHTML_parse_line;
  oclass->parse_eof   = MimeInlineTextHTML_parse_eof;

  return 0;
}

static int
MimeInlineTextHTML_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)&mimeLeafClass)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  /* If this HTML part has a Content-Base header, and if we're displaying
	 to the screen (that is, not writing this part "raw") then translate
	 that Content-Base header into a <BASE> tag in the HTML.
   */
  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
	  char *base_hdr = MimeHeaders_get (obj->headers, HEADER_CONTENT_BASE,
										PR_FALSE, PR_FALSE);

    /* rhp - for MHTML Spec changes!!! */
    if (!base_hdr)
    {
      base_hdr = MimeHeaders_get (obj->headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE);
    }
    /* rhp - for MHTML Spec changes!!! */

      /* Encapsulate the entire text/html part inside an in-flow
         layer.  This will provide it a private coordinate system and
         prevent it from escaping the bounds of its clipping so that
         it might, for example, spoof a mail header. */
	  if (obj->options->set_html_state_fn)
        {
          status = obj->options->set_html_state_fn(obj->options->stream_closure,
                                                   PR_TRUE,   /* layer_encapulate_p */
                                                   PR_TRUE,   /* start_p */
                                                   PR_FALSE); /* abort_p */
          if (status < 0) return status;
        }

	  if (base_hdr)
		{
		  char *buf = (char *) PR_MALLOC(PL_strlen(base_hdr) + 20);
		  const char *in;
		  char *out;
		  if (!buf)
			return MK_OUT_OF_MEMORY;

		  /* The value of the Content-Base header is a number of "words".
			 Whitespace in this header is not significant -- it is assumed
			 that any real whitespace in the URL has already been encoded,
			 and whitespace has been inserted to allow the lines in the
			 mail header to be wrapped reasonably.  Creators are supposed
			 to insert whitespace every 40 characters or less.
		   */
		  PL_strcpy(buf, "<BASE HREF=\"");
		  out = buf + PL_strlen(buf);

		  for (in = base_hdr; *in; in++)
			/* ignore whitespace and quotes */
			if (!XP_IS_SPACE(*in) && *in != '"')
			  *out++ = *in;

		  /* Close the tag and argument. */
		  *out++ = '"';
		  *out++ = '>';
		  *out++ = 0;

		  PR_Free(base_hdr);

		  status = MimeObject_write(obj, buf, PL_strlen(buf), PR_FALSE);
		  PR_Free(buf);
		  if (status < 0) return status;
		}
	}

  return 0;
}


static int
MimeInlineTextHTML_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  if (!obj->output_p) return 0;

  if (obj->options && obj->options->output_fn)
	return MimeObject_write(obj, line, length, PR_TRUE);
  else
	return 0;
}

/* This method is the same as that of MimeInlineTextRichtext (and thus
   MimeInlineTextEnriched); maybe that means that MimeInlineTextHTML
   should share a common parent with them which is not also shared by
   MimeInlineTextPlain?
 */
static int
MimeInlineTextHTML_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (obj->output_p &&
	  obj->options &&
	  obj->options->write_html_p &&
	  obj->options->set_html_state_fn)
	{
      return obj->options->set_html_state_fn(obj->options->stream_closure,
                                             PR_TRUE,  /* layer_encapulate_p */
                                             PR_FALSE, /* start_p */
                                             abort_p);
	}
  return 0;
}
