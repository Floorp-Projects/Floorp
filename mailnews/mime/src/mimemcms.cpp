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

#include "mimemcms.h"
#include "nsMimeTypes.h"
#include "nspr.h"
#include "nsMimeStringResources.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"

#define MIME_SUPERCLASS mimeMultipartSignedClass
MimeDefClass(MimeMultipartSignedCMS, MimeMultipartSignedCMSClass,
			 mimeMultipartSignedCMSClass, &MIME_SUPERCLASS);

static int MimeMultipartSignedCMS_initialize (MimeObject *);

static void *MimeMultCMS_init (MimeObject *);
static int MimeMultCMS_data_hash (char *, PRInt32, void *);
static int MimeMultCMS_sig_hash  (char *, PRInt32, void *);
static int MimeMultCMS_data_eof (void *, PRBool);
static int MimeMultCMS_sig_eof  (void *, PRBool);
static int MimeMultCMS_sig_init (void *, MimeObject *, MimeHeaders *);
static char * MimeMultCMS_generate (void *);
static void MimeMultCMS_free (void *);
static void MimeMultCMS_get_content_info (MimeObject *,
											nsICMSMessage **,
											char **, PRInt32 *, PRInt32 *, PRBool *);

extern int SEC_ERROR_CERT_ADDR_MISMATCH;

static int
MimeMultipartSignedCMSClassInitialize(MimeMultipartSignedCMSClass *clazz)
{
  MimeObjectClass          *oclass = (MimeObjectClass *)    clazz;
  MimeMultipartSignedClass *sclass = (MimeMultipartSignedClass *) clazz;

  oclass->initialize  = MimeMultipartSignedCMS_initialize;

  sclass->crypto_init           = MimeMultCMS_init;
  sclass->crypto_data_hash      = MimeMultCMS_data_hash;
  sclass->crypto_data_eof       = MimeMultCMS_data_eof;
  sclass->crypto_signature_init = MimeMultCMS_sig_init;
  sclass->crypto_signature_hash = MimeMultCMS_sig_hash;
  sclass->crypto_signature_eof  = MimeMultCMS_sig_eof;
  sclass->crypto_generate_html  = MimeMultCMS_generate;
  sclass->crypto_free           = MimeMultCMS_free;

  clazz->get_content_info	    = MimeMultCMS_get_content_info;

  PR_ASSERT(!oclass->class_initialized);
  return 0;
}

static int
MimeMultipartSignedCMS_initialize (MimeObject *object)
{
  return ((MimeObjectClass*)&MIME_SUPERCLASS)->initialize(object);
}


typedef struct MimeMultCMSdata {
  PRInt16 hash_type;
  nsCOMPtr<nsIHash> data_hash_context;
  nsCOMPtr<nsICMSDecoder> sig_decoder_context;
  nsCOMPtr<nsICMSMessage> content_info;
  char *sender_addr;
  PRInt32 decode_error;
  PRInt32 verify_error;
  unsigned char* item_data;
  PRUint32 item_len;
  MimeObject *self;
  PRBool parent_is_encrypted_p;
  PRBool parent_holds_stamp_p;
} MimeMultCMSdata;


static void
MimeMultCMS_get_content_info(MimeObject *obj,
							   nsICMSMessage **content_info_ret,
							   char **sender_email_addr_return,
							   PRInt32 *decode_error_ret,
							   PRInt32 *verify_error_ret,
                               PRBool *ci_is_encrypted)
{
  MimeMultipartSigned *msig = (MimeMultipartSigned *) obj;
  if (msig && msig->crypto_closure)
	{
	  MimeMultCMSdata *data = (MimeMultCMSdata *) msig->crypto_closure;

	  *decode_error_ret = data->decode_error;
	  *verify_error_ret = data->verify_error;
	  *content_info_ret = data->content_info;
      *ci_is_encrypted     = PR_FALSE;

	  if (sender_email_addr_return)
		*sender_email_addr_return = (data->sender_addr
                   ? nsCRT::strdup(data->sender_addr)
									 : 0);
	}
}

/* #### MimeEncryptedCMS and MimeMultipartSignedCMS have a sleazy,
        incestuous, dysfunctional relationship. */
extern PRBool MimeEncryptedCMS_encrypted_p (MimeObject *obj);
extern PRBool MimeCMSHeadersAndCertsMatch(MimeObject *obj,
											 nsICMSMessage *,
											 char **);
extern char *MimeCMS_MakeSAURL(MimeObject *obj);
extern char *IMAP_CreateReloadAllPartsUrl(const char *url);

