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

#include "nsXMLContentSink.h"
#include "nsIParser.h"
#include "nsIUnicharInputStream.h"
#include "nsIDocument.h"
#include "nsIXMLDocument.h"
#include "nsIXMLContent.h"
#include "nsIScriptObjectOwner.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMComment.h"
#include "nsIHTMLContent.h"
#include "nsHTMLEntities.h" 
#include "nsHTMLParts.h" 
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsHTMLAtoms.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "prtime.h"
#include "prlog.h"
#include "prmem.h"

static char kNameSpaceSeparator[] = ":";
static char kNameSpaceDef[] = "xmlns";
static char kStyleSheetPI[] = "<?xml-stylesheet";
static char kCSSType[] = "text/css";
static char kQuote = '\"';
static char kApostrophe = '\'';

static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

#define XML_PSEUDO_ELEMENT  0

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
                     nsIURL* aURL,
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
}

nsXMLContentSink::~nsXMLContentSink()
{
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mDocumentURL);
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
}

nsresult
nsXMLContentSink::Init(nsIDocument* aDoc,
                       nsIURL* aURL,
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
  mWebShell = aContainer;
  NS_ADDREF(aContainer);

  mState = eXMLContentSinkState_InProlog;
  mDocElement = nsnull;
  mRootElement = nsnull;

  return NS_OK;
}

// nsISupports
NS_IMPL_ISUPPORTS(nsXMLContentSink, kIXMLContentSinkIID)

  // nsIContentSink
NS_IMETHODIMP 
nsXMLContentSink::WillBuildModel(void)
{
  // Notify document that the load is beginning
  mDocument->BeginLoad();
  nsresult result = NS_OK;

#if XML_PSEUDO_ELEMENT
  // XXX Create a pseudo root element. This is a parent of the
  // document element. For now, it will be seen in the document
  // hierarchy. In the future we might want to get rid of it
  // or at least make it invisible from the perspective of the
  // DOM.
  nsIAtom *tagAtom = NS_NewAtom("XML");
  nsIXMLContent *content;
  result = NS_NewXMLElement(&content, tagAtom);
  NS_RELEASE(tagAtom);
  // For XML elements, set the namespace
  if (NS_OK == result) {
    content->SetNameSpaceIdentifier(kNameSpaceID_None);
    content->SetDocument(mDocument, PR_FALSE);

    mRootElement = content;
    NS_ADDREF(mRootElement);
    PushContent(content);

    mDocument->SetRootContent(mRootElement);
  }
#endif

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  // XXX this is silly; who cares?
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      nsIViewManager* vm = shell->GetViewManager();
      if(vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
      NS_RELEASE(vm);
      NS_RELEASE(shell);
    }
  }

#if XML_PSEUDO_ELEMENT
  // Pop the pseudo root content
  PopContent();
#endif

  StartLayout();

#if 0
  // XXX For now, we don't do incremental loading. We wait
  // till the end to flow the entire document.
  if (nsnull != mDocElement) {
    mDocument->ContentAppended(mDocElement, 0);
  }
#endif

  // XXX Should scroll to ref when that makes sense
  // ScrollToRef();

  mDocument->EndLoad();
  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);
  return NS_OK;
}

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
  if ((first == '"') || (first == '\'')) {
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
        PRInt32 ch = NS_EntityToUnicode(cbuf);
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
static nsresult
AddAttributes(const nsIParserNode& aNode,
              nsIContent* aContent,
              PRBool aIsHTML)
{
  // Add tag attributes to the content attributes
  nsAutoString k, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsString& key = aNode.GetKeyAt(i);
    k.Truncate();
    k.Append(key);
    if (aIsHTML) {
      k.ToUpperCase();
    }

    // XXX TODO Currently we don't look for namespace identifiers
    // in the attribute name

    nsAutoString value;
    if (NS_CONTENT_ATTR_NOT_THERE == 
        aContent->GetAttribute(k, value)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);

      // Add attribute to content
      aContent->SetAttribute(k, v, PR_FALSE);
    }
  }
  return NS_OK;
}

