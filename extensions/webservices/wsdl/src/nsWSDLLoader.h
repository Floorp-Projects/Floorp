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

#ifndef __nsWSDLLoader_h__
#define __nsWSDLLoader_h__

#include "nsIWebServiceErrorHandler.h"

#include "nsIWSDLLoader.h"
#include "nsWSDLPrivate.h"

#include "nsDOMUtils.h"

// XPCOM Includes
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsHashtable.h"
#include "nsError.h"

// Loading includes
#include "nsIURI.h"
#include "nsIXMLHttpRequest.h"
#include "nsIDOMEventListener.h"

// schema includes
#include "nsISchemaLoader.h"

// DOM includes
#include "nsIDOMDocument.h"

#define NS_ERROR_WSDL_LOADPENDING NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 1)

class nsWSDLAtoms 
{
public:
  static nsresult AddRefAtoms();

#define WSDL_ATOM(_name, _value) static nsIAtom* _name;
#include "nsWSDLAtomList.h"
#undef WSDL_ATOM
};

class nsWSDLLoader : public nsIWSDLLoader
{
public:
  nsWSDLLoader();
  virtual ~nsWSDLLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWSDLLOADER

  nsresult Init();

protected:
  nsresult doLoad(const nsAString& wsdlURI, const nsAString& portName,
                  nsIWSDLLoadListener *aListener, nsIWSDLPort **_retval);

  nsresult GetResolvedURI(const nsAString& aSchemaURI,
                          const char* aMethod,
                          nsIURI** aURI);

  nsWSDLPort* mPort;
};


class nsWSDLLoadingContext
{
public:
  nsWSDLLoadingContext(nsIDOMDocument* aDocument,
                       const nsAString& aLocation)
    : mDocument(aDocument), mChildIndex(0), mDocumentLocation(aLocation)
  {
  }
  ~nsWSDLLoadingContext()
  {
  }

  void GetRootElement(nsIDOMElement** aElement)
  {
    mDocument->GetDocumentElement(aElement);
  }

  PRUint32 GetChildIndex()
  {
    return mChildIndex;
  }
  void SetChildIndex(PRUint32 aChildIndex)
  {
    mChildIndex = aChildIndex;
  }

  void GetTargetNamespace(nsAString& aNamespace)
  {
    nsCOMPtr<nsIDOMElement> element;
    GetRootElement(getter_AddRefs(element));
    if (element) {
      element->GetAttribute(NS_LITERAL_STRING("targetNamespace"), aNamespace);
    }
    else {
      aNamespace.Truncate();
    }
  }

  void GetDocumentLocation(nsAString& aLocation)
  {
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
  nsWSDLLoadRequest(PRBool aIsSync, nsIWSDLLoadListener* aListener,
                    const nsAString& aPortName);
  virtual ~nsWSDLLoadRequest();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsresult LoadDefinition(const nsAString& aURI);
  nsresult ResumeProcessing();
  nsresult ContineProcessingTillDone();
  nsresult GetPort(nsIWSDLPort** aPort);

  nsresult PushContext(nsIDOMDocument* aDocument, const nsAString& aLocation);
  nsWSDLLoadingContext* GetCurrentContext();
  void PopContext();

  nsresult GetSchemaElement(const nsAString& aName,
                            const nsAString& aNamespace,
                            nsISchemaElement** aSchemaComponent);
  nsresult GetSchemaType(const nsAString& aName, const nsAString& aNamespace,
                         nsISchemaType** aSchemaComponent);
  nsresult GetMessage(const nsAString& aName, const nsAString& aNamespace,
                      nsIWSDLMessage** aMessage);
  nsresult GetPortType(const nsAString& aName, const nsAString& aNamespace,
                       nsIWSDLPort** aPort);

  nsresult ProcessImportElement(nsIDOMElement* aElement, PRUint32 aIndex);
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
  nsCOMPtr<nsIWSDLPort> mPort;
  nsCOMArray<nsIURI> mImportList;
  nsCOMPtr<nsIWebServiceErrorHandler> mErrorHandler;
  
  PRPackedBool mIsSync;

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
