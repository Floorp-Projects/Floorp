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
 *   Kai Engert <kengert@redhat.com>
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
#include "nsICryptoHash.h"
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
  nsCOMPtr<nsICryptoHash> data_hash_context;
  nsCOMPtr<nsICMSDecoder> sig_decoder_context;
  nsCOMPtr<nsICMSMessage> content_info;
  char *sender_addr;
  PRBool decoding_failed;
  unsigned char* item_data;
  PRUint32 item_len;
  MimeObject *self;
  PRBool parent_is_encrypted_p;
  PRBool parent_holds_stamp_p;
  nsCOMPtr<nsIMsgSMIMEHeaderSink> smimeHeaderSink;
  
  MimeMultCMSdata()
  :hash_type(0),
  sender_addr(nsnull),
  decoding_failed(PR_FALSE),
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

/* #### MimeEncryptedCMS and MimeMultipartSignedCMS have a sleazy,
        incestuous, dysfunctional relationship. */
extern PRBool MimeEncryptedCMS_encrypted_p (MimeObject *obj);
extern void MimeCMSGetFromSender(MimeObject *obj,
                                 nsXPIDLCString &from_addr,
                                 nsXPIDLCString &from_name,
                                 nsXPIDLCString &sender_addr,
                                 nsXPIDLCString &sender_name);
extern PRBool MimeCMSHeadersAndCertsMatch(MimeObject *obj,
                                          nsICMSMessage *,
                                          PRBool *signing_cert_without_email_address);
extern void MimeCMSRequestAsyncSignatureVerification(nsICMSMessage *aCMSMsg,
                                                     const char *aFromAddr, const char *aFromName,
                                                     const char *aSenderAddr, const char *aSenderName,
                                                     nsIMsgSMIMEHeaderSink *aHeaderSink, PRInt32 aMimeNestingLevel,
                                                     unsigned char* item_data, PRUint32 item_len);
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
    hash_type = nsICryptoHash::MD5;
  else if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1) ||
       !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_2) ||
       !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_3) ||
       !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_4) ||
       !nsCRT::strcasecmp(micalg, PARAM_MICALG_SHA1_5))
    hash_type = nsICryptoHash::SHA1;
  else if (!nsCRT::strcasecmp(micalg, PARAM_MICALG_MD2))
    hash_type = nsICryptoHash::MD2;
  else
    hash_type = -1;

  PR_Free(micalg);
  micalg = 0;

  if (hash_type == -1) return 0; /* #### bogus message? */

  data = new MimeMultCMSdata;
  if (!data)
    return 0;

  data->self = obj;
  data->hash_type = hash_type;

  data->data_hash_context = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->data_hash_context->Init(data->hash_type);
  if (NS_FAILED(rv)) return 0;

  PR_SetError(0,0);

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
            !strstr(urlSpec.get(), "&header=filter")&&
            !strstr(urlSpec.get(), "?header=attach") &&
            !strstr(urlSpec.get(), "&header=attach"))
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
  nsresult rv = data->data_hash_context->Update((unsigned char *) buf, size);
  data->decoding_failed = NS_FAILED(rv);

  return 0;
}

static int
MimeMultCMS_data_eof (void *crypto_closure, PRBool abort_p)
{
  MimeMultCMSdata *data = (MimeMultCMSdata *) crypto_closure;
  if (!data || !data->data_hash_context) {
    return -1;
  }

  nsCAutoString hashString;
  data->data_hash_context->Finish(PR_FALSE, hashString);
  PR_SetError(0, 0);
  
  data->item_len  = hashString.Length();
  data->item_data = new unsigned char[data->item_len];
  if (!data->item_data) return MIME_OUT_OF_MEMORY;
  
  memcpy(data->item_data, hashString.get(), data->item_len);

  // Release our reference to nsICryptoHash //
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
  data->decoding_failed = NS_FAILED(rv);

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
  PRBool encrypted_p;
  if (!data) return 0;
  encrypted_p = data->parent_is_encrypted_p;
  nsCOMPtr<nsIX509Cert> signerCert;

  int aRelativeNestLevel = MIMEGetRelativeCryptoNestLevel(data->self);

  if (aRelativeNestLevel < 0)
    return nsnull;

  PRInt32 maxNestLevel = 0;
  if (data->smimeHeaderSink && aRelativeNestLevel >= 0)
  {
    data->smimeHeaderSink->MaxWantedNesting(&maxNestLevel);

    if (aRelativeNestLevel > maxNestLevel)
      return nsnull;
  }

  if (data->self->options->missing_parts)
  {
    // We were not given all parts of the message.
    // We are therefore unable to verify correctness of the signature.
    
    if (data->smimeHeaderSink)
      data->smimeHeaderSink->SignedStatus(aRelativeNestLevel, 
                                          nsICMSMessageErrors::VERIFY_NOT_YET_ATTEMPTED, 
                                          nsnull);
    return nsnull;
  }

  if (!data->content_info)
  {
    /* No content_info at all -- since we're inside a multipart/signed,
     that means that we've either gotten a message that was truncated
     before the signature part, or we ran out of memory, or something
     awful has happened.
     */
     return nsnull;
  }
  
  nsXPIDLCString from_addr;
  nsXPIDLCString from_name;
  nsXPIDLCString sender_addr;
  nsXPIDLCString sender_name;
  
  MimeCMSGetFromSender(data->self, 
                       from_addr, from_name,
                       sender_addr, sender_name);

  MimeCMSRequestAsyncSignatureVerification(data->content_info, 
                                           from_addr, from_name,
                                           sender_addr, sender_name,
                                           data->smimeHeaderSink, aRelativeNestLevel, 
                                           data->item_data, data->item_len);

  if (data->content_info)
  {
#if 0 // XXX Fix this. What do we do here? //
    if (SEC_CMSContainsCertsOrCrls(data->content_info))
    {
      /* #### call libsec telling it to import the certs */
    }
#endif
  }

  return nsnull;
}
