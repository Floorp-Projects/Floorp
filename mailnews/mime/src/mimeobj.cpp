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

#include "mimeobj.h"
#include "prmem.h"
#include "plstr.h"
#include "mimebuf.h"

/* Way to destroy any notions of modularity or class hierarchy, Terry! */
# include "mimetpla.h"
# include "mimethtm.h"
# include "mimecont.h"

MimeDefClass (MimeObject, MimeObjectClass, mimeObjectClass, NULL);

static int MimeObject_initialize (MimeObject *);
static void MimeObject_finalize (MimeObject *);
static int MimeObject_parse_begin (MimeObject *);
static int MimeObject_parse_buffer (char *, PRInt32, MimeObject *);
static int MimeObject_parse_line (char *, PRInt32, MimeObject *);
static int MimeObject_parse_eof (MimeObject *, PRBool);
static int MimeObject_parse_end (MimeObject *, PRBool);
static PRBool MimeObject_displayable_inline_p (MimeObjectClass *clazz,
												MimeHeaders *hdrs);

#if defined(DEBUG) && defined(XP_UNIX)
static int MimeObject_debug_print (MimeObject *, FILE *, PRInt32 depth);
#endif

static int
MimeObjectClassInitialize(MimeObjectClass *clazz)
{
  PR_ASSERT(!clazz->class_initialized);
  clazz->initialize   = MimeObject_initialize;
  clazz->finalize     = MimeObject_finalize;
  clazz->parse_begin  = MimeObject_parse_begin;
  clazz->parse_buffer = MimeObject_parse_buffer;
  clazz->parse_line   = MimeObject_parse_line;
  clazz->parse_eof    = MimeObject_parse_eof;
  clazz->parse_end    = MimeObject_parse_end;
  clazz->displayable_inline_p = MimeObject_displayable_inline_p;

#if defined(DEBUG) && defined(XP_UNIX)
  clazz->debug_print  = MimeObject_debug_print;
#endif
  return 0;
}


static int
MimeObject_initialize (MimeObject *obj)
{
  /* This is an abstract class; it shouldn't be directly instanciated. */
  PR_ASSERT(obj->clazz != &mimeObjectClass);

  /* Set up the content-type and encoding. */
  if (!obj->content_type && obj->headers)
	obj->content_type = MimeHeaders_get (obj->headers, HEADER_CONTENT_TYPE,
										 PR_TRUE, PR_FALSE);
  if (!obj->encoding && obj->headers)
	obj->encoding = MimeHeaders_get (obj->headers,
									 HEADER_CONTENT_TRANSFER_ENCODING,
									 PR_TRUE, PR_FALSE);


  /* Special case to normalize some types and encodings to a canonical form.
	 (These are nonstandard types/encodings which have been seen to appear in
	 multiple forms; we normalize them so that things like looking up icons
	 and extensions has consistent behavior for the receiver, regardless of
	 the "alias" type that the sender used.)
   */
  if (!obj->content_type)
	;
  else if (!PL_strcasecmp(obj->content_type, APPLICATION_UUENCODE2) ||
		   !PL_strcasecmp(obj->content_type, APPLICATION_UUENCODE3) ||
		   !PL_strcasecmp(obj->content_type, APPLICATION_UUENCODE4))
	{
	  PR_Free(obj->content_type);
	  obj->content_type = PL_strdup(APPLICATION_UUENCODE);
	}
  else if (!PL_strcasecmp(obj->content_type, IMAGE_XBM2) ||
		   !PL_strcasecmp(obj->content_type, IMAGE_XBM3))
	{
	  PR_Free(obj->content_type);
	  obj->content_type = PL_strdup(IMAGE_XBM);
	}

  if (!obj->encoding)
	;
  else if (!PL_strcasecmp(obj->encoding, ENCODING_UUENCODE2) ||
		   !PL_strcasecmp(obj->encoding, ENCODING_UUENCODE3) ||
		   !PL_strcasecmp(obj->encoding, ENCODING_UUENCODE4))
	{
	  PR_Free(obj->encoding);
	  obj->encoding = PL_strdup(ENCODING_UUENCODE);
	}
  else if (!PL_strcasecmp(obj->encoding, ENCODING_COMPRESS2))
	{
	  PR_Free(obj->encoding);
	  obj->encoding = PL_strdup(ENCODING_COMPRESS);
	}
  else if (!PL_strcasecmp(obj->encoding, ENCODING_GZIP2))
	{
	  PR_Free(obj->encoding);
	  obj->encoding = PL_strdup(ENCODING_GZIP);
	}


  return 0;
}

