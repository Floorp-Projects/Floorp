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

/* mimemsig.c --- definition of the MimeMultipartSigned class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Sep-96.
 */

#include "mimemsig.h"
#include "nspr.h"
#include "xp_error.h"

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeMultipartSigned, MimeMultipartSignedClass,
			 mimeMultipartSignedClass, &MIME_SUPERCLASS);

static int MimeMultipartSigned_initialize (MimeObject *);
static int MimeMultipartSigned_create_child (MimeObject *);
static int MimeMultipartSigned_close_child(MimeObject *);
static int MimeMultipartSigned_parse_line (char *, int32, MimeObject *);
static int MimeMultipartSigned_parse_child_line (MimeObject *, char *, int32,
												 XP_Bool);
static int MimeMultipartSigned_parse_eof (MimeObject *, XP_Bool);
static void MimeMultipartSigned_finalize (MimeObject *);

static int MimeMultipartSigned_emit_child (MimeObject *obj);

static int
MimeMultipartSignedClassInitialize(MimeMultipartSignedClass *class)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    class;
  MimeMultipartClass *mclass = (MimeMultipartClass *) class;

  oclass->initialize       = MimeMultipartSigned_initialize;
  oclass->parse_line       = MimeMultipartSigned_parse_line;
  oclass->parse_eof        = MimeMultipartSigned_parse_eof;
  oclass->finalize         = MimeMultipartSigned_finalize;
  mclass->create_child     = MimeMultipartSigned_create_child;
  mclass->parse_child_line = MimeMultipartSigned_parse_child_line;
  mclass->close_child      = MimeMultipartSigned_close_child;

  XP_ASSERT(!oclass->class_initialized);
  return 0;
}

static int
MimeMultipartSigned_initialize (MimeObject *object)
{
  MimeMultipartSigned *sig = (MimeMultipartSigned *) object;

  /* This is an abstract class; it shouldn't be directly instantiated. */
  XP_ASSERT(object->class != (MimeObjectClass *) &mimeMultipartSignedClass);

  sig->state = MimeMultipartSignedPreamble;

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeMultipartSigned_cleanup (MimeObject *obj, XP_Bool finalizing_p)
{
  MimeMultipart *mult = (MimeMultipart *) obj; /* #58075.  Fix suggested by jwz */
  MimeMultipartSigned *sig = (MimeMultipartSigned *) obj;
  if (sig->part_buffer)
	{
	  MimePartBufferDestroy(sig->part_buffer);
	  sig->part_buffer = 0;
	}
  if (sig->body_hdrs)
	{
	  MimeHeaders_free (sig->body_hdrs);
	  sig->body_hdrs = 0;
	}
  if (sig->sig_hdrs)
	{
	  MimeHeaders_free (sig->sig_hdrs);
	  sig->sig_hdrs = 0;
	}

  mult->state = MimeMultipartEpilogue;  /* #58075.  Fix suggested by jwz */
  sig->state = MimeMultipartSignedEpilogue;

  if (finalizing_p && sig->crypto_closure)
	{
	  /* Don't free these until this object is really going away -- keep them
		 around for the lifetime of the MIME object, so that we can get at the
		 security info of sub-parts of the currently-displayed message. */
	  ((MimeMultipartSignedClass *) obj->class)
		->crypto_free (sig->crypto_closure);
	  sig->crypto_closure = 0;
	}

  if (sig->sig_decoder_data)
	{
	  MimeDecoderDestroy(sig->sig_decoder_data, TRUE);
	  sig->sig_decoder_data = 0;
	}
}


static int
MimeMultipartSigned_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  MimeMultipartSigned *sig = (MimeMultipartSigned *) obj;
  int status = 0;

  if (obj->closed_p) return 0;

  /* Close off the signature, if we've gotten that far.
   */
  if (sig->state == MimeMultipartSignedSignatureHeaders ||
	  sig->state == MimeMultipartSignedSignatureFirstLine ||
	  sig->state == MimeMultipartSignedSignatureLine ||
	  sig->state == MimeMultipartSignedEpilogue)
	{
	  status = (((MimeMultipartSignedClass *) obj->class)
				->crypto_signature_eof) (sig->crypto_closure, abort_p);
	  if (status < 0) return status;
	}

  if (!abort_p)
	{
	  /* Now that we've read both the signed object and the signature (and
		 have presumably verified the signature) write out a blurb, and then
		 the signed object.
	   */
	  XP_ASSERT(sig->crypto_closure);
	  status = MimeMultipartSigned_emit_child(obj);
	  if (status < 0) return status;
	}

  MimeMultipartSigned_cleanup(obj, FALSE);
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
}


