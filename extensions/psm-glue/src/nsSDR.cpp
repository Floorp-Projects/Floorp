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
/*
#include "nsIModule.h"
#include "nsIGenericFactory.h"
*/

#include "stdlib.h"
#include "plstr.h"
#include "nsIAllocator.h"
#include "nsIServiceManager.h"

#include "nsISecretDecoderRing.h"

#include "cmtcmn.h"
#include "nsIPSMComponent.h"

#include "nsSDR.h"

/* Test version */
static const char *kSuccess = "Success:";
static const char *kFailure = "Failure:";


#if 0
// ===============================================
// nsSecretDecoderRing - implementation of nsISecretDecoderRing
// ===============================================

#define NS_SDR_PROGID "netscape.security.sdr.1"

// {0D9A0341-0CE7-11d4-9FDD-000064657374}
#define NS_SDR_CID \
  { 0xd9a0341, 0xce7, 0x11d4, { 0x9f, 0xdd, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsSecretDecoderRing : public nsISecretDecoderRing
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING

  nsSecretDecoderRing();
  virtual ~nsSecretDecoderRing();

  nsresult init();

private:
  nsIPSMComponent *mPSM;

  static const char *kPrefix;
  static const char *kFailed;
  static const char *kPSMComponentProgID;

  nsresult encode(const unsigned char *data, PRInt32 dataLen, char **_retval);
  nsresult decode(const char *data, unsigned char **result, PRInt32 * _retval);
};

#endif /* 0 */

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
  CMT_CONTROL *control;

  rv = nsServiceManager::GetService(kPSMComponentProgID, NS_GET_IID(nsIPSMComponent),
                                    &psm);
  if (rv == NS_OK) mPSM = (nsIPSMComponent *)psm;

  rv = mPSM->GetControlConnection(&control);
  return NS_OK;
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

    rv = mPSM->GetControlConnection(&control);
    if (rv != CMTSuccess) { rv = NS_ERROR_NULL_POINTER; goto loser; } /* XXX */
   
    status = CMT_SDREncrypt(control, (const unsigned char *)0, 0, 
               data, dataLen, result, &cLen);
    if (status != CMTSuccess) { rv = NS_ERROR_NULL_POINTER; goto loser; } /* XXX */

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

    /* Get the control connection */
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) goto loser;
    
    /* Call PSM to decrypt the value */
    status = CMT_SDRDecrypt(control, data, dataLen, result, &len);
    if (status != CMTSuccess) { rv = NS_ERROR_NULL_POINTER; goto loser; }

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

    rv = Encrypt((unsigned char *)text, PL_strlen(text), &encrypted, &eLen);
    if (rv != NS_OK) { goto loser; }

    rv = encode(encrypted, eLen, _retval);

loser:
    if (encrypted) nsAllocator::Free(encrypted);

    return NS_OK;
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

const char * nsSecretDecoderRing::kPrefix = "PSMtest:";
const char * nsSecretDecoderRing::kFailed = "Failed:";
const char * nsSecretDecoderRing::kPSMComponentProgID = PSM_COMPONENT_PROGID;

#if 0

// *** MODULE *** ///

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the object nsSampleImpl
//
// What this does is defines a function nsSampleImplConstructor which we
// will specific in the nsModuleComponentInfo table. This function will
// be used by the generic factory to create an instance of nsSampleImpl.
//
// NOTE: This creates an instance of nsSampleImpl by using the default
//		 constructor nsSampleImpl::nsSampleImpl()
//
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSecretDecoderRing, init)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, progid, and
// class name.
//
// The Registration and Unregistration proc are optional in the structure.
//
static NS_METHOD nsSDRRegistrationProc(nsIComponentManager *aCompMgr,
                                          nsIFile *aPath,
                                          const char *registryLocation,
                                          const char *componentType)
{
    // Do any registration specific activity like adding yourself to a
    // category. The Generic Module will take care of registering your
    // component with xpcom. You dont need to do that. Only any component
    // specific additional activity needs to be done here.

    // This functions is optional. If you dont need it, dont add it to the structure.

    return NS_OK;
}

static NS_METHOD nsSDRUnregistrationProc(nsIComponentManager *aCompMgr,
                                            nsIFile *aPath,
                                            const char *registryLocation)
{
    // Undo any component specific registration like adding yourself to a
    // category here. The Generic Module will take care of unregistering your
    // component from xpcom. You dont need to do that. Only any component
    // specific additional activity needs to be done here.

    // This functions is optional. If you dont need it, dont add it to the structure.

    // Return value is not used from this function.
    return NS_OK;
}

static nsModuleComponentInfo components[] =
{
  { "SDR Component", NS_SDR_CID, NS_SDR_PROGID, nsSecretDecoderRingConstructor,
    nsSDRRegistrationProc /* NULL if you dont need one */,
    nsSDRUnregistrationProc /* NULL if you dont need one */
  }
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
// NOTE: If you want to use the module shutdown to release any
//		module specific resources, use the macro
//		NS_IMPL_NSGETMODULE_WITH_DTOR() instead of the vanilla
//		NS_IMPL_NSGETMODULE()
//
NS_IMPL_NSGETMODULE("nsSecretDecoderRing", components)

#endif /* 0 */
