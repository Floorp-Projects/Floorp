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

#include "mimeunty.h"
#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "prlog.h"
#include "nsMimeTypes.h"
#include "msgCore.h"

#define MIME_SUPERCLASS mimeContainerClass
MimeDefClass(MimeUntypedText, MimeUntypedTextClass,
			 mimeUntypedTextClass, &MIME_SUPERCLASS);

static int MimeUntypedText_initialize (MimeObject *);
static void MimeUntypedText_finalize (MimeObject *);
static int MimeUntypedText_parse_begin (MimeObject *);
static int MimeUntypedText_parse_line (char *, PRInt32, MimeObject *);

static int MimeUntypedText_open_subpart (MimeObject *obj,
										 MimeUntypedTextSubpartType ttype,
										 const char *type,
										 const char *enc,
										 const char *name,
										 const char *desc);
static int MimeUntypedText_close_subpart (MimeObject *obj);

static PRBool MimeUntypedText_uu_begin_line_p(const char *line, PRInt32 length,
											   MimeDisplayOptions *opt,
											   char **type_ret,
											   char **name_ret);
static PRBool MimeUntypedText_uu_end_line_p(const char *line, PRInt32 length);

static PRBool MimeUntypedText_binhex_begin_line_p(const char *line,
												   PRInt32 length,
												   MimeDisplayOptions *opt);
static PRBool MimeUntypedText_binhex_end_line_p(const char *line,
												 PRInt32 length);

static int
MimeUntypedTextClassInitialize(MimeUntypedTextClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  PR_ASSERT(!oclass->class_initialized);
  oclass->initialize  = MimeUntypedText_initialize;
  oclass->finalize    = MimeUntypedText_finalize;
  oclass->parse_begin = MimeUntypedText_parse_begin;
  oclass->parse_line  = MimeUntypedText_parse_line;
  return 0;
}


static int
MimeUntypedText_initialize (MimeObject *object)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}

static void
MimeUntypedText_finalize (MimeObject *object)
{
  MimeUntypedText *uty = (MimeUntypedText *) object;

  if (uty->open_hdrs)
	{
	  /* Oops, those shouldn't still be here... */
	  MimeHeaders_free(uty->open_hdrs);
	  uty->open_hdrs = 0;
	}

  /* What about the open_subpart?  We're gonna have to assume that it
	 is also on the MimeContainer->children list, and will get cleaned
	 up by that class. */

  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize(object);
}

static int
MimeUntypedText_parse_begin (MimeObject *obj)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
}

static int
MimeUntypedText_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  MimeUntypedText *uty = (MimeUntypedText *) obj;
  int status = 0;
  char *name = 0, *type = 0;
  PRBool begin_line_p = PR_FALSE;

  PR_ASSERT(line && *line);
  if (!line || !*line) return -1;

  /* If we're supposed to write this object, but aren't supposed to convert
	 it to HTML, simply pass it through unaltered. */
  if (obj->output_p &&
	  obj->options &&
	  !obj->options->write_html_p &&
	  obj->options->output_fn)
	return MimeObject_write(obj, line, length, PR_TRUE);


  /* Open a new sub-part if this line demands it.
   */
  if (line[0] == 'b' &&
	  MimeUntypedText_uu_begin_line_p(line, length, obj->options,
									  &type, &name))
	{
	  /* Close the old part and open a new one. */
	  status = MimeUntypedText_open_subpart (obj,
											 MimeUntypedTextSubpartTypeUUE,
											 type, ENCODING_UUENCODE,
											 name, NULL);
	  PR_FREEIF(name);
	  PR_FREEIF(type);
	  if (status < 0) return status;
	  begin_line_p = PR_TRUE;
	}

  else if (line[0] == '(' && line[1] == 'T' &&
		   MimeUntypedText_binhex_begin_line_p(line, length, obj->options))
	{
	  /* Close the old part and open a new one. */
	  status = MimeUntypedText_open_subpart (obj,
											 MimeUntypedTextSubpartTypeBinhex,
											 APPLICATION_BINHEX, NULL,
											 NULL, NULL);
	  if (status < 0) return status;
	  begin_line_p = PR_TRUE;
	}

  /* Open a text/plain sub-part if there is no sub-part open.
   */
  if (!uty->open_subpart)
	{
	  PR_ASSERT(!begin_line_p);
	  status = MimeUntypedText_open_subpart (obj,
											 MimeUntypedTextSubpartTypeText,
											 TEXT_PLAIN, NULL, NULL, NULL);
	  PR_ASSERT(uty->open_subpart);
	  if (!uty->open_subpart) return -1;
	  if (status < 0) return status;
	}

  /* Hand this line to the currently-open sub-part.
   */
  status = uty->open_subpart->clazz->parse_buffer(line, length,
												  uty->open_subpart);
  if (status < 0) return status;

  /* Close this sub-part if this line demands it.
   */
  if (begin_line_p)
	;
  else if (line[0] == 'e' &&
		   uty->type == MimeUntypedTextSubpartTypeUUE &&
		   MimeUntypedText_uu_end_line_p(line, length))
	{
	  status = MimeUntypedText_close_subpart (obj);
	  if (status < 0) return status;
	  PR_ASSERT(!uty->open_subpart);
	}
  else if (uty->type == MimeUntypedTextSubpartTypeBinhex &&
		   MimeUntypedText_binhex_end_line_p(line, length))
	{
	  status = MimeUntypedText_close_subpart (obj);
	  if (status < 0) return status;
	  PR_ASSERT(!uty->open_subpart);
	}

  return 0;
}


