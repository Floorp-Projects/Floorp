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

#include "mimesun.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsMimeTypes.h"
#include "msgCore.h"
#include "nsMimeStringResources.h"

#define MIME_SUPERCLASS mimeMultipartClass
MimeDefClass(MimeSunAttachment, MimeSunAttachmentClass,
			 mimeSunAttachmentClass, &MIME_SUPERCLASS);

static MimeMultipartBoundaryType MimeSunAttachment_check_boundary(MimeObject *,
																  const char *,
																  PRInt32);
static int MimeSunAttachment_create_child(MimeObject *);
static int MimeSunAttachment_parse_child_line (MimeObject *, char *, PRInt32,
											   PRBool);
static int MimeSunAttachment_parse_begin (MimeObject *);
static int MimeSunAttachment_parse_eof (MimeObject *, PRBool);

static int
MimeSunAttachmentClassInitialize(MimeSunAttachmentClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartClass *mclass = (MimeMultipartClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);
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
MimeSunAttachment_parse_eof (MimeObject *obj, PRBool abort_p)
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
								 PRInt32 length)
{
  /* ten dashes */

  if (line &&
	  line[0] == '-' && line[1] == '-' && line[2] == '-' && line[3] == '-' &&
	  line[4] == '-' && line[5] == '-' && line[6] == '-' && line[7] == '-' &&
	  line[8] == '-' && line[9] == '-' &&
	  (line[10] == nsCRT::CR || line[10] == nsCRT::LF))
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
									  PR_TRUE, PR_FALSE)
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
		if (!nsCRT::strcasecmp(sun_data_type, sun_types[i].in))
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
	  char *name = MimeHeaders_get_name(mult->hdrs, obj->options);
	  if (name)
		{
		  mime_ct2 = obj->options->file_type_fn(name,
											    obj->options->stream_closure);
		  mime_ct = mime_ct2;
		  PR_Free(name);
		  if (!mime_ct2 || !nsCRT::strcasecmp (mime_ct2, UNKNOWN_CONTENT_TYPE))
			{
			  PR_FREEIF(mime_ct2);
			  mime_ct = APPLICATION_OCTET_STREAM;
			}
		}
	}
  if (!mime_ct)
	mime_ct = APPLICATION_OCTET_STREAM;

  PR_FREEIF(sun_data_type);


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
									  PR_FALSE,PR_FALSE)
				   : 0);
  sun_enc_info = sun_data_type;


  /* this "adpcm-compress" pseudo-encoding is some random junk that
	 MailTool adds to the encoding description of .AU files: we can
	 ignore it if it is the leftmost element of the encoding field.
	 (It looks like it's created via `audioconvert -f g721'.  Why?
	 Who knows.)
   */
  if (sun_enc_info && !nsCRT::strncasecmp (sun_enc_info, "adpcm-compress", 14))
	{
	  sun_enc_info += 14;
	  while (nsCRT::IsAsciiSpace(*sun_enc_info) || *sun_enc_info == ',')
		sun_enc_info++;
	}

  /* Extract the last element of the encoding field, changing the content
	 type if necessary (as described above.)
   */
  if (sun_enc_info && *sun_enc_info)
	{
	  const char *prev;
	  const char *end = PL_strrchr(sun_enc_info, ',');
	  if (end)
		{
		  const char *start = sun_enc_info;
		  sun_enc_info = end + 1;
		  while (nsCRT::IsAsciiSpace(*sun_enc_info))
			sun_enc_info++;
		  for (prev = end-1; prev > start && *prev != ','; prev--)
			;
		  if (*prev == ',') prev++;

		  if (!nsCRT::strncasecmp (prev, "uuencode", end-prev))
			mime_ct = APPLICATION_UUENCODE;
		  else if (!nsCRT::strncasecmp (prev, "gzip", end-prev))
			mime_ct = APPLICATION_GZIP;
		  else if (!nsCRT::strncasecmp (prev, "compress", end-prev))
			mime_ct = APPLICATION_COMPRESS;
		  else if (!nsCRT::strncasecmp (prev, "default-compress", end-prev))
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
  else if (!nsCRT::strcasecmp(sun_enc_info,"compress")) mime_cte = ENCODING_COMPRESS;
  else if (!nsCRT::strcasecmp(sun_enc_info,"uuencode")) mime_cte = ENCODING_UUENCODE;
  else if (!nsCRT::strcasecmp(sun_enc_info,"gzip"))	  mime_cte = ENCODING_GZIP;
  else										mime_ct = APPLICATION_OCTET_STREAM;

  PR_FREEIF(sun_data_type);


  /* Now that we know its type and encoding, create a MimeObject to represent
	 this part.
   */
  child = mime_create(mime_ct, mult->hdrs, obj->options);
  if (!child)
	{
	  status = MIME_OUT_OF_MEMORY;
	  goto FAIL;
	}

  /* Fake out the child's content-type and encoding (it probably doesn't have
	 one right now, because the X-Sun- headers aren't generally recognised by
	 the rest of this library.)
   */
  PR_FREEIF(child->content_type);
  PR_FREEIF(child->encoding);
  PR_ASSERT(mime_ct);
  child->content_type = (mime_ct  ? nsCRT::strdup(mime_ct)  : 0);
  child->encoding     = (mime_cte ? nsCRT::strdup(mime_cte) : 0);

  status = ((MimeContainerClass *) obj->clazz)->add_child(obj, child);
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
  status = child->clazz->parse_begin(child);
  if (status < 0) goto FAIL;

 FAIL:
  PR_FREEIF(mime_ct2);
  PR_FREEIF(sun_data_type);
  return status;
}


static int
MimeSunAttachment_parse_child_line (MimeObject *obj, char *line, PRInt32 length,
									PRBool first_line_p)
{
  MimeContainer *cont = (MimeContainer *) obj;
  MimeObject *kid;

  /* This is simpler than MimeMultipart->parse_child_line in that it doesn't
	 play games about body parts without trailing newlines.
   */

  PR_ASSERT(cont->nchildren > 0);
  if (cont->nchildren <= 0)
	return -1;

  kid = cont->children[cont->nchildren-1];
  PR_ASSERT(kid);
  if (!kid) return -1;

  return kid->clazz->parse_buffer (line, length, kid);
}
