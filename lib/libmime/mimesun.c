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

/* mimemult.h --- definition of the MimeMultipart class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */

#include "mimesun.h"

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeSunAttachment, MimeSunAttachmentClass,
			 mimeSunAttachmentClass, &MIME_SUPERCLASS);

static MimeMultipartBoundaryType MimeSunAttachment_check_boundary(MimeObject *,
																  const char *,
																  int32);
static int MimeSunAttachment_create_child(MimeObject *);
static int MimeSunAttachment_parse_child_line (MimeObject *, char *, int32,
											   XP_Bool);
static int MimeSunAttachment_parse_begin (MimeObject *);
static int MimeSunAttachment_parse_eof (MimeObject *, XP_Bool);

static int
MimeSunAttachmentClassInitialize(MimeSunAttachmentClass *class)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    class;
  MimeMultipartClass *mclass = (MimeMultipartClass *) class;

  XP_ASSERT(!oclass->class_initialized);
  oclass->parse_begin      = MimeSunAttachment_parse_begin;
  oclass->parse_eof        = MimeSunAttachment_parse_eof;
  mclass->check_boundary   = MimeSunAttachment_check_boundary;
  mclass->create_child     = MimeSunAttachment_create_child;
  mclass->parse_child_line = MimeSunAttachment_parse_child_line;
  return 0;
}


static int
MimeSunAttachment_parse_begin (MimeObject *obj)
{
  int status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
  if (status < 0) return status;

  /* Sun messages always have separators at the beginning. */
  return MimeObject_write_separator(obj);
}

static int
MimeSunAttachment_parse_eof (MimeObject *obj, XP_Bool abort_p)
{
  int status = 0;

  status = ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof(obj, abort_p);
  if (status < 0) return status;

  /* Sun messages always have separators at the end. */
  if (!abort_p)
	{
	  status = MimeObject_write_separator(obj);
	  if (status < 0) return status;
	}

  return 0;
}


static MimeMultipartBoundaryType
MimeSunAttachment_check_boundary(MimeObject *obj, const char *line,
								 int32 length)
{
  /* ten dashes */

  if (line &&
	  line[0] == '-' && line[1] == '-' && line[2] == '-' && line[3] == '-' &&
	  line[4] == '-' && line[5] == '-' && line[6] == '-' && line[7] == '-' &&
	  line[8] == '-' && line[9] == '-' &&
	  (line[10] == CR || line[10] == LF))
	return MimeMultipartBoundaryTypeSeparator;
  else
	return MimeMultipartBoundaryTypeNone;
}


