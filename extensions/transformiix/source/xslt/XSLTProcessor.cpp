/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999, 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * Pierre Phaneuf, pp@ludusdesign.com
 *    -- fixed some XPCOM usage.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- Added call to recurisvely attribute-set processing on
 *       xsl:attribute-set itself
 *    -- Added call to handle attribute-set processing for xsl:copy
 *
 * Nathan Pride, npride@wavo.com
 *    -- fixed a document base issue
 *
 * Olivier Gerardin
 *    -- Changed behavior of passing parameters to templates
 *
 */

#include "XSLTProcessor.h"
#include "Names.h"
#include "XMLParser.h"
#include "txVariableMap.h"
#include "XMLUtils.h"
#include "XMLDOMUtils.h"
#include "txNodeSorter.h"
#include "txXSLTNumber.h"
#include "Tokenizer.h"
#include "txURIUtils.h"
#include "txAtoms.h"
#include "TxLog.h"
#include "txRtfHandler.h"
#include "txNodeSetContext.h"
#include "txSingleNodeContext.h"
#ifndef TX_EXE
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsIDOMClassInfo.h"
#include "nsIConsoleService.h"
#include "nsDOMError.h"
#else
#include "txHTMLOutput.h"
#include "txTextOutput.h"
#include "txUnknownHandler.h"
#include "txXMLOutput.h"
#endif

  //-----------------------------------/
 //- Implementation of XSLTProcessor -/
//-----------------------------------/

/**
 * XSLTProcessor is a class for Processing XSL stylesheets
**/

/**
 * A warning message used by all templates that do not allow non character
 * data to be generated
**/
const String XSLTProcessor::NON_TEXT_TEMPLATE_WARNING(
"templates for the following element are not allowed to generate non character data: ");

/*
 * Implement static variables for atomservice and dom.
 */
#ifdef TX_EXE
TX_IMPL_ATOM_STATICS;
TX_IMPL_DOM_STATICS;
#endif

/**
 * Creates a new XSLTProcessor
**/
XSLTProcessor::XSLTProcessor() : mOutputHandler(0),
                                 mResultHandler(0)
{
#ifndef TX_EXE
    NS_INIT_ISUPPORTS();
#endif

    xslVersion.append("1.0");
    appName.append("TransforMiiX");
    appVersion.append("1.0 [beta v20010123]");

    // Create default expressions

    // "node()"
    txNodeTest* nt = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
    mNodeExpr = new LocationStep(nt, LocationStep::CHILD_AXIS);

} //-- XSLTProcessor

/**
 * Default destructor
**/
XSLTProcessor::~XSLTProcessor()
{
    delete mOutputHandler;
    delete mNodeExpr;
}

#ifndef TX_EXE

// XXX START
// XXX Mozilla module only code. This should move to txMozillaXSLTProcessor
// XXX

NS_IMPL_ADDREF(XSLTProcessor)
NS_IMPL_RELEASE(XSLTProcessor)
NS_INTERFACE_MAP_BEGIN(XSLTProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIDocumentTransformer)
    NS_INTERFACE_MAP_ENTRY(nsIScriptLoaderObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentTransformer)
    NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XSLTProcessor)
NS_INTERFACE_MAP_END

// XXX
// XXX Mozilla module only code. This should move to txMozillaXSLTProcessor
// XXX END

#else

/*
 * Initialize atom tables.
 */
MBool txInit()
{
    if (!txNamespaceManager::init())
        return MB_FALSE;
    if (!txHTMLAtoms::init())
        return MB_FALSE;
    if (!txXMLAtoms::init())
        return MB_FALSE;
    if (!txXPathAtoms::init())
        return MB_FALSE;
    return txXSLTAtoms::init();
}

/*
 * To be called when done with transformiix.
 *
 * Free atom table, namespace manager.
 */
MBool txShutdown()
{
    txNamespaceManager::shutdown();
    txHTMLAtoms::shutdown();
    txXMLAtoms::shutdown();
    txXPathAtoms::shutdown();
    txXSLTAtoms::shutdown();
    return MB_TRUE;
}
#endif

/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void XSLTProcessor::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

String& XSLTProcessor::getAppName() {
    return appName;
} //-- getAppName

String& XSLTProcessor::getAppVersion() {
    return appVersion;
} //-- getAppVersion

#ifdef TX_EXE

// XXX START
// XXX Standalone only code. This should move to txStandaloneXSLTProcessor
// XXX

/**
 * Parses all XML Stylesheet PIs associated with the
 * given XML document. If any stylesheet PIs are found with
 * type="text/xsl" the href psuedo attribute value will be
 * added to the given href argument. If multiple text/xsl stylesheet PIs
 * are found, the one closest to the end of the document is used.
**/
void XSLTProcessor::getHrefFromStylesheetPI(Document& xmlDocument, String& href) {

    Node* node = xmlDocument.getFirstChild();
    String type;
    String tmpHref;
    while (node) {
        if ( node->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE ) {
            String target = ((ProcessingInstruction*)node)->getTarget();
            if ( STYLESHEET_PI.isEqual(target) ||
                 STYLESHEET_PI_OLD.isEqual(target) ) {
                String data = ((ProcessingInstruction*)node)->getData();
                type.clear();
                tmpHref.clear();
                parseStylesheetPI(data, type, tmpHref);
                if ( XSL_MIME_TYPE.isEqual(type) ) {
                    href.clear();
                    URIUtils::resolveHref(tmpHref, node->getBaseURI(), href);
                }
            }
        }
        node = node->getNextSibling();
    }

} //-- getHrefFromStylesheetPI

/**
 * Parses the contents of data, and returns the type and href psuedo attributes
**/
void XSLTProcessor::parseStylesheetPI(String& data, String& type, String& href) {

    PRUint32 size = data.length();
    NamedMap bufferMap;
    bufferMap.put(String("type"), &type);
    bufferMap.put(String("href"), &href);
    PRUint32 ccount = 0;
    MBool inLiteral = MB_FALSE;
    UNICODE_CHAR matchQuote = '"';
    String sink;
    String* buffer = &sink;

    for (ccount = 0; ccount < size; ccount++) {
        UNICODE_CHAR ch = data.charAt(ccount);
        switch ( ch ) {
            case ' ' :
                if ( inLiteral ) {
                    buffer->append(ch);
                }
                break;
            case '=':
                if ( inLiteral ) buffer->append(ch);
                else if (!buffer->isEmpty()) {
                    buffer = (String*)bufferMap.get(*buffer);
                    if ( !buffer ) {
                        sink.clear();
                        buffer = &sink;
                    }
                }
                break;
            case '"' :
            case '\'':
                if (inLiteral) {
                    if ( matchQuote == ch ) {
                        inLiteral = MB_FALSE;
                        sink.clear();
                        buffer = &sink;
                    }
                    else buffer->append(ch);
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

} //-- parseStylesheetPI

/**
 * Processes the given XML Document, the XSL stylesheet
 * will be retrieved from the XML Stylesheet Processing instruction,
 * otherwise an empty document will be returned.
 * @param xmlDocument the XML document to process
 * @param documentBase the document base of the XML document, for
 * resolving relative URIs
 * @return the result tree.
**/
Document* XSLTProcessor::process(Document& xmlDocument) {
    //-- look for Stylesheet PI
    Document xslDocument; //-- empty for now
    return process(xmlDocument, xslDocument);
} //-- process

/**
 * Reads an XML Document from the given XML input stream, and
 * processes the document using the XSL document derived from
 * the given XSL input stream.
 * @return the result tree.
**/
Document* XSLTProcessor::process
    (istream& xmlInput, String& xmlFilename,
     istream& xslInput, String& xslFilename) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, xmlFilename);
    if (!xmlDoc) {
        String err("error reading XML document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        return 0;
    }
    //-- Read in XSL document
    Document* xslDoc = xmlParser.parse(xslInput, xslFilename);
    if (!xslDoc) {
        String err("error reading XSL stylesheet document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        delete xmlDoc;
        return 0;
    }
    Document* result = process(*xmlDoc, *xslDoc);
    delete xmlDoc;
    delete xslDoc;
    return result;
} //-- process

/**
 * Reads an XML document from the given XML input stream. The
 * XML document is processed using the associated XSL document
 * retrieved from the XML document's Stylesheet Processing Instruction,
 * otherwise an empty document will be returned.
 * @param xmlDocument the XML document to process
 * @param documentBase the document base of the XML document, for
 * resolving relative URIs
 * @return the result tree.
**/
Document* XSLTProcessor::process(istream& xmlInput, String& xmlFilename) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, xmlFilename);
    if (!xmlDoc) {
        String err("error reading XML document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        return 0;
    }
    //-- Read in XSL document
    String href;
    String errMsg;
    getHrefFromStylesheetPI(*xmlDoc, href);
    istream* xslInput = URIUtils::getInputStream(href,errMsg);
    Document* xslDoc = 0;
    if ( xslInput ) {
        xslDoc = xmlParser.parse(*xslInput, href);
        delete xslInput;
    }
    if (!xslDoc) {
        String err("error reading XSL stylesheet document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        delete xmlDoc;
        return 0;
    }
    Document* result = process(*xmlDoc, *xslDoc);
    delete xmlDoc;
    delete xslDoc;
    return result;
} //-- process

// XXX
// XXX Standalone only code. This should move to txStandaloneXSLTProcessor
// XXX END

#endif

/**
 * Copy a node. For document nodes and document fragments, copy the children.
 */
void XSLTProcessor::copyNode(Node* aNode, ProcessorState* aPs)
{
    if (!aNode)
        return;

    switch (aNode->getNodeType()) {
        case Node::ATTRIBUTE_NODE:
        {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->attribute(aNode->getNodeName(), aNode->getNamespaceID(),
                                      aNode->getNodeValue());
            break;
        }
        case Node::COMMENT_NODE:
        {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->comment(aNode->getNodeValue());
            break;
        }
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        {
            // Copy children
            Node* child = aNode->getFirstChild();
            while (child) {
                copyNode(child, aPs);
                child = child->getNextSibling();
            }
            break;
        }
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)aNode;
            const String& name = element->getNodeName();
            PRInt32 nsID = element->getNamespaceID();
            startElement(aPs, name, nsID);

            // Copy attributes
            NamedNodeMap* attList = element->getAttributes();
            if (attList) {
                PRUint32 i = 0;
                for (i = 0; i < attList->getLength(); i++) {
                    Attr* attr = (Attr*)attList->item(i);
                    NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                    mResultHandler->attribute(attr->getNodeName(), attr->getNamespaceID(),
                                              attr->getNodeValue());
                }
            }

            // Copy children
            Node* child = element->getFirstChild();
            while (child) {
                copyNode(child, aPs);
                child = child->getNextSibling();
            }

            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->endElement(name, nsID);
            break;
        }
        case Node::PROCESSING_INSTRUCTION_NODE:
        {
            ProcessingInstruction* pi = (ProcessingInstruction*)aNode;
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->processingInstruction(pi->getTarget(), pi->getData());
            break;
        }
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->characters(aNode->getNodeValue());
            break;
        }
    }
}

