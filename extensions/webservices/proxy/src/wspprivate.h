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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao (vidur@netscape.com) (Original author)
 *   John Bandhauer (jband@netscape.com)
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

#ifndef __wspprivate_h__
#define __wspprivate_h__

// XPCOM includes
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIPropertyBag.h"
#include "nsIException.h"
#include "nsIExceptionService.h"
#include "nsIServiceManager.h"
#include "nsAString.h"

// SOAP includes
#include "nsISOAPCall.h"
#include "nsISOAPResponse.h"
#include "nsISOAPResponseListener.h"
#include "nsISOAPCallCompletion.h"
#include "nsISOAPFault.h"
#include "nsSOAPUtils.h"

// interface info includes
#include "xptcall.h"
#include "nsIInterfaceInfo.h"

// WSDL includes
#include "nsIWSDL.h"
#include "nsIWSDLLoader.h"
#include "nsIWSDLSOAPBinding.h"

// iix includes
#include "nsIGenericInterfaceInfoSet.h"

// Proxy includes
#include "nsIWebServiceProxy.h"
#include "nsIWSPInterfaceInfoService.h"

// Forward decls
class WSPCallContext;

class WSPFactory : public nsIWebServiceProxyFactory
{
public:
  WSPFactory();
  virtual ~WSPFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICEPROXYFACTORY  

  static nsresult C2XML(const nsACString& aCIdentifier,
                        nsAString& aXMLIdentifier);
  static void XML2C(const nsAString& aXMLIndentifier,
                    nsACString& aCIdentifier);
};

class WSPProxy : public nsXPTCStubBase,
                 public nsIWebServiceProxy,
                 public nsIClassInfo
{
public:
  WSPProxy();
  virtual ~WSPProxy();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICEPROXY
  NS_DECL_NSICLASSINFO

  // Would be nice to have a NS_DECL_NSXPTCSTUBBASE
  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);
  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

  void GetListenerInterfaceInfo(nsIInterfaceInfo** aInfo);
  void CallCompleted(WSPCallContext* aContext);

  static NS_METHOD
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

  static nsresult XPTCMiniVariantToVariant(uint8 aTypeTag,
                                           nsXPTCMiniVariant aResult,
                                           nsIInterfaceInfo* aInterfaceInfo,
                                           nsIVariant** aVariant);
  static nsresult ArrayXPTCMiniVariantToVariant(uint8 aTypeTag,
                                                nsXPTCMiniVariant aResult,
                                                PRUint32 aLength,
                                                nsIInterfaceInfo* aIfaceInfo,
                                                nsIVariant** aVariant);

  static nsresult VariantToValue(uint8 aTypeTag,
                                 void* aValue,
                                 nsIInterfaceInfo* aInterfaceInfo,
                                 nsIVariant* aProperty);
  static nsresult VariantToArrayValue(uint8 aTypeTag,
                                      nsXPTCMiniVariant* aResultSize,
                                      nsXPTCMiniVariant* aResultArray,
                                      nsIInterfaceInfo* aInterfaceInfo,
                                      nsIVariant* aProperty);

  static nsresult ParameterToVariant(nsIInterfaceInfo* aInterfaceInfo,
                                     PRUint32 aMethodIndex,
                                     const nsXPTParamInfo* aParamInfo,
                                     nsXPTCMiniVariant aMiniVariant,
                                     PRUint32 aArrayLength,
                                     nsIVariant** aVariant);
  static nsresult VariantToInParameter(nsIInterfaceInfo* aInterfaceInfo,
                                       PRUint32 aMethodIndex,
                                       const nsXPTParamInfo* aParamInfo,
                                       nsIVariant* aVariant,
                                       nsXPTCVariant* aXPTCVariant);
  static nsresult VariantToOutParameter(nsIInterfaceInfo* aInterfaceInfo,
                                        PRUint32 aMethodIndex,
                                        const nsXPTParamInfo* aParamInfo,
                                        nsIVariant* aVariant,
                                        nsXPTCMiniVariant* aMiniVariant);
  static nsresult WrapInPropertyBag(nsISupports* aInstance,
                                    nsIInterfaceInfo* aInterfaceInfo,
                                    nsIPropertyBag** aPropertyBag);
  static nsresult WrapInComplexType(nsIPropertyBag* aPropertyBag,
                                    nsIInterfaceInfo* aInterfaceInfo,
                                    nsISupports** aComplexType);


protected:
  nsresult GetInterfaceName(PRBool listener, char** retval);

