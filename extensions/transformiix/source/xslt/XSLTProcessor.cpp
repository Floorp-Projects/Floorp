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
#include "Tokenizer.h"
#include "txAtoms.h"
#include "TxLog.h"
#include "txNodeSetContext.h"
#include "txNodeSorter.h"
#include "txRtfHandler.h"
#include "txTextHandler.h"
#include "txURIUtils.h"
#include "txVariableMap.h"
#include "txXSLTNumber.h"
#include "XMLDOMUtils.h"
#include "XMLUtils.h"

#ifndef TX_EXE
#include "nsContentCID.h"
#include "nsIConsoleService.h"
#include "nsIDOMDocument.h"
#include "nsIServiceManagerUtils.h"

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
#endif

Expr* txXSLTProcessor::gNodeExpr = 0;

/*
 * Implement static variables for atomservice and dom.
 */
#ifdef TX_EXE
TX_IMPL_DOM_STATICS;
#endif

TX_LG_IMPL;

/* static */
MBool
txXSLTProcessor::txInit()
{
    TX_LG_CREATE;
#ifdef TX_EXE
    if (!txNamespaceManager::init())
        return MB_FALSE;
#endif

    // Create default expressions
    // "node()"
    txNodeTest* nt = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
    if (!nt) {
        return MB_FALSE;
    }
    gNodeExpr = new LocationStep(nt, LocationStep::CHILD_AXIS);
    if (!gNodeExpr) {
        return MB_FALSE;
    }

    if (!txHTMLAtoms::init())
        return MB_FALSE;
    if (!txXMLAtoms::init())
        return MB_FALSE;
    if (!txXPathAtoms::init())
        return MB_FALSE;
    return txXSLTAtoms::init();
}

/* static */
MBool
txXSLTProcessor::txShutdown()
{
    TX_LG_DELETE;
#ifdef TX_EXE
    txNamespaceManager::shutdown();
#endif

    delete gNodeExpr;

    txHTMLAtoms::shutdown();
    txXMLAtoms::shutdown();
    txXPathAtoms::shutdown();
    txXSLTAtoms::shutdown();
    return MB_TRUE;
}

void
txXSLTProcessor::copyNode(Node* aSourceNode, ProcessorState* aPs)
{
    if (!aSourceNode)
        return;

    switch (aSourceNode->getNodeType()) {
        case Node::ATTRIBUTE_NODE:
        {
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->attribute(aSourceNode->getNodeName(),
                                           aSourceNode->getNamespaceID(),
                                           aSourceNode->getNodeValue());
            break;
        }
        case Node::COMMENT_NODE:
        {
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->comment(aSourceNode->getNodeValue());
            break;
        }
        case Node::DOCUMENT_NODE:
        case Node::DOCUMENT_FRAGMENT_NODE:
        {
            // Copy children
            Node* child = aSourceNode->getFirstChild();
            while (child) {
                copyNode(child, aPs);
                child = child->getNextSibling();
            }
            break;
        }
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)aSourceNode;
            const String& name = element->getNodeName();
            PRInt32 nsID = element->getNamespaceID();
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->startElement(name, nsID);

            // Copy attributes
            NamedNodeMap* attList = element->getAttributes();
            if (attList) {
                PRUint32 i = 0;
                for (i = 0; i < attList->getLength(); i++) {
                    Attr* attr = (Attr*)attList->item(i);
                    NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
                    aPs->mResultHandler->attribute(attr->getNodeName(),
                                                   attr->getNamespaceID(),
                                                   attr->getNodeValue());
                }
            }

            // Copy children
            Node* child = element->getFirstChild();
            while (child) {
                copyNode(child, aPs);
                child = child->getNextSibling();
            }

            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->endElement(name, nsID);
            break;
        }
        case Node::PROCESSING_INSTRUCTION_NODE:
        {
            ProcessingInstruction* pi = (ProcessingInstruction*)aSourceNode;
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->processingInstruction(pi->getTarget(), pi->getData());
            break;
        }
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        {
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->characters(aSourceNode->getNodeValue());
            break;
        }
    }
}

