/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   David Drinan <ddrinan@netscape.com>
 *   Stephane Saux <ssaux@netscape.com>
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

#include "nsMsgComposeSecure.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsIX509Cert.h"
#include "nsISMimeCert.h"
#include "nsIMimeConverter.h"
#include "nsMimeStringResources.h"
#include "nsMimeTypes.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgHeaderParser.h"
#include "nsFileStream.h"
#include "nsIServiceManager.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgCompFields.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsArray.h"

// String bundle for smime. Class static.
nsCOMPtr<nsIStringBundle> nsMsgComposeSecure::mSMIMEBundle = nsnull;

// XXX These strings should go in properties file XXX //
#define MIME_MULTIPART_SIGNED_BLURB "This is a cryptographically signed message in MIME format."
#define MIME_SMIME_ENCRYPTED_CONTENT_DESCRIPTION "S/MIME Encrypted Message"
#define MIME_SMIME_SIGNATURE_CONTENT_DESCRIPTION "S/MIME Cryptographic Signature"

#define MK_MIME_ERROR_WRITING_FILE -1

#define SMIME_STRBUNDLE_URL "chrome://messenger/locale/am-smime.properties"

static void mime_crypto_write_base64 (void *closure, const char *buf,
						  unsigned long size);
static nsresult PR_CALLBACK mime_encoder_output_fn(const char *buf, PRInt32 size, void *closure);
static nsresult PR_CALLBACK mime_nested_encoder_output_fn (const char *buf, PRInt32 size, void *closure);
static int make_multipart_signed_header_string(PRBool outer_p,
									char **header_return,
									char **boundary_return);
static char *mime_make_separator(const char *prefix);

// mscott --> FIX ME...for now cloning code from compose\nsMsgEncode.h/.cpp

#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsMsgMimeCID.h"
#include "nsIMimeConverter.h"

