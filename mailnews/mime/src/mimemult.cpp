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

#include "msgCore.h"
#include "mimemult.h"
#include "mimemoz2.h"

#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "prio.h"
#include "nsMimeStringResources.h"
#include "nsMimeTypes.h"

#ifdef XP_MAC
  extern MimeObjectClass mimeMultipartAppleDoubleClass;
#endif

#define MIME_SUPERCLASS mimeContainerClass
MimeDefClass(MimeMultipart, MimeMultipartClass,
			 mimeMultipartClass, &MIME_SUPERCLASS);

static int MimeMultipart_initialize (MimeObject *);
static void MimeMultipart_finalize (MimeObject *);
static int MimeMultipart_parse_line (char *line, PRInt32 length, MimeObject *);
static int MimeMultipart_parse_eof (MimeObject *object, PRBool abort_p);

static MimeMultipartBoundaryType MimeMultipart_check_boundary(MimeObject *,
															  const char *,
															  PRInt32);
static int MimeMultipart_create_child(MimeObject *);
static PRBool MimeMultipart_output_child_p(MimeObject *, MimeObject *);
static int MimeMultipart_parse_child_line (MimeObject *, char *, PRInt32,
										   PRBool);
static int MimeMultipart_close_child(MimeObject *);

extern "C" MimeObjectClass mimeMultipartAlternativeClass;
extern "C" MimeObjectClass mimeMultipartRelatedClass;
extern "C" MimeObjectClass mimeMultipartSignedClass;
extern "C" MimeObjectClass mimeInlineTextVCardClass;

#if defined(DEBUG) && defined(XP_UNIX)
static int MimeMultipart_debug_print (MimeObject *, PRFileDesc *, PRInt32);
#endif

