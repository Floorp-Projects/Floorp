/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsCrypto.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsCRT.h"
#include "prprf.h"
#include "prmem.h"
#include "nsPSMComponent.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIThreadManager.h"
#include "nsJSUtils.h"
#include "nsJSPrincipals.h"
#include "nsIPrincipal.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "jscntxt.h"
#include "cmtcmn.h"
#include "cmtjs.h"
#include <ctype.h>
#include "nsReadableUtils.h"

/*
 * These are the most common error strings that are returned
 * by the JavaScript methods in case of error.
 */

#define JS_ERROR             "error:"
#define JS_ERROR_INVAL_PARAM JS_ERROR"invalidParameter:"
#define JS_ERROR_USER_CANCEL JS_ERROR"userCancel"
#define JS_ERROR_INTERNAL    JS_ERROR"internalError"
#define JS_ERROR_ARGC_ERR    JS_ERROR"incorrect number of parameters"

#undef REPORT_INCORRECT_NUM_ARGS

#define JS_OK_ADD_MOD                      3
#define JS_OK_DEL_EXTERNAL_MOD             2
#define JS_OK_DEL_INTERNAL_MOD             1

#define JS_ERR_INTERNAL                   -1
#define JS_ERR_USER_CANCEL_ACTION         -2
#define JS_ERR_INCORRECT_NUM_OF_ARGUMENTS -3
#define JS_ERR_DEL_MOD                    -4
#define JS_ERR_ADD_MOD                    -5
#define JS_ERR_BAD_MODULE_NAME            -6
#define JS_ERR_BAD_DLL_NAME               -7
#define JS_ERR_BAD_MECHANISM_FLAGS        -8
#define JS_ERR_BAD_CIPHER_ENABLE_FLAGS    -9
#define JS_ERR_ADD_DUPLICATE_MOD         -10

/*
 * This structure is used to store information for one key generation.
 * The nsCrypto::GenerateCRMFRequest method parses the inputs and then
 * stores one of these structures for every key generation that happens.
 * The information stored in this structure is then used to set some
 * values in the CRMF request.
 */

typedef struct CRYPTO_KeyPairInfoStr {
    CMUint32      keyId;      /* The Key Id returned by PSM */
    SSMKeyGenType keyGenType; /* What type of key gen are we doing.*/
} CRYPTO_KeyPairInfo;

struct RunnableEvent : PLEvent {
        RunnableEvent(nsIRunnable* runnable);
        ~RunnableEvent();

        nsIRunnable* mRunnable;
};



/*
 * The structure passed back between the plugin event handler
 * and the handler function which waits on termination.
 */
typedef struct CRYPTO_KeyGenContextHandlerStr {
  CMUint32              numRequests;
  CMUint32              keyGenContext;
  void               *context; //This is for UI info.
  CRYPTO_KeyPairInfo *keyids;
  CMTItem             reqDN, regToken, authenticator, eaCert;
  nsCRMFObject       *crmfObject;
  JSContext          *cx;
  PCMT_CONTROL        control;
  nsCrypto           *cryptoObject;
  JSObject           *scope;
  char               *jsCallback;
  const char         *fileName;
  PRUint32            lineNo;
  nsIPrincipal       *principals;
  CMTStatus           result;
} CRYPTO_KeyGenContextHandler;

//
// This is the class we'll use to run the keygen done code
// as an nsIRunnable object;
//
class nsCryptoRunnable : public nsIRunnable {
public:
  nsCryptoRunnable(CRYPTO_KeyGenContextHandler *handlerInfo);
  virtual ~nsCryptoRunnable();

  NS_IMETHOD Run ();
  NS_DECL_ISUPPORTS
private:
  CRYPTO_KeyGenContextHandler *mHandlerInfo;
};

const char * nsCrypto::kPSMComponentProgID = PSM_COMPONENT_PROGID;
static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

NS_IMPL_ISUPPORTS2(nsCrypto, nsIDOMCrypto,nsIScriptObjectOwner)
NS_IMPL_ISUPPORTS2(nsCRMFObject, nsIDOMCRMFObject,nsIScriptObjectOwner)
NS_IMPL_ISUPPORTS2(nsCryptoRunnable, nsIRunnable,nsISupports);
NS_IMPL_ISUPPORTS2(nsPkcs11, nsIDOMPkcs11, nsIScriptObjectOwner);

nsIEventQueue* getUIEventQueue();

nsCrypto::nsCrypto()
{
  NS_INIT_REFCNT();
  mPSM = nsnull;
  mVersionStringSet = PR_FALSE;
  mScriptObject = nsnull;
}

nsCrypto::~nsCrypto()
{
  NS_IF_RELEASE(mPSM);
}

nsresult
getPSMComponent(nsIPSMComponent ** retPSM)
{
  nsresult rv;
  nsISupports *psm;
  
  rv = nsPSMComponent::CreatePSMComponent(nsnull, NS_GET_IID(nsIPSMComponent), (void**)&psm);
  if (rv == NS_OK) 
    *retPSM = (nsIPSMComponent *)psm;
  
  return rv;
}

nsresult
nsCrypto::init()
{
  return getPSMComponent(&mPSM);
}

nsIDOMScriptObjectFactory* nsCrypto::gScriptObjectFactory=nsnull;

static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

nsresult 
nsCrypto::GetScriptObjectFactory(nsIDOMScriptObjectFactory **aResult)
{
  nsresult result = NS_OK;

  if (nsnull == gScriptObjectFactory) {
    result = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          kIDOMScriptObjectFactoryIID,
                                          (nsISupports **)&gScriptObjectFactory);
    if (result != NS_OK) {
      return result;
    }
  }

  *aResult = gScriptObjectFactory;
  NS_ADDREF(gScriptObjectFactory);
  return result;
}

NS_IMETHODIMP
nsCrypto::GetScriptObject(nsIScriptContext *aContext, 
                          void** aScriptObject)
{
  nsresult rv = NS_OK;

  if (mScriptObject == nsnull) {
    nsIDOMScriptObjectFactory *factory=nsnull;
    
    rv = GetScriptObjectFactory(&factory);
    if (rv == NS_OK) {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();
      rv = factory->NewScriptCrypto(aContext, 
                                    (nsISupports *)(nsIDOMCrypto *)this, 
                                    (nsISupports *)global, 
                                    (void**)&mScriptObject);
      NS_IF_RELEASE(factory);
    }
  }
  *aScriptObject = mScriptObject;
  return rv;
}

NS_IMETHODIMP
nsCrypto::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

/*
 * This function converts a string read through JavaScript parameters
 * and translates it to the internal enumeration representing the
 * key gen type.
 */
