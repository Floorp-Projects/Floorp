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
#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIParser.h"
#include "nsIUnicharInputStream.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIDocument.h"
#include "nsIXMLDocument.h"
#include "nsIXMLContent.h"
#include "nsIScriptObjectOwner.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIURL.h"
#include "nsNeckoUtil.h"
#else
#include "nsIURLGroup.h"
#endif // NECKO
#include "nsIWebShell.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsIHTMLContent.h"
#include "nsHTMLEntities.h" 
#include "nsHTMLParts.h" 
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIContentViewer.h"
#include "prtime.h"
#include "prlog.h"
#include "prmem.h"
#ifdef XSL
#include "nsXSLContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include <windows.h>
#include "nsISupports.h"
#include "nsParserCIID.h"
#endif

// XXX misnamed header file, but oh well
#include "nsHTMLTokens.h"  

static char kNameSpaceSeparator = ':';
static char kNameSpaceDef[] = "xmlns";
static char kStyleSheetPI[] = "xml-stylesheet";
static char kCSSType[] = "text/css";

#ifdef XSL
static char kXSLType[] = "text/xsl";
#endif

static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMCDATASectionIID, NS_IDOMCDATASECTION_IID);
#ifdef XSL
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
#endif

static void SetTextStringOnTextNode(const nsString& aTextString, nsIContent* aTextNode);

// XXX Open Issues:
// 1) html:style - Should we allow inline style? If so, the content
//    sink needs to process the tag and invoke the CSS parser.
// 2) html:base - Should we allow a base tag? If so, the content
//    sink needs to maintain the base when resolving URLs for
//    loaded scripts and style sheets. Should it be allowed anywhere?
// 3) what's not allowed - We need to figure out which HTML tags
//    (prefixed with a HTML namespace qualifier) are explicitly not
//    allowed (if any).
// 4) factoring code with nsHTMLContentSink - There's some amount of
//    common code between this and the HTML content sink. This will
//    increase as we support more and more HTML elements. How can code
//    from the code be factored?

nsresult
NS_NewXMLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
                     nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXMLContentSink* it;
  NS_NEWXPCOM(it, nsXMLContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL, aWebShell);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIXMLContentSinkIID, (void **)aResult);
}

nsXMLContentSink::nsXMLContentSink()
{
  NS_INIT_REFCNT();
  mDocument = nsnull;
  mDocumentURL = nsnull;
  mDocumentBaseURL = nsnull;
  mWebShell = nsnull;
  mParser = nsnull;
  mRootElement = nsnull;
  mDocElement = nsnull;
  mContentStack = nsnull;
  mNameSpaceStack = nsnull;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
  mConstrainSize = PR_TRUE;
  mInScript = PR_FALSE;
  mStyleSheetCount = 0;
  mCSSLoader       = nsnull;
#ifdef XSL
  mXSLTransformMediator = nsnull;
#endif
}

nsXMLContentSink::~nsXMLContentSink()
{
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mDocumentBaseURL);
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mRootElement);
  NS_IF_RELEASE(mDocElement);
  if (nsnull != mContentStack) {
    // there shouldn't be anything here except in an error condition
    PRInt32 index = mContentStack->Count();
    while (0 < index--) {
      nsIContent* content = (nsIContent*)mContentStack->ElementAt(index);
      NS_RELEASE(content);
    }
    delete mContentStack;
  }
  if (nsnull != mNameSpaceStack) {
    // There shouldn't be any here except in an error condition
    PRInt32 index = mNameSpaceStack->Count();
    while (0 < index--) {
      nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
      NS_RELEASE(nameSpace);
    }
    delete mNameSpaceStack;
  }
  if (nsnull != mText) {
    PR_FREEIF(mText);
  }
  NS_IF_RELEASE(mCSSLoader);
#ifdef XSL  
  NS_IF_RELEASE(mXSLTransformMediator);
#endif
}

nsresult
nsXMLContentSink::Init(nsIDocument* aDoc,
                       nsIURI* aURL,
                       nsIWebShell* aContainer)
{
  NS_PRECONDITION(nsnull != aDoc, "null ptr");
  NS_PRECONDITION(nsnull != aURL, "null ptr");
  NS_PRECONDITION(nsnull != aContainer, "null ptr");
  if ((nsnull == aDoc) || (nsnull == aURL) || (nsnull == aContainer)) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;
  NS_ADDREF(aDoc);
  mDocumentURL = aURL;
  NS_ADDREF(aURL);
  mDocumentBaseURL = aURL;
  NS_ADDREF(aURL);
  mWebShell = aContainer;
  NS_ADDREF(aContainer);

  mState = eXMLContentSinkState_InProlog;
  mDocElement = nsnull;
  mRootElement = nsnull;

  // XXX this presumes HTTP header info is alread set in document
  // XXX if it isn't we need to set it here...
  mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle, mPreferredStyle);

  nsIHTMLContentContainer* htmlContainer = nsnull;
  if (NS_SUCCEEDED(aDoc->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer))) {
    htmlContainer->GetCSSLoader(mCSSLoader);
    NS_RELEASE(htmlContainer);
  }

  return NS_OK;
}

#ifndef XSL
// nsISupports
NS_IMPL_ISUPPORTS(nsXMLContentSink, kIXMLContentSinkIID)
#else

NS_IMPL_THREADSAFE_ADDREF(nsXMLContentSink)
NS_IMPL_THREADSAFE_RELEASE(nsXMLContentSink)

nsresult
nsXMLContentSink::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIXMLContentSinkIID)) {
    *aInstancePtr = (void*)(nsIXMLContentSink*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }    
  if (aIID.Equals(kIObserverIID)) {
    *aInstancePtr = (void*)(nsIObserver*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }  
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIXMLContentSink*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}
#endif

  // nsIContentSink
NS_IMETHODIMP 
nsXMLContentSink::WillBuildModel(void)
{
  // Notify document that the load is beginning
  mDocument->BeginLoad();
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  // XXX this is silly; who cares?
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell( dont_AddRef(mDocument->GetShellAt(i)) );
    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if(vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
    }
  }

#ifndef XSL
  StartLayoutProcess();
#else
  nsresult rv;
  if (mXSLTransformMediator) {
    rv = SetupTransformMediator();  
  } 

  if (!mXSLTransformMediator || NS_FAILED(rv)) {
    mDocument->SetRootContent(mDocElement);
    StartLayoutProcess();
  }
#endif

  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

  return NS_OK;
}

