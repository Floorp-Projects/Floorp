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
#include "mimetric.h"
#include "mimebuf.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "prlog.h"
#include "msgCore.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextRichtext, MimeInlineTextRichtextClass,
			 mimeInlineTextRichtextClass, &MIME_SUPERCLASS);

static int MimeInlineTextRichtext_parse_line (char *, PRInt32, MimeObject *);
static int MimeInlineTextRichtext_parse_begin (MimeObject *);
static int MimeInlineTextRichtext_parse_eof (MimeObject *, PRBool);

static int
MimeInlineTextRichtextClassInitialize(MimeInlineTextRichtextClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextRichtext_parse_begin;
  oclass->parse_line  = MimeInlineTextRichtext_parse_line;
  oclass->parse_eof   = MimeInlineTextRichtext_parse_eof;
  return 0;
}

/* This function has this clunky interface because it needs to be called
   from outside this module (no MimeObject, etc.)
 */
int
MimeRichtextConvert (char *line, PRInt32 length,
					 int (*output_fn) (char *buf, PRInt32 size, void *closure),
					 void *closure,
					 char **obufferP,
					 PRInt32 *obuffer_sizeP,
					 PRBool enriched_p)
{
  /* RFC 1341 (the original MIME spec) defined text/richtext.
	 RFC 1563 superceded text/richtext with text/enriched.
	 The changes from text/richtext to text/enriched are:
	  - CRLF semantics are different
	  - << maps to <
	  - These tags were added:
	     <VERBATIM>, <NOFILL>, <PARAM>, <FLUSHBOTH>
	  - These tags were removed:
	     <COMMENT>, <OUTDENT>, <OUTDENTRIGHT>, <SAMEPAGE>, <SUBSCRIPT>,
         <SUPERSCRIPT>, <HEADING>, <FOOTING>, <PARAGRAPH>, <SIGNATURE>,
	     <LT>, <NL>, <NP>
	 This method implements them both.

	 draft-resnick-text-enriched-03.txt is a proposed update to 1563.
	  - These tags were added:
	    <FONTFAMILY>, <COLOR>, <PARAINDENT>, <LANG>.
		However, all of these rely on the magic <PARAM> tag, which we
		don't implement, so we're ignoring all of these.
	 Interesting fact: it's by Peter W. Resnick from Qualcomm (Eudora).
	 And it also says "It is fully expected that other text formatting
	 standards like HTML and SGML will supplant text/enriched in
	 Internet mail."
   */
  int status = 0;
  char *out;
  const char *data_end;
  const char *last_end;
  const char *this_start;
  const char *this_end;
  int desired_size;

  desired_size = length * 4;
  if (desired_size >= *obuffer_sizeP)
	status = mime_GrowBuffer (desired_size, sizeof(char), 1024,
							 obufferP, obuffer_sizeP);
  if (status < 0) return status;

  if (enriched_p)
	{
	  for (this_start = line; this_start < line + length; this_start++)
		if (!nsCRT::IsAsciiSpace (*this_start)) break;
	  if (this_start >= line + length) /* blank line */
		{
		  PL_strcpy (*obufferP, "<BR>");
		  return output_fn (*obufferP, nsCRT::strlen(*obufferP), closure);
		}
	}

  out = *obufferP;
  *out = 0;

  data_end = line + length;
  last_end = line;
  this_start = last_end;
  this_end = this_start;
  while (this_end < data_end)
	{
	  /* Skip forward to next special character. */
	  while (this_start < data_end &&
			 *this_start != '<' && *this_start != '>' &&
			 *this_start != '&')
		this_start++;

	  this_end = this_start;

	  /* Skip to the end of the tag. */
	  if (this_start < data_end && *this_start == '<')
		{
		  this_end++;
		  while (this_end < data_end &&
				 !nsCRT::IsAsciiSpace (*this_end) &&
				 *this_end != '<' && *this_end != '>' &&
				 *this_end != '&')
			this_end++;
		}

	  this_end++;

	  /* Push out the text preceeding the tag. */
	  if (last_end && last_end != this_start)
		{
		  nsCRT::memcpy (out, last_end, this_start - last_end);
		  out += this_start - last_end;
		  *out = 0;
		}

	  if (this_start >= data_end)
		break;
	  else if (*this_start == '&')
		{
		  PL_strcpy (out, "&amp;"); out += nsCRT::strlen (out);
		}
	  else if (*this_start == '>')
		{
		  PL_strcpy (out, "&gt;"); out += nsCRT::strlen (out);
		}
	  else if (enriched_p &&
			   this_start < data_end + 1 &&
			   this_start[0] == '<' &&
			   this_start[1] == '<')
		{
		  PL_strcpy (out, "&lt;"); out += nsCRT::strlen (out);
		}
	  else if (this_start != this_end)
		{
		  /* Push out this ID. */
		  const char *old = this_start + 1;
		  char *tag_open  = 0;
		  char *tag_close = 0;
		  if (*old == '/')
			{
			  /* This is </tag> */
			  old++;
			}

		  switch (*old)
			{
			case 'b': case 'B':
			  if (!nsCRT::strncasecmp ("BIGGER>", old, 7))
				tag_open = "<FONT SIZE=\"+1\">", tag_close = "</FONT>";
			  else if (!nsCRT::strncasecmp ("BLINK>", old, 5))
				/* Of course, both text/richtext and text/enriched must be
				   enhanced *somehow*...  Or else what would people think. */
				tag_open = "<BLINK>", tag_close = "</BLINK>";
			  else if (!nsCRT::strncasecmp ("BOLD>", old, 5))
				tag_open = "<B>", tag_close = "</B>";
			  break;
			case 'c': case 'C':
			  if (!nsCRT::strncasecmp ("CENTER>", old, 7))
				tag_open = "<CENTER>", tag_close = "</CENTER>";
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("COMMENT>", old, 8))
				tag_open = "<!-- ", tag_close = " -->";
			  break;
			case 'e': case 'E':
			  if (!nsCRT::strncasecmp ("EXCERPT>", old, 8))
				tag_open = "<BLOCKQUOTE>", tag_close = "</BLOCKQUOTE>";
			  break;
			case 'f': case 'F':
			  if (!nsCRT::strncasecmp ("FIXED>", old, 6))
				tag_open = "<TT>", tag_close = "</TT>";
			  else if (enriched_p &&
					   !nsCRT::strncasecmp ("FLUSHBOTH>", old, 10))
				tag_open = "<P ALIGN=LEFT>", tag_close = "</P>";
			  else if (!nsCRT::strncasecmp ("FLUSHLEFT>", old, 10))
				tag_open = "<P ALIGN=LEFT>", tag_close = "</P>";
			  else if (!nsCRT::strncasecmp ("FLUSHRIGHT>", old, 11))
				tag_open = "<P ALIGN=RIGHT>", tag_close = "</P>";
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("FOOTING>", old, 8))
				tag_open = "<H6>", tag_close = "</H6>";
			  break;
			case 'h': case 'H':
			  if (!enriched_p &&
				  !nsCRT::strncasecmp ("HEADING>", old, 8))
				tag_open = "<H6>", tag_close = "</H6>";
			  break;
			case 'i': case 'I':
			  if (!nsCRT::strncasecmp ("INDENT>", old, 7))
				tag_open = "<UL>", tag_close = "</UL>";
			  else if (!nsCRT::strncasecmp ("INDENTRIGHT>", old, 12))
				tag_open = 0, tag_close = 0;
/*			  else if (!enriched_p &&
				       !nsCRT::strncasecmp ("ISO-8859-", old, 9))
				tag_open = 0, tag_close = 0; */
			  else if (!nsCRT::strncasecmp ("ITALIC>", old, 7))
				tag_open = "<I>", tag_close = "</I>";
			  break;
			case 'l': case 'L':
			  if (!enriched_p &&
				  !nsCRT::strncasecmp ("LT>", old, 3))
				tag_open = "&lt;", tag_close = 0;
			  break;
			case 'n': case 'N':
			  if (!enriched_p &&
				  !nsCRT::strncasecmp ("NL>", old, 3))
				tag_open = "<BR>", tag_close = 0;
			  if (enriched_p &&
				  !nsCRT::strncasecmp ("NOFILL>", old, 7))
				tag_open = "<NOBR>", tag_close = "</NOBR>";
/*			  else if (!enriched_p &&
				       !nsCRT::strncasecmp ("NO-OP>", old, 6))
				tag_open = 0, tag_close = 0; */
/*			  else if (!enriched_p &&
				       !nsCRT::strncasecmp ("NP>", old, 3))
				tag_open = 0, tag_close = 0; */
			  break;
			case 'o': case 'O':
			  if (!enriched_p &&
				  !nsCRT::strncasecmp ("OUTDENT>", old, 8))
				tag_open = 0, tag_close = 0;
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("OUTDENTRIGHT>", old, 13))
				tag_open = 0, tag_close = 0;
			  break;
			case 'p': case 'P':
			  if (enriched_p &&
				  !nsCRT::strncasecmp ("PARAM>", old, 6))
				tag_open = "<!-- ", tag_close = " -->";
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("PARAGRAPH>", old, 10))
				tag_open = "<P>", tag_close = 0;
			  break;
			case 's': case 'S':
			  if (!enriched_p &&
				  !nsCRT::strncasecmp ("SAMEPAGE>", old, 9))
				tag_open = 0, tag_close = 0;
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("SIGNATURE>", old, 10))
				tag_open = "<I><FONT SIZE=\"-1\">", tag_close = "</FONT></I>";
			  else if (!nsCRT::strncasecmp ("SMALLER>", old, 8))
				tag_open = "<FONT SIZE=\"-1\">", tag_close = "</FONT>";
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("SUBSCRIPT>", old, 10))
				tag_open = "<SUB>", tag_close = "</SUB>";
			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("SUPERSCRIPT>", old, 12))
				tag_open = "<SUP>", tag_close = "</SUP>";
			  break;
			case 'u': case 'U':
			  if (!nsCRT::strncasecmp ("UNDERLINE>", old, 10))
				tag_open = "<U>", tag_close = "</U>";