static SSMKeyGenType
cryptojs_interpret_key_gen_type(char *keyAlg)
{
    char *end;
    if (keyAlg == NULL) {
        return invalidKeyGen;
    }
    /* First let's remove all leading and trailing white space */
    while (isspace(keyAlg[0])) keyAlg++;
    end = strchr(keyAlg, '\0');
    if (end == NULL) {
        return invalidKeyGen;
    }
    end--;
    while (isspace(*end)) end--;
    end[1] = '\0';
    if (strcmp(keyAlg, "rsa-ex") == 0) {
        return rsaEnc;
    } else if (strcmp(keyAlg, "rsa-dual-use") == 0) {
        return rsaDualUse;
    } else if (strcmp(keyAlg, "rsa-sign") == 0) {
        return rsaSign;
    } else if (strcmp(keyAlg, "rsa-sign-nonrepudiation") == 0) {
        return rsaSignNonrepudiation;
    } else if (strcmp(keyAlg, "rsa-nonrepudiation") == 0) {
        return rsaNonrepudiation;
    } else if (strcmp(keyAlg, "dsa-sign-nonrepudiation") == 0) {
        return dsaSignNonrepudiation;
    } else if (strcmp(keyAlg, "dsa-sign") ==0 ){
        return dsaSign;
    } else if (strcmp(keyAlg, "dsa-nonrepudiation") == 0) {
        return dsaNonrepudiation;
    } else if (strcmp(keyAlg, "dh-ex") == 0) {
        return dhEx;
    }
    return invalidKeyGen;
}


NS_IMETHODIMP
nsCrypto::GetVersion(nsAWritableString& aVersion)
{
  if (!mVersionStringSet) {
    PCMT_CONTROL control = NULL;
    nsresult rv;
    char *version, *protocolVersion;
    
    rv = mPSM->GetControlConnection(&control);
    if (rv != NS_OK) {
      return rv;
    }
    protocolVersion = SSM_GetVersionString();
    version = PR_smprintf("%s,%s", protocolVersion,
			  CMT_GetServerStringVersion(control));
    
    mVersionString.AssignWithConversion(version);
    PR_smprintf_free(version);
    nsCRT::free(protocolVersion);
    mVersionStringSet = PR_TRUE;
  }
  
  aVersion.Assign(mVersionString);
  return NS_OK;
}
//These defines are taken from the PKCS#11 spec
#define CKM_RSA_PKCS_KEY_PAIR_GEN     0x00000000
#define CKM_DH_PKCS_KEY_PAIR_GEN      0x00000020
#define CKM_DSA_KEY_PAIR_GEN          0x00000010

//This is NSS's define for invalid mechanism
#define CKM_INVALID_MECHANISM 0xffffffffL

/*
 * Given an SSMKeyGenType, return the PKCS11 mechanism that will
 * perform the correct key generation.
 */
CMUint32
cryptojs_convert_to_mechanism(SSMKeyGenType keyGenType)
{
    CMUint32 retMech;

    switch (keyGenType) {
    case rsaEnc:
    case rsaDualUse:
    case rsaSign:
    case rsaNonrepudiation:
    case rsaSignNonrepudiation:
        retMech = CKM_RSA_PKCS_KEY_PAIR_GEN;
        break;
    case dhEx:
        retMech = CKM_DH_PKCS_KEY_PAIR_GEN;
        break;
    case dsaSign:
    case dsaSignNonrepudiation:
    case dsaNonrepudiation:
        retMech = CKM_DSA_KEY_PAIR_GEN;
        break;
    default:
        retMech = CKM_INVALID_MECHANISM;
    }
    return retMech;
}

CMTStatus
cryptojs_generateOneKeyPair(PCMT_CONTROL control, CMUint32 keyGenContext,
                            SSMKeyGenType keyGenType, int keySize,
                            char *params, CMUint32 *keyId)
{
  CMTStatus rv;
  
  rv = CMT_GenerateKeyPair(control, keyGenContext,
                           cryptojs_convert_to_mechanism(keyGenType),
                           NULL,
                           keySize, keyId);
  if (rv != CMTSuccess) {
    goto loser;
  }
  return CMT_SetNumericAttribute(control, *keyId,
                                 SSM_FID_KEYPAIR_KEY_GEN_TYPE, keyGenType);
 loser:
  return rv;
}

/*
 * FUNCTION: cryptojs_ReadArgsAndGenerateKey
 * -------------------------------------
 * INPUTS:
 *    control
 *        A established control connection with PSM.
 *    cx
 *        The JSContext associated with the execution of the corresponging
 *        crypto.generateCRMFRequest call
 *    keyGenContext
 *        The resource id of a key gen context in PSM with which to
 *        generate the one key.
 *    argv
 *        A pointer to an array of JavaScript parameters passed to the
 *        method crypto.generateCRMFRequest.  The array should have the
 *        3 arguments keySize, "keyParams", and "keyGenAlg" mentioned in
 *        the definition of crypto.generateCRMFRequest at the following
 *        document http://warp/hardcore/library/arch/cert-issue.html
 *    keyid
 *        A pointer to a pre-allocated chunk of memory where the funciton
 *        can place a copy of the resource id corresponging to the new key
 *        created by PSM.
 *    keyGenType
 *        A pointer to a pre-allocated chunk of memory where the function
 *        can place a copy of the key gen type associated with the key
 *        generated by this function.
 *
 * NOTES:
 * This function takes care of reading a set of 3 parameters that define
 * one key generation.  The argv pointer should be one that originates
 * from the argv parameter passed in to the method nsCrypto::GenerateCRMFRequest.
 * The function interprets the argument in the first index as an integer and
 * passes that as the key size for the key generation-this parameter is
 * mandatory.  The second parameter is read in as a string.  This value can
 * be null in JavaScript world and everything will still work.  The third
 * parameter is a mandatory string that indicates what kind of key to generate.
 * There should always be 1-to-1 correspondence between the strings compared
 * in the function cryptojs_interpret_key_gen_type and the strings listed in
 * document at http://docs.iplanet.com/docs/manuals/psm/11/cmcjavascriptapi.html 
 * under the definition of the method generateCRMFRequest, for the parameter
 * "keyGenAlgN".  After reading the parameters, the function then sends
 * a message to PSM telling it to generate the key passing the parameters
 * parsed from the JavaScript routine.  
 *
 * RETURN:
 * CMTSuccess if creating the Key was successful.  Any other return value
 * indicates an error.
 */

