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
#include "nsCOMPtr.h"
#include "mimeiimg.h"
#include "mimemoz2.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsMimeTypes.h"
#include "nsMimeStringResources.h"
#include "nsCRT.h"
#include "nsEscape.h"

#define MIME_SUPERCLASS mimeLeafClass
MimeDefClass(MimeInlineImage, MimeInlineImageClass,
			 mimeInlineImageClass, &MIME_SUPERCLASS);

static int MimeInlineImage_initialize (MimeObject *);
static void MimeInlineImage_finalize (MimeObject *);
static int MimeInlineImage_parse_begin (MimeObject *);
static int MimeInlineImage_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineImage_parse_eof (MimeObject *, PRBool);
static int MimeInlineImage_parse_decoded_buffer (char *, PRInt32, MimeObject *);

static int
MimeInlineImageClassInitialize(MimeInlineImageClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  MimeLeafClass   *lclass = (MimeLeafClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
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
  MimeInlineImageClass *clazz;

  int status;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  if (!obj->output_p) return 0;

  if (!obj->options || !obj->options->output_fn)
	return 0;

  clazz = (MimeInlineImageClass *) obj->clazz;

  if (obj->options &&
	  obj->options->image_begin &&
	  obj->options->write_html_p &&
	  obj->options->image_write_buffer)
	{
	  char *html, *part, *image_url;
	  const char *ct;

	  part = mime_part_address(obj);
	  if (!part) return MIME_OUT_OF_MEMORY;
	  image_url = mime_set_url_part(obj->options->url, part, PR_TRUE);
	  if (!image_url)
		{
		  PR_Free(part);
		  return MIME_OUT_OF_MEMORY;
		}
	  PR_Free(part);

	  ct = obj->content_type;
	  if (!ct) ct = IMAGE_GIF;  /* Can't happen?  Close enough. */

	  // Fill in content type and attachment name here.
	  nsCAutoString url_with_filename(image_url);
	  url_with_filename += "&type=";
	  url_with_filename += ct;
	  char * filename = MimeHeaders_get_name ( obj->headers, obj->options );
	  if (filename)
	  {
		  char *escapedName = nsEscape(filename, url_Path);
		  if (!escapedName) return MIME_OUT_OF_MEMORY;
			  url_with_filename += "&filename=";
			  url_with_filename += escapedName;
			  nsCRT::free(escapedName);
	  }

    // We need to separate images with HR's...
    MimeObject_write_separator(obj);

	  img->image_data =
		obj->options->image_begin(url_with_filename.get(), ct, obj->options->stream_closure);
	  PR_Free(image_url);

	  if (!img->image_data) return MIME_OUT_OF_MEMORY;

	  html = obj->options->make_image_html(img->image_data);
	  if (!html) return MIME_OUT_OF_MEMORY;

	  status = MimeObject_write(obj, html, nsCRT::strlen(html), PR_TRUE);
	  PR_Free(html);
	  if (status < 0) return status;
	}

  // 
  // Now we are going to see if we should set the content type in the
  // URI for the url being run...
  //
  if (obj->options && obj->options->stream_closure && obj->content_type)
  {
    mime_stream_data  *msd = (mime_stream_data *) (obj->options->stream_closure);
    if ( (msd) && (msd->channel) )
    {
      msd->channel->SetContentType(obj->content_type);
    }
  }

  return 0;
}


static int
MimeInlineImage_parse_eof (MimeObject *obj, PRBool abort_p)
{
  MimeInlineImage *img = (MimeInlineImage *) obj;
  int status;
  if (obj->closed_p) return 0;

  /* Force out any buffered data from the superclass (the base64 decoder.) */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) abort_p = PR_TRUE;

  if (img->image_data)
	{
	  obj->options->image_end(img->image_data,
							  (status < 0 ? status : (abort_p ? -1 : 0)));
	  img->image_data = 0;
	}

  return status;
}


static int
MimeInlineImage_parse_decoded_buffer (char *buf, PRInt32 size, MimeObject *obj)
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
		  PR_ASSERT(obj->options->state->first_data_written_p);
		}
	  
	  return MimeObject_write(obj, buf, size, PR_TRUE);
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
MimeInlineImage_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  /* This method should never be called (inline images do no line buffering).
   */
  PR_ASSERT(0);
  return -1;
}