static void *
MimeMultCMS_init (MimeObject *obj)
{
  MimeHeaders *hdrs = obj->headers;
  MimeMultCMSdata *data = 0;
  char *ct, *micalg;
  PRInt16 hash_type;
  nsresult rv;

  ct = MimeHeaders_get (hdrs, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
  if (!ct) return 0; /* #### bogus message?  out of memory? */
  micalg = MimeHeaders_get_parameter (ct, PARAM_MICALG, NULL, NULL);
  PR_Free(ct);
  ct = 0;
  if (!micalg) return 0; /* #### bogus message?  out of memory? */

  if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_MD5))
	hash_type = nsIHash::HASH_AlgMD5;
  else if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1) ||
		   !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_2) ||
		   !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_3) ||
		   !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_4) ||
		   !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_5))
	hash_type = nsIHash::HASH_AlgSHA1;
  else if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_MD2))
	hash_type = nsIHash::HASH_AlgMD2;
  else
	hash_type = nsIHash::HASH_AlgNULL;

  PR_Free(micalg);
  micalg = 0;

  if (hash_type == nsIHash::HASH_AlgNULL) return 0; /* #### bogus message? */

  data = (MimeMultCMSdata *) PR_MALLOC(sizeof(*data));
  if (!data) return 0;

  nsCRT::memset(data, 0, sizeof(*data));

  data->self = obj;
  data->hash_type = hash_type;

  PR_ASSERT(!data->data_hash_context);
  PR_ASSERT(!data->sig_decoder_context);

  data->data_hash_context = do_CreateInstance(NS_HASH_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->data_hash_context->Create(data->hash_type);
  if (NS_FAILED(rv)) return 0;

  PR_SetError(0,0);
  data->data_hash_context->Begin();
  if (!data->decode_error)
	{
	  data->decode_error = PR_GetError();
	  if (data->decode_error)
		{
		  PR_Free(data);
		  return 0;
		}
	}

  data->parent_holds_stamp_p =
	(obj->parent && mime_crypto_stamped_p(obj->parent));

  data->parent_is_encrypted_p =
	(obj->parent && MimeEncryptedCMS_encrypted_p (obj->parent));

  /* If the parent of this object is a crypto-blob, then it's the grandparent
	 who would have written out the headers and prepared for a stamp...
	 (This s##t s$%#s.)
   */
  if (data->parent_is_encrypted_p &&
	  !data->parent_holds_stamp_p &&
	  obj->parent && obj->parent->parent)
	data->parent_holds_stamp_p =
	  mime_crypto_stamped_p (obj->parent->parent);

  return data;
}

static int
MimeMultCMS_data_hash (char *buf, PRInt32 size, void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  PR_ASSERT(data && data->data_hash_context);
  if (!data || !data->data_hash_context) return -1;

  PR_ASSERT(!data->sig_decoder_context);

  PR_SetError(0, 0);
  data->data_hash_context->Update((unsigned char *) buf, size);
  if (!data->verify_error)
	data->verify_error = PR_GetError();

  return 0;
}

static int
MimeMultCMS_data_eof (void *crypto_closure, PRBool abort_p)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  PR_ASSERT(data && data->data_hash_context);
  if (!data || !data->data_hash_context) return -1;

  PR_ASSERT(!data->sig_decoder_context);

  data->data_hash_context->ResultLen(data->hash_type, &data->item_len);
  data->item_data = (unsigned char *) PR_MALLOC(data->item_len);
  if (!data->item_data) return MIME_OUT_OF_MEMORY;

  PR_SetError(0, 0);
  data->data_hash_context->End(data->item_data, &data->item_len, data->item_len);
  if (!data->verify_error)
	data->verify_error = PR_GetError();

  // Release our reference to nsIHash //
  data->data_hash_context = 0;

  /* At this point, data->item.data contains a digest for the first part.
	 When we process the signature, the security library will compare this
	 digest to what's in the signature object. */

  return 0;
}


static int
MimeMultCMS_sig_init (void *crypto_closure,
						MimeObject *multipart_object,
						MimeHeaders *signature_hdrs)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  MimeDisplayOptions *opts = multipart_object->options;
  char *ct;
  int status = 0;
  nsresult rv;

  PR_ASSERT(!data->data_hash_context);
  PR_ASSERT(!data->sig_decoder_context);

  PR_ASSERT(signature_hdrs);
  if (!signature_hdrs) return -1;

  ct = MimeHeaders_get (signature_hdrs, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);

  /* Verify that the signature object is of the right type. */
  if (!ct || (nsCRT::strcasecmp(ct, APPLICATION_XPKCS7_SIGNATURE))) {
	  status = -1; /* #### error msg about bogus message */
  }
  PR_FREEIF(ct);
  if (status < 0) return status;

  data->sig_decoder_context = do_CreateInstance(NS_CMSDECODER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->sig_decoder_context->Start(nsnull, nsnull);
  if (NS_FAILED(rv)) {
	  status = PR_GetError();
	  PR_ASSERT(status < 0);
	  if (status >= 0) status = -1;
	}
  return status;
}


static int
MimeMultCMS_sig_hash (char *buf, PRInt32 size, void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  nsresult rv;

  PR_ASSERT(data && data->sig_decoder_context);
  if (!data || !data->sig_decoder_context) return -1;

  PR_ASSERT(!data->data_hash_context);

  rv = data->sig_decoder_context->Update(buf, size);
  if (NS_FAILED(rv)) {
	  if (!data->verify_error)
		data->verify_error = PR_GetError();
	  PR_ASSERT(data->verify_error < 0);
	  if (data->verify_error >= 0)
		data->verify_error = -1;
	}

  return 0;
}