void XSLTProcessor::processAction(Node* aXSLTAction,
                                  ProcessorState* aPs)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aXSLTAction, "We need an action to process.");
    if (!aXSLTAction)
        return;

    Document* resultDoc = aPs->getResultDocument();

    unsigned short nodeType = aXSLTAction->getNodeType();

    // Handle text nodes
    if (nodeType == Node::TEXT_NODE ||
        nodeType == Node::CDATA_SECTION_NODE) {
        const String& textValue = aXSLTAction->getNodeValue();
        if (!XMLUtils::isWhitespace(textValue) ||
            XMLUtils::getXMLSpacePreserve(aXSLTAction)) {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->characters(textValue);
        }
        return;
    }

    if (nodeType != Node::ELEMENT_NODE) {
        return;
    }

    // Handle element nodes
    Element* actionElement = (Element*)aXSLTAction;
    PRInt32 nsID = aXSLTAction->getNamespaceID();
    if (nsID != kNameSpaceID_XSLT) {
        // Literal result element
        // XXX TODO Check for excluded namespaces and aliased namespaces (element and attributes) 
        const String& nodeName = aXSLTAction->getNodeName();
        startElement(aPs, nodeName, nsID);

        processAttributeSets(actionElement, aPs);

        // Handle attributes
        NamedNodeMap* atts = actionElement->getAttributes();

        if (atts) {
            // Process all non XSLT attributes
            PRUint32 i;
            for (i = 0; i < atts->getLength(); ++i) {
                Attr* attr = (Attr*)atts->item(i);
                if (attr->getNamespaceID() == kNameSpaceID_XSLT)
                    continue;
                // Process Attribute Value Templates
                String value;
                aPs->processAttrValueTemplate(attr->getNodeValue(), actionElement, value);
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->attribute(attr->getNodeName(), attr->getNamespaceID(), value);
            }
        }

        // Process children
        processChildren(actionElement, aPs);
        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->endElement(nodeName, nsID);
        return;
    }

    Expr* expr = 0;
    txAtom* localName;
    aXSLTAction->getLocalName(&localName);
    // xsl:apply-imports
    if (localName == txXSLTAtoms::applyImports) {
        ProcessorState::TemplateRule* curr;
        Node* xslTemplate;
        ProcessorState::ImportFrame *frame;

        curr = aPs->getCurrentTemplateRule();
        if (!curr) {
            String err("apply-imports not allowed here");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }

        xslTemplate = aPs->findTemplate(aPs->getEvalContext()->getContextNode(),
                                        *curr->mMode, curr->mFrame, &frame);
        processMatchedTemplate(xslTemplate, curr->mParams, *curr->mMode,
                               frame, aPs);

    }
    // xsl:apply-templates
    else if (localName == txXSLTAtoms::applyTemplates) {
        if (actionElement->hasAttr(txXSLTAtoms::select,
                                   kNameSpaceID_None))
            expr = aPs->getExpr(actionElement,
                                ProcessorState::SelectAttr);
        else
            expr = mNodeExpr;

        if (!expr) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        ExprResult* exprResult = expr->evaluate(aPs->getEvalContext());
        if (!exprResult) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        if (exprResult->getResultType() == ExprResult::NODESET) {
            NodeSet* nodeSet = (NodeSet*)exprResult;
            if (nodeSet->isEmpty()) {
                delete nodeSet;
                return;
            }

            // Look for xsl:sort elements
            txNodeSorter sorter(aPs);
            Node* child = actionElement->getFirstChild();
            while (child) {
                if (child->getNodeType() == Node::ELEMENT_NODE &&
                    child->getNamespaceID() == kNameSpaceID_XSLT) {
                    txAtom* childLocalName;
                    child->getLocalName(&childLocalName);
                    if (childLocalName == txXSLTAtoms::sort) {
                        sorter.addSortElement((Element*)child);
                    }
                    TX_IF_RELEASE_ATOM(childLocalName);
                }
                child = child->getNextSibling();
            }
            sorter.sortNodeSet(nodeSet);

            // Process xsl:with-param elements
            txVariableMap params(0);
            processParameters(actionElement, &params, aPs);

            // Get mode
            String modeStr;
            txExpandedName mode;
            if (actionElement->getAttr(txXSLTAtoms::mode,
                                       kNameSpaceID_None, modeStr)) {
                rv = mode.init(modeStr, actionElement, MB_FALSE);
                if (NS_FAILED(rv)) {
                    String err("malformed mode-name in xsl:apply-templates");
                    aPs->receiveError(err);
                    TX_IF_RELEASE_ATOM(localName);
                    return;
                }
            }

            txNodeSetContext evalContext(nodeSet, aPs);
            txIEvalContext* priorEC =
                aPs->setEvalContext(&evalContext);
            while (evalContext.hasNext()) {
                evalContext.next();
                ProcessorState::ImportFrame *frame;
                Node* currNode = evalContext.getContextNode();
                Node* xslTemplate;
                xslTemplate = aPs->findTemplate(currNode, mode, &frame);
                processMatchedTemplate(xslTemplate, &params, mode, frame, aPs);
            }

            aPs->setEvalContext(priorEC);
        }
        else {
            String err("error processing apply-templates");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }
        //-- clean up
        delete exprResult;
    }
    // xsl:attribute
    else if (localName == txXSLTAtoms::attribute) {
        String nameAttr;
        if (!actionElement->getAttr(txXSLTAtoms::name,
                                    kNameSpaceID_None, nameAttr)) {
            String err("missing required name attribute for xsl:attribute");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }

        // Process name as an AttributeValueTemplate
        String name;
        aPs->processAttrValueTemplate(nameAttr, actionElement, name);

        // Check name validity (must be valid QName and not xmlns)
        if (!XMLUtils::isValidQName(name)) {
            String err("error processing xsl:attribute, ");
            err.append(name);
            err.append(" is not a valid QName.");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }

        txAtom* nameAtom = TX_GET_ATOM(name);
        if (nameAtom == txXMLAtoms::xmlns) {
            TX_RELEASE_ATOM(nameAtom);
            String err("error processing xsl:attribute, name is xmlns.");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }
        TX_IF_RELEASE_ATOM(nameAtom);

        // Determine namespace URI from the namespace attribute or
        // from the prefix of the name (using the xslt action element).
        String resultNs;
        PRInt32 resultNsID = kNameSpaceID_None;
        if (actionElement->getAttr(txXSLTAtoms::_namespace, kNameSpaceID_None,
                                   resultNs)) {
            String nsURI;
            aPs->processAttrValueTemplate(resultNs, actionElement,
                                          nsURI);
            resultNsID = resultDoc->namespaceURIToID(nsURI);
        }
        else {
            String prefix;
            XMLUtils::getPrefix(name, prefix);
            txAtom* prefixAtom = TX_GET_ATOM(prefix);
            if (prefixAtom != txXMLAtoms::_empty) {
                if (prefixAtom != txXMLAtoms::xmlns)
                    resultNsID = actionElement->lookupNamespaceID(prefixAtom);
                else
                    // Cut xmlns: (6 characters)
                    name.deleteChars(0, 6);
            }
            TX_IF_RELEASE_ATOM(prefixAtom);
        }

        // XXX Should verify that this is correct behaviour. Signal error too?
        if (resultNsID == kNameSpaceID_Unknown) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        // Compute value
        String value;
        processChildrenAsValue(actionElement, aPs, MB_TRUE, value);

        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->attribute(name, resultNsID, value);
    }
    // xsl:call-template
    else if (localName == txXSLTAtoms::callTemplate) {
        String nameStr;
        txExpandedName templateName;
        actionElement->getAttr(txXSLTAtoms::name,
                               kNameSpaceID_None, nameStr);

        rv = templateName.init(nameStr, actionElement, MB_FALSE);
        if (NS_SUCCEEDED(rv)) {
            Element* xslTemplate = aPs->getNamedTemplate(templateName);
            if (xslTemplate) {
                PR_LOG(txLog::xslt, PR_LOG_DEBUG,
                       ("CallTemplate, Name %s, Stylesheet %s\n",
                        NS_LossyConvertUCS2toASCII(nameStr).get(),
                        NS_LossyConvertUCS2toASCII(xslTemplate->getBaseURI())
                        .get()));
                txVariableMap params(0);
                processParameters(actionElement, &params, aPs);
                processTemplate(xslTemplate, &params, aPs);
            }
        }
        else {
            String err("missing or malformed name in xsl:call-template");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }
    }
    // xsl:choose
    else if (localName == txXSLTAtoms::choose) {
        Node* condition = actionElement->getFirstChild();
        MBool caseFound = MB_FALSE;
        while (!caseFound && condition) {
            if (condition->getNodeType() != Node::ELEMENT_NODE ||
                condition->getNamespaceID() != kNameSpaceID_XSLT) {
                condition = condition->getNextSibling();
                continue;
            }

            Element* xslTemplate = (Element*)condition;
            txAtom* conditionLocalName;
            condition->getLocalName(&conditionLocalName);
            if (conditionLocalName == txXSLTAtoms::when) {
                expr = aPs->getExpr(xslTemplate,
                                    ProcessorState::TestAttr);
                if (!expr) {
                    TX_RELEASE_ATOM(conditionLocalName);
                    condition = condition->getNextSibling();
                    continue;
                }

                ExprResult* result = expr->evaluate
                    (aPs->getEvalContext());
                if (result && result->booleanValue()) {
                    processChildren(xslTemplate, aPs);
                    caseFound = MB_TRUE;
                }
                delete result;
            }
            else if (conditionLocalName == txXSLTAtoms::otherwise) {
                processChildren(xslTemplate, aPs);
                caseFound = MB_TRUE;
            }
            TX_IF_RELEASE_ATOM(conditionLocalName);
            condition = condition->getNextSibling();
        } // end for-each child of xsl:choose
    }
    // xsl:comment
    else if (localName == txXSLTAtoms::comment) {
        String value;
        processChildrenAsValue(actionElement, aPs, MB_TRUE, value);
        PRInt32 pos = 0;
        PRUint32 length = value.length();
        while ((pos = value.indexOf('-', pos)) != kNotFound) {
            ++pos;
            if (((PRUint32)pos == length) || (value.charAt(pos) == '-')) {
                value.insert(pos++, ' ');
                ++length;
            }
        }
        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->comment(value);
    }
    // xsl:copy
    else if (localName == txXSLTAtoms::copy) {
        xslCopy(actionElement, aPs);
    }
    // xsl:copy-of
    else if (localName == txXSLTAtoms::copyOf) {
        expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
        if (!expr) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        ExprResult* exprResult = expr->evaluate(aPs->getEvalContext());
        xslCopyOf(exprResult, aPs);
        delete exprResult;
    }
    // xsl:element
    else if (localName == txXSLTAtoms::element) {
        String nameAttr;
        if (!actionElement->getAttr(txXSLTAtoms::name,
                                    kNameSpaceID_None, nameAttr)) {
            String err("missing required name attribute for xsl:element");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }

        // Process name as an AttributeValueTemplate
        String name;
        aPs->processAttrValueTemplate(nameAttr, actionElement, name);

        // Check name validity (must be valid QName and not xmlns)
        if (!XMLUtils::isValidQName(name)) {
            String err("error processing xsl:element, '");
            err.append(name);
            err.append("' is not a valid QName.");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            // XXX We should processChildren without creating attributes or
            //     namespace nodes.
            TX_RELEASE_ATOM(localName);
            return;
        }

        // Determine namespace URI from the namespace attribute or
        // from the prefix of the name (using the xslt action element).
        String resultNs;
        PRInt32 resultNsID;
        if (actionElement->getAttr(txXSLTAtoms::_namespace, kNameSpaceID_None, resultNs)) {
            String nsURI;
            aPs->processAttrValueTemplate(resultNs, actionElement, nsURI);
            if (nsURI.isEmpty())
                resultNsID = kNameSpaceID_None;
            else
                resultNsID = resultDoc->namespaceURIToID(nsURI);
        }
        else {
            String prefix;
            XMLUtils::getPrefix(name, prefix);
            txAtom* prefixAtom = TX_GET_ATOM(prefix);
            resultNsID = actionElement->lookupNamespaceID(prefixAtom);
            TX_IF_RELEASE_ATOM(prefixAtom);
         }

        if (resultNsID == kNameSpaceID_Unknown) {
            String err("error processing xsl:element, can't resolve prefix on'");
            err.append(name);
            err.append("'.");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            // XXX We should processChildren without creating attributes or
            //     namespace nodes.
            TX_RELEASE_ATOM(localName);
            return;
        }

        startElement(aPs, name, resultNsID);
        processAttributeSets(actionElement, aPs);
        processChildren(actionElement, aPs);
        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->endElement(name, resultNsID);
    }
    // xsl:for-each
    else if (localName == txXSLTAtoms::forEach) {
        expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
        if (!expr) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        ExprResult* exprResult = expr->evaluate(aPs->getEvalContext());
        if (!exprResult) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        if (exprResult->getResultType() == ExprResult::NODESET) {
            NodeSet* nodeSet = (NodeSet*)exprResult;
            if (nodeSet->isEmpty()) {
                delete nodeSet;
                return;
            }
            txNodeSetContext evalContext(nodeSet, aPs);
            txIEvalContext* priorEC =
                aPs->setEvalContext(&evalContext);

            // Look for xsl:sort elements
            txNodeSorter sorter(aPs);
            Node* child = actionElement->getFirstChild();
            while (child) {
                unsigned short nodeType = child->getNodeType();
                if (nodeType == Node::ELEMENT_NODE) {
                    txAtom* childLocalName;
                    child->getLocalName(&childLocalName);
                    if (child->getNamespaceID() != kNameSpaceID_XSLT ||
                        childLocalName != txXSLTAtoms::sort) {
                        // xsl:sort must occur first
                        TX_IF_RELEASE_ATOM(childLocalName);
                        break;
                    }
                    sorter.addSortElement((Element*)child);
                    TX_RELEASE_ATOM(childLocalName);
                }
                else if ((nodeType == Node::TEXT_NODE ||
                          nodeType == Node::CDATA_SECTION_NODE) &&
                         !XMLUtils::isWhitespace(child->getNodeValue())) {
                    break;
                }

                child = child->getNextSibling();
            }
            sorter.sortNodeSet(nodeSet);

            // Set current template to null
            ProcessorState::TemplateRule *oldTemplate;
            oldTemplate = aPs->getCurrentTemplateRule();
            aPs->setCurrentTemplateRule(0);

            while (evalContext.hasNext()) {
                evalContext.next();
                processChildren(actionElement, aPs);
            }

            aPs->setCurrentTemplateRule(oldTemplate);
            aPs->setEvalContext(priorEC);
        }
        else {
            String err("error processing for-each");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }
        //-- clean up exprResult
        delete exprResult;
    }
    // xsl:if
    else if (localName == txXSLTAtoms::_if) {
        expr = aPs->getExpr(actionElement, ProcessorState::TestAttr);
        if (!expr) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        ExprResult* exprResult = expr->evaluate(aPs->getEvalContext());
        if (!exprResult) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        if ( exprResult->booleanValue() ) {
            processChildren(actionElement, aPs);
        }
        delete exprResult;
    }
    // xsl:message
    else if (localName == txXSLTAtoms::message) {
        String message;
        processChildrenAsValue(actionElement, aPs, MB_FALSE, message);
        // We should add a MessageObserver class
#ifdef TX_EXE
        cout << "xsl:message - "<< message << endl;
#else
        nsCOMPtr<nsIConsoleService> consoleSvc = 
          do_GetService("@mozilla.org/consoleservice;1", &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "xsl:message couldn't get console service");
        if (consoleSvc) {
            nsAutoString logString(NS_LITERAL_STRING("xsl:message - "));
            logString.Append(message);
            rv = consoleSvc->LogStringMessage(logString.get());
            NS_ASSERTION(NS_SUCCEEDED(rv), "xsl:message couldn't log");
        }
#endif
    }
    // xsl:number
    else if (localName == txXSLTAtoms::number) {
        String result;
        // XXX ErrorReport, propagate result from ::createNumber
        txXSLTNumber::createNumber(actionElement, aPs, result);
        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->characters(result);
    }
    // xsl:param
    else if (localName == txXSLTAtoms::param) {
        String err("misplaced xsl:param");
        aPs->receiveError(err, NS_ERROR_FAILURE);
    }
    // xsl:processing-instruction
    else if (localName == txXSLTAtoms::processingInstruction) {
        String nameAttr;
        if (!actionElement->getAttr(txXSLTAtoms::name,
                                    kNameSpaceID_None, nameAttr)) {
            String err("missing required name attribute for"
                       " xsl:processing-instruction");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }

        // Process name as an AttributeValueTemplate
        String name;
        aPs->processAttrValueTemplate(nameAttr, actionElement, name);

        // Check name validity (must be valid NCName and a PITarget)
        // XXX Need to check for NCName and PITarget
        if (!XMLUtils::isValidQName(name)) {
            String err("error processing xsl:processing-instruction, '");
            err.append(name);
            err.append("' is not a valid QName.");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }

        // Compute value
        String value;
        processChildrenAsValue(actionElement, aPs, MB_TRUE, value);
        XMLUtils::normalizePIValue(value);
        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
        mResultHandler->processingInstruction(name, value);
    }
    // xsl:sort
    else if (localName == txXSLTAtoms::sort) {
        // Ignore in this loop
    }
    // xsl:text
    else if (localName == txXSLTAtoms::text) {
        String data;
        XMLDOMUtils::getNodeValue(actionElement, data);

        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
#ifdef TX_EXE
        String aValue;
        if ((mResultHandler == mOutputHandler) &&
            actionElement->getAttr(txXSLTAtoms::disableOutputEscaping,
                                   kNameSpaceID_None, aValue) &&
            aValue.isEqual(YES_VALUE))
            mOutputHandler->charactersNoOutputEscaping(data);
        else
            mResultHandler->characters(data);
#else
        mResultHandler->characters(data);
#endif
    }
    // xsl:value-of
    else if (localName == txXSLTAtoms::valueOf) {
        expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
        if (!expr) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        ExprResult* exprResult = expr->evaluate(aPs->getEvalContext());
        String value;
        if (!exprResult) {
            String err("null ExprResult");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }
        exprResult->stringValue(value);

        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
#ifdef TX_EXE
        String aValue;
        if ((mResultHandler == mOutputHandler) &&
            actionElement->getAttr(txXSLTAtoms::disableOutputEscaping,
                                   kNameSpaceID_None, aValue) &&
            aValue.isEqual(YES_VALUE))
            mOutputHandler->charactersNoOutputEscaping(value);
        else
            mResultHandler->characters(value);
#else
        mResultHandler->characters(value);
#endif
        delete exprResult;
    }
    // xsl:variable
    else if (localName == txXSLTAtoms::variable) {
        txExpandedName varName;
        String qName;
        actionElement->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                               qName);
        rv = varName.init(qName, actionElement, MB_FALSE);
        if (NS_FAILED(rv)) {
            String err("bad name for xsl:variable");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            TX_RELEASE_ATOM(localName);
            return;
        }
        ExprResult* exprResult = processVariable(actionElement, aPs);
        if (!exprResult) {
            TX_RELEASE_ATOM(localName);
            return;
        }

        txVariableMap* vars = aPs->getLocalVariables();
        NS_ASSERTION(vars, "missing localvariable map");
        rv = vars->bindVariable(varName, exprResult, MB_TRUE);
        if (NS_FAILED(rv)) {
            String err("bad name for xsl:variable");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }
    }
    TX_IF_RELEASE_ATOM(localName);
}

