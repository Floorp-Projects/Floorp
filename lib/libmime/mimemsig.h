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

/* mimemsig.h --- definition of the MimeMultipartSigned class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 23-Sep-96.
 */

#ifndef _MIMEMSIG_H_
#define _MIMEMSIG_H_

#include "mimemult.h"
#include "mimepbuf.h"
#include "mimeenc.h"

/* The MimeMultipartSigned class implements the multipart/signed MIME
   container, which provides a general method of associating a cryptographic
   signature to an arbitrary MIME object.

   The MimeMultipartSigned class provides the following methods:

   void *crypto_init (MimeObject *multipart_object)

     This is called with the object, the object->headers of which should be
	 used to initialize the decryption engine.  NULL indicates failure;
	 otherwise, an opaque closure object should be returned.

   int crypto_data_hash (char *data, int32 data_size, 
						 void *crypto_closure)

     This is called with the raw data, for which a signature has been computed.
	 The crypto module should examine this, and compute a signature for it.

   int crypto_data_eof (void *crypto_closure, XP_Bool abort_p)

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

   int crypto_signature_hash (char *data, int32 data_size,
							  void *crypto_closure)

     This is called with the raw data of the detached signature block.  It will
	 be called after crypto_data_eof() has been called to signify the end of
	 the data which is signed.  This data is the data of the signature itself.

   int crypto_signature_eof (void *crypto_closure, XP_Bool abort_p)

     This is called when no more signature data remains.  If `abort_p' is true,
	 then the crypto module may choose to discard any data rather than
	 processing it, as we're terminating abnormally.

   char * crypto_generate_html (void *crypto_closure)

     This is called after `crypto_signature_eof' but before `crypto_free'.
	 The crypto module should return a newly-allocated string of HTML code
	 which explains the status of the decryption to the user (whether the
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

  /* Callbacks used by decryption (really, signature verification) module. */
  void * (*crypto_init) (MimeObject *multipart_object);

  int (*crypto_data_hash)      (char *data, int32 data_size,
								void *crypto_closure);
  int (*crypto_signature_hash) (char *data, int32 data_size,
								void *crypto_closure);

  int (*crypto_data_eof)      (void *crypto_closure, XP_Bool abort_p);
  int (*crypto_signature_eof) (void *crypto_closure, XP_Bool abort_p);

  int (*crypto_signature_init) (void *crypto_closure,
								MimeObject *multipart_object,
								MimeHeaders *signature_hdrs);

  char * (*crypto_generate_html) (void *crypto_closure);

  void (*crypto_free) (void *crypto_closure);
};

extern MimeMultipartSignedClass mimeMultipartSignedClass;

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