void 
nsXMLContentSink::StartLayoutProcess()
{
  StartLayout();

  // XXX Should scroll to ref when that makes sense
  // ScrollToRef();

  mDocument->EndLoad();
}

#ifdef XSL
// The observe method is called on completion of the transform.  The nsISupports argument is an
// nsIDOMElement interface to the root node of the output content model.
NS_IMETHODIMP
nsXMLContentSink::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsIContent* content;
  nsresult rv = NS_OK;

  // Set the output content model on the document
  rv = aSubject->QueryInterface(kIContentIID, (void **) &content);
  if (NS_SUCCEEDED(rv)) {
    mDocument->SetRootContent(content);
    NS_RELEASE(content);
  }
  else
    mDocument->SetRootContent(mDocElement);

  // Start the layout process
  StartLayoutProcess();

  // Reset the observer on the transform mediator
  mXSLTransformMediator->SetTransformObserver(nsnull);
  
  return rv;
}


// Provide the transform mediator with the source document's content
// model and the output document, and register the XML content sink 
// as the transform observer.  The transform mediator will call
// the nsIObserver::Observe() method on the transform observer once
// the transform is completed.  The nsISupports pointer to the Observe
// method will be an nsIDOMElement pointer to the root node of the output
// content model.
nsresult
nsXMLContentSink::SetupTransformMediator()
{
  nsIDOMElement* source;
  nsIDOMDocument* currentDoc;
  nsresult rv = NS_OK;

  rv = mDocElement->QueryInterface(kIDOMElementIID, (void **) &source);
  if (NS_SUCCEEDED(rv)) {
    mXSLTransformMediator->SetSourceContentModel(source);
    rv = mDocument->QueryInterface(kIDOMDocumentIID, (void **) &currentDoc);
    if (NS_SUCCEEDED(rv)) {
      mXSLTransformMediator->SetCurrentDocument(currentDoc);
      mXSLTransformMediator->SetTransformObserver(this);
      NS_RELEASE(currentDoc);
    }
    NS_RELEASE(source);
  }

  return rv;
}
#endif

NS_IMETHODIMP 
nsXMLContentSink::WillInterrupt(void)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::WillResume(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::SetParser(nsIParser* aParser)
{
  NS_IF_RELEASE(mParser);
  mParser = aParser;
  NS_IF_ADDREF(mParser);
  
  return NS_OK;
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
static void
GetAttributeValueAt(const nsIParserNode& aNode,
                    PRInt32 aIndex,
                    nsString& aResult)
{
  // Copy value
  const nsString& value = aNode.GetValueAt(aIndex);
  aResult.Truncate();
  aResult.Append(value);

  // Strip quotes if present
  PRUnichar first = aResult.First();
  if ((first == '\"') || (first == '\'')) {
    if (aResult.Last() == first) {
      aResult.Cut(0, 1);
      PRInt32 pos = aResult.Length() - 1;
      if (pos >= 0) {
        aResult.Cut(pos, 1);
      }
    } else {
      // Mismatched quotes - leave them in
    }
  }

  // Reduce any entities
  // XXX Note: as coded today, this will only convert well formed
  // entities.  This may not be compatible enough.
  // XXX there is a table in navigator that translates some numeric entities
  // should we be doing that? If so then it needs to live in two places (bad)
  // so we should add a translate numeric entity method from the parser...
  char cbuf[100];
  PRInt32 index = 0;
  while (index < aResult.Length()) {
    // If we have the start of an entity (and it's not at the end of
    // our string) then translate the entity into it's unicode value.
    if ((aResult.CharAt(index++) == '&') && (index < aResult.Length())) {
      PRInt32 start = index - 1;
      PRUnichar e = aResult.CharAt(index);
      if (e == '#') {
        // Convert a numeric character reference
        index++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        PRBool ok = PR_FALSE;
        PRInt32 slen = aResult.Length();
        while ((index < slen) && (cp < limit)) {
          PRUnichar e = aResult.CharAt(index);
          if (e == ';') {
            index++;
            ok = PR_TRUE;
            break;
          }
          if ((e >= '0') && (e <= '9')) {
            *cp++ = char(e);
            index++;
            continue;
          }
          break;
        }
        if (!ok || (cp == cbuf)) {
          continue;
        }
        *cp = '\0';
        if (cp - cbuf > 5) {
          continue;
        }
        PRInt32 ch = PRInt32( ::atoi(cbuf) );
        if (ch > 65535) {
          continue;
        }

        // Remove entity from string and replace it with the integer
        // value.
        aResult.Cut(start, index - start);
        aResult.Insert(PRUnichar(ch), start);
        index = start + 1;
      }
      else if (((e >= 'A') && (e <= 'Z')) ||
               ((e >= 'a') && (e <= 'z'))) {
        // Convert a named entity
        index++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        *cp++ = char(e);
        PRBool ok = PR_FALSE;
        PRInt32 slen = aResult.Length();
        while ((index < slen) && (cp < limit)) {
          PRUnichar e = aResult.CharAt(index);
          if (e == ';') {
            index++;
            ok = PR_TRUE;
            break;
          }
          if (((e >= '0') && (e <= '9')) ||
              ((e >= 'A') && (e <= 'Z')) ||
              ((e >= 'a') && (e <= 'z'))) {
            *cp++ = char(e);
            index++;
            continue;
          }
          break;
        }
        if (!ok || (cp == cbuf)) {
          continue;
        }
        *cp = '\0';
        PRInt32 ch = nsHTMLEntities::EntityToUnicode(nsSubsumeCStr(cbuf, PR_FALSE));
        if (ch < 0) {
          continue;
        }

        // Remove entity from string and replace it with the integer
        // value.
        aResult.Cut(start, index - start);
        aResult.Insert(PRUnichar(ch), start);
        index = start + 1;
      }
      else if (e == '{') {
        // Convert a script entity
        // XXX write me!
      }
    }
  }
}

// XXX Code copied from nsHTMLContentSink. It should be shared.
nsresult
nsXMLContentSink::AddAttributes(const nsIParserNode& aNode,
                                nsIContent* aContent,
                                PRBool aIsHTML)
{
  // Add tag attributes to the content attributes
  nsAutoString name, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsString& key = aNode.GetKeyAt(i);
    name.Truncate();
    name.Append(key);

    nsIAtom* nameSpacePrefix = CutNameSpacePrefix(name);
    nsIAtom* nameAtom = NS_NewAtom(name);
    PRInt32 nameSpaceID = (nsnull == nameSpacePrefix) ? kNameSpaceID_None : GetNameSpaceId(nameSpacePrefix);
    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;  // XXX is this correct? or is it a bad document?
    }
    if ((kNameSpaceID_XMLNS == nameSpaceID) && aIsHTML) {
      NS_RELEASE(nameAtom);
      name.Insert("xmlns:", 0);
      nameAtom = NS_NewAtom(name);
      nameSpaceID = kNameSpaceID_HTML;  // XXX this is wrong, but necessary until HTML can store other namespaces for attrs
    }

    nsAutoString value;
    if (NS_CONTENT_ATTR_NOT_THERE == 
        aContent->GetAttribute(nameSpaceID, nameAtom, value)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);

      // Add attribute to content
      aContent->SetAttribute(nameSpaceID, nameAtom, v, PR_FALSE);
    }
    NS_RELEASE(nameAtom);
    NS_IF_RELEASE(nameSpacePrefix);
  }
  return NS_OK;
}

