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
#include "nsIStyleSheet.h"
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
#include "prtime.h"
#include "prlog.h"

static char kNameSpaceSeparator[] = ":";
static char kNameSpaceDef[] = "xmlns";
static char kHTMLNameSpaceURI[] = "http://www.w3.org/TR/REC-html40";
static char kStyleSheetPI[] = "<?xml-stylesheet";
static char kCSSType[] = "text/css";
static char kQuote = '\"';
static char kApostrophe = '\'';

static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

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
  mRootElement = nsnull;
  mDocElement = nsnull;
  mNameSpaces = nsnull;
  mNestLevel = 0;
  mContentStack = nsnull;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
}

nsXMLContentSink::~nsXMLContentSink()
{
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mRootElement);
  NS_IF_RELEASE(mDocElement);
  if (nsnull != mNameSpaces) {
    // There shouldn't be any here except in an error condition
    PRInt32 i, count = mNameSpaces->Count();
    
    for (i=0; i < count; i++) {
      NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
      
      if (nsnull != ns) {
        NS_IF_RELEASE(ns->mPrefix);
        delete ns;
      }
    }
    delete mNameSpaces;
  }
  if (nsnull != mContentStack) {
    delete mContentStack;
  }
  if (nsnull != mText) {
    delete [] mText;
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
    content->SetNameSpaceIdentifier(gNameSpaceId_Unknown);
    content->SetDocument(mDocument, PR_FALSE);

    mRootElement = content;
    NS_ADDREF(mRootElement);
    PushContent(content);

    mDocument->SetRootContent(mRootElement);
  }

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

  // Pop the pseudo root content
  PopContent();

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
nsXMLContentSink::FindNameSpaceAttributes(const nsIParserNode& aNode)
{
  nsAutoString k, uri, prefix;
  PRInt32 ac = aNode.GetAttributeCount();
  PRInt32 offset;
  nsresult result = NS_OK;

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

      // Get the attribute value (the URI for the namespace)
      GetAttributeValueAt(aNode, i, uri);
      
      // Open a local namespace
      OpenNameSpace(prefix, uri);
    }
  }
}