MimeEncoderData *
MIME_B64EncoderInit(nsresult (*PR_CALLBACK output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->B64EncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

MimeEncoderData *	
MIME_QPEncoderInit(nsresult (*PR_CALLBACK output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->QPEncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

MimeEncoderData *	
MIME_UUEncoderInit(char *filename, nsresult (*PR_CALLBACK output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->UUEncoderInit(filename, output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

nsresult
MIME_EncoderDestroy(MimeEncoderData *data, PRBool abort_p) 
{
  //MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->EncoderDestroy(data, abort_p);
    NS_RELEASE(converter);
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}

nsresult
MIME_EncoderWrite(MimeEncoderData *data, const char *buffer, PRInt32 size) 
{
  //  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  PRInt32 written = 0;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->EncoderWrite(data, buffer, size, &written);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? 0 : -1;
}

static void
GenerateGlobalRandomBytes(unsigned char *buf, PRInt32 len)
{
  static PRBool    firstTime = PR_TRUE;
  
  if (firstTime)
  {
    // Seed the random-number generator with current time so that
    // the numbers will be different every time we run.
    PRInt32 aTime;
    LL_L2I(aTime, PR_Now());
    srand( (unsigned)aTime );
    firstTime = PR_FALSE;
  }
  
  for( PRInt32 i = 0; i < len; i++ )
    buf[i] = rand() % 10;
}
   
char 
*mime_make_separator(const char *prefix)
{
	unsigned char rand_buf[13]; 
	GenerateGlobalRandomBytes(rand_buf, 12);

  return PR_smprintf("------------%s"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X",
					 prefix,
					 rand_buf[0], rand_buf[1], rand_buf[2], rand_buf[3],
					 rand_buf[4], rand_buf[5], rand_buf[6], rand_buf[7],
					 rand_buf[8], rand_buf[9], rand_buf[10], rand_buf[11]);
}

// end of copied code which needs fixed....

/////////////////////////////////////////////////////////////////////////////////////////
// Implementation of nsMsgSMIMEComposeFields
/////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsMsgSMIMEComposeFields, nsIMsgSMIMECompFields)

nsMsgSMIMEComposeFields::nsMsgSMIMEComposeFields()
:mSignMessage(PR_FALSE), mAlwaysEncryptMessage(PR_FALSE)
{
}

nsMsgSMIMEComposeFields::~nsMsgSMIMEComposeFields()
{
}

NS_IMETHODIMP nsMsgSMIMEComposeFields::SetSignMessage(PRBool value)
{
  mSignMessage = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgSMIMEComposeFields::GetSignMessage(PRBool *_retval)
{
  *_retval = mSignMessage;
  return NS_OK;
}

NS_IMETHODIMP nsMsgSMIMEComposeFields::SetRequireEncryptMessage(PRBool value)
{
  mAlwaysEncryptMessage = value;
  return NS_OK;
}

NS_IMETHODIMP nsMsgSMIMEComposeFields::GetRequireEncryptMessage(PRBool *_retval)
{
  *_retval = mAlwaysEncryptMessage;
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Implementation of nsMsgComposeSecure
/////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsMsgComposeSecure, nsIMsgComposeSecure)

nsMsgComposeSecure::nsMsgComposeSecure()
{
  /* member initializers and constructor code */
  mStream = 0;
  mDataHash = 0;
  mSigEncoderData = 0;
  mMultipartSignedBoundary  = 0;
  mSelfSigningCert = 0;
  mSelfEncryptionCert = 0;
  mCerts = 0;
  mEncryptionCinfo = 0;
  mEncryptionContext = 0;
  mCryptoEncoderData = 0;
  mBuffer = 0;
  mBufferedBytes = 0;
}

nsMsgComposeSecure::~nsMsgComposeSecure()
{
  /* destructor code */
  if (mEncryptionContext) {
    if (mBufferedBytes) {
      mEncryptionContext->Update(mBuffer, mBufferedBytes);
      mBufferedBytes = 0;
    }
    mEncryptionContext->Finish();
  }

  if (mSigEncoderData) {
	  MIME_EncoderDestroy (mSigEncoderData, PR_TRUE);
  }
  if (mCryptoEncoderData) {
	  MIME_EncoderDestroy (mCryptoEncoderData, PR_TRUE);
  }

  delete [] mBuffer;

  PR_FREEIF(mMultipartSignedBoundary);
}

NS_IMETHODIMP nsMsgComposeSecure::RequiresCryptoEncapsulation(nsIMsgIdentity * aIdentity, nsIMsgCompFields * aCompFields, PRBool * aRequiresEncryptionWork)
{
  NS_ENSURE_ARG_POINTER(aRequiresEncryptionWork);

  nsresult rv = NS_OK;
  *aRequiresEncryptionWork = PR_FALSE;

  PRBool alwaysEncryptMessages = PR_FALSE;
  PRBool signMessage = PR_FALSE;
  rv = ExtractEncryptionState(aIdentity, aCompFields, &signMessage, &alwaysEncryptMessages);

  if (alwaysEncryptMessages || signMessage)
    *aRequiresEncryptionWork = PR_TRUE;

  return NS_OK;
}


nsresult nsMsgComposeSecure::GetSMIMEBundleString(const PRUnichar *name,
                                                  PRUnichar **outString)
{
  nsresult rv = NS_ERROR_FAILURE;

  *outString = nsnull;

  if ( ! mSMIMEBundle ) {
    InitializeSMIMEBundle();
    if ( ! mSMIMEBundle ) {
      return rv;
    }
  }

  if (name) {
    rv = mSMIMEBundle->GetStringFromName(name, outString);
    if (NS_SUCCEEDED(rv)) {
      rv = NS_OK;
    }
  }

  return rv;
}


nsresult
nsMsgComposeSecure::
SMIMEBundleFormatStringFromName(const PRUnichar *name,
                                const PRUnichar **params,
                                PRUint32 numParams,
                                PRUnichar **outString)
{

  nsresult rv = NS_ERROR_FAILURE;

  if ( ! mSMIMEBundle ) {
    InitializeSMIMEBundle();
    if ( ! mSMIMEBundle ) {
      return rv;
    }
  }

  if (name) {
    rv = mSMIMEBundle->FormatStringFromName(name, params, 
                                             numParams, outString);
  }
  return rv;
}

void nsMsgComposeSecure::InitializeSMIMEBundle()
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));

  if ( NS_FAILED(rv) ) {
    return;
  }  

  bundleService->CreateBundle(SMIME_STRBUNDLE_URL,
                              getter_AddRefs(mSMIMEBundle));
}

void nsMsgComposeSecure::SetError(nsIMsgSendReport *sendReport, const PRUnichar *bundle_string)
{
  if (!sendReport || !bundle_string)
    return;

  if (mErrorAlreadyReported)
    return;

  mErrorAlreadyReported = PR_TRUE;
  
  nsXPIDLString errorString;
  nsresult res;

  res = GetSMIMEBundleString(bundle_string,
                             getter_Copies(errorString));

  if (NS_SUCCEEDED(res) && !errorString.IsEmpty())
  {
    sendReport->SetMessage(nsIMsgSendReport::process_Current,
                           errorString.get(),
                           PR_TRUE);
  }
}

void nsMsgComposeSecure::SetErrorWithParam(nsIMsgSendReport *sendReport, const PRUnichar *bundle_string, const char *param)
{
  if (!sendReport || !bundle_string || !param)
    return;

  if (mErrorAlreadyReported)
    return;

  mErrorAlreadyReported = PR_TRUE;
  
  nsXPIDLString errorString;
  nsresult res;
  const PRUnichar *params[1];

  NS_ConvertASCIItoUCS2 ucs2(param);
  params[0]= ucs2.get();

  res = SMIMEBundleFormatStringFromName(bundle_string,
                                        params,
                                        1,
                                        getter_Copies(errorString));

  if (NS_SUCCEEDED(res) && !errorString.IsEmpty())
  {
    sendReport->SetMessage(nsIMsgSendReport::process_Current,
                           errorString.get(),
                           PR_TRUE);
  }
}

nsresult nsMsgComposeSecure::ExtractEncryptionState(nsIMsgIdentity * aIdentity, nsIMsgCompFields * aComposeFields, PRBool * aSignMessage, PRBool * aEncrypt)
{
  if (!aComposeFields && !aIdentity)
    return NS_ERROR_FAILURE; // kick out...invalid args....

  NS_ENSURE_ARG(aSignMessage);
  NS_ENSURE_ARG(aEncrypt);

  nsCOMPtr<nsISupports> securityInfo;
  if (aComposeFields)
    aComposeFields->GetSecurityInfo(getter_AddRefs(securityInfo));

  if (securityInfo) // if we were given security comp fields, use them.....
  {
    nsCOMPtr<nsIMsgSMIMECompFields> smimeCompFields = do_QueryInterface(securityInfo);
    if (smimeCompFields)
    {
      smimeCompFields->GetSignMessage(aSignMessage);
      smimeCompFields->GetRequireEncryptMessage(aEncrypt);
      return NS_OK;
    }
  }

  // get the default info from the identity....
  PRInt32 ep = 0;
  nsresult testrv = aIdentity->GetIntAttribute("encryptionpolicy", &ep);
  if (NS_FAILED(testrv)) {
    *aEncrypt = PR_FALSE;
  }
  else {
    *aEncrypt = (ep > 0);
  }

  testrv = aIdentity->GetBoolAttribute("sign_mail", aSignMessage);
  if (NS_FAILED(testrv))
  {
    *aSignMessage = PR_FALSE;
  }
  return NS_OK;
}

/* void beginCryptoEncapsulation (in nsOutputFileStream aStream, in boolean aEncrypt, in boolean aSign, in string aRecipeints, in boolean aIsDraft); */
NS_IMETHODIMP nsMsgComposeSecure::BeginCryptoEncapsulation(nsOutputFileStream * aStream,
                                                           const char * aRecipients,
                                                           nsIMsgCompFields * aCompFields,
                                                           nsIMsgIdentity * aIdentity,
                                                           nsIMsgSendReport *sendReport,
                                                           PRBool aIsDraft)
{
  mErrorAlreadyReported = PR_FALSE;
  nsresult rv = NS_OK;

  PRBool encryptMessages = PR_FALSE;
  PRBool signMessage = PR_FALSE;
  ExtractEncryptionState(aIdentity, aCompFields, &signMessage, &encryptMessages);

  if (!signMessage && !encryptMessages) return NS_ERROR_FAILURE;

  mStream = aStream;
  mIsDraft = aIsDraft;

  if (encryptMessages && signMessage)
	  mCryptoState = mime_crypto_signed_encrypted;
  else if (encryptMessages)
	  mCryptoState = mime_crypto_encrypted;
  else if (signMessage)
	  mCryptoState = mime_crypto_clear_signed;
  else
	  PR_ASSERT(0);

  aIdentity->GetUnicharAttribute("signing_cert_name", getter_Copies(mSigningCertName));
  aIdentity->GetUnicharAttribute("encryption_cert_name", getter_Copies(mEncryptionCertName));

  rv = MimeCryptoHackCerts(aRecipients, sendReport, encryptMessages, signMessage);
  if (NS_FAILED(rv)) {
    goto FAIL;
  }

  switch (mCryptoState)
	{
	case mime_crypto_clear_signed:
    rv = MimeInitMultipartSigned(PR_TRUE, sendReport);
    break;
	case mime_crypto_opaque_signed:
    PR_ASSERT(0);    /* #### no api for this yet */
    rv = NS_ERROR_NOT_IMPLEMENTED;
	  break;
	case mime_crypto_signed_encrypted:
    rv = MimeInitEncryption(PR_TRUE, sendReport);
	  break;
	case mime_crypto_encrypted:
    rv = MimeInitEncryption(PR_FALSE, sendReport);
	  break;
	case mime_crypto_none:
	  /* This can happen if mime_crypto_hack_certs() decided to turn off
		 encryption (by asking the user.) */
    rv = 1;
    break;
  default:
    PR_ASSERT(0);
    break;
  }

FAIL:
  return rv;
}

/* void finishCryptoEncapsulation (in boolean aAbort); */
NS_IMETHODIMP nsMsgComposeSecure::FinishCryptoEncapsulation(PRBool aAbort, nsIMsgSendReport *sendReport)
{
  nsresult rv = NS_OK;

  if (!aAbort) {
	  switch (mCryptoState) {
		case mime_crypto_clear_signed:
      rv = MimeFinishMultipartSigned (PR_TRUE, sendReport);
      break;
    case mime_crypto_opaque_signed:
      PR_ASSERT(0);    /* #### no api for this yet */
      rv = NS_ERROR_FAILURE;
      break;
    case mime_crypto_signed_encrypted:
      rv = MimeFinishEncryption (PR_TRUE, sendReport);
		  break;
		case mime_crypto_encrypted:
		  rv = MimeFinishEncryption (PR_FALSE, sendReport);
		  break;
		default:
		  PR_ASSERT(0);
		  rv = NS_ERROR_FAILURE;
		  break;
		}
	}
  return rv;
}

nsresult nsMsgComposeSecure::MimeInitMultipartSigned(PRBool aOuter, nsIMsgSendReport *sendReport)
{
  /* First, construct and write out the multipart/signed MIME header data.
   */
  nsresult rv = NS_OK;
  char *header = 0;
  PRInt32 L;

  rv = make_multipart_signed_header_string(aOuter, &header,
										&mMultipartSignedBoundary);
  if (NS_FAILED(rv)) goto FAIL;

  L = strlen(header);

  if (aOuter){
	  /* If this is the outer block, write it to the file. */
    if (PRInt32(mStream->write(header, L)) < L) {
		  rv = MK_MIME_ERROR_WRITING_FILE;
    }
  } else {
	  /* If this is an inner block, feed it through the crypto stream. */
	  rv = MimeCryptoWriteBlock (header, L);
	}

  PR_Free(header);
  if (NS_FAILED(rv)) goto FAIL;

  /* Now initialize the crypto library, so that we can compute a hash
	 on the object which we are signing.
   */

  mHashType = nsIHash::HASH_AlgSHA1;

  PR_SetError(0,0);
  mDataHash = do_CreateInstance(NS_HASH_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = mDataHash->Create(mHashType);
  if (NS_FAILED(rv)) {
	  goto FAIL;
	}

  PR_SetError(0,0);
  rv = mDataHash->Begin();
 FAIL:
  return rv;
}

nsresult nsMsgComposeSecure::MimeInitEncryption(PRBool aSign, nsIMsgSendReport *sendReport)
{
  nsresult rv;

  /* First, construct and write out the opaque-crypto-blob MIME header data.
   */

  char *s =
	PR_smprintf("Content-Type: " APPLICATION_XPKCS7_MIME
				  "; name=\"smime.p7m\"" CRLF
				"Content-Transfer-Encoding: " ENCODING_BASE64 CRLF
				"Content-Disposition: attachment"
				  "; filename=\"smime.p7m\"" CRLF
				"Content-Description: %s" CRLF
				CRLF,
				MIME_SMIME_ENCRYPTED_CONTENT_DESCRIPTION);
  PRInt32 L;
  if (!s) return NS_ERROR_OUT_OF_MEMORY;
  L = strlen(s);
  if (PRInt32(mStream->write(s, L)) < L) {
	  return NS_ERROR_FAILURE;
  }
  PR_Free(s);
  s = 0;

  /* Now initialize the crypto library, so that we can filter the object
	 to be encrypted through it.
   */

  if (!mIsDraft) {
    PRUint32 numCerts;
    mCerts->GetLength(&numCerts);
    PR_ASSERT(numCerts > 0);
	  if (numCerts == 0) return NS_ERROR_FAILURE;
  }

  /* Initialize the base64 encoder. */
  PR_ASSERT(!mCryptoEncoderData);
  mCryptoEncoderData = MIME_B64EncoderInit(mime_encoder_output_fn,
												  this);
  if (!mCryptoEncoderData) {
	  return NS_ERROR_OUT_OF_MEMORY;
  }

  /* Initialize the encrypter (and add the sender's cert.) */
  PR_ASSERT(mSelfEncryptionCert);
  PR_SetError(0,0);
  mEncryptionCinfo = do_CreateInstance(NS_CMSMESSAGE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = mEncryptionCinfo->CreateEncrypted(mCerts);
  if (NS_FAILED(rv)) {
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotEncrypt").get());
	  goto FAIL;
	}

  mEncryptionContext = do_CreateInstance(NS_CMSENCODER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  if (!mBuffer) {
    mBuffer = new char[eBufferSize];
    if (!mBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mBufferedBytes = 0;

  rv = mEncryptionContext->Start(mEncryptionCinfo, mime_crypto_write_base64, mCryptoEncoderData);
  if (NS_FAILED(rv)) {
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotEncrypt").get());
	  goto FAIL;
	}

  /* If we're signing, tack a multipart/signed header onto the front of
	 the data to be encrypted, and initialize the sign-hashing code too.
   */
  if (aSign) {
	  rv = MimeInitMultipartSigned(PR_FALSE, sendReport);
	  if (NS_FAILED(rv)) goto FAIL;
	}

 FAIL:
  return rv;
}

nsresult nsMsgComposeSecure::MimeFinishMultipartSigned (PRBool aOuter, nsIMsgSendReport *sendReport)
{
  int status;
  nsresult rv;
  PRUint32 sec_item_len;
  unsigned char * sec_item_data;
  nsCOMPtr<nsICMSMessage> cinfo = do_CreateInstance(NS_CMSMESSAGE_CONTRACTID, &rv);
  nsCOMPtr<nsICMSEncoder> encoder = do_CreateInstance(NS_CMSENCODER_CONTRACTID, &rv);
  char * header = nsnull;

  /* Compute the hash...
   */
  mDataHash->ResultLen(mHashType, &sec_item_len);
  sec_item_data = (unsigned char *) PR_MALLOC(sec_item_len);
  if (!sec_item_data)	{
	  status = NS_ERROR_OUT_OF_MEMORY;
	  goto FAIL;
	}

  PR_SetError(0,0);
  mDataHash->End(sec_item_data, &sec_item_len, sec_item_len);
  status = PR_GetError();
  if (status < 0) {
    rv = NS_ERROR_FAILURE;
    goto FAIL;
  }

  PR_SetError(0,0);
  mDataHash = 0;

  status = PR_GetError();
  if (status < 0) goto FAIL;

  /* Write out the headers for the signature.
   */
	PRInt32 L;
	header =
	  PR_smprintf(CRLF
				  "--%s" CRLF
				  "Content-Type: " APPLICATION_XPKCS7_SIGNATURE
				    "; name=\"smime.p7s\"" CRLF
				  "Content-Transfer-Encoding: " ENCODING_BASE64 CRLF
				  "Content-Disposition: attachment; "
				    "filename=\"smime.p7s\"" CRLF
				  "Content-Description: %s" CRLF
				  CRLF,
				  mMultipartSignedBoundary,
				  MIME_SMIME_SIGNATURE_CONTENT_DESCRIPTION);
	if (!header) {
    rv = NS_ERROR_OUT_OF_MEMORY;
		goto FAIL;
	}

	L = strlen(header);
	if (aOuter) {
		/* If this is the outer block, write it to the file. */
    if (PRInt32(mStream->write(header, L)) < L) {
		  rv = MK_MIME_ERROR_WRITING_FILE;
    } 
  } else {
    /* If this is an inner block, feed it through the crypto stream. */
		rv = MimeCryptoWriteBlock (header, L);
  }

	PR_Free(header);

  /* Create the signature...
   */

  PR_ASSERT(mHashType == nsIHash::HASH_AlgSHA1);

  PR_ASSERT (mSelfSigningCert);
  PR_SetError(0,0);
  rv = cinfo->CreateSigned(mSelfSigningCert, mSelfEncryptionCert, sec_item_data, sec_item_len);
  if (NS_FAILED(rv))	{
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotSign").get());
	  goto FAIL;
	}

  /* Initialize the base64 encoder for the signature data.
   */
  PR_ASSERT(!mSigEncoderData);
  mSigEncoderData =
	MIME_B64EncoderInit((aOuter
						? mime_encoder_output_fn
						: mime_nested_encoder_output_fn),
					   this);
  if (!mSigEncoderData) {
	  rv = NS_ERROR_OUT_OF_MEMORY;
	  goto FAIL;
	}

  /* Write out the signature.
   */
  PR_SetError(0,0);
  rv = encoder->Start(cinfo, mime_crypto_write_base64, mSigEncoderData);
  if (NS_FAILED(rv)) {
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotSign").get());
	  goto FAIL;
	}

  // We're not passing in any data, so no update needed.
  rv = encoder->Finish();
  if (NS_FAILED(rv)) {
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotSign").get());
	  goto FAIL;
	}

  /* Shut down the sig's base64 encoder.
   */
  rv = MIME_EncoderDestroy(mSigEncoderData, PR_FALSE);
  mSigEncoderData = 0;
  if (NS_FAILED(rv)) {
    goto FAIL;
  }

  /* Now write out the terminating boundary.
   */
  {
	PRInt32 L;
	char *header = PR_smprintf(CRLF "--%s--" CRLF,
							   mMultipartSignedBoundary);
	PR_Free(mMultipartSignedBoundary);
	mMultipartSignedBoundary = 0;

	if (!header) {
		rv = NS_ERROR_OUT_OF_MEMORY;
		goto FAIL;
	}
	L = strlen(header);
	if (aOuter) {
		/* If this is the outer block, write it to the file. */
		if (PRInt32(mStream->write(header, L)) < L)
		  rv = MK_MIME_ERROR_WRITING_FILE;
  } else {
		/* If this is an inner block, feed it through the crypto stream. */
		rv = MimeCryptoWriteBlock (header, L);
	}
  }

FAIL:
  PR_FREEIF(sec_item_data);
  return rv;
}


/* Helper function for mime_finish_crypto_encapsulation() to close off
   an opaque crypto object (for encrypted or signed-and-encrypted messages.)
 */
nsresult nsMsgComposeSecure::MimeFinishEncryption (PRBool aSign, nsIMsgSendReport *sendReport)
{
  nsresult rv;

  /* If this object is both encrypted and signed, close off the
	 signature first (since it's inside.) */
  if (aSign) {
	  rv = MimeFinishMultipartSigned (PR_FALSE, sendReport);
    if (NS_FAILED(rv)) {
      goto FAIL;
    }
	}

  /* Close off the opaque encrypted blob.
   */
  PR_ASSERT(mEncryptionContext);

  if (mBufferedBytes) {
    rv = mEncryptionContext->Update(mBuffer, mBufferedBytes);
    mBufferedBytes = 0;
    if (NS_FAILED(rv)) {
      PR_ASSERT(PR_GetError() < 0);
      goto FAIL;
    }
  }
  
  rv = mEncryptionContext->Finish();
  if (NS_FAILED(rv)) {
    SetError(sendReport, NS_LITERAL_STRING("ErrorCanNotEncrypt").get());
    goto FAIL;
	}

  mEncryptionContext = 0;

  PR_ASSERT(mEncryptionCinfo);
  if (!mEncryptionCinfo) {
    rv = NS_ERROR_FAILURE;
  }
  if (mEncryptionCinfo) {
    mEncryptionCinfo = 0;
  }

  /* Shut down the base64 encoder. */
  rv = MIME_EncoderDestroy(mCryptoEncoderData, PR_FALSE);
  mCryptoEncoderData = 0;

  if (PRInt32(mStream->write(CRLF, 2)) < 2)
    rv = NS_ERROR_FAILURE;

 FAIL:
  return rv;
}

/* Used to figure out what certs should be used when encrypting this message.
 */
nsresult nsMsgComposeSecure::MimeCryptoHackCerts(const char *aRecipients,
                                                 nsIMsgSendReport *sendReport,
                                                 PRBool aEncrypt,
                                                 PRBool aSign)
{
  char *all_mailboxes = 0, *mailboxes = 0, *mailbox_list = 0;
  const char *mailbox = 0;
  PRUint32 count = 0;
  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
  nsresult res;
  nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);
  NS_ENSURE_SUCCESS(res,res);

  res = NS_NewArray(getter_AddRefs(mCerts));
  if (NS_FAILED(res)) {
    return res;
  }

  PR_ASSERT(aEncrypt || aSign);
  certdb->FindEmailEncryptionCert(mEncryptionCertName, getter_AddRefs(mSelfEncryptionCert));
  certdb->FindEmailSigningCert(mSigningCertName, getter_AddRefs(mSelfSigningCert));

  // must have both the signing and encryption certs to sign
	if ((mSelfSigningCert == nsnull) && aSign) {
    SetError(sendReport, NS_LITERAL_STRING("NoSenderSigningCert").get());
    res = NS_ERROR_FAILURE;
		goto FAIL;
	}

	if ((mSelfEncryptionCert == nsnull) && aEncrypt) {
    SetError(sendReport, NS_LITERAL_STRING("NoSenderEncryptionCert").get());
    res = NS_ERROR_FAILURE;
    goto FAIL;
  }

  pHeader->ExtractHeaderAddressMailboxes(nsnull,aRecipients, &all_mailboxes);
  pHeader->RemoveDuplicateAddresses(nsnull, all_mailboxes, 0, PR_FALSE /*removeAliasesToMe*/, &mailboxes);
  if (all_mailboxes) {
    nsMemory::Free(all_mailboxes);
    all_mailboxes = nsnull;
  }

  if (mailboxes) {
	  pHeader->ParseHeaderAddresses (nsnull, mailboxes, 0, &mailbox_list, &count);
    nsMemory::Free(mailboxes);
    mailboxes = nsnull;
  }
  if (count < 0) return count;

  if (aEncrypt && mSelfEncryptionCert) {
    // Make sure self's configured cert is prepared for being used
    // as an email recipient cert.
    
    nsCOMPtr<nsISMimeCert> sc = do_QueryInterface(mSelfEncryptionCert);
    if (sc) {
      sc->SaveSMimeProfile();
    }
  }

  /* If the message is to be encrypted, then get the recipient certs */
  if (aEncrypt) {
	  mailbox = mailbox_list;

    PRBool already_added_self_cert = PR_FALSE;

	  for (; count > 0; count--) {
	    nsCString mailbox_lowercase;
	    ToLowerCase(nsDependentCString(mailbox), mailbox_lowercase);
      nsCOMPtr<nsIX509Cert> cert;
      certdb->FindCertByEmailAddress(nsnull, mailbox_lowercase.get(), getter_AddRefs(cert));
      PRBool foundValidCert = PR_FALSE;

		  if (cert) {
        PRUint32 verification_result;

        if (NS_SUCCEEDED(
            cert->VerifyForUsage(nsIX509Cert::CERT_USAGE_EmailRecipient, &verification_result))
            &&
            nsIX509Cert::VERIFIED_OK == verification_result)
        {
          foundValidCert = PR_TRUE;
        }
      }
      
      if (!foundValidCert) {
        // Failure to find a valid encryption cert is fatal.
        // here I assume that mailbox contains ascii rather than utf8.
        SetErrorWithParam(sendReport, NS_LITERAL_STRING("MissingRecipientEncryptionCert").get(), mailbox);
        res = NS_ERROR_FAILURE;
        goto FAIL;
		  }

	  /* #### see if recipient requests `signedData'.
		 if (...) no_clearsigning_p = PR_TRUE;
		 (This is the only reason we even bother looking up the certs
		 of the recipients if we're sending a signed-but-not-encrypted
		 message.)
	   */

      PRBool isSame;
      if (NS_SUCCEEDED(cert->Equals(mSelfEncryptionCert, &isSame))
          && isSame) {
        already_added_self_cert = PR_TRUE;
      }

      mCerts->AppendElement(cert, PR_FALSE);
      // To understand this loop, especially the "+= strlen +1", look at the documentation
      // of ParseHeaderAddresses. Basically, it returns a list of zero terminated strings.
		  mailbox += strlen(mailbox) + 1;
	  }
    
    if (!already_added_self_cert) {
      mCerts->AppendElement(mSelfEncryptionCert, PR_FALSE);
    }
	}
FAIL:
  if (mailbox_list) {
    nsMemory::Free(mailbox_list);
  }
  return res;
}

NS_IMETHODIMP nsMsgComposeSecure::MimeCryptoWriteBlock (const char *buf, PRInt32 size)
{
  int status = 0;
  nsresult rv;

  /* If this is a From line, mangle it before signing it.  You just know
	 that something somewhere is going to mangle it later, and that's
	 going to cause the signature check to fail.

	 (This assumes that, in the cases where From-mangling must happen,
	 this function is called a line at a time.  That happens to be the
	 case.)
  */
  if (size >= 5 && buf[0] == 'F' && !nsCRT::strncmp(buf, "From ", 5))	{
	  char mangle[] = ">";
	  status = MimeCryptoWriteBlock (mangle, 1);
	  if (status < 0)
		return status;
	}

  /* If we're signing, or signing-and-encrypting, feed this data into
	 the computation of the hash. */
  if (mDataHash) {
	  PR_SetError(0,0);
	  mDataHash->Update((unsigned char *) buf, size);
	  status = PR_GetError();
	  if (status < 0) goto FAIL;
	}

  PR_SetError(0,0);
  if (mEncryptionContext) {
	  /* If we're encrypting, or signing-and-encrypting, write this data
		 by filtering it through the crypto library. */

    /* We want to create equally sized encryption strings */
    const char *inputBytesIterator = buf;
    PRUint32 inputBytesLeft = size;

    while (inputBytesLeft) {
      const PRUint32 spaceLeftInBuffer = eBufferSize - mBufferedBytes;
      const PRUint32 bytesToAppend = NS_MIN(inputBytesLeft, spaceLeftInBuffer);

      memcpy(mBuffer+mBufferedBytes, inputBytesIterator, bytesToAppend);
      mBufferedBytes += bytesToAppend;
      
      inputBytesIterator += bytesToAppend;
      inputBytesLeft -= bytesToAppend;

      if (eBufferSize == mBufferedBytes) {
        rv = mEncryptionContext->Update(mBuffer, mBufferedBytes);
        mBufferedBytes = 0;
        if (NS_FAILED(rv)) {
          status = PR_GetError();
          PR_ASSERT(status < 0);
          if (status >= 0) status = -1;
          goto FAIL;
        }
      }
    }
  } else {
	  /* If we're not encrypting (presumably just signing) then write this
		 data directly to the file. */

    if (PRInt32(mStream->write (buf, size)) < size) {
  		return MK_MIME_ERROR_WRITING_FILE;
    }
	}
 FAIL:
  return status;
}

/* Returns a string consisting of a Content-Type header, and a boundary
   string, suitable for moving from the header block, down into the body
   of a multipart object.  The boundary itself is also returned (so that
   the caller knows what to write to close it off.)
 */
static int
make_multipart_signed_header_string(PRBool outer_p,
									char **header_return,
									char **boundary_return)
{
  *header_return = 0;
  *boundary_return = mime_make_separator("ms");
  const char * crypto_multipart_blurb = nsnull;

  if (!*boundary_return)
	return NS_ERROR_OUT_OF_MEMORY;

  if (outer_p) {
	  crypto_multipart_blurb = MIME_MULTIPART_SIGNED_BLURB;
  }

  *header_return =
	PR_smprintf("Content-Type: " MULTIPART_SIGNED "; "
				"protocol=\"" APPLICATION_XPKCS7_SIGNATURE "\"; "
				"micalg=" PARAM_MICALG_SHA1 "; "
				"boundary=\"%s\"" CRLF
				CRLF
				"%s%s"
				"--%s" CRLF,

				*boundary_return,
				(crypto_multipart_blurb ? crypto_multipart_blurb : ""),
				(crypto_multipart_blurb ? CRLF CRLF : ""),
				*boundary_return);

  if (!*header_return) {
	  PR_Free(*boundary_return);
	  *boundary_return = 0;
	  return NS_ERROR_OUT_OF_MEMORY;
	}

  return 0;
}

/* Used as the output function of a SEC_PKCS7EncoderContext -- we feed
   plaintext into the crypto engine, and it calls this function with encrypted
   data; then this function writes a base64-encoded representation of that
   data to the file (by filtering it through the given MimeEncoderData object.)

   Also used as the output function of SEC_PKCS7Encode() -- but in that case,
   it's used to write the encoded representation of the signature.  The only
   difference is which MimeEncoderData object is used.
 */
static void
mime_crypto_write_base64 (void *closure, const char *buf,
						  unsigned long size)
{
  MimeEncoderData *data = (MimeEncoderData *) closure;
  int status = MIME_EncoderWrite (data, buf, size);
  PR_SetError(status < 0 ? status : 0, 0);
}


/* Used as the output function of MimeEncoderData -- when we have generated
   the signature for a multipart/signed object, this is used to write the
   base64-encoded representation of the signature to the file.
 */
nsresult mime_encoder_output_fn(const char *buf, PRInt32 size, void *closure)
{
  nsMsgComposeSecure *state = (nsMsgComposeSecure *) closure;
  nsOutputFileStream *stream = state->GetOutputStream();
  if (PRInt32(stream->write((char *) buf, size)) < size)
    return MK_MIME_ERROR_WRITING_FILE;
  else
    return 0;
}

/* Like mime_encoder_output_fn, except this is used for the case where we
   are both signing and encrypting -- the base64-encoded output of the
   signature should be fed into the crypto engine, rather than being written
   directly to the file.
 */
static nsresult
mime_nested_encoder_output_fn (const char *buf, PRInt32 size, void *closure)
{
  nsMsgComposeSecure *state = (nsMsgComposeSecure *) closure;
  return state->MimeCryptoWriteBlock ((char *) buf, size);
}
