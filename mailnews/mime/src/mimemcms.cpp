/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsICMSMessage.h"
#include "nsICMSMessageErrors.h"
#include "nsICMSDecoder.h"
#include "nsIHash.h"
#include "mimemcms.h"
#include "mimecryp.h"
#include "nsMimeTypes.h"
#include "nspr.h"
#include "nsMimeStringResources.h"
#include "mimemsg.h"
#include "mimemoz2.h"
#include "nsIURI.h"
#include "nsIMsgWindow.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgSMIMEHeaderSink.h"
#include "nsCOMPtr.h"
#include "nsIX509Cert.h"

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


typedef struct MimeMultCMSdata
{
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
  nsCOMPtr<nsIMsgSMIMEHeaderSink> smimeHeaderSink;
  
  MimeMultCMSdata()
  :hash_type(0),
  sender_addr(nsnull),
  decode_error(0),
  verify_error(0),
  item_data(nsnull),
  self(nsnull),
  parent_is_encrypted_p(PR_FALSE),
  parent_holds_stamp_p(PR_FALSE)
  {
  }
  
  ~MimeMultCMSdata()
  {
    PR_FREEIF(sender_addr);

    // Do a graceful shutdown of the nsICMSDecoder and release the nsICMSMessage //
    if (sig_decoder_context)
    {
	    nsCOMPtr<nsICMSMessage> cinfo;
      sig_decoder_context->Finish(getter_AddRefs(cinfo));
    }

    delete [] item_data;
  }
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
											 PRBool *signing_cert_without_email_address,
											 char **);
extern char *MimeCMS_MakeSAURL(MimeObject *obj);
extern char *IMAP_CreateReloadAllPartsUrl(const char *url);
extern int MIMEGetRelativeCryptoNestLevel(MimeObject *obj);

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

  if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_MD5) ||
      !nsCRT::strcasecmp(micalg, PARAM_MICALG_MD5_2))
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

  data = new MimeMultCMSdata;
  if (!data)
    return 0;

  data->self = obj;
  data->hash_type = hash_type;

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
		  delete data;
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

  mime_stream_data *msd = (mime_stream_data *) (data->self->options->stream_closure);
  if (msd)
  {
    nsIChannel *channel = msd->channel;  // note the lack of ref counting...
    if (channel)
    {
      nsCOMPtr<nsIURI> uri;
      nsCOMPtr<nsIMsgWindow> msgWindow;
      nsCOMPtr<nsIMsgHeaderSink> headerSink;
      nsCOMPtr<nsIMsgMailNewsUrl> msgurl;
      nsCOMPtr<nsISupports> securityInfo;
      channel->GetURI(getter_AddRefs(uri));
      if (uri)
      {
        nsCAutoString urlSpec;
        rv = uri->GetSpec(urlSpec);

        // We only want to update the UI if the current mime transaction
        // is intended for display.
        // If the current transaction is intended for background processing,
        // we can learn that by looking at the additional header=filter
        // string contained in the URI.
        //
        // If we find something, we do not set smimeHeaderSink,
        // which will prevent us from giving UI feedback.
        //
        // If we do not find header=filter, we assume the result of the
        // processing will be shown in the UI.
        
        if (!strstr(urlSpec.get(), "?header=filter") &&
            !strstr(urlSpec.get(), "&header=filter"))
        {
          msgurl = do_QueryInterface(uri);
          if (msgurl)
            msgurl->GetMsgWindow(getter_AddRefs(msgWindow));
          if (msgWindow)
            msgWindow->GetMsgHeaderSink(getter_AddRefs(headerSink));
          if (headerSink)
            headerSink->GetSecurityInfo(getter_AddRefs(securityInfo));
          if (securityInfo)
            data->smimeHeaderSink = do_QueryInterface(securityInfo);
         }
       }
    } // if channel
  } // if msd

  return data;
}

static int
MimeMultCMS_data_hash (char *buf, PRInt32 size, void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  if (!data || !data->data_hash_context) {
    return -1;
  }

  PR_SetError(0, 0);
  data->data_hash_context->Update((unsigned char *) buf, size);
  if (!data->verify_error) {
  	data->verify_error = PR_GetError();
  }

  return 0;
}

static int
MimeMultCMS_data_eof (void *crypto_closure, PRBool abort_p)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  if (!data || !data->data_hash_context) {
    return -1;
  }

  data->data_hash_context->ResultLen(data->hash_type, &data->item_len);
  data->item_data = new unsigned char[data->item_len];
  if (!data->item_data) return MIME_OUT_OF_MEMORY;

  PR_SetError(0, 0);
  data->data_hash_context->End(data->item_data, &data->item_len, data->item_len);
  if (!data->verify_error) {
	  data->verify_error = PR_GetError();
  }

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
  char *ct;
  int status = 0;
  nsresult rv;

  if (!signature_hdrs) {
    return -1;
  }

  ct = MimeHeaders_get (signature_hdrs, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);

  /* Verify that the signature object is of the right type. */
  if (!ct || /* is not a signature type */
             (nsCRT::strcasecmp(ct, APPLICATION_XPKCS7_SIGNATURE) != 0
              && nsCRT::strcasecmp(ct, APPLICATION_PKCS7_SIGNATURE) != 0)) {
	  status = -1; /* #### error msg about bogus message */
  }
  PR_FREEIF(ct);
  if (status < 0) return status;

  data->sig_decoder_context = do_CreateInstance(NS_CMSDECODER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->sig_decoder_context->Start(nsnull, nsnull);
  if (NS_FAILED(rv)) {
	  status = PR_GetError();
	  if (status >= 0) status = -1;
	}
  return status;
}