static int
MimeMultipartClassInitialize(MimeMultipartClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
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
  PR_ASSERT(object->clazz != (MimeObjectClass *) &mimeMultipartClass);

  ct = MimeHeaders_get (object->headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  mult->boundary = (ct
					? MimeHeaders_get_parameter (ct, HEADER_PARM_BOUNDARY, NULL, NULL)
					: 0);
  PR_FREEIF(ct);
  mult->state = MimeMultipartPreamble;
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}


static void
MimeMultipart_finalize (MimeObject *object)
{
  MimeMultipart *mult = (MimeMultipart *) object;

  object->clazz->parse_eof(object, PR_FALSE);

  PR_FREEIF(mult->boundary);
  if (mult->hdrs)
	MimeHeaders_free(mult->hdrs);
  mult->hdrs = 0;
  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}


static int
MimeMultipart_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  int status = 0;
  MimeMultipartBoundaryType boundary;

  PR_ASSERT(line && *line);
  if (!line || !*line) return -1;

  PR_ASSERT(!obj->closed_p);
  if (obj->closed_p) return -1;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p &&
	  obj->options->output_fn)
	return MimeObject_write(obj, line, length, PR_TRUE);


  if (mult->state == MimeMultipartEpilogue)  /* already done */
	boundary = MimeMultipartBoundaryTypeNone;
  else
	boundary = ((MimeMultipartClass *)obj->clazz)->check_boundary(obj, line,
																  length);

  if (boundary == MimeMultipartBoundaryTypeTerminator ||
	  boundary == MimeMultipartBoundaryTypeSeparator)
	{
	  /* Match!  Close the currently-open part, move on to the next
		 state, and discard this line.
	   */
	  if (mult->state != MimeMultipartPreamble)
		status = ((MimeMultipartClass *)obj->clazz)->close_child(obj);
	  if (status < 0) return status;

	  if (boundary == MimeMultipartBoundaryTypeTerminator)
		mult->state = MimeMultipartEpilogue;
	  else
		{
		  mult->state = MimeMultipartHeaders;

		  /* Reset the header parser for this upcoming part. */
		  PR_ASSERT(!mult->hdrs);
		  if (mult->hdrs)
			MimeHeaders_free(mult->hdrs);
		  mult->hdrs = MimeHeaders_new();
		  if (!mult->hdrs)
			return MIME_OUT_OF_MEMORY;
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
      
      // If this line is blank, we're now done parsing headers, and should
      // now examine the content-type to create this "body" part.
      //
      if (*line == nsCRT::CR || *line == nsCRT::LF)
      {
        status = ((MimeMultipartClass *) obj->clazz)->create_child(obj);
        if (status < 0) return status;
        PR_ASSERT(mult->state != MimeMultipartHeaders);

        // Ok, at this point, we need to examine the headers and see if there
        // is a special charset (i.e. non US-ASCII) for this message. If so, 
        // we need to tell the emitter that this is the case for use in in any
        // possible reply or forward operation.
        //
        PRBool isAlternativeOrRelated = PR_FALSE;
        PRBool isBody = MimeObjectChildIsMessageBody(obj, &isAlternativeOrRelated);
        if ( (isAlternativeOrRelated || isBody) && obj->options)
        {
          MimeContainer *container = (MimeContainer*) obj; 
          // If we have only one child and this is the message body object,     
          // this we should check for a special charset and notify the emitter  
          // if one exists!                                                     
          if ( (isAlternativeOrRelated) || ((container->children) && (container->nchildren == 1)) )
          {
           char *ct = MimeHeaders_get(mult->hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
           if (ct)
           {
             char *cset = MimeHeaders_get_parameter (ct, "charset", NULL, NULL);
             if (cset)
             {
                mimeEmitterUpdateCharacterSet(obj->options, cset);
                if (!(obj->options->override_charset))
                {
                  // Also set this charset to msgWindow
                  if (!nsCRT::strcasecmp(cset, "us-ascii"))
                    SetMailCharacterSetToMsgWindow(obj, NS_LITERAL_STRING("ISO-8859-1").get());
                  else
                    SetMailCharacterSetToMsgWindow(obj, NS_ConvertASCIItoUCS2(cset).get());
                }
              }

              PR_FREEIF(ct);
              PR_FREEIF(cset);
            }
          }
          }
        }
      break;
    }

	case MimeMultipartPartFirstLine:
	  /* Hand this line off to the sub-part. */
	  status = (((MimeMultipartClass *) obj->clazz)->parse_child_line(obj,
																	  line,
																	  length,
																	  PR_TRUE));
	  if (status < 0) return status;
	  mult->state = MimeMultipartPartLine;
	  break;

	case MimeMultipartPartLine:
	  /* Hand this line off to the sub-part. */
	  status = (((MimeMultipartClass *) obj->clazz)->parse_child_line(obj,
																	  line,
																	  length,
																	  PR_FALSE));
	  if (status < 0) return status;
	  break;

	default:
	  PR_ASSERT(0);
	  return -1;
	}

  return 0;
}


