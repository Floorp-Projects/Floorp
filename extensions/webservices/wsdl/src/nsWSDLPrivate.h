/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#ifndef __nsWSDLPrivate_h__
#define __nsWSDLPrivate_h__

#include "nsIWSDL.h"
#include "nsIWSDLSOAPBinding.h"

// DOM Includes
#include "nsIDOMElement.h"

// Schema includes
#include "nsISchema.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"
#include "nsString.h"
#include "nsWeakReference.h"

// Typelib includes
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xpt_struct.h"
#include "xptcall.h"


#define NS_WSDL_SCHEMA_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define NS_WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
#define NS_WSDL_SOAP_NAMESPACE "http://schemas.xmlsoap.org/wsdl/soap/"

class nsSOAPPortBinding : public nsISOAPPortBinding {
public:
  nsSOAPPortBinding(const nsAReadableString& aName);
  virtual ~nsSOAPPortBinding();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPPORTBINDING

  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetAddress(const nsAReadableString& aAddress);
  NS_IMETHOD SetStyle(PRUint16 aStyle);
  NS_IMETHOD SetTransport(const nsAReadableString& aTransport);

protected:
  nsString mName;
  nsString mAddress;  
  PRUint16 mStyle;
  nsString mTransport;  
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
};

class nsWSDLPort : public nsIWSDLPort {
public:
  nsWSDLPort(const nsAReadableString &aName);
  virtual ~nsWSDLPort();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPORT

  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD AddOperation(nsIWSDLOperation* aOperation);
  NS_IMETHOD SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsSupportsArray mOperations;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

class nsSOAPOperationBinding : public nsISOAPOperationBinding {
public:
  nsSOAPOperationBinding();
  virtual ~nsSOAPOperationBinding();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPOPERATIONBINDING

  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetStyle(PRUint16 aStyle);
  NS_IMETHOD SetSoapAction(const nsAReadableString& aAction);

protected:
  PRUint16 mStyle;
  nsString mSoapAction;  
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
};

class nsWSDLOperation : public nsIWSDLOperation {
public:
  nsWSDLOperation(const nsAReadableString &aName);
  virtual ~nsWSDLOperation();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLOPERATION

  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetInput(nsIWSDLMessage* aInputMessage);
  NS_IMETHOD SetOutput(nsIWSDLMessage* aOutputMessage);
  NS_IMETHOD AddFault(nsIWSDLMessage* aFaultMessage);
  NS_IMETHOD AddParameter(const nsAReadableString& aParameter);
  NS_IMETHOD SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsCOMPtr<nsIWSDLMessage> mInputMessage;
  nsCOMPtr<nsIWSDLMessage> mOutputMessage;
  nsSupportsArray mFaultMessages;
  nsStringArray mParameters;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

class nsWSDLMessage : public nsIWSDLMessage {
public:
  nsWSDLMessage(const nsAReadableString& aName);
  virtual ~nsWSDLMessage();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLMESSAGE

  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD AddPart(nsIWSDLPart* aPart);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsSupportsArray mParts;
};

class nsSOAPPartBinding : public nsISOAPPartBinding {
public:
  nsSOAPPartBinding(PRUint16 aLocation, PRUint16 aUse,
                    const nsAReadableString& aEncodingStyle,
                    const nsAReadableString& aNamespace);
  virtual ~nsSOAPPartBinding();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPPARTBINDING

protected:
  PRUint16 mLocation;
  PRUint16 mUse;
  nsString mEncodingStyle;
  nsString mNamespace;  
};

class nsWSDLPart : public nsIWSDLPart {
public:
  nsWSDLPart(const nsAReadableString& aName);
  virtual ~nsWSDLPart();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPART

