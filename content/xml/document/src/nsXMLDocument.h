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
#include "nsGenericDOMNodeList.h"

class nsIParser;
class nsIDOMNode;
class nsXMLDocument;

// Represents the children of an XML document (prolog, epilog and
// document element)
class nsXMLDocumentChildNodes : public nsGenericDOMNodeList
{
public:
  nsXMLDocumentChildNodes(nsXMLDocument* aDocument);
  ~nsXMLDocumentChildNodes();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);

  void DropReference();

protected:
  nsXMLDocument* mDocument;
};


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

  NS_IMETHOD StartDocumentLoad(nsIURL *aUrl, 
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener,
                               const char* aCommand);

  NS_IMETHOD EndLoad();

  // nsIDOMNode interface
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild,
                          nsIDOMNode* aRefChild, 
                          nsIDOMNode** aReturn);
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild,
                          nsIDOMNode* aOldChild, 
                          nsIDOMNode** aReturn);
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);
  NS_IMETHOD HasChildNodes(PRBool* aReturn);

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
  NS_IMETHOD PrologElementAt(PRUint32 aOffset, nsIContent** aContent);
  NS_IMETHOD PrologCount(PRUint32* aCount);
  NS_IMETHOD AppendToProlog(nsIContent* aContent);

  NS_IMETHOD EpilogElementAt(PRUint32 aOffset, nsIContent** aContent);
  NS_IMETHOD EpilogCount(PRUint32* aCount);
  NS_IMETHOD AppendToEpilog(nsIContent* aContent);

  NS_IMETHOD GetContentById(const nsString& aName, nsIContent** aContent);

  // nsIHTMLContentContainer
  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

protected:
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hook for sheet ordering
  virtual nsresult Reset(nsIURL* aUrl);


  // For HTML elements in our content model
  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mInlineStyleSheet;

  nsVoidArray *mProlog;
  nsVoidArray *mEpilog;

  nsIParser *mParser;
  nsXMLDocumentChildNodes* mChildNodes;
};


#endif // nsXMLDocument_h___