static int
MimeUntypedText_close_subpart (MimeObject *obj)
{
  MimeUntypedText *uty = (MimeUntypedText *) obj;
  int status;

  if (uty->open_subpart)
	{
	  status = uty->open_subpart->clazz->parse_eof(uty->open_subpart, PR_FALSE);
	  uty->open_subpart = 0;

	  PR_ASSERT(uty->open_hdrs);
	  if (uty->open_hdrs)
		{
		  MimeHeaders_free(uty->open_hdrs);
		  uty->open_hdrs = 0;
		}
	  uty->type = MimeUntypedTextSubpartTypeText;
	  if (status < 0) return status;

	  /* Never put out a separator between sub-parts of UntypedText.
		 (This bypasses the rule that text/plain subparts always
		 have separators before and after them.)
		 */
	  if (obj->options && obj->options->state)
		obj->options->state->separator_suppressed_p = PR_TRUE;
	}

  PR_ASSERT(!uty->open_hdrs);
  return 0;
}

static int
MimeUntypedText_open_subpart (MimeObject *obj,
							  MimeUntypedTextSubpartType ttype,
							  const char *type,
							  const char *enc,
							  const char *name,
							  const char *desc)
{
  MimeUntypedText *uty = (MimeUntypedText *) obj;
  int status = 0;
  char *h = 0;

  if (!type || !*type || !PL_strcasecmp(type, UNKNOWN_CONTENT_TYPE))
	type = APPLICATION_OCTET_STREAM;
  if (enc && !*enc)
	enc = 0;
  if (desc && !*desc)
	desc = 0;
  if (name && !*name)
	name = 0;

  if (uty->open_subpart)
	{
	  status = MimeUntypedText_close_subpart (obj);
	  if (status < 0) return status;
	}
  PR_ASSERT(!uty->open_subpart);
  PR_ASSERT(!uty->open_hdrs);

  /* To make one of these implicitly-typed sub-objects, we make up a fake
	 header block, containing only the minimum number of MIME headers needed.
	 We could do most of this (Type and Encoding) by making a null header
	 block, and simply setting obj->content_type and obj->encoding; but making
	 a fake header block is better for two reasons: first, it means that
	 something will actually be displayed when in `Show All Headers' mode;
	 and second, it's the only way to communicate the filename parameter,
	 aside from adding a new slot to MimeObject (which is something to be
	 avoided when possible.)
   */

  uty->open_hdrs = MimeHeaders_new();
  if (!uty->open_hdrs) return MK_OUT_OF_MEMORY;

  h = (char *) PR_MALLOC(PL_strlen(type) +
						(enc ? PL_strlen(enc) : 0) +
						(desc ? PL_strlen(desc) : 0) +
						(name ? PL_strlen(name) : 0) +
						100);
  if (!h) return MK_OUT_OF_MEMORY;

  PL_strcpy(h, HEADER_CONTENT_TYPE ": ");
  PL_strcat(h, type);
  PL_strcat(h, LINEBREAK);
  status = MimeHeaders_parse_line(h, PL_strlen(h), uty->open_hdrs);
  if (status < 0) goto FAIL;

  if (enc)
	{
	  PL_strcpy(h, HEADER_CONTENT_TRANSFER_ENCODING ": ");
	  PL_strcat(h, enc);
	  PL_strcat(h, LINEBREAK);
	  status = MimeHeaders_parse_line(h, PL_strlen(h), uty->open_hdrs);
	  if (status < 0) goto FAIL;
	}

  if (desc)
	{
	  PL_strcpy(h, HEADER_CONTENT_DESCRIPTION ": ");
	  PL_strcat(h, desc);
	  PL_strcat(h, LINEBREAK);
	  status = MimeHeaders_parse_line(h, PL_strlen(h), uty->open_hdrs);
	  if (status < 0) goto FAIL;
	}
  if (name)
	{
	  PL_strcpy(h, HEADER_CONTENT_DISPOSITION ": inline; filename=\"");
	  PL_strcat(h, name);
	  PL_strcat(h, "\"" LINEBREAK);
	  status = MimeHeaders_parse_line(h, PL_strlen(h), uty->open_hdrs);
	  if (status < 0) goto FAIL;
	}

  /* push out a blank line. */
  PL_strcpy(h, LINEBREAK);
  status = MimeHeaders_parse_line(h, PL_strlen(h), uty->open_hdrs);
  if (status < 0) goto FAIL;


  /* Create a child... */
  {
	PRBool horrid_kludge = (obj->options && obj->options->state &&
							 obj->options->state->first_part_written_p);
	if (horrid_kludge)
	  obj->options->state->first_part_written_p = PR_FALSE;

	uty->open_subpart = mime_create(type, uty->open_hdrs, obj->options);

	if (horrid_kludge)
	  obj->options->state->first_part_written_p = PR_TRUE;

	if (!uty->open_subpart)
	  {
		status = MK_OUT_OF_MEMORY;
		goto FAIL;
	  }
  }

  /* Add it to the list... */
  status = ((MimeContainerClass *) obj->clazz)->add_child(obj,
														  uty->open_subpart);
  if (status < 0)
	{
	  mime_free(uty->open_subpart);
	  uty->open_subpart = 0;
	  goto FAIL;
	}

  /* And start its parser going. */
  status = uty->open_subpart->clazz->parse_begin(uty->open_subpart);
  if (status < 0)
	{
	  /* MimeContainer->finalize will take care of shutting it down now. */
	  uty->open_subpart = 0;
	  goto FAIL;
	}

  uty->type = ttype;

 FAIL:
  PR_FREEIF(h);

  if (status < 0 && uty->open_hdrs)
	{
	  MimeHeaders_free(uty->open_hdrs);
	  uty->open_hdrs = 0;
	}

  return status;
}

