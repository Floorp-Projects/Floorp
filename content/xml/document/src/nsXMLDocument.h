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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsXMLDocument_h___
#define nsXMLDocument_h___

#include "nsMarkupDocument.h"
#include "nsIXMLDocument.h"
#include "nsIHTMLContentContainer.h"
#ifdef MOZ_XSL
#include "nsITransformMediator.h"
#endif

class nsIParser;
class nsIDOMNode;
class nsICSSLoader;


class nsXMLDocument : public nsMarkupDocument,
                      public nsIXMLDocument,
                      public nsIHTMLContentContainer
{
public:
  nsXMLDocument();
  virtual ~nsXMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD GetContentType(nsAWritableString& aContentType) const;

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE);

  NS_IMETHOD EndLoad();

  // nsIDOMNode interface
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // nsIDOMDocument interface
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDocumentType);
  NS_IMETHOD    CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn);
  NS_IMETHOD    CreateEntityReference(const nsAReadableString& aName, nsIDOMEntityReference** aReturn);
  NS_IMETHOD    CreateProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData, nsIDOMProcessingInstruction** aReturn);
  NS_IMETHOD    CreateElement(const nsAReadableString& aTagName, 
                              nsIDOMElement** aReturn);
  NS_IMETHOD    ImportNode(nsIDOMNode* aImportedNode,
                           PRBool aDeep,
                           nsIDOMNode** aReturn);
  NS_IMETHOD    CreateElementNS(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aQualifiedName,
                               nsIDOMElement** aReturn);
  NS_IMETHOD    CreateAttributeNS(const nsAReadableString& aNamespaceURI,
                                  const nsAReadableString& aQualifiedName,
                                  nsIDOMAttr** aReturn);
  NS_IMETHOD    GetElementById(const nsAReadableString& aElementId,
                               nsIDOMElement** aReturn);
  NS_IMETHOD    Load(const nsAReadableString& aUrl);

  // nsIXMLDocument interface
#ifdef MOZ_XSL
  NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);
#endif

  // nsIHTMLContentContainer
  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

  virtual nsresult Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
protected:
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hook for sheet ordering
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);

  // For HTML elements in our content model
  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mInlineStyleSheet;

  nsIParser *mParser;
  nsICSSLoader* mCSSLoader;
#ifdef MOZ_XSL
  nsITransformMediator* mTransformMediator;
#endif
};


#endif // nsXMLDocument_h___
