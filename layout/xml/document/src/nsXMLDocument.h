/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsXMLDocument_h___
#define nsXMLDocument_h___

#include "nsMarkupDocument.h"
#include "nsIXMLDocument.h"
#include "nsIHTMLContentContainer.h"
#ifdef XSL
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

  NS_IMETHOD GetContentType(nsString& aContentType) const;

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                               nsIChannel* aChannel,
#else
                               nsIURI *aUrl, 
#endif
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener);

  NS_IMETHOD EndLoad();

  // nsIDOMDocument interface
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDocumentType);
  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);
  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);
  NS_IMETHOD    CreateElement(const nsString& aTagName, 
                              nsIDOMElement** aReturn);
  NS_IMETHOD    CreateElementWithNameSpace(const nsString& aTagName, 
                                           const nsString& aNameSpace, 
                                           nsIDOMElement** aReturn);

  // nsIXMLDocument interface
  NS_IMETHOD GetContentById(const nsString& aName, nsIContent** aContent);
#ifdef XSL
  NS_IMETHOD SetTransformMediator(nsITransformMediator* aMediator);
#endif

  // nsIHTMLContentContainer
  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

protected:
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hook for sheet ordering
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);
#ifdef NECKO
  virtual nsresult Reset(nsIChannel* aChannel);
#else
  virtual nsresult Reset(nsIURI* aUrl);
#endif

  // For HTML elements in our content model
  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mInlineStyleSheet;

  nsIParser *mParser;
  nsICSSLoader* mCSSLoader;
#ifdef XSL
  nsITransformMediator* mTransformMediator;
#endif
};


#endif // nsXMLDocument_h___