static int
MimeMultCMS_sig_eof (void *crypto_closure, PRBool abort_p)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;

  if (!data) return -1;

  PR_ASSERT(!data->data_hash_context);

  /* Hand an EOF to the crypto library.

	 We save away the value returned and will use it later to emit a
	 blurb about whether the signature validation was cool.
   */

  PR_ASSERT(!data->content_info);

  if (data->sig_decoder_context)
	{
	  data->sig_decoder_context->Finish(getter_AddRefs(data->content_info));

    // Release our reference to nsICMSDecoder //
	  data->sig_decoder_context = 0;

	  if (!data->content_info && !data->verify_error)
		data->verify_error = PR_GetError();

	  PR_ASSERT(data->content_info ||
				data->verify_error || data->decode_error);
	}

  return 0;
}

static void
MimeMultCMS_free (void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  PR_ASSERT(data);
  if (!data) return;

  PR_FREEIF(data->sender_addr);

  if (data->data_hash_context)
	{
    // Release our reference to nsIHash //
	  data->data_hash_context = 0;
	}

  // Do a graceful shutdown of the nsICMSDecoder and release the nsICMSMessage //
  if (data->sig_decoder_context)
	{
	  nsCOMPtr<nsICMSMessage> cinfo;
    data->sig_decoder_context->Finish(getter_AddRefs(cinfo));
	}

  if (data->content_info)
	{
    // Release our reference to nsICMSMessage //
	  data->content_info = 0;
	}

  PR_FREEIF(data->item_data);
  PR_FREEIF(data);
}

static char *
MimeMultCMS_generate (void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  PRBool signed_p = PR_TRUE;
  PRBool good_p = PR_TRUE;
  PRBool encrypted_p;
  PRBool unverified_p = PR_FALSE;
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsIPrompt> prompter;
  wwatch->GetNewPrompter(0, getter_AddRefs(prompter));

  PR_ASSERT(data);
  if (!data) return 0;
  encrypted_p = data->parent_is_encrypted_p;

  if (data->content_info)
	{
	  rv = data->content_info->VerifyDetachedSignature(data->item_data, data->item_len);
	  if (NS_FAILED(rv)) {
      if (!data->verify_error)
        data->verify_error = PR_GetError();
      PR_ASSERT(data->verify_error < 0);
      if (data->verify_error >= 0)
        data->verify_error = -1;
    } else {
		  good_p = MimeCMSHeadersAndCertsMatch(data->self,
												 data->content_info,
												 &data->sender_addr);
      if (!good_p && !data->verify_error) {
        data->verify_error = -1;
        // XXX Fix this		data->verify_error = SEC_ERROR_CERT_ADDR_MISMATCH; XXX //
      }
    }

#if 0 // XXX Fix this. What do we do here? //
	  if (SEC_CMSContainsCertsOrCrls(data->content_info))
		{
		  /* #### call libsec telling it to import the certs */
		
#endif

	  /* Don't free these yet -- keep them around for the lifetime of the
		 MIME object, so that we can get at the security info of sub-parts
		 of the currently-displayed message. */
#if 0
	  SEC_CMSDestroyContentInfo(data->content_info);
	  data->content_info = 0;
#endif /* 0 */
	}
  else
	{
	  /* No content_info at all -- since we're inside a multipart/signed,
		 that means that we've either gotten a message that was truncated
		 before the signature part, or we ran out of memory, or something
		 awful has happened.  Anyway, it sure ain't good_p.
	   */
	  good_p = PR_FALSE;
	}

  // XXX Temporary hack until we write out to the chrome XXX //
  if (good_p) {
    nsString msg(NS_LITERAL_STRING("This is a signed message with a valid signature").get());
    prompter->Alert(0, msg.get());
  } else {
    nsString msg(NS_LITERAL_STRING("This is a signed message with an invalid signature").get());
    prompter->Alert(0, msg.get());
  }

  unverified_p = data->self->options->missing_parts; 

  PR_ASSERT(data->self);
  if (data->self && data->self->parent)
	mime_set_crypto_stamp(data->self->parent, signed_p, encrypted_p);


  {
	char *stamp_url = 0, *result;
	if (data->self)
	{
    if (unverified_p && data->self->options) {
			// XXX Fix this stamp_url = IMAP_CreateReloadAllPartsUrl(data->self->options->url); XXX //
    } else {
			stamp_url = MimeCMS_MakeSAURL(data->self);
    }
	}

	result =
	  MimeHeaders_make_crypto_stamp (encrypted_p, signed_p, good_p,
									 unverified_p,
									 data->parent_holds_stamp_p,
									 stamp_url);
	PR_FREEIF(stamp_url);
	return result;
  }
}
