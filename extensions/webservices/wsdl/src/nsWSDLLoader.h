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

#ifndef __nsWSDLLoader_h__
#define __nsWSDLLoader_h__

#include "nsIWSDLLoader.h"
#include "nsWSDLPrivate.h"

#include "nsDOMUtils.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsSupportsArray.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsHashtable.h"
#include "nsError.h"

// Loading includes
#include "nsIURI.h"
#include "nsIXMLHTTPRequest.h"
#include "nsIDOMEventListener.h"

// schema includes
#include "nsISchemaLoader.h"

// DOM includes
#include "nsIDOMDocument.h"

#define NS_ERROR_WSDL_LOADPENDING NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 1)

class nsWSDLAtoms {
public:
  static void CreateWSDLAtoms();
  static void DestroyWSDLAtoms();

  static nsIAtom* sDefinitions_atom;
  static nsIAtom* sTypes_atom;
  static nsIAtom* sMessage_atom;
  static nsIAtom* sPortType_atom;
  static nsIAtom* sBinding_atom;
  static nsIAtom* sService_atom;
  static nsIAtom* sPort_atom;
  static nsIAtom* sOperation_atom;
  static nsIAtom* sPart_atom;
  static nsIAtom* sDocumentation_atom;
  static nsIAtom* sImport_atom;
  static nsIAtom* sInput_atom;
  static nsIAtom* sOutput_atom;
  static nsIAtom* sFault_atom;

  static nsIAtom* sBody_atom;
  static nsIAtom* sHeader_atom;
  static nsIAtom* sHeaderFault_atom;
  static nsIAtom* sAddress_atom;
};

class nsWSDLLoader : public nsIWSDLLoader {
public:
  nsWSDLLoader();
  virtual ~nsWSDLLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLLOADER

protected:
  nsresult GetResolvedURI(const nsAReadableString& aSchemaURI,
                          const char* aMethod,
                          nsIURI** aURI);

protected:
  nsWSDLPort* mPort;
};


class nsWSDLLoadingContext {
public:
  nsWSDLLoadingContext(nsIDOMDocument* aDocument,
                       const nsAReadableString& aLocation) :
    mDocument(aDocument), mChildIndex(0), mDocumentLocation(aLocation) {
  }
  ~nsWSDLLoadingContext() {
  }
          
  void GetRootElement(nsIDOMElement** aElement) {
    mDocument->GetDocumentElement(aElement);
  }

  PRUint32 GetChildIndex() { return mChildIndex; }
  void SetChildIndex(PRUint32 aChildIndex) { mChildIndex = aChildIndex; }

  void GetTargetNamespace(nsAWritableString& aNamespace) {
    nsCOMPtr<nsIDOMElement> element;
    GetRootElement(getter_AddRefs(element));
    if (element) {
      element->GetAttribute(NS_LITERAL_STRING("targetNamespace"),
                            aNamespace);
    }
    else {
      aNamespace.Truncate();
    }
  }

  void GetDocumentLocation(nsAWritableString& aLocation) {
    aLocation.Assign(mDocumentLocation);
  }

protected:
  // XXX hold onto the document till issues related to dangling
  // document pointers in content are fixed. After that, just
  // hold onto the root element.
  nsCOMPtr<nsIDOMDocument> mDocument; 
  PRUint32 mChildIndex;
  nsString mDocumentLocation;
};

class nsWSDLLoadRequest : public nsIDOMEventListener
{
public:
  nsWSDLLoadRequest(PRBool aIsSync, 
                    nsIWSDLLoadListener* aListener,
                    const nsAReadableString& aPortName);
  virtual ~nsWSDLLoadRequest();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult LoadDefinition(const nsAReadableString& aURI);
  nsresult ResumeProcessing();
  nsresult ContineProcessingTillDone();
  nsresult GetPort(nsIWSDLPort** aPort);

  nsresult PushContext(nsIDOMDocument* aDocument, 
                       const nsAReadableString& aLocation);
  nsWSDLLoadingContext* GetCurrentContext();
  void PopContext();

  nsresult GetSchemaElement(const nsAReadableString& aName,
                            const nsAReadableString& aNamespace,
                            nsISchemaElement** aSchemaComponent);
  nsresult GetSchemaType(const nsAReadableString& aName,
                         const nsAReadableString& aNamespace,
                         nsISchemaType** aSchemaComponent);
  nsresult GetMessage(const nsAReadableString& aName,
                      const nsAReadableString& aNamespace,
                      nsIWSDLMessage** aMessage);
  nsresult GetPortType(const nsAReadableString& aName,
                       const nsAReadableString& aNamespace,
                       nsIWSDLPort** aPort);

  nsresult ProcessImportElement(nsIDOMElement* aElement, 
                                PRUint32 aIndex);
  nsresult ProcessTypesElement(nsIDOMElement* aElement);
  nsresult ProcessMessageElement(nsIDOMElement* aElement);
  nsresult ProcessAbstractPartElement(nsIDOMElement* aElement,
                                      nsWSDLMessage* aMessage);
  nsresult ProcessPortTypeElement(nsIDOMElement* aElement);
  nsresult ProcessAbstractOperation(nsIDOMElement* aElement,
                                    nsWSDLPort* aPort);
  nsresult ProcessOperationComponent(nsIDOMElement* aElement,
                                     nsIWSDLMessage** aMessage);
  nsresult ProcessMessageBinding(nsIDOMElement* aElement, 
                                 nsIWSDLMessage* aMessage);
  nsresult ProcessOperationBinding(nsIDOMElement* aElement,
                                   nsIWSDLPort* aPort);
  nsresult ProcessBindingElement(nsIDOMElement* aElement);
  nsresult ProcessPortBinding(nsIDOMElement* aElement);
  nsresult ProcessServiceElement(nsIDOMElement* aElement);
  
protected:
  nsCOMPtr<nsIWSDLLoadListener> mListener;
  nsCOMPtr<nsIXMLHttpRequest> mRequest;
  nsCOMPtr<nsISchemaLoader> mSchemaLoader;

  PRBool mIsSync;

  nsCOMPtr<nsIWSDLPort> mPort;
  nsString mPortName;
  nsString mBindingName;
  nsString mBindingNamespace;
  nsString mAddress;

  nsVoidArray mContextStack;

  nsSupportsHashtable mTypes;
  nsSupportsHashtable mMessages;
  nsSupportsHashtable mPortTypes;
};

#endif // __nsWSDLLoader_h__
