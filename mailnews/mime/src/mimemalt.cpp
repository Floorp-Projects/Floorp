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

#include "mimemalt.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsMimeTypes.h"
#include "nsMimeStringResources.h"

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeMultipartAlternative, MimeMultipartAlternativeClass,
			 mimeMultipartAlternativeClass, &MIME_SUPERCLASS);

static int MimeMultipartAlternative_initialize (MimeObject *);
static void MimeMultipartAlternative_finalize (MimeObject *);
static int MimeMultipartAlternative_parse_eof (MimeObject *, PRBool);
static int MimeMultipartAlternative_create_child(MimeObject *);
static int MimeMultipartAlternative_parse_child_line (MimeObject *, char *,
													  PRInt32, PRBool);
static int MimeMultipartAlternative_close_child(MimeObject *);

static PRBool MimeMultipartAlternative_display_part_p(MimeObject *self,
													   MimeHeaders *sub_hdrs);
static int MimeMultipartAlternative_discard_cached_part(MimeObject *);
static int MimeMultipartAlternative_display_cached_part(MimeObject *);

static int
MimeMultipartAlternativeClassInitialize(MimeMultipartAlternativeClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize       = MimeMultipartAlternative_initialize;
  oclass->finalize         = MimeMultipartAlternative_finalize;
  oclass->parse_eof        = MimeMultipartAlternative_parse_eof;
  mclass->create_child     = MimeMultipartAlternative_create_child;
  mclass->parse_child_line = MimeMultipartAlternative_parse_child_line;
  mclass->close_child      = MimeMultipartAlternative_close_child;
  return 0;
}


static int
MimeMultipartAlternative_initialize (MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  PR_ASSERT(!malt->part_buffer);
  malt->part_buffer = MimePartBufferCreate();
  if (!malt->part_buffer)
	return MIME_OUT_OF_MEMORY;

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(obj);
}

static void
MimeMultipartAlternative_cleanup(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  if (malt->buffered_hdrs)
	{
	  MimeHeaders_free(malt->buffered_hdrs);
	  malt->buffered_hdrs = 0;
	}
  if (malt->part_buffer)
	{
	  MimePartBufferDestroy(malt->part_buffer);
	  malt->part_buffer = 0;
	}
}


static void
MimeMultipartAlternative_finalize (MimeObject *obj)
{
  MimeMultipartAlternative_cleanup(obj);
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(obj);
}


static int
MimeMultipartAlternative_parse_eof (MimeObject *obj, PRBool abort_p)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  int status = 0;

  if (obj->closed_p) return 0;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  /* If there's a cached part we haven't written out yet, do it now.
   */
  if (malt->buffered_hdrs && !abort_p)
	{
	  status = MimeMultipartAlternative_display_cached_part(obj);
	  if (status < 0) return status;
	}

  MimeMultipartAlternative_cleanup(obj);

  return status;
}


static int
MimeMultipartAlternative_create_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  if (MimeMultipartAlternative_display_part_p (obj, mult->hdrs))
	{
	  /* If this part is potentially displayable, begin populating the cache
		 with it.  If there's something in the cache already, discard it
		 first.  (Just because this part is displayable doesn't mean we will
		 display it -- of two consecutive displayable parts, it is the second
		 one that gets displayed.)
	   */
	  int status;
	  mult->state = MimeMultipartPartFirstLine;

	  status = MimeMultipartAlternative_discard_cached_part(obj);
	  if (status < 0) return status;

	  PR_ASSERT(!malt->buffered_hdrs);
	  malt->buffered_hdrs = MimeHeaders_copy(mult->hdrs);
	  if (!malt->buffered_hdrs) return MIME_OUT_OF_MEMORY;
	  return 0;
	}
  else
	{
	  /* If this part is not displayable, then we're done -- all that is left
		 to do is to flush out the part that is currently in the cache.
	   */
	  mult->state = MimeMultipartEpilogue;
	  return MimeMultipartAlternative_display_cached_part(obj);
	}
}


static int
MimeMultipartAlternative_parse_child_line (MimeObject *obj,
										   char *line, PRInt32 length,
										   PRBool first_line_p)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  PR_ASSERT(malt->part_buffer);
  if (!malt->part_buffer) return -1;

  /* Push this line into the buffer for later retrieval. */
  return MimePartBufferWrite (malt->part_buffer, line, length);
}


static int
MimeMultipartAlternative_close_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  /* PR_ASSERT(malt->part_buffer);			Some Mac brokenness trips this...
  if (!malt->part_buffer) return -1; */

  if (malt->part_buffer)
	MimePartBufferClose(malt->part_buffer);

  /* PR_ASSERT(mult->hdrs);					I expect the Mac trips this too	*/
  if (mult->hdrs)
	MimeHeaders_free(mult->hdrs);
  mult->hdrs = 0;

  return 0;
}


