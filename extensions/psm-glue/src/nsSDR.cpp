/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *  thayes@netscape.com
 */

#include "stdlib.h"
#include "plstr.h"
#include "nsIAllocator.h"
#include "nsIServiceManager.h"

#include "nsISecretDecoderRing.h"

#include "cmtcmn.h"
#include "nsIPSMComponent.h"

#include "nsSDR.h"

NS_IMPL_ISUPPORTS1(nsSecretDecoderRing, nsISecretDecoderRing)

nsSecretDecoderRing::nsSecretDecoderRing()
{
  NS_INIT_ISUPPORTS();

  mPSM = NULL;
}

nsSecretDecoderRing::~nsSecretDecoderRing()
{
  if (mPSM) mPSM->Release();
}

/* Init the new instance */
nsresult nsSecretDecoderRing::
init()
{
  nsresult rv;
  nsISupports *psm;

  rv = nsServiceManager::GetService(kPSMComponentProgID, NS_GET_IID(nsIPSMComponent),
                                    &psm);
  if (rv != NS_OK) goto loser;  /* Should promote error */

  mPSM = (nsIPSMComponent *)psm;

loser:
  return rv;
}

/* [noscript] long encrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Encrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
    nsresult rv = NS_OK;
    unsigned char *r = 0;
    CMT_CONTROL *control;
    CMTStatus status;
    CMUint32 cLen;

    if (data == nsnull || result == nsnull || _retval == nsnull) {
       rv = NS_ERROR_INVALID_POINTER;
       goto loser;
    }

    /* Check object initialization */
    NS_ASSERTION(mPSM != nsnull, "SDR object not initialized");
    if (mPSM == nsnull) { rv = NS_ERROR_NOT_INITIALIZED; goto loser; }

    /* Get the control connect to use for the request */
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
   
    status = CMT_SDREncrypt(control, (const unsigned char *)0, 0, 
               data, dataLen, result, &cLen);
    if (status != CMTSuccess) { rv = NS_ERROR_FAILURE; goto loser; } /* XXX */

    /* Copy returned data to nsAllocator buffer ? */
    *_retval = cLen;

loser:
    return rv;
}

/* [noscript] long decrypt (in buffer data, in long dataLen, out buffer result); */
NS_IMETHODIMP nsSecretDecoderRing::
Decrypt(unsigned char * data, PRInt32 dataLen, unsigned char * *result, PRInt32 *_retval)
{
    nsresult rv = NS_OK;
    CMTStatus status;
    CMT_CONTROL *control;
    CMUint32 len;

    if (data == nsnull || result == nsnull || _retval == nsnull) {
       rv = NS_ERROR_INVALID_POINTER;
       goto loser;
    }

    /* Check object initialization */
    NS_ASSERTION(mPSM != nsnull, "SDR object not initialized");
    if (mPSM == nsnull) { rv = NS_ERROR_NOT_INITIALIZED; goto loser; }

    /* Get the control connection */
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }
    
    /* Call PSM to decrypt the value */
    status = CMT_SDRDecrypt(control, data, dataLen, result, &len);
    if (status != CMTSuccess) { rv = NS_ERROR_FAILURE; goto loser; } /* Promote? */

    /* Copy returned data to nsAllocator buffer ? */
    *_retval = len;

loser:
    return rv;
}

/* string encryptString (in string text); */
NS_IMETHODIMP nsSecretDecoderRing::
EncryptString(const char *text, char **_retval)
{
    nsresult rv = NS_OK;
    unsigned char *encrypted = 0;
    PRInt32 eLen;

    if (text == nsnull || _retval == nsnull) {
        rv = NS_ERROR_INVALID_POINTER;
        goto loser;
    }

    rv = Encrypt((unsigned char *)text, PL_strlen(text), &encrypted, &eLen);
    if (rv != NS_OK) { goto loser; }

    rv = encode(encrypted, eLen, _retval);

loser:
    if (encrypted) nsAllocator::Free(encrypted);

    return rv;
}

/* string decryptString (in string crypt); */
NS_IMETHODIMP nsSecretDecoderRing::
DecryptString(const char *crypt, char **_retval)
{
    nsresult rv = NS_OK;
    char *r = 0;
    unsigned char *decoded = 0;
    PRInt32 decodedLen;
    unsigned char *decrypted = 0;
    PRInt32 decryptedLen;

    if (crypt == nsnull || _retval == nsnull) {
      rv = NS_ERROR_INVALID_POINTER;
      goto loser;
    }

    rv = decode(crypt, &decoded, &decodedLen);
    if (rv != NS_OK) goto loser;

    rv = Decrypt(decoded, decodedLen, &decrypted, &decryptedLen);
    if (rv != NS_OK) goto loser;

    // Convert to NUL-terminated string
    r = (char *)nsAllocator::Alloc(decryptedLen+1);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, decrypted, decryptedLen);
    r[decryptedLen] = 0;

    *_retval = r;
    r = 0;

loser:
    if (r) nsAllocator::Free(r);
    if (decrypted) nsAllocator::Free(decrypted);
    if (decoded) nsAllocator::Free(decoded);
 
    return rv;
}

nsresult nsSecretDecoderRing::
encode(const unsigned char *data, PRInt32 dataLen, char **_retval)
{
    nsresult rv = NS_OK;
    char *r = 0;

    // Allocate space for encoded string (with NUL)
    r = (char *)nsAllocator::Alloc(dataLen+1);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, data, dataLen);
    r[dataLen] = 0;

    *_retval = r;
    r = 0;

loser:
    if (r) nsAllocator::Free(r);

    return rv;
}

nsresult nsSecretDecoderRing::
decode(const char *data, unsigned char **result, PRInt32 * _retval)
{
    nsresult rv = NS_OK;
    unsigned char *r = 0;
    PRInt32 rLen;

    // Allocate space for decoded string (missing NUL)
    rLen = PL_strlen(data);
    r = (unsigned char *)nsAllocator::Alloc(rLen);
    if (!r) { rv = NS_ERROR_OUT_OF_MEMORY; goto loser; }

    memcpy(r, data, rLen);

    *result = r;
    r = 0;
    *_retval = rLen;

loser:
    if (r) nsAllocator::Free(r);

    return rv;
}

const char * nsSecretDecoderRing::kPSMComponentProgID = PSM_COMPONENT_PROGID;
