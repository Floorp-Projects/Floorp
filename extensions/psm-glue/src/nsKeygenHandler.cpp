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

//For some weird reason, nsProxiedService has to be the first file 
//included.  Don't ask me, I'm just the messenger.
#include "nsProxiedService.h"
#include "nsKeygenHandler.h"
#include "nsVoidArray.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIContent.h"
#include "nsIPSMComponent.h"
#include "nsIPSMUIHandler.h"
#include "nsPSMUICallbacks.h"
#include "nsCrypto.h"
#include "cmtcmn.h"
#include "cmtjs.h"

//These defines are taken from the PKCS#11 spec
#define CKM_RSA_PKCS_KEY_PAIR_GEN     0x00000000
#define CKM_DH_PKCS_KEY_PAIR_GEN      0x00000020
#define CKM_DSA_KEY_PAIR_GEN          0x00000010

static NS_DEFINE_IID(kFormProcessorIID,   NS_IFORMPROCESSOR_IID); 
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);

static const char *mozKeyGen = "-mozilla-keygen";

NS_IMPL_ADDREF(nsKeygenFormProcessor); 
NS_IMPL_RELEASE(nsKeygenFormProcessor); 
NS_IMPL_QUERY_INTERFACE(nsKeygenFormProcessor, kFormProcessorIID); 

MOZ_DECL_CTOR_COUNTER(nsKeygenFormProcessor);

nsKeygenFormProcessor::nsKeygenFormProcessor()
	:	mPSM(0)
{ 
   NS_INIT_REFCNT();
   getPSMComponent(&mPSM);
   MOZ_COUNT_CTOR(nsKeygenFormProcessor);
} 

nsKeygenFormProcessor::~nsKeygenFormProcessor()
{
  MOZ_COUNT_DTOR(nsKeygenFormProcessor);
  NS_IF_RELEASE(mPSM);
}

NS_METHOD
nsKeygenFormProcessor::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
  nsresult rv;
  NS_ENSURE_NO_AGGREGATION(aOuter);
  nsKeygenFormProcessor* formProc = new nsKeygenFormProcessor();
  if (formProc == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(formProc);
  rv = formProc->QueryInterface(aIID, aResult);
  NS_RELEASE(formProc);
  return rv;
}

char *
nsKeygenFormProcessor::ChooseToken(PCMT_CONTROL control, 
				   CMKeyGenTagArg *psmarg,
				   CMKeyGenTagReq *reason)
{
  CMUint32 resID;
  CMTStatus crv;
  CMTItem url;
  char *keyString = nsnull;
  nsresult rv = NS_OK;
  NameList *tokenNames;
  int i;

  // In this case, PSM provided us with a list of potential tokens to choose
  // from, but we're gonna make it use it's UI for now, so let's delte the
  // memory associated with the structure it sent back.
  tokenNames = (NameList*)psmarg->current;
  for (i=0; i < tokenNames->numitems; i++) {
    nsCRT::free(tokenNames->names[i]);
  }
  nsCRT::free((char*)tokenNames);
  resID = psmarg->rid;
  memset(&url, 0, sizeof(CMTItem));
  NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
  crv = CMT_GetStringAttribute(control, resID, SSM_FID_CHOOSE_TOKEN_URL, &url);
  if (crv != CMTSuccess) {
    goto loser;
  }
  if (NS_SUCCEEDED(rv)) {
    handler->DisplayURI(400, 300, PR_TRUE, (char*)url.data, nsnull);
  } else {
    goto loser;
  }
  return CMT_GetGenKeyResponse(control, psmarg, reason);
 loser:
  if (keyString)
    nsCRT::free(keyString);
  return nsnull;
}

char *
nsKeygenFormProcessor::SetUserPassword(PCMT_CONTROL control, 
				       CMKeyGenTagArg *psmarg,
				       CMKeyGenTagReq *reason)
{
  nsresult rv;
  CMTStatus crv;
  CMTItem url;
  char *keystring=nsnull;

  // We need to delete the memory the PSM client API allocated for us since
  // we're just gonna tell it to use it's own UI.
  nsCRT::free((char*)psmarg->current);
  NS_WITH_PROXIED_SERVICE(nsIPSMUIHandler, handler, nsPSMUIHandlerImpl::GetCID(), NS_UI_THREAD_EVENTQ, &rv);
  memset (&url, 0, sizeof(CMTItem));
  crv = CMT_GetStringAttribute(control,psmarg->rid, SSM_FID_INIT_DB_URL, &url);
  if (crv != CMTSuccess || NS_FAILED(rv)){
    goto loser;
  }

  handler->DisplayURI(500, 450, PR_TRUE, (char*)url.data, nsnull);
  
  return CMT_GetGenKeyResponse(control, psmarg, reason);
 loser:
  if (keystring)
    nsCRT::free(keystring);
  return nsnull;
}