void
nsXMLContentSink::PushNameSpacesFrom(const nsIParserNode& aNode)
{
  nsAutoString k, uri, prefix;
  PRInt32 ac = aNode.GetAttributeCount();
  PRInt32 offset;
  nsINameSpace* nameSpace = nsnull;

  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(mNameSpaceStack->Count() - 1);
    NS_ADDREF(nameSpace);
  }
  else {
    nsINameSpaceManager* manager = nsnull;
    mDocument->GetNameSpaceManager(manager);
    NS_ASSERTION(nsnull != manager, "no name space manager in document");
    if (nsnull != manager) {
      manager->CreateRootNameSpace(nameSpace);
      NS_RELEASE(manager);
    }
  }

  if (nsnull != nameSpace) {
    for (PRInt32 i = 0; i < ac; i++) {
      const nsString& key = aNode.GetKeyAt(i);
      k.Truncate();
      k.Append(key);
      // Look for "xmlns" at the start of the attribute name
      offset = k.Find(kNameSpaceDef);
      if (0 == offset) {
        if (k.Length() == (sizeof(kNameSpaceDef)-1)) {
          // If there's nothing left, this is a default namespace
          prefix.Truncate();
        }
        else {
          PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
          // If the next character is a :, there is a namespace prefix
          if (':' == next) {
            prefix.Truncate();
            k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
          }
          else {
            continue;
          }
        }

        // Get the attribute value (the URI for the namespace)
        GetAttributeValueAt(aNode, i, uri);
      
        // Open a local namespace
        nsIAtom* prefixAtom = ((0 < prefix.Length()) ? NS_NewAtom(prefix) : nsnull);
        nsINameSpace* child = nsnull;
        nameSpace->CreateChildNameSpace(prefixAtom, uri, child);
        if (nsnull != child) {
          NS_RELEASE(nameSpace);
          nameSpace = child;
        }
        NS_IF_RELEASE(prefixAtom);
      }
    }
    if (nsnull == mNameSpaceStack) {
      mNameSpaceStack = new nsVoidArray();
    }
    mNameSpaceStack->AppendElement(nameSpace);
  }
}

nsIAtom*  nsXMLContentSink::CutNameSpacePrefix(nsString& aString)
{
  nsAutoString  prefix;
  PRInt32 nsoffset = aString.FindChar(kNameSpaceSeparator);
  if (-1 != nsoffset) {
    aString.Left(prefix, nsoffset);
    aString.Cut(0, nsoffset+1);
  }
  if (0 < prefix.Length()) {
    return NS_NewAtom(prefix);
  }
  return nsnull;
}