Document*
txXSLTProcessor::createRTFDocument(txOutputMethod aMethod)
{
#ifdef TX_EXE
    return new Document();
#else
    nsresult rv;
    nsCOMPtr<nsIDOMDocument> domDoc = do_CreateInstance(kXMLDocumentCID, &rv);
    NS_ENSURE_SUCCESS(rv, nsnull);
    return new Document(domDoc);
#endif
}

void
txXSLTProcessor::logMessage(const String& aMessage)
{
#ifdef TX_EXE
    cout << "xsl:message - "<< aMessage << endl;
#else
    nsresult rv;
    nsCOMPtr<nsIConsoleService> consoleSvc = 
      do_GetService("@mozilla.org/consoleservice;1", &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "xsl:message couldn't get console service");
    if (consoleSvc) {
        nsAutoString logString(NS_LITERAL_STRING("xsl:message - "));
        logString.Append(aMessage.getConstNSString());
        rv = consoleSvc->LogStringMessage(logString.get());
        NS_ASSERTION(NS_SUCCEEDED(rv), "xsl:message couldn't log");
    }
#endif
}

void
txXSLTProcessor::processAction(Node* aAction,
                               ProcessorState* aPs)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aAction, "We need an action to process.");
    if (!aAction)
        return;

    unsigned short nodeType = aAction->getNodeType();

    // Handle text nodes
    if (nodeType == Node::TEXT_NODE ||
        nodeType == Node::CDATA_SECTION_NODE) {
        const String& textValue = aAction->getNodeValue();
        if (!XMLUtils::isWhitespace(textValue) ||
            XMLUtils::getXMLSpacePreserve(aAction)) {
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->characters(textValue);
        }
        return;
    }

    if (nodeType != Node::ELEMENT_NODE) {
        return;
    }

    // Handle element nodes
    Element* actionElement = (Element*)aAction;
    PRInt32 nsID = aAction->getNamespaceID();
    if (nsID != kNameSpaceID_XSLT) {
        // Literal result element
        // XXX TODO Check for excluded namespaces and aliased namespaces (element and attributes) 
        const String& nodeName = aAction->getNodeName();
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->startElement(nodeName, nsID);

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
                NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
                aPs->mResultHandler->attribute(attr->getNodeName(), attr->getNamespaceID(), value);
            }
        }

        // Process children
        processChildren(actionElement, aPs);
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->endElement(nodeName, nsID);
        return;
    }

    Expr* expr = 0;
    txAtom* localName;
    aAction->getLocalName(&localName);
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
            expr = gNodeExpr;

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
                TX_RELEASE_ATOM(localName);
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
            resultNsID = aPs->getStylesheetDocument()->namespaceURIToID(nsURI);
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

        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->attribute(name, resultNsID, value);
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
#ifdef PR_LOGGING
                String baseURI = xslTemplate->getBaseURI();
                PR_LOG(txLog::xslt, PR_LOG_DEBUG,
                       ("CallTemplate, Name %s, Stylesheet %s\n",
                        NS_LossyConvertUCS2toASCII(nameStr).get(),
                        NS_LossyConvertUCS2toASCII(baseURI).get()));
