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

#include "mimeeobj.h"
#include "xpgetstr.h"

#include "prmem.h"
#include "plstr.h"

#define MIME_SUPERCLASS mimeLeafClass
MimeDefClass(MimeExternalObject, MimeExternalObjectClass,
			 mimeExternalObjectClass, &MIME_SUPERCLASS);

extern "C" int MK_MSG_ATTACHMENT;

#ifdef XP_MAC
extern MimeObjectClass mimeMultipartAppleDoubleClass;
#endif

static int MimeExternalObject_initialize (MimeObject *);
static void MimeExternalObject_finalize (MimeObject *);
static int MimeExternalObject_parse_begin (MimeObject *);
static int MimeExternalObject_parse_buffer (char *, PRInt32, MimeObject *);
static int MimeExternalObject_parse_line (char *, PRInt32, MimeObject *);
static int MimeExternalObject_parse_decoded_buffer (char*, PRInt32, MimeObject*);
static PRBool MimeExternalObject_displayable_inline_p (MimeObjectClass *clazz,
														MimeHeaders *hdrs);

static int
MimeExternalObjectClassInitialize(MimeExternalObjectClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  MimeLeafClass   *lclass = (MimeLeafClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
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

#ifdef XP_MAC
  if (obj->parent && mime_typep(obj->parent,
	  (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
	  goto done;
#endif /* XP_MAC */

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
	  PR_ASSERT(obj->options->state->first_data_written_p);
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
	  PRBool all_headers_p = obj->options->headers == MimeHeadersAll;

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
			id_url = mime_set_url_part(url, id, PR_TRUE);
		  }
		  if (!id_url)
			{
			  PR_Free(id);
			  return MK_OUT_OF_MEMORY;
			}
		}

	  if (!PL_strcmp (id, "0"))
		{
		  PR_Free(id);
		  id = PL_strdup(XP_GetString(MK_MSG_ATTACHMENT));
		}
	  else
		{
		  const char *p = "Part ";  /* #### i18n */
		  char *s = (char *)PR_MALLOC(PL_strlen(p) + PL_strlen(id) + 1);
		  if (!s)
			{
			  PR_Free(id);
			  PR_Free(id_url);
			  return MK_OUT_OF_MEMORY;
			}
  		  /* we have a valid id */
		  if (id)
			  id_name = mime_find_suggested_name_of_part(id, obj);
		  PL_strcpy(s, p);
		  PL_strcat(s, id);
		  PR_Free(id);
		  id = s;
		}

	  if (all_headers_p &&
		  /* Don't bother showing all headers on this part if it's the only
			 part in the message: in that case, we've already shown these
			 headers. */
		  obj->options->state &&
		  obj->options->state->root == obj->parent)
		all_headers_p = PR_FALSE;

	  newopt.fancy_headers_p = PR_TRUE;
	  newopt.headers = (all_headers_p ? MimeHeadersAll : MimeHeadersSome);

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, PR_FALSE);
		if (status < 0) goto FAIL;
	  }

	  status = MimeHeaders_write_attachment_box (obj->headers, &newopt,
												 obj->content_type,
												 obj->encoding,
												 id_name? id_name : id, id_url, 0);
	  PR_FREEIF(id_name);
	  if (status < 0) goto FAIL;

	  {
		char p[] = "<P>";
		status = MimeObject_write(obj, p, 3, PR_FALSE);
		if (status < 0) goto FAIL;
	  }

	FAIL:
	  PR_FREEIF(id);
	  PR_FREEIF(id_url);
  	  PR_FREEIF(id_name);
	  if (status < 0) return status;
	}

#ifdef XP_MAC
done:
#endif /* XP_MAC */

  return 0;
}

static int
MimeExternalObject_parse_buffer (char *buffer, PRInt32 size, MimeObject *obj)
{
  PR_ASSERT(!obj->closed_p);
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
MimeExternalObject_parse_decoded_buffer (char *buf, PRInt32 size,
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
	  PR_ASSERT(0);
	  return -1;
	}

  return MimeObject_write(obj, buf, size, PR_TRUE);
}


static int
MimeExternalObject_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  /* This method should never be called (externals do no line buffering).
   */
  PR_ASSERT(0);
  return -1;
}

static PRBool
MimeExternalObject_displayable_inline_p (MimeObjectClass *clazz,
										 MimeHeaders *hdrs)
{
  return PR_FALSE;
}
