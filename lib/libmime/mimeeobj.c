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

/* mimeeobj.c --- definition of the MimeExternalObject class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#include "mimeeobj.h"
#include "xpgetstr.h"

#define MIME_SUPERCLASS mimeLeafClass
MimeDefClass(MimeExternalObject, MimeExternalObjectClass,
			 mimeExternalObjectClass, &MIME_SUPERCLASS);

extern int MK_MSG_ATTACHMENT;

#if defined(XP_MAC) && !defined(MOZILLA_30)
extern MimeObjectClass mimeMultipartAppleDoubleClass;
#endif

static int MimeExternalObject_initialize (MimeObject *);
static void MimeExternalObject_finalize (MimeObject *);
static int MimeExternalObject_parse_begin (MimeObject *);
static int MimeExternalObject_parse_buffer (char *, int32, MimeObject *);
static int MimeExternalObject_parse_line (char *, int32, MimeObject *);
static int MimeExternalObject_parse_decoded_buffer (char*, int32, MimeObject*);
static XP_Bool MimeExternalObject_displayable_inline_p (MimeObjectClass *class,
														MimeHeaders *hdrs);

static int
MimeExternalObjectClassInitialize(MimeExternalObjectClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;
  MimeLeafClass   *lclass = (MimeLeafClass *) class;

  XP_ASSERT(!oclass->class_initialized);
  oclass->initialize   = MimeExternalObject_initialize;
  oclass->finalize     = MimeExternalObject_finalize;
  oclass->parse_begin  = MimeExternalObject_parse_begin;
  oclass->parse_buffer = MimeExternalObject_parse_buffer;
  oclass->parse_line   = MimeExternalObject_parse_line;
  oclass->displayable_inline_p = MimeExternalObject_displayable_inline_p;
  lclass->parse_decoded_buffer = MimeExternalObject_parse_decoded_buffer;
  return 0;
}


static int
MimeExternalObject_initialize (MimeObject *object)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeExternalObject_finalize (MimeObject *object)
{
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}


static int
MimeExternalObject_parse_begin (MimeObject *obj)
{
  int status;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

#if defined (XP_MAC) && !defined(MOZILLA_30)
  if (obj->parent && mime_typep(obj->parent,
	  (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	  goto done;
#endif /* defined (XP_MAC) && !defined(MOZILLA_30) */

  /* If we're writing this object, and we're doing it in raw form, then
	 now is the time to inform the backend what the type of this data is.
   */
  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p &&
	  !obj->options->state->first_data_written_p)
	{
	  status = MimeObject_output_init(obj, 0);
	  if (status < 0) return status;
	  XP_ASSERT(obj->options->state->first_data_written_p);
	}


  /* If we're writing this object as HTML, do all the work now -- just write
	 out a table with a link in it.  (Later calls to the `parse_buffer' method
	 will simply discard the data of the object itself.)
   */
  if (obj->options &&
	  obj->output_p &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
	  MimeDisplayOptions newopt = *obj->options;  /* copy it */
	  char *id = 0;
	  char *id_url = 0;
	  char *id_name = 0;
	  char *id_imap = 0;
	  XP_Bool all_headers_p = obj->options->headers == MimeHeadersAll;

	  id = mime_part_address (obj);
	  if (obj->options->missing_parts)
		id_imap = mime_imap_part_address (obj);
	  if (! id) return MK_OUT_OF_MEMORY;

      if (obj->options && obj->options->url)
		{
		  const char *url = obj->options->url;
		  if (id_imap && id)
		  {
			/* if this is an IMAP part. */
			id_url = mime_set_url_imap_part(url, id_imap, id);
		  }
		  else
		  {
			/* This is just a normal MIME part as usual. */
			id_url = mime_set_url_part(url, id, TRUE);
		  }
		  if (!id_url)
			{
			  XP_FREE(id);
			  return MK_OUT_OF_MEMORY;
			}
		}

	  if (!XP_STRCMP (id, "0"))
		{
		  XP_FREE(id);
		  id = XP_STRDUP(XP_GetString(MK_MSG_ATTACHMENT));
		}
	  else
		{
		  const char *p = "Part ";  /* #### i18n */
		  char *s = (char *)XP_ALLOC(XP_STRLEN(p) + XP_STRLEN(id) + 1);
		  if (!s)
			{
			  XP_FREE(id);
			  XP_FREE(id_url);
			  return MK_OUT_OF_MEMORY;
			}
  		  /* we have a valid id */
		  if (id)
			  id_name = mime_find_suggested_name_of_part(id, obj);
		  XP_STRCPY(s, p);
		  XP_STRCAT(s, id);
		  XP_FREE(id);
		  id = s;
		}

	  if (all_headers_p &&
		  /* Don't bother showing all headers on this part if it's the only
			 part in the message: in that case, we've already shown these
			 headers. */
		  obj->options->state &&
		  obj->options->state->root == obj->parent)
		all_headers_p = FALSE;

	  newopt.fancy_headers_p = TRUE;
	  newopt.headers = (all_headers_p ? MimeHeadersAll : MimeHeadersSome);

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, FALSE);
		if (status < 0) goto FAIL;
	  }

	  status = MimeHeaders_write_attachment_box (obj->headers, &newopt,
												 obj->content_type,
												 obj->encoding,
												 id_name? id_name : id, id_url, 0);
	  FREEIF(id_name);
	  if (status < 0) goto FAIL;

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, FALSE);
		if (status < 0) goto FAIL;
	  }

	FAIL:
	  FREEIF(id);
	  FREEIF(id_url);
  	  FREEIF(id_name);
	  if (status < 0) return status;
	}

#if defined (XP_MAC) && !defined(MOZILLA_30)
done:
#endif /* defined (XP_MAC) && !defined(MOZILLA_30) */

  return 0;
}

static int
MimeExternalObject_parse_buffer (char *buffer, int32 size, MimeObject *obj)
{
  XP_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p)
	{
	  /* The data will be base64-decoded and passed to
		 MimeExternalObject_parse_decoded_buffer. */
	  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_buffer(buffer, size,
																obj);
	}
  else
	{
	  /* Otherwise, simply ignore the data. */
	  return 0;
	}
}


static int
MimeExternalObject_parse_decoded_buffer (char *buf, int32 size,
										 MimeObject *obj)
{
  /* This is called (by MimeLeafClass->parse_buffer) with blocks of data
	 that have already been base64-decoded.  This will only be called in
	 the case where we're not emitting HTML, and want access to the raw
	 data itself.

	 We override the `parse_decoded_buffer' method provided by MimeLeaf
	 because, unlike most children of MimeLeaf, we do not want to line-
	 buffer the decoded data -- we want to simply pass it along to the
	 backend, without going through our `parse_line' method.
   */
  if (!obj->output_p ||
	  !obj->options ||
	  obj->options->write_html_p)
	{
	  XP_ASSERT(0);
	  return -1;
	}

  return MimeObject_write(obj, buf, size, TRUE);
}


static int
MimeExternalObject_parse_line (char *line, int32 length, MimeObject *obj)
{
  /* This method should never be called (externals do no line buffering).
   */
  XP_ASSERT(0);
  return -1;
}

static XP_Bool
MimeExternalObject_displayable_inline_p (MimeObjectClass *class,
										 MimeHeaders *hdrs)
{
  return FALSE;
}