  NS_IMETHOD SetTypeInfo(const nsAReadableString& aType,
                         const nsAReadableString& aElementName,
                         nsISchemaComponent* aSchemaComponent);
  NS_IMETHOD SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsString mType;
  nsString mElementName;
  nsCOMPtr<nsISchemaComponent> mSchemaComponent;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

#define NS_WSDLINTERFACEINFOID_ISUPPORTS 0
// The boundary number should be incremented as reserved
// ids are added.
#define NS_WSDLINTERFACEINFOID_RESERVED_BOUNDARY 1

class nsWSDLInterfaceSet : public nsIInterfaceInfoManager,
                           public nsSupportsWeakReference
{
public:
  nsWSDLInterfaceSet();
  virtual ~nsWSDLInterfaceSet();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEINFOMANAGER

  nsresult AppendInterface(nsIInterfaceInfo* aInfo, PRUint16* aIndex);
  nsresult GetInterfaceAt(PRUint16 aIndex, nsIInterfaceInfo** aInfo);
  PRUint16 Count();
  XPTArena* GetArena() { return mArena; }

private:
  nsSupportsArray mInterfaces;
  XPTArena* mArena;
};

class nsWSDLInterfaceInfo : public nsIInterfaceInfo,
                            public nsSupportsWeakReference
{
public:
  nsWSDLInterfaceInfo(nsAReadableCString& aName, 
                      const nsIID& aIID,
                      nsWSDLInterfaceSet* aInterfaceSet);
  virtual ~nsWSDLInterfaceInfo();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEINFO

  void AddMethod(XPTMethodDescriptor* aMethod);
  void AddAdditionalType(XPTTypeDescriptor* aType);

private:
  nsresult GetTypeInArray(const nsXPTParamInfo* param,
                          uint16 dimension,
                          const XPTTypeDescriptor** type);

private:
  nsCString mName;
  nsIID mIID;
  nsVoidArray mMethodDescriptors;
  nsVoidArray mAdditionalTypes;
  nsWSDLInterfaceSet* mInterfaceSet;
};

#if 0
class nsWSDLServiceProxy : public nsXPTCStubBase,
                           public nsIWSDLServiceProxy,
                           public nsIClassInfo
{
public:
  nsWSDLServiceProxy();
  virtual ~nsWSDLServiceProxy();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLSERVICEPROXY
  NS_DECL_NSICLASSINFO

  NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info); 
  NS_IMETHOD CallMethod(PRUint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);

private:
  nsWSDLInterfaceSet* mInterfaceSet;
  nsCOMPtr<nsIInterfaceInfo> mInterfaceInfo;
  nsCOMPtr<nsIWSDLPort> mPort;
};
#endif

#define NS_WSDLPORT_CID                           \
{ /* c1dfb250-0c19-4339-8211-24eabc0103e5 */      \
 0xc1dfb250, 0x0c19, 0x4339,                      \
 {0x82, 0x11, 0x24, 0xea, 0xbc, 0x01, 0x03, 0xe5}}

#define NS_WSDLPORT_CONTRACTID   \
"@mozilla/xmlextras/wsdl/wsdlport;1"

#define NS_WSDLOPERATION_CID                      \
{ /* cf54bdf5-20de-45ef-b6c8-aa535007549a */      \
 0xcf54bdf5, 0x20de, 0x45ef,                      \
 {0xb6, 0xc8, 0xaa, 0x53, 0x50, 0x07, 0x54, 0x9a}}

#define NS_WSDLOPERATION_CONTRACTID   \
"@mozilla/xmlextras/wsdl/wsdloperation;1"

#define NS_WSDLMESSAGE_CID                        \
{ /* 36b26cab-3eed-4c7c-81ad-94c8b1eb9ebe */      \
 0x36b26cab, 0x3eed, 0x4c7c,                      \
 {0x81, 0xad, 0x94, 0xc8, 0xb1, 0xeb, 0x9e, 0xbe}}

#define NS_WSDLMESSAGE_CONTRACTID   \
"@mozilla/xmlextras/wsdl/wsdlmessage;1"

#define NS_WSDLPART_CID                           \
{ /* 1841ebe8-5bdc-4e79-bcf4-329785318491 */      \
 0x1841ebe8, 0x5bdc, 0x4e79,                      \
 {0xbc, 0xf4, 0x32, 0x97, 0x85, 0x31, 0x84, 0x91}}

#define NS_WSDLPART_CONTRACTID   \
"@mozilla/xmlextras/wsdl/wsdlpart;1"

#define NS_SOAPPORTBINDING_CID                    \
{ /* a9155950-e49d-4123-93b7-e263a3af2b32 */      \
 0xa9155950, 0xe49d, 0x4123,                      \
 {0x93, 0xb7, 0xe2, 0x63, 0xa3, 0xaf, 0x2b, 0x32}}

#define NS_SOAPPORTBINDING_CONTRACTID   \
"@mozilla/xmlextras/wsdl/soapportbinding;1"

#define NS_SOAPOPERATIONBINDING_CID               \
{ /* f5230937-4af6-43fb-9766-1890896632b2 */      \
 0xf5230937, 0x4af6, 0x43fb,                      \
 {0x97, 0x66, 0x18, 0x90, 0x89, 0x66, 0x32, 0xb2}}

#define NS_SOAPOPERATIONBINDING_CONTRACTID   \
"@mozilla/xmlextras/wsdl/soapoperationbinding;1"

#define NS_SOAPPARTBINDING_CID                    \
{ /* b7698d5c-06cc-45fe-b6bc-88e32a9f970e */      \
 0xb7698d5c, 0x06cc, 0x45fe,                      \
 {0xb6, 0xbc, 0x88, 0xe3, 0x2a, 0x9f, 0x97, 0x0e}}

#define NS_SOAPPARTBINDING_CONTRACTID   \
"@mozilla/xmlextras/wsdl/soappartbinding;1"

#endif // __nsWSDLPrivate_h__
