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

#ifndef _MIMEMSIG_H_
#define _MIMEMSIG_H_

#include "mimemult.h"
#include "mimepbuf.h"
#include "modmimee.h"

/* The MimeMultipartSigned class implements the multipart/signed MIME
   container, which provides a general method of associating a cryptographic
   signature to an arbitrary MIME object.

   The MimeMultipartSigned class provides the following methods:

   void *crypto_init (MimeObject *multipart_object)

     This is called with the object, the object->headers of which should be
	 used to initialize the dexlateion engine.  NULL indicates failure;
	 otherwise, an opaque closure object should be returned.

   int crypto_data_hash (char *data, PRInt32 data_size, 
						 void *crypto_closure)

     This is called with the raw data, for which a signature has been computed.
	 The crypto module should examine this, and compute a signature for it.

   int crypto_data_eof (void *crypto_closure, PRBool abort_p)

     This is called when no more data remains.  If `abort_p' is true, then the
	 crypto module may choose to discard any data rather than processing it,
	 as we're terminating abnormally.

   int crypto_signature_init (void *crypto_closure,
                              MimeObject *multipart_object,
							  MimeHeaders *signature_hdrs)

     This is called after crypto_data_eof() and just before the first call to
	 crypto_signature_hash().  The crypto module may wish to do some
	 initialization here, or may wish to examine the actual headers of the
	 signature object itself.

   int crypto_signature_hash (char *data, PRInt32 data_size,
							  void *crypto_closure)

     This is called with the raw data of the detached signature block.  It will
	 be called after crypto_data_eof() has been called to signify the end of
	 the data which is signed.  This data is the data of the signature itself.

   int crypto_signature_eof (void *crypto_closure, PRBool abort_p)

     This is called when no more signature data remains.  If `abort_p' is true,
	 then the crypto module may choose to discard any data rather than
	 processing it, as we're terminating abnormally.

   char * crypto_generate_html (void *crypto_closure)

     This is called after `crypto_signature_eof' but before `crypto_free'.
	 The crypto module should return a newly-allocated string of HTML code
	 which explains the status of the dexlateion to the user (whether the
	 signature checks out, etc.)

   void crypto_free (void *crypto_closure)

     This will be called when we're all done, after `crypto_signature_eof' and
	 `crypto_emit_html'.  It is intended to free any data represented by the
	 crypto_closure.
 */

typedef struct MimeMultipartSignedClass MimeMultipartSignedClass;
typedef struct MimeMultipartSigned      MimeMultipartSigned;

typedef enum {
  MimeMultipartSignedPreamble,
  MimeMultipartSignedBodyFirstHeader,
  MimeMultipartSignedBodyHeaders,
  MimeMultipartSignedBodyFirstLine,
  MimeMultipartSignedBodyLine,
  MimeMultipartSignedSignatureHeaders,
  MimeMultipartSignedSignatureFirstLine,
  MimeMultipartSignedSignatureLine,
  MimeMultipartSignedEpilogue
} MimeMultipartSignedParseState;

struct MimeMultipartSignedClass {
  MimeMultipartClass multipart;

  /* Callbacks used by dexlateion (really, signature verification) module. */
  void * (*crypto_init) (MimeObject *multipart_object);

  int (*crypto_data_hash)      (char *data, PRInt32 data_size,
								void *crypto_closure);
  int (*crypto_signature_hash) (char *data, PRInt32 data_size,
								void *crypto_closure);

  int (*crypto_data_eof)      (void *crypto_closure, PRBool abort_p);
  int (*crypto_signature_eof) (void *crypto_closure, PRBool abort_p);

  int (*crypto_signature_init) (void *crypto_closure,
								MimeObject *multipart_object,
								MimeHeaders *signature_hdrs);

  char * (*crypto_generate_html) (void *crypto_closure);

  void (*crypto_free) (void *crypto_closure);
};

extern "C" MimeMultipartSignedClass mimeMultipartSignedClass;

struct MimeMultipartSigned {
  MimeMultipart multipart;
  MimeMultipartSignedParseState state;	/* State of parser */

  void *crypto_closure;				 	/* Opaque data used by signature
											verification module. */

  MimeHeaders *body_hdrs;				/* The headers of the signed object. */
  MimeHeaders *sig_hdrs;				/* The headers of the signature. */

  MimePartBufferData *part_buffer;	    /* The buffered body of the signed
										   object (see mimepbuf.h) */

  MimeDecoderData *sig_decoder_data;	/* The signature is probably base64
										   encoded; this is the decoder used
										   to get raw bits out of it. */
};

#endif /* _MIMEMSIG_H_ */