CMTStatus
cryptojs_ReadArgsAndGenerateKey(PCMT_CONTROL control, JSContext *cx,
                                CMUint32 keyGenContext, jsval *argv,
                                CMUint32 *keyid, SSMKeyGenType *keyGenType)
{
    JSString  *jsString;
    char      *params, *keyGenAlg;
    int        keySize;
    CMTStatus  rv;

    if (!JSVAL_IS_INT(argv[0])) {
        JS_ReportError(cx, "%s%s\n", JS_ERROR,
                       "passed in non-integer for key size");
        goto loser;
    }
    keySize = JSVAL_TO_INT(argv[0]);
    if (JSVAL_IS_NULL(argv[1])) {
      params = nsnull;
    } else {
      jsString = JS_ValueToString(cx,argv[1]);
      params   = JS_GetStringBytes(jsString);
    }

    if (JSVAL_IS_NULL(argv[2])) {
        JS_ReportError(cx,"%s%s\n", JS_ERROR,
                       "key generation type not specified");
        goto loser;
    }
    jsString = JS_ValueToString(cx, argv[2]);
    keyGenAlg = JS_GetStringBytes(jsString);
    *keyGenType = cryptojs_interpret_key_gen_type(keyGenAlg);
    if (*keyGenType == invalidKeyGen) {
        JS_ReportError(cx, "%s%s%s", JS_ERROR,
                       "invalid key generation argument:",
                       keyGenAlg);
        goto loser;
    }
    rv = cryptojs_generateOneKeyPair(control,
                                     keyGenContext,
                                     *keyGenType,
                                     keySize,
                                     params,
                                     keyid);

    if (rv != CMTSuccess) {
        JS_ReportError(cx,"%s%s%s", JS_ERROR,
                       "could not generate the key for algorithm",
                       keyGenAlg);
        goto loser;
    }
    return CMTSuccess;
loser:
    CMT_DestroyResource(control, keyGenContext, SSM_RESTYPE_KEYGEN_CONTEXT);
    return CMTFailure;
}

/*
 * FUNCTION: cryptojs_CreateNewCRMFReqForKey
 * -------------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established communication with PSM.
 *    cx
 *        The JavaScript context to use in executing this portion
 *        of the crypto.generateCRMFRequest method.
 *    keyid
 *        The resource id of the key that should be associated with
 *        the CRMF request to create.
 *    keyGenType
 *        The type of key this request if for.  This is used in setting
 *        the keyUsage extension in the request.
 *    reqId
 *        A pointer to a pre-allocated chunk of memory where the function 
 *        can place a copy of the resource id corresponding to the CRMF
 *        request created by Cartman.
 *    dnValue
 *        The value to set as the subject name for the request.  This 
 *        correspoinds to the reqDN parameter to the JavaScript method
 *        crypto.generateCRMFRequest (This parameter is not optional.  The
 *        functions that call this makes sure this value is not NULL)
 *    regToken
 *        This parameter is the value to set as the Registration Token 
 *        Control in the CRMF request.  Setting the Registration Token
 *        Control is optional, so this parameter may be NULL.
 *    eaCertValue
 *        This parameter is the base64 encoded certificate to use when
 *        escrowing the private key.  If the keyType is not one that can
 *        be escrowed, then this parameter is ignored.  This value is 
 *        optional and can be passed in NULL.
 *    authenticatorValue
 *        This parameter is the value to set as the Authenticator Control
 *        in the CRMF request.  Setting the Authencticator Control is 
 *        optional, so this parameter may be NULL.
 *
 * NOTES:
 * This function creates a CRMF request which corresponds to one key.  This 
 * function should be called once for every single generated key that we 
 * want to request a certificate for.
 *
 * RETURN:
 * NS_OK if creating a new CRMF request was successful.  Any other return
 * value indicates an error.
 */
nsresult
cryptojs_CreateNewCRMFReqForKey(PCMT_CONTROL control, JSContext *cx, 
                                CMUint32 keyid, SSMKeyGenType keyGenType,
                                CMUint32 *reqId, CMTItem *dnValue, 
                                CMTItem *regTokenValue, CMTItem *eaCertValue,
                                CMTItem *authenticatorValue, 
                                PRBool reportErrors)
{
  CMUint32               currId;
  CMTStatus              rv;
  nsresult               frv = NS_ERROR_FAILURE;

      
  rv = CMT_CreateNewCRMFRequest(control, keyid, keyGenType, reqId);
    
  if (rv != CMTSuccess) {
    if (reportErrors) {
      JS_ReportError(cx, "%s", JS_ERROR_INTERNAL);
    }
    goto loser;
  }
  
  currId = *reqId;

  rv = CMT_SetStringAttribute(control, currId, SSM_FID_CRMFREQ_DN, dnValue);
  
  if (rv != CMTSuccess) {
    if (reportErrors) {
      JS_ReportError(cx, "%s", JS_ERROR_INTERNAL);
    }
    goto loser;
  }
  if (authenticatorValue->data != NULL) {
    rv = CMT_SetStringAttribute(control, currId, SSM_FID_CRMFREQ_AUTHENTICATOR,
                                authenticatorValue);
    if (rv != CMTSuccess) {
      if (reportErrors) {
        JS_ReportError(cx, "%s%s\n", JS_ERROR, 
                       "could not set authenticator control");
      }
	    goto loser;
    }
  }
  if (regTokenValue->data != NULL) {
    rv = CMT_SetStringAttribute(control, currId, SSM_FID_CRMFREQ_REGTOKEN,
                                     regTokenValue);
    if (rv != CMTSuccess) {
      if (reportErrors) {
        JS_ReportError(cx, "%s%s\n", JS_ERROR, 
                                     "could not set regToken control");
      }
	    goto loser;
    }
  }
  frv = NS_OK;
 loser:
  return frv;
}

/*
 * FUNCTION: cryptojs_DestroyCRMFRequests
 * --------------------------------------
 * INPUTS:
 *    control
 *        Pointer to an established control connection with PSM.
 *    rsrcds
 *        An array of CRMF resource ids to destroy.
 *    numRequests
 *        The length of the rsrcids array passed in.
 * NOTES:
 * This function destroys CRMF requests.
 */
void
cryptojs_DestroyCRMFRequests(PCMT_CONTROL control, CMUint32 *rsrcids, 
                             int numRequests)
{
    int i;
    
    for (i=0; i<numRequests; i++) {
      CMT_DestroyResource(control, rsrcids[i], SSM_RESTYPE_CRMF_REQUEST);
    }
}

nsIEventQueue* getUIEventQueue()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, 
                  NS_EVENTQUEUESERVICE_PROGID, &rv);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  nsIEventQueue* result = nsnull;
  rv = eventQService->GetThreadEventQueue(NS_UI_THREAD, &result);
  if (NS_FAILED(rv)) 
    return nsnull;
  
  return result;
}

