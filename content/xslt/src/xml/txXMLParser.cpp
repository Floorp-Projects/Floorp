/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tom Kneeland <tomk@mitre.org> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txXMLParser.h"
#include "txURIUtils.h"
#include "txXPathTreeWalker.h"

#ifndef TX_EXE
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsISyncLoadDOMService.h"
#include "nsNetUtil.h"
#else
#include "xmlparse.h"
#endif

#ifdef TX_EXE
/**
 *  Implementation of an In-Memory DOM based XML parser.  The actual XML
 *  parsing is provided by EXPAT.
 */
class txXMLParser
{
  public:
    nsresult parse(istream& aInputStream, const nsAString& aUri,
                   txXPathNode** aResultDoc);
    const nsAString& getErrorString();

    /**
     * Expat handlers
     */
    int StartElement(const XML_Char *aName, const XML_Char **aAtts);
    int EndElement(const XML_Char* aName);
    void CharacterData(const XML_Char* aChars, int aLength);
    void Comment(const XML_Char* aChars);
    int ProcessingInstruction(const XML_Char *aTarget, const XML_Char *aData);
    int ExternalEntityRef(const XML_Char *aContext, const XML_Char *aBase,
                          const XML_Char *aSystemId,
                          const XML_Char *aPublicId);

  protected:
    void createErrorString();
    nsString  mErrorString;
    Document* mDocument;
    Node*  mCurrentNode;
    XML_Parser mExpatParser;
};
#endif

