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
#include "nsIFrameUtil.h"
#include "nsWellFormedDTD.h"
#include "nsParserCIID.h"
#include "nsIContent.h"
#include "nsIParser.h"
#include "nsIXMLContentSink.h"
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsHTMLEntities.h" 
#include "stdlib.h"

static NS_DEFINE_IID(kIFrameUtilIID, NS_IFRAME_UTIL_IID);
static NS_DEFINE_IID(kIXMLContentIID, NS_IXMLCONTENT_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID, NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

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
AddAttributes(const nsIParserNode& aNode, nsIContent* aContent)
{
  // Add tag attributes to the content attributes
  nsAutoString k, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsString& key = aNode.GetKeyAt(i);
    k.Truncate();
    k.Append(key);

    nsAutoString value;
    if (NS_CONTENT_ATTR_NOT_THERE == aContent->GetAttribute(k, value)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);

      // Add attribute to content
      aContent->SetAttribute(k, v, PR_FALSE);
    }
  }
  return NS_OK;
}

static nsresult
NewElement(const nsIParserNode& aNode, nsIXMLContent** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null pointer");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;

  nsAutoString tag = aNode.GetText();
  nsIAtom* tagAtom = NS_NewAtom(tag);
  if (nsnull == tagAtom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsIXMLContent* xmlContent;
  nsresult rv = NS_NewXMLElement(&xmlContent, tagAtom);
  NS_RELEASE(tagAtom);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = AddAttributes(aNode, xmlContent);
  if (NS_FAILED(rv)) {
    NS_RELEASE(xmlContent);
    return rv;
  }

  *aResult = xmlContent;
  return rv;
}

//----------------------------------------------------------------------

/**
 * This class provides a sink for the xml parser and in particular knows
 * how to process frame regression data.
 */
class nsFrameRegressionDataSink : public nsIXMLContentSink {
public:
  nsFrameRegressionDataSink();
  ~nsFrameRegressionDataSink();

  NS_DECL_ISUPPORTS

  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(nsresult aErrorResult);
  NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
  NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
  NS_IMETHOD AddNotation(const nsIParserNode& aNode);
  NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

  nsIXMLContent* GetRoot() const { return mRoot; }

protected:
  nsIXMLContent* mRoot;
  nsIXMLContent* mCurrentContainer;
};

nsFrameRegressionDataSink::nsFrameRegressionDataSink()
{
  mRoot = nsnull;
  mCurrentContainer = nsnull;
  mRefCnt = 1;
}

nsFrameRegressionDataSink::~nsFrameRegressionDataSink()
{
  NS_IF_RELEASE(mRoot);
  NS_IF_RELEASE(mCurrentContainer);
}

NS_IMPL_ISUPPORTS(nsFrameRegressionDataSink, kIXMLContentSinkIID)

NS_IMETHODIMP
nsFrameRegressionDataSink::WillBuildModel()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::DidBuildModel(PRInt32 aQualityLevel)
{
  mRoot->List();
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::WillInterrupt()
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::WillResume()
{
  return NS_OK;
}

NS_IMETHODIMP 
nsFrameRegressionDataSink::SetParser(nsIParser* aParser)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::OpenContainer(const nsIParserNode& aNode)
{
  nsIXMLContent* element;
  nsresult rv = NewElement(aNode, &element);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsnull == mRoot) {
    mRoot = element;
    mCurrentContainer = element;
    NS_ADDREF(element);
  }
  else {
    NS_ASSERTION(nsnull != mCurrentContainer, "whoops");
    mCurrentContainer->AppendChildTo(element, PR_FALSE);
    mCurrentContainer = element;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::CloseContainer(const nsIParserNode& aNode)
{
  NS_PRECONDITION(nsnull != mRoot, "whoops");
  NS_PRECONDITION(nsnull != mCurrentContainer, "whoops");

  nsresult rv = NS_OK;
  if (mCurrentContainer == mRoot) {
    NS_RELEASE(mCurrentContainer);
  }
  else {
    nsIContent* parent;
    rv = mCurrentContainer->GetParent(parent);
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ASSERTION(nsnull != parent, "whoops");
    nsIXMLContent* xmlContent;
    rv = parent->QueryInterface(kIXMLContentIID, (void**) &xmlContent);
    NS_ASSERTION(NS_SUCCEEDED(rv), "whoops");
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_RELEASE(parent);
    NS_RELEASE(mCurrentContainer);
    mCurrentContainer = xmlContent;
  }
  return rv;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddLeaf(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddComment(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::NotifyError(nsresult aErrorResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddXMLDecl(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddCharacterData(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddUnparsedEntity(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddNotation(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFrameRegressionDataSink::AddEntityReference(const nsIParserNode& aNode)
{
  return NS_OK;
}

//----------------------------------------------------------------------

class nsFrameUtil : public nsIFrameUtil {
public:
  nsFrameUtil();
  ~nsFrameUtil();

  NS_DECL_ISUPPORTS
  NS_IMETHOD LoadFrameRegressionData(nsIURL* aURL, nsIXMLContent** aResult);
};

nsresult
NS_NewFrameUtil(nsIFrameUtil** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null pointer");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;

  nsFrameUtil* it = new nsFrameUtil();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIFrameUtilIID, (void**) aResult);
}

nsFrameUtil::nsFrameUtil()
{
  NS_INIT_REFCNT();
}

nsFrameUtil::~nsFrameUtil()
{
}

NS_IMPL_ISUPPORTS(nsFrameUtil, kIFrameUtilIID);

NS_IMETHODIMP
nsFrameUtil::LoadFrameRegressionData(nsIURL* aURL, nsIXMLContent** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null pointer");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsFrameRegressionDataSink* sink = new nsFrameRegressionDataSink();
  if (nsnull == sink) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsIParser* parser;
  nsresult rv = nsRepository::CreateInstance(kCParserCID, nsnull, kCParserIID,
                                             (void **)&parser);
  if (NS_FAILED(rv)) {
    delete sink;
    return rv;
  }

  // XXX since the parser can't sync load data, we do it right here
  nsAutoString theWholeDarnThing;
  nsIInputStream* iin;
  rv = NS_OpenURL(aURL, &iin);
  if (NS_OK != rv) {
    return rv;
  }
  for (;;) {
    char buf[4000];
    PRUint32 rc;
    rv = iin->Read(buf, 0, sizeof(buf), &rc);
    if (NS_FAILED(rv) || (rc <= 0)) {
      break;
    }
    theWholeDarnThing.Append(buf, rc);
  }
  NS_RELEASE(iin);

  // XXX yucky API
  nsIDTD* theDTD = 0;
  NS_NewWellFormed_DTD(&theDTD);
  parser->RegisterDTD(theDTD);
  parser->SetContentSink(sink);
  parser->Parse(theWholeDarnThing, PR_FALSE);
  NS_RELEASE(parser);

  *aResult = sink->GetRoot();
  NS_RELEASE(sink); 

  return rv;
}
