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

#ifndef _NSKEYGENHANDLER_H_
#define _NSKEYGENHANDLER_H_
// Form Processor 
#include "nsIFormProcessor.h" 
#include "ssmdefs.h"
#include "cmtcmn.h"

class nsIPSMComponent;

class nsKeygenFormProcessor : public nsIFormProcessor { 
public: 
  nsKeygenFormProcessor(); 
  virtual ~nsKeygenFormProcessor();

  NS_IMETHOD ProcessValue(nsIDOMHTMLElement *aElement, 
                          const nsString& aName, 
                          nsString& aValue); 

  NS_IMETHOD ProvideContent(const nsString& aFormType, 
                            nsVoidArray& aContent, 
                            nsString& aAttribute); 
  NS_DECL_ISUPPORTS 

  static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

protected:
  nsresult GetPublicKey(nsString& value, nsString& challenge, 
			nsString& keyType, nsString& outPublicKey,
			nsString& pqg);
  char * ChooseToken(PCMT_CONTROL control, CMKeyGenTagArg *psmarg,  
		     CMKeyGenTagReq *reason);
  char * SetUserPassword(PCMT_CONTROL control, CMKeyGenTagArg *psmarg,  
		     CMKeyGenTagReq *reason);
  nsIPSMComponent *mPSM;
}; 

#endif //_NSKEYGENHANDLER_H_