NS_IMETHODIMP 
nsXMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsAutoString tag;
  nsIAtom* nameSpacePrefix;
  PRInt32 nameSpaceID = kNameSpaceID_Unknown;
  PRBool isHTML = PR_FALSE;
  PRBool pushContent = PR_TRUE;
  nsIContent *content;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the epilog, there should be no
  // new elements
  PR_ASSERT(eXMLContentSinkState_InEpilog != mState);

  FlushText();

  mState = eXMLContentSinkState_InDocumentElement;

  tag = aNode.GetText();
  nameSpacePrefix = CutNameSpacePrefix(tag);

  // We must register namespace declarations found in the attribute list
  // of an element before creating the element. This is because the
  // namespace prefix for an element might be declared within the attribute
  // list.
  PushNameSpacesFrom(aNode);

  nameSpaceID = GetNameSpaceId(nameSpacePrefix);
  isHTML = IsHTMLNameSpace(nameSpaceID);

  if (isHTML) {
    nsIAtom* tagAtom = NS_NewAtom(tag);
    if (nsHTMLAtoms::script == tagAtom) {
      result = ProcessStartSCRIPTTag(aNode);
    }
    NS_RELEASE(tagAtom);

    nsIHTMLContent *htmlContent = nsnull;
    result = NS_CreateHTMLElement(&htmlContent, tag);
    content = (nsIContent *)htmlContent;
  }
  else {
    nsIAtom *tagAtom = NS_NewAtom(tag);
    nsIXMLContent *xmlContent;
    result = NS_NewXMLElement(&xmlContent, tagAtom);
    NS_RELEASE(tagAtom);
    // For XML elements, set the namespace
    if (NS_OK == result) {
      xmlContent->SetNameSpacePrefix(nameSpacePrefix);
      xmlContent->SetNameSpaceID(nameSpaceID);
    }
    content = (nsIContent *)xmlContent;
  }
  NS_IF_RELEASE(nameSpacePrefix);

  if (NS_OK == result) {
    content->SetDocument(mDocument, PR_FALSE);

    // Set the attributes on the new content element
    result = AddAttributes(aNode, content, isHTML);
    if (NS_OK == result) {
      // If this is the document element
      if (nsnull == mDocElement) {
        mDocElement = content;
        NS_ADDREF(mDocElement);

        // For XSL, we need to wait till after the transform 
        // to set the root content object.  Hence, the following
        // ifndef.
#ifndef XSL         
        mDocument->SetRootContent(mDocElement);
#endif
      }
      else {
        nsIContent *parent = GetCurrentContent();

        parent->AppendChildTo(content, PR_FALSE);
      }
      if (pushContent) {
        PushContent(content);
      }
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsAutoString tag;
  nsIAtom* nameSpacePrefix;
  PRInt32 nameSpaceID = kNameSpaceID_Unknown;
  PRBool isHTML = PR_FALSE;
  PRBool popContent = PR_TRUE;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);

  tag = aNode.GetText();
  nameSpacePrefix = CutNameSpacePrefix(tag);
  nameSpaceID = GetNameSpaceId(nameSpacePrefix);
  isHTML = IsHTMLNameSpace(nameSpaceID);

  if (!mInScript) {
    FlushText();
  }

  if (isHTML) {
    nsIAtom* tagAtom = NS_NewAtom(tag);
    if (nsHTMLAtoms::script == tagAtom) {
      result = ProcessEndSCRIPTTag(aNode);
    }
    NS_RELEASE(tagAtom);
  }

  nsIContent* content = nsnull;
  if (popContent) {
    content = PopContent();
    if (nsnull != content) {
      if (mDocElement == content) {
        mState = eXMLContentSinkState_InEpilog;
      }
      NS_RELEASE(content);
    }
    else {
      // XXX Again, the parser should catch unmatched tags and
      // we should never get here.
      PR_ASSERT(0);
    }
  }
  nsINameSpace* nameSpace = PopNameSpaces();
  if (nsnull != content) {
    nsIXMLContent* xmlContent;
    if (NS_OK == content->QueryInterface(kIXMLContentIID, 
                                         (void **)&xmlContent)) {
      xmlContent->SetContainingNameSpace(nameSpace);
      NS_RELEASE(xmlContent);
    }
  }
  NS_IF_RELEASE(nameSpace);

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  // XXX For now, all leaf content is character data
  // XXX make sure to push/pop name spaces here too (for attributes)
  switch (aNode.GetTokenType()) {
    case eToken_text:
    case eToken_whitespace:
    case eToken_newline:
      AddText(aNode.GetText());
      break;

    case eToken_cdatasection:
      AddCDATASection(aNode);
      break;

    case eToken_entity:
    {
      nsAutoString tmp;
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
      if (unicode < 0) {
        return AddText(aNode.GetText());
      }
      return AddText(tmp);
    }
  }
  return NS_OK;
}

nsresult nsXMLContentSink::CreateErrorText(const nsParserError* aError, nsString& aErrorString)
{    
  nsString errorText("XML Parsing Error: ");

  if (aError) {
    errorText.Append(aError->description);
    errorText.Append("\nLine Number ");
    errorText.Append(aError->lineNumber, 10);
    errorText.Append(", Column ");
    errorText.Append(aError->colNumber, 10);
    errorText.Append(":");
  }
  
  aErrorString = errorText;
  
  return NS_OK;
}

nsresult nsXMLContentSink::CreateSourceText(const nsParserError* aError, nsString& aSourceString)
{  
  nsString sourceText;
  PRInt32 errorPosition = aError->colNumber;

  sourceText.Append(aError->sourceLine);
  sourceText.Append("\n");
  for (int i = 0; i < errorPosition; i++)
    sourceText.Append("-");
  sourceText.Append("^");

  aSourceString = sourceText;

  return NS_OK;
}

static void SetTextStringOnTextNode(const nsString& aTextString, nsIContent* aTextNode)
{
  nsITextContent* text = nsnull;
  PRUnichar *tempUnicode = aTextString.ToNewUnicode();
  static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

  aTextNode->QueryInterface(kITextContentIID, (void**) &text);
  text->SetText(tempUnicode, aTextString.Length(), PR_FALSE);
  delete [] tempUnicode;
  NS_RELEASE(text);
}


/*
 * We create the following XML snippet programmatically and
 * insert it into the content model:
 *
 *    <ParserError>
 *       XML Error: "contents of aError->description"
 *       Line Number: "contents of aError->lineNumber"
 *       <SourceText>
 *          "Contents of aError->sourceLine"
 *          "^ pointing at the error location"
 *       </SourceText>
 *    </ParserError>
 *
 */
NS_IMETHODIMP
nsXMLContentSink::NotifyError(const nsParserError* aError)
{  
  nsresult result = NS_OK;
  nsAutoString parserErrorTag = "parsererror";
  nsAutoString sourceTextTag = "sourcetext";
  nsString errorText;  
  nsString sourceText;
  nsIHTMLContent* errorContainerNode = nsnull;
  nsIHTMLContent* sourceContainerNode = nsnull;
  nsIContent* errorTextNode = nsnull;
  nsIContent* sourceTextNode = nsnull;

  /* Create container and text content nodes */
  result = NS_CreateHTMLElement(&errorContainerNode, parserErrorTag); // XXX these should NOT be in the HTML namespace
  if (NS_OK == result) {  
    result = NS_NewTextNode(&errorTextNode);
    if (NS_OK == result) {    
      result = NS_CreateHTMLElement(&sourceContainerNode, sourceTextTag);
      if (NS_OK == result) {
        result = NS_NewTextNode(&sourceTextNode);
      }
    }
  }

  /* Create the error text string and source text string
     and set the text strings into the text nodes. */
  result = CreateErrorText(aError, errorText);
  if (NS_OK == result) {
     SetTextStringOnTextNode(errorText, errorTextNode);
  }

  result = CreateSourceText(aError, sourceText);
  if (NS_OK == result) {
    SetTextStringOnTextNode(sourceText, sourceTextNode);
  }

  /* Hook the content nodes up to the document and to each other */
  if (NS_OK == result) {
    errorContainerNode->SetDocument(mDocument, PR_FALSE);
    errorTextNode->SetDocument(mDocument, PR_FALSE);
    sourceContainerNode->SetDocument(mDocument, PR_FALSE);
    sourceTextNode->SetDocument(mDocument, PR_FALSE);

    if (nsnull == mDocElement) {
      mDocElement = errorContainerNode;
      NS_ADDREF(mDocElement);
      mDocument->SetRootContent(mDocElement);
    }
    else {
      mDocElement->AppendChildTo(errorContainerNode, PR_FALSE);
    }
    errorContainerNode->AppendChildTo(errorTextNode, PR_FALSE);
    errorContainerNode->AppendChildTo(sourceContainerNode, PR_FALSE);
    sourceContainerNode->AppendChildTo(sourceTextNode, PR_FALSE);
  }
  
  return result;
}