#endif
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

                ExprResult* result =
                    expr->evaluate(aPs->getEvalContext());
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
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->comment(value);
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
                resultNsID = aPs->getStylesheetDocument()->namespaceURIToID(nsURI);
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

        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->startElement(name, resultNsID);
        processAttributeSets(actionElement, aPs);
        processChildren(actionElement, aPs);
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->endElement(name, resultNsID);
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
                TX_RELEASE_ATOM(localName);
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

        if (exprResult->booleanValue()) {
            processChildren(actionElement, aPs);
        }
        delete exprResult;
    }
    // xsl:message
    else if (localName == txXSLTAtoms::message) {
        String message;
        processChildrenAsValue(actionElement, aPs, MB_FALSE, message);
        // We should add a MessageObserver class
        logMessage(message);
    }
    // xsl:number
    else if (localName == txXSLTAtoms::number) {
        String result;
        // XXX ErrorReport, propagate result from ::createNumber
        txXSLTNumber::createNumber(actionElement, aPs, result);
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->characters(result);
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
        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        aPs->mResultHandler->processingInstruction(name, value);
    }
    // xsl:sort
    else if (localName == txXSLTAtoms::sort) {
        // Ignore in this loop
    }
    // xsl:text
    else if (localName == txXSLTAtoms::text) {
        String data;
        XMLDOMUtils::getNodeValue(actionElement, data);

        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        MBool doe = MB_FALSE;
        if ((aPs->mResultHandler == aPs->mOutputHandler) &&
            aPs->mOutputHandler->hasDisableOutputEscaping()) {
            String attValue;
            doe = actionElement->getAttr(txXSLTAtoms::disableOutputEscaping,
                                         kNameSpaceID_None, attValue) &&
                  attValue.isEqual(YES_VALUE);
        }
        if (doe) {
            aPs->mOutputHandler->charactersNoOutputEscaping(data);
        }
        else {
            aPs->mResultHandler->characters(data);
        }
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

        NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
        MBool doe = MB_FALSE;
        if ((aPs->mResultHandler == aPs->mOutputHandler) &&
            aPs->mOutputHandler->hasDisableOutputEscaping()) {
            String attValue;
            doe = actionElement->getAttr(txXSLTAtoms::disableOutputEscaping,
                                         kNameSpaceID_None, attValue) &&
                  attValue.isEqual(YES_VALUE);
        }
        if (doe) {
            aPs->mOutputHandler->charactersNoOutputEscaping(value);
        }
        else {
            aPs->mResultHandler->characters(value);
        }
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

void
txXSLTProcessor::processAttributeSets(Element* aElement,
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
}

void
txXSLTProcessor::processChildren(Element* aElement,
                                 ProcessorState* aPs)
{
    NS_ASSERTION(aElement, "missing aElement");

    txVariableMap* oldVars = aPs->getLocalVariables();
    txVariableMap localVars(oldVars);
    aPs->setLocalVariables(&localVars);
    Node* child = aElement->getFirstChild();
    while (child) {
        processAction(child, aPs);
        child = child->getNextSibling();
    }
    aPs->setLocalVariables(oldVars);
}

void
txXSLTProcessor::processChildrenAsValue(Element* aElement,
                                        ProcessorState* aPs,
                                        MBool aOnlyText,
                                        String& aValue)
{
    txXMLEventHandler* previousHandler = aPs->mResultHandler;
    txTextHandler valueHandler(aValue, aOnlyText);
    aPs->mResultHandler = &valueHandler;
    processChildren(aElement, aPs);
    aPs->mResultHandler = previousHandler;
}

void
txXSLTProcessor::processDefaultTemplate(ProcessorState* aPs,
                                        const txExpandedName& aMode)
{
    Node* node = aPs->getEvalContext()->getContextNode();
    switch (node->getNodeType())
    {
        case Node::ELEMENT_NODE :
        case Node::DOCUMENT_NODE :
        {
            if (!gNodeExpr)
                break;

            ExprResult* exprResult = gNodeExpr->evaluate(aPs->getEvalContext());
            if (!exprResult ||
                exprResult->getResultType() != ExprResult::NODESET) {
                String err("None-nodeset returned while processing default template");
                aPs->receiveError(err, NS_ERROR_FAILURE);
                delete exprResult;
                return;
            }

            NodeSet* nodeSet = (NodeSet*)exprResult;
            if (nodeSet->isEmpty()) {
                delete nodeSet;
                return;
            }
            txNodeSetContext evalContext(nodeSet, aPs);
            txIEvalContext* priorEC = aPs->setEvalContext(&evalContext);

            while (evalContext.hasNext()) {
                evalContext.next();
                Node* currNode = evalContext.getContextNode();

                ProcessorState::ImportFrame *frame;
                Node* xslTemplate = aPs->findTemplate(currNode, aMode, &frame);
                processMatchedTemplate(xslTemplate, 0, aMode, frame, aPs);
            }
            aPs->setEvalContext(priorEC);
            delete exprResult;
            break;
        }
        case Node::ATTRIBUTE_NODE :
        case Node::TEXT_NODE :
        case Node::CDATA_SECTION_NODE :
        {
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->characters(node->getNodeValue());
            break;
        }
        default:
            // on all other nodetypes (including namespace nodes)
            // we do nothing
            break;
    }
}

void
txXSLTProcessor::processInclude(String& aHref,
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
            processStylesheet((Document*)stylesheet, 0,
                              aImportFrame, aPs);
            break;
        case Node::ELEMENT_NODE :
            processTopLevel((Element*)stylesheet, 0,
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

void
txXSLTProcessor::processMatchedTemplate(Node* aTemplate,
                                        txVariableMap* aParams,
                                        const txExpandedName& aMode,
                                        ProcessorState::ImportFrame* aFrame,
                                        ProcessorState* aPs)
{
    if (aTemplate) {
        ProcessorState::TemplateRule *oldTemplate, newTemplate;
        oldTemplate = aPs->getCurrentTemplateRule();
        newTemplate.mFrame = aFrame;
        newTemplate.mMode = &aMode;
        newTemplate.mParams = aParams;
        aPs->setCurrentTemplateRule(&newTemplate);

        processTemplate(aTemplate, aParams, aPs);

        aPs->setCurrentTemplateRule(oldTemplate);
    }
    else {
        processDefaultTemplate(aPs, aMode);
    }
}

nsresult
txXSLTProcessor::processParameters(Element* aAction,
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

nsresult
txXSLTProcessor::processStylesheet(Document* aStylesheet,
                                   txExpandedNameMap* aGlobalParams,
                                   ProcessorState* aPs)
{
    txListIterator importFrame(aPs->getImportFrames());
    importFrame.addAfter(new ProcessorState::ImportFrame(0));
    if (!importFrame.next()) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    processStylesheet(aStylesheet, aGlobalParams, &importFrame, aPs);
    return NS_OK;
}

void
txXSLTProcessor::processStylesheet(Document* aStylesheet,
                                   txExpandedNameMap* aGlobalParams,
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
        processTopLevel(elem, aGlobalParams, aImportFrame, aPs);
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

void
txXSLTProcessor::processTemplate(Node* aTemplate,
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

nsresult
txXSLTProcessor::processTopLevel(Element* aStylesheet,
                                 txExpandedNameMap* aGlobalParams,
                                 ProcessorState* aPs)
{
    txListIterator importFrame(aPs->getImportFrames());
    importFrame.addAfter(new ProcessorState::ImportFrame(0));
    if (!importFrame.next()) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    processTopLevel(aStylesheet, aGlobalParams, &importFrame, aPs);
    return NS_OK;
}

void
txXSLTProcessor::processTopLevel(Element* aStylesheet,
                                 txExpandedNameMap* aGlobalParams,
                                 txListIterator* importFrame,
                                 ProcessorState* aPs)
{
    // Index templates and process top level xslt elements
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
            String qName;
            element->getAttr(txXSLTAtoms::name, kNameSpaceID_None, qName);
            txExpandedName varName;
            nsresult rv = varName.init(qName, element, MB_FALSE);
            if (NS_SUCCEEDED(rv)) {
                ExprResult* defaultValue = 0;
                if (aGlobalParams) {
                    txIGlobalParameter* globalParam =
                        (txIGlobalParameter*)aGlobalParams->get(varName);
                    if (globalParam) {
                        globalParam->getValue(&defaultValue);
                    }
                }

                rv = aPs->addGlobalVariable(varName, element, currentFrame,
                                            defaultValue);
                if (NS_FAILED(rv)) {
                    String err("unable to add global xsl:param");
                    aPs->receiveError(err, NS_ERROR_FAILURE);
                }
            }
            else {
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
            String qName;
            element->getAttr(txXSLTAtoms::name, kNameSpaceID_None, qName);
            txExpandedName varName;
            nsresult rv = varName.init(qName, element, MB_FALSE);
            if (NS_SUCCEEDED(rv)) {
                rv = aPs->addGlobalVariable(varName, element, currentFrame,
                                            0);
                if (NS_FAILED(rv)) {
                    String err("unable to add global xsl:variable");
                    aPs->receiveError(err, NS_ERROR_FAILURE);
                }
            }
            else {
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

ExprResult*
txXSLTProcessor::processVariable(Element* aVariable,
                                 ProcessorState* aPs)
{
    NS_ASSERTION(aVariable, "missing xslVariable");

    //-- check for select attribute
    if (aVariable->hasAttr(txXSLTAtoms::select, kNameSpaceID_None)) {
        Expr* expr = aPs->getExpr(aVariable, ProcessorState::SelectAttr);
        if (!expr)
            return new StringResult("unable to process variable");
        return expr->evaluate(aPs->getEvalContext());
    }
    if (aVariable->hasChildNodes()) {
        txResultTreeFragment* rtf = new txResultTreeFragment();
        if (!rtf)
            // XXX ErrorReport: Out of memory
            return 0;
        txXMLEventHandler* previousHandler = aPs->mResultHandler;
        Document* rtfDocument = aPs->getRTFDocument();
        if (!rtfDocument) {
            rtfDocument = createRTFDocument(eXMLOutput);
            aPs->setRTFDocument(rtfDocument);
        }
        NS_ASSERTION(rtfDocument, "No document to create result tree fragments");
        if (!rtfDocument) {
            return 0;
        }
        txRtfHandler rtfHandler(rtfDocument, rtf);
        aPs->mResultHandler = &rtfHandler;
        processChildren(aVariable, aPs);
        NS_ASSERTION(previousHandler, "Setting mResultHandler to NULL!");
        aPs->mResultHandler = previousHandler;
        return rtf;
    }
    return new StringResult();
}

void
txXSLTProcessor::transform(ProcessorState* aPs)
{
    nsresult rv = NS_OK;
    txListIterator frameIter(aPs->getImportFrames());
    ProcessorState::ImportFrame* frame;
    txOutputFormat* outputFormat = aPs->getOutputFormat();
    while ((frame = (ProcessorState::ImportFrame*)frameIter.next())) {
        outputFormat->merge(frame->mOutputFormat);
    }

    txIOutputXMLEventHandler* handler = 0;
    rv = aPs->mOutputHandlerFactory->createHandlerWith(aPs->getOutputFormat(),
                                                       &handler);
    if (NS_FAILED(rv)) {
        return;
    }
    aPs->mOutputHandler = handler;
    aPs->mResultHandler = handler;
    aPs->mOutputHandler->startDocument();

    frame = 0;
    txExpandedName nullMode;
    Node* xslTemplate = aPs->findTemplate(aPs->getEvalContext()->getContextNode(),
                                          nullMode, &frame);
    processMatchedTemplate(xslTemplate, 0, nullMode, frame, aPs);

    aPs->mOutputHandler->endDocument();
}

void
txXSLTProcessor::xslCopy(Element* aAction, ProcessorState* aPs)
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

            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->startElement(nodeName, nsID);
            // XXX copy namespace attributes once we have them
            processAttributeSets(aAction, aPs);
            processChildren(aAction, aPs);
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->endElement(nodeName, nsID);
            break;
        }
        default:
        {
            copyNode(node, aPs);
        }
    }
}

void
txXSLTProcessor::xslCopyOf(ExprResult* aExprResult, ProcessorState* aPs)
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
            NS_ASSERTION(aPs->mResultHandler, "mResultHandler must not be NULL!");
            aPs->mResultHandler->characters(value);
        }
    }
}