/**
 * Processes the attribute sets specified in the use-attribute-sets attribute
 * of the element specified in aElement
**/
void XSLTProcessor::processAttributeSets(Element* aElement,
                                         ProcessorState* aPs,
                                         Stack* aRecursionStack)
{
    nsresult rv = NS_OK;
    String names;
    PRInt32 namespaceID;
    if (aElement->getNamespaceID() == kNameSpaceID_XSLT)
        namespaceID = kNameSpaceID_None;
    else
        namespaceID = kNameSpaceID_XSLT;
    if (!aElement->getAttr(txXSLTAtoms::useAttributeSets, namespaceID, names) || names.isEmpty())
        return;

    // Split names
    txTokenizer tokenizer(names);
    String nameStr;
    while (tokenizer.hasMoreTokens()) {
        tokenizer.nextToken(nameStr);
        txExpandedName name;
        rv = name.init(nameStr, aElement, MB_FALSE);
        if (NS_FAILED(rv)) {
            String err("missing or malformed name in use-attribute-sets");
            aPs->receiveError(err);
            return;
        }

        if (aRecursionStack) {
            txStackIterator attributeSets(aRecursionStack);
            while (attributeSets.hasNext()) {
                if (name == *(txExpandedName*)attributeSets.next()) {
                    String err("circular inclusion detected in use-attribute-sets");
                    aPs->receiveError(err);
                    return;
                }
            }
        }

        NodeSet* attSet = aPs->getAttributeSet(name);
        if (attSet) {
            int i;
            //-- issue: we still need to handle the following fix cleaner, since
            //-- attribute sets are merged, a different parent could exist
            //-- for different xsl:attribute nodes. I will probably create
            //-- an AttributeSet object, which will handle this case better. - Keith V.
            if (attSet->size() > 0) {
                Element* parent = (Element*) attSet->get(0)->getXPathParent();
                if (aRecursionStack) {
                    aRecursionStack->push(&name);
                    processAttributeSets(parent, aPs, aRecursionStack);
                    aRecursionStack->pop();
                }
                else {
                    Stack recursionStack;
                    recursionStack.push(&name);
                    processAttributeSets(parent, aPs, &recursionStack);
                    recursionStack.pop();
                }
            }
            for (i = 0; i < attSet->size(); i++)
                processAction(attSet->get(i), aPs);
            delete attSet;
        }
    }
} //-- processAttributeSets