// nsIXMLContentSink
NS_IMETHODIMP 
nsXMLContentSink::AddXMLDecl(const nsIParserNode& aNode)
{
  // XXX We'll ignore it for now
  printf("nsXMLContentSink::AddXMLDecl\n");
  return NS_OK;
}

nsresult
nsXMLContentSink::AddContentAsLeaf(nsIContent *aContent)
{      
  nsresult result = NS_OK;

  if (eXMLContentSinkState_InProlog == mState) {
    result = mDocument->AppendToProlog(aContent);
  }
  else if (eXMLContentSinkState_InEpilog == mState) {   
    result = mDocument->AppendToEpilog(aContent);
  }
  else {
    nsIContent *parent = GetCurrentContent();
    
    if (nsnull != parent) {
      result = parent->AppendChildTo(aContent, PR_FALSE);
    }
  }
  
  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddComment(const nsIParserNode& aNode)
{
  FlushText();

  nsAutoString text;
  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  text = aNode.GetText();
  result = NS_NewCommentNode(&comment);
  if (NS_OK == result) {
    result = comment->QueryInterface(kIDOMCommentIID, (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(text);
      NS_RELEASE(domComment);

      comment->SetDocument(mDocument, PR_FALSE);
      result = AddContentAsLeaf(comment);
    }
    NS_RELEASE(comment);
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddCDATASection(const nsIParserNode& aNode)
{
  FlushText();

  nsAutoString text;
  nsIContent *cdata;
  nsIDOMCDATASection *domCDATA;
  nsresult result = NS_OK;

  text = aNode.GetText();
  result = NS_NewXMLCDATASection(&cdata);
  if (NS_OK == result) {
    result = cdata->QueryInterface(kIDOMCDATASectionIID, (void **)&domCDATA);
    if (NS_OK == result) {
      domCDATA->AppendData(text);
      NS_RELEASE(domCDATA);

      cdata->SetDocument(mDocument, PR_FALSE);
      result = AddContentAsLeaf(cdata);
    }
    NS_RELEASE(cdata);
  }

  return result;
}


static nsresult
GetQuotedAttributeValue(nsString& aSource, 
                        const nsString& aAttribute,
                        nsString& aValue)
{
  PRInt32 offset;
  PRInt32 endOffset = -1;
  nsresult result = NS_OK;

  offset = aSource.Find(aAttribute);
  if (-1 != offset) {
    offset = aSource.FindChar('=', PR_FALSE,offset);

    PRUnichar next = aSource.CharAt(++offset);
    if (kQuote == next) {
      endOffset = aSource.FindChar(kQuote,PR_FALSE, ++offset);
    }
    else if (kApostrophe == next) {
      endOffset = aSource.FindChar(kApostrophe, PR_FALSE,++offset);	  
    }
  
    if (-1 != endOffset) {
      aSource.Mid(aValue, offset, endOffset-offset);
    }
    else {
      // Mismatched quotes - return an error
      result = NS_ERROR_FAILURE;
    }
  }
  else {
    aValue.Truncate();
  }

  return result;
}

static void
ParseProcessingInstruction(const nsString& aText,
                           nsString& aTarget,
                           nsString& aData)
{
  PRInt32 offset;

  aTarget.Truncate();
  aData.Truncate();

  offset = aText.FindCharInSet(" \n\r\t");
  if (-1 != offset) {
    aText.Mid(aTarget, 2, offset-2);
    aText.Mid(aData, offset+1, aText.Length()-offset-3);
  }
}

static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
  }
  else {
    aType = aValue;
  }
}

#ifdef XSL
nsXMLContentSink::CreateStyleSheetURL(nsIURI** aUrl, 
                                      const nsAutoString& aHref)
{
   nsAutoString absURL;
   nsIURI* docURL = mDocument->GetDocumentURL();
   nsILoadGroup* LoadGroup; 
   nsresult result = NS_OK;
   
   result = docURL->GetLoadGroup(&LoadGroup);

   if ((NS_SUCCEEDED(result)) && LoadGroup) {
     result = LoadGroup->CreateURL(aUrl, docURL, aHref, nsnull);
     NS_RELEASE(LoadGroup);
   }
   else {
#ifndef NECKO
     result = NS_MakeAbsoluteURL(docURL, nsnull, aHref, absURL);
     if (NS_SUCCEEDED(result)) {
       result = NS_NewURL(aUrl, absURL);
     }
#else
     result = NS_MakeAbsoluteURI(aHref, docURL, absURL);
     if (NS_SUCCEEDED(result)) {
       result = NS_NewURI(aUrl, absURL);
     }
#endif // NECKO
   }
   NS_RELEASE(docURL);
   return result;
}

// Create an XML parser and an XSL content sink and start parsing
// the XSL stylesheet located at the given URL.
nsresult
nsXMLContentSink::LoadXSLStyleSheet(nsIURI* aUrl, const nsString& aType)
{  
  nsresult rv = NS_OK;
  nsIParser* parser;

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  // Create the XML parser
  rv = nsComponentManager::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&parser);

  if (NS_SUCCEEDED(rv)) {
    // Create a transform mediator
    rv = NS_NewTransformMediator(&mXSLTransformMediator, aType);

    if (NS_SUCCEEDED(rv)) {
      // Enable the transform mediator. It will start the transform
      // as soon as it has enough state to do so.  The state needed is
      // the source content model, the style content model, the current
      // document, and an observer.  The XML and XSL content sinks provide 
      // this state by calling the various setters on nsITransformMediator.
      mXSLTransformMediator->SetEnabled(PR_TRUE);

      // The XML document owns the transform mediator.  Give the mediator to
      // the XML document.
      nsIXMLDocument* xmlDoc;
      rv = mDocument->QueryInterface(kIXMLDocumentIID, (void **) &xmlDoc);
      if (NS_SUCCEEDED(rv)) {
        xmlDoc->SetTransformMediator(mXSLTransformMediator);

        // Create the XSL content sink
        nsIXMLContentSink* sink;
        rv = NS_NewXSLContentSink(&sink, mXSLTransformMediator, mDocument, aUrl, mWebShell);

        if (NS_SUCCEEDED(rv)) {
          // Hook up the content sink to the parser's output and ask the parser
          // to start parsing the URL specified by aURL.   
          parser->SetContentSink(sink);

          nsAutoString utf8("UTF-8");
          mDocument->SetDocumentCharacterSet(utf8);
          parser->SetDocumentCharset(utf8, kCharsetFromDocTypeDefault);
          parser->Parse(aUrl);
    
          // XXX Don't we have to NS_RELEASE() theDTD?
          NS_RELEASE(sink);
        }
        NS_RELEASE(xmlDoc);
      }
      NS_RELEASE(mXSLTransformMediator);
    }    
    NS_RELEASE(parser);
  }
  return rv;
}

nsresult
nsXMLContentSink::ProcessStyleLink(nsIContent* aElement,
                                   const nsString& aHref, PRBool aAlternate,
                                   const nsString& aTitle, const nsString& aType,
                                   const nsString& aMedia)
{
  nsresult rv = NS_OK;

  if (aType.EqualsIgnoreCase(kXSLType))
    rv = ProcessXSLStyleLink(aElement, aHref, aAlternate, aTitle, aType, aMedia);
  else
    rv = ProcessCSSStyleLink(aElement, aHref, aAlternate, aTitle, aType, aMedia);

  return rv;
}

nsresult
nsXMLContentSink::ProcessXSLStyleLink(nsIContent* aElement,
                                   const nsString& aHref, PRBool aAlternate,
                                   const nsString& aTitle, const nsString& aType,
                                   const nsString& aMedia)
{
  nsresult rv = NS_OK;
  nsIURI* url;
  
  rv = CreateStyleSheetURL(&url, aHref);
  if (NS_SUCCEEDED(rv)) {
    rv = LoadXSLStyleSheet(url, aType);
    NS_RELEASE(url);
  }
  
  return rv;
}
#endif

nsresult
nsXMLContentSink::ProcessCSSStyleLink(nsIContent* aElement,
                                   const nsString& aHref, PRBool aAlternate,
                                   const nsString& aTitle, const nsString& aType,
                                   const nsString& aMedia)
{
  nsresult result = NS_OK;

  if (aAlternate) { // if alternate, does it have title?
    if (0 == aTitle.Length()) { // alternates must have title
      return NS_OK; //return without error, for now
    }
  }

  nsAutoString  mimeType;
  nsAutoString  params;
  SplitMimeType(aType, mimeType, params);

  if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase("text/css")) {
    nsIURI* url = nsnull;
#ifdef NECKO    // XXX we need to get passed in the nsILoadGroup here!
//    nsILoadGroup* group = mDocument->GetDocumentLoadGroup();
    result = NS_NewURI(&url, aHref, mDocumentBaseURL/*, group*/);
#else
    nsILoadGroup* LoadGroup = nsnull;
    mDocumentBaseURL->GetLoadGroup(&LoadGroup);
    if (LoadGroup) {
      result = LoadGroup->CreateURL(&url, mDocumentBaseURL, aHref, nsnull);
      NS_RELEASE(LoadGroup);
    }
    else {
      result = NS_NewURL(&url, aHref, mDocumentBaseURL);
    }
#endif
    if (NS_OK != result) {
      return NS_OK; // The URL is bad, move along, don't propogate the error (for now)
    }

    PRBool blockParser = PR_FALSE;
    if (! aAlternate) {
      if (0 < aTitle.Length()) {  // possibly preferred sheet
        if (0 == mPreferredStyle.Length()) {
          mPreferredStyle = aTitle;
          mCSSLoader->SetPreferredSheet(aTitle);
          mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
        }
      }
      else {  // persistent sheet, block
        blockParser = PR_TRUE;
      }
    }

    PRBool doneLoading;
    result = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_Unknown,
                                       mStyleSheetCount++, 
                                      ((blockParser) ? mParser : nsnull),
                                      doneLoading);
    NS_RELEASE(url);
    if (NS_SUCCEEDED(result) && blockParser && (! doneLoading)) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }
  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsAutoString text, target, data;
  nsIContent* node;

  FlushText();

  text = aNode.GetText();
  ParseProcessingInstruction(text, target, data);
  result = NS_NewXMLProcessingInstruction(&node, target, data);
  if (NS_OK == result) {
    node->SetDocument(mDocument, PR_FALSE);
    result = AddContentAsLeaf(node);
  }

  if (NS_OK == result) {
    nsAutoString type, href, title, media, alternate;
    
    // If it's a stylesheet PI...
    if (target.EqualsIgnoreCase(kStyleSheetPI)) {
      result = GetQuotedAttributeValue(text, "href", href);
      // If there was an error or there's no href, we can't do
      // anything with this PI
      if ((NS_OK != result) || (0 == href.Length())) {
        return result;
      }
      
      result = GetQuotedAttributeValue(text, "type", type);
      if (NS_OK != result) {
        return result;
      }
      result = GetQuotedAttributeValue(text, "title", title);
      if (NS_OK != result) {
        return result;
      }
      title.CompressWhitespace();
      result = GetQuotedAttributeValue(text, "media", media);
      if (NS_OK != result) {
        return result;
      }
      media.ToLowerCase();
      result = GetQuotedAttributeValue(text, "alternate", alternate);
      if (NS_OK != result) {
        return result;
      }
#ifndef XSL      
      result = ProcessCSSStyleLink(node, href, alternate.Equals("yes"),
                                title, type, media);
#else
      result = ProcessStyleLink(node, href, alternate.Equals("yes"),
                                title, type, media);
#endif
    }
  }

  return result;
}