static int
MimeMultCMS_sig_hash (char *buf, PRInt32 size, void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  nsresult rv;

  if (!data || !data->sig_decoder_context) {
    return -1;
  }

  rv = data->sig_decoder_context->Update(buf, size);
  if (NS_FAILED(rv)) {
	  if (!data->verify_error)
		data->verify_error = PR_GetError();
	  if (data->verify_error >= 0)
		data->verify_error = -1;
	}

  return 0;
}

static int
MimeMultCMS_sig_eof (void *crypto_closure, PRBool abort_p)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;

  if (!data) {
    return -1;
  }

  /* Hand an EOF to the crypto library.

	 We save away the value returned and will use it later to emit a
	 blurb about whether the signature validation was cool.
   */

  if (data->sig_decoder_context) {
	  data->sig_decoder_context->Finish(getter_AddRefs(data->content_info));

    // Release our reference to nsICMSDecoder //
	  data->sig_decoder_context = 0;

    if (!data->content_info && !data->verify_error) {
      data->verify_error = PR_GetError();
    }
  }

  return 0;
}

static void
MimeMultCMS_free (void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  if (!data) return;

  delete data;
}

static char *
MimeMultCMS_generate (void *crypto_closure)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  PRBool signed_p = PR_TRUE;
  PRBool good_p = PR_FALSE;
  PRBool encrypted_p;
  PRBool unverified_p = PR_FALSE;
  nsresult rv;
  if (!data) return 0;
  encrypted_p = data->parent_is_encrypted_p;
  PRInt32 signature_status = nsICMSMessageErrors::GENERAL_ERROR;
  nsCOMPtr<nsIX509Cert> signerCert;

  int aRelativeNestLevel = MIMEGetRelativeCryptoNestLevel(data->self);

  unverified_p = data->self->options->missing_parts; 

  if (unverified_p)
  {
    // We were not given all parts of the message.
    // We are therefore unable to verify correctness of the signature.
    signature_status = nsICMSMessageErrors::VERIFY_NOT_YET_ATTEMPTED;
  }
  else
  if (data->content_info)
	{
	  rv = data->content_info->VerifyDetachedSignature(data->item_data, data->item_len);
    data->content_info->GetSignerCert(getter_AddRefs(signerCert));

	  if (NS_FAILED(rv)) {
      if (NS_ERROR_MODULE_SECURITY == NS_ERROR_GET_MODULE(rv)) {
        signature_status = NS_ERROR_GET_CODE(rv);
      }
      
      if (!data->verify_error) {
        data->verify_error = PR_GetError();
      }
      if (data->verify_error >= 0) {
        data->verify_error = -1;
      }
    } else {
		  PRBool signing_cert_without_email_address;

		  good_p = MimeCMSHeadersAndCertsMatch(data->self,
												 data->content_info,
												 &signing_cert_without_email_address,
												 &data->sender_addr);
      if (!good_p) {
        if (signing_cert_without_email_address) {
          signature_status = nsICMSMessageErrors::VERIFY_CERT_WITHOUT_ADDRESS;
        }
        else {
          signature_status = nsICMSMessageErrors::VERIFY_HEADER_MISMATCH;
        }
        if (!data->verify_error) {
          data->verify_error = -1;
          // XXX Fix this		data->verify_error = SEC_ERROR_CERT_ADDR_MISMATCH; XXX //
        }
      }
      else 
      {
        signature_status = nsICMSMessageErrors::SUCCESS;
      }
    }

#if 0 // XXX Fix this. What do we do here? //
	  if (SEC_CMSContainsCertsOrCrls(data->content_info))
    {
		  /* #### call libsec telling it to import the certs */
    }
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
	}

  PRInt32 maxNestLevel = 0;
  if (data->smimeHeaderSink) {
    if (aRelativeNestLevel >= 0) {
      data->smimeHeaderSink->MaxWantedNesting(&maxNestLevel);

      if (aRelativeNestLevel <= maxNestLevel)
      {
        data->smimeHeaderSink->SignedStatus(aRelativeNestLevel, signature_status, signerCert);
      }
    }
  }

  if (data->self && data->self->parent) {
    mime_set_crypto_stamp(data->self->parent, signed_p, encrypted_p);
  }

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
