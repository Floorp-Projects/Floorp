/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
 *   Axel Hecht <axel@pike.org>
 *
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
#include "txURIUtils.h"
#include "XMLParser.h"
#include "txSingleNodeContext.h"
#include "Names.h"
#include "txUnknownHandler.h"
#include "txHTMLOutput.h"
#include "txTextOutput.h"

/**
 * Output Handler Factory
 */
class txStandaloneHandlerFactory : public txIOutputHandlerFactory
{
public:
    txStandaloneHandlerFactory(ProcessorState* aPs,
                               ostream* aStream)
        : mPs(aPs), mStream(aStream)
    {
    }

    virtual ~txStandaloneHandlerFactory()
    {
    };

    TX_DECL_TXIOUTPUTHANDLERFACTORY;

private:
    ProcessorState* mPs;
    ostream* mStream;
};

nsresult
txStandaloneHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              txIOutputXMLEventHandler** aHandler)
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
            *aHandler = new txUnknownHandler(mPs);
            break;
    }
    NS_ENSURE_TRUE(*aHandler, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

nsresult
txStandaloneHandlerFactory::createHandlerWith(txOutputFormat* aFormat,
                                              const String& aName,
                                              PRInt32 aNsID,
                                              txIOutputXMLEventHandler** aHandler)
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
txStandaloneXSLTProcessor::transform(String& aXMLPath, ostream& aOut,
                                     ErrorObserver& aErr)
{
    Document* xmlDoc = parsePath(aXMLPath, aErr);
    if (!xmlDoc) {
        return NS_ERROR_FAILURE;
    }

    // transform
    nsresult rv = transform(xmlDoc, aOut, aErr);

    delete xmlDoc;

    return rv;
}

/**
 * Transform a XML document given by path with the given
 * stylesheet.
 */
nsresult
txStandaloneXSLTProcessor::transform(String& aXMLPath, String& aXSLPath,
                                     ostream& aOut, ErrorObserver& aErr)
{
    Document* xmlDoc = parsePath(aXMLPath, aErr);
    if (!xmlDoc) {
        return NS_ERROR_FAILURE;
    }
    Document* xslDoc = parsePath(aXSLPath, aErr);
    if (!xslDoc) {
        delete xmlDoc;
        return NS_ERROR_FAILURE;
    }
    // transform
    nsresult rv = transform(xmlDoc, xslDoc, aOut, aErr);

    delete xmlDoc;
    delete xslDoc;

    return rv;
}

/**
 * Transform a XML document.
 * The stylesheet is retrieved by a processing instruction,
 * or an error is returned.
 */
nsresult
txStandaloneXSLTProcessor::transform(Document* aXMLDoc, ostream& aOut,
                                     ErrorObserver& aErr)
{
    if (!aXMLDoc) {
        return NS_ERROR_INVALID_POINTER;
    }

    // get stylesheet path
    String stylePath;
    getHrefFromStylesheetPI(*aXMLDoc, stylePath);

    Document* xslDoc = parsePath(stylePath, aErr);
    if (!xslDoc) {
        return NS_ERROR_FAILURE;
    }

    // transform
    nsresult rv = transform(aXMLDoc, xslDoc, aOut, aErr);

    delete xslDoc;
    return rv;
}

/**
 * Processes the given XML Document using the given XSL document
 * and prints the results to the given ostream argument
 */
nsresult
txStandaloneXSLTProcessor::transform(Document* aSource, Node* aStylesheet,
                                     ostream& aOut, ErrorObserver& aErr)
{
    // Create a new ProcessorState
    Document* stylesheetDoc = 0;
    Element* stylesheetElem = 0;
    if (aStylesheet->getNodeType() == Node::DOCUMENT_NODE) {
        stylesheetDoc = (Document*)aStylesheet;
    }
    else {
        stylesheetElem = (Element*)aStylesheet;
        stylesheetDoc = aStylesheet->getOwnerDocument();
    }
    ProcessorState ps(aSource, stylesheetDoc);

    ps.addErrorObserver(aErr);

    txSingleNodeContext evalContext(aSource, &ps);
    ps.setEvalContext(&evalContext);

    // Index templates and process top level xsl elements
    nsresult rv = NS_OK;
    if (stylesheetElem) {
        rv = processTopLevel(stylesheetElem, 0, &ps);
    }
    else {
        rv = processStylesheet(stylesheetDoc, 0, &ps);
    }
    if (NS_FAILED(rv)) {
        return rv;
    }

    txStandaloneHandlerFactory handlerFactory(&ps, &aOut);
    ps.mOutputHandlerFactory = &handlerFactory;

#ifndef XP_WIN
    bool sync = aOut.sync_with_stdio(false);
#endif
    // Process root of XML source document
    txXSLTProcessor::transform(&ps);
#ifndef XP_WIN
    aOut.sync_with_stdio(sync);
#endif

    return NS_OK;
}

/**
 * Parses all XML Stylesheet PIs associated with the
 * given XML document. If any stylesheet PIs are found with
 * type="text/xsl" the href psuedo attribute value will be
 * added to the given href argument. If multiple text/xsl stylesheet PIs
 * are found, the one closest to the end of the document is used.
 */
void txStandaloneXSLTProcessor::getHrefFromStylesheetPI(Document& xmlDocument,
                                                        String& href)
{
    Node* node = xmlDocument.getFirstChild();
    String type;
    String tmpHref;
    while (node) {
        if (node->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            String target = ((ProcessingInstruction*)node)->getTarget();
            if (STYLESHEET_PI.isEqual(target) ||
                STYLESHEET_PI_OLD.isEqual(target)) {
                String data = ((ProcessingInstruction*)node)->getData();
                type.clear();
                tmpHref.clear();
                parseStylesheetPI(data, type, tmpHref);
                if (XSL_MIME_TYPE.isEqual(type)) {
                    href.clear();
                    URIUtils::resolveHref(tmpHref, node->getBaseURI(), href);
                }
            }
        }
        node = node->getNextSibling();
    }

}

/**
 * Parses the contents of data, and returns the type and href pseudo attributes
 */
void txStandaloneXSLTProcessor::parseStylesheetPI(String& data,
                                                  String& type,
                                                  String& href)
{
    PRUint32 size = data.length();
    NamedMap bufferMap;
    bufferMap.put(String("type"), &type);
    bufferMap.put(String("href"), &href);
    PRUint32 ccount = 0;
    MBool inLiteral = MB_FALSE;
    char matchQuote = '"';
    String sink;
    String* buffer = &sink;

    for (ccount = 0; ccount < size; ccount++) {
        char ch = data.charAt(ccount);
        switch ( ch ) {
            case ' ':
                if (inLiteral) {
                    buffer->append(ch);
                }
                break;
            case '=':
                if (inLiteral) {
                    buffer->append(ch);
                }
                else if (buffer->length() > 0) {
                    buffer = (String*)bufferMap.get(*buffer);
                    if (!buffer) {
                        sink.clear();
                        buffer = &sink;
                    }
                }
                break;
            case '"':
            case '\'':
                if (inLiteral) {
                    if (matchQuote == ch) {
                        inLiteral = MB_FALSE;
                        sink.clear();
                        buffer = &sink;
                    }
                    else {
                        buffer->append(ch);
                    }
                }
                else {
                    inLiteral = MB_TRUE;
                    matchQuote = ch;
                }
                break;
            default:
                buffer->append(ch);
                break;
        }
    }
}

Document*
txStandaloneXSLTProcessor::parsePath(const String& aPath, ErrorObserver& aErr)
{
    ifstream xmlInput(NS_LossyConvertUCS2toASCII(aPath).get(), ios::in);
    if (!xmlInput) {
        String err("Couldn't open ");
        err.append(aPath);
        aErr.receiveError(err);
        return 0;
    }
    // parse source
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, aPath);
    if (!xmlDoc) {
        String err("Parsing error in ");
        err.append(aPath);
        aErr.receiveError(err);
    }
    return xmlDoc;
}
