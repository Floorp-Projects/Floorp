/* -*- Mode: idl; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
*/
#ifndef _nsMsgComposeSecure_H_
#define _nsMsgComposeSecure_H_

#include "nsIMsgComposeSecure.h"
#include "nsIMsgSMIMECompFields.h"
#include "nsCOMPtr.h"
#include "nsICMSEncoder.h"
#include "nsIX509Cert.h"
#include "nsIMimeConverter.h"
#include "nsIStringBundle.h"
#include "nsIHash.h"
#include "nsICMSMessage.h"
#include "nsIArray.h"

class nsIMsgCompFields;

class nsMsgSMIMEComposeFields : public nsIMsgSMIMECompFields
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSMIMECOMPFIELDS

  nsMsgSMIMEComposeFields();
  virtual ~nsMsgSMIMEComposeFields();

private:
  PRBool mSignMessage;
  PRBool mAlwaysEncryptMessage;
};

typedef enum {
  mime_crypto_none,				/* normal unencapsulated MIME message */
  mime_crypto_clear_signed,		/* multipart/signed encapsulation */
  mime_crypto_opaque_signed,	/* application/x-pkcs7-mime (signedData) */
  mime_crypto_encrypted,		/* application/x-pkcs7-mime */
  mime_crypto_signed_encrypted	/* application/x-pkcs7-mime */
} mimeDeliveryCryptoState;

class nsMsgComposeSecure : public nsIMsgComposeSecure
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGCOMPOSESECURE

  nsMsgComposeSecure();
  virtual ~nsMsgComposeSecure();
  /* additional members */
  nsOutputFileStream *GetOutputStream() { return mStream;}
private:
  nsresult MimeInitMultipartSigned(PRBool aOuter, nsIMsgSendReport *sendReport);
  nsresult MimeInitEncryption(PRBool aSign, nsIMsgSendReport *sendReport);
  nsresult MimeFinishMultipartSigned (PRBool aOuter, nsIMsgSendReport *sendReport);
  nsresult MimeFinishEncryption (PRBool aSign, nsIMsgSendReport *sendReport);
  nsresult MimeCryptoHackCerts(const char *aRecipients, nsIMsgSendReport *sendReport, PRBool aEncrypt, PRBool aSign);
  static void InitializeSMIMEBundle();
  nsresult GetSMIMEBundleString(const PRUnichar *name,
				PRUnichar **outString);
  nsresult SMIMEBundleFormatStringFromName(const PRUnichar *name,
					   const PRUnichar **params,
					   PRUint32 numParams,
					   PRUnichar **outString);
  nsresult ExtractEncryptionState(nsIMsgIdentity * aIdentity, nsIMsgCompFields * aComposeFields, PRBool * aSignMessage, PRBool * aEncrypt);

  mimeDeliveryCryptoState mCryptoState;
  nsOutputFileStream *mStream;
  PRInt16 mHashType;
  nsCOMPtr<nsIHash> mDataHash;
  MimeEncoderData *mSigEncoderData;
  char *mMultipartSignedBoundary;
  nsXPIDLString mSigningCertName;
  nsCOMPtr<nsIX509Cert> mSelfSigningCert;
  nsXPIDLString mEncryptionCertName;
  nsCOMPtr<nsIX509Cert> mSelfEncryptionCert;
  nsCOMPtr<nsIMutableArray> mCerts;
  nsCOMPtr<nsICMSMessage> mEncryptionCinfo;
  nsCOMPtr<nsICMSEncoder> mEncryptionContext;
  static nsCOMPtr<nsIStringBundle> mSMIMEBundle;

  MimeEncoderData *mCryptoEncoderData;
  PRBool mIsDraft;

  PRBool mErrorAlreadyReported;
  void SetError(nsIMsgSendReport *sendReport, const PRUnichar *bundle_string);
  void SetErrorWithParam(nsIMsgSendReport *sendReport, const PRUnichar *bundle_string, const char *param);
};

#endif