static MimeMultipartBoundaryType
MimeMultipart_check_boundary(MimeObject *obj, const char *line, PRInt32 length)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  PRInt32 blen;
  PRBool term_p;

  if (!mult->boundary ||
	  line[0] != '-' ||
	  line[1] != '-')
	return MimeMultipartBoundaryTypeNone;

  /* This is a candidate line to be a boundary.  Check it out... */
  blen = nsCRT::strlen(mult->boundary);
  term_p = PR_FALSE;

  /* strip trailing whitespace (including the newline.) */
  while(length > 2 && nsCRT::IsAsciiSpace(line[length-1]))
	length--;

  /* Could this be a terminating boundary? */
  if (length == blen + 4 &&
	  line[length-1] == '-' &&
	  line[length-2] == '-')
	{
	  term_p = PR_TRUE;
	}

  //looks like we have a separator but first, we need to check it's not for one of the part's children.
  MimeContainer *cont = (MimeContainer *) obj;
  if (cont->nchildren > 0)
  {
    MimeObject *kid = cont->children[cont->nchildren-1];
    if (kid)
      if (mime_typep(kid, (MimeObjectClass*) &mimeMultipartClass))
      {
        //Don't ask the kid to check the boundary if it has already detected a Teminator
        MimeMultipart *mult = (MimeMultipart *) kid;
        if (mult->state != MimeMultipartEpilogue)
          if (MimeMultipart_check_boundary(kid, line, length) != MimeMultipartBoundaryTypeNone)
            return MimeMultipartBoundaryTypeNone;
      }
  }

  if (term_p)
    length -= 2;

  if (blen == length-2 && !nsCRT::strncmp(line+2, mult->boundary, length-2))
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
  int           status;
  char *ct = (mult->hdrs
			  ? MimeHeaders_get (mult->hdrs, HEADER_CONTENT_TYPE,
								 PR_TRUE, PR_FALSE)
			  : 0);
  const char *dct = (((MimeMultipartClass *) obj->clazz)->default_part_type);
  MimeObject *body = NULL;

  mult->state = MimeMultipartPartFirstLine;
  /* Don't pass in NULL as the content-type (this means that the
	 auto-uudecode-hack won't ever be done for subparts of a
	 multipart, but only for untyped children of message/rfc822.
   */
  body = mime_create(((ct && *ct) ? ct : (dct ? dct: TEXT_PLAIN)),
					 mult->hdrs, obj->options);
  PR_FREEIF(ct);
  if (!body) return MIME_OUT_OF_MEMORY;
  status = ((MimeContainerClass *) obj->clazz)->add_child(obj, body);
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
#ifdef MIME_DETAIL_CHECK
		   !mime_typep(body, (MimeObjectClass*)&mimeMultipartRelatedClass) &&
		   !mime_typep(body, (MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		   !mime_typep(body,(MimeObjectClass*)&mimeMultipartSignedClass) 
#else
           /* bug 21869 -- due to the fact that we are not generating the
              correct mime class object for content-typ multipart/signed part
              the above check failed. to solve the problem in general and not
              to cause early temination when parsing message for opening as
              draft we can simply make sure that the child is not a multipart
              mime object. this way we could have a proper decomposing message
              part functions set correctly */
           !mime_typep(body, (MimeObjectClass*) &mimeMultipartClass)
#endif
#ifdef RICHIE_VCARD
       && !mime_typep(body, (MimeObjectClass*)&mimeInlineTextVCardClass)
#endif
       )
	  {
		status = obj->options->decompose_file_init_fn ( obj->options->stream_closure, mult->hdrs );
		if (status < 0) return status;
	  }
	}
#endif /* MIME_DRAFTS */


  /* Now that we've added this new object to our list of children,
	 start its parser going (if we want to display it.)
   */
  body->output_p = (((MimeMultipartClass *) obj->clazz)->output_child_p(obj, body));
  if (body->output_p)
	{  
	  status = body->clazz->parse_begin(body);

#ifdef XP_MAC
    /* if we are saving an apple double attachment, we need to set correctly the conten type of the channel */
    if (mime_typep(obj, (MimeObjectClass *) &mimeMultipartAppleDoubleClass))
    {
      struct mime_stream_data *msd = (struct mime_stream_data *)body->options->stream_closure;
      if (!body->options->write_html_p && body->content_type && !nsCRT::strcasecmp(body->content_type, APPLICATION_APPLEFILE))
        msd->channel->SetContentType(APPLICATION_APPLEFILE);
    }
#endif

	  if (status < 0) return status;
	}

  return 0;
}