/*			  else if (!enriched_p &&
					   !nsCRT::strncasecmp ("US-ASCII>", old, 10))
				tag_open = 0, tag_close = 0; */
			  break;
			case 'v': case 'V':
			  if (enriched_p &&
				  !nsCRT::strncasecmp ("VERBATIM>", old, 9))
				tag_open = "<PRE>", tag_close = "</PRE>";
			  break;
			}

		  if (this_start[1] == '/')
			{
			  if (tag_close) PL_strcpy (out, tag_close);
			  out += nsCRT::strlen (out);
			}
		  else
			{
			  if (tag_open) PL_strcpy (out, tag_open);
			  out += nsCRT::strlen (out);
			}
		}

	  /* now go around again */
	  last_end = this_end;
	  this_start = last_end;
	}
  *out = 0;

  return output_fn (*obufferP, out - *obufferP, closure);
}


static int
MimeInlineTextRichtext_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  PRBool enriched_p = (((MimeInlineTextRichtextClass *) obj->clazz)
						->enriched_p);

  return MimeRichtextConvert (line, length,
							  obj->options->output_fn,
							  obj->options->stream_closure,
							  &obj->obuffer, &obj->obuffer_size,
							  enriched_p);
}


static int
MimeInlineTextRichtext_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  char s[] = "";
  if (status < 0) return status;
  return MimeObject_write(obj, s, 0, PR_TRUE); /* force out any separators... */
}


/* This method is largely the same as that of MimeInlineTextHTML; maybe that
   means that MimeInlineTextRichtext and MimeInlineTextEnriched should share
   a common parent with it which is not also shared by MimeInlineTextPlain?
 */
static int
MimeInlineTextRichtext_parse_eof (MimeObject *obj, PRBool abort_p)
{
  int status;
  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  return 0;
}