NS_IMETHODIMP 
nsXMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsAutoString tag, nameSpace;
  PRInt32 nsoffset;
  PRInt32 id = gNameSpaceId_Unknown;
  PRBool isHTML = nsnull;
  nsIContent *content;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the epilog, there should be no
  // new elements
  PR_ASSERT(eXMLContentSinkState_InEpilog != mState);

  FlushText();

  mState = eXMLContentSinkState_InDocumentElement;

  tag = aNode.GetText();
  nsoffset = tag.Find(kNameSpaceSeparator);
  if (-1 != nsoffset) {
    tag.Left(nameSpace, nsoffset);
    tag.Cut(0, nsoffset+1);
  }
  else {
    nameSpace.Truncate();
  }

  // We must register namespace declarations found in the attribute list
  // of an element before creating the element. This is because the
  // namespace prefix for an element might be declared within the attribute
  // list.
  FindNameSpaceAttributes(aNode);

  id = GetNameSpaceId(nameSpace);
  isHTML = IsHTMLNameSpace(id);

  // XXX We have to uppercase the tag since the CSS system does the
  // same. We need to teach the CSS system to be case insensitive.
  tag.ToUpperCase();
  if (isHTML) {
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
      if (0 < nameSpace.Length()) {
        nsIAtom *nameSpaceAtom = NS_NewAtom(nameSpace);
        xmlContent->SetNameSpace(nameSpaceAtom);
        NS_RELEASE(nameSpaceAtom);
      }
      xmlContent->SetNameSpaceIdentifier(id);
    }
    content = (nsIContent *)xmlContent;
  }

  if (NS_OK == result) {
    content->SetDocument(mDocument, PR_FALSE);

    // Set the attributes on the new content element
    result = AddAttributes(aNode, content, isHTML);
    if (NS_OK == result) {
      // If this is the document element
      if (nsnull == mDocElement) {
        mDocElement = content;
        NS_ADDREF(mDocElement);
      }

      nsIContent *parent = GetCurrentContent();
      
      parent->AppendChildTo(content, PR_FALSE);
      PushContent(content);
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);
  
  FlushText();

  nsIContent* content = PopContent();
  if (nsnull != content) {
    PRInt32 nestLevel = GetCurrentNestLevel();

    CloseNameSpacesAtNestLevel(nestLevel);

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

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  // XXX For now, all leaf content is character data
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
                                 nsIUnicharInputStream* aUIN,
                                 PRBool aInline)
{
  /* XXX use repository */
  nsICSSParser* parser;
  nsresult rv = NS_NewCSSParser(&parser);
  if (NS_OK == rv) {
    if (aInline && (nsnull != mStyleSheet)) {
      parser->SetStyleSheet(mStyleSheet);
      // XXX we do probably need to trigger a style change reflow
      // when we are finished if this is adding data to the same sheet
    }
    nsIStyleSheet* sheet = nsnull;
    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->Parse(aUIN, aURL, sheet);
    if (nsnull != sheet) {
      if (aInline) {
        if (nsnull == mStyleSheet) {
          // Add in the sheet the first time; if we update the sheet
          // with new data (mutliple style tags in the same document)
          // then the sheet will be updated by the css parser and
          // therefore we don't need to add it to the document)
          mDocument->AddStyleSheet(sheet);
          mStyleSheet = sheet;
        }
      }
      else {
        mDocument->AddStyleSheet(sheet);
      }
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
      
      result = LoadStyleSheet(url, uin, PR_FALSE);
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
nsXMLContentSink::FlushText(PRBool* aDidFlush)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;
  if (0 != mTextLength) {
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
    mText = new PRUnichar[4096];
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
      nsresult rv = FlushText();
      if (NS_OK != rv) {
        return rv;
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
nsXMLContentSink::OpenNameSpace(const nsString& aPrefix, const nsString& aURI)
{
  nsIAtom *nameSpaceAtom = nsnull;
  PRInt32 id = gNameSpaceId_Unknown;

  nsIXMLDocument *xmlDoc;
  nsresult result = mDocument->QueryInterface(kIXMLDocumentIID, 
                                              (void **)&xmlDoc);
  if (NS_OK != result) {
    return id;
  }

  if (0 < aPrefix.Length()) {
    nameSpaceAtom = NS_NewAtom(aPrefix);
  }
  
  result = xmlDoc->RegisterNameSpace(nameSpaceAtom, aURI, id);
  if (NS_OK == result) {
    NameSpaceStruct *ns;
    
    ns = new NameSpaceStruct;
    if (nsnull != ns) {
      ns->mPrefix = nameSpaceAtom;
      NS_IF_ADDREF(nameSpaceAtom);
      ns->mId = id;
      ns->mNestLevel = GetCurrentNestLevel();
      
      if (nsnull == mNameSpaces) {
        mNameSpaces = new nsVoidArray();
      }
      // XXX Should check for duplication
      mNameSpaces->AppendElement((void *)ns);
    }
  }

  NS_IF_RELEASE(nameSpaceAtom);
  NS_RELEASE(xmlDoc);

  return id;
}

PRInt32 
nsXMLContentSink::GetNameSpaceId(const nsString& aPrefix)
{
  nsIAtom *nameSpaceAtom = nsnull;
  PRInt32 id = gNameSpaceId_Unknown;
  PRInt32 i, count;
  
  if (nsnull == mNameSpaces) {
    return id;
  }

  if (0 < aPrefix.Length()) {
    nameSpaceAtom = NS_NewAtom(aPrefix);
  }

  count = mNameSpaces->Count();
  for (i = 0; i < count; i++) {
    NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
    
    if ((nsnull != ns) && (ns->mPrefix == nameSpaceAtom)) {
      id = ns->mId;
      break;
    }
  }

  NS_IF_RELEASE(nameSpaceAtom);

  return id;
}

void    
nsXMLContentSink::CloseNameSpacesAtNestLevel(PRInt32 mNestLevel)
{
  PRInt32 nestLevel = GetCurrentNestLevel();

  if (nsnull == mNameSpaces) {
    return;
  }

  PRInt32 i, count;
  count = mNameSpaces->Count();
  // Go backwards so that we can delete as we go along
  for (i = count; i >= 0; i--) {
    NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
    
    if ((nsnull != ns) && (ns->mNestLevel == nestLevel)) {
      NS_IF_RELEASE(ns->mPrefix);
      mNameSpaces->RemoveElementAt(i);
      delete ns;
    }
  }
}

PRBool  
nsXMLContentSink::IsHTMLNameSpace(PRInt32 aId)
{
  nsAutoString uri;
  PRBool isHTML = PR_FALSE;
  nsIXMLDocument *xmlDoc;
  nsresult result = mDocument->QueryInterface(kIXMLDocumentIID, 
                                              (void **)&xmlDoc);
  if (NS_OK != result) {
    return isHTML;
  }

  result = xmlDoc->GetNameSpaceURI(aId, uri);
  if ((NS_OK == result) &&
      (uri.Equals(kHTMLNameSpaceURI))) {
    isHTML = PR_TRUE;
  }
  
  NS_RELEASE(xmlDoc);
  return isHTML;
}

nsIContent* 
nsXMLContentSink::GetCurrentContent()
{
  if ((nsnull == mContentStack) ||
      (0 == mNestLevel)) {
    return nsnull;
  }
  
  return (nsIContent *)mContentStack->ElementAt(mNestLevel-1);
}

PRInt32 
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  if (nsnull == mContentStack) {
    mContentStack = new nsVoidArray();
  }
  
  mContentStack->AppendElement((void *)aContent);
  return ++mNestLevel;
}
 
nsIContent*
nsXMLContentSink::PopContent()
{
  nsIContent *content;
  if ((nsnull == mContentStack) ||
      (0 == mNestLevel)) {
    return nsnull;
  }
  
  --mNestLevel;
  content = (nsIContent *)mContentStack->ElementAt(mNestLevel);
  mContentStack->RemoveElementAt(mNestLevel);

  return content;
}
 
PRInt32 
nsXMLContentSink::GetCurrentNestLevel()
{
  return mNestLevel;
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