protected:
  nsCOMPtr<nsIWSDLPort> mPort;
  nsCOMPtr<nsIInterfaceInfo> mPrimaryInterface;
  nsCOMPtr<nsIInterfaceInfoManager> mInterfaceInfoManager;
  nsString mQualifier;
  PRBool mIsAsync;
  nsCOMArray<nsIWebServiceCallContext> mPendingCalls;
  const nsIID* mIID;
  nsCOMPtr<nsISupports> mAsyncListener;
  nsCOMPtr<nsIInterfaceInfo> mListenerInterfaceInfo;
  nsCOMPtr<nsIScriptableInterfaces> mInterfaces;
};

class WSPCallContext : public nsIWebServiceSOAPCallContext,
                       public nsISOAPResponseListener
{
public:
  WSPCallContext(WSPProxy* aProxy,
                 nsISOAPCall* aSOAPCall,
                 const nsAString& aMethodName,
                 nsIWSDLOperation* aOperation);
  virtual ~WSPCallContext();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICECALLCONTEXT
  NS_DECL_NSIWEBSERVICESOAPCALLCONTEXT
  NS_DECL_NSISOAPRESPONSELISTENER
  
  nsresult CallAsync(PRUint32 aListenerMethodIndex,
                     nsISupports* aListener);
  nsresult CallSync(PRUint32 aMethodIndex,
                    nsXPTCMiniVariant* params);

protected:
  nsresult CallCompletionListener();

protected:
  WSPProxy* mProxy;
  nsCOMPtr<nsISOAPCall> mCall;
  nsString mMethodName;
  nsCOMPtr<nsIWSDLOperation> mOperation;
  nsCOMPtr<nsISOAPCallCompletion> mCompletion;
  nsCOMPtr<nsISOAPResponse> mResponse;
  nsresult mStatus;
  nsCOMPtr<nsIException> mException;
  nsCOMPtr<nsISupports> mAsyncListener;
  PRUint32 mListenerMethodIndex;
};

class WSPException : public nsIException 
{
public:
  WSPException(nsISOAPFault* aFault, nsresult aStatus);
  WSPException(nsresult aStatus, const char* aMsg, nsISupports* aData);
  virtual ~WSPException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTION

protected:
  nsCOMPtr<nsISOAPFault> mFault;
  nsCOMPtr<nsISupports> mData;
  nsresult mStatus;
  char* mMsg;
};

class WSPComplexTypeWrapper : public nsIWebServiceComplexTypeWrapper,
                              public nsIPropertyBag
{
public:
  WSPComplexTypeWrapper();
  virtual ~WSPComplexTypeWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYBAG
  NS_DECL_NSIWEBSERVICECOMPLEXTYPEWRAPPER

  static NS_METHOD
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

  nsresult GetPropertyValue(PRUint32 aMethodIndex,
                            const nsXPTMethodInfo* aMethodInfo,
                            nsIVariant** _retval);

protected:
  nsCOMPtr<nsISupports> mComplexTypeInstance;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
};

class WSPPropertyBagWrapper : public nsXPTCStubBase,
                              public nsIWebServicePropertyBagWrapper,
                              public nsIClassInfo
{
public:
  WSPPropertyBagWrapper();
  virtual ~WSPPropertyBagWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICEPROPERTYBAGWRAPPER
  NS_DECL_NSICLASSINFO

  // Would be nice to have a NS_DECL_NSXPTCSTUBBASE
  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);
  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

  static NS_METHOD
  Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

protected:
  nsCOMPtr<nsIPropertyBag> mPropertyBag;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  const nsIID* mIID;
};

class nsWSPInterfaceInfoService : public nsIWSPInterfaceInfoService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSPINTERFACEINFOSERVICE

  nsWSPInterfaceInfoService();
  virtual ~nsWSPInterfaceInfoService();
};

#define NS_WEBSERVICECALLCONTEXT_CLASSID          \
{ /* a42e9bf3-6bae-4c59-8f76-9dc175eec5b1 */      \
 0xa42e9bf3, 0x6bae, 0x4c59,                      \
 {0x8f, 0x76, 0x9d, 0xc1, 0x75, 0xee, 0xc5, 0xb1}}
#define NS_WEBSERVICECALLCONTEXT_CONTRACTID "@mozilla.org/xmlextras/proxy/webservicecallcontext;1"
#define NS_WEBSERVICEEXCEPTION_CLASSID            \
{ /* 07c2bf7b-376e-4629-b9c0-dbb17630b98d */      \
 0x07c2bf7b, 0x376e, 0x4629,                      \
 {0xb9, 0xc0, 0xdb, 0xb1, 0x76, 0x30, 0xb9, 0x8d}}
#define NS_WEBSERVICEEXCEPTION_CONTRACTID "@mozilla.org/xmlextras/proxy/webserviceexception;1"

#endif // __wspprivate_h__