static PRBool
MimeUntypedText_uu_begin_line_p(const char *line, PRInt32 length,
								MimeDisplayOptions *opt,
								char **type_ret, char **name_ret)
{
  const char *s;
  char *name = 0;
  char *type = 0;

  if (type_ret) *type_ret = 0;
  if (name_ret) *name_ret = 0;

  if (PL_strncmp (line, "begin ", 6)) return PR_FALSE;
  /* ...then three or four octal digits. */
  s = line + 6;
  if (*s < '0' || *s > '7') return PR_FALSE;
  s++;
  if (*s < '0' || *s > '7') return PR_FALSE;
  s++;
  if (*s < '0' || *s > '7') return PR_FALSE;
  s++;
  if (*s == ' ')
	s++;
  else
	{
	  if (*s < '0' || *s > '7') return PR_FALSE;
	  s++;
	  if (*s != ' ') return PR_FALSE;
	}

  while (IS_SPACE(*s))
	s++;

  name = (char *) PR_MALLOC(((line+length)-s) + 1);
  if (!name) return PR_FALSE; /* grr... */
  nsCRT::memcpy(name, s, (line+length)-s);
  name[(line+length)-s] = 0;

  /* take off newline. */
  if (name[PL_strlen(name)-1] == LF) name[PL_strlen(name)-1] = 0;
  if (name[PL_strlen(name)-1] == CR) name[PL_strlen(name)-1] = 0;

  /* Now try and figure out a type.
   */
  if (opt && opt->file_type_fn)
	type = opt->file_type_fn(name, opt->stream_closure);
  else
	type = 0;

  if (name_ret)
	*name_ret = name;
  else
	PR_FREEIF(name);

  if (type_ret)
	*type_ret = type;
  else
	PR_FREEIF(type);

  return PR_TRUE;
}

static PRBool
MimeUntypedText_uu_end_line_p(const char *line, PRInt32 length)
{
#if 0
  /* A strictly conforming uuencode end line. */
  return (line[0] == 'e' &&
		  line[1] == 'n' &&
		  line[2] == 'd' &&
		  (line[3] == 0 || IS_SPACE(line[3])));
#else
  /* ...but, why don't we accept any line that begins with the three
	 letters "END" in any case: I've seen lots of partial messages
	 that look like

		BEGIN----- Cut Here-----
		begin 644 foo.gif
		Mxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
		Mxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
		Mxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
		END------- Cut Here-----

	 so let's be lenient here.  (This is only for the untyped-text-plain
	 case -- the uudecode parser itself is strict.)
   */
  return (line[0] == ' ' ||
		  line[0] == '\t' ||
		  ((line[0] == 'e' || line[0] == 'E') &&
		   (line[1] == 'n' || line[1] == 'N') &&
		   (line[2] == 'd' || line[2] == 'D')));
#endif
}


#define BINHEX_MAGIC "(This file must be converted with BinHex 4.0)"
#define BINHEX_MAGIC_LEN 45

static PRBool
MimeUntypedText_binhex_begin_line_p(const char *line, PRInt32 length,
									MimeDisplayOptions *opt)
{
  if (length <= BINHEX_MAGIC_LEN)
	return PR_FALSE;

  while(length > 0 && IS_SPACE(line[length-1]))
	length--;

  if (length != BINHEX_MAGIC_LEN)
	return PR_FALSE;

  if (!PL_strncmp(line, BINHEX_MAGIC, BINHEX_MAGIC_LEN))
	return PR_TRUE;
  else
	return PR_FALSE;
}

static PRBool
MimeUntypedText_binhex_end_line_p(const char *line, PRInt32 length)
{
  if (length > 0 && line[length-1] == LF) length--;
  if (length > 0 && line[length-1] == CR) length--;

  if (length != 0 && length != 64)
	return PR_TRUE;
  else
	return PR_FALSE;
}
