/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "modmimee.h"
#include "mimeleaf.h"
#include "nsMimeTypes.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "nsMimeStringResources.h"
#include "nsCRT.h"

#define MIME_SUPERCLASS mimeObjectClass
MimeDefClass(MimeLeaf, MimeLeafClass, mimeLeafClass, &MIME_SUPERCLASS);

static int MimeLeaf_initialize (MimeObject *);
static void MimeLeaf_finalize (MimeObject *);
static int MimeLeaf_parse_begin (MimeObject *);
static int MimeLeaf_parse_buffer (const char *, PRInt32, MimeObject *);
static int MimeLeaf_parse_line (char *, PRInt32, MimeObject *);
static int MimeLeaf_close_decoder (MimeObject *);
static int MimeLeaf_parse_eof (MimeObject *, PRBool);
static PRBool MimeLeaf_displayable_inline_p (MimeObjectClass *clazz,
											  MimeHeaders *hdrs);

static int
MimeLeafClassInitialize(MimeLeafClass *clazz)
{
  MimeObjectClass *oclass = (MimeObjectClass *) clazz;
  NS_ASSERTION(!oclass->class_initialized, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  oclass->initialize   = MimeLeaf_initialize;
  oclass->finalize     = MimeLeaf_finalize;
  oclass->parse_begin  = MimeLeaf_parse_begin;
  oclass->parse_buffer = MimeLeaf_parse_buffer;
  oclass->parse_line   = MimeLeaf_parse_line;
  oclass->parse_eof    = MimeLeaf_parse_eof;
  oclass->displayable_inline_p = MimeLeaf_displayable_inline_p;
  clazz->close_decoder = MimeLeaf_close_decoder;

  /* Default `parse_buffer' method is one which line-buffers the now-decoded
	 data and passes it on to `parse_line'.  (We snarf the implementation of
	 this method from our superclass's implementation of `parse_buffer', which
	 inherited it from MimeObject.)
   */
  clazz->parse_decoded_buffer =
	((MimeObjectClass*)&MIME_SUPERCLASS)->parse_buffer;

  return 0;
}


static int
MimeLeaf_initialize (MimeObject *obj)
{
  /* This is an abstract class; it shouldn't be directly instanciated. */
  NS_ASSERTION(obj->clazz != (MimeObjectClass *) &mimeLeafClass, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(obj);
}


static void
MimeLeaf_finalize (MimeObject *object)
{
  MimeLeaf *leaf = (MimeLeaf *)object;
  object->clazz->parse_eof (object, PR_FALSE);

  /* Free the decoder data, if it's still around.  It was probably freed
	 in MimeLeaf_parse_eof(), but just in case... */
  if (leaf->decoder_data)
	{
	  MimeDecoderDestroy(leaf->decoder_data, PR_TRUE);
	  leaf->decoder_data = 0;
	}

  ((MimeObjectClass*)&MIME_SUPERCLASS)->finalize (object);
}


static int
MimeLeaf_parse_begin (MimeObject *obj)
{
  MimeLeaf *leaf = (MimeLeaf *) obj;
  MimeDecoderData *(*fn) (nsresult (*) (const char*, PRInt32, void*), void*) = 0;

  /* Initialize a decoder if necessary.
   */
  if (!obj->encoding)
	;
  else if (!nsCRT::strcasecmp(obj->encoding, ENCODING_BASE64))
	fn = &MimeB64DecoderInit;
  else if (!nsCRT::strcasecmp(obj->encoding, ENCODING_QUOTED_PRINTABLE))
	fn = &MimeQPDecoderInit;
  else if (!nsCRT::strcasecmp(obj->encoding, ENCODING_UUENCODE) ||
		   !nsCRT::strcasecmp(obj->encoding, ENCODING_UUENCODE2) ||
		   !nsCRT::strcasecmp(obj->encoding, ENCODING_UUENCODE3) ||
		   !nsCRT::strcasecmp(obj->encoding, ENCODING_UUENCODE4))
	fn = &MimeUUDecoderInit;
  else if (!nsCRT::strcasecmp(obj->encoding, ENCODING_YENCODE))
    fn = &MimeYDecoderInit;

  if (fn)
	{
	  leaf->decoder_data =
		fn (/* The (nsresult (*) ...) cast is to turn the `void' argument
			   into `MimeObject'. */
			((nsresult (*) (const char *, PRInt32, void *))
			 ((MimeLeafClass *)obj->clazz)->parse_decoded_buffer),
			obj);

	  if (!leaf->decoder_data)
		return MIME_OUT_OF_MEMORY;
	}

  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_begin(obj);
}


static int
MimeLeaf_parse_buffer (const char *buffer, PRInt32 size, MimeObject *obj)
{
  MimeLeaf *leaf = (MimeLeaf *) obj;

  NS_ASSERTION(!obj->closed_p, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  if (obj->closed_p) return -1;

  /* If we're not supposed to write this object, bug out now.
   */
  if (!obj->output_p ||
	  !obj->options ||
	  !obj->options->output_fn)
	return 0;

  if (leaf->decoder_data &&
      obj->options && 
      obj->options->format_out != nsMimeOutput::nsMimeMessageDecrypt)
	return MimeDecoderWrite (leaf->decoder_data, buffer, size);
  else
	return ((MimeLeafClass *)obj->clazz)->parse_decoded_buffer (buffer, size,
																obj);
}

static int
MimeLeaf_parse_line (char *line, PRInt32 length, MimeObject *obj)
{
  NS_ASSERTION(0, "1.1 <rhp@netscape.com> 19 Mar 1999 12:00");
  /* This method shouldn't ever be called. */
  return -1;
}


static int
MimeLeaf_close_decoder (MimeObject *obj)
{
  MimeLeaf *leaf = (MimeLeaf *) obj;

  if (leaf->decoder_data)
  {
      int status = MimeDecoderDestroy(leaf->decoder_data, PR_FALSE);
      leaf->decoder_data = 0;
      return status;
  }

  return 0;
}


static int
MimeLeaf_parse_eof (MimeObject *obj, PRBool abort_p)
{
  MimeLeaf *leaf = (MimeLeaf *) obj;
  if (obj->closed_p) return 0;

  /* Close off the decoder, to cause it to give up any buffered data that
	 it is still holding.
   */
  if (leaf->decoder_data)
  {
      int status = MimeLeaf_close_decoder(obj);
      if (status < 0) return status;
  }

  /* Now run the superclass's parse_eof, which will force out the line
	 buffer (which we may have just repopulated, above.)
   */
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->parse_eof (obj, abort_p);
}


static PRBool
MimeLeaf_displayable_inline_p (MimeObjectClass *clazz, MimeHeaders *hdrs)
{
  return PR_TRUE;
}
