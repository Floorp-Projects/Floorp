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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *   Axel Hecht <axel@pike.org>
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

#include "txStandaloneXSLTProcessor.h"
#include "txStandaloneStylesheetCompiler.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "txHTMLOutput.h"
#include "txTextOutput.h"
#include "txUnknownHandler.h"
#include "txURIUtils.h"
#include "txXMLParser.h"

TX_IMPL_DOM_STATICS;

/**
 * Output Handler Factory
 */
class txStandaloneHandlerFactory : public txAOutputHandlerFactory
{
public:
    txStandaloneHandlerFactory(txExecutionState* aEs,
                               ostream* aStream)
        : mEs(aEs), mStream(aStream)
    {
    }

    virtual ~txStandaloneHandlerFactory()
    {
    }

    TX_DECL_TXAOUTPUTHANDLERFACTORY

private:
    txExecutionState* mEs;
    ostream* mStream;
};

nsresult
txStandaloneHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              txAOutputXMLEventHandler** aHandler)
{
    *aHandler = 0;
    switch (aFormat->mMethod) {
        case eXMLOutput:
            *aHandler = new txXMLOutput(aFormat, mStream);
            break;

        case eHTMLOutput:
            *aHandler = new txHTMLOutput(aFormat, mStream);
            break;

        case eTextOutput:
            *aHandler = new txTextOutput(mStream);
            break;

        case eMethodNotSet:
            *aHandler = new txUnknownHandler(mEs);
            break;
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txStandaloneHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              const nsAString& aName,
                                              PRInt32 aNsID,
                                              txAOutputXMLEventHandler** aHandler)
{
    *aHandler = 0;
    NS_ASSERTION(aFormat->mMethod != eMethodNotSet,
                 "How can method not be known when root element is?");
    NS_ENSURE_TRUE(aFormat->mMethod != eMethodNotSet, NS_ERROR_UNEXPECTED);
    return createHandlerWith(aFormat, aHandler);
}


/**
 * txStandaloneXSLTProcessor
 */

/**
 * Transform a XML document given by path.
 * The stylesheet is retrieved by a processing instruction,
 * or an error is returned.
 */
nsresult
txStandaloneXSLTProcessor::transform(nsACString& aXMLPath, ostream& aOut,
                                     ErrorObserver& aErr)
{
    txXPathNode* xmlDoc = parsePath(aXMLPath, aErr);
    if (!xmlDoc) {
        return NS_ERROR_FAILURE;
    }

    // transform
    nsresult rv = transform(*xmlDoc, aOut, aErr);

    delete xmlDoc;

    return rv;
}

/**
 * Transform a XML document given by path with the given
 * stylesheet.
 */
nsresult
txStandaloneXSLTProcessor::transform(nsACString& aXMLPath,
                                     nsACString& aXSLPath, ostream& aOut,
                                     ErrorObserver& aErr)
{
    txXPathNode* xmlDoc = parsePath(aXMLPath, aErr);
    if (!xmlDoc) {
        return NS_ERROR_FAILURE;
    }
    txParsedURL path;
    path.init(NS_ConvertASCIItoUCS2(aXSLPath));
    nsRefPtr<txStylesheet> style;
    nsresult rv = TX_CompileStylesheetPath(path, getter_AddRefs(style));
    if (NS_FAILED(rv)) {
        delete xmlDoc;
        return rv;
    }
    // transform
    rv = transform(*xmlDoc, style, aOut, aErr);

    delete xmlDoc;

    return rv;
}

/**
 * Transform a XML document.
 * The stylesheet is retrieved by a processing instruction,
 * or an error is returned.
 */
nsresult
txStandaloneXSLTProcessor::transform(txXPathNode& aXMLDoc, ostream& aOut,
                                     ErrorObserver& aErr)
{
    Document* xmlDoc;
    nsresult rv = txXPathNativeNode::getDocument(aXMLDoc, &xmlDoc);
    NS_ENSURE_SUCCESS(rv, rv);

    // get stylesheet path
    nsAutoString stylePath, basePath;
    xmlDoc->getBaseURI(basePath);
    getHrefFromStylesheetPI(*xmlDoc, stylePath);
    txParsedURL base, ref, resolved;
    base.init(basePath);
    ref.init(stylePath);
    base.resolve(ref, resolved);

    nsRefPtr<txStylesheet> style;
    rv = TX_CompileStylesheetPath(resolved, getter_AddRefs(style));
    NS_ENSURE_SUCCESS(rv, rv);

    // transform
    rv = transform(aXMLDoc, style, aOut, aErr);

    return rv;
}

/**
 * Processes the given XML Document using the given XSL document
 * and prints the results to the given ostream argument
 */
nsresult
txStandaloneXSLTProcessor::transform(txXPathNode& aSource,
                                     txStylesheet* aStylesheet,
                                     ostream& aOut, ErrorObserver& aErr)
{
    // Create a new txEvalState
    txExecutionState es(aStylesheet);

    // XXX todo es.addErrorObserver(aErr);

    txStandaloneHandlerFactory handlerFactory(&es, &aOut);

#ifndef XP_WIN
    bool sync = aOut.sync_with_stdio(false);
#endif
    es.mOutputHandlerFactory = &handlerFactory;

    es.init(aSource, nsnull);

    // Process root of XML source document
    nsresult rv = txXSLTProcessor::execute(es);
    es.end(rv);

#ifndef XP_WIN
    aOut.sync_with_stdio(sync);
#endif

    return rv;
}

/**
 * Parses the XML Stylesheet PIs associated with the
 * given XML document. If a stylesheet PIs is found with type="text/xsl",
 * type="text/xml" or type="application/xml" the href pseudo attribute
 * value will be appended to the given href argument. If multiple XSLT
 * stylesheet PIs are found, the first one is used.
 */
void txStandaloneXSLTProcessor::getHrefFromStylesheetPI(Document& xmlDocument,
                                                        nsAString& href)
{
    Node* node = xmlDocument.getFirstChild();
    nsAutoString type;
    nsAutoString tmpHref;
    while (node) {
        if (node->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            nsAutoString target;
            node->getNodeName(target);
            if (target.EqualsLiteral("xml-stylesheet")) {
                nsAutoString data;
                node->getNodeValue(data);
                type.Truncate();
                tmpHref.Truncate();
                parseStylesheetPI(data, type, tmpHref);
                if (type.EqualsLiteral("text/xsl") ||
                    type.EqualsLiteral("text/xml") ||
                    type.EqualsLiteral("application/xml")) {
                    href = tmpHref;
                    return;
                }
            }
        }
        node = node->getNextSibling();
    }
}

/**
 * Parses the contents of data, and returns the type and href pseudo attributes
 * (Based on code copied from nsParserUtils)
 */
#define SKIP_WHITESPACE(iter, end_iter)                          \
  while ((iter) != (end_iter) && nsCRT::IsAsciiSpace(*(iter))) { \
    ++(iter);                                                    \
  }                                                              \
  if ((iter) == (end_iter))                                      \
    break

#define SKIP_ATTR_NAME(iter, end_iter)                            \
  while ((iter) != (end_iter) && !nsCRT::IsAsciiSpace(*(iter)) && \
         *(iter) != '=') {                                        \
    ++(iter);                                                     \
  }                                                               \
  if ((iter) == (end_iter))                                       \
    break

void txStandaloneXSLTProcessor::parseStylesheetPI(const nsAFlatString& aData,
                                                  nsAString& aType,
                                                  nsAString& aHref)
{
  nsAFlatString::const_char_iterator start, end;
  aData.BeginReading(start);
  aData.EndReading(end);
  nsAFlatString::const_char_iterator iter;
  PRInt8 found = 0;

  while (start != end) {
    SKIP_WHITESPACE(start, end);
    iter = start;
    SKIP_ATTR_NAME(iter, end);

    // Remember the attr name.
    const nsAString & attrName = Substring(start, iter);

    // Now check whether this is a valid name="value" pair.
    start = iter;
    SKIP_WHITESPACE(start, end);
    if (*start != '=') {
      // No '=', so this is not a name="value" pair.  We don't know
      // what it is, and we have no way to handle it.
      break;
    }

    // Have to skip the value.
    ++start;
    SKIP_WHITESPACE(start, end);
    PRUnichar q = *start;
    if (q != QUOTE && q != APOSTROPHE) {
      // Not a valid quoted value, so bail.
      break;
    }

    ++start;  // Point to the first char of the value.
    iter = start;
    while (iter != end && *iter != q) {
      ++iter;
    }
    if (iter == end) {
      // Oops, unterminated quoted string.
      break;
    }
    
    // At this point attrName holds the name of the "attribute" and
    // the value is between start and iter.
    if (attrName.EqualsLiteral("type")) {
      aType = Substring(start, iter);
      ++found;
    }
    else if (attrName.EqualsLiteral("href")) {
      aHref = Substring(start, iter);
      ++found;
    }

    // Stop if we found both attributes
    if (found == 2) {
      break;
    }

    // Resume scanning after the end of the attribute value.
    start = iter;
    ++start;  // To move past the quote char.
  }
}

txXPathNode*
txStandaloneXSLTProcessor::parsePath(const nsACString& aPath, ErrorObserver& aErr)
{
    NS_ConvertASCIItoUCS2 path(aPath);

    ifstream xmlInput(PromiseFlatCString(aPath).get(), ios::in);
    if (!xmlInput) {
        aErr.receiveError(NS_LITERAL_STRING("Couldn't open ") + path);
        return 0;
    }
    // parse source
    txXPathNode* xmlDoc;
    nsAutoString errors;
    nsresult rv = txParseFromStream(xmlInput, path, errors, &xmlDoc);
    xmlInput.close();
    if (NS_FAILED(rv) || !xmlDoc) {
        aErr.receiveError(NS_LITERAL_STRING("Parsing error \"") + errors +
                          NS_LITERAL_STRING("\""));
    }
    return xmlDoc;
}