static void
MimeMultipartSigned_finalize (MimeObject *obj)
{
  MimeMultipartSigned_cleanup(obj, TRUE);
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(obj);
}


static int
MimeMultipartSigned_parse_line (char *line, int32 length, MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeMultipartSigned *sig = (MimeMultipartSigned *) obj;
  MimeMultipartParseState old_state = mult->state;
  XP_Bool hash_line_p = TRUE;
  XP_Bool no_headers_p = FALSE;
  int status = 0;

  /* First do the parsing for normal multipart/ objects by handing it off to
	 the superclass method.  This includes calling the create_child and
	 close_child methods.
   */
  status = (((MimeObjectClass *)(&MIME_SUPERCLASS))
			->parse_line (line, length, obj));
  if (status < 0) return status;


  /* Now we want to do various other crypto-related things to these lines.
   */


  /* The instance variable MimeMultipartClass->state tracks motion through
	 the various stages of multipart/ parsing.  The instance variable
	 MimeMultipartSigned->state tracks the difference between the first
	 part (the body) and the second part (the signature.)  This second,
	 more specific state variable is updated by noticing the transitions
	 of the first, more general state variable.
   */
  if (old_state != mult->state)  /* there has been a state change */
	{
	  switch (mult->state)
		{
		case MimeMultipartPreamble:
		  XP_ASSERT(0);  /* can't move *in* to preamble state. */
		  sig->state = MimeMultipartSignedPreamble;
		  break;

		case MimeMultipartHeaders:
		  /* If we're moving in to the Headers state, then that means
			 that this line is the preceeding boundary string (and we
			 should ignore it.)
		   */
		  hash_line_p = FALSE;

		  if (sig->state == MimeMultipartSignedPreamble)
			sig->state = MimeMultipartSignedBodyFirstHeader;
		  else if (sig->state == MimeMultipartSignedBodyFirstLine ||
				   sig->state == MimeMultipartSignedBodyLine)
			sig->state = MimeMultipartSignedSignatureHeaders;
		  else if (sig->state == MimeMultipartSignedSignatureFirstLine ||
				   sig->state == MimeMultipartSignedSignatureLine)
			sig->state = MimeMultipartSignedEpilogue;
		  break;

		case MimeMultipartPartFirstLine:
		  if (sig->state == MimeMultipartSignedBodyFirstHeader)
			{			
			  sig->state = MimeMultipartSignedBodyFirstLine;
			  no_headers_p = TRUE;
			}
		  else if (sig->state == MimeMultipartSignedBodyHeaders)
			sig->state = MimeMultipartSignedBodyFirstLine;
		  else if (sig->state == MimeMultipartSignedSignatureHeaders)
			sig->state = MimeMultipartSignedSignatureFirstLine;
		  else
			sig->state = MimeMultipartSignedEpilogue;
		  break;

		case MimeMultipartPartLine:

		  XP_ASSERT(sig->state == MimeMultipartSignedBodyFirstLine ||
					sig->state == MimeMultipartSignedBodyLine ||
					sig->state == MimeMultipartSignedSignatureFirstLine ||
					sig->state == MimeMultipartSignedSignatureLine);

		  if (sig->state == MimeMultipartSignedBodyFirstLine)
			sig->state = MimeMultipartSignedBodyLine;
		  else if (sig->state == MimeMultipartSignedSignatureFirstLine)
			sig->state = MimeMultipartSignedSignatureLine;
		  break;

		case MimeMultipartEpilogue:
		  sig->state = MimeMultipartSignedEpilogue;
		  break;

		default:  /* bad state */
		  XP_ASSERT(0);
		  return -1;
		  break;
		}
	}


  /* Perform multipart/signed-related actions on this line based on the state
	 of the parser.
   */
  switch (sig->state)
	{
	case MimeMultipartSignedPreamble:
	  /* Do nothing. */
	  break;

	case MimeMultipartSignedBodyFirstLine:
	  /* We have just moved out of the MimeMultipartSignedBodyHeaders
		 state, so cache away the headers that apply only to the body part.
	   */
	  XP_ASSERT(mult->hdrs);
	  XP_ASSERT(!sig->body_hdrs);
	  sig->body_hdrs = mult->hdrs;
	  mult->hdrs = 0;

	  /* fall through. */

	case MimeMultipartSignedBodyFirstHeader:
	case MimeMultipartSignedBodyHeaders:
	case MimeMultipartSignedBodyLine:

	  if (!sig->crypto_closure)
		{
		  XP_SetError(0);
		  /* Initialize the signature verification library. */
		  sig->crypto_closure = (((MimeMultipartSignedClass *) obj->class)
								 ->crypto_init) (obj);
		  if (!sig->crypto_closure)
			{
			  status = PR_GetError();
			  XP_ASSERT(status < 0);
			  if (status >= 0) status = -1;
			  return status;
			}
		}

	  if (hash_line_p)
		{
		  /* this is the first hashed line if this is the first header
			 (that is, if it's the first line in the header state after
			 a state change.)
		   */
		  XP_Bool first_line_p
			= (no_headers_p ||
			   sig->state == MimeMultipartSignedBodyFirstHeader);

		  if (sig->state == MimeMultipartSignedBodyFirstHeader)
			sig->state = MimeMultipartSignedBodyHeaders;

		  /* The newline issues here are tricky, since both the newlines
			 before and after the boundary string are to be considered part
			 of the boundary: this is so that a part can be specified such
			 that it does not end in a trailing newline.

			 To implement this, we send a newline *before* each line instead
			 of after, except for the first line, which is not preceeded by a
			 newline.

			 For purposes of cryptographic hashing, we always hash line
			 breaks as CRLF -- the canonical, on-the-wire linebreaks, since
			 we have no idea of knowing what line breaks were used on the
			 originating system (SMTP rightly destroys that information.)
		   */

		  /* Remove the trailing newline... */
		  if (length > 0 && line[length-1] == LF) length--;
		  if (length > 0 && line[length-1] == CR) length--;

		  XP_ASSERT(sig->crypto_closure);

		  if (!first_line_p)
			{
			  /* Push out a preceeding newline... */
			  char nl[] = CRLF;
			  status = (((MimeMultipartSignedClass *) obj->class)
						->crypto_data_hash (nl, 2, sig->crypto_closure));
			  if (status < 0) return status;
			}

		  /* Now push out the line sans trailing newline. */
		  if (length > 0)
			status = (((MimeMultipartSignedClass *) obj->class)
					  ->crypto_data_hash (line,length, sig->crypto_closure));
		  if (status < 0) return status;
		}
	  break;

	case MimeMultipartSignedSignatureHeaders:

	  if (sig->crypto_closure &&
		  old_state != mult->state)
		{
		  /* We have just moved out of the MimeMultipartSignedBodyLine
			 state, so tell the signature verification library that we've
			 reached the end of the signed data.
		   */
		  status = (((MimeMultipartSignedClass *) obj->class)
					->crypto_data_eof) (sig->crypto_closure, FALSE);
		  if (status < 0) return status;
		}
	  break;

	case MimeMultipartSignedSignatureFirstLine:
	  /* We have just moved out of the MimeMultipartSignedSignatureHeaders
		 state, so cache away the headers that apply only to the sig part.
	   */
	  XP_ASSERT(mult->hdrs);
	  XP_ASSERT(!sig->sig_hdrs);
	  sig->sig_hdrs = mult->hdrs;
	  mult->hdrs = 0;


	  /* If the signature block has an encoding, set up a decoder for it.
		 (Similar logic is in MimeLeafClass->parse_begin.)
	   */
	  {
		MimeDecoderData *(*fn) (int (*) (const char*, int32,void*), void*) = 0;
		char *encoding = MimeHeaders_get (sig->sig_hdrs,
										  HEADER_CONTENT_TRANSFER_ENCODING,
										  TRUE, FALSE);
		if (!encoding)
		  ;
		else if (!strcasecomp(encoding, ENCODING_BASE64))
		  fn = &MimeB64DecoderInit;
		else if (!strcasecomp(encoding, ENCODING_QUOTED_PRINTABLE))
		  fn = &MimeQPDecoderInit;
		else if (!strcasecomp(encoding, ENCODING_UUENCODE) ||
				 !strcasecomp(encoding, ENCODING_UUENCODE2) ||
				 !strcasecomp(encoding, ENCODING_UUENCODE3) ||
				 !strcasecomp(encoding, ENCODING_UUENCODE4))
		  fn = &MimeUUDecoderInit;

		if (fn)
		  {
			sig->sig_decoder_data =
			  fn (((int (*) (const char *, int32, void *))
				   (((MimeMultipartSignedClass *) obj->class)
					->crypto_signature_hash)),
				  sig->crypto_closure);
			if (!sig->sig_decoder_data)
			  return MK_OUT_OF_MEMORY;
		  }
	  }

	  /* Show these headers to the crypto module. */
	  if (hash_line_p)
		{
		  status = (((MimeMultipartSignedClass *) obj->class)
					->crypto_signature_init) (sig->crypto_closure,
											  obj, sig->sig_hdrs);
		  if (status < 0) return status;
		}

	  /* fall through. */

	case MimeMultipartSignedSignatureLine:
	  if (hash_line_p)
		{
		  /* Feed this line into the signature verification routines. */

		  if (sig->sig_decoder_data)
			status = MimeDecoderWrite (sig->sig_decoder_data, line, length);
		  else
			status = (((MimeMultipartSignedClass *) obj->class)
					  ->crypto_signature_hash (line, length,
											   sig->crypto_closure));
		  if (status < 0) return status;
		}
	  break;

	case MimeMultipartSignedEpilogue:
	  /* Nothing special to do here. */
	  break;

	default:  /* bad state */
	  XP_ASSERT(0);
	  return -1;
	}

  return status;
}