NS_IMETHODIMP 
nsXMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  printf("nsXMLContentSink::AddDocTypeDecl\n");
  return NS_OK;
}

nsresult
nsXMLContentSink::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;
  if (0 != mTextLength) {
    if (aCreateTextNode) {
      nsIContent* content;
      rv = NS_NewTextNode(&content);
      if (NS_OK == rv) {
        // Set the content's document
        content->SetDocument(mDocument, PR_FALSE);
        
        // Set the text in the text node
        static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);
        nsITextContent* text = nsnull;
        content->QueryInterface(kITextContentIID, (void**) &text);
        text->SetText(mText, mTextLength, PR_FALSE);
        NS_RELEASE(text);

        // Add text to its parent
        AddContentAsLeaf(content);      
        NS_RELEASE(content);
      }
    }
    mTextLength = 0;
    didFlush = PR_TRUE;
  }
  if (nsnull != aDidFlush) {
    *aDidFlush = didFlush;
  }
  return rv;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

NS_IMETHODIMP 
nsXMLContentSink::AddCharacterData(const nsIParserNode& aNode)
{
  return AddText(aNode.GetText());
}

nsresult
nsXMLContentSink::AddText(const nsString& aString)
{
  PRInt32 addLen = aString.Length();
  if (0 == addLen) {
    return NS_OK;
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * NS_ACCUMULATION_BUFFER_SIZE);
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = NS_ACCUMULATION_BUFFER_SIZE;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  while (0 != addLen) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > addLen) {
      amount = addLen;
    }
    if (0 == amount) {
      if (mConstrainSize) {
        nsresult rv = FlushText();
        if (NS_OK != rv) {
          return rv;
        }
      }
      else {
        mTextSize += addLen;
        mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
        if (nsnull == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    memcpy(&mText[mTextLength], aString.GetUnicode() + offset,
           sizeof(PRUnichar) * amount);
    mTextLength += amount;
    offset += amount;
    addLen -= amount;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::AddUnparsedEntity(const nsIParserNode& aNode)
{
  printf("nsXMLContentSink::AddUnparsedEntity\n");
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::AddNotation(const nsIParserNode& aNode)
{
  printf("nsXMLContentSink::AddNotation\n");
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLContentSink::AddEntityReference(const nsIParserNode& aNode)
{
  printf("nsXMLContentSink::AddEntityReference\n");
  return NS_OK;
}

PRInt32 
nsXMLContentSink::GetNameSpaceId(nsIAtom* aPrefix)
{
  PRInt32 id = (nsnull == aPrefix) ? kNameSpaceID_None : kNameSpaceID_Unknown;
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    nameSpace->FindNameSpaceID(aPrefix, id);
  }

  return id;
}

nsINameSpace*    
nsXMLContentSink::PopNameSpaces()
{
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    mNameSpaceStack->RemoveElementAt(index);
    return nameSpace;
  }

  return nsnull;
}

PRBool  
nsXMLContentSink::IsHTMLNameSpace(PRInt32 aID)
{
  return PRBool(kNameSpaceID_HTML == aID);
}

nsIContent* 
nsXMLContentSink::GetCurrentContent()
{
  if (nsnull != mContentStack) {
    PRInt32 index = mContentStack->Count() - 1;
    return (nsIContent *)mContentStack->ElementAt(index);
  }
  return nsnull;
}

PRInt32 
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  if (nsnull == mContentStack) {
    mContentStack = new nsVoidArray();
  }
  
  mContentStack->AppendElement((void *)aContent);
  return mContentStack->Count();
}
 
nsIContent*
nsXMLContentSink::PopContent()
{
  nsIContent* content = nsnull;
  if (nsnull != mContentStack) {
    PRInt32 index = mContentStack->Count() - 1;
    content = (nsIContent *)mContentStack->ElementAt(index);
    mContentStack->RemoveElementAt(index);
  }
  return content;
}
 
void
nsXMLContentSink::StartLayout()
{
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsRect r;
      cx->GetVisibleArea(r);
      shell->InitialReflow(r.width, r.height);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        RefreshIfEnabled(vm);
      }

      NS_RELEASE(shell);
    }
  }

  // If the document we are loading has a reference or it is a top level
  // frameset document, disable the scroll bars on the views.
