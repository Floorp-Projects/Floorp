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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsWeakReference.h"

// Typelib includes
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xpt_struct.h"
#include "xptcall.h"

#define NS_WSDL_SCHEMA_NAMESPACE "http://www.w3.org/2001/XMLSchema"
#define NS_WSDL_NAMESPACE "http://schemas.xmlsoap.org/wsdl/"
#define NS_WSDL_SOAP_NAMESPACE "http://schemas.xmlsoap.org/wsdl/soap/"

/**
 * Fire error on error handler passed as argument, only to be used
 * in ProcessXXX or Resolve methods.
 */
#define NS_WSDLLOADER_FIRE_ERROR(status,statusMessage)     \
  PR_BEGIN_MACRO                                           \
  if (mErrorHandler) {                                     \
    mErrorHandler->OnError(status, statusMessage);         \
  }                                                        \
  PR_END_MACRO

class nsSOAPPortBinding : public nsISOAPPortBinding
{
public:
  nsSOAPPortBinding(const nsAString& aName);
  virtual ~nsSOAPPortBinding();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPPORTBINDING

  nsresult SetDocumentationElement(nsIDOMElement* aElement);
  nsresult SetAddress(const nsAString& aAddress);
  nsresult SetStyle(PRUint16 aStyle);
  nsresult SetTransport(const nsAString& aTransport);
  nsresult SetSoapVersion(PRUint16 aSoapVersion);

protected:
  nsString mName;
  nsString mAddress;
  nsString mTransport;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  PRUint16 mSoapVersion;
  PRUint16 mStyle;
};

class nsWSDLPort : public nsIWSDLPort
{
public:
  nsWSDLPort(const nsAString &aName);
  virtual ~nsWSDLPort();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPORT

  nsresult SetDocumentationElement(nsIDOMElement* aElement);
  nsresult AddOperation(nsIWSDLOperation* aOperation);
  nsresult SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsCOMArray<nsIWSDLOperation> mOperations;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

class nsSOAPOperationBinding : public nsISOAPOperationBinding
{
public:
  nsSOAPOperationBinding();
  virtual ~nsSOAPOperationBinding();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPOPERATIONBINDING

  nsresult SetDocumentationElement(nsIDOMElement* aElement);
  nsresult SetStyle(PRUint16 aStyle);
  nsresult SetSoapAction(const nsAString& aAction);

protected:
  nsString mSoapAction;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  PRUint16 mStyle;
};

class nsWSDLOperation : public nsIWSDLOperation
{
public:
  nsWSDLOperation(const nsAString &aName);
  virtual ~nsWSDLOperation();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLOPERATION

  nsresult SetDocumentationElement(nsIDOMElement* aElement);
  nsresult SetInput(nsIWSDLMessage* aInputMessage);
  nsresult SetOutput(nsIWSDLMessage* aOutputMessage);
  nsresult AddFault(nsIWSDLMessage* aFaultMessage);
  nsresult AddParameter(const nsAString& aParameter);
  nsresult SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsCOMPtr<nsIWSDLMessage> mInputMessage;
  nsCOMPtr<nsIWSDLMessage> mOutputMessage;
  nsCOMArray<nsIWSDLMessage> mFaultMessages;
  nsStringArray mParameters;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

class nsSOAPMessageBinding : public nsISOAPMessageBinding
{
public:
  nsSOAPMessageBinding(const nsAString& aNamespace);
  virtual ~nsSOAPMessageBinding();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLBINDING
  NS_DECL_NSISOAPMESSAGEBINDING

protected:
  nsString mNamespace;
};

class nsWSDLMessage : public nsIWSDLMessage
{
public:
  nsWSDLMessage(const nsAString& aName);
  virtual ~nsWSDLMessage();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLMESSAGE

  nsresult SetDocumentationElement(nsIDOMElement* aElement);
  nsresult AddPart(nsIWSDLPart* aPart);
  nsresult SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsCOMArray<nsIWSDLPart> mParts;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

class nsSOAPPartBinding : public nsISOAPPartBinding
{
public:
  nsSOAPPartBinding(PRUint16 aLocation, PRUint16 aUse,
                    const nsAString& aEncodingStyle,
                    const nsAString& aNamespace);
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

class nsWSDLPart : public nsIWSDLPart
{
public:
  nsWSDLPart(const nsAString& aName);
  virtual ~nsWSDLPart();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPART

  nsresult SetTypeInfo(const nsAString& aType, const nsAString& aElementName,
                       nsISchemaComponent* aSchemaComponent);
  nsresult SetBinding(nsIWSDLBinding* aBinding);

protected:
  nsString mName;
  nsString mType;
  nsString mElementName;
  nsCOMPtr<nsISchemaComponent> mSchemaComponent;
  nsCOMPtr<nsIWSDLBinding> mBinding;
};

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

#define NS_SOAPMESSAGEBINDING_CID               \
{ /* 0adc6e39-49cd-42a2-a862-698e7885fcbd */      \
 0x0adc6e39, 0x49cd, 0x42a2,                      \
 {0xa8, 0x62, 0x69, 0x8e, 0x78, 0x85, 0xfc, 0xbd}}

#define NS_SOAPMESSAGEBINDING_CONTRACTID   \
"@mozilla/xmlextras/wsdl/soapmessagebinding;1"

#define NS_SOAPPARTBINDING_CID                    \
{ /* b7698d5c-06cc-45fe-b6bc-88e32a9f970e */      \
 0xb7698d5c, 0x06cc, 0x45fe,                      \
 {0xb6, 0xbc, 0x88, 0xe3, 0x2a, 0x9f, 0x97, 0x0e}}

#define NS_SOAPPARTBINDING_CONTRACTID   \
"@mozilla/xmlextras/wsdl/soappartbinding;1"

#endif // __nsWSDLPrivate_h__
