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
#include "nsICMSMessage2.h"
#include "nsICMSMessageErrors.h"
#include "nsICMSDecoder.h"
#include "mimecms.h"
#include "mimemsig.h"
#include "nsCRT.h"
#include "nspr.h"
#include "nsEscape.h"
#include "mimemsg.h"
#include "mimemoz2.h"
#include "nsIURI.h"
#include "nsIMsgWindow.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgSMIMEHeaderSink.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIX509Cert.h"
#include "nsIMsgHeaderParser.h"
#include "nsIProxyObjectManager.h"


#define MIME_SUPERCLASS mimeEncryptedClass
MimeDefClass(MimeEncryptedCMS, MimeEncryptedCMSClass,
       mimeEncryptedCMSClass, &MIME_SUPERCLASS);

static void *MimeCMS_init(MimeObject *, int (*output_fn) (const char *, PRInt32, void *), void *);
static int MimeCMS_write (const char *, PRInt32, void *);
static int MimeCMS_eof (void *, PRBool);
static char * MimeCMS_generate (void *);
static void MimeCMS_free (void *);

extern int SEC_ERROR_CERT_ADDR_MISMATCH;

static int MimeEncryptedCMSClassInitialize(MimeEncryptedCMSClass *clazz)
{
#ifdef DEBUG
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  NS_ASSERTION(!oclass->class_initialized, "1.2 <mscott@netscape.com> 01 Nov 2001 17:59");
#endif

  MimeEncryptedClass *eclass = (MimeEncryptedClass *) clazz;
  eclass->crypto_init          = MimeCMS_init;
  eclass->crypto_write         = MimeCMS_write;
  eclass->crypto_eof           = MimeCMS_eof;
  eclass->crypto_generate_html = MimeCMS_generate;
  eclass->crypto_free          = MimeCMS_free;

  return 0;
}


typedef struct MimeCMSdata
{
  int (*output_fn) (const char *buf, PRInt32 buf_size, void *output_closure);
  void *output_closure;
  nsCOMPtr<nsICMSDecoder> decoder_context;
  nsCOMPtr<nsICMSMessage> content_info;
  PRBool ci_is_encrypted;
  char *sender_addr;
  PRBool decoding_failed;
  MimeObject *self;
  PRBool parent_is_encrypted_p;
  PRBool parent_holds_stamp_p;
  nsCOMPtr<nsIMsgSMIMEHeaderSink> smimeHeaderSink;
  
  MimeCMSdata()
  :output_fn(nsnull),
  output_closure(nsnull),
  ci_is_encrypted(PR_FALSE),
  sender_addr(nsnull),
  decoding_failed(PR_FALSE),
  self(nsnull),
  parent_is_encrypted_p(PR_FALSE),
  parent_holds_stamp_p(PR_FALSE)
  {
  }
  
  ~MimeCMSdata()
  {
    if(sender_addr)
      PR_Free(sender_addr);

    // Do an orderly release of nsICMSDecoder and nsICMSMessage //
    if (decoder_context)
    {
      nsCOMPtr<nsICMSMessage> cinfo;
      decoder_context->Finish(getter_AddRefs(cinfo));
    }
  }
} MimeCMSdata;

/*   SEC_PKCS7DecoderContentCallback for SEC_PKCS7DecoderStart() */
static void MimeCMS_content_callback (void *arg, const char *buf, unsigned long length)
{
  int status;
  MimeCMSdata *data = (MimeCMSdata *) arg;
  if (!data) return;

  if (!data->output_fn)
    return;

  PR_SetError(0,0);
  status = data->output_fn (buf, length, data->output_closure);
  if (status < 0)
  {
    PR_SetError(status, 0);
    data->output_fn = 0;
    return;
  }
}

PRBool MimeEncryptedCMS_encrypted_p (MimeObject *obj)
{
  PRBool encrypted;

  if (!obj) return PR_FALSE;
  if (mime_typep(obj, (MimeObjectClass *) &mimeEncryptedCMSClass))
  {
    MimeEncrypted *enc = (MimeEncrypted *) obj;
    MimeCMSdata *data = (MimeCMSdata *) enc->crypto_closure;
    if (!data || !data->content_info) return PR_FALSE;
                data->content_info->ContentIsEncrypted(&encrypted);
          return encrypted;
  }
  return PR_FALSE;
}