nsresult
txParseDocumentFromURI(const nsAString& aHref, const nsAString& aReferrer,
                       const txXPathNode& aLoader, nsAString& aErrMsg,
                       txXPathNode** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
#ifndef TX_EXE
    nsCOMPtr<nsIURI> documentURI;
    nsresult rv = NS_NewURI(getter_AddRefs(documentURI), aHref);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDocument* loaderDocument = txXPathNativeNode::getDocument(aLoader);

    nsCOMPtr<nsILoadGroup> loadGroup = loaderDocument->GetDocumentLoadGroup();
    nsIURI *loaderUri = loaderDocument->GetDocumentURI();
    NS_ENSURE_TRUE(loaderUri, NS_ERROR_FAILURE);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), documentURI, nsnull,
                       loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(channel);
    if (http) {
        nsCOMPtr<nsIURI> refUri;
        NS_NewURI(getter_AddRefs(refUri), aReferrer);
        if (refUri) {
            http->SetReferrer(refUri);
        }
        http->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),     
                               NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
                               PR_FALSE);


    }

    nsCOMPtr<nsISyncLoadDOMService> loader =
      do_GetService("@mozilla.org/content/syncload-dom-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Raw pointer, we want the resulting txXPathNode to hold a reference to
    // the document.
    nsIDOMDocument* theDocument = nsnull;
    rv = loader->LoadDocumentAsXML(channel, loaderUri, &theDocument);
    if (NS_FAILED(rv) || !theDocument) {
        aErrMsg.Append(NS_LITERAL_STRING("Document load of ") + 
                       aHref + NS_LITERAL_STRING(" failed."));
        return rv;
    }

    *aResult = txXPathNativeNode::createXPathNode(theDocument);
    if (!*aResult) {
        NS_RELEASE(theDocument);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
#else
    istream* xslInput = URIUtils::getInputStream(aHref, aErrMsg);
    if (!xslInput) {
        return NS_ERROR_FAILURE;
    }
    return txParseFromStream(*xslInput, aHref, aErrMsg, aResult);
#endif
}

#ifdef TX_EXE
nsresult
txParseFromStream(istream& aInputStream, const nsAString& aUri,
                  nsAString& aErrorString, txXPathNode** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    txXMLParser parser;
    nsresult rv = parser.parse(aInputStream, aUri, aResult);
    aErrorString = parser.getErrorString();
    return rv;
}

/**
 * expat C stub handlers
 */

// shortcut macro for redirection into txXMLParser method calls
#define TX_XMLPARSER(_userData) NS_STATIC_CAST(txXMLParser*, _userData)
#define TX_ENSURE_DATA(_userData)                       \
  PR_BEGIN_MACRO                                        \
    if (!aUserData) {                                   \
        NS_WARNING("no userData in comment handler");   \
        return;                                         \
    }                                                   \
  PR_END_MACRO

PR_STATIC_CALLBACK(void)
startElement(void *aUserData, const XML_Char *aName, const XML_Char **aAtts)
{
    TX_ENSURE_DATA(aUserData);
    TX_XMLPARSER(aUserData)->StartElement(aName, aAtts);
}

PR_STATIC_CALLBACK(void)
endElement(void *aUserData, const XML_Char* aName)
{
    TX_ENSURE_DATA(aUserData);
    TX_XMLPARSER(aUserData)->EndElement(aName);
}

PR_STATIC_CALLBACK(void)
charData(void* aUserData, const XML_Char* aChars, int aLength)
{
    TX_ENSURE_DATA(aUserData);
    TX_XMLPARSER(aUserData)->CharacterData(aChars, aLength);
}

PR_STATIC_CALLBACK(void)
commentHandler(void* aUserData, const XML_Char* aChars)
{
    TX_ENSURE_DATA(aUserData);
    TX_XMLPARSER(aUserData)->Comment(aChars);
}

PR_STATIC_CALLBACK(void)
piHandler(void *aUserData, const XML_Char *aTarget, const XML_Char *aData)
{
    TX_ENSURE_DATA(aUserData);
    TX_XMLPARSER(aUserData)->ProcessingInstruction(aTarget, aData);
}

PR_STATIC_CALLBACK(int)
externalEntityRefHandler(XML_Parser aParser,
                         const XML_Char *aContext,
                         const XML_Char *aBase,
                         const XML_Char *aSystemId,
                         const XML_Char *aPublicId)
{
    // aParser is aUserData is the txXMLParser,
    // we set that in txXMLParser::parse
    NS_ENSURE_TRUE(aParser, XML_ERROR_NONE);
    return TX_XMLPARSER(aParser)->ExternalEntityRef(aContext, aBase,
                                                    aSystemId, aPublicId);
}


/**
 *  Parses the given input stream and returns a DOM Document.
 *  A NULL pointer will be returned if errors occurred
 */
nsresult
txXMLParser::parse(istream& aInputStream, const nsAString& aUri,
                   txXPathNode** aResultDoc)
{
    mErrorString.Truncate();
    *aResultDoc = nsnull;
    if (!aInputStream) {
        mErrorString.AppendLiteral("unable to parse xml: invalid or unopen stream encountered.");
        return NS_ERROR_FAILURE;
    }
    mExpatParser = XML_ParserCreate(nsnull);
    if (!mExpatParser) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mDocument = new Document();
    if (!mDocument) {
        XML_ParserFree(mExpatParser);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mDocument->documentBaseURI = aUri;
    mCurrentNode = mDocument;

    XML_SetUserData(mExpatParser, this);
    XML_SetElementHandler(mExpatParser, startElement, endElement);
    XML_SetCharacterDataHandler(mExpatParser, charData);
    XML_SetProcessingInstructionHandler(mExpatParser, piHandler);
    XML_SetCommentHandler(mExpatParser, commentHandler);
#ifdef XML_DTD
    XML_SetParamEntityParsing(mExpatParser, XML_PARAM_ENTITY_PARSING_ALWAYS);
#endif
    XML_SetExternalEntityRefHandler(mExpatParser, externalEntityRefHandler);
    XML_SetExternalEntityRefHandlerArg(mExpatParser, this);
    XML_SetBase(mExpatParser,
                (const XML_Char*)(PromiseFlatString(aUri).get()));

    const int bufferSize = 1024;
    char buf[bufferSize];
    PRBool done;
    do {
        aInputStream.read(buf, bufferSize);
        done = aInputStream.eof();

        if (!XML_Parse(mExpatParser, buf, aInputStream.gcount(), done)) {
            createErrorString();
            done = MB_TRUE;
            delete mDocument;
            mDocument = nsnull;
        }
    } while (!done);
    aInputStream.clear();

    // clean up
    XML_ParserFree(mExpatParser);
    // ownership to the caller
    *aResultDoc = txXPathNativeNode::createXPathNode(mDocument);
    mDocument = nsnull;
    return *aResultDoc ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

const nsAString&
txXMLParser::getErrorString()
{
    return mErrorString;
}


int
txXMLParser::StartElement(const XML_Char *aName, const XML_Char **aAtts)
{
    const XML_Char** theAtts = aAtts;

    Element* newElement =
        mDocument->createElement(nsDependentString((const PRUnichar*)aName));
    if (!newElement) {
        return XML_ERROR_NO_MEMORY;
    }

    while (*theAtts) {
        nsDependentString attName((const PRUnichar*)*theAtts++);
        nsDependentString attValue((const PRUnichar*)*theAtts++);
        newElement->setAttribute(attName, attValue);
    }

    int idx;
    if ((idx = XML_GetIdAttributeIndex(mExpatParser)) > -1) {
        nsDependentString idName((const PRUnichar*)*(aAtts + idx));
        nsDependentString idValue((const PRUnichar*)*(aAtts + idx + 1));
        // make sure IDs are unique
        if (!idValue.IsEmpty()) {
            mDocument->setElementID(idValue, newElement);
        }
    }
    mCurrentNode->appendChild(newElement);
    mCurrentNode = newElement;

    return XML_ERROR_NONE;
}

int
txXMLParser::EndElement(const XML_Char* aName)
{
    if (mCurrentNode->getParentNode()) {
        mCurrentNode = mCurrentNode->getParentNode();
    }
    return XML_ERROR_NONE;
}

void
txXMLParser::CharacterData(const XML_Char* aChars, int aLength)
{
    Node* prevSib = mCurrentNode->getLastChild();
    const PRUnichar* pChars = NS_STATIC_CAST(const PRUnichar*, aChars);
    if (prevSib && prevSib->getNodeType() == Node::TEXT_NODE) {
        NS_STATIC_CAST(NodeDefinition*, prevSib)->appendData(pChars, aLength);
    }
    else {
        // aChars is not null-terminated so we use Substring here.
        Node* node = mDocument->createTextNode(Substring(pChars,
                                                         pChars + aLength));
        mCurrentNode->appendChild(node);
    }
}

void
txXMLParser::Comment(const XML_Char* aChars)
{
    Node* node = mDocument->createComment(
        nsDependentString(NS_STATIC_CAST(const PRUnichar*, aChars)));
    mCurrentNode->appendChild(node);
}

int
txXMLParser::ProcessingInstruction(const XML_Char *aTarget,
                                   const XML_Char *aData)
{
    nsDependentString target((const PRUnichar*)aTarget);
    nsDependentString data((const PRUnichar*)aData);
    Node* node = mDocument->createProcessingInstruction(target, data);
    mCurrentNode->appendChild(node);

    return XML_ERROR_NONE;
}

int
txXMLParser::ExternalEntityRef(const XML_Char *aContext,
                               const XML_Char *aBase,
                               const XML_Char *aSystemId,
                               const XML_Char *aPublicId)
{
    if (aPublicId) {
        // not supported, this is "http://some.site.net/foo.dtd" stuff
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    nsAutoString absUrl;
    URIUtils::resolveHref(nsDependentString((PRUnichar*)aSystemId),
                          nsDependentString((PRUnichar*)aBase), absUrl);
    istream* extInput = URIUtils::getInputStream(absUrl, mErrorString);
    if (!extInput) {
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    XML_Parser parent = mExpatParser;
    mExpatParser = 
        XML_ExternalEntityParserCreate(mExpatParser, aContext, nsnull);
    if (!mExpatParser) {
        mExpatParser = parent;
        delete extInput;
        return XML_ERROR_EXTERNAL_ENTITY_HANDLING;
    }
    XML_SetBase(mExpatParser, absUrl.get());

    const int bufSize = 1024;
    char buffer[bufSize];
    int result;
    PRBool done;
    do {
        extInput->read(buffer, bufSize);
        done = extInput->eof();
        if (!(result =
              XML_Parse(mExpatParser, buffer,  extInput->gcount(), done))) {
            createErrorString();
            mErrorString.Append(PRUnichar('\n'));
            done = MB_TRUE;
        }
    } while (!done);

    delete extInput;
    XML_ParserFree(mExpatParser);

    mExpatParser = parent;

    return result;
}

void
txXMLParser::createErrorString()
{
    XML_Error errCode = XML_GetErrorCode(mExpatParser);
    mErrorString.AppendWithConversion(XML_ErrorString(errCode));
    mErrorString.AppendLiteral(" at line ");
    mErrorString.AppendInt(XML_GetCurrentLineNumber(mExpatParser));
    mErrorString.AppendLiteral(" in ");
    mErrorString.Append((const PRUnichar*)XML_GetBase(mExpatParser));
}
#endif