/**
 * Processes the children of the specified element using the given context node
 * and ProcessorState
 * @param aXslElement template to be processed. Must be != NULL
 * @param aPs         current ProcessorState
**/
void XSLTProcessor::processChildren(Element* aXslElement,
                                    ProcessorState* aPs)
{
    NS_ASSERTION(aXslElement, "missing aXslElement");

    txVariableMap* oldVars = aPs->getLocalVariables();
    txVariableMap localVars(oldVars);
    aPs->setLocalVariables(&localVars);
    Node* child = aXslElement->getFirstChild();
    while (child) {
        processAction(child, aPs);
        child = child->getNextSibling();
    }
    aPs->setLocalVariables(oldVars);
} //-- processChildren

void
XSLTProcessor::processChildrenAsValue(Element* aElement,
                                      ProcessorState* aPs,
                                      MBool aOnlyText,
                                      String& aValue)
{
    txXMLEventHandler* previousHandler = mResultHandler;
    txTextHandler valueHandler(aValue, aOnlyText);
    mResultHandler = &valueHandler;
    processChildren(aElement, aPs);
    mResultHandler = previousHandler;
}

/**
 * Invokes the default template for the specified node
 * @param ps    current ProcessorState
 * @param mode  template mode
**/
void XSLTProcessor::processDefaultTemplate(ProcessorState* ps,
                                           const txExpandedName& mode)
{
    Node* node = ps->getEvalContext()->getContextNode();
    switch(node->getNodeType())
    {
        case Node::ELEMENT_NODE :
        case Node::DOCUMENT_NODE :
        {
            if (!mNodeExpr)
                break;

            ExprResult* exprResult = mNodeExpr->evaluate(ps->getEvalContext());
            if (!exprResult ||
                exprResult->getResultType() != ExprResult::NODESET) {
                String err("None-nodeset returned while processing default template");
                ps->receiveError(err, NS_ERROR_FAILURE);
                delete exprResult;
                return;
            }

            NodeSet* nodeSet = (NodeSet*)exprResult;
            if (nodeSet->isEmpty()) {
                delete nodeSet;
                return;
            }
            txNodeSetContext evalContext(nodeSet, ps);
            txIEvalContext* priorEC = ps->setEvalContext(&evalContext);

            while (evalContext.hasNext()) {
                evalContext.next();
                Node* currNode = evalContext.getContextNode();

                ProcessorState::ImportFrame *frame;
                Node* xslTemplate = ps->findTemplate(currNode, mode, &frame);
                processMatchedTemplate(xslTemplate, 0, mode, frame, ps);
            }
            ps->setEvalContext(priorEC);
            delete exprResult;
            break;
        }
        case Node::ATTRIBUTE_NODE :
        case Node::TEXT_NODE :
        case Node::CDATA_SECTION_NODE :
        {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->characters(node->getNodeValue());
            break;
        }
        default:
            // on all other nodetypes (including namespace nodes)
            // we do nothing
            break;
    }
} //-- processDefaultTemplate

/*
 * Processes an include or import stylesheet
 * @param aHref    URI of stylesheet to process
 * @param aImportFrame current importFrame iterator
 * @param aPs      current ProcessorState
 */
