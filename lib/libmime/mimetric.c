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

/* mimetric.c --- definition of the MimeInlineTextRichtext class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "mimetric.h"

#define MIME_SUPERCLASS mimeInlineTextClass
MimeDefClass(MimeInlineTextRichtext, MimeInlineTextRichtextClass,
			 mimeInlineTextRichtextClass, &MIME_SUPERCLASS);

static int MimeInlineTextRichtext_parse_line (char *, int32, MimeObject *);
static int MimeInlineTextRichtext_parse_begin (MimeObject *);
static int MimeInlineTextRichtext_parse_eof (MimeObject *, XP_Bool);

static int
MimeInlineTextRichtextClassInitialize(MimeInlineTextRichtextClass *class)
{
  MimeObjectClass *oclass = (MimeObjectClass *) class;
  XP_ASSERT(!oclass->class_initialized);
  oclass->parse_begin = MimeInlineTextRichtext_parse_begin;
  oclass->parse_line  = MimeInlineTextRichtext_parse_line;
  oclass->parse_eof   = MimeInlineTextRichtext_parse_eof;
  return 0;
}

/* This function has this clunky interface because it needs to be called
   from outside this module (no MimeObject, etc.)
 */
int
MimeRichtextConvert (char *line, int32 length,
					 int (*output_fn) (char *buf, int32 size, void *closure),
					 void *closure,
					 char **obufferP,
					 uint32 *obuffer_sizeP,
					 XP_Bool enriched_p)
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
	status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
							 obufferP, obuffer_sizeP);
  if (status < 0) return status;

  if (enriched_p)
	{
	  for (this_start = line; this_start < line + length; this_start++)
		if (!XP_IS_SPACE (*this_start)) break;
	  if (this_start >= line + length) /* blank line */
		{
		  XP_STRCPY (*obufferP, "<BR>");
		  return output_fn (*obufferP, XP_STRLEN(*obufferP), closure);
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
				 !XP_IS_SPACE (*this_end) &&
				 *this_end != '<' && *this_end != '>' &&
				 *this_end != '&')
			this_end++;
		}

	  this_end++;

	  /* Push out the text preceeding the tag. */
	  if (last_end && last_end != this_start)
		{
		  XP_MEMCPY (out, last_end, this_start - last_end);
		  out += this_start - last_end;
		  *out = 0;
		}

	  if (this_start >= data_end)
		break;
	  else if (*this_start == '&')
		{
		  XP_STRCPY (out, "&amp;"); out += XP_STRLEN (out);
		}
	  else if (*this_start == '>')
		{
		  XP_STRCPY (out, "&gt;"); out += XP_STRLEN (out);
		}
	  else if (enriched_p &&
			   this_start < data_end + 1 &&
			   this_start[0] == '<' &&
			   this_start[1] == '<')
		{
		  XP_STRCPY (out, "&lt;"); out += XP_STRLEN (out);
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
			  if (!strncasecomp ("BIGGER>", old, 7))
				tag_open = "<FONT SIZE=\"+1\">", tag_close = "</FONT>";
			  else if (!strncasecomp ("BLINK>", old, 5))
				/* Of course, both text/richtext and text/enriched must be
				   enhanced *somehow*...  Or else what would people think. */
				tag_open = "<BLINK>", tag_close = "</BLINK>";
			  else if (!strncasecomp ("BOLD>", old, 5))
				tag_open = "<B>", tag_close = "</B>";
			  break;
			case 'c': case 'C':
			  if (!strncasecomp ("CENTER>", old, 7))
				tag_open = "<CENTER>", tag_close = "</CENTER>";
			  else if (!enriched_p &&
					   !strncasecomp ("COMMENT>", old, 8))
				tag_open = "<!-- ", tag_close = " -->";
			  break;
			case 'e': case 'E':
			  if (!strncasecomp ("EXCERPT>", old, 8))
				tag_open = "<BLOCKQUOTE>", tag_close = "</BLOCKQUOTE>";
			  break;
			case 'f': case 'F':
			  if (!strncasecomp ("FIXED>", old, 6))
				tag_open = "<TT>", tag_close = "</TT>";
			  else if (enriched_p &&
					   !strncasecomp ("FLUSHBOTH>", old, 10))
				tag_open = "<P ALIGN=LEFT>", tag_close = "</P>";
			  else if (!strncasecomp ("FLUSHLEFT>", old, 10))
				tag_open = "<P ALIGN=LEFT>", tag_close = "</P>";
			  else if (!strncasecomp ("FLUSHRIGHT>", old, 11))
				tag_open = "<P ALIGN=RIGHT>", tag_close = "</P>";
			  else if (!enriched_p &&
					   !strncasecomp ("FOOTING>", old, 8))
				tag_open = "<H6>", tag_close = "</H6>";
			  break;
			case 'h': case 'H':
			  if (!enriched_p &&
				  !strncasecomp ("HEADING>", old, 8))
				tag_open = "<H6>", tag_close = "</H6>";
			  break;
			case 'i': case 'I':
			  if (!strncasecomp ("INDENT>", old, 7))
				tag_open = "<UL>", tag_close = "</UL>";
			  else if (!strncasecomp ("INDENTRIGHT>", old, 12))
				tag_open = 0, tag_close = 0;
/*			  else if (!enriched_p &&
				       !strncasecomp ("ISO-8859-", old, 9))
				tag_open = 0, tag_close = 0; */
			  else if (!strncasecomp ("ITALIC>", old, 7))
				tag_open = "<I>", tag_close = "</I>";
			  break;
			case 'l': case 'L':
			  if (!enriched_p &&
				  !strncasecomp ("LT>", old, 3))
				tag_open = "&lt;", tag_close = 0;
			  break;
			case 'n': case 'N':
			  if (!enriched_p &&
				  !strncasecomp ("NL>", old, 3))
				tag_open = "<BR>", tag_close = 0;
			  if (enriched_p &&
				  !strncasecomp ("NOFILL>", old, 7))
				tag_open = "<NOBR>", tag_close = "</NOBR>";
/*			  else if (!enriched_p &&
				       !strncasecomp ("NO-OP>", old, 6))
				tag_open = 0, tag_close = 0; */
/*			  else if (!enriched_p &&
				       !strncasecomp ("NP>", old, 3))
				tag_open = 0, tag_close = 0; */
			  break;
			case 'o': case 'O':
			  if (!enriched_p &&
				  !strncasecomp ("OUTDENT>", old, 8))
				tag_open = 0, tag_close = 0;
			  else if (!enriched_p &&
					   !strncasecomp ("OUTDENTRIGHT>", old, 13))
				tag_open = 0, tag_close = 0;
			  break;
			case 'p': case 'P':
			  if (enriched_p &&
				  !strncasecomp ("PARAM>", old, 6))
				tag_open = "<!-- ", tag_close = " -->";
			  else if (!enriched_p &&
					   !strncasecomp ("PARAGRAPH>", old, 10))
				tag_open = "<P>", tag_close = 0;
			  break;
			case 's': case 'S':
			  if (!enriched_p &&
				  !strncasecomp ("SAMEPAGE>", old, 9))
				tag_open = 0, tag_close = 0;
			  else if (!enriched_p &&
					   !strncasecomp ("SIGNATURE>", old, 10))
				tag_open = "<I><FONT SIZE=\"-1\">", tag_close = "</FONT></I>";
			  else if (!strncasecomp ("SMALLER>", old, 8))
				tag_open = "<FONT SIZE=\"-1\">", tag_close = "</FONT>";
			  else if (!enriched_p &&
					   !strncasecomp ("SUBSCRIPT>", old, 10))
				tag_open = "<SUB>", tag_close = "</SUB>";
			  else if (!enriched_p &&
					   !strncasecomp ("SUPERSCRIPT>", old, 12))
				tag_open = "<SUP>", tag_close = "</SUP>";
			  break;
			case 'u': case 'U':
			  if (!strncasecomp ("UNDERLINE>", old, 10))
				tag_open = "<U>", tag_close = "</U>";
/*			  else if (!enriched_p &&
					   !strncasecomp ("US-ASCII>", old, 10))
				tag_open = 0, tag_close = 0; */
			  break;
			case 'v': case 'V':
			  if (enriched_p &&
				  !strncasecomp ("VERBATIM>", old, 9))
				tag_open = "<PRE>", tag_close = "</PRE>";
			  break;
			}

		  if (this_start[1] == '/')
			{
			  if (tag_close) XP_STRCPY (out, tag_close);
			  out += XP_STRLEN (out);
			}
		  else
			{
			  if (tag_open) XP_STRCPY (out, tag_open);
			  out += XP_STRLEN (out);
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
MimeInlineTextRichtext_parse_line (char *line, int32 length, MimeObject *obj)
{
  XP_Bool enriched_p = (((MimeInlineTextRichtextClass *) obj->class)
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
  return MimeObject_write(obj, s, 0, TRUE); /* force out any separators... */
}


/* This method is largely the same as that of MimeInlineTextHTML; maybe that
   means that MimeInlineTextRichtext and MimeInlineTextEnriched should share
   a common parent with it which is not also shared by MimeInlineTextPlain?
 */
static int
MimeInlineTextRichtext_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  int status;
  if (obj->closed_p) return 0;

  /* Run parent method first, to flush out any buffered data. */
  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  if (obj->options &&
	  obj->options->write_html_p &&
	  obj->options->set_html_state_fn)
	{
	  return obj->options->set_html_state_fn(obj->options->stream_closure,
                                                 FALSE, /* layer_encapulate_p */
                                                 FALSE, /* start_p */
                                                 abort_p);
	}
  return 0;
}