static PRBool
MimeMultipartAlternative_display_part_p(MimeObject *self,
										MimeHeaders *sub_hdrs)
{
  char *ct = MimeHeaders_get (sub_hdrs, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);

  /* RFC 1521 says:
	   Receiving user agents should pick and display the last format
	   they are capable of displaying.  In the case where one of the
	   alternatives is itself of type "multipart" and contains unrecognized
	   sub-parts, the user agent may choose either to show that alternative,
	   an earlier alternative, or both.

	 Ugh.  If there is a multipart subtype of alternative, we simply show
	 that, without descending into it to determine if any of its sub-parts
	 are themselves unknown.
   */

  MimeObjectClass *clazz = mime_find_class (ct, sub_hdrs, self->options, PR_FALSE);
  PRBool result = (clazz
					? clazz->displayable_inline_p(clazz, sub_hdrs)
					: PR_FALSE);
  PR_FREEIF(ct);
  return result;
}

static int 
MimeMultipartAlternative_discard_cached_part(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;

  if (malt->buffered_hdrs)
	{
	  MimeHeaders_free(malt->buffered_hdrs);
	  malt->buffered_hdrs = 0;
	}
  if (malt->part_buffer)
	MimePartBufferReset (malt->part_buffer);

  return 0;
}

static int 
MimeMultipartAlternative_display_cached_part(MimeObject *obj)
{
  MimeMultipartAlternative *malt = (MimeMultipartAlternative *) obj;
  int status;

  char *ct = (malt->buffered_hdrs
			  ? MimeHeaders_get (malt->buffered_hdrs, HEADER_CONTENT_TYPE,
								 PR_TRUE, PR_FALSE)
			  : 0);
  const char *dct = (((MimeMultipartClass *) obj->clazz)->default_part_type);
  MimeObject *body;
  PRBool multipart_p;

  /* Don't pass in NULL as the content-type (this means that the
	 auto-uudecode-hack won't ever be done for subparts of a
	 multipart, but only for untyped children of message/rfc822.
   */
  body = mime_create(((ct && *ct) ? ct : (dct ? dct: TEXT_PLAIN)),
					 malt->buffered_hdrs, obj->options);

  PR_FREEIF(ct);
  if (!body) return MIME_OUT_OF_MEMORY;

  multipart_p = mime_typep(body, (MimeObjectClass *) &mimeMultipartClass);

  status = ((MimeContainerClass *) obj->clazz)->add_child(obj, body);
  if (status < 0)
	{
	  mime_free(body);
	  return status;
	}

#ifdef MIME_DRAFTS
  if ( obj->options && 
	   obj->options->decompose_file_p &&
	   !multipart_p &&
	   obj->options->decompose_file_init_fn )
	{
	  status = obj->options->decompose_file_init_fn (
												obj->options->stream_closure,
												malt->buffered_hdrs);
	  if (status < 0) return status;
	}
#endif /* MIME_DRAFTS */


  /* Now that we've added this new object to our list of children,
	 start its parser going. */
  status = body->clazz->parse_begin(body);
  if (status < 0) return status;

#ifdef MIME_DRAFTS
  if ( obj->options &&
	   obj->options->decompose_file_p &&
	   !multipart_p &&
	   obj->options->decompose_file_output_fn )
	status = MimePartBufferRead (malt->part_buffer,
								 obj->options->decompose_file_output_fn,
								 obj->options->stream_closure);
  else
#endif /* MIME_DRAFTS */

	status = MimePartBufferRead (malt->part_buffer,
								 /* The (nsresult (*) ...) cast is to turn the
									`void' argument into `MimeObject'. */
								 ((nsresult (*) (char *, PRInt32, void *))
								  body->clazz->parse_buffer),
								 body);

  if (status < 0) return status;

  MimeMultipartAlternative_cleanup(obj);

  /* Done parsing. */
  status = body->clazz->parse_eof(body, PR_FALSE);
  if (status < 0) return status;
  status = body->clazz->parse_end(body, PR_FALSE);
  if (status < 0) return status;

#ifdef MIME_DRAFTS
  if ( obj->options &&
	   obj->options->decompose_file_p &&
	   !multipart_p &&
	   obj->options->decompose_file_close_fn ) {
	status = obj->options->decompose_file_close_fn ( obj->options->stream_closure );
	if (status < 0) return status;
  }
#endif /* MIME_DRAFTS */

  return 0;
}
