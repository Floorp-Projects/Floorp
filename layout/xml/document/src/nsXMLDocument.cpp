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

// ==================================================================
// =
// ==================================================================
nsXMLDocumentChildNodes::nsXMLDocumentChildNodes(nsXMLDocument* aDocument)
{
  // We don't reference count our document reference (to avoid circular
  // references). We'll be told when the document goes away.
  mDocument = aDocument;
}
 
nsXMLDocumentChildNodes::~nsXMLDocumentChildNodes()
{
}

NS_IMETHODIMP
nsXMLDocumentChildNodes::GetLength(PRUint32* aLength)
{
  if (nsnull != mDocument) {
    PRUint32 prolog, epilog;

    // The length is the sum of the prolog, epilog and
    // document element;
    mDocument->PrologCount(&prolog);
    mDocument->EpilogCount(&epilog);
    *aLength = prolog + epilog + 1;
  }
  else {
    *aLength = 0;
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsXMLDocumentChildNodes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;
  if (nsnull != mDocument) {
    PRUint32 prolog;
    
    mDocument->PrologCount(&prolog);
    if (aIndex < prolog) {
      // It's in the prolog
      nsIContent* content;
      result = mDocument->PrologElementAt(aIndex, &content);
      if ((NS_OK == result) && (nsnull != content)) {
        result = content->QueryInterface(kIDOMNodeIID, (void**)aReturn);
        NS_RELEASE(content);
      }
    }
    else if (aIndex == prolog) {
      // It's the document element
      nsIDOMElement* element;
      result = mDocument->GetDocumentElement(&element);
      if (NS_OK == result) {
        result = element->QueryInterface(kIDOMNodeIID, (void**)aReturn);
        NS_RELEASE(element);
      }
    }
    else {
      // It's in the epilog
      nsIContent* content;
      result = mDocument->EpilogElementAt(aIndex-prolog-1, &content);
      if ((NS_OK == result) && (nsnull != content)) {
        result = content->QueryInterface(kIDOMNodeIID, (void**)aReturn);
        NS_RELEASE(content);
      }
    }
  }
  
  return result;
}

void 
nsXMLDocumentChildNodes::DropReference()
{
  mDocument = nsnull;
}

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
  mProlog = nsnull;
  mEpilog = nsnull;
  mChildNodes = nsnull;
  
  // XXX The XML world depends on the html atoms
  nsHTMLAtoms::AddrefAtoms();
#ifdef INCLUDE_XUL
  // XUL world lives within XML world until it gets a place of its own
  nsXULAtoms::AddrefAtoms();
#endif
}

nsXMLDocument::~nsXMLDocument()
{
  PRInt32 i, count;
  nsIContent* content;
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
    count = mProlog->Count();
    for (i = 0; i < count; i++) {
      content = (nsIContent*)mProlog->ElementAt(i);
      NS_RELEASE(content);
    }
    delete mProlog;
  }
  if (nsnull != mEpilog) {
    count = mEpilog->Count();
    for (i = 0; i < count; i++) {
      content = (nsIContent*)mEpilog->ElementAt(i);
      NS_RELEASE(content);
    }
    delete mEpilog;
  }
  NS_IF_RELEASE(mChildNodes);
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


// nsIDOMNode interface
NS_IMETHODIMP 
nsXMLDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  if (nsnull == mChildNodes) {
    mChildNodes = new nsXMLDocumentChildNodes(this);
    if (nsnull == mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mChildNodes);
  }

  return mChildNodes->QueryInterface(kIDOMNodeListIID, (void**)aChildNodes);
}

NS_IMETHODIMP 
nsXMLDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  nsresult result = NS_OK;

  if ((nsnull != mProlog) && (0 != mProlog->Count())) {
    nsIContent* content;
    result = PrologElementAt(0, &content);
    if ((NS_OK == result) && (nsnull != content)) {
      result = content->QueryInterface(kIDOMNodeIID, (void**)aFirstChild);
      NS_RELEASE(content);
    }
  }
  else {
    nsIDOMElement* element;
    result = GetDocumentElement(&element);
    if (NS_OK == result) {
      result = element->QueryInterface(kIDOMNodeIID, (void**)aFirstChild);
      NS_RELEASE(element);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  nsresult result = NS_OK;

  if ((nsnull != mEpilog) && (0 != mEpilog->Count())) {
    nsIContent* content;
    result = EpilogElementAt(mEpilog->Count()-1, &content);
    if ((NS_OK == result) && (nsnull != content)) {
      result = content->QueryInterface(kIDOMNodeIID, (void**)aLastChild);
      NS_RELEASE(content);
    }
  }
  else {
    nsIDOMElement* element;
    result = GetDocumentElement(&element);
    if (NS_OK == result) {
      result = element->QueryInterface(kIDOMNodeIID, (void**)aLastChild);
      NS_RELEASE(element);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::InsertBefore(nsIDOMNode* aNewChild,
                            nsIDOMNode* aRefChild, 
                            nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsXMLDocument::ReplaceChild(nsIDOMNode* aNewChild,
                            nsIDOMNode* aOldChild, 
                            nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsXMLDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsXMLDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsXMLDocument::HasChildNodes(PRBool* aReturn)
{
  *aReturn = PR_TRUE;
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
nsXMLDocument::CreateProcessingInstruction(const nsString& aTarget, const nsString& aData, nsIDOMProcessingInstruction** aReturn)
{
  // XXX TBI
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;  
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
NS_IMETHODIMP 
nsXMLDocument::PrologElementAt(PRUint32 aIndex, nsIContent** aContent)
{
  if (nsnull == mProlog) {
    *aContent = nsnull;
  }
  else {
    *aContent = (nsIContent *)mProlog->ElementAt((PRInt32)aIndex);
    NS_ADDREF(*aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::PrologCount(PRUint32* aCount)
{
  if (nsnull == mProlog) {
    *aCount = 0;
  }
  else {
    *aCount = (PRUint32)mProlog->Count();
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
  NS_ADDREF(aContent);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::EpilogElementAt(PRUint32 aIndex, nsIContent** aContent)
{
  if (nsnull == mEpilog) {
    *aContent = nsnull;
  }
  else {
    *aContent = (nsIContent *)mEpilog->ElementAt((PRInt32)aIndex);
    NS_ADDREF(*aContent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::EpilogCount(PRUint32* aCount)
{
  if (nsnull == mEpilog) {
    *aCount = 0;
  }
  else {
    *aCount = (PRUint32)mEpilog->Count();
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
  NS_ADDREF(aContent);

  return NS_OK;
}

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