/*
 * FUNCTION: cryptojs_CreateCRMFRequests
 * -------------------------------------
 * INPUTS:
 *    control
 *        Pointer to established control connection with PSM
 *    window
 *        The browser window in which this portion of JavaScript
 *        will be executed.
 *    keyids
 *        An array that specifies the needed information for each individual
 *        key that will have a CRMF request created for it.
 *    numRequests
 *        The length of the array keyids that is passed in.
 *    dnValue
 *        The value to set as the subject name for the request.  This 
 *        correspoinds to the reqDN parameter to the JavaScript method
 *        crypto.generateCRMFRequest (This parameter is not optional.  The
 *        functions that call this makes sure this value is not NULL)
 *    regToken
 *        This parameter is the value to set as the Registration Token 
 *        Control in the CRMF request.  Setting the Registration Token
 *        Control is optional, so this parameter may be NULL.
 *    authenticatorValue
 *        This parameter is the value to set as the Authenticator Control
 *        in the CRMF request.  Setting the Authencticator Control is 
 *        optional, so this parameter may be NULL.
 *    eaCertValue
 *        This parameter is the base64 encoded certificate to use when
 *        escrowing the private key.  If the keyType is not one that can
 *        be escrowed, then this parameter is ignored.  This value is 
 *        optional and can be passed in NULL.
 *    crmfObject
 *        This is the JavaScript CRMF object that will have the request
 *        property set to the result of the operatoin of encoding the
 *        requests.
 *    jsCallback
 *        A string that is passed to the method crypto.generateCRMFRequest.
 *        This string is JavaScript code to execute once generating the
 *        CRMF requests is done.
 *    cryptoObject
 *        The crypto object associated with the script being run.
 * NOTES
 * This function creates all of the CRMF requests for the key pairs created
 * and then executes the callback provedided by the JavaScript once the 
 * generation is done.
 * RETURN:
 * NS_OK if creating the CRMF request(s) is successful.  Any other return
 * value indicates an error in generating the requests.
 */
nsresult
cryptojs_CreateCRMFRequests(PCMT_CONTROL control, void *window, 
                            JSContext *cx,
                            CRYPTO_KeyPairInfo *keyids, 
                            int numRequests, CMTItem *dnValue, 
                            CMTItem *regTokenValue, 
                            CMTItem *authenticatorValue,
                            CMTItem *eaCertValue,  
                            nsCRMFObject *crmfObject,
                            CRYPTO_KeyGenContextHandler *handlerInfo,
                            nsCrypto *cryptoObject, PRBool reportErrors)
{
    int                 i;
    CMUint32           *rsrcids;
    nsresult            rv;
    char               *der;
    CMTStatus           crv;
    nsIEventQueue      *uiQueue=nsnull;
    RunnableEvent      *event = nsnull;
    JSPrincipals       *principals;
    jsval               retVal;

    rsrcids = new CMUint32[numRequests];
    if (rsrcids == nsnull) {
        return NS_ERROR_FAILURE;
    }
    memset(rsrcids, 0, sizeof(CMUint32)*numRequests);
    for (i=0; i<numRequests; i++) {
      rv = cryptojs_CreateNewCRMFReqForKey(control, cx, keyids[i].keyId, 
                                           keyids[i].keyGenType,
                                           &rsrcids[i], dnValue, 
                                           regTokenValue, eaCertValue, 
                                           authenticatorValue, reportErrors);
      if (rv != NS_OK) {
        goto loser;
      }
    }

    crv = CMT_EncodeCRMFRequest(control, rsrcids, numRequests, &der);
    cryptojs_DestroyCRMFRequests(control, rsrcids, numRequests);
    delete []rsrcids;
    if (crv != CMTSuccess || der == NULL) {
      if (reportErrors) {
        JS_ReportError(cx, "%s%s\n", JS_ERROR, 
                       "could not encode requests");
      }
      goto loser;
    }
    crmfObject->SetCRMFRequest(der);
    rv = handlerInfo->principals->GetJSPrincipals(&principals);
    if (NS_FAILED(rv)) {
      goto loser;
    }
    if (JS_EvaluateScriptForPrincipals(cx, handlerInfo->scope,  principals, 
                                       handlerInfo->jsCallback, 
                                       PL_strlen(handlerInfo->jsCallback),
                                       handlerInfo->fileName, 
                                       handlerInfo->lineNo,
                                       &retVal) != JS_TRUE) {
      if (reportErrors) {
        JS_ReportError(cx, "%s%s", JS_ERROR, 
                       "execution of callback failed");
      }
      rv = NS_ERROR_FAILURE;
    }
    JS_RemoveRoot(cx, &handlerInfo->scope);
    return NS_OK;
 loser:
    if (rsrcids){
      cryptojs_DestroyCRMFRequests(control, rsrcids, numRequests);
    }
    return NS_ERROR_FAILURE;
}

/*
 * FUNCTION: cryptojs_DestroyKeys
 * ------------------------------
 * INPUTS:
 *    control
 *        Pointer to an established control connection with PSM.
 *    keyids
 *        An array of information about the keys to destroy.
 *    numKeys
 *        The length of the array numKeys passed in.
 * NOTES:
 * This function destroys all of the keys in the keyids array passed in.
 * RETURN:
 * No return value.
 */
void 
cryptojs_DestroyKeys(PCMT_CONTROL control, CRYPTO_KeyPairInfo *keyids, 
                     int numKeys)
{
    int i;

    for (i=0; i<numKeys; i++) {
      CMT_DestroyResource(control, keyids[i].keyId, SSM_RESTYPE_KEY_PAIR);
    }
}

/*
 * FUNCTION: cryptojs_KeyGencontextEventHandler
 * --------------------------------------------
 * INPUTS:
 *    rsrcid
 *        The rsrcid for the resource that this handler is being called for.
 *    numProcessed
 *        The number of taks completed by PSM at the time this event
 *        handler was called.
 *    result
 *        The result code for the task completed event being reported.
 *    data
 *        An opaque pointer to data that was passed to the
 *        CMT_RegisterEventHandler when this event handler function was
 *        registered.
 * NOTES:
 * This is the function registered to handle the Task Completed Events
 * reported by the KeyGen Context in PSM.  Key Generation works as
 * follows.  The client creates a KeyGen Context with which the client
 * can create one or more key pairs.  After PSM is done generating a key
 * pair,it sends a Task Complete Event to the plug-in.  Eventually this
 * function will get called and then deals with the task completed event.
 * This function may get called multiple times, depending on how many keys
 * the client needs to generate.  Only when the correct number of keys are
 * generated does this function actually do something.  At that time, it
 * will de-register itself as an event handler, and create the CRMF requests
 * to correspond to the keys that were just created.
 * RETURN:
 * No return value.
 */