#ifdef NECKO
  char* ref = nsnull;
  nsIURL* url;
  nsresult rv = mDocumentURL->QueryInterface(nsIURL::GetIID(), (void**)&url);
  if (NS_SUCCEEDED(rv)) {
    rv = url->GetRef(&ref);
    NS_RELEASE(url);
  }
#else
  const char* ref;
  (void)mDocumentURL->GetRef(&ref);
#endif
  PRBool topLevelFrameset = PR_FALSE;
  if (mWebShell) {
    nsIWebShell* rootWebShell;
    mWebShell->GetRootWebShell(rootWebShell);
    if (mWebShell == rootWebShell) {
      topLevelFrameset = PR_TRUE;
    }
    NS_IF_RELEASE(rootWebShell);
  }

  if ((nsnull != ref) || topLevelFrameset) {
    // XXX support more than one presentation-shell here

    // Get initial scroll preference and save it away; disable the
    // scroll bars.
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsIPresShell* shell = mDocument->GetShellAt(i);
      if (nsnull != shell) {
        nsCOMPtr<nsIViewManager>vm;
        shell->GetViewManager(getter_AddRefs(vm));
        if (vm) {
          nsIView* rootView = nsnull;
          vm->GetRootView(rootView);
          if (nsnull != rootView) {
            nsIScrollableView* sview = nsnull;
            rootView->QueryInterface(kIScrollableViewIID, (void**) &sview);
            if (nsnull != sview) {
              if (topLevelFrameset)
                mOriginalScrollPreference = nsScrollPreference_kNeverScroll;
              else
                sview->GetScrollPreference(mOriginalScrollPreference);
              sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
            }
          }
        }
        NS_RELEASE(shell);
      }
    }
