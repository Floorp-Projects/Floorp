/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   John Bandhauer (jband@netscape.com)
 *   Vidur Apparao (vidur@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __wspprivate_h__
#define __wspprivate_h__

#include "nsIWebServiceProxy.h"
#include "nsIWSDL.h"

// SOAP includes
#include "nsISOAPCall.h"
#include "nsISOAPResponse.h"
#include "nsISOAPResponseListener.h"
#include "nsISOAPCallCompletion.h"
#include "nsISOAPFault.h"

// interface info includes
#include "xptcall.h"
#include "nsIInterfaceInfo.h"

// XPCOM includes
#include "nsCOMPtr.h"
#include "nsSupportsArray.h"
#include "nsIPropertyBag.h"
#include "nsIException.h"

class WSPFactory : public nsIWebServiceProxyFactory 
{
public:
  WSPFactory();
  virtual ~WSPFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICEPROXYFACTORY  

  static nsresult C2XML(const nsAReadableCString& aCIdentifier,
                        nsAWritableString& aXMLIdentifier);
  static void XML2C(const nsAReadableString& aXMLIndentifier,
                    nsAWritableCString& aCIdentifier);
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

  nsresult Init(nsIWSDLPort* aPort,
                nsIInterfaceInfo* aPrimaryInterface,
                const nsAReadableString& aQualifier,
                PRBool aIsAsync);
  void GetListenerInterfaceInfo(nsIInterfaceInfo** aInfo);

  static nsresult Create(nsIWSDLPort* aPort,
                         nsIInterfaceInfo* aPrimaryInterface,
                         const nsAReadableString& aQualifier,
                         PRBool aIsAsync, WSPProxy** aProxy);

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
                                      nsXPTCMiniVariant* aResult,
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
  static PRBool IsArray(nsIWSDLPart* aPart);

protected:
  nsCOMPtr<nsIWSDLPort> mPort;
  nsCOMPtr<nsIInterfaceInfo> mPrimaryInterface;
  nsString mQualifier;
  PRBool mIsAsync;
  nsSupportsArray mPendingCalls;
  const nsIID* mIID;
  nsCOMPtr<nsISupports> mAsyncListener;
  nsCOMPtr<nsIInterfaceInfo> mListenerInterfaceInfo;
};

class WSPCallContext : public nsIWebServiceSOAPCallContext,
                       public nsISOAPResponseListener
{
public:
  WSPCallContext(WSPProxy* aProxy,
                 nsISOAPCall* aSOAPCall,
                 const nsAReadableString& aMethodName,
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
  nsresult mStatus;
  nsCOMPtr<nsIException> mException;
  nsCOMPtr<nsISupports> mAsyncListener;
  PRUint32 mListenerMethodIndex;
};

class WSPException : public nsIException 
{
public:
  WSPException(nsISOAPFault* aFault, nsresult aStatus);
  virtual ~WSPException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTION

protected:
  nsCOMPtr<nsISOAPFault> mFault;
  nsresult mStatus;
};

class WSPComplexTypeWrapper : public nsIPropertyBag
{
public:
  WSPComplexTypeWrapper(nsISupports* aComplexTypeInstance,
                        nsIInterfaceInfo* aInterfaceInfo);
  virtual ~WSPComplexTypeWrapper();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTYBAG

  static nsresult Create(nsISupports* aComplexTypeInstance,
                         nsIInterfaceInfo* aInterfaceInfo,
                         WSPComplexTypeWrapper** aWrapper);

  nsresult GetPropertyValue(PRUint32 aMethodIndex,
                            const nsXPTMethodInfo* aMethodInfo,
                            nsIVariant** _retval);

protected:
  nsCOMPtr<nsISupports> mComplexTypeInstance;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
};

class WSPPropertyBagWrapper : public nsXPTCStubBase
{
public:
  WSPPropertyBagWrapper(nsIPropertyBag* aPropertyBag,
                        nsIInterfaceInfo* aInterfaceInfo);
  virtual ~WSPPropertyBagWrapper();

  NS_DECL_ISUPPORTS

  // Would be nice to have a NS_DECL_NSXPTCSTUBBASE
  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);
  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info);

  static nsresult Create(nsIPropertyBag* aPropertyBag,
                         nsIInterfaceInfo* aInterfaceInfo,
                         WSPPropertyBagWrapper** aWrapper);
protected:
  nsCOMPtr<nsIPropertyBag> mPropertyBag;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  const nsIID* mIID;
};

#endif // __wspprivate_h__
