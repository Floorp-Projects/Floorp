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

/* mimemult.c --- definition of the MimeMultipart class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "mimemult.h"
#include "mime.h"

#define MIME_SUPERCLASS mimeContainerClass
MimeDefClass(MimeMultipart, MimeMultipartClass,
			 mimeMultipartClass, &MIME_SUPERCLASS);

static int MimeMultipart_initialize (MimeObject *);
static void MimeMultipart_finalize (MimeObject *);
static int MimeMultipart_parse_line (char *line, int32 length, MimeObject *);
static int MimeMultipart_parse_eof (MimeObject *object, XP_Bool abort_p);

static MimeMultipartBoundaryType MimeMultipart_check_boundary(MimeObject *,
															  const char *,
															  int32);
static int MimeMultipart_create_child(MimeObject *);
static XP_Bool MimeMultipart_output_child_p(MimeObject *, MimeObject *);
static int MimeMultipart_parse_child_line (MimeObject *, char *, int32,
										   XP_Bool);
static int MimeMultipart_close_child(MimeObject *);

extern MimeObjectClass mimeMultipartAlternativeClass;
extern MimeObjectClass mimeMultipartRelatedClass;
extern MimeObjectClass mimeMultipartSignedClass;

#if defined(DEBUG) && defined(XP_UNIX)
static int MimeMultipart_debug_print (MimeObject *, FILE *, int32);
#endif

static int
MimeMultipartClassInitialize(MimeMultipartClass *class)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    class;
  MimeMultipartClass *mclass = (MimeMultipartClass *) class;

  XP_ASSERT(!oclass->class_initialized);
  oclass->initialize  = MimeMultipart_initialize;
  oclass->finalize    = MimeMultipart_finalize;
  oclass->parse_line  = MimeMultipart_parse_line;
  oclass->parse_eof   = MimeMultipart_parse_eof;

  mclass->check_boundary   = MimeMultipart_check_boundary;
  mclass->create_child     = MimeMultipart_create_child;
  mclass->output_child_p   = MimeMultipart_output_child_p;
  mclass->parse_child_line = MimeMultipart_parse_child_line;
  mclass->close_child      = MimeMultipart_close_child;

#if defined(DEBUG) && defined(XP_UNIX)
  oclass->debug_print = MimeMultipart_debug_print;
#endif

  return 0;
}


static int
MimeMultipart_initialize (MimeObject *object)
{
  MimeMultipart *mult = (MimeMultipart *) object;
  char *ct;

  /* This is an abstract class; it shouldn't be directly instanciated. */
  XP_ASSERT(object->class != (MimeObjectClass *) &mimeMultipartClass);

  ct = MimeHeaders_get (object->headers, HEADER_CONTENT_TYPE, FALSE, FALSE);
  mult->boundary = (ct
					? MimeHeaders_get_parameter (ct, HEADER_PARM_BOUNDARY, NULL, NULL)
					: 0);
  FREEIF(ct);
  mult->state = MimeMultipartPreamble;
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}


static void
MimeMultipart_finalize (MimeObject *object)
{
  MimeMultipart *mult = (MimeMultipart *) object;

  object->class->parse_eof(object, FALSE);

  FREEIF(mult->boundary);
  if (mult->hdrs)
	MimeHeaders_free(mult->hdrs);
  mult->hdrs = 0;
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}


