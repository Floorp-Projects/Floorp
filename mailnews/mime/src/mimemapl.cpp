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

#include "mimemapl.h"
#include "prmem.h"
#include "plstr.h"
#include "nsMimeTransition.h"

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeMultipartAppleDouble, MimeMultipartAppleDoubleClass,
			 mimeMultipartAppleDoubleClass, &MIME_SUPERCLASS);

extern "C" int MK_MSG_ATTACHMENT;

static int MimeMultipartAppleDouble_parse_begin (MimeObject *);
static PRBool MimeMultipartAppleDouble_output_child_p(MimeObject *,
													   MimeObject *);

static int
MimeMultipartAppleDoubleClassInitialize(MimeMultipartAppleDoubleClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin    = MimeMultipartAppleDouble_parse_begin;
  mclass->output_child_p = MimeMultipartAppleDouble_output_child_p;
  return 0;
}

static int
MimeMultipartAppleDouble_parse_begin (MimeObject *obj)
{
  /* #### This method is identical to MimeExternalObject_parse_begin
	 which kinda s#$%s...
   */
  int status;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

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

#ifdef XP_MAC
  if (obj->options && obj->options->state) 
  {
	obj->options->state->separator_suppressed_p = PR_TRUE;
	goto done;
  }
  /*
   * It would be nice to not showing the resource fork links
   * if we are displaying inline. But, there is no way we could
   * know ahead of time that we could display the data fork and
   * the data fork is always hidden on MAC platform.
   */
#endif
  /* If we're writing this object as HTML, then emit a link for the
	 multipart/appledouble part (both links) that looks just like the
	 links that MimeExternalObject emits for external leaf parts.
   */
  if (obj->options &&
	  obj->output_p &&
	  obj->options->write_html_p &&
	  obj->options->output_fn)
	{
	  MimeDisplayOptions newopt = *obj->options;  /* copy it */
	  char *id = 0;
	  char *id_url = 0;
	  char *id_imap = 0;
	  PRBool all_headers_p = obj->options->headers == MimeHeadersAll;

	  id = mime_part_address (obj);
	  if (! id) return MK_OUT_OF_MEMORY;
	  if (obj->options->missing_parts)
		id_imap = mime_imap_part_address (obj);

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
												 id, id_url, 0);
	  if (status < 0) goto FAIL;

	  /* No <P> after the first attachment-box in an AppleDouble, to keep
		 them closer together. */

	FAIL:
	  PR_FREEIF(id);
	  PR_FREEIF(id_url);
	  if (status < 0) return status;
	}

#ifdef XP_MAC
done:
#endif

  return 0;
}

static PRBool
MimeMultipartAppleDouble_output_child_p(MimeObject *obj, MimeObject *child)
{
  MimeContainer *cont = (MimeContainer *) obj;

  /* If this is the first child, and it's an application/applefile, then
	 don't emit a link for it.  (There *should* be only two children, and
	 the first one should always be an application/applefile.)
   */

  if (obj->output_p &&
	  obj->options &&
	  obj->options->write_html_p &&
	  cont->nchildren >= 1 &&
	  cont->children[0] == child &&
	  child->content_type &&
	  !PL_strcasecmp(child->content_type, APPLICATION_APPLEFILE))
	return PR_FALSE;
  else
	return PR_TRUE;
}