static PRBool
MimeMultipart_output_child_p(MimeObject *obj, MimeObject *child)
{
  /* if we are saving an apple double attachment, ignore the appledouble wrapper part */
  return obj->options->write_html_p || nsCRT::strcasecmp(child->content_type, MULTIPART_APPLEDOUBLE);
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

  PR_ASSERT(cont->nchildren > 0);
  if (cont->nchildren > 0)
	{
	  MimeObject *kid = cont->children[cont->nchildren-1];
	  if (kid)
		{
		  int status;
		  status = kid->clazz->parse_eof(kid, PR_FALSE);
		  if (status < 0) return status;
		  status = kid->clazz->parse_end(kid, PR_FALSE);
		  if (status < 0) return status;

#ifdef MIME_DRAFTS
		  if ( object->options &&
			   object->options->decompose_file_p &&
			   object->options->is_multipart_msg &&
			   object->options->decompose_file_close_fn ) 
		  {
			  if ( !mime_typep(object,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
				   !mime_typep(object,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
				   !mime_typep(object,(MimeObjectClass*)&mimeMultipartSignedClass) &&
#ifdef MIME_DETAIL_CHECK
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
				   !mime_typep(kid,(MimeObjectClass*)&mimeMultipartSignedClass) 
#else
                   /* bug 21869 -- due to the fact that we are not generating the
                      correct mime class object for content-typ multipart/signed part
                      the above check failed. to solve the problem in general and not
                      to cause early temination when parsing message for opening as
                      draft we can simply make sure that the child is not a multipart
                      mime object. this way we could have a proper decomposing message
                      part functions set correctly */
                   !mime_typep(kid,(MimeObjectClass*) &mimeMultipartClass)
#endif
#ifdef RICHIE_VCARD
           && !mime_typep(kid, (MimeObjectClass*)&mimeInlineTextVCardClass)
#endif
           )
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
MimeMultipart_parse_child_line (MimeObject *obj, char *line, PRInt32 length,
								PRBool first_line_p)
{
  MimeContainer *cont = (MimeContainer *) obj;
  int status;
  MimeObject *kid;

  PR_ASSERT(cont->nchildren > 0);
  if (cont->nchildren <= 0)
	return -1;

  kid = cont->children[cont->nchildren-1];
  PR_ASSERT(kid);
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
#ifdef MIME_DETAIL_CHECK
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartAlternativeClass) &&
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartRelatedClass) &&
		!mime_typep(kid,(MimeObjectClass*)&mimeMultipartSignedClass)
#else
        /* bug 21869 -- due to the fact that we are not generating the
           correct mime class object for content-typ multipart/signed part
           the above check failed. to solve the problem in general and not
           to cause early temination when parsing message for opening as
           draft we can simply make sure that the child is not a multipart
           mime object. this way we could have a proper decomposing message
           part functions set correctly */
        !mime_typep(kid, (MimeObjectClass*) &mimeMultipartClass)
#endif
#ifdef RICHIE_VCARD
    && !mime_typep(kid, (MimeObjectClass*)&mimeInlineTextVCardClass)
#endif
    )
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
  if (length > 0 && line[length-1] == nsCRT::LF) length--;
  if (length > 0 && line[length-1] == nsCRT::CR) length--;

  if (!first_line_p)
	{
	  /* Push out a preceeding newline... */
	  char nl[] = MSG_LINEBREAK;
	  status = kid->clazz->parse_buffer (nl, MSG_LINEBREAK_LEN, kid);
	  if (status < 0) return status;
	}

  /* Now push out the line sans trailing newline. */
  return kid->clazz->parse_buffer (line, length, kid);
}


static int
MimeMultipart_parse_eof (MimeObject *obj, PRBool abort_p)
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
	  int status = obj->clazz->parse_buffer (obj->ibuffer, obj->ibuffer_fp,
											 obj);
	  obj->ibuffer_fp = 0;
	  if (status < 0)
		{
		  obj->closed_p = PR_TRUE;
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
	  PR_ASSERT(kid);
	  if (kid)
		{
		  int status = kid->clazz->parse_eof(kid, abort_p);
		  if (status < 0) return status;
		}
	}

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
}


#if defined(DEBUG) && defined(XP_UNIX)
static int
MimeMultipart_debug_print (MimeObject *obj, PRFileDesc *stream, PRInt32 depth)
{
  /*  MimeMultipart *mult = (MimeMultipart *) obj; */
  MimeContainer *cont = (MimeContainer *) obj;
  char *addr = mime_part_address(obj);
  int i;
  for (i=0; i < depth; i++)
	PR_Write(stream, "  ", 2);
/**
  fprintf(stream, "<%s %s (%d kid%s) boundary=%s 0x%08X>\n",
		  obj->clazz->class_name,
		  addr ? addr : "???",
		  cont->nchildren, (cont->nchildren == 1 ? "" : "s"),
		  (mult->boundary ? mult->boundary : "(none)"),
		  (PRUint32) mult);
**/
  PR_FREEIF(addr);

/*
  if (cont->nchildren > 0)
	fprintf(stream, "\n");
 */

  for (i = 0; i < cont->nchildren; i++)
	{
	  MimeObject *kid = cont->children[i];
	  int status = kid->clazz->debug_print (kid, stream, depth+1);
	  if (status < 0) return status;
	}

/*
  if (cont->nchildren > 0)
	fprintf(stream, "\n");
 */

  return 0;
}
#endif