static void
MimeObject_finalize (MimeObject *obj)
{
  obj->clazz->parse_eof (obj, PR_FALSE);
  obj->clazz->parse_end (obj, PR_FALSE);

  if (obj->headers)
	{
	  MimeHeaders_free(obj->headers);
	  obj->headers = 0;
	}

  /* Should have been freed by parse_eof, but just in case... */
  PR_ASSERT(!obj->ibuffer);
  PR_ASSERT(!obj->obuffer);
  PR_FREEIF (obj->ibuffer);
  PR_FREEIF (obj->obuffer);

  PR_FREEIF(obj->content_type);
  PR_FREEIF(obj->encoding);

  if (obj->options && obj->options->state)
	{
	  PR_Free(obj->options->state);
	  obj->options->state = 0;
	}
}


static int
MimeObject_parse_begin (MimeObject *obj)
{
  PR_ASSERT (!obj->closed_p);

  /* If we haven't set up the state object yet, then this should be
	 the outermost object... */
  if (obj->options && !obj->options->state)
	{
	  PR_ASSERT(!obj->headers);  /* should be the outermost object. */

	  obj->options->state = PR_NEW(MimeParseStateObject);
	  if (!obj->options->state) return MK_OUT_OF_MEMORY;
	  memset(obj->options->state, 0, sizeof(*obj->options->state));
	  obj->options->state->root = obj;
	  obj->options->state->separator_suppressed_p = PR_TRUE; /* no first sep */
	}

  /* Decide whether this object should be output or not... */
  if (!obj->options || !obj->options->output_fn)
	obj->output_p = PR_FALSE;
  else if (!obj->options->part_to_load)
	obj->output_p = PR_TRUE;
  else
	{
	  char *id = mime_part_address(obj);
	  if (!id) return MK_OUT_OF_MEMORY;
	  obj->output_p = !PL_strcmp(id, obj->options->part_to_load);
	  PR_Free(id);
	}

/* Way to destroy any notions of modularity or class hierarchy, Terry! */
  if (obj->options && obj->options->nice_html_only_p) {
	  if (!mime_subclass_p(obj->clazz,
						   (MimeObjectClass*) &mimeInlineTextHTMLClass) &&
		  !mime_subclass_p(obj->clazz,
						   (MimeObjectClass*) &mimeInlineTextPlainClass) &&
		  !mime_subclass_p(obj->clazz,
						   (MimeObjectClass*) &mimeContainerClass)) {
		  obj->output_p = PR_FALSE;
	  }
  }

  return 0;
}

static int
MimeObject_parse_buffer (char *buffer, PRInt32 size, MimeObject *obj)
{
  PR_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  return mime_LineBuffer (buffer, size,
						 &obj->ibuffer, &obj->ibuffer_size, &obj->ibuffer_fp,
						 PR_TRUE,
						 ((int (*) (char *, PRInt32, void *))
						  /* This cast is to turn void into MimeObject */
						  obj->clazz->parse_line),
						 obj);
}


static int
MimeObject_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  /* This method should never be called. */
  PR_ASSERT(0);
  return -1;
}

static int
MimeObject_parse_eof (MimeObject *obj, PRBool abort_p)
{
  if (obj->closed_p) return 0;
  PR_ASSERT(!obj->parsed_p);

  /* If there is still data in the ibuffer, that means that the last line of
	 this part didn't end in a newline; so push it out anyway (this means that
	 the parse_line method will be called with a string with no trailing
	 newline, which isn't the usual case.)
   */
  if (!abort_p &&
	  obj->ibuffer_fp > 0)
	{
	  int status = obj->clazz->parse_line (obj->ibuffer, obj->ibuffer_fp, obj);
	  obj->ibuffer_fp = 0;
	  if (status < 0)
		{
		  obj->closed_p = PR_TRUE;
		  return status;
		}
	}

  obj->closed_p = PR_TRUE;
  return 0;
}


static int
MimeObject_parse_end (MimeObject *obj, PRBool abort_p)
{
  if (obj->parsed_p)
	{
	  PR_ASSERT(obj->closed_p);
	  return 0;
	}

  /* We won't be needing these buffers any more; nuke 'em. */
  PR_FREEIF(obj->ibuffer);
  obj->ibuffer_fp = 0;
  obj->ibuffer_size = 0;
  PR_FREEIF(obj->obuffer);
  obj->obuffer_fp = 0;
  obj->obuffer_size = 0;

  obj->parsed_p = PR_TRUE;
  return 0;
}


static PRBool
MimeObject_displayable_inline_p (MimeObjectClass *clazz, MimeHeaders *hdrs)
{
  PR_ASSERT(0);  /* This method should never be called. */
  return PR_FALSE;
}



#if defined(DEBUG) && defined(XP_UNIX)
static int
MimeObject_debug_print (MimeObject *obj, FILE *stream, PRInt32 depth)
{
  int i;
  char *addr = mime_part_address(obj);
  for (i=0; i < depth; i++)
	fprintf(stream, "  ");
  fprintf(stream, "<%s %s 0x%08X>\n", obj->clazz->class_name,
		  addr ? addr : "???",
		  (PRUint32) obj);
  PR_FREEIF(addr);
  return 0;
}
#endif