// extern MimeMessageClass mimeMessageClass;      /* gag */

static void ParseRFC822Addresses (const char *line, nsXPIDLCString &names, nsXPIDLCString &addresses)
{
  PRUint32 numAddresses;
  nsresult res;
  nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);

  if (NS_SUCCEEDED(res))
  {
    pHeader->ParseHeaderAddresses(nsnull, line, getter_Copies(names), getter_Copies(addresses), &numAddresses);
  }
}

PRBool MimeCMSHeadersAndCertsMatch(nsICMSMessage *content_info, 
                                   nsIX509Cert *signerCert,
                                   const char *from_addr,
                                   const char *from_name,
                                   const char *sender_addr,
                                   const char *sender_name,
                                   PRBool *signing_cert_without_email_address)
{
  nsXPIDLCString cert_addr;
  PRBool match = PR_TRUE;
  PRBool foundFrom = PR_FALSE;
  PRBool foundSender = PR_FALSE;

  /* Find the name and address in the cert.
   */
  if (content_info)
  {
    // Extract any address contained in the cert.
    // This will be used for testing, whether the cert contains no addresses at all.
    content_info->GetSignerEmailAddress (getter_Copies(cert_addr));
  }

  if (signing_cert_without_email_address)
  {
    *signing_cert_without_email_address = (!cert_addr);
  }

  /* Now compare them --
   consider it a match if the address in the cert matches either the
   address in the From or Sender field
   */

  /* If there is no addr in the cert at all, it can not match and we fail. */
  if (!cert_addr)
  {
    match = PR_FALSE;
  }
  else
  {
    if (signerCert)
    {
      if (from_addr && *from_addr)
      {
        NS_ConvertASCIItoUTF16 ucs2From(from_addr);
        if (NS_FAILED(signerCert->ContainsEmailAddress(ucs2From, &foundFrom)))
        {
          foundFrom = PR_FALSE;
        }
      }

      if (sender_addr && *sender_addr)
      {
        NS_ConvertASCIItoUTF16 ucs2Sender(sender_addr);
        if (NS_FAILED(signerCert->ContainsEmailAddress(ucs2Sender, &foundSender)))
        {
          foundSender = PR_FALSE;
        }
      }
    }

    if (!foundSender && !foundFrom)
    {
      match = PR_FALSE;
    }
  }

  return match;
}

class nsSMimeVerificationListener : public nsISMimeVerificationListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMIMEVERIFICATIONLISTENER

  nsSMimeVerificationListener(const char *aFromAddr, const char *aFromName,
                              const char *aSenderAddr, const char *aSenderName,
                              nsIMsgSMIMEHeaderSink *aHeaderSink, PRInt32 aMimeNestingLevel);

  virtual ~nsSMimeVerificationListener() {}
  
protected:
  nsCOMPtr<nsIMsgSMIMEHeaderSink> mHeaderSink;
  PRInt32 mMimeNestingLevel;

  nsXPIDLCString mFromAddr;
  nsXPIDLCString mFromName;
  nsXPIDLCString mSenderAddr;
  nsXPIDLCString mSenderName;
};

NS_IMPL_ISUPPORTS1(nsSMimeVerificationListener, nsISMimeVerificationListener)

nsSMimeVerificationListener::nsSMimeVerificationListener(const char *aFromAddr, const char *aFromName,
                                                         const char *aSenderAddr, const char *aSenderName,
                                                         nsIMsgSMIMEHeaderSink *aHeaderSink, PRInt32 aMimeNestingLevel)
{
  mHeaderSink = aHeaderSink;
  mMimeNestingLevel = aMimeNestingLevel;

  mFromAddr = aFromAddr;
  mFromName = aFromName;
  mSenderAddr = aSenderAddr;
  mSenderName = aSenderName;
}

