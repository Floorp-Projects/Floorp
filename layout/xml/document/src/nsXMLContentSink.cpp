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
#include "nsIURLGroup.h"
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
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "prtime.h"
#include "prlog.h"
#include "prmem.h"

// XXX misnamed header file, but oh well
#include "nsHTMLTokens.h"  

static char kNameSpaceSeparator[] = ":";
static char kNameSpaceDef[] = "xmlns";
static char kStyleSheetPI[] = "xml-stylesheet";
static char kCSSType[] = "text/css";

#ifdef XSL
static char kXSLType[] = "text/xsl";
#endif


static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMCDATASectionIID, NS_IDOMCDATASECTION_IID);

static void SetTextStringOnTextNode(const nsString& aTextString, nsIContent* aTextNode);

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

#ifdef XSL
  mXSLState.sheetExists = PR_FALSE;
  mXSLState.sink = nsnull;
#endif
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
  nsIAtom *tagAtom = NS_NewAtom("xml");
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
    nsCOMPtr<nsIPresShell> shell( dont_AddRef(mDocument->GetShellAt(i)) );
    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if(vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
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
    PRInt32 nameSpaceID = GetNameSpaceId(nameSpacePrefix);
    if ((kNameSpaceID_XMLNS == nameSpaceID) && aIsHTML) {
      NS_RELEASE(nameAtom);
      name.Insert("xmlns:", 0);
      nameAtom = NS_NewAtom(name);
      nameSpaceID = kNameSpaceID_HTML;
    }
    else if ((kNameSpaceID_None == nameSpaceID) && aIsHTML) {
      nameSpaceID = kNameSpaceID_HTML;
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
  for (int i = 0; i < errorPosition - 1; i++)
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
  result = NS_CreateHTMLElement(&errorContainerNode, parserErrorTag);
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

// XXX Borrowed from HTMLContentSink. Should be shared.
NS_IMETHODIMP
nsXMLContentSink::LoadStyleSheet(nsIURL* aURL,
                                 nsIUnicharInputStream* aUIN,
                                 PRBool aActive,
                                 const nsString& aTitle,
                                 const nsString& aMedia,
                                 nsIContent* aOwner)
{
  /* XXX use repository */
  nsICSSParser* parser;
  nsresult rv = NS_NewCSSParser(&parser);
  if (NS_OK == rv) {
    nsICSSStyleSheet* sheet = nsnull;
    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->SetCaseSensitive(PR_TRUE);
    parser->Parse(aUIN, aURL, sheet);
    if (nsnull != sheet) {
      sheet->SetTitle(aTitle);
      sheet->SetEnabled(aActive);
      mDocument->AddStyleSheet(sheet);
      if (nsnull != aOwner) {
        nsIDOMNode* domNode = nsnull;
        if (NS_SUCCEEDED(aOwner->QueryInterface(kIDOMNodeIID, (void**)&domNode))) {
          sheet->SetOwningNode(domNode);
          NS_RELEASE(domNode);
        }
      }
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

typedef struct {
  nsString mTitle;
  nsString mMedia;
  PRBool mIsActive;
  nsIURL* mURL;
  nsIContent* mElement;
  nsXMLContentSink* mSink;
} nsAsyncStyleProcessingDataXML;

static void
nsDoneLoadingStyle(nsIUnicharStreamLoader* aLoader,
                   nsString& aData,
                   void* aRef,
                   nsresult aStatus)
{
  nsresult rv = NS_OK;
  nsAsyncStyleProcessingDataXML* d = (nsAsyncStyleProcessingDataXML*)aRef;
  nsIUnicharInputStream* uin = nsnull;

  if ((NS_OK == aStatus) && (0 < aData.Length())) {
    // wrap the string with the CSS data up in a unicode
    // input stream.
    rv = NS_NewStringUnicharInputStream(&uin, new nsString(aData));
    if (NS_OK == rv) {
      // XXX We have no way of indicating failure. Silently fail?
      rv = d->mSink->LoadStyleSheet(d->mURL, uin, d->mIsActive, 
                                    d->mTitle, d->mMedia, d->mElement);
    }
  }
    
  d->mSink->ResumeParsing();

  NS_RELEASE(d->mURL);
  NS_IF_RELEASE(d->mElement);
  NS_RELEASE(d->mSink);
  delete d;

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
}

#ifdef XSL
nsresult
nsXMLContentSink::CreateStyleSheetURL(nsIURL** aUrl, 
                                      const nsAutoString& aHref)
{
  nsAutoString absURL;
  nsIURL* docURL = mDocument->GetDocumentURL();
  nsIURLGroup* urlGroup; 
  nsresult result = NS_OK;
  
  result = docURL->GetURLGroup(&urlGroup);

  if ((NS_SUCCEEDED(result)) && urlGroup) {
    result = urlGroup->CreateURL(aUrl, docURL, aHref, nsnull);
    NS_RELEASE(urlGroup);
  }
  else {
    result = NS_MakeAbsoluteURL(docURL, nsnull, aHref, absURL);
    if (NS_SUCCEEDED(result)) {
      result = NS_NewURL(aUrl, absURL);
    }
  }
  NS_RELEASE(docURL);
  return result;
}


// Create an XML parser and an XSL content sink and start parsing
// the XSL stylesheet located at the given URL.
nsresult
nsXMLContentSink::LoadXSLStyleSheet(const nsIURL* aUrl)
{  
  nsresult rv = NS_OK;

  static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
  static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

  // Create the XML parser
  rv = nsComponentManager::CreateInstance(kCParserCID, 
                                    nsnull, 
                                    kCParserIID, 
                                    (void **)&parser);
  if (NS_SUCCEEDED(rv)) {
    nsIXSLContentSink* sink;
    
    // Create the XSL content sink
    rv = NS_NewXSLContentSink(&sink, mDocument, aUrl, mWebShell);
    if (NS_OK == rv) {
      // Set up XSL state in the XML content sink
      mXSLState.sheetExists = PR_TRUE;
      mXSLState.sink = sink;

      // Hook up the content sink to the parser's output and ask the parser
      // to start parsing the URL specified by aURL.
      nsIDTD* theDTD=0;
      NS_NewWellFormed_DTD(&theDTD);
      parser->RegisterDTD(theDTD);
      parser->SetContentSink(sink);
      parser->Parse(aUrl);
      
      // XXX Don't we have to NS_RELEASE() theDTD?
      NS_RELEASE(sink);
    }
  }
  return rv;
}
#endif

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

#ifndef XSL
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
    nsAutoString type, href, title, media;
    
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
      
      if (type.Equals(kCSSType)) {
        // Use the SRC attribute value to load the URL
        nsIURL* url = nsnull;
        nsAutoString absURL;
        nsIURL* docURL = mDocument->GetDocumentURL();
        nsIURLGroup* urlGroup; 
        
        result = docURL->GetURLGroup(&urlGroup);
        
        if ((NS_OK == result) && urlGroup) {
          result = urlGroup->CreateURL(&url, docURL, href, nsnull);
          NS_RELEASE(urlGroup);
        }
        else {
          result = NS_NewURL(&url, absURL);
        }
        NS_RELEASE(docURL);
        if (NS_OK != result) {
          return result;
        }
        
        nsAsyncStyleProcessingDataXML* d = new nsAsyncStyleProcessingDataXML;
        if (nsnull == d) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        d->mTitle=title;
        d->mMedia=media;
        d->mIsActive = PR_TRUE;
        d->mURL = url;
        NS_ADDREF(url);
        // XXX Need to create PI node
        d->mElement = nsnull;
        d->mSink = this;
        NS_ADDREF(this);
        
        nsIUnicharStreamLoader* loader;
        result = NS_NewUnicharStreamLoader(&loader,
                                           url, 
                                           (nsStreamCompleteFunc)nsDoneLoadingStyle, 
                                           (void *)d);
        NS_RELEASE(url);
        if (NS_OK == result) {
          result = NS_ERROR_HTMLPARSER_BLOCK;
        }
      }
    }
  }

  return result;
}
#else
/* The version of AddProcessingInstruction down below is being hacked on for XSL...
   Please make changes to the version above this comment.  
   I'll merge the changes when I un-ifdef stuff.

NS_IMETHODIMP 
nsXMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  nsIURL* url = nsnull;
  FlushText();

  // XXX For now, we don't add the PI to the content model.
  // We just check for a style sheet PI
  nsAutoString text, type, href, title, media;
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
    result = GetQuotedAttributeValue(text, "title", title);
    if (NS_OK != result) {
      return result;
    }
    title.CompressWhitespace();
    result = GetQuotedAttributeValue(text, "media", media);
    if (NS_OK != result) {
      return result;
    }

    // XXX At some point, we need to have a registry based mechanism
    // for dealing with loading stylesheets attached to XML documents
    if (type.Equals(kCSSType) || type.Equals(kXSLType)) {
      result = CreateStylesheetURL(&url, href);
      if (NS_OK != result) {
        return result;
      }
    }

    if (type.Equals(kCSSType)) {
      nsAsyncStyleProcessingDataXML* d = new nsAsyncStyleProcessingDataXML;
      if (nsnull == d) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      d->mTitle.SetString(title);
      d->mMedia.SetString(media);
      d->mIsActive = PR_TRUE;
      d->mURL = url;
      NS_ADDREF(url);
      // XXX Need to create PI node
      d->mElement = nsnull;
      d->mSink = this;
      NS_ADDREF(this);

      nsIUnicharStreamLoader* loader;
      result = NS_NewUnicharStreamLoader(&loader,
                                         url, 
                                         (nsStreamCompleteFunc)nsDoneLoadingStyle, 
                                         (void *)d);
      if (NS_SUCCEEDED(result)) {
        result = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
    else if (type.Equals(kXSLType)) {
      result = LoadXSLStyleSheet(url);
    }

    if (type.Equals(kCSSType) || type.Equals(kXSLType)) {
      NS_RELEASE(url);
    }
  }
                     
  return result;
}
*/
#endif


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
        vm->EnableRefresh();
      }

      NS_RELEASE(shell);
    }
  }

  // If the document we are loading has a reference or it is a top level
  // frameset document, disable the scroll bars on the views.
  const char* ref;
  (void)mDocumentURL->GetRef(&ref);
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
        
      nsIURL* docURL = mDocument->GetDocumentURL();
      const char* url;
      if (docURL) {
         (void)docURL->GetSpec(&url);
      }

      nsAutoString val;
      PRBool isUndefined;

      PRBool result = context->EvaluateString(aScript, url, aLineNo, 
                                              val, &isUndefined);
      
      NS_IF_RELEASE(docURL);
      
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

    // If there is a SRC attribute...
    if (src.Length() > 0) {
      // Use the SRC attribute value to load the URL
      nsIURL* url = nsnull;
      nsAutoString absURL;
      nsIURL* docURL = mDocument->GetDocumentURL();
      nsIURLGroup* urlGroup;

      rv = docURL->GetURLGroup(&urlGroup);
      
      if ((NS_OK == rv) && urlGroup) {
        rv = urlGroup->CreateURL(&url, docURL, src, nsnull);
        NS_RELEASE(urlGroup);
      }
      else {
        rv = NS_NewURL(&url, absURL);
      }
      NS_RELEASE(docURL);
      if (NS_OK != rv) {
        return rv;
      }

      // Add a reference to this since the url loader is holding
      // onto it as opaque data.
      NS_ADDREF(this);

      nsIUnicharStreamLoader* loader;
      rv = NS_NewUnicharStreamLoader(&loader,
                                     url, 
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