void
cryptojs_KeyGenContextEventHandler(CMUint32 rsrcid, CMUint32 numProcessed,
                                   CMUint32 result, void *data)
{
    CRYPTO_KeyGenContextHandler *handlerInfo;
    handlerInfo = (CRYPTO_KeyGenContextHandler*)data;
    nsCryptoRunnable *cryptoRunnable = nsnull;
    RunnableEvent    *runnable = nsnull;
    nsIEventQueue    *uiQueue  = nsnull;

    if (rsrcid == handlerInfo->keyGenContext) {
        if (numProcessed == handlerInfo->numRequests) {
          // Do all processing on the main thread.
          handlerInfo->result = (CMTStatus)result;
          cryptoRunnable = new nsCryptoRunnable(handlerInfo);
          if (cryptoRunnable == nsnull) {
            goto loser;
          }
          runnable = new RunnableEvent(cryptoRunnable);
          if (runnable == nsnull) {
            goto loser;
          }
          uiQueue = getUIEventQueue();
          uiQueue->PostEvent(runnable);
        }
    }
    return;
 loser:
    // Do some clean up in case of error.
    return;
}

nsresult
cryptojs_GetScriptPrincipal(JSContext *cx, JSScript *script, 
                            nsIPrincipal **result)
{
  if (!script) {
    *result = nsnull;
    return NS_OK;
  }
  JSPrincipals *jsp = JS_GetScriptPrincipals(cx, script);
  if (!jsp) {
    return NS_ERROR_FAILURE;
  }
  nsJSPrincipals *nsJSPrin = NS_STATIC_CAST(nsJSPrincipals *, jsp);
  *result = nsJSPrin->nsIPrincipalPtr;
  if (!result) {
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
cryptojs_GetFunctionObjectPrincipal(JSContext *cx, JSObject *obj, 
                                    nsIPrincipal **result)
{
  JSObject *parent = obj;
  do {
    JSClass *jsClass = JS_GetClass(cx, parent);
    const uint32 privateNsISupports = JSCLASS_HAS_PRIVATE | 
                                      JSCLASS_PRIVATE_IS_NSISUPPORTS;
    if (jsClass && (jsClass->flags & (privateNsISupports)) == 
        privateNsISupports)
      {
        nsISupports *supports = (nsISupports *) JS_GetPrivate(cx, parent);
        nsCOMPtr<nsIScriptObjectPrincipal> objPrin = 
          do_QueryInterface(supports);
        if (objPrin && NS_SUCCEEDED(objPrin->GetPrincipal(result)))
          return NS_OK;
      }
    parent = JS_GetParent(cx, parent);
  } while (parent);
  
  // Couldn't find a principal for this object.
  return NS_ERROR_FAILURE;
}

nsresult
cryptojs_GetFramePrincipal(JSContext *cx, JSStackFrame *fp, 
                           nsIPrincipal **principal)
{
  JSObject *obj = JS_GetFrameFunctionObject(cx, fp);
  if (!obj) {
    JSScript *script = JS_GetFrameScript(cx, fp);
    return cryptojs_GetScriptPrincipal(cx, script, principal);
  }
  return cryptojs_GetFunctionObjectPrincipal(cx, obj, principal);
}

nsIPrincipal*
nsCrypto::GetScriptPrincipal(JSContext *cx)
{
  JSStackFrame *fp = nsnull;
  nsIPrincipal *principal=nsnull;

  for (fp = JS_FrameIterator(cx, &fp); fp; fp = JS_FrameIterator(cx, &fp)) {
    cryptojs_GetFramePrincipal(cx, fp, &principal);
    if (principal != nsnull) {
      break;
    }
  }
  return principal;
}

NS_IMETHODIMP
nsCrypto::GenerateCRMFRequest(JSContext* cx, jsval* argv, PRUint32 argc, 
                              nsIDOMCRMFObject** aReturn)
{
  JSString           *jsString;
  JSObject           *crmfObject=NULL;
  CMUint32            localKeyGenContext, errorCode;
  PRUint32            i;
  int                 numRequests, currIndex;
  char               *reqDN;
  char               *authenticator, *regToken, *eaCert;
  char               *jsCallback=NULL;
  CMTItem             dnValue, regTokenValue,windowCx;
  CMTItem             authenticatorValue, eaCertValue;
  CMTStatus           rv;
  nsresult            nrv;
  PCMT_CONTROL        control;
  CMTItem window_context = {0, 0, 0};
  nsCRMFObject       *newObject;
  CRYPTO_KeyGenContextHandler *keyGenHandler;
  CRYPTO_KeyPairInfo          *keyids;
  const char         *fileName;
  PRUint32            lineNo;
  nsIPrincipal       *principals;

  /*
   * Get all of the parameters.
   */
  if (((argc-5) % 3) != 0) {
    JS_ReportError(cx, "%s", "%s%s\n", JS_ERROR,
                   "incorrect number of parameters");
    goto loser;
  }
  
  numRequests = (argc - 5)/3;
  keyids  = new CRYPTO_KeyPairInfo[numRequests];
  if (keyids == nsnull) {
    JS_ReportError(cx, "%s\n", JS_ERROR_INTERNAL);
    goto loser;
  }
  if (JSVAL_IS_NULL(argv[0])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no DN specified");
    goto loser;
  }
  jsString = JS_ValueToString(cx,argv[0]);
  
  reqDN = JS_GetStringBytes(jsString);
  if (JSVAL_IS_NULL(argv[1])) {
    regToken           = NULL;
    regTokenValue.data = NULL;
  } else {
    jsString = JS_ValueToString(cx, argv[1]);
    regToken = JS_GetStringBytes(jsString);
    regTokenValue.data = (unsigned char*)regToken;
    regTokenValue.len  = PL_strlen(regToken)+1;
  }
  if (JSVAL_IS_NULL(argv[2])) {
    authenticator           = NULL;
    authenticatorValue.data = NULL;
  } else {
    jsString = JS_ValueToString(cx, argv[2]);
    authenticator = JS_GetStringBytes(jsString);
    authenticatorValue.data = (unsigned char*)authenticator;
    authenticatorValue.len  = PL_strlen(authenticator)+1;
  }
  if (JSVAL_IS_NULL(argv[3])) {
    eaCert           = NULL;
    eaCertValue.data = NULL;
  } else {
    jsString = JS_ValueToString(cx, argv[3]);
    eaCert           = JS_GetStringBytes(jsString);
    eaCertValue.data = (unsigned char*)eaCert;
    eaCertValue.len  = PL_strlen(eaCert)+1;
  }
  if (JSVAL_IS_NULL(argv[4])) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "no completion "
                   "function specified");
    goto loser;        
  }
  jsString = JS_ValueToString(cx, argv[4]);
  jsCallback = JS_GetStringBytes(jsString);
  dnValue.data = (unsigned char*)reqDN;
  dnValue.len  = PL_strlen(reqDN) + 1;
  
  nrv = mPSM->GetControlConnection(&control);
  if (nrv != NS_OK) {
    goto loser;
  }
  windowCx.data = nsnull;
  windowCx.len  = 0;
  windowCx.type = 0;
  rv = CMT_CreateResource(control, SSM_RESTYPE_KEYGEN_CONTEXT, &windowCx,
                          &localKeyGenContext, &errorCode);

  //Register an event handler with the psm client libraries so that we get 
  //notified when the key gens are done.
  newObject = new nsCRMFObject();
  if (newObject == NULL) {
    JS_ReportError(cx, "%s%s\n", JS_ERROR, "could not create crmf JS object");
    goto loser;
  }
  keyGenHandler = new CRYPTO_KeyGenContextHandler;
  if(keyGenHandler == nsnull) {
    goto loser;
  }
#if 0
  nsJSUtils::nsGetCallingLocation(cx, &fileName, &lineNo);
#else
  fileName = nsnull;
  lineNo   = 0;
#endif
  principals = GetScriptPrincipal(cx);

  keyGenHandler->result        = CMTFailure;
  keyGenHandler->numRequests   = numRequests;
  keyGenHandler->keyGenContext = localKeyGenContext;
  keyGenHandler->context       = nsnull;
  keyGenHandler->keyids        = keyids;
  keyGenHandler->reqDN         = dnValue;
  keyGenHandler->regToken      = regTokenValue;
  keyGenHandler->authenticator = authenticatorValue;
  keyGenHandler->eaCert        = eaCertValue;
  keyGenHandler->crmfObject    = newObject;
  keyGenHandler->control       = control;
  keyGenHandler->cryptoObject  = this;
  keyGenHandler->cx            = cx;
  keyGenHandler->scope         = JS_GetParent(cx, (JSObject*)mScriptObject);
  keyGenHandler->jsCallback    = PL_strdup(jsCallback);
  keyGenHandler->principals    = principals;
  keyGenHandler->fileName      = fileName;
  keyGenHandler->lineNo        = lineNo;

  
  rv = CMT_RegisterEventHandler(control, SSM_TASK_COMPLETED_EVENT, 
                                localKeyGenContext,
                                (void_fun)cryptojs_KeyGenContextEventHandler,
                                keyGenHandler);
  if (rv != CMTSuccess) {
    goto loser;
  }

  if (eaCertValue.data) {
    rv = CMT_SetStringAttribute(control, localKeyGenContext, 
                                SSM_FID_KEYGEN_ESCROW_AUTHORITY,
                                &eaCertValue);
    if (rv != CMTSuccess) {
      goto loser;
    }
  }
  for (i=5; i<argc; i+=3) {
    currIndex = (i-5)/3;
    rv = cryptojs_ReadArgsAndGenerateKey(control, cx, localKeyGenContext, 
                                         &argv[i], &keyids[currIndex].keyId,
                                         &keyids[currIndex].keyGenType);
    if (rv != CMTSuccess) {
      goto loser;
    }
  }
  rv = CMT_FinishGeneratingKeys(control, localKeyGenContext);
  if (rv != CMTSuccess) {
    goto loser;
  }
  *aReturn = newObject;
  //Give a reference to the returnee.
  NS_ADDREF(newObject);
  return NS_OK;
 loser:
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCrypto::ImportUserCertificates(const nsAReadableString& aNickname, 
                                 const nsAReadableString& aCmmfResponse, 
                                 PRBool aDoForcedBackup, 
                                 nsAWritableString& aReturn)
{
  char *nickname=nsnull, *cmmfResponse=nsnull;
  nsresult nrv;
  PCMT_CONTROL control;
  CMTStatus crv;
  char *retString=nsnull;
  char *freeString=nsnull;

  nickname = ToNewCString(aNickname);
  cmmfResponse = ToNewCString(aCmmfResponse);
  if (PL_strcmp("null", nickname) == 0) {
    nsCRT::free(nickname);
    nickname = nsnull;
  }
  nrv = mPSM->GetControlConnection(&control);
  if (nrv != NS_OK) {
    goto loser;
  }
  crv = CMT_ProcessCMMFResponse(control, nickname, cmmfResponse, 
                                (aDoForcedBackup) ? CM_TRUE : CM_FALSE,
                                nsnull);

  if (crv != CMTSuccess) {
    retString = PR_smprintf("%s%s", 
                            JS_ERROR,"Could not import user certificates");
    freeString = retString;
    goto loser;
  }
  retString = "";
 loser:
  aReturn.Assign(NS_ConvertASCIItoUCS2(retString));
  if (freeString != NULL) {
    PR_smprintf_free(freeString);
  }
  if (nickname) {
    nsCRT::free(nickname);
  }
  if (cmmfResponse) {
    nsCRT::free(cmmfResponse);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCrypto::PopChallengeResponse(const nsAReadableString& aChallenge, 
                               nsAWritableString& aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCrypto::Random(PRInt32 aNumBytes, nsAWritableString& aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCrypto::SignText(JSContext *cx, jsval *argv, PRUint32 argc,
                   nsAWritableString& aReturn)
{
  return NS_ERROR_FAILURE;
}

void
alertUser(char *message)
{
  nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
  JSContext *jsContext;
  nsresult rv;

  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = appShell->GetHiddenWindowAndJSContext(getter_AddRefs(hiddenWindow),
                                               &jsContext); 
    if (NS_SUCCEEDED(rv)) {
      // set up arguments for window.alert

      void *stackPtr;
      jsval *argv = JS_PushArguments(jsContext, &stackPtr, "s", message);
      if (argv) {
        hiddenWindow->Alert(jsContext, argv, 1);
        JS_PopArguments(jsContext, stackPtr);
      }
    }
  }
}


NS_IMETHODIMP
nsCrypto::Alert(const nsAReadableString& aMessage)
{
  char *message;

  message = ToNewCString(aMessage);
  if (message == nsnull) {
    return NS_ERROR_FAILURE;
  }
  alertUser(message);
  delete []message;
  return NS_OK;
}

NS_IMETHODIMP
nsCrypto::Logout()
{
  PCMT_CONTROL control = nsnull;
  nsresult rv;
  
  rv = mPSM->GetControlConnection(&control);
  if (NS_FAILED(rv)) {
    return rv;
  }
  CMT_LogoutAllTokens(control);
  return NS_OK;
}

NS_IMETHODIMP
nsCrypto::DisableRightClick()
{
  return NS_OK;
}

nsCRMFObject::nsCRMFObject()
{
  NS_INIT_ISUPPORTS();
  mScriptObject = nsnull;
}

nsCRMFObject::~nsCRMFObject()
{
}

NS_IMETHODIMP
nsCRMFObject::GetScriptObject(nsIScriptContext *aContext, 
                              void** aScriptObject)
{
  nsresult rv = NS_OK;
  
  if (mScriptObject == nsnull) {
    nsIDOMScriptObjectFactory *factory=nsnull;
    
    rv = nsCrypto::GetScriptObjectFactory(&factory);
    if (rv == NS_OK) {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();
      rv = factory->NewScriptCRMFObject(aContext, 
                                (nsISupports *)(nsIDOMCRMFObject *)this, 
                                (nsISupports *)global, (void**)&mScriptObject);
      NS_IF_RELEASE(factory);
    }
  }
  *aScriptObject = mScriptObject;
  return rv;
}

NS_IMETHODIMP
nsCRMFObject::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult
nsCRMFObject::init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsCRMFObject::GetRequest(nsAWritableString& aRequest)
{
  aRequest.Assign(mBase64Request);
  return NS_OK;
}

nsresult
nsCRMFObject::SetCRMFRequest(char *inRequest)
{
  mBase64Request.AssignWithConversion(inRequest);  
  return NS_OK;
}

static void PR_CALLBACK
handleRunnableEvent(RunnableEvent* aEvent)
{
  aEvent->mRunnable->Run();
}

static void PR_CALLBACK
destroyRunnableEvent(RunnableEvent* aEvent)
{
  delete aEvent;
}

RunnableEvent::RunnableEvent(nsIRunnable* runnable)
  :  mRunnable(runnable)
{
  NS_ADDREF(mRunnable);
  PL_InitEvent(this, nsnull, PLHandleEventProc(handleRunnableEvent), 
               PLDestroyEventProc(&destroyRunnableEvent));
}

RunnableEvent::~RunnableEvent()
{
  NS_RELEASE(mRunnable);
}

nsCryptoRunnable::nsCryptoRunnable(CRYPTO_KeyGenContextHandler *handlerInfo)
{
  NS_INIT_REFCNT();
  mHandlerInfo = handlerInfo;
  JS_AddNamedRoot(mHandlerInfo->cx, &mHandlerInfo->scope,
                  "nsCryptoRunnable::mScope");
}

nsCryptoRunnable::~nsCryptoRunnable() {
  NS_IF_RELEASE(mHandlerInfo->principals);
  PR_FREEIF(mHandlerInfo->jsCallback);
  delete mHandlerInfo;
}

NS_IMETHODIMP
nsCryptoRunnable::Run() {
  nsresult          rv;
  PRBool            reportErrors = PR_FALSE;
  JSStackFrame     *fp=nsnull;
  nsCrypto         *cryptoObject;
#if 0 
  char              msg[512]; 
  nsIScriptContext *scriptCx;
  JSObject         *obj;
  PR_snprintf(msg, 512, "Javi's test of JS_ReportError from a separate thread.\n");
  if (mHandlerInfo->cx->fp == nsnull) {
    rv = nsJSUtils::nsGetDynamicScriptContext(mHandlerInfo->cx, &scriptCx);
    if (NS_FAILED(rv)){
      reportErrors = PR_FALSE;
    } else {
      rv = mHandlerInfo->cryptoObject->GetScriptObject(scriptCx,
                                                       (void**)&obj);
      if (NS_FAILED(rv)) {
        reportErrors = PR_FALSE;
      } else {
        fp = (JSStackFrame*)JS_GetPrivate(mHandlerInfo->cx,obj);
      }
    }
  }
  
  JS_ReportError(mHandlerInfo->cx, msg);
#endif
  if (mHandlerInfo->result != CMTSuccess) {
    if (reportErrors) {
      JS_ReportError(mHandlerInfo->cx, "%s%s\n", JS_ERROR,
                     "generation of key(s) failed");
    }
    goto loser;
  }
  
  cryptoObject = mHandlerInfo->cryptoObject;
  CMT_UnregisterEventHandler(mHandlerInfo->control, 
                             SSM_TASK_COMPLETED_EVENT,
                             mHandlerInfo->keyGenContext);
  rv = cryptojs_CreateCRMFRequests(mHandlerInfo->control,
                                   mHandlerInfo->context, 
                                   mHandlerInfo->cx,
                                   mHandlerInfo->keyids,
                                   mHandlerInfo->numRequests,
                                   &mHandlerInfo->reqDN,
                                   &mHandlerInfo->regToken,
                                   &mHandlerInfo->authenticator,
                                   &mHandlerInfo->eaCert,
                                   mHandlerInfo->crmfObject,
                                   mHandlerInfo, cryptoObject, reportErrors);
  if (fp) {
    mHandlerInfo->cx->fp = nsnull;
  }
  cryptojs_DestroyKeys(mHandlerInfo->control, mHandlerInfo->keyids, 
                       mHandlerInfo->numRequests);
  CMT_DestroyResource(mHandlerInfo->control,
                      mHandlerInfo->keyGenContext,
                      SSM_RESTYPE_KEYGEN_CONTEXT);
  delete []mHandlerInfo->keyids;
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_OK;
 loser:
  if (fp) {
    mHandlerInfo->cx->fp = nsnull;
  }
  cryptojs_DestroyKeys(mHandlerInfo->control, mHandlerInfo->keyids, 
                       mHandlerInfo->numRequests);
  CMT_DestroyResource(mHandlerInfo->control,
                      mHandlerInfo->keyGenContext,
                      SSM_RESTYPE_KEYGEN_CONTEXT);
  delete []mHandlerInfo->keyids;
  return NS_ERROR_FAILURE;
}


nsPkcs11::nsPkcs11()
{
  NS_INIT_REFCNT();
  mPSM = nsnull;
  mScriptObject = nsnull;
}

nsPkcs11::~nsPkcs11()
{
  NS_IF_RELEASE(mPSM);
}

nsresult
nsPkcs11::init()
{
  return getPSMComponent(&mPSM);
}

NS_IMETHODIMP
nsPkcs11::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
nsPkcs11::GetScriptObject(nsIScriptContext *aContext, 
                          void** aScriptObject)
{
  nsresult rv = NS_OK;
  
  if (mScriptObject == nsnull) {
    nsIDOMScriptObjectFactory *factory=nsnull;
    
    rv = nsCrypto::GetScriptObjectFactory(&factory);
    if (rv == NS_OK) {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();
      rv = factory->NewScriptPkcs11(aContext, 
                                (nsISupports *)(nsIDOMPkcs11 *)this, 
                                (nsISupports *)global, (void**)&mScriptObject);
      NS_IF_RELEASE(factory);
    }
  }
  *aScriptObject = mScriptObject;
  return rv;
}


PRBool
confirm_user(char *message)
{
  nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
  JSContext *jsContext;
  PRBool confirmation = PR_FALSE;
  nsresult rv;

  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = appShell->GetHiddenWindowAndJSContext(getter_AddRefs(hiddenWindow),
                                               &jsContext); 
    if (NS_SUCCEEDED(rv)) {
      // set up arguments for window.confirm

      void *stackPtr;
      jsval *argv = JS_PushArguments(jsContext, &stackPtr, "s", message);
      if (argv) {
        hiddenWindow->Confirm(jsContext, argv, 1, &confirmation);
        JS_PopArguments(jsContext, stackPtr);
      }
    }
  }
  return confirmation;
}

//These defines are returned by PSM as the type of module deleted.
/* Cryptographic module types */
#define SECMOD_EXTERNAL 0       /* external module */
#define SECMOD_INTERNAL 1       /* internal default module */
#define SECMOD_FIPS     2       /* internal fips module */


NS_IMETHODIMP
nsPkcs11::Deletemodule(const nsAReadableString& aModuleName, PRInt32* aReturn)
{
  PCMT_CONTROL control;
  char *errorString = nsnull, *warning = nsnull, *wholeMsg = nsnull;
  nsresult rv;
  CMTStatus status;
  char *moduleName=nsnull, *modPrompt = nsnull, *successString = nsnull;
  int moduleType, length;
  PRBool okay;

  rv = mPSM->GetControlConnection(&control);
  if (NS_FAILED(rv)) {
    goto loser;
  }
    
  if (aModuleName.IsEmpty()) {
    CMT_GetLocalizedString(control, SSM_STRING_BAD_MOD_NAME,
                           &errorString);
    *aReturn = JS_ERR_BAD_MODULE_NAME;
    goto loser;
  }
  moduleName = ToNewCString(aModuleName);
  status = CMT_GetLocalizedString(control, SSM_STRING_DEL_MOD_WARN,
                                &warning);
  if (status != CMTSuccess) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }
  status = CMT_GetLocalizedString(control, SSM_STRING_MOD_PROMPT,
                                  &modPrompt);
  if (status != CMTSuccess) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }
  length = strlen(warning) + strlen(modPrompt) +
           strlen(moduleName) + 5;
  
  wholeMsg = new char[length];
  if (!wholeMsg) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }
  
  wholeMsg[0] = '\0';
  strcat(wholeMsg, warning);
  strcat(wholeMsg, "\n");
  strcat(wholeMsg, modPrompt);
  strcat(wholeMsg, moduleName);
  nsCRT::free(warning);
  nsCRT::free(modPrompt);
  okay = confirm_user(wholeMsg);
  delete []wholeMsg;
  //To avoid freeing up again in loser clause.
  modPrompt = warning = wholeMsg = nsnull;
  if (okay) {
    /* Send a message telling PSM to delete this module*/
    status = CMT_DeleteModule(control, moduleName, &moduleType);
    delete []moduleName;
    moduleName = nsnull;
    if (status == CMTSuccess) {
      if (moduleType == SECMOD_EXTERNAL) {
        CMT_GetLocalizedString(control, SSM_STRING_EXT_MOD_DEL,
                               &successString);
        *aReturn = JS_OK_DEL_EXTERNAL_MOD;
      } else {
        *aReturn = JS_OK_DEL_INTERNAL_MOD;
        CMT_GetLocalizedString(control, SSM_STRING_INT_MOD_DEL,
                               &successString);
      }
    } else {
      *aReturn = JS_ERR_DEL_MOD;
      CMT_GetLocalizedString(control, SSM_STRING_MOD_DEL_FAIL,
                             &errorString);
      goto loser;
    }
  } else {
    *aReturn = JS_ERR_USER_CANCEL_ACTION;
  }
  if (successString != NULL) {
    alertUser(successString);
    nsCRT::free(successString);
  }
  return NS_OK;
 loser:
  if (moduleName) {
    delete []moduleName;
  }
  if (errorString != nsnull) {
    alertUser(errorString);
    nsCRT::free(errorString);
  }
  if (warning != nsnull) {
    nsCRT::free(warning);
  }
  if (modPrompt != nsnull) {
    nsCRT::free(modPrompt);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPkcs11::Addmodule(const nsAReadableString& aModuleName, 
                    const nsAReadableString& aLibraryFullPath, 
                    PRInt32 aCryptoMechanismFlags, 
                    PRInt32 aCipherFlags, PRInt32* aReturn)
{
  char *warning = nsnull, *dllPrompt = nsnull, *modPrompt = nsnull;
  char *confirmationMessage;
  char *moduleName = nsnull, *libraryPath = nsnull;
  char *errorString = nsnull;
  CMTStatus status;
  PCMT_CONTROL control = nsnull;
  nsresult rv;
  PRBool proceed;
  PRInt32 length;

  rv = mPSM->GetControlConnection(&control);
  status = CMT_GetLocalizedString(control,
                                  SSM_STRING_ADD_MOD_WARN,
                                  &warning);
  if (status != CMTSuccess) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }
  
  status = CMT_GetLocalizedString(control,
                                  SSM_STRING_MOD_PROMPT,
                                  &modPrompt);
  if (status != CMTSuccess) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }
  
  status = CMT_GetLocalizedString(control,
                                  SSM_STRING_DLL_PROMPT,
                                  &dllPrompt);
  if (status != CMTSuccess) {
    *aReturn = JS_ERR_INTERNAL;
    goto loser;
  }

  moduleName  = ToNewCString(aModuleName);
  libraryPath = ToNewCString(aLibraryFullPath);

  length = strlen(warning)   + strlen(modPrompt)   + strlen(moduleName) +
           strlen(dllPrompt) + strlen(libraryPath) + 5;
  confirmationMessage = new char[length];
  if (!confirmationMessage)
    goto loser;

  
  confirmationMessage[0] = '\0';
  strcat(confirmationMessage, warning);
  strcat(confirmationMessage, "\n");
  strcat(confirmationMessage, modPrompt);
  strcat(confirmationMessage, moduleName);
  strcat(confirmationMessage, "\n");
  strcat(confirmationMessage, dllPrompt);
  strcat(confirmationMessage, libraryPath);
  proceed = confirm_user(confirmationMessage);
  delete []confirmationMessage;
  
  if (proceed) {
    status = CMT_AddNewModule(control, moduleName, libraryPath,
                              aCryptoMechanismFlags, aCipherFlags);
    if (status == CMTSuccess) {
      CMT_GetLocalizedString(control, SSM_STRING_ADD_MOD_SUCCESS,
                             &errorString);
      *aReturn = JS_OK_ADD_MOD;
      goto loser;
    } else if (status == (CMTStatus)-2) {
      *aReturn = JS_ERR_ADD_DUPLICATE_MOD;
      goto loser;
    } else {
      *aReturn = JS_ERR_ADD_MOD;
      CMT_GetLocalizedString(control, SSM_STRING_ADD_MOD_FAILURE, 
                             &errorString);
      goto loser;
    }
  } else {
    *aReturn = JS_ERR_USER_CANCEL_ACTION;
  }
  delete warning;
  delete modPrompt;
  delete dllPrompt;
  return NS_OK;
 loser:
  if (warning)
    delete warning;
  if (modPrompt)
    delete modPrompt;
  if (dllPrompt)
    delete dllPrompt;
  //Alert the user about what actually happened.
  if (errorString) {
    alertUser(errorString);
    PR_Free(errorString);
  }
  return NS_OK;
}