void XSLTProcessor::processInclude(String& aHref,
                                   txListIterator* aImportFrame,
                                   ProcessorState* aPs)
{
    // make sure the include isn't included yet
    txStackIterator iter(aPs->getEnteredStylesheets());
    while (iter.hasNext()) {
        if (((String*)iter.next())->isEqual(aHref)) {
            String err("Stylesheet includes itself. URI: ");
            err.append(aHref);
            aPs->receiveError(err, NS_ERROR_FAILURE);
            return;
        }
    }
    aPs->getEnteredStylesheets()->push(&aHref);

    // Load XSL document
    Node* stylesheet = aPs->retrieveDocument(aHref, String());
    if (!stylesheet) {
        String err("Unable to load included stylesheet ");
        err.append(aHref);
        aPs->receiveError(err, NS_ERROR_FAILURE);
        aPs->getEnteredStylesheets()->pop();
        return;
    }

    switch(stylesheet->getNodeType()) {
        case Node::DOCUMENT_NODE :
            processStylesheet((Document*)stylesheet,
                              aImportFrame, aPs);
            break;
        case Node::ELEMENT_NODE :
            processTopLevel((Element*)stylesheet,
                            aImportFrame, aPs);
            break;
        default:
            // This should never happen
            String err("Unsupported fragment identifier");
            aPs->receiveError(err, NS_ERROR_FAILURE);
            break;
    }

    aPs->getEnteredStylesheets()->pop();
}

void XSLTProcessor::processMatchedTemplate(Node* aXslTemplate,
                                           txVariableMap* aParams,
                                           const txExpandedName& aMode,
                                           ProcessorState::ImportFrame* aFrame,
                                           ProcessorState* aPs)
{
    if (aXslTemplate) {
        ProcessorState::TemplateRule *oldTemplate, newTemplate;
        oldTemplate = aPs->getCurrentTemplateRule();
        newTemplate.mFrame = aFrame;
        newTemplate.mMode = &aMode;
        newTemplate.mParams = aParams;
        aPs->setCurrentTemplateRule(&newTemplate);

        processTemplate(aXslTemplate, aParams, aPs);

        aPs->setCurrentTemplateRule(oldTemplate);
    }
    else {
        processDefaultTemplate(aPs, aMode);
    }
}

/*
 * Processes the xsl:with-param child elements of the given xsl action.
 * @param aAction  the action node that takes parameters (xsl:call-template
 *                 or xsl:apply-templates
 * @param aMap     map to place parsed variables in
 * @param aPs      the current ProcessorState
 * @return         errorcode
 */
nsresult XSLTProcessor::processParameters(Element* aAction,
                                          txVariableMap* aMap,
                                          ProcessorState* aPs)
{
    NS_ASSERTION(aAction && aMap, "missing argument");
    nsresult rv = NS_OK;

    Node* tmpNode = aAction->getFirstChild();
    while (tmpNode) {
        if (tmpNode->getNodeType() == Node::ELEMENT_NODE &&
            tmpNode->getNamespaceID() == kNameSpaceID_XSLT) {
            txAtom* localName;
            tmpNode->getLocalName(&localName);
            if (localName != txXSLTAtoms::withParam) {
                TX_IF_RELEASE_ATOM(localName);
                tmpNode = tmpNode->getNextSibling();
                continue;
            }

            Element* action = (Element*)tmpNode;
            txExpandedName paramName;
            String qName;
            action->getAttr(txXSLTAtoms::name, kNameSpaceID_None, qName);
            rv = paramName.init(qName, action, MB_FALSE);
            if (NS_FAILED(rv)) {
                String err("bad name for xsl:param");
                aPs->receiveError(err, NS_ERROR_FAILURE);
                break;
            }

            ExprResult* exprResult = processVariable(action, aPs);
            if (!exprResult) {
                return NS_ERROR_FAILURE;
            }

            rv = aMap->bindVariable(paramName, exprResult, MB_TRUE);
            if (NS_FAILED(rv)) {
                String err("Unable to bind parameter '");
                err.append(qName);
                err.append("'");
                aPs->receiveError(err, NS_ERROR_FAILURE);
                return rv;
            }
            TX_RELEASE_ATOM(localName);
        }
        tmpNode = tmpNode->getNextSibling();
    }
    return NS_OK;
}

/**
 * Processes the Top level elements for an XSL stylesheet
**/
void XSLTProcessor::processStylesheet(Document* aStylesheet,
                                      txListIterator* aImportFrame,
                                      ProcessorState* aPs)
{
    NS_ASSERTION(aStylesheet, "processTopLevel called without stylesheet");
    if (!aStylesheet || !aStylesheet->getDocumentElement())
        return;

    Element* elem = aStylesheet->getDocumentElement();

    txAtom* localName;
    PRInt32 namespaceID = elem->getNamespaceID();
    elem->getLocalName(&localName);

    if (((localName == txXSLTAtoms::stylesheet) ||
         (localName == txXSLTAtoms::transform)) &&
        (namespaceID == kNameSpaceID_XSLT)) {
        processTopLevel(elem, aImportFrame, aPs);
    }
    else {
        NS_ASSERTION(aImportFrame->current(), "no current importframe");
        if (!aImportFrame->current()) {
            TX_IF_RELEASE_ATOM(localName);
            return;
        }
        aPs->addLREStylesheet(aStylesheet,
            (ProcessorState::ImportFrame*)aImportFrame->current());
    }
    TX_IF_RELEASE_ATOM(localName);
}

/*
 * Processes the specified template using the given context,
 * ProcessorState, and parameters.
 * @param aTemplate the node in the xslt document that contains the
 *                  template
 * @param aParams   map with parameters to the template
 * @param aPs       the current ProcessorState
 */
void XSLTProcessor::processTemplate(Node* aTemplate,
                                    txVariableMap* aParams,
                                    ProcessorState* aPs)
{
    NS_ASSERTION(aTemplate, "aTemplate is NULL");

    nsresult rv;

    txVariableMap* oldVars = aPs->getLocalVariables();
    txVariableMap localVars(0);
    aPs->setLocalVariables(&localVars);

    // handle params
    Node* tmpNode = aTemplate->getFirstChild();
    while (tmpNode) {
        int nodeType = tmpNode->getNodeType();
        if (nodeType == Node::ELEMENT_NODE) {
            txAtom* localName;
            tmpNode->getLocalName(&localName);
            if (tmpNode->getNamespaceID() != kNameSpaceID_XSLT ||
                localName != txXSLTAtoms::param) {
                TX_RELEASE_ATOM(localName);
                break;
            }
            TX_RELEASE_ATOM(localName);

            Element* action = (Element*)tmpNode;
            txExpandedName paramName;
            String qName;
            action->getAttr(txXSLTAtoms::name, kNameSpaceID_None, qName);
            rv = paramName.init(qName, action, MB_FALSE);
            if (NS_FAILED(rv)) {
                String err("bad name for xsl:param");
                aPs->receiveError(err, NS_ERROR_FAILURE);
                break;
            }

            ExprResult* exprResult;
            if (aParams && (exprResult = aParams->getVariable(paramName))) {
                rv = localVars.bindVariable(paramName, exprResult, MB_FALSE);
            }
            else {
                exprResult = processVariable(action, aPs);
                if (!exprResult)
                    break;
                rv = localVars.bindVariable(paramName, exprResult, MB_TRUE);
            }

            if (NS_FAILED(rv)) {
                String err("unable to bind xsl:param");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }

        }
        else if (!(nodeType == Node::COMMENT_NODE ||
                  ((nodeType == Node::TEXT_NODE ||
                    nodeType == Node::CDATA_SECTION_NODE) &&
                   XMLUtils::isWhitespace(tmpNode->getNodeValue())))) {
            break;
        }
        tmpNode = tmpNode->getNextSibling();
    }

    // execute contents
    while (tmpNode) {
        processAction(tmpNode, aPs);
        tmpNode = tmpNode->getNextSibling();
    }
    aPs->setLocalVariables(oldVars);
}

