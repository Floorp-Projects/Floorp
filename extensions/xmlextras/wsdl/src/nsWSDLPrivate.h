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
#include "nsIWSDLLoader.h"

// DOM Includes
#include "nsIDOMElement.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsSupportsArray.h"
#include "nsString.h"

class nsWSDLLoader : public nsIWSDLLoader {
public:
  nsWSDLLoader();
  virtual ~nsWSDLLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLLOADER
};

class nsWSDLService : public nsIWSDLService {
public:
  nsWSDLService();
  virtual ~nsWSDLService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLSERVICE

  NS_IMETHOD SetName(const nsAReadableString& aName);
  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD AddPort(nsIWSDLPort* aPort);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsSupportsArray mPorts;
};

class nsWSDLPort : public nsIWSDLPort {
public:
  nsWSDLPort();
  virtual ~nsWSDLPort();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPORT

  NS_IMETHOD SetName(const nsAReadableString& aName);
  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetBindingInfo(const nsAReadableString& aBindingName,
                            PRUint16 aStyle,
                            const nsAReadableString& aTransport);
  NS_IMETHOD AddOperation(nsIWSDLOperation* aOperation);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  nsString mBindingName;
  PRUint16 mStyle;
  nsString mTransport;
  nsSupportsArray mOperations;
};

class nsWSDLOperation : public nsIWSDLOperation {
public:
  nsWSDLOperation();
  virtual ~nsWSDLOperation();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLOPERATION

  NS_IMETHOD SetName(const nsAReadableString& aName);
  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetBindingInfo(PRUint16 aStyle,
                            const nsAReadableString& aSoapAction);
  NS_IMETHOD SetInputMessage(nsIWSDLMessage* aInputMessage);
  NS_IMETHOD SetOutputMessage(nsIWSDLMessage* aOutputMessage);
  NS_IMETHOD SetFaultMessage(nsIWSDLMessage* aFaultMessage);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  PRUint16 mStyle;
  nsString mSoapAction;
  nsCOMPtr<nsIWSDLMessage> mInputMessage;
  nsCOMPtr<nsIWSDLMessage> mOutputMessage;
  nsCOMPtr<nsIWSDLMessage> mFaultMessage;
};

class nsWSDLMessage : public nsIWSDLMessage {
public:
  nsWSDLMessage();
  virtual ~nsWSDLMessage();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLMESSAGE

  NS_IMETHOD SetName(const nsAReadableString& aName);
  NS_IMETHOD SetDocumentationElement(nsIDOMElement* aElement);
  NS_IMETHOD SetBindingInfo(PRUint16 aLocation, PRUint16 aUse,
                            const nsAReadableString& aEncodingStyle,
                            const nsAReadableString& aNamespace);
  NS_IMETHOD AddPart(nsIWSDLPart* aPart);

protected:
  nsString mName;
  nsCOMPtr<nsIDOMElement> mDocumentationElement;
  PRUint16 mLocation;
  PRUint16 mUse;
  nsString mEncodingStyle;
  nsString mNamespace;
  nsSupportsArray mParts;
};

class nsWSDLPart : public nsIWSDLPart {
public:
  nsWSDLPart();
  virtual ~nsWSDLPart();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLPART

  NS_IMETHOD SetName(const nsAReadableString& aName);
  NS_IMETHOD SetTypeInfo(const nsAReadableString& aType,
                         const nsAReadableString& aElementName,
                         nsIDOMElement* aSchema,
                         nsIDOMElement* aSchemaRoot);

protected:
  nsString mName;
  nsString mType;
  nsString mElementName;
  nsCOMPtr<nsIDOMElement> mSchema;
  nsCOMPtr<nsIDOMElement> mSchemaRoot;
};

#endif // __nsWSDLPrivate_h__
