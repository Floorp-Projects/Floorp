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

class nsIParser;

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

  // nsIDOMDocument interface
  NS_IMETHOD    GetDoctype(nsIDOMDocumentType** aDocumentType);
  NS_IMETHOD    CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn);
  NS_IMETHOD    CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn);
  NS_IMETHOD    CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn);
  NS_IMETHOD    CreateElement(const nsString& aTagName, 
                              nsIDOMElement** aReturn);

  // nsIXMLDocument interface
  NS_IMETHOD PrologElementAt(PRInt32 aOffset, nsIContent** aContent);
  NS_IMETHOD PrologCount(PRInt32* aCount);
  NS_IMETHOD AppendToProlog(nsIContent* aContent);

  NS_IMETHOD EpilogElementAt(PRInt32 aOffset, nsIContent** aContent);
  NS_IMETHOD EpilogCount(PRInt32* aCount);
  NS_IMETHOD AppendToEpilog(nsIContent* aContent);

  // nsIHTMLContentContainer
  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);

protected:
  void AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet);
  virtual nsresult Reset(nsIURL* aUrl);


  // For HTML elements in our content model
  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mInlineStyleSheet;

  nsVoidArray *mProlog;
  nsVoidArray *mEpilog;

  nsIParser *mParser;
};


#endif // nsXMLDocument_h___