nsresult
nsKeygenFormProcessor::GetPublicKey(nsString& value, nsString& challenge, 
				    nsString& keyType,
				    nsString& outPublicKey, nsString& pqg)
{
    PCMT_CONTROL control;
    nsresult rv;
    CMKeyGenParams *params = nsnull;
    CMKeyGenTagArg *psmarg = nsnull;
    CMKeyGenTagReq reason;
    char *emptyCString = "null";
    char *keystring = nsnull;

    rv = mPSM->GetControlConnection(&control);
    if (NS_FAILED(rv)) {
	goto loser;
    }
    params = new CMKeyGenParams;
    if (params == nsnull) {
      goto loser;
    }
    params->typeString = (keyType.IsEmpty()) ? emptyCString : 
                                               keyType.ToNewCString();
    params->challenge  = (challenge.IsEmpty()) ? emptyCString : 
                                                 challenge.ToNewCString();
    params->choiceString = value.ToNewCString();
    params->pqgString = (pqg.IsEmpty()) ? emptyCString : pqg.ToNewCString();
    psmarg = new CMKeyGenTagArg;
    if (psmarg == nsnull) {
      goto loser;
    }
    // ARGH, while this is going on, we need to lock the control
    // connection so that the event loop doesn't drop our response on
    // the floor.
    CMT_LockConnection(control);
    psmarg->op = CM_KEYGEN_START;
    psmarg->rid = 0;
    psmarg->tokenName = NULL;
    psmarg->current = params;
    keystring = CMT_GenKeyOldStyle(control, psmarg, &reason);
    while (!keystring) {
      psmarg->op = reason;
      switch (psmarg->op) {
      case CM_KEYGEN_PICK_TOKEN:
	keystring = ChooseToken(control, psmarg, &reason);
	break;
      case CM_KEYGEN_SET_PASSWORD:
	keystring = SetUserPassword(control, psmarg, &reason);
	break;
      case CM_KEYGEN_ERR:
      default:
	goto loser;
      }
    }
    CMT_UnlockConnection(control);
    outPublicKey.AssignWithConversion(keystring);
    nsCRT::free(keystring);
    return NS_OK;
 loser:
    return NS_ERROR_FAILURE;
}

NS_METHOD 
nsKeygenFormProcessor::ProcessValue(nsIDOMHTMLElement *aElement, 
				    const nsString& aName, 
				    nsString& aValue) 
{ 
#ifdef DEBUG_javi
  char *name = aName.ToNewCString(); 
  char *value = aValue.ToNewCString(); 
  printf("ProcessValue: name %s value %s\n",  name, value); 
  delete [] name; 
  delete [] value; 
#endif 
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLSelectElement>selectElement;
  nsresult res = aElement->QueryInterface(kIDOMHTMLSelectElementIID, 
					  getter_AddRefs(selectElement));
  if (NS_SUCCEEDED(res)) {
    nsAutoString keygenvalue;
    nsAutoString challengeValue;
    nsString publicKey;
    nsString mozillaKeygen;
    nsString mozType;

    mozType.AssignWithConversion("_moz-type");
    mozillaKeygen.AssignWithConversion(mozKeyGen);
    res = selectElement->GetAttribute(mozType, keygenvalue);

    if (NS_CONTENT_ATTR_HAS_VALUE == res && keygenvalue.Equals(mozillaKeygen)) {
      nsString challenge;
      nsString keyType;
      nsString keyTypeValue;
      nsString pqg, pqgValue;

      challenge.AssignWithConversion("challenge");
      pqg.AssignWithConversion("pqg");
      res = selectElement->GetAttribute(pqg, pqgValue);
      keyType.AssignWithConversion("keytype");
      res = selectElement->GetAttribute(keyType, keyTypeValue);
      if (NS_FAILED(res) || keyTypeValue.IsEmpty()) {
	// If this field is not present, we default to rsa.
	keyTypeValue.AssignWithConversion("rsa");
      }
      res = selectElement->GetAttribute(challenge, challengeValue);
      rv = GetPublicKey(aValue, challengeValue, keyTypeValue, 
			publicKey, pqgValue);
      aValue = publicKey;
    }
  }

  return rv; 
} 

NS_METHOD nsKeygenFormProcessor::ProvideContent(const nsString& aFormType, 
						nsVoidArray& aContent, 
						nsString& aAttribute) 
{ 
  nsString selectKey;
  nsresult rv;
  PCMT_CONTROL control;
  PRUint32 i;

  selectKey.AssignWithConversion("SELECT");
  if (aFormType.EqualsIgnoreCase(selectKey)) {
    nsString *selectString;
    char **result;

    rv = mPSM->GetControlConnection(&control);
    if (NS_FAILED(rv)) {
	goto loser;
    }
    result = CMT_GetKeyChoiceList(control, "rsa"/*Need to figure out if DSA*/,
				  nsnull);
    for (i=0; result[i] != nsnull; i++) {
	selectString = new nsString;
	selectString->AssignWithConversion(result[i]);
	aContent.AppendElement(selectString);
	delete []result[i];
    }
    delete []result;
    aAttribute.AssignWithConversion(mozKeyGen);
  }
  return NS_OK;
 loser:
  return NS_ERROR_FAILURE; 
} 



