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

/* mimeiimg.c --- definition of the MimeInlineImage class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "mimeiimg.h"

#define MIME_SUPERCLASS mimeLeafClass
MimeDefClass(MimeInlineImage, MimeInlineImageClass,
			 mimeInlineImageClass, &MIME_SUPERCLASS);

static int MimeInlineImage_initialize (MimeObject *);
static void MimeInlineImage_finalize (MimeObject *);
static int MimeInlineImage_parse_begin (MimeObject *);
static int MimeInlineImage_parse_line (char *, int32, MimeObject *);
static int MimeInlineImage_parse_eof (MimeObject *, XP_Bool);
static int MimeInlineImage_parse_decoded_buffer (char *, int32, MimeObject *);

static int
MimeInlineImageClassInitialize(MimeInlineImageClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;
  MimeLeafClass   *lclass = (MimeLeafClass *) class;

  XP_ASSERT(!oclass->class_initialized);
  oclass->initialize   = MimeInlineImage_initialize;
  oclass->finalize     = MimeInlineImage_finalize;
  oclass->parse_begin  = MimeInlineImage_parse_begin;
  oclass->parse_line   = MimeInlineImage_parse_line;
  oclass->parse_eof    = MimeInlineImage_parse_eof;
  lclass->parse_decoded_buffer = MimeInlineImage_parse_decoded_buffer;

  return 0;
}


static int
MimeInlineImage_initialize (MimeObject *object)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeInlineImage_finalize (MimeObject *object)
{
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}

static int
MimeInlineImage_parse_begin (MimeObject *obj)
{
  MimeInlineImage *img = (MimeInlineImage *) obj;
  MimeInlineImageClass *class;

  int status;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (!obj->options || !obj->options->output_fn)
	return 0;

  class = (MimeInlineImageClass *) obj->class;

  if (obj->options &&
	  obj->options->image_begin &&
	  obj->options->write_html_p &&
	  obj->options->image_write_buffer)
	{
	  char *html, *part, *image_url;
	  const char *ct;

	  part = mime_part_address(obj);
	  if (!part) return MK_OUT_OF_MEMORY;
	  image_url = mime_set_url_part(obj->options->url, part, TRUE);
	  if (!image_url)
		{
		  XP_FREE(part);
		  return MK_OUT_OF_MEMORY;
		}
	  XP_FREE(part);

	  ct = obj->content_type;
	  if (!ct) ct = IMAGE_GIF;  /* Can't happen?  Close enough. */

	  img->image_data =
		obj->options->image_begin(image_url, ct, obj->options->stream_closure);
	  XP_FREE(image_url);

	  if (!img->image_data) return MK_OUT_OF_MEMORY;

	  html = obj->options->make_image_html(img->image_data);
	  if (!html) return MK_OUT_OF_MEMORY;

	  status = MimeObject_write(obj, html, XP_STRLEN(html), TRUE);
	  XP_FREE(html);
	  if (status < 0) return status;
	}

  return 0;
}


static int
MimeInlineImage_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  MimeInlineImage *img = (MimeInlineImage *) obj;
  int status;
  if (obj->closed_p) return 0;

  /* Force out any buffered data from the superclass (the base64 decoder.) */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) abort_p = TRUE;

  if (img->image_data)
	{
	  obj->options->image_end(img->image_data,
							  (status < 0 ? status : (abort_p ? -1 : 0)));
	  img->image_data = 0;
	}

  return status;
}


static int
MimeInlineImage_parse_decoded_buffer (char *buf, int32 size, MimeObject *obj)
{
  /* This is called (by MimeLeafClass->parse_buffer) with blocks of data
	 that have already been base64-decoded.  Pass this raw image data
	 along to the backend-specific image display code.
   */
  MimeInlineImage *img  = (MimeInlineImage *) obj;
  int status;

  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p)
	{
	  /* in this case, we just want the raw data...
		 Make the stream, if it's not made, and dump the data out.
	   */

	  if (!obj->options->state->first_data_written_p)
		{
		  status = MimeObject_output_init(obj, 0);
		  if (status < 0) return status;
		  XP_ASSERT(obj->options->state->first_data_written_p);
		}
	  
	  return MimeObject_write(obj, buf, size, TRUE);
	}


  if (!obj->options ||
	  !obj->options->image_write_buffer)
	return 0;

  /* If we don't have any image data, the image_end method must have already
	 been called, so don't call image_write_buffer again. */
  if (!img->image_data) return 0;

  /* Hand this data off to the backend-specific image display stream.
   */
  status = obj->options->image_write_buffer (buf, size, img->image_data);
  
  /* If the image display stream fails, then close the stream - but do not
	 return the failure status, and do not give up on parsing this object.
	 Just because the image data was corrupt doesn't mean we need to give up
	 on the whole document; we can continue by just skipping over the rest of
	 this part, and letting our parent continue.
   */
  if (status < 0)
	{
	  obj->options->image_end (img->image_data, status);
	  img->image_data = 0;
	  status = 0;
	}

  return status;
}


static int
MimeInlineImage_parse_line (char *line, int32 length, MimeObject *obj)
{
  /* This method should never be called (inline images do no line buffering).
   */
  XP_ASSERT(0);
  return -1;
}