static int
MimeSunAttachment_create_child(MimeObject *obj)
{
  MimeMultipart *mult = (MimeMultipart *) obj;
  int status = 0;

  char *sun_data_type = 0;
  const char *mime_ct = 0, *sun_enc_info = 0, *mime_cte = 0;
  char *mime_ct2 = 0;    /* sometimes we need to copy; this is for freeing. */
  MimeObject *child = 0;

  mult->state = MimeMultipartPartLine;

  sun_data_type = (mult->hdrs
				   ? MimeHeaders_get (mult->hdrs, HEADER_X_SUN_DATA_TYPE,
									  TRUE, FALSE)
				   : 0);
  if (sun_data_type)
	{
	  int i;
	  static const struct { const char *in, *out; } sun_types[] = {

		/* Convert recognised Sun types to the corresponding MIME types,
		   and convert unrecognized ones based on the file extension and
		   the mime.types file.

		   These are the magic types used by MailTool that I can determine.
		   The only actual written spec I've found only listed the first few.
		   The rest were found by inspection (both of real-world messages,
		   and by running `strings' on the MailTool binary, and on the file
		   /usr/openwin/lib/cetables/cetables (the "Class Engine", Sun's
		   equivalent to .mailcap and mime.types.)
		 */
		{ "default",				TEXT_PLAIN },
		{ "default-doc",			TEXT_PLAIN },
		{ "text",					TEXT_PLAIN },
		{ "scribe",					TEXT_PLAIN },
		{ "sgml",					TEXT_PLAIN },
		{ "tex",					TEXT_PLAIN },
		{ "troff",					TEXT_PLAIN },
		{ "c-file",					TEXT_PLAIN },
		{ "h-file",					TEXT_PLAIN },
		{ "readme-file",			TEXT_PLAIN },
		{ "shell-script",			TEXT_PLAIN },
		{ "cshell-script",			TEXT_PLAIN },
		{ "makefile",				TEXT_PLAIN },
		{ "hidden-docs",			TEXT_PLAIN },
		{ "message",				MESSAGE_RFC822 },
		{ "mail-message",			MESSAGE_RFC822 },
		{ "mail-file",				TEXT_PLAIN },
		{ "gif-file",				IMAGE_GIF },
		{ "jpeg-file",				IMAGE_JPG },
		{ "ppm-file",				IMAGE_PPM },
		{ "pgm-file",				"image/x-portable-graymap" },
		{ "pbm-file",				"image/x-portable-bitmap" },
		{ "xpm-file",				"image/x-xpixmap" },
		{ "ilbm-file",				"image/ilbm" },
		{ "tiff-file",				"image/tiff" },
		{ "photocd-file",			"image/x-photo-cd" },
		{ "sun-raster",				"image/x-sun-raster" },
		{ "audio-file",				AUDIO_BASIC },
		{ "postscript",				APPLICATION_POSTSCRIPT },
		{ "postscript-file",		APPLICATION_POSTSCRIPT },
		{ "framemaker-document",	"application/x-framemaker" },
		{ "sundraw-document",		"application/x-sun-draw" },
		{ "sunpaint-document",		"application/x-sun-paint" },
		{ "sunwrite-document",		"application/x-sun-write" },
		{ "islanddraw-document",	"application/x-island-draw" },
		{ "islandpaint-document",	"application/x-island-paint" },
		{ "islandwrite-document",	"application/x-island-write" },
		{ "sun-executable",			APPLICATION_OCTET_STREAM },
		{ "default-app",			APPLICATION_OCTET_STREAM },
		{ 0, 0 }};
	  for (i = 0; sun_types[i].in; i++)
		if (!strcasecomp(sun_data_type, sun_types[i].in))
		  {
			mime_ct = sun_types[i].out;
			break;
		  }
	}

  /* If we didn't find a type, look at the extension on the file name.
   */
  if (!mime_ct &&
	  obj->options &&
	  obj->options->file_type_fn)
	{
	  char *name = MimeHeaders_get_name(mult->hdrs);
	  if (name)
		{
		  mime_ct2 = obj->options->file_type_fn(name,
											    obj->options->stream_closure);
		  mime_ct = mime_ct2;
		  XP_FREE(name);
		  if (!mime_ct2 || !strcasecomp (mime_ct2, UNKNOWN_CONTENT_TYPE))
			{
			  FREEIF(mime_ct2);
			  mime_ct = APPLICATION_OCTET_STREAM;
			}
		}
	}
  if (!mime_ct)
	mime_ct = APPLICATION_OCTET_STREAM;

  FREEIF(sun_data_type);


  /* Convert recognised Sun encodings to the corresponding MIME encodings.
	 However, if the X-Sun-Encoding-Info field contains more than one
	 encoding (that is, contains a comma) then assign it the encoding of
	 the *rightmost* element in the list; and change its Content-Type to
	 application/octet-stream.  Examples:

             Sun Type:                    Translates To:
        ==================            ====================
        type:     TEXT                type:     text/plain
        encoding: COMPRESS            encoding: x-compress

        type:     POSTSCRIPT          type:     application/x-compress
        encoding: COMPRESS,UUENCODE   encoding: x-uuencode

        type:     TEXT                type:     application/octet-stream
        encoding: UNKNOWN,UUENCODE    encoding: x-uuencode
   */

  sun_data_type = (mult->hdrs
				   ? MimeHeaders_get (mult->hdrs, HEADER_X_SUN_ENCODING_INFO,
									  FALSE,FALSE)
				   : 0);
  sun_enc_info = sun_data_type;


  /* this "adpcm-compress" pseudo-encoding is some random junk that
	 MailTool adds to the encoding description of .AU files: we can
	 ignore it if it is the leftmost element of the encoding field.
	 (It looks like it's created via `audioconvert -f g721'.  Why?
	 Who knows.)
   */
  if (sun_enc_info && !strncasecomp (sun_enc_info, "adpcm-compress", 14))
	{
	  sun_enc_info += 14;
	  while (XP_IS_SPACE(*sun_enc_info) || *sun_enc_info == ',')
		sun_enc_info++;
	}

  /* Extract the last element of the encoding field, changing the content
	 type if necessary (as described above.)
   */
  if (sun_enc_info && *sun_enc_info)
	{
	  const char *prev;
	  const char *end = XP_STRRCHR(sun_enc_info, ',');
	  if (end)
		{
		  const char *start = sun_enc_info;
		  sun_enc_info = end + 1;
		  while (XP_IS_SPACE(*sun_enc_info))
			sun_enc_info++;
		  for (prev = end-1; prev > start && *prev != ','; prev--)
			;
		  if (*prev == ',') prev++;

		  if (!strncasecomp (prev, "uuencode", end-prev))
			mime_ct = APPLICATION_UUENCODE;
		  else if (!strncasecomp (prev, "gzip", end-prev))
			mime_ct = APPLICATION_GZIP;
		  else if (!strncasecomp (prev, "compress", end-prev))
			mime_ct = APPLICATION_COMPRESS;
		  else if (!strncasecomp (prev, "default-compress", end-prev))
			mime_ct = APPLICATION_COMPRESS;
		  else
			mime_ct = APPLICATION_OCTET_STREAM;
		}
	}

  /* Convert the remaining Sun encoding to a MIME encoding.
	 If it isn't known, change the content-type instead.
   */
  if (!sun_enc_info || !*sun_enc_info)
	;
  else if (!strcasecomp(sun_enc_info,"compress")) mime_cte = ENCODING_COMPRESS;
  else if (!strcasecomp(sun_enc_info,"uuencode")) mime_cte = ENCODING_UUENCODE;
  else if (!strcasecomp(sun_enc_info,"gzip"))	  mime_cte = ENCODING_GZIP;
  else										mime_ct = APPLICATION_OCTET_STREAM;

  FREEIF(sun_data_type);


  /* Now that we know its type and encoding, create a MimeObject to represent
	 this part.
   */
  child = mime_create(mime_ct, mult->hdrs, obj->options);
  if (!child)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  /* Fake out the child's content-type and encoding (it probably doesn't have
	 one right now, because the X-Sun- headers aren't generally recognised by
	 the rest of this library.)
   */
  FREEIF(child->content_type);
  FREEIF(child->encoding);
  XP_ASSERT(mime_ct);
  child->content_type = (mime_ct  ? XP_STRDUP(mime_ct)  : 0);
  child->encoding     = (mime_cte ? XP_STRDUP(mime_cte) : 0);

  status = ((MimeContainerClass *) obj->class)->add_child(obj, child);
  if (status < 0)
	{
	  mime_free(child);
	  child = 0;
	  goto FAIL;
	}

  /* Sun attachments always have separators between parts. */
  status = MimeObject_write_separator(obj);
  if (status < 0) goto FAIL;

  /* And now that we've added this new object to our list of
	 children, start its parser going. */
  status = child->class->parse_begin(child);
  if (status < 0) goto FAIL;

 FAIL:
  FREEIF(mime_ct2);
  FREEIF(sun_data_type);
  return status;
}


static int
MimeSunAttachment_parse_child_line (MimeObject *obj, char *line, int32 length,
									XP_Bool first_line_p)
{
  MimeContainer *cont = (MimeContainer *) obj;
  MimeObject *kid;

  /* This is simpler than MimeMultipart->parse_child_line in that it doesn't
	 play games about body parts without trailing newlines.
   */

  XP_ASSERT(cont->nchildren > 0);
  if (cont->nchildren <= 0)
	return -1;

  kid = cont->children[cont->nchildren-1];
  XP_ASSERT(kid);
  if (!kid) return -1;

  return kid->class->parse_buffer (line, length, kid);
}
