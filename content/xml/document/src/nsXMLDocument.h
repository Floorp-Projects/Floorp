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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsXMLDocument_h___
#define nsXMLDocument_h___

#include "nsMarkupDocument.h"
#include "nsIXMLDocument.h"
#include "nsIHTMLContentContainer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIHttpEventSink.h"
#include "nsIDOMXMLDocument.h"

class nsIParser;
class nsIDOMNode;
class nsICSSLoader;
class nsIURI;

class nsXMLDocument : public nsMarkupDocument,
                      public nsIXMLDocument,
                      public nsIHTMLContentContainer,
                      public nsIInterfaceRequestor,
                      public nsIHttpEventSink,
                      public nsIDOMXMLDocument
{
public:
  nsXMLDocument();
  virtual ~nsXMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
                               nsIChannel* channel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               PRBool aReset = PR_TRUE);

  NS_IMETHOD EndLoad();

  NS_IMETHOD GetBaseTarget(nsAWritableString &aBaseTarget);
  NS_IMETHOD SetBaseTarget(const nsAReadableString &aBaseTarget);

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

  // nsIXMLDocument interface
  NS_IMETHOD SetDefaultStylesheets(nsIURI* aUrl);

  // nsIHTMLContentContainer
  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

  // nsIInterfaceRequestor
  NS_IMETHOD GetInterface(const nsIID& aIID, void** aSink);

  // nsIHTTPEventSink
  NS_DECL_NSIHTTPEVENTSINK

  // nsIDOMXMLDocument
  NS_DECL_NSIDOMXMLDOCUMENT

protected:
  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);  // subclass hook for sheet ordering
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);

  // For HTML elements in our content model
  // XXX This is not clean, but is there a better way? 
  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mInlineStyleSheet;
  nsString mBaseTarget;

  nsIParser *mParser;
};


#endif // nsXMLDocument_h___