/**
 * Processes the Top level elements for an XSL stylesheet
**/
void XSLTProcessor::processTopLevel(Element* aStylesheet,
                                    txListIterator* importFrame,
                                    ProcessorState* aPs)
{
    nsresult rv = NS_OK;

    // Index templates and process top level xsl elements
    NS_ASSERTION(aStylesheet, "processTopLevel called without stylesheet element");
    if (!aStylesheet)
        return;

    ProcessorState::ImportFrame* currentFrame =
        (ProcessorState::ImportFrame*)importFrame->current();

    NS_ASSERTION(currentFrame,
                 "processTopLevel called with no current importframe");
    if (!currentFrame)
        return;

    MBool importsDone = MB_FALSE;
    Node* node = aStylesheet->getFirstChild();
    while (node && !importsDone) {
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            txAtom* localName;
            node->getLocalName(&localName);
            if (node->getNamespaceID() == kNameSpaceID_XSLT &&
                localName == txXSLTAtoms::import) {
                Element* element = (Element*)node;
                String hrefAttr, href;
                element->getAttr(txXSLTAtoms::href, kNameSpaceID_None,
                                 hrefAttr);
                URIUtils::resolveHref(hrefAttr, element->getBaseURI(),
                                      href);

                // Create a new ImportFrame with correct firstNotImported
                ProcessorState::ImportFrame *nextFrame, *newFrame;
                nextFrame =
                    (ProcessorState::ImportFrame*)importFrame->next();
                newFrame = new ProcessorState::ImportFrame(nextFrame);
                if (!newFrame) {
                    // XXX ErrorReport: out of memory
                    break;
                }

                // Insert frame and process stylesheet
                importFrame->addBefore(newFrame);
                importFrame->previous();
                processInclude(href, importFrame, aPs);

                // Restore iterator to initial position
                importFrame->previous();
            }
            else {
                importsDone = MB_TRUE;
            }
            TX_IF_RELEASE_ATOM(localName);
        }
        if (!importsDone)
            node = node->getNextSibling();
    }

    while (node) {
        if (node->getNodeType() != Node::ELEMENT_NODE ||
            node->getNamespaceID() != kNameSpaceID_XSLT) {
            node = node->getNextSibling();
            continue;
        }

        txAtom* localName;
        node->getLocalName(&localName);
        Element* element = (Element*)node;
        // xsl:attribute-set
        if (localName == txXSLTAtoms::attributeSet) {
            aPs->addAttributeSet(element, currentFrame);
        }
        // xsl:decimal-format
        else if (localName == txXSLTAtoms::decimalFormat) {
            if (!aPs->addDecimalFormat(element)) {
                // Add error to ErrorObserver
                String fName;
                element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                 fName);
                String err("unable to add ");
                if (fName.isEmpty()) {
                    err.append("default");
                }
                else {
                    err.append("\"");
                    err.append(fName);
                    err.append("\"");
                }
                err.append(" decimal format for xsl:decimal-format");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
        }
        // xsl:param
        else if (localName == txXSLTAtoms::param) {
            // Once we support stylesheet parameters we should process the
            // value here. For now we just use the default value
            // See bug 73492
            rv = aPs->addGlobalVariable(element, currentFrame);
            if (NS_FAILED(rv)) {
                String err("unable to add global xsl:param");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
        }
        // xsl:import
        else if (localName == txXSLTAtoms::import) {
            String err("xsl:import only allowed at top of stylesheet");
            aPs->receiveError(err, NS_ERROR_FAILURE);
        }
        // xsl:include
        else if (localName == txXSLTAtoms::include) {
            String hrefAttr, href;
            element->getAttr(txXSLTAtoms::href, kNameSpaceID_None,
                             hrefAttr);
            URIUtils::resolveHref(hrefAttr, element->getBaseURI(),
                                  href);

            processInclude(href, importFrame, aPs);
        }
        // xsl:key
        else if (localName == txXSLTAtoms::key) {
            if (!aPs->addKey(element)) {
                String name;
                element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                 name);
                String err("error adding key '");
                err.append(name);
                err.append("'");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
        }
        // xsl:output
        else if (localName == txXSLTAtoms::output) {
            txOutputFormat& format = currentFrame->mOutputFormat;
            String attValue;

            if (element->getAttr(txXSLTAtoms::method, kNameSpaceID_None,
                                 attValue)) {
                if (attValue.isEqual("html")) {
                    format.mMethod = eHTMLOutput;
                }
                else if (attValue.isEqual("text")) {
                    format.mMethod = eTextOutput;
                }
                else {
                    format.mMethod = eXMLOutput;
                }
            }

            if (element->getAttr(txXSLTAtoms::version, kNameSpaceID_None,
                                 attValue)) {
                format.mVersion = attValue;
            }

            if (element->getAttr(txXSLTAtoms::encoding, kNameSpaceID_None,
                                 attValue)) {
                format.mEncoding = attValue;
            }

            if (element->getAttr(txXSLTAtoms::omitXmlDeclaration,
                                 kNameSpaceID_None, attValue)) {
                format.mOmitXMLDeclaration = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;
            }

            if (element->getAttr(txXSLTAtoms::standalone, kNameSpaceID_None,
                                 attValue)) {
                format.mStandalone = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;
            }

            if (element->getAttr(txXSLTAtoms::doctypePublic,
                                 kNameSpaceID_None, attValue)) {
                format.mPublicId = attValue;
            }

            if (element->getAttr(txXSLTAtoms::doctypeSystem,
                                 kNameSpaceID_None, attValue)) {
                format.mSystemId = attValue;
            }

            if (element->getAttr(txXSLTAtoms::cdataSectionElements,
                                 kNameSpaceID_None, attValue)) {
                txTokenizer tokens(attValue);
                String token;
                while (tokens.hasMoreTokens()) {
                    tokens.nextToken(token);
                    if (!XMLUtils::isValidQName(token)) {
                        break;
                    }

                    String namePart;
                    XMLUtils::getPrefix(token, namePart);
                    txAtom* nameAtom = TX_GET_ATOM(namePart);
                    PRInt32 nsID = element->lookupNamespaceID(nameAtom);
                    TX_IF_RELEASE_ATOM(nameAtom);
                    if (nsID == kNameSpaceID_Unknown) {
                        // XXX ErrorReport: unknown prefix
                        break;
                    }
                    XMLUtils::getLocalPart(token, namePart);
                    nameAtom = TX_GET_ATOM(namePart);
                    if (!nameAtom) {
                        // XXX ErrorReport: out of memory
                        break;
                    }
                    txExpandedName* qname = new txExpandedName(nsID, nameAtom);
                    TX_RELEASE_ATOM(nameAtom);
                    if (!qname) {
                        // XXX ErrorReport: out of memory
                        break;
                    }
                    format.mCDATASectionElements.add(qname);
                }
            }

            if (element->getAttr(txXSLTAtoms::indent, kNameSpaceID_None,
                                 attValue)) {
                format.mIndent = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;
            }

            if (element->getAttr(txXSLTAtoms::mediaType, kNameSpaceID_None,
                                 attValue)) {
                format.mMediaType = attValue;
            }
        }
        // xsl:template
        else if (localName == txXSLTAtoms::_template) {
            aPs->addTemplate(element, currentFrame);
        }
        // xsl:variable
        else if (localName == txXSLTAtoms::variable) {
            rv = aPs->addGlobalVariable(element, currentFrame);
            if (NS_FAILED(rv)) {
                String err("unable to add global xsl:variable");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
        }
        // xsl:preserve-space
        else if (localName == txXSLTAtoms::preserveSpace) {
            String elements;
            if (!element->getAttr(txXSLTAtoms::elements,
                                  kNameSpaceID_None, elements)) {
                //-- add error to ErrorObserver
                String err("missing required 'elements' attribute for ");
                err.append("xsl:preserve-space");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
            else {
                aPs->shouldStripSpace(elements, element,
                                      MB_FALSE,
                                      currentFrame);
            }
        }
        // xsl:strip-space
        else if (localName == txXSLTAtoms::stripSpace) {
            String elements;
            if (!element->getAttr(txXSLTAtoms::elements,
                                  kNameSpaceID_None, elements)) {
                //-- add error to ErrorObserver
                String err("missing required 'elements' attribute for ");
                err.append("xsl:strip-space");
                aPs->receiveError(err, NS_ERROR_FAILURE);
            }
            else {
                aPs->shouldStripSpace(elements, element,
                                      MB_TRUE,
                                      currentFrame);
            }
        }
        TX_IF_RELEASE_ATOM(localName);
        node = node->getNextSibling();
    }
}

/**
 *  processes the xslVariable parameter as an xsl:variable using the given context,
 *  and ProcessorState.
 *  If the xslTemplate contains an "expr" attribute, the attribute is evaluated
 *  as an Expression and the ExprResult is returned. Otherwise the xslVariable is
 *  is processed as a template, and it's result is converted into an ExprResult
 *  @return an ExprResult
**/
ExprResult* XSLTProcessor::processVariable
        (Element* xslVariable, ProcessorState* ps)
{
    NS_ASSERTION(xslVariable, "missing xslVariable");

    //-- check for select attribute
    if (xslVariable->hasAttr(txXSLTAtoms::select, kNameSpaceID_None)) {
        Expr* expr = ps->getExpr(xslVariable, ProcessorState::SelectAttr);
        if (!expr)
            return new StringResult("unable to process variable");
        return expr->evaluate(ps->getEvalContext());
    }
    else if (xslVariable->hasChildNodes()) {
        txResultTreeFragment* rtf = new txResultTreeFragment();
        if (!rtf)
            // XXX ErrorReport: Out of memory
            return 0;
        txXMLEventHandler* previousHandler = mResultHandler;
        txRtfHandler rtfHandler(ps->getResultDocument(), rtf);
        mResultHandler = &rtfHandler;
        processChildren(xslVariable, ps);
        //NS_ASSERTION(previousHandler, "Setting mResultHandler to NULL!");
        mResultHandler = previousHandler;
        return rtf;
    }
    else {
        return new StringResult();
    }
} //-- processVariable

#ifdef TX_EXE

// XXX START
// XXX Standalone only code. This should move to txStandaloneXSLTProcessor
// XXX

/*
 * Processes the given XML Document using the given XSL document
 * and returns the result tree
 */
Document* XSLTProcessor::process(Document& xmlDocument,
                                 Document& xslDocument)
{
    Document* result = new Document();
    if (!result)
        // XXX ErrorReport: out of memory
        return 0;

    /* XXX Disabled for now, need to implement a handler
           that creates a result tree.
    // Start of block to ensure the destruction of the ProcessorState
    // before the destruction of the result document.
    {
        // Create a new ProcessorState
        ProcessorState ps(&aXMLDocument, &aXSLTDocument, &result);
    
        // Add error observers
        txListIterator iter(&errorObservers);
        while (iter.hasNext())
            ps.addErrorObserver(*(ErrorObserver*)iter.next());
    
        txSingleNodeContext evalContext(&aXMLDocument, &ps);
        ps.setEvalContext(&evalContext);

        // Index templates and process top level xsl elements
        txListIterator importFrame(ps.getImportFrames());
        importFrame.addAfter(new ProcessorState::ImportFrame(0));
        if (!importFrame.next()) {
            delete result;
            // XXX ErrorReport: out of memory
            return 0;
        }
        processStylesheet(&aXMLDocument, &aXSLTDocument, &importFrame, &ps);
    
        initializeHandlers(&ps);
        // XXX Set the result document on the handler
    
        // Process root of XML source document
        startTransform(&aXMLDocument, &ps);
    }
    // End of block to ensure the destruction of the ProcessorState
    // before the destruction of the result document.
       XXX End of disabled section */

    // Return result Document
    return result;
}

/*
 * Processes the given XML Document using the given XSL document
 * and prints the results to the given ostream argument
 */
void XSLTProcessor::process(Document& aXMLDocument,
                            Node& aStylesheet,
                            ostream& aOut)
{
    // Need a result document for creating result tree fragments.
    Document result;

    // Start of block to ensure the destruction of the ProcessorState
    // before the destruction of the result document.
    {
        // Create a new ProcessorState
        Document* stylesheetDoc = 0;
        Element* stylesheetElem = 0;
        if (aStylesheet.getNodeType() == Node::DOCUMENT_NODE) {
            stylesheetDoc = (Document*)&aStylesheet;
        }
        else {
            stylesheetElem = (Element*)&aStylesheet;
            stylesheetDoc = aStylesheet.getOwnerDocument();
        }
        ProcessorState ps(&aXMLDocument, stylesheetDoc, &result);

        // Add error observers
        txListIterator iter(&errorObservers);
        while (iter.hasNext())
            ps.addErrorObserver(*(ErrorObserver*)iter.next());

        txSingleNodeContext evalContext(&aXMLDocument, &ps);
        ps.setEvalContext(&evalContext);

        // Index templates and process top level xsl elements
        txListIterator importFrame(ps.getImportFrames());
        importFrame.addAfter(new ProcessorState::ImportFrame(0));
        if (!importFrame.next())
            // XXX ErrorReport: out of memory
            return;
        
        if (stylesheetElem)
            processTopLevel(stylesheetElem, &importFrame, &ps);
        else
            processStylesheet(stylesheetDoc, &importFrame, &ps);

        initializeHandlers(&ps);
        if (mOutputHandler)
            mOutputHandler->setOutputStream(&aOut);

        // Process root of XML source document
        startTransform(&aXMLDocument, &ps);
    }
    // End of block to ensure the destruction of the ProcessorState
    // before the destruction of the result document.
}

/**
 * Reads an XML Document from the given XML input stream.
 * The XSL Stylesheet is obtained from the XML Documents stylesheet PI.
 * If no Stylesheet is found, an empty document will be the result;
 * otherwise the XML Document is processed using the stylesheet.
 * The result tree is printed to the given ostream argument,
 * will not close the ostream argument
**/
void XSLTProcessor::process
   (istream& xmlInput, String& xmlFilename, ostream& out)
{

    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, xmlFilename);
    if (!xmlDoc) {
        String err("error reading XML document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        return;
    }
    //-- Read in XSL document
    String href;
    String errMsg;
    getHrefFromStylesheetPI(*xmlDoc, href);
    istream* xslInput = URIUtils::getInputStream(href,errMsg);
    Document* xslDoc = 0;
    if ( xslInput ) {
        xslDoc = xmlParser.parse(*xslInput, href);
        delete xslInput;
    }
    if (!xslDoc) {
        String err("error reading XSL stylesheet document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        delete xmlDoc;
        return;
    }

    Node* stylesheet;
    String frag;
    URIUtils::getFragmentIdentifier(href, frag);
    if (!frag.isEmpty()) {
        stylesheet = xslDoc->getElementById(frag);
        if (!stylesheet) {
            String err("unable to get fragment");
            cerr << err << endl;
            delete xmlDoc;
            delete xslDoc;
            return;
        }
    }
    else {
        stylesheet = xslDoc;
    }

    process(*xmlDoc, *stylesheet, out);
    delete xmlDoc;
    delete xslDoc;
} //-- process

/**
 * Reads an XML Document from the given XML input stream, and
 * processes the document using the XSL document derived from
 * the given XSL input stream.
 * The result tree is printed to the given ostream argument,
 * will not close the ostream argument
**/
void XSLTProcessor::process
   (istream& xmlInput, String& xmlFilename,
    istream& xslInput, String& xslFilename,
    ostream& out)
{
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, xmlFilename);
    if (!xmlDoc) {
        String err("error reading XML document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        return;
    }
    //-- read in XSL Document
    Document* xslDoc = xmlParser.parse(xslInput, xslFilename);
    if (!xslDoc) {
        String err("error reading XSL stylesheet document: ");
        err.append(xmlParser.getErrorString());
        cerr << err << endl;
        delete xmlDoc;
        return;
    }
    process(*xmlDoc, *xslDoc, out);
    delete xmlDoc;
    delete xslDoc;
} //-- process

// XXX
// XXX Standalone only code. This should move to txStandaloneXSLTProcessor
// XXX END

#endif // ifdef TX_EXE

  //-------------------/
 //- Private Methods -/
//-------------------/

void XSLTProcessor::process(Node* node,
                            const String& mode,
                            ProcessorState* ps) {
    if (!node)
        return;

    ProcessorState::ImportFrame *frame;
    txExpandedName nullMode;
    Node* xslTemplate = ps->findTemplate(node, nullMode, &frame);
    processMatchedTemplate(xslTemplate, 0, nullMode, frame, ps);
} //-- process

void XSLTProcessor::startTransform(Node* aNode, ProcessorState* aPs)
{
    mHaveDocumentElement = MB_FALSE;
    mOutputHandler->startDocument();
    process(aNode, String(), aPs);
#ifdef TX_EXE
    txOutputFormat* format = aPs->getOutputFormat();
    if (format->mMethod == eMethodNotSet) {
        // This is an unusual case, no output method has been set and we
        // didn't create a document element. Switching to XML output mode
        // anyway.
        txUnknownHandler* oldHandler = (txUnknownHandler*)mOutputHandler;
        ostream* out;
        oldHandler->getOutputStream(&out);
        mOutputHandler = new txXMLOutput();
        format->mMethod = eXMLOutput;
        NS_ASSERTION(mOutputHandler, "Setting mResultHandler to NULL!");
        mOutputHandler->setOutputStream(out);
        mOutputHandler->setOutputFormat(format);
        mResultHandler = mOutputHandler;
        oldHandler->flush(mOutputHandler);
        delete oldHandler;
    }
#endif
    mOutputHandler->endDocument();
}

MBool XSLTProcessor::initializeHandlers(ProcessorState* aPs)
{
    txListIterator frameIter(aPs->getImportFrames());
    ProcessorState::ImportFrame* frame;
    txOutputFormat* outputFormat = aPs->getOutputFormat();
    while ((frame = (ProcessorState::ImportFrame*)frameIter.next()))
        outputFormat->merge(frame->mOutputFormat);

    delete mOutputHandler;
#ifdef TX_EXE
    switch (outputFormat->mMethod) {
        case eXMLOutput:
        {
            mOutputHandler = new txXMLOutput();
            break;
        }
        case eHTMLOutput:
        {
            mOutputHandler = new txHTMLOutput();
            break;
        }
        case eTextOutput:
        {
            mOutputHandler = new txTextOutput();
            break;
        }
        case eMethodNotSet:
        {
            mOutputHandler = new txUnknownHandler();
            break;
        }
    }
#else
    switch (outputFormat->mMethod) {
        case eMethodNotSet:
        case eXMLOutput:
        case eHTMLOutput:
        {
            mOutputHandler = new txMozillaXMLOutput();
            break;
        }
        case eTextOutput:
        {
            mOutputHandler = new txMozillaTextOutput();
            break;
        }
    }
#endif

    mResultHandler = mOutputHandler;
    if (!mOutputHandler)
        return MB_FALSE;
    mOutputHandler->setOutputFormat(outputFormat);
    return MB_TRUE;
}

void
XSLTProcessor::startElement(ProcessorState* aPs, const String& aName, const PRInt32 aNsID)
{
    if (!mHaveDocumentElement && (mResultHandler == mOutputHandler)) {
        txOutputFormat* format = aPs->getOutputFormat();
        if (format->mMethod == eMethodNotSet) {
            // XXX Should check for whitespace-only sibling text nodes
            if ((aNsID == kNameSpaceID_None) &&
                aName.isEqualIgnoreCase(String("html"))) {
                // Switch to html output mode according to the XSLT spec.
                format->mMethod = eHTMLOutput;
#ifndef TX_EXE
                mOutputHandler->setOutputFormat(format);
#endif
            }
#ifdef TX_EXE
            ostream* out;
            mOutputHandler->getOutputStream(&out);
            txUnknownHandler* oldHandler = (txUnknownHandler*)mOutputHandler;
            if (format->mMethod == eHTMLOutput) {
                mOutputHandler = new txHTMLOutput();
            }
            else {
                mOutputHandler = new txXMLOutput();
                format->mMethod = eXMLOutput;
            }
            NS_ASSERTION(mOutputHandler, "Setting mResultHandler to NULL!");
            mOutputHandler->setOutputStream(out);
            mOutputHandler->setOutputFormat(format);
            mResultHandler = mOutputHandler;
            oldHandler->flush(mOutputHandler);
            delete oldHandler;
#endif
        }
        mHaveDocumentElement = MB_TRUE;
    }
    NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
    mResultHandler->startElement(aName, aNsID);
}

#ifndef TX_EXE

// XXX START
// XXX Mozilla module only code. This should move to txMozillaXSLTProcessor
// XXX

NS_IMETHODIMP
XSLTProcessor::TransformDocument(nsIDOMNode* aSourceDOM,
                                 nsIDOMNode* aStyleDOM,
                                 nsIDOMDocument* aOutputDoc,
                                 nsITransformObserver* aObserver)
{
    NS_ENSURE_ARG(aSourceDOM);
    NS_ENSURE_ARG(aStyleDOM);
    NS_ENSURE_ARG(aOutputDoc);
    
    if (!URIUtils::CanCallerAccess(aSourceDOM) ||
        !URIUtils::CanCallerAccess(aStyleDOM) ||
        !URIUtils::CanCallerAccess(aOutputDoc))
        return NS_ERROR_DOM_SECURITY_ERR;

    // Create wrapper for the source document.
    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    aSourceDOM->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    if (!sourceDOMDocument)
        sourceDOMDocument = do_QueryInterface(aSourceDOM);
    NS_ENSURE_TRUE(sourceDOMDocument, NS_ERROR_FAILURE);
    Document sourceDocument(sourceDOMDocument);
    Node* sourceNode = sourceDocument.createWrapper(aSourceDOM);
    NS_ENSURE_TRUE(sourceNode, NS_ERROR_FAILURE);

    // Create wrapper for the style document.
    nsCOMPtr<nsIDOMDocument> styleDOMDocument;
    aStyleDOM->GetOwnerDocument(getter_AddRefs(styleDOMDocument));
    if (!styleDOMDocument)
        styleDOMDocument = do_QueryInterface(aStyleDOM);
    Document xslDocument(styleDOMDocument);

    // Create wrapper for the output document.
    nsCOMPtr<nsIDocument> document = do_QueryInterface(aOutputDoc);
    NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);
    Document resultDocument(aOutputDoc);

    // Reset the output document.
    // Create a temporary channel to get nsIDocument->Reset to
    // do the right thing.
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIURI> docURL;

    document->GetDocumentURL(getter_AddRefs(docURL));
    NS_ASSERTION(docURL, "No document URL");
    document->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    if (!loadGroup) {
        nsCOMPtr<nsIDocument> source = do_QueryInterface(sourceDOMDocument);
        if (source) {
            source->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
        }
    }

    nsresult rv = NS_NewChannel(getter_AddRefs(channel), docURL,
                                nsnull, loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    // Start of hack for keeping the scrollbars on an existing document.
    // Based on similar hack that jst wrote for document.open().
    // See bugs 78070 and 55334.
    nsCOMPtr<nsIContent> root;
    document->GetRootContent(getter_AddRefs(root));
    if (root) {
        document->SetRootContent(nsnull);
    }

    // Call Reset(), this will now do the full reset, except removing
    // the root from the document, doing that confuses the scrollbar
    // code in mozilla since the document in the root element and all
    // the anonymous content (i.e. scrollbar elements) is set to
    // null.
    rv = document->Reset(channel, loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    if (root) {
        // Tear down the frames for the root element.
        document->ContentRemoved(nsnull, root, 0);
    }
    // End of hack for keeping the scrollbars on an existing document.

    // Start of block to ensure the destruction of the ProcessorState
    // before the destruction of the documents.
    {
        // Create a new ProcessorState
        ProcessorState ps(&sourceDocument, &xslDocument, &resultDocument);

        // XXX Need to add error observers

        // Set current txIEvalContext
        txSingleNodeContext evalContext(&sourceDocument, &ps);
        ps.setEvalContext(&evalContext);

        // Index templates and process top level xsl elements
        txListIterator importFrame(ps.getImportFrames());
        importFrame.addAfter(new ProcessorState::ImportFrame(0));
        if (!importFrame.next())
            return NS_ERROR_OUT_OF_MEMORY;
        nsCOMPtr<nsIDOMDocument> styleDoc = do_QueryInterface(aStyleDOM);
        if (styleDoc) {
            processStylesheet(&xslDocument, &importFrame, &ps);
        }
        else {
            nsCOMPtr<nsIDOMElement> styleElem = do_QueryInterface(aStyleDOM);
            NS_ENSURE_TRUE(styleElem, NS_ERROR_FAILURE);
            Element* element = xslDocument.createElement(styleElem);
            NS_ENSURE_TRUE(element, NS_ERROR_OUT_OF_MEMORY);
            processTopLevel(element, &importFrame, &ps);
        }

        initializeHandlers(&ps);

        if (mOutputHandler) {
            mOutputHandler->setOutputDocument(aOutputDoc);
        }

        // Get the script loader of the result document.
        if (aObserver) {
            document->GetScriptLoader(getter_AddRefs(mScriptLoader));
            if (mScriptLoader) {
                mScriptLoader->AddObserver(this);
            }
        }

        // Process root of XML source document
        startTransform(sourceNode, &ps);
    }
    // End of block to ensure the destruction of the ProcessorState
    // before the destruction of the documents.

    mOutputHandler->getRootContent(getter_AddRefs(root));
    if (root) {
        document->ContentInserted(nsnull, root, 0);
    }

    mObserver = do_GetWeakReference(aObserver);
    SignalTransformEnd();

    return NS_OK;
}

NS_IMETHODIMP
XSLTProcessor::ScriptAvailable(nsresult aResult, 
                               nsIDOMHTMLScriptElement *aElement, 
                               PRBool aIsInline,
                               PRBool aWasPending,
                               nsIURI *aURI, 
                               PRInt32 aLineNo,
                               const nsAString& aScript)
{
    if (NS_FAILED(aResult) && mOutputHandler) {
        mOutputHandler->removeScriptElement(aElement);
        SignalTransformEnd();
    }

    return NS_OK;
}

NS_IMETHODIMP 
XSLTProcessor::ScriptEvaluated(nsresult aResult, 
                               nsIDOMHTMLScriptElement *aElement,
                               PRBool aIsInline,
                               PRBool aWasPending)
{
    if (mOutputHandler) {
        mOutputHandler->removeScriptElement(aElement);
        SignalTransformEnd();
    }

    return NS_OK;
}

void
XSLTProcessor::SignalTransformEnd()
{
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (!observer)
        return;

    if (!mOutputHandler || !mOutputHandler->isDone())
        return;

    if (mScriptLoader) {
        mScriptLoader->RemoveObserver(this);
        mScriptLoader = nsnull;
    }

    mObserver = nsnull;

    // XXX Need a better way to determine transform success/failure
    nsCOMPtr<nsIContent> rootContent;
    mOutputHandler->getRootContent(getter_AddRefs(rootContent));
    nsCOMPtr<nsIDOMNode> root = do_QueryInterface(rootContent);
    if (root) {
      nsCOMPtr<nsIDOMDocument> resultDoc;
      root->GetOwnerDocument(getter_AddRefs(resultDoc));
      observer->OnTransformDone(NS_OK, resultDoc);
    }
    else {
      // XXX Need better error message and code.
      observer->OnTransformDone(NS_ERROR_FAILURE, nsnull);
    }
}

// XXX
// XXX Mozilla module only code. This should move to txMozillaXSLTProcessor
// XXX END

#endif

/**
 * Performs the xsl:copy action as specified in the XSLT specification
 */
void XSLTProcessor::xslCopy(Element* aAction, ProcessorState* aPs)
{
    Node* node = aPs->getEvalContext()->getContextNode();

    switch (node->getNodeType()) {
        case Node::DOCUMENT_NODE:
        {
            // Just process children
            processChildren(aAction, aPs);
            break;
        }
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)node;
            String nodeName = element->getNodeName();
            PRInt32 nsID = element->getNamespaceID();

            startElement(aPs, nodeName, nsID);
            // XXX copy namespace attributes once we have them
            processAttributeSets(aAction, aPs);
            processChildren(aAction, aPs);
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->endElement(nodeName, nsID);
            break;
        }
        default:
        {
            copyNode(node, aPs);
        }
    }
}

/**
 * Performs the xsl:copy-of action as specified in the XSLT specification
 */
void XSLTProcessor::xslCopyOf(ExprResult* aExprResult, ProcessorState* aPs)
{
    if (!aExprResult)
        return;

    switch (aExprResult->getResultType()) {
        case ExprResult::NODESET:
        {
            NodeSet* nodes = (NodeSet*)aExprResult;
            int i;
            for (i = 0; i < nodes->size(); i++) {
                Node* node = nodes->get(i);
                copyNode(node, aPs);
            }
            break;
        }
        default:
        {
            String value;
            aExprResult->stringValue(value);
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->characters(value);
        }
    }
}