NS_IMETHODIMP nsSMimeVerificationListener::Notify(nsICMSMessage2 *aVerifiedMessage,
                                                  nsresult aVerificationResultCode)
{
  // Only continue if we have a valid pointer to the UI
  NS_ENSURE_TRUE(mHeaderSink, NS_OK);
  
  NS_ENSURE_TRUE(aVerifiedMessage, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsICMSMessage> msg = do_QueryInterface(aVerifiedMessage);
  NS_ENSURE_TRUE(msg, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIX509Cert> signerCert;
  msg->GetSignerCert(getter_AddRefs(signerCert));
  
  PRInt32 signature_status = nsICMSMessageErrors::GENERAL_ERROR;
  
  if (NS_FAILED(aVerificationResultCode))
  {
    if (NS_ERROR_MODULE_SECURITY == NS_ERROR_GET_MODULE(aVerificationResultCode))
      signature_status = NS_ERROR_GET_CODE(aVerificationResultCode);
    else if (NS_ERROR_NOT_IMPLEMENTED == aVerificationResultCode)
      signature_status = nsICMSMessageErrors::VERIFY_ERROR_PROCESSING;
  }
  else
  {
    PRBool signing_cert_without_email_address;

    PRBool good_p = MimeCMSHeadersAndCertsMatch(msg, signerCert,
                                                mFromAddr.get(), mFromName.get(),
                                                mSenderAddr.get(), mSenderName.get(),
                                                &signing_cert_without_email_address);
    if (!good_p)
    {
      if (signing_cert_without_email_address)
        signature_status = nsICMSMessageErrors::VERIFY_CERT_WITHOUT_ADDRESS;
      else
        signature_status = nsICMSMessageErrors::VERIFY_HEADER_MISMATCH;
    }
    else 
      signature_status = nsICMSMessageErrors::SUCCESS;
  }

  nsCOMPtr<nsIMsgSMIMEHeaderSink> proxySink;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, NS_GET_IID(nsIMsgSMIMEHeaderSink),
                       mHeaderSink, NS_PROXY_SYNC, getter_AddRefs(proxySink));
  if (proxySink)
    proxySink->SignedStatus(mMimeNestingLevel, signature_status, signerCert);

  return NS_OK;
}

int MIMEGetRelativeCryptoNestLevel(MimeObject *obj)
{
  /*
    the part id of any mimeobj is mime_part_address(obj)
    our currently displayed crypto part is obj
    the part shown as the toplevel object in the current window is
        obj->options->part_to_load
        possibly stored in the toplevel object only ???
        but hopefully all nested mimeobject point to the same displayooptions

    we need to find out the nesting level of our currently displayed crypto object
    wrt the shown part in the toplevel window
  */

  // if we are showing the toplevel message, aTopMessageNestLevel == 0
  int aTopMessageNestLevel = 0;
  MimeObject *aTopShownObject = nsnull;
  if (obj && obj->options->part_to_load) {
    PRBool aAlreadyFoundTop = PR_FALSE;
    for (MimeObject *walker = obj; walker; walker = walker->parent) {
      if (aAlreadyFoundTop) {
        if (!mime_typep(walker, (MimeObjectClass *) &mimeEncryptedClass)
            && !mime_typep(walker, (MimeObjectClass *) &mimeMultipartSignedClass)) {
          ++aTopMessageNestLevel;
        }
      }
      if (!aAlreadyFoundTop && !strcmp(mime_part_address(walker), walker->options->part_to_load)) {
        aAlreadyFoundTop = PR_TRUE;
        aTopShownObject = walker;
      }
      if (!aAlreadyFoundTop && !walker->parent) {
        // The mime part part_to_load is not a parent of the
        // the crypto mime part passed in to this function as parameter obj.
        // That means the crypto part belongs to another branch of the mime tree.
        return -1;
      }
    }
  }

  PRBool CryptoObjectIsChildOfTopShownObject = PR_FALSE;
  if (!aTopShownObject) {
    // no sub part specified, top message is displayed, and
    // our crypto object is definitively a child of it
    CryptoObjectIsChildOfTopShownObject = PR_TRUE;
  }

  // if we are the child of the topmost message, aCryptoPartNestLevel == 1
  int aCryptoPartNestLevel = 0;
  if (obj) {
    for (MimeObject *walker = obj; walker; walker = walker->parent) {
      // Crypto mime objects are transparent wrt nesting.
      if (!mime_typep(walker, (MimeObjectClass *) &mimeEncryptedClass)
          && !mime_typep(walker, (MimeObjectClass *) &mimeMultipartSignedClass)) {
        ++aCryptoPartNestLevel;
      }
      if (aTopShownObject && walker->parent == aTopShownObject) {
        CryptoObjectIsChildOfTopShownObject = PR_TRUE;
      }
    }
  }

  if (!CryptoObjectIsChildOfTopShownObject) {
    return -1;
  }

  return aCryptoPartNestLevel - aTopMessageNestLevel;
}

static void *MimeCMS_init(MimeObject *obj,
                          int (*output_fn) (const char *buf, PRInt32 buf_size, void *output_closure), 
                          void *output_closure)
{
  MimeCMSdata *data;
  MimeDisplayOptions *opts;
  nsresult rv;

  if (!(obj && obj->options && output_fn)) return 0;

  opts = obj->options;
  data = new MimeCMSdata;
  if (!data) return 0;

  data->self = obj;
  data->output_fn = output_fn;
  data->output_closure = output_closure;
  PR_SetError(0, 0);
  data->decoder_context = do_CreateInstance(NS_CMSDECODER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->decoder_context->Start(MimeCMS_content_callback, data);
  if (NS_FAILED(rv)) return 0;

  // XXX Fix later XXX //
  data->parent_holds_stamp_p =
  (obj->parent &&
   (mime_crypto_stamped_p(obj->parent) ||
    mime_typep(obj->parent, (MimeObjectClass *) &mimeEncryptedClass)));

  data->parent_is_encrypted_p =
  (obj->parent && MimeEncryptedCMS_encrypted_p (obj->parent));

  /* If the parent of this object is a crypto-blob, then it's the grandparent
   who would have written out the headers and prepared for a stamp...
   (This shit sucks.)
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
            !strstr(urlSpec.get(), "&header=filter") &&
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
MimeCMS_write (const char *buf, PRInt32 buf_size, void *closure)
{
  MimeCMSdata *data = (MimeCMSdata *) closure;
  nsresult rv;

  if (!data || !data->output_fn || !data->decoder_context) return -1;

  PR_SetError(0, 0);
  rv = data->decoder_context->Update(buf, buf_size);
  data->decoding_failed = NS_FAILED(rv);

  return 0;
}

void MimeCMSGetFromSender(MimeObject *obj,
                          nsXPIDLCString &from_addr,
                          nsXPIDLCString &from_name,
                          nsXPIDLCString &sender_addr,
                          nsXPIDLCString &sender_name)
{
  MimeHeaders *msg_headers = 0;

  /* Find the headers of the MimeMessage which is the parent (or grandparent)
   of this object (remember, crypto objects nest.) */
  MimeObject *o2 = obj;
  msg_headers = o2->headers;
  while (o2 &&
       o2->parent &&
       !mime_typep(o2->parent, (MimeObjectClass *) &mimeMessageClass))
    {
    o2 = o2->parent;
    msg_headers = o2->headers;
    }

  if (!msg_headers)
    return;

  /* Find the names and addresses in the From and/or Sender fields.
   */
  char *s;

  /* Extract the name and address of the "From:" field. */
  s = MimeHeaders_get(msg_headers, HEADER_FROM, PR_FALSE, PR_FALSE);
  if (s)
    {
    ParseRFC822Addresses(s, from_name, from_addr);
    PR_FREEIF(s);
    }

  /* Extract the name and address of the "Sender:" field. */
  s = MimeHeaders_get(msg_headers, HEADER_SENDER, PR_FALSE, PR_FALSE);
  if (s)
    {
    ParseRFC822Addresses(s, sender_name, sender_addr);
    PR_FREEIF(s);
    }
}

void MimeCMSRequestAsyncSignatureVerification(nsICMSMessage *aCMSMsg,
                                              const char *aFromAddr, const char *aFromName,
                                              const char *aSenderAddr, const char *aSenderName,
                                              nsIMsgSMIMEHeaderSink *aHeaderSink, PRInt32 aMimeNestingLevel,
                                              unsigned char* item_data, PRUint32 item_len)
{
  nsCOMPtr<nsICMSMessage2> msg2 = do_QueryInterface(aCMSMsg);
  if (!msg2)
    return;
  
  nsRefPtr<nsSMimeVerificationListener> listener = 
    new nsSMimeVerificationListener(aFromAddr, aFromName, aSenderAddr, aSenderName,
                                    aHeaderSink, aMimeNestingLevel);
  if (!listener)
    return;
  
  if (item_data)
    msg2->AsyncVerifyDetachedSignature(listener, item_data, item_len);
  else
    msg2->AsyncVerifySignature(listener);
}

static int
MimeCMS_eof (void *crypto_closure, PRBool abort_p)
{
  MimeCMSdata *data = (MimeCMSdata *) crypto_closure;
  nsresult rv;
  PRInt32 status = nsICMSMessageErrors::SUCCESS;

  if (!data || !data->output_fn || !data->decoder_context) {
    return -1;
  }

  int aRelativeNestLevel = MIMEGetRelativeCryptoNestLevel(data->self);

  /* Hand an EOF to the crypto library.  It may call data->output_fn.
   (Today, the crypto library has no flushing to do, but maybe there
   will be someday.)

   We save away the value returned and will use it later to emit a
   blurb about whether the signature validation was cool.
   */

  PR_SetError(0, 0);
  rv = data->decoder_context->Finish(getter_AddRefs(data->content_info));
  if (NS_FAILED(rv))
    status = nsICMSMessageErrors::GENERAL_ERROR;

  data->decoder_context = 0;

  nsCOMPtr<nsIX509Cert> certOfInterest;

  if (!data->smimeHeaderSink)
    return 0;

  if (aRelativeNestLevel < 0)
    return 0;

  PRInt32 maxNestLevel = 0;
  data->smimeHeaderSink->MaxWantedNesting(&maxNestLevel);

  if (aRelativeNestLevel > maxNestLevel)
    return 0;

  if (data->decoding_failed)
    status = nsICMSMessageErrors::GENERAL_ERROR;

  if (!data->content_info)
  {
    status = nsICMSMessageErrors::GENERAL_ERROR;

    // Although a CMS message could be either encrypted or opaquely signed,
    // what we see is most likely encrypted, because if it were
    // signed only, we probably would have been able to decode it.

    data->ci_is_encrypted = PR_TRUE;
  }
  else
  {
    rv = data->content_info->ContentIsEncrypted(&data->ci_is_encrypted);

    if (NS_SUCCEEDED(rv) && data->ci_is_encrypted) {
      data->content_info->GetEncryptionCert(getter_AddRefs(certOfInterest));
    }
    else {
      // Existing logic in mimei assumes, if !ci_is_encrypted, then it is signed.
      // Make sure it indeed is signed.

      PRBool testIsSigned;
      rv = data->content_info->ContentIsSigned(&testIsSigned);

      if (NS_FAILED(rv) || !testIsSigned) {
        // Neither signed nor encrypted?
        // We are unable to understand what we got, do not try to indicate S/Mime status.
        return 0;
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
                                               nsnull, 0);
    }
  }

  if (data->ci_is_encrypted)
  {
    data->smimeHeaderSink->EncryptionStatus(
      aRelativeNestLevel,
      status,
      certOfInterest
    );
  }

  return 0;
}

static void
MimeCMS_free (void *crypto_closure)
{
  MimeCMSdata *data = (MimeCMSdata *) crypto_closure;
  if (!data) return;
  
  delete data;
}

static char *
MimeCMS_generate (void *crypto_closure)
{
  return nsnull;
}