static int
MimeMultipartSigned_create_child (MimeObject *parent)
{
  /* Don't actually create a child -- we call the superclass create_child
	 method later, after we've fully parsed everything.  (And we only call
	 it once, for part #1, and never for part #2 (the signature.))
   */
  MimeMultipart *mult = (MimeMultipart *) parent;
  mult->state = MimeMultipartPartFirstLine;
  return 0;
}


static int
MimeMultipartSigned_close_child (MimeObject *obj)
{
  /* The close_child method on MimeMultipartSigned doesn't actually do
	 anything to the children list, since the create_child method also
	 doesn't do anything.
   */
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeContainer *cont = (MimeContainer *) obj;
  MimeMultipartSigned *msig = (MimeMultipartSigned *) obj;

  if (msig->part_buffer)
	/* Closes the tmp file, if there is one: doesn't free the part_buffer. */
	MimePartBufferClose(msig->part_buffer);

  if (mult->hdrs)	/* duplicated from MimeMultipart_close_child, ugh. */
	{
	  MimeHeaders_free(mult->hdrs);
	  mult->hdrs = 0;
	}

  /* Should be no kids yet. */
  XP_ASSERT(cont->nchildren == 0);
  if (cont->nchildren != 0) return -1;

  return 0;
}


static int
MimeMultipartSigned_parse_child_line (MimeObject *obj,
									  char *line, int32 length,
									  XP_Bool first_line_p)
{
  MimeMultipartSigned *sig = (MimeMultipartSigned *) obj;
  MimeContainer *cont = (MimeContainer *) obj;
  int status = 0;

  /* Shouldn't have made any sub-parts yet. */
  XP_ASSERT(cont->nchildren == 0);
  if (cont->nchildren != 0) return -1;

  switch (sig->state)
	{
	case MimeMultipartSignedPreamble:
	case MimeMultipartSignedBodyFirstHeader:
	case MimeMultipartSignedBodyHeaders:
	  XP_ASSERT(0);  /* How'd we get here?  Oh well, fall through. */

	case MimeMultipartSignedBodyFirstLine:
	  XP_ASSERT(first_line_p);
	  if (!sig->part_buffer)
		{
		  sig->part_buffer = MimePartBufferCreate();
		  if (!sig->part_buffer)
			return MK_OUT_OF_MEMORY;
		}
	  /* fall through */

	case MimeMultipartSignedBodyLine:
	  {
		/* This is the first part; we are buffering it, and will emit it all
		   at the end (so that we know whether the signature matches before
		   showing anything to the user.)
		 */

		/* The newline issues here are tricky, since both the newlines
		   before and after the boundary string are to be considered part
		   of the boundary: this is so that a part can be specified such
		   that it does not end in a trailing newline.

		   To implement this, we send a newline *before* each line instead
		   of after, except for the first line, which is not preceeded by a
		   newline.
		 */

		/* Remove the trailing newline... */
		if (length > 0 && line[length-1] == LF) length--;
		if (length > 0 && line[length-1] == CR) length--;

		XP_ASSERT(sig->part_buffer);
		XP_ASSERT(first_line_p ==
				  (sig->state == MimeMultipartSignedBodyFirstLine));

		if (!first_line_p)
		  {
			/* Push out a preceeding newline... */
			char nl[] = LINEBREAK;
			status = MimePartBufferWrite (sig->part_buffer, nl, LINEBREAK_LEN);
			if (status < 0) return status;
		  }

		/* Now push out the line sans trailing newline. */
		if (length > 0)
			status = MimePartBufferWrite (sig->part_buffer, line, length);
		if (status < 0) return status;
	  }
	break;

	case MimeMultipartSignedSignatureHeaders:
	  XP_ASSERT(0);  /* How'd we get here?  Oh well, fall through. */

	case MimeMultipartSignedSignatureFirstLine:
	case MimeMultipartSignedSignatureLine:
	  /* Nothing to do here -- hashing of the signature part is handled up
		 in MimeMultipartSigned_parse_line().
	   */
	  break;

	case MimeMultipartSignedEpilogue:
	  /* Too many kids?  MimeMultipartSigned_create_child() should have
		 prevented us from getting here. */
	  XP_ASSERT(0);
	  return -1;
	  break;

	default: /* bad state */
	  XP_ASSERT(0);
	  return -1;
	  break;
	}

  return status;
}