static int
MimeMultipart_parse_line (char *line, int32 length, MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  int status = 0;
  MimeMultipartBoundaryType boundary;

  XP_ASSERT(line && *line);
  if (!line || !*line) return -1;

  XP_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p &&
	  obj->options->output_fn)
	return MimeObject_write(obj, line, length, TRUE);


  if (mult->state == MimeMultipartEpilogue)  /* already done */
	boundary = MimeMultipartBoundaryTypeNone;
  else
	boundary = ((MimeMultipartClass *)obj->class)->check_boundary(obj, line,
																  length);

  if (boundary == MimeMultipartBoundaryTypeTerminator ||
	  boundary == MimeMultipartBoundaryTypeSeparator)
	{
	  /* Match!  Close the currently-open part, move on to the next
		 state, and discard this line.
	   */
	  if (mult->state != MimeMultipartPreamble)
		status = ((MimeMultipartClass *)obj->class)->close_child(obj);
	  if (status < 0) return status;

	  if (boundary == MimeMultipartBoundaryTypeTerminator)
		mult->state = MimeMultipartEpilogue;
	  else
		{
		  mult->state = MimeMultipartHeaders;

		  /* Reset the header parser for this upcoming part. */
		  XP_ASSERT(!mult->hdrs);
		  if (mult->hdrs)
			MimeHeaders_free(mult->hdrs);
		  mult->hdrs = MimeHeaders_new();
		  if (!mult->hdrs)
			return MK_OUT_OF_MEMORY;
		}

	  /* Now return, to ignore the boundary line itself. */
	  return 0;
	}

  /* Otherwise, this isn't a boundary string.  So do whatever it is we
	 should do with this line (parse it as a header, feed it to the
	 child part, ignore it, etc.) */

  switch (mult->state)
	{
	case MimeMultipartPreamble:
	case MimeMultipartEpilogue:
	  /* Ignore this line. */
	  break;

	case MimeMultipartHeaders:
	  /* Parse this line as a header for the sub-part. */
	  {
		status = MimeHeaders_parse_line(line, length, mult->hdrs);
		if (status < 0) return status;

		/* If this line is blank, we're now done parsing headers, and should
		   now examine the content-type to create this "body" part.
		 */
		if (*line == CR || *line == LF)
		  {
			status = ((MimeMultipartClass *) obj->class)->create_child(obj);
			if (status < 0) return status;
			XP_ASSERT(mult->state != MimeMultipartHeaders);
		  }
		break;
	  }

	case MimeMultipartPartFirstLine:
	  /* Hand this line off to the sub-part. */
	  status = (((MimeMultipartClass *) obj->class)->parse_child_line(obj,
																	  line,
																	  length,
																	  TRUE));
	  if (status < 0) return status;
	  mult->state = MimeMultipartPartLine;
	  break;

	case MimeMultipartPartLine:
	  /* Hand this line off to the sub-part. */
	  status = (((MimeMultipartClass *) obj->class)->parse_child_line(obj,
																	  line,
																	  length,
																	  FALSE));
	  if (status < 0) return status;
	  break;

	default:
	  XP_ASSERT(0);
	  return -1;
	}

  return 0;
}


static MimeMultipartBoundaryType
MimeMultipart_check_boundary(MimeObject *obj, const char *line, int32 length)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  int32 blen;
  XP_Bool term_p;

  if (!mult->boundary ||
	  line[0] != '-' ||
	  line[1] != '-')
	return MimeMultipartBoundaryTypeNone;

  /* This is a candidate line to be a boundary.  Check it out... */
  blen = XP_STRLEN(mult->boundary);
  term_p = FALSE;

  /* strip trailing whitespace (including the newline.) */
  while(length > 2 && XP_IS_SPACE(line[length-1]))
	length--;

  /* Could this be a terminating boundary? */
  if (length == blen + 4 &&
	  line[length-1] == '-' &&
	  line[length-2] == '-')
	{
	  term_p = TRUE;
	  length -= 2;
	}

  if (blen == length-2 && !XP_STRNCMP(line+2, mult->boundary, length-2))
	return (term_p
			? MimeMultipartBoundaryTypeTerminator
			: MimeMultipartBoundaryTypeSeparator);
  else
	return MimeMultipartBoundaryTypeNone;
}



static int
MimeMultipart_create_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
#ifdef JS_ATTACHMENT_MUMBO_JUMBO
  MimeContainer *cont = (MimeContainer *) obj;
#endif
  int status;
  char *ct = (mult->hdrs
			  ? MimeHeaders_get (mult->hdrs, HEADER_CONTENT_TYPE,
								 TRUE, FALSE)
			  : 0);
  const char *dct = (((MimeMultipartClass *) obj->class)->default_part_type);
  MimeObject *body = NULL;
  MimeObject *parent = NULL;
  XP_Bool showIcon = TRUE;

  mult->state = MimeMultipartPartFirstLine;
  /* Don't pass in NULL as the content-type (this means that the
	 auto-uudecode-hack won't ever be done for subparts of a
	 multipart, but only for untyped children of message/rfc822.
   */
  body = mime_create(((ct && *ct) ? ct : (dct ? dct: TEXT_PLAIN)),
					 mult->hdrs, obj->options);
  FREEIF(ct);
  if (!body) return MK_OUT_OF_MEMORY;
  status = ((MimeContainerClass *) obj->class)->add_child(obj, body);
  if (status < 0)
	{
	  mime_free(body);
	  return status;
	}

