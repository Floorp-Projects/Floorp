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

// interface info includes
#include "xptcall.h"
#include "nsIInterfaceInfo.h"

// XPCOM includes
#include "nsCOMPtr.h"
#include "nsSupportsArray.h"
#include "nsIPropertyBag.h"

class nsISOAPCall;
class nsISOAPResponse;
class nsISOAPParameter;

class WSPFactory : public nsIWebServiceProxyFactory 
{
public:
  WSPFactory();
  virtual ~WSPFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICEPROXYFACTORY  

  static nsresult MethodToPropertyName(const nsAReadableCString& aMethodName,
                                       nsAWritableString& aPropertyName);
  static void PropertyToMethodName(const nsAReadableString& aPropertyName,
                                   nsAWritableCString& aMethodName);
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
    
  static nsresult Create(nsIWSDLPort* aPort,
                         nsIInterfaceInfo* aPrimaryInterface,
                         const nsAReadableString& aNamespace,
                         PRBool aIsAsync, WSPProxy** aProxy);

protected:
  nsCOMPtr<nsIWSDLPort> mPort;
  nsCOMPtr<nsIInterfaceInfo> mPrimaryInterface;
  nsString mNamespace;
  PRBool mIsAsync;
  nsSupportsArray mPendingCalls;
};

class WSPCallContext : public nsIWebServiceCallContext
//                       public nsISOAPResponseListener
{
public:
  WSPCallContext();
  virtual ~WSPCallContext();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSERVICECALLCONTEXT

  static nsresult Create(WSPProxy* aProxy,
                         nsISOAPCall* aSOAPCall,
                         const nsAReadableString& aMethodName,
                         nsIWSDLOperation* aOperation,
                         WSPCallContext** aCallContext);

protected:
  nsCOMPtr<WSPProxy> mProxy;
  //  nsCOMPtr<nsISOAPCall> mSOAPCall;
  nsString mMethodName;
  nsCOMPtr<nsIWSDLOperation> mOperation;
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
  nsresult ResultAsVariant(uint8 aTypeTag,
                           nsXPTCVariant aResult,
                           nsIInterfaceInfo* aInterfaceInfo,
                           nsIVariant** aVariant);
  nsresult ArrayResultAsVariant(uint8 aTypeTag,
                                nsXPTCVariant aResult,
                                PRUint32 aLength,
                                nsIInterfaceInfo* aInterfaceInfo,
                                nsIVariant** aVariant);
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
  nsresult VariantToResult(uint8 aTypeTag,
                           void* aResult,
                           nsIInterfaceInfo* aInterfaceInfo,
                           nsIVariant* aProperty);
  nsresult VariantToArrayResult(uint8 aTypeTag,
                                nsXPTCMiniVariant* aResult,
                                nsIInterfaceInfo* aInterfaceInfo,
                                nsIVariant* aProperty);

protected:
  nsCOMPtr<nsIPropertyBag> mPropertyBag;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  const nsIID* mIID;
};

#endif // __wspprivate_h__
