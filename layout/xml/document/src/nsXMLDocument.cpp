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
#include "nsRepository.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);


NS_LAYOUT nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult)
{
  nsXMLDocument* doc = new nsXMLDocument();
  return doc->QueryInterface(kIDocumentIID, (void**) aInstancePtrResult);
}

nsXMLDocument::nsXMLDocument()
{
  mParser = nsnull;
  mNameSpaces = nsnull;
  mAttrStyleSheet = nsnull;
  mProlog = nsnull;
  mEpilog = nsnull;
}

nsXMLDocument::~nsXMLDocument()
{
  NS_IF_RELEASE(mParser);
  if (nsnull != mNameSpaces) {
    int i;
    for (i = 0; i < mNameSpaces->Count(); i++) {
      nsXMLNameSpace *ns = (nsXMLNameSpace *)mNameSpaces->ElementAt(i);
      
      if (nsnull != ns) {
	NS_IF_RELEASE(ns->mPrefix);
	delete ns->mURI;
	delete ns;
      }
    }
    mNameSpaces = nsnull;
  }
  NS_IF_RELEASE(mAttrStyleSheet);
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

NS_IMETHODIMP 
nsXMLDocument::StartDocumentLoad(nsIURL *aUrl, 
				 nsIContentViewerContainer* aContainer,
				 nsIStreamListener **aDocListener,
				 const char* aCommand)
{
  nsresult rv = nsDocument::StartDocumentLoad(aUrl, aContainer,
                                              aDocListener);
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
      // For the HTML content within a document
      if (NS_OK == NS_NewHTMLStyleSheet(&mAttrStyleSheet, aUrl)) {
        AddStyleSheet(mAttrStyleSheet); // tell the world about our new style sheet
      }
      
      // Set the parser as the stream listener for the document loader...
      static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
      rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener);

      if (NS_OK == rv) {

        nsIDTD* theDTD=0;
	// XXX For now, we'll use the HTML DTD
        NS_NewWellFormed_DTD(&theDTD);
        mParser->RegisterDTD(theDTD);

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

// XXX Currently a nsIHTMLDocument method. Should go into an interface
// implemented for XML documents that hold HTML content.
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
nsXMLDocument::CreateAttribute(const nsString& aName, nsIDOMAttr** aReturn)
{
  // XXX TBI
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateElement(const nsString& aTagName, 
                              nsIDOMElement** aReturn)
{
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
nsXMLDocument::RegisterNameSpace(nsIAtom *aPrefix, const nsString& aURI, 
				 PRInt32& aNameSpaceId)
{
  if (nsnull == mNameSpaces) {
    mNameSpaces = new nsVoidArray();
    if (nsnull == mNameSpaces) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  nsXMLNameSpace* ns = new nsXMLNameSpace;
  if (nsnull == ns) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ns->mPrefix = aPrefix;
  NS_IF_ADDREF(ns->mPrefix);
  ns->mURI = new nsString(aURI);

  mNameSpaces->AppendElement((void *)ns);
  aNameSpaceId = mNameSpaces->Count();

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetNameSpaceURI(PRInt32 aNameSpaceId, nsString& aURI)
{
  if (nsnull == mNameSpaces) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  nsXMLNameSpace *ns = (nsXMLNameSpace *)mNameSpaces->ElementAt(aNameSpaceId - 1);
  if (nsnull == ns) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  aURI.SetString(*ns->mURI);

  return NS_OK;
}
 
NS_IMETHODIMP 
nsXMLDocument::GetNameSpacePrefix(PRInt32 aNameSpaceId, nsIAtom*& aPrefix)
{
  if (nsnull == mNameSpaces) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  nsXMLNameSpace *ns = (nsXMLNameSpace *)mNameSpaces->ElementAt(aNameSpaceId - 1);
  if (nsnull == ns) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  
  aPrefix = ns->mPrefix;
  NS_IF_ADDREF(aPrefix);

  return NS_OK;
}

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



