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


#include "nsXMLDocument.h"
#include "nsWellFormedDTD.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIXMLContent.h"
#include "nsIXMLContentSink.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
#include "nsIContentViewerContainer.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsRepository.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);


NS_LAYOUT nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult)
{
  nsXMLDocument* doc = new nsXMLDocument();
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsXMLDocument::nsXMLDocument()
{
  mParser = nsnull;
  mAttrStyleSheet = nsnull;
  mInlineStyleSheet = nsnull;
  mProlog = nsnull;
  mEpilog = nsnull;

  // XXX The XML world depends on the html atoms
  nsHTMLAtoms::AddrefAtoms();
}

nsXMLDocument::~nsXMLDocument()
{
  NS_IF_RELEASE(mParser);
  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }
  if (nsnull != mProlog) {
    delete mProlog;
  }
  if (nsnull != mEpilog) {
    delete mProlog;
  }
}

NS_IMETHODIMP 
nsXMLDocument::QueryInterface(REFNSIID aIID,
                              void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIXMLDocumentIID)) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIXMLDocument *)this;
    return NS_OK;
  }
  if (aIID.Equals(kIHTMLContentContainerIID)) {
    NS_ADDREF_THIS();
    *aInstancePtr = (void**) (nsIHTMLContentContainer *)this;
    return NS_OK;
  }
  return nsDocument::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsXMLDocument::AddRef()
{
  return nsDocument::AddRef();
}

nsrefcnt nsXMLDocument::Release()
{
  return nsDocument::Release();
}

nsresult
nsXMLDocument::Reset(nsIURL* aURL)
{
  nsresult result = nsDocument::Reset(aURL);
  if (NS_FAILED(result)) {
    return result;
  }

  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }

  result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aURL, this);
  if (NS_OK == result) {
    AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
    
    result = NS_NewHTMLCSSStyleSheet(&mInlineStyleSheet, aURL, this);
    if (NS_OK == result) {
      AddStyleSheet(mInlineStyleSheet); // tell the world about our new style sheet
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::StartDocumentLoad(nsIURL *aUrl, 
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener,
                                 const char* aCommand)
{
  nsresult rv = nsDocument::StartDocumentLoad(aUrl, 
                                              aContainer, 
                                              aDocListener,
                                              aCommand);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIWebShell* webShell;

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  rv = nsRepository::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&mParser);
  if (NS_OK == rv) {
    nsIXMLContentSink* sink;
    
    aContainer->QueryInterface(kIWebShellIID, (void**)&webShell);
    rv = NS_NewXMLContentSink(&sink, this, aUrl, webShell);
    NS_IF_RELEASE(webShell);

    if (NS_OK == rv) {      
      // Set the parser as the stream listener for the document loader...
      static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
      rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener);

      if (NS_OK == rv) {

        nsIDTD* theDTD=0;
        // XXX For now, we'll use the HTML DTD
        NS_NewWellFormed_DTD(&theDTD);
        mParser->RegisterDTD(theDTD);
        mParser->SetCommand(aCommand);
        mParser->SetContentSink(sink);
        mParser->Parse(aUrl);
      }
      NS_RELEASE(sink); 
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::EndLoad()
{
  NS_IF_RELEASE(mParser);
  return nsDocument::EndLoad();
}

NS_IMETHODIMP 
nsXMLDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mAttrStyleSheet;
  if (nsnull == mAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mAttrStyleSheet);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mInlineStyleSheet;
  if (nsnull == mInlineStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mInlineStyleSheet);
  }
  return NS_OK;
}

void nsXMLDocument::AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet)
{
  if ((nsnull != mInlineStyleSheet) && (aSheet != mInlineStyleSheet)) {
    aSet->InsertDocStyleSheetAfter(aSheet, mInlineStyleSheet);
  }
  else {
    aSet->InsertDocStyleSheetBefore(aSheet, nsnull);  // put it in front
  }
}

// nsIDOMDocument interface
NS_IMETHODIMP    
nsXMLDocument::GetDoctype(nsIDOMDocumentType** aDocumentType)
{
  // XXX TBI
  *aDocumentType = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
  // XXX TBI
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
  // XXX TBI
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
  // XXX Should just be regular nsIContent
  nsIHTMLContent* comment = nsnull;
  nsresult        rv = NS_NewCommentNode(&comment);

  if (NS_OK == rv) {
    rv = comment->QueryInterface(kIDOMCommentIID, (void**)aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
{
  // XXX TBI
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;  
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateElement(const nsString& aTagName, 
                              nsIDOMElement** aReturn)
{
  // XXX Should actually check parse namespace, determine
  // current namespace scope and, potentially, create new
  // HTML content form tags with a HTML prefix.
  nsIXMLContent* content;
  nsIAtom* tag = NS_NewAtom(aTagName);
  nsresult rv = NS_NewXMLElement(&content, tag);
  NS_RELEASE(tag);
  
  if (NS_OK != rv) {
    return rv;
  }
  rv = content->QueryInterface(kIDOMElementIID, (void**)aReturn);
  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
  // XXX Should just be regular nsIContent
  nsIHTMLContent* text = nsnull;
  nsresult        rv = NS_NewTextNode(&text);

  if (NS_OK == rv) {
    rv = text->QueryInterface(kIDOMTextIID, (void**)aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}


// nsIXMLDocument interface
NS_IMETHODIMP 
nsXMLDocument::PrologElementAt(PRInt32 aIndex, nsIContent** aContent)
{
  if (nsnull == mProlog) {
    *aContent = nsnull;
  }
  else {
    *aContent = (nsIContent *)mProlog->ElementAt(aIndex);
    NS_ADDREF(*aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::PrologCount(PRInt32* aCount)
{
  if (nsnull == mProlog) {
    *aCount = 0;
  }
  else {
    *aCount = mProlog->Count();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::AppendToProlog(nsIContent* aContent)
{
  if (nsnull == mProlog) {
    mProlog = new nsVoidArray();
  }

  mProlog->AppendElement((void *)aContent);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::EpilogElementAt(PRInt32 aIndex, nsIContent** aContent)
{
  if (nsnull == mEpilog) {
    *aContent = nsnull;
  }
  else {
    *aContent = (nsIContent *)mEpilog->ElementAt(aIndex);
    NS_ADDREF(*aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::EpilogCount(PRInt32* aCount)
{
  if (nsnull == mEpilog) {
    *aCount = 0;
  }
  else {
    *aCount = mEpilog->Count();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::AppendToEpilog(nsIContent* aContent)
{
  if (nsnull == mEpilog) {
    mEpilog = new nsVoidArray();
  }

  mEpilog->AppendElement((void *)aContent);

  return NS_OK;
}