static int
MimeMultipartSigned_emit_child (MimeObject *obj)
{
  MimeMultipartSigned *sig = (MimeMultipartSigned *) obj;
  MimeMultipart *mult = (MimeMultipart *) obj;
  MimeContainer *cont = (MimeContainer *) obj;
  int status = 0;
  MimeObject *body;

  XP_ASSERT(sig->crypto_closure);

  /* Emit some HTML saying whether the signature was cool.
	 But don't emit anything if in FO_QUOTE_MESSAGE mode.
   */
  if (obj->options &&
	  obj->options->headers != MimeHeadersCitation &&
	  obj->options->write_html_p &&
	  obj->options->output_fn &&
	  obj->options->headers != MimeHeadersCitation &&
	  sig->crypto_closure)
	{
	  char *html = (((MimeMultipartSignedClass *) obj->class)
					->crypto_generate_html (sig->crypto_closure));
	  if (!html) return -1; /* MK_OUT_OF_MEMORY? */

	  status = MimeObject_write(obj, html, XP_STRLEN(html), FALSE);
	  XP_FREE(html);
	  if (status < 0) return status;

	  /* Now that we have written out the crypto stamp, the outermost header
		 block is well and truly closed.  If this is in fact the outermost
		 message, then run the post_header_html_fn now.
	   */
	  if (obj->options &&
		  obj->options->state &&
		  obj->options->generate_post_header_html_fn &&
		  !obj->options->state->post_header_html_run_p)
		{
		  MimeHeaders *outer_headers;
		  MimeObject *p;
		  for (p = obj; p->parent; p = p->parent)
			outer_headers = p->headers;
		  XP_ASSERT(obj->options->state->first_data_written_p);
		  html = obj->options->generate_post_header_html_fn(NULL,
													obj->options->html_closure,
															outer_headers);
		  obj->options->state->post_header_html_run_p = TRUE;
		  if (html)
			{
			  status = MimeObject_write(obj, html, XP_STRLEN(html), FALSE);
			  XP_FREE(html);
			  if (status < 0) return status;
			}
		}
	}


  /* Oh, this is fairly nasty.  We're skipping over our "create child" method
	 and using the one our superclass defines.  Perhaps instead we should add
	 a new method on this class, and initialize that method to be the
	 create_child method of the superclass.  Whatever.
   */


  /* The superclass method expects to find the headers for the part that it's
	 to create in mult->hdrs, so ensure that they're there. */
  XP_ASSERT(!mult->hdrs);
  if (mult->hdrs) MimeHeaders_free(mult->hdrs);
  mult->hdrs = sig->body_hdrs;
  sig->body_hdrs = 0;

  /* Run the superclass create_child method.
   */
  status = (((MimeMultipartClass *)(&MIME_SUPERCLASS))->create_child(obj));
  if (status < 0) return status;

  /* Retrieve the child that it created.
   */
  XP_ASSERT(cont->nchildren == 1);
  if (cont->nchildren != 1) return -1;
  body = cont->children[0];
  XP_ASSERT(body);
  if (!body) return -1;

#ifdef MIME_DRAFTS
  if (body->options->decompose_file_p) {
	  body->options->signed_p = TRUE;
	  if (!mime_typep(body, (MimeObjectClass*)&mimeMultipartClass) &&
		body->options->decompose_file_init_fn)
		body->options->decompose_file_init_fn ( body->options->stream_closure, body->headers );
  }
#endif /* MIME_DRAFTS */

  /* If there's no part_buffer, this is a zero-length signed message? */
  if (sig->part_buffer)
	{
#ifdef MIME_DRAFTS
	  if (body->options->decompose_file_p &&
		  !mime_typep(body, (MimeObjectClass*)&mimeMultipartClass)  &&
		  body->options->decompose_file_output_fn)
		  status = MimePartBufferRead (sig->part_buffer,
							   /* The (int (*) ...) cast is to turn the
								  `void' argument into `MimeObject'. */
							   ((int (*) (char *, int32, void *))
							   body->options->decompose_file_output_fn),
							   body->options->stream_closure);
	  else
#endif /* MIME_DRAFTS */

	  status = MimePartBufferRead (sig->part_buffer,
							   /* The (int (*) ...) cast is to turn the
								  `void' argument into `MimeObject'. */
							   ((int (*) (char *, int32, void *))
								body->class->parse_buffer),
								body);
	  if (status < 0) return status;
	}

  MimeMultipartSigned_cleanup(obj, FALSE);

  /* Done parsing. */
  status = body->class->parse_eof(body, FALSE);
  if (status < 0) return status;
  status = body->class->parse_end(body, FALSE);
  if (status < 0) return status;

#ifdef MIME_DRAFTS
  if (body->options->decompose_file_p &&
	  !mime_typep(body, (MimeObjectClass*)&mimeMultipartClass)  &&
	  body->options->decompose_file_close_fn)
	  body->options->decompose_file_close_fn(body->options->stream_closure);
#endif /* MIME_DRAFTS */

  /* Put out a separator after every multipart/signed object. */
  status = MimeObject_write_separator(obj);
  if (status < 0) return status;

  return 0;
}
