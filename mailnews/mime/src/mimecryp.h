/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   David Drinan <ddrinan@netscape.com>
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

#ifndef _MIMECRYP_H_
#define _MIMECRYP_H_

#include "mimecont.h"
// #include "mimeenc.h"
#include "modmimee.h"
#include "mimepbuf.h"

/* The MimeEncrypted class implements a type of MIME object where the object
   is passed to some other routine, which then returns a new MIME object.
   This is the basis of a decryption module.

   Oddly, this class behaves both as a container and as a leaf: it acts as a
   container in that it parses out data in order to eventually present a
   contained object; however, it acts as a leaf in that this container may
   itself have a Content-Transfer-Encoding applied to its body.  This violates
   the cardinal rule of MIME containers, which is that encodings don't nest,
   and therefore containers can't have encodings.  But, the fact that the
   S/MIME spec doesn't follow the groundwork laid down by previous MIME specs
   isn't something we can do anything about at this point...

   Therefore, this class duplicates some of the work done by the MimeLeaf
   class, to meet its dual goals of container-hood and leaf-hood.  (We could
   alternately have made this class be a subclass of leaf, and had it duplicate
   the effort of MimeContainer, but that seemed like the harder approach.)

   The MimeEncrypted class provides the following methods:

   void *crypto_init(MimeObject *obj,
					 int (*output_fn) (const char *data, int32 data_size,
									   void *output_closure),
					 void *output_closure)

     This is called with the MimeObject representing the encrypted data.
	 The obj->headers should be used to initialize the decryption engine.
	 NULL indicates failure; otherwise, an opaque closure object should
	 be returned.

	 output_fn is what the decryption module should use to write a new MIME
	 object (the decrypted data.)  output_closure should be passed along to
	 every call to the output routine.

	 The data sent to output_fn should begin with valid MIME headers indicating
	 the type of the data.  For example, if decryption resulted in a text
	 document, the data fed through to the output_fn might minimally look like

				Content-Type: text/plain

				This is the decrypted data.
				It is only two lines long.

     Of course, the data may be of any MIME type, including multipart/mixed.
	 Any returned MIME object will be recursively processed and presented
	 appropriately.  (This also imples that encrypted objects may nest, and
	 thus that the underlying decryption module must be reentrant.)

   int crypto_write (const char *data, int32 data_size, void *crypto_closure)

     This is called with the raw encrypted data.  This data might not come
	 in line-based chunks: if there was a Content-Transfer-Encoding applied
	 to the data (base64 or quoted-printable) then it will have been decoded
	 first (handing binary data to the filter_fn.)  `crypto_closure' is the
	 object that `crypto_init' returned.  This may return negative on error.

   int crypto_eof (void *crypto_closure, PRBool abort_p)

     This is called when no more data remains.  It may call `output_fn' again
	 to flush out any buffered data.  If `abort_p' is true, then it may choose
	 to discard any data rather than processing it, as we're terminating
	 abnormally.

   char * crypto_generate_html (void *crypto_closure)

     This is called after `crypto_eof' but before `crypto_free'.  The crypto
	 module should return a newly-allocated string of HTML code which
	 explains the status of the decryption to the user (whether the signature
	 checked out, etc.)

   void crypto_free (void *crypto_closure)

     This will be called when we're all done, after `crypto_eof' and
	 `crypto_emit_html'.  It is intended to free any data represented
	 by the crypto_closure.  output_fn may not be called.


   int (*parse_decoded_buffer) (char *buf, int32 size, MimeObject *obj)

     This method, of the same name as one in MimeLeaf, is a part of the
	 afforementioned leaf/container hybridization.  This method is invoked
	 with the content-transfer-decoded body of this part (without line
	 buffering.)  The default behavior of this method is to simply invoke
	 `crypto_write' on the data with which it is called.  It's unlikely that
	 a subclass will need to specialize this.
 */

typedef struct MimeEncryptedClass MimeEncryptedClass;
typedef struct MimeEncrypted      MimeEncrypted;

struct MimeEncryptedClass {
  MimeContainerClass container;

  /* Duplicated from MimeLeaf, see comments above.
     This is the callback that is handed to the decoder. */
  int (*parse_decoded_buffer) (char *buf, PRInt32 size, MimeObject *obj);


  /* Callbacks used by decryption module. */
  void * (*crypto_init) (MimeObject *obj,
						 int (*output_fn) (const char *data, PRInt32 data_size,
										   void *output_closure),
						 void *output_closure);
  int (*crypto_write) (const char *data, PRInt32 data_size,
					   void *crypto_closure);
  int (*crypto_eof) (void *crypto_closure, PRBool abort_p);
  char * (*crypto_generate_html) (void *crypto_closure);
  void (*crypto_free) (void *crypto_closure);
};

extern MimeEncryptedClass mimeEncryptedClass;

struct MimeEncrypted {
  MimeContainer container;		 /* superclass variables */
  void *crypto_closure;			 /* Opaque data used by decryption module. */
  MimeDecoderData *decoder_data; /* Opaque data for the Transfer-Encoding
									decoder. */
  MimeHeaders *hdrs;			 /* Headers of the enclosed object (including
									the type of the *decrypted* data.) */
  MimePartBufferData *part_buffer;	/* The data of the decrypted enclosed
									   object (see mimepbuf.h) */
};

#endif /* _MIMECRYP_H_ */