#ifdef NECKO
    // XXX who actually uses ref here anyway?
    nsCRT::free(ref);
#endif
  }
}

NS_IMETHODIMP
nsXMLContentSink::ResumeParsing()
{
  if (nsnull != mParser) {
    mParser->EnableParser(PR_TRUE);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::EvaluateScript(nsString& aScript, PRUint32 aLineNo)
{
  nsresult rv = NS_OK;

  if (0 < aScript.Length()) {
    nsIScriptContextOwner *owner;
    nsIScriptContext *context;
    owner = mDocument->GetScriptContextOwner();
    if (nsnull != owner) {
      
      rv = owner->GetScriptContext(&context);
      if (rv != NS_OK) {
        NS_RELEASE(owner);
        return rv;
      }
        
      nsIURI* docURL = mDocument->GetDocumentURL();
#ifdef NECKO
      char* url;
#else
      const char* url;
#endif
      if (docURL) {
        rv = docURL->GetSpec(&url);
      }

      if (NS_SUCCEEDED(rv)) {
        nsAutoString val;
        PRBool isUndefined;

        PRBool result = context->EvaluateString(aScript, url, aLineNo, 
                                                val, &isUndefined);
      
        NS_IF_RELEASE(docURL);
      
        NS_RELEASE(context);
        NS_RELEASE(owner);
#ifdef NECKO
        nsCRT::free(url);
#endif
      }
    }
  }

  return rv;
}

nsresult
nsXMLContentSink::ProcessEndSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  if (mInScript) {
    nsAutoString script;
    script.SetString(mText, mTextLength);
    result = EvaluateScript(script, mScriptLineNo);
    FlushText(PR_FALSE);
    mInScript = PR_FALSE;
  }

  return result;
}

#define SCRIPT_BUF_SIZE 1024

// XXX Stolen from nsHTMLContentSink. Needs to be shared.
// Returns PR_TRUE if the language name is a version of JavaScript and
// PR_FALSE otherwise
static PRBool
IsJavaScriptLanguage(const nsString& aName)
{
  if (aName.EqualsIgnoreCase("JavaScript") || 
      aName.EqualsIgnoreCase("LiveScript") || 
      aName.EqualsIgnoreCase("Mocha")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.1")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.2")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.3")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.4")) { 
    return PR_TRUE;
  } 
  else { 
    return PR_FALSE;
  } 
}

static void
nsDoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                    nsString& aData,
                    void* aRef,
                    nsresult aStatus)
{
  nsXMLContentSink* sink = (nsXMLContentSink*)aRef;

  if (NS_OK == aStatus) {
    // XXX We have no way of indicating failure. Silently fail?
    sink->EvaluateScript(aData, 0);
  }

  sink->ResumeParsing();

  // The url loader held a reference to the sink
  NS_RELEASE(sink);

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
}

nsresult
nsXMLContentSink::ProcessStartSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRBool   isJavaScript = PR_TRUE;
  PRInt32 i, ac = aNode.GetAttributeCount();

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  for (i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      GetAttributeValueAt(aNode, i, src);
    }
    else if (key.EqualsIgnoreCase("type")) {
      nsAutoString  type;
      
      GetAttributeValueAt(aNode, i, type);
      nsAutoString  mimeType;
      nsAutoString  params;
      SplitMimeType(type, mimeType, params);

      isJavaScript = mimeType.EqualsIgnoreCase("text/javascript");
    }
    else if (key.EqualsIgnoreCase("language")) {
      nsAutoString  lang;
      
      GetAttributeValueAt(aNode, i, lang);
      isJavaScript = IsJavaScriptLanguage(lang);
    }
  }
  
  // Don't process scripts that aren't JavaScript
  if (isJavaScript) {
    nsAutoString script;

    // If there is a SRC attribute...
    if (src.Length() > 0) {
      // Use the SRC attribute value to load the URL
      nsIURI* url = nsnull;
      nsAutoString absURL;
#ifdef NECKO    // XXX we need to get passed in the nsILoadGroup here!
//      nsILoadGroup* group = mDocument->GetDocumentLoadGroup();
      rv = NS_NewURI(&url, src, mDocumentBaseURL);
#else
      nsIURI* docURL = mDocument->GetDocumentURL();
      nsILoadGroup* group = nsnull;
      rv = docURL->GetLoadGroup(&group);
      if ((NS_OK == rv) && group) {
        rv = group->CreateURL(&url, docURL, src, nsnull);
        NS_RELEASE(group);
      }
      else {
        rv = NS_NewURL(&url, absURL);
      }
      NS_RELEASE(docURL);
#endif
      if (NS_OK != rv) {
        return rv;
      }

      // Add a reference to this since the url loader is holding
      // onto it as opaque data.
      NS_ADDREF(this);

      nsIUnicharStreamLoader* loader;
      rv = NS_NewUnicharStreamLoader(&loader,
                                     url, 
#ifdef NECKO
                                     nsCOMPtr<nsILoadGroup>(mDocument->GetDocumentLoadGroup()),
#endif
                                     (nsStreamCompleteFunc)nsDoneLoadingScript, 
                                     (void *)this);
      NS_RELEASE(url);
      if (NS_OK == rv) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
    else {
      // Wait until we get the script content
      mInScript = PR_TRUE;
      mConstrainSize = PR_FALSE;
      mScriptLineNo = (PRUint32)aNode.GetSourceLineNumber();
    }
  }

  return rv;
}

nsresult
nsXMLContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (vm) {
    nsIContentViewer* contentViewer = nsnull;
    nsresult rv = mWebShell->GetContentViewer(&contentViewer);
    if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
      PRBool enabled;
      contentViewer->GetEnableRendering(&enabled);
      if (enabled) {
        vm->EnableRefresh();
      }
      NS_RELEASE(contentViewer); 
    }
  }
  return NS_OK;
}
