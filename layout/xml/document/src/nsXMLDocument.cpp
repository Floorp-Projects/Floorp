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
#include "nsIComponentManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsExpatDTD.h"
#include "nsINameSpaceManager.h"

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"
#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#endif

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMProcessingInstructionIID, NS_IDOMPROCESSINGINSTRUCTION_IID);
static NS_DEFINE_IID(kIDOMCDATASectionIID, NS_IDOMCDATASECTION_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);


// ==================================================================
// =
// ==================================================================

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
  
  // XXX The XML world depends on the html atoms
  nsHTMLAtoms::AddrefAtoms();
#ifdef INCLUDE_XUL
  // XUL world lives within XML world until it gets a place of its own
  nsXULAtoms::AddrefAtoms();
#endif
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
#ifdef INCLUDE_XUL
  nsXULAtoms::ReleaseAtoms();
#endif
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

  rv = nsComponentManager::CreateInstance(kCParserCID, 
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

        /* Commenting out the call to RegisterDTD() as per rickg's instructions.
           XML and HTML DTD's are going to be pre-registered withing nsParser. */
        // mParser->RegisterDTD(theDTD);
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

void nsXMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet)  // subclass hook for sheet ordering
{
  if (aSheet == mAttrStyleSheet) {  // always first
    mStyleSheets.InsertElementAt(aSheet, 0);
  }
  else if (aSheet == mInlineStyleSheet) {  // always last
    mStyleSheets.AppendElement(aSheet);
  }
  else {
    if (mInlineStyleSheet == mStyleSheets.ElementAt(mStyleSheets.Count() - 1)) {
      // keep attr sheet last
      mStyleSheets.InsertElementAt(aSheet, mStyleSheets.Count() - 1);
    }
    else {
      mStyleSheets.AppendElement(aSheet);
    }
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
  nsIContent* content;
  nsresult rv = NS_NewXMLCDATASection(&content);

  if (NS_OK == rv) {
    rv = content->QueryInterface(kIDOMCDATASectionIID, (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(content);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsString& aName, nsIDOMEntityReference** aReturn)
{
  *aReturn = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateProcessingInstruction(const nsString& aTarget, 
                                           const nsString& aData, 
                                           nsIDOMProcessingInstruction** aReturn)
{
  nsIContent* content;
  nsresult rv = NS_NewXMLProcessingInstruction(&content, aTarget, aData);

  if (NS_OK != rv) {
    return rv;
  }

  rv = content->QueryInterface(kIDOMProcessingInstructionIID, (void**)aReturn);
  NS_RELEASE(content);
  
  return rv;
}
 
static char kNameSpaceSeparator[] = ":";

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
  NS_RELEASE(content);
 
  return rv;
}

NS_IMETHODIMP    
nsXMLDocument::CreateElementWithNameSpace(const nsString& aTagName, 
                                            const nsString& aNameSpace, 
                                            nsIDOMElement** aReturn)
{
  PRInt32 nsID = kNameSpaceID_None;
  nsresult rv = NS_OK;

  if ((0 < aNameSpace.Length() && (nsnull != mNameSpaceManager))) {
    mNameSpaceManager->GetNameSpaceID(aNameSpace, nsID);
  }

  nsIContent* content;
  if (nsID == kNameSpaceID_HTML) {
    nsIHTMLContent* htmlContent;
    
    rv = NS_CreateHTMLElement(&htmlContent, aTagName);
    content = (nsIContent*)htmlContent;
  }
  else {
    nsIXMLContent* xmlContent;
    nsIAtom* tag;

    tag = NS_NewAtom(aTagName);
    rv = NS_NewXMLElement(&xmlContent, tag);
    NS_RELEASE(tag);
    if (NS_OK == rv) {
      xmlContent->SetNameSpaceID(nsID);
    }
    content = (nsIXMLContent*)xmlContent;
  }

  if (NS_OK != rv) {
    return rv;
  }
  rv = content->QueryInterface(kIDOMElementIID, (void**)aReturn);
  
  return rv;
}
 

// nsIXMLDocument interface
static nsIContent *
MatchName(nsIContent *aContent, const nsString& aName)
{
  nsAutoString value;
  nsIContent *result = nsnull;
  PRInt32 ns;

  aContent->GetNameSpaceID(ns);
  if (kNameSpaceID_HTML == ns) {
    if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, value)) &&
        aName.Equals(value)) {
      return aContent;
    }
    else if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, value)) &&
             aName.Equals(value)) {
      return aContent;
    }
  }
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchName(child, aName);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::GetContentById(const nsString& aName, nsIContent** aContent)
{
  // XXX Since we don't have a validating parser, the only content
  // that this applies to is HTML content.
  nsIContent *content;

  content = MatchName(mRootContent, aName);

  NS_IF_ADDREF(content);
  *aContent = content;

  return NS_OK;
}