void
nsXMLContentSink::PushNameSpacesFrom(const nsIParserNode& aNode)
{
  nsAutoString k, uri, prefix;
  PRInt32 ac = aNode.GetAttributeCount();
  PRInt32 offset;
  nsresult result = NS_OK;
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
        PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
        // If the next character is a :, there is a namespace prefix
        if (':' == next) {
          k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
        }
        else {
          prefix.Truncate();
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
  PRInt32 nsoffset = aString.Find(kNameSpaceSeparator);
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
    tag.ToUpperCase();  // HTML is case-insensative
    nsIAtom* tagAtom = NS_NewAtom(tag);
    if (nsHTMLAtoms::script == tagAtom) {
      result = ProcessStartSCRIPTTag(aNode);
    }
    // XXX Treat the form elements as a leaf element (even if it is a
    // container). Need to do further processing with forms
    else if (nsHTMLAtoms::form == tagAtom) {
      pushContent = PR_FALSE;
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
        mDocument->SetRootContent(mDocElement);
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
    tag.ToUpperCase();
    nsIAtom* tagAtom = NS_NewAtom(tag);
    if (nsHTMLAtoms::script == tagAtom) {
      result = ProcessEndSCRIPTTag(aNode);
    }
    // XXX Form content was never pushed on the stack
    else if (nsHTMLAtoms::form == tagAtom) {
      popContent = PR_FALSE;
    }
    NS_RELEASE(tagAtom);
  }

  if (popContent) {
    nsIContent* content = PopContent();
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
  PopNameSpaces();

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  // XXX For now, all leaf content is character data
  // XXX make sure to push/pop name spaces here too (for attributes)
  AddCharacterData(aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::NotifyError(nsresult aErrorResult)
{
  printf("nsXMLContentSink::NotifyError\n");
  return NS_OK;
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
  nsIXMLDocument *xmlDoc;
  nsresult result = mDocument->QueryInterface(kIXMLDocumentIID, 
                                              (void **)&xmlDoc);

  if (eXMLContentSinkState_InProlog == mState) {
    result = xmlDoc->AppendToProlog(aContent);
  }
  else if (eXMLContentSinkState_InEpilog == mState) {   
    result = xmlDoc->AppendToEpilog(aContent);
  }
  else {
    nsIContent *parent = GetCurrentContent();
    
    if (nsnull != parent) {
      result = parent->AppendChildTo(aContent, PR_FALSE);
    }
  }
  
  NS_RELEASE(xmlDoc);
  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddComment(const nsIParserNode& aNode)
{
  FlushText();

  nsAutoString text;
  nsIHTMLContent *comment;
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

// XXX Borrowed from HTMLContentSink. Should be shared.
nsresult
nsXMLContentSink::LoadStyleSheet(nsIURL* aURL,
                                 nsIUnicharInputStream* aUIN)
{
  /* XXX use repository */
  nsICSSParser* parser;
  nsresult rv = NS_NewCSSParser(&parser);
  if (NS_OK == rv) {
    nsICSSStyleSheet* sheet = nsnull;
    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->SetCaseSensative(PR_TRUE);
    parser->Parse(aUIN, aURL, sheet);
    if (nsnull != sheet) {
      mDocument->AddStyleSheet(sheet);
      NS_RELEASE(sheet);
      rv = NS_OK;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;/* XXX */
    }
    NS_RELEASE(parser);
  }
  return rv;
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
    offset = aSource.Find('=', offset);

    PRUnichar next = aSource.CharAt(++offset);
    if (kQuote == next) {
      endOffset = aSource.Find(kQuote, ++offset);
    }
    else if (kApostrophe == next) {
      endOffset = aSource.Find(kApostrophe, ++offset);	  
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

NS_IMETHODIMP 
nsXMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  FlushText();

  // XXX For now, we don't add the PI to the content model.
  // We just check for a style sheet PI
  nsAutoString text, type, href;
  PRInt32 offset;
  nsresult result = NS_OK;

  text = aNode.GetText();

  offset = text.Find(kStyleSheetPI);
  // If it's a stylesheet PI...
  if (0 == offset) {
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
    
    if (type.Equals(kCSSType)) {
      nsIURL* url = nsnull;
      nsIUnicharInputStream* uin = nsnull;
      nsAutoString absURL;
      nsIURL* docURL = mDocument->GetDocumentURL();
      nsAutoString emptyURL;
      emptyURL.Truncate();
      result = NS_MakeAbsoluteURL(docURL, emptyURL, href, absURL);
      if (NS_OK != result) {
        return result;
      }
      NS_RELEASE(docURL);
      result = NS_NewURL(&url, nsnull, absURL);
      if (NS_OK != result) {
        return result;
      }
      PRInt32 ec;
      nsIInputStream* iin = url->Open(&ec);
      if (nsnull == iin) {
        NS_RELEASE(url);
        return (nsresult) ec;/* XXX fix url->Open */
      }
      result = NS_NewConverterStream(&uin, nsnull, iin);
      NS_RELEASE(iin);
      if (NS_OK != result) {
        NS_RELEASE(url);
        return result;
      }
      
      result = LoadStyleSheet(url, uin);
      NS_RELEASE(uin);
      NS_RELEASE(url);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
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
      nsIHTMLContent* content;
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

NS_IMETHODIMP 
nsXMLContentSink::AddCharacterData(const nsIParserNode& aNode)
{
  nsAutoString text = aNode.GetText();

  PRInt32 addLen = text.Length();
  if (0 == addLen) {
    return NS_OK;
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * 4096);
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = 4096;
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
    memcpy(&mText[mTextLength], text.GetUnicode() + offset,
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
  PRInt32 id = kNameSpaceID_Unknown;
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    nameSpace->FindNameSpaceID(aPrefix, id);
  }

  return id;
}

void    
nsXMLContentSink::PopNameSpaces()
{
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    mNameSpaceStack->RemoveElementAt(index);
    NS_RELEASE(nameSpace);
  }
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
      nsIPresContext* cx = shell->GetPresContext();
      nsRect r;
      cx->GetVisibleArea(r);
      shell->InitialReflow(r.width, r.height);
      NS_RELEASE(cx);

      // Now trigger a refresh
      nsIViewManager* vm = shell->GetViewManager();
      if (nsnull != vm) {
        vm->EnableRefresh();
        NS_RELEASE(vm);
      }

      NS_RELEASE(shell);
    }
  }

  // If the document we are loading has a reference or it is a top level
  // frameset document, disable the scroll bars on the views.
  const char* ref = mDocumentURL->GetRef();
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
        nsIViewManager* vm = shell->GetViewManager();
        if (nsnull != vm) {
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
          NS_RELEASE(vm);
        }
        NS_RELEASE(shell);
      }
    }
  }
}

nsresult
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
        
      jsval val;
      nsIURL* mDocURL = mDocument->GetDocumentURL();
      const char* mURL;
      if (mDocURL) {
        mURL = mDocURL->GetSpec();
      }
      
      PRBool result = context->EvaluateString(aScript, mURL, aLineNo, &val);
      
      NS_IF_RELEASE(mDocURL);
      
      NS_RELEASE(context);
      NS_RELEASE(owner);
    }
  }

  return NS_OK;
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
      src = aNode.GetValueAt(i);
      src.Trim("\"", PR_TRUE, PR_TRUE); 
    }
    else if (key.EqualsIgnoreCase("type")) {
      nsAutoString  type;
      
      GetAttributeValueAt(aNode, i, type);
      isJavaScript = type.EqualsIgnoreCase("text/javascript");
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
    
    // If there is a SRC attribute, (for now) read from the
    // stream synchronously and hold the data in a string.
    if (src != "") {
      // Use the SRC attribute value to open a blocking stream
      nsIURL* url = nsnull;
      nsAutoString absURL;
      nsIURL* docURL = mDocument->GetDocumentURL();
      nsAutoString emptyURL;
      emptyURL.Truncate();
      rv = NS_MakeAbsoluteURL(docURL, emptyURL, src, absURL);
      if (NS_OK != rv) {
        return rv;
      }
      NS_RELEASE(docURL);
      rv = NS_NewURL(&url, nsnull, absURL);
      if (NS_OK != rv) {
        return rv;
      }
      PRInt32 ec;
      nsIInputStream* iin = url->Open(&ec);
      if (nsnull == iin) {
        NS_RELEASE(url);
        return (nsresult) ec;/* XXX fix url->Open */
      }
      
      // Drain the stream by reading from it a chunk at a time
      PRInt32 nb;
      nsresult err;
      do {
        char buf[SCRIPT_BUF_SIZE];
        
        err = iin->Read(buf, 0, SCRIPT_BUF_SIZE, &nb);
        if (NS_OK == err) {
          script.Append((const char *)buf, nb);
        }
      } while (err == NS_OK);
      
      if (NS_BASE_STREAM_EOF != err) {
          rv = NS_ERROR_FAILURE;
      }
      
      NS_RELEASE(iin);
      NS_RELEASE(url);
      
      rv = EvaluateScript(script, (PRUint32)aNode.GetSourceLineNumber());
      
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