#ifdef MIME_DRAFTS
  if ( obj->options && 
	   obj->options->decompose_file_p &&
	   obj->options->is_multipart_msg &&
	   obj->options->decompose_file_init_fn )
	{
	  if ( !mime_typep(obj,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
           !mime_typep(obj,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		   !mime_typep(obj,(MimeObjectClass*)&mimeMultipartSignedClass) &&
		   !mime_typep(body, (MimeObjectClass*)&mimeMultipartRelatedClass) &&
		   !mime_typep(body, (MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		   !mime_typep(body,(MimeObjectClass*)&mimeMultipartSignedClass) )
	  {
		status = obj->options->decompose_file_init_fn ( obj->options->stream_closure, mult->hdrs );
		if (status < 0) return status;
	  }
	}
#endif /* MIME_DRAFTS */


  /* Now that we've added this new object to our list of children,
	 start its parser going (if we want to display it.)
   */
  body->output_p = (((MimeMultipartClass *) obj->class)
					->output_child_p(obj, body));
  if (body->output_p)
	{
#ifdef JS_ATTACHMENT_MUMBO_JUMBO
	  int attachment_count = 0;
	  XP_Bool isMsgBody = FALSE, isAlternativeOrRelated = FALSE;
#endif
	  status = body->class->parse_begin(body);
	  if (status < 0) return status;
#ifdef JS_ATTACHMENT_MUMBO_JUMBO
	  isMsgBody = MimeObjectChildIsMessageBody
		                        (obj, &isAlternativeOrRelated); 
	  if (isAlternativeOrRelated)
		  attachment_count = 0;
	  else if (isMsgBody)
		  attachment_count = cont->nchildren - 1;
	  else
		  attachment_count = cont->nchildren;

	  if (attachment_count &&
		  obj->options && !obj->options->nice_html_only_p &&
		  obj->options->attachment_icon_layer_id) {
		/* This is not the first child, so it's an attachment.  Cause the
		   "attachments in this message" icon(s) to become visible.
		   Excluding the following types to avoid inline graphics and dull items :
		   Headers: Content-Disposition: inline
		   Content-Type: text/x-vcard
		   Content-Type: text/html
		   Content-Type: text/plain
		   Content-Type: message/rfc822    */
		  char *tmp = NULL;

		  /* if (strncasestr(body->headers->all_headers, "DISPOSITION: INLINE", 300))
		  	showIcon = FALSE; */
		  if (XP_STRSTR(body->content_type, "text/x-vcard"))
			  showIcon = FALSE;
		  else if (XP_STRSTR(body->content_type, "text/html"))
			  showIcon = FALSE;
		  else if (XP_STRSTR(body->content_type, "message/rfc822"))
			  showIcon = FALSE;
		  else if (XP_STRSTR(body->content_type, "multipart/signed"))
			  showIcon = FALSE;
		  else if (XP_STRSTR(body->content_type, "application/x-pkcs7-signature"))
			  showIcon = FALSE;
		  else if (XP_STRSTR(body->content_type, "multipart/mixed"))
			  showIcon = FALSE;

		  if (showIcon)
		  {
			  (obj->class)->showAttachmentIcon = TRUE;
			  parent = obj->parent;
			  while (parent) {
				  (parent->class)->showAttachmentIcon = TRUE;
				  parent = parent->parent;
			  }
		      tmp = PR_smprintf("\n\
<SCRIPT>\n\
window.document.layers[\"noattach-%ld\"].visibility = \"hide\";\n\
window.document.layers[\"attach-%ld\"].visibility = \"show\";\n\
</SCRIPT>\n",
								(long) obj->options->attachment_icon_layer_id,
								(long) obj->options->attachment_icon_layer_id);
		  }
		  if (tmp) {
			status = MimeObject_write(obj, tmp, XP_STRLEN(tmp), TRUE);
			XP_FREE(tmp);
			if (status < 0)
				return status;
		  }
	  }
#endif /* JS_ATTACHMENT_MUMBO_JUMBO */
	}

  return 0;
}


static XP_Bool
MimeMultipart_output_child_p(MimeObject *obj, MimeObject *child)
{
  return TRUE;
}



static int
MimeMultipart_close_child(MimeObject *object)
{
  MimeMultipart *mult = (MimeMultipart *) object;
  MimeContainer *cont = (MimeContainer *) object;

  if (!mult->hdrs)
	return 0;

  MimeHeaders_free(mult->hdrs);
  mult->hdrs = 0;

  XP_ASSERT(cont->nchildren > 0);
  if (cont->nchildren > 0)
	{
	  MimeObject *kid = cont->children[cont->nchildren-1];
	  if (kid)
		{
		  int status;
		  status = kid->class->parse_eof(kid, FALSE);
		  if (status < 0) return status;
		  status = kid->class->parse_end(kid, FALSE);
		  if (status < 0) return status;

#ifdef MIME_DRAFTS
		  if ( object->options &&
			   object->options->decompose_file_p &&
			   object->options->is_multipart_msg &&
			   object->options->decompose_file_close_fn ) 
		  {
			  if ( !mime_typep(object,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
				   !mime_typep(object,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
				   !mime_typep(object,(MimeObjectClass*)&mimeMultipartSignedClass) &&
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartSignedClass) )
				{
					status = object->options->decompose_file_close_fn ( object->options->stream_closure );
					if (status < 0) return status;
				}
		  }
#endif /* MIME_DRAFTS */

		}
	}
  return 0;
}


static int
MimeMultipart_parse_child_line (MimeObject *obj, char *line, int32 length,
								XP_Bool first_line_p)
{
  MimeContainer *cont = (MimeContainer *) obj;
  int status;
  MimeObject *kid;

  XP_ASSERT(cont->nchildren > 0);
  if (cont->nchildren <= 0)
	return -1;

  kid = cont->children[cont->nchildren-1];
  XP_ASSERT(kid);
  if (!kid) return -1;

#ifdef MIME_DRAFTS
  if ( obj->options &&
	   obj->options->decompose_file_p &&
	   obj->options->is_multipart_msg && 
	   obj->options->decompose_file_output_fn ) 
  {
	if (!mime_typep(obj,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		!mime_typep(obj,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
		!mime_typep(obj,(MimeObjectClass*)&mimeMultipartSignedClass) &&
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartSignedClass) )
		return obj->options->decompose_file_output_fn (line, length, obj->options->stream_closure);
  }
#endif /* MIME_DRAFTS */

  /* The newline issues here are tricky, since both the newlines before
	 and after the boundary string are to be considered part of the
	 boundary: this is so that a part can be specified such that it
	 does not end in a trailing newline.

	 To implement this, we send a newline *before* each line instead
	 of after, except for the first line, which is not preceeded by a
	 newline.
   */

  /* Remove the trailing newline... */
  if (length > 0 && line[length-1] == LF) length--;
  if (length > 0 && line[length-1] == CR) length--;

  if (!first_line_p)
	{
	  /* Push out a preceeding newline... */
	  char nl[] = LINEBREAK;
	  status = kid->class->parse_buffer (nl, LINEBREAK_LEN, kid);
	  if (status < 0) return status;
	}

  /* Now push out the line sans trailing newline. */
  return kid->class->parse_buffer (line, length, kid);
}


static int
MimeMultipart_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeContainer *cont = (MimeContainer *) obj;

  if (obj->closed_p) return 0;

  /* Push out one last newline if part of the last line is still in the
	 ibuffer.  If this happens, this object does not end in a trailing newline
	 (and the parse_line method will be called with a string with no trailing
	 newline, which isn't the usual case.)
   */
  if (!abort_p && obj->ibuffer_fp > 0)
	{
	  int status = obj->class->parse_buffer (obj->ibuffer, obj->ibuffer_fp,
											 obj);
	  obj->ibuffer_fp = 0;
	  if (status < 0)
		{
		  obj->closed_p = TRUE;
		  return status;
		}
	}

  /* Now call parse_eof for our active child, if there is one.
   */
  if (cont->nchildren > 0 &&
	  (mult->state == MimeMultipartPartLine ||
	   mult->state == MimeMultipartPartFirstLine))
	{
	  MimeObject *kid = cont->children[cont->nchildren-1];
	  XP_ASSERT(kid);
	  if (kid)
		{
		  int status = kid->class->parse_eof(kid, abort_p);
		  if (status < 0) return status;
		}
	}

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
}


#if defined(DEBUG) && defined(XP_UNIX)
static int
MimeMultipart_debug_print (MimeObject *obj, FILE *stream, int32 depth)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeContainer *cont = (MimeContainer *) obj;
  char *addr = mime_part_address(obj);
  int i;
  for (i=0; i < depth; i++)
	fprintf(stream, "  ");
  fprintf(stream, "<%s %s (%d kid%s) boundary=%s 0x%08X>\n",
		  obj->class->class_name,
		  addr ? addr : "???",
		  cont->nchildren, (cont->nchildren == 1 ? "" : "s"),
		  (mult->boundary ? mult->boundary : "(none)"),
		  (uint32) mult);
  FREEIF(addr);

/*
  if (cont->nchildren > 0)
	fprintf(stream, "\n");
 */

  for (i = 0; i < cont->nchildren; i++)
	{
	  MimeObject *kid = cont->children[i];
	  int status = kid->class->debug_print (kid, stream, depth+1);
	  if (status < 0) return status;
	}

/*
  if (cont->nchildren > 0)
	fprintf(stream, "\n");
 */

  return 0;
}
#endif
