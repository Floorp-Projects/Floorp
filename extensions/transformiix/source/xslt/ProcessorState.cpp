/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * (C) 1999-2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- added code in ::resolveFunctionCall to support the
 *      document() function.
 *
 */

/**
 * Implementation of ProcessorState
 * Much of this code was ported from XSL:P
**/

#include "ProcessorState.h"
#include "XSLTFunctions.h"
#include "FunctionLib.h"
#include "URIUtils.h"
#include "XMLUtils.h"
#include "XMLDOMUtils.h"
#include "Tokenizer.h"
#include "VariableBinding.h"
#include "ExprResult.h"
#include "Names.h"
#include "XMLParser.h"
#ifndef TX_EXE
//  #include "nslog.h"
//  #define PRINTF NS_LOG_PRINTF(XPATH)
//  #define FLUSH  NS_LOG_FLUSH(XPATH)
#else
  #include "TxLog.h"
#endif

  //-------------/
 //- Constants -/
//-------------/
const String ProcessorState::wrapperNSPrefix  = "transformiix";
const String ProcessorState::wrapperName      = "transformiix:result";
const String ProcessorState::wrapperNS        = "http://www.mitre.org/TransforMiix";

/**
 * Creates a new ProcessorState
**/
ProcessorState::ProcessorState() {
    this->mSourceDocument = NULL;
    this->xslDocument = NULL;
    this->resultDocument = NULL;
    currentAction = 0;
    initialize();
} //-- ProcessorState

/**
 * Creates a new ProcessorState for the given XSL document
 * and resultDocument
**/
ProcessorState::ProcessorState(Document& sourceDocument, Document& xslDocument, Document& resultDocument) {
    this->mSourceDocument = &sourceDocument;
    this->xslDocument = &xslDocument;
    this->resultDocument = &resultDocument;
    currentAction = 0;
    initialize();
} //-- ProcessorState

/**
 * Destroys this ProcessorState
**/
ProcessorState::~ProcessorState() {

  delete resultNodeStack;

  while ( ! variableSets.empty() ) {
      delete (NamedMap*) variableSets.pop();
  }

  //-- clean up XSLT actions stack
  while (currentAction) {
      XSLTAction* item = currentAction;
      item->node = 0;
      currentAction = item->prev;
      item->prev = 0;
      delete item;
  }

  // Delete all ImportFrames
  txListIterator iter(&mImportFrames);
  while (iter.hasNext())
      delete (ImportFrame*)iter.next();

  // Make sure that xslDocument and mSourceDocument isn't deleted along with
  // the rest of the documents in the loadedDocuments hash
  if (xslDocument)
      loadedDocuments.remove(xslDocument->getBaseURI());
  if (mSourceDocument)
      loadedDocuments.remove(mSourceDocument->getBaseURI());

} //-- ~ProcessorState


/*
 * Adds the given attribute set to the list of available named attribute
 * sets
 * @param aAttributeSet the Element to add as a named attribute set
 * @param aImportFrame  ImportFrame to add the attributeset to
 */
void ProcessorState::addAttributeSet(Element* aAttributeSet,
                                     ImportFrame* aImportFrame)
{
    if (!aAttributeSet)
        return;

    const String& name = aAttributeSet->getAttribute(NAME_ATTR);
    if (name.isEmpty()) {
        String err("missing required name attribute for xsl:attribute-set");
        recieveError(err);
        return;
    }
    //-- get attribute set, if already exists, then merge
    NodeSet* attSet = (NodeSet*)aImportFrame->mNamedAttributeSets.get(name);
    if ( !attSet) {
        attSet = new NodeSet();
        aImportFrame->mNamedAttributeSets.put(name, attSet);
    }

    //-- add xsl:attribute elements to attSet
    Node* node = aAttributeSet->getFirstChild();
    while (node) {
        if ( node->getNodeType() == Node::ELEMENT_NODE) {
            String nodeName = node->getNodeName();
            String ns;
            XMLUtils::getNameSpace(nodeName, ns);
            if ( !xsltNameSpace.isEqual(ns)) continue;
            String localPart;
            XMLUtils::getLocalPart(nodeName, localPart);
            if ( ATTRIBUTE.isEqual(localPart) ) attSet->add(node);
        }
        node = node->getNextSibling();
    }

} //-- addAttributeSet

/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void ProcessorState::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

/**
 * Adds the given template to the list of templates to process
 * @param xslTemplate  The Element to add as a template
 * @param importFrame  ImportFrame to add the template to
**/
void ProcessorState::addTemplate(Element* aXslTemplate,
                                 ImportFrame* aImportFrame)
{
    NS_ASSERTION(aXslTemplate, "missing template");
    
    const String& name = aXslTemplate->getAttribute(NAME_ATTR);
    if (!name.isEmpty()) {
        // check for duplicates
        Element* tmp = (Element*)aImportFrame->mNamedTemplates.get(name);
        if (tmp) {
            String err("Duplicate template name: ");
            err.append(name);
            recieveError(err);
            return;
        }
        aImportFrame->mNamedTemplates.put(name, aXslTemplate);
    }

    const String& match = aXslTemplate->getAttribute(MATCH_ATTR);
    if (!match.isEmpty()) {
        // get the txList for the right mode
        const String& mode = aXslTemplate->getAttribute(MODE_ATTR);
        txList* templates =
            (txList*)aImportFrame->mMatchableTemplates.get(mode);

        if (!templates) {
            templates = new txList;
            if (!templates) {
                NS_ASSERTION(0, "out of memory");
                return;
            }
            aImportFrame->mMatchableTemplates.put(mode, templates);
        }

        // Add the template to the list of templates
        MatchableTemplate* templ = new MatchableTemplate;
        if (!templ) {
            NS_ASSERTION(0, "out of memory");
            return;
        }
        templ->mTemplate = aXslTemplate;
        templ->mMatch = exprParser.createPatternExpr(match);
        if (templ->mMatch)
            templates->add(templ);
        else
            delete templ;
    }
}

/*
 * Adds the given LRE Stylesheet to the list of templates to process
 * @param aStylesheet  The Stylesheet to add as a template
 * @param importFrame  ImportFrame to add the template to
 */
void ProcessorState::addLREStylesheet(Document* aStylesheet,
                                      ImportFrame* aImportFrame)
{
    NS_ASSERTION(aStylesheet, "missing stylesheet");
    
    // get the txList for null mode
    txList* templates =
        (txList*)aImportFrame->mMatchableTemplates.get(NULL_STRING);

    if (!templates) {
        templates = new txList;
        if (!templates) {
            // XXX ErrorReport: out of memory
            return;
        }
        aImportFrame->mMatchableTemplates.put(NULL_STRING, templates);
    }

    // Add the template to the list of templates
    MatchableTemplate* templ = new MatchableTemplate;
    if (!templ) {
        // XXX ErrorReport: out of memory
        return;
    }

    templ->mTemplate = aStylesheet;
    String match("/");
    templ->mMatch = exprParser.createPatternExpr(match);
    if (templ->mMatch)
        templates->add(templ);
    else
        delete templ;
}

/**
 * Adds the given node to the result tree
 * @param node the Node to add to the result tree
**/
MBool ProcessorState::addToResultTree(Node* node) {

    Node* current = resultNodeStack->peek();
#ifndef TX_EXE
    String nameSpaceURI, name, localName;
#endif

    switch (node->getNodeType()) {

        case Node::ATTRIBUTE_NODE:
        {
            if (current->getNodeType() != Node::ELEMENT_NODE) return MB_FALSE;
            Element* element = (Element*)current;
            Attr* attr = (Attr*)node;
#ifndef TX_EXE
            name = attr->getName();
            getResultNameSpaceURI(name, nameSpaceURI);
            // XXX HACK (pvdb) Workaround for BUG 51656 Html rendered as xhtml
            if (getOutputFormat()->isHTMLOutput()) {
                name.toLowerCase();
            }
            element->setAttributeNS(nameSpaceURI, name, attr->getValue());
#else
            element->setAttribute(attr->getName(),attr->getValue());
#endif
            delete node;
            break;
        }
        case Node::ELEMENT_NODE:
            //-- if current node is the document, make sure
            //-- we don't already have a document element.
            //-- if we do, create a wrapper element
            if ( current == resultDocument ) {
                Element* docElement = resultDocument->getDocumentElement();
                if ( docElement ) {
                    String nodeName(wrapperName);
                    Element* wrapper = resultDocument->createElement(nodeName);
                    resultNodeStack->push(wrapper);
                    current->appendChild(wrapper);
                    current = wrapper;
                }
#ifndef TX_EXE
                else {
                    // Checking if we should set the output method to HTML
                    name = node->getNodeName();
                    XMLUtils::getLocalPart(name, localName);
                    if (localName.isEqualIgnoreCase(HTML)) {
                        setOutputMethod(HTML);
                        // XXX HACK (pvdb) Workaround for BUG 51656 
                        // Html rendered as xhtml
                        name.toLowerCase();
                    }
                }
#endif
            }
            current->appendChild(node);
            break;
        case Node::TEXT_NODE :
            //-- if current node is the document, create wrapper element
            if ( current == resultDocument && 
                 !XMLUtils::isWhitespace(node->getNodeValue())) {
                String nodeName(wrapperName);
                Element* wrapper = resultDocument->createElement(nodeName);
                resultNodeStack->push(wrapper);
                while (current->hasChildNodes()){
                    wrapper->appendChild(current->getFirstChild());
                    current->removeChild(current->getFirstChild());
                }
                current->appendChild(wrapper);
                current = wrapper;
            }
            current->appendChild(node);
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE :
            current->appendChild(node);
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
        {
            current->appendChild(node);
            delete node; //-- DOM Implementation does not clean up DocumentFragments
            break;

        }
        //-- only add if not adding to document Node
        default:
            if (current != resultDocument) current->appendChild(node);
            else return MB_FALSE;
            break;
    }
    return MB_TRUE;

} //-- addToResultTree

/**
 * Copies the node using the rules defined in the XSL specification
**/
Node* ProcessorState::copyNode(Node* node) {
    return 0;
} //-- copyNode

/*
 * Retrieve the document designated by the URI uri, using baseUri as base URI.
 * Parses it as an XML document, and returns it. If a fragment identifier is
 * supplied, the element with seleced id is returned.
 * The returned document is owned by the ProcessorState
 *
 * @param uri the URI of the document to retrieve
 * @param baseUri the base URI used to resolve the URI if uri is relative
 * @return loaded document or element pointed to by fragment identifier. If
 *         loading or parsing fails NULL will be returned.
 */
Node* ProcessorState::retrieveDocument(const String& uri, const String& baseUri)
{
    String absUrl, frag, docUrl;
    URIUtils::resolveHref(uri, baseUri, absUrl);
    URIUtils::getFragmentIdentifier(absUrl, frag);
    URIUtils::getDocumentURI(absUrl, docUrl);

    // try to get already loaded document
    Document* xmlDoc = (Document*)loadedDocuments.get(docUrl);

    if (!xmlDoc) {
        // open URI
        String errMsg;
        XMLParser xmlParser;

        NS_ASSERTION(currentAction && currentAction->node,
                     "missing currentAction->node");

        Document* loaderDoc;
        if (currentAction->node->getNodeType() == Node::DOCUMENT_NODE)
            loaderDoc = (Document*)currentAction->node;
        else
            loaderDoc = currentAction->node->getOwnerDocument();

        xmlDoc = xmlParser.getDocumentFromURI(docUrl, loaderDoc, errMsg);

        if (!xmlDoc) {
            String err("Couldn't load document '");
            err.append(docUrl);
            err.append("': ");
            err.append(errMsg);
            recieveError(err, ErrorObserver::WARNING);
            return NULL;
        }
        // add to list of documents
        loadedDocuments.put(docUrl, xmlDoc);
    }

    // return element with supplied id if supplied
    if (!frag.isEmpty())
        return xmlDoc->getElementById(frag);

    return xmlDoc;
}

/*
 * Return stack of urls of currently entered stylesheets
 */
Stack* ProcessorState::getEnteredStylesheets()
{
    return &enteredStylesheets;
}

/*
 * Return list of import containers
 */
List* ProcessorState::getImportFrames()
{
    return &mImportFrames;
}

/*
 * Finds a template for the given Node. Only templates with
 * a mode attribute equal to the given mode will be searched.
 */
Node* ProcessorState::findTemplate(Node* aNode,
                                   Node* aContext,
                                   const String& aMode)
{
    if (!aNode)
        return 0;

    Node* matchTemplate = 0;
    double currentPriority = Double::NEGATIVE_INFINITY;
    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);

    while (!matchTemplate && (frame = (ImportFrame*)frameIter.next())) {
        // get templatelist for this mode
        txList* templates;
        templates = (txList*)frame->mMatchableTemplates.get(aMode);

        if (templates) {
            txListIterator templateIter(templates);

            MatchableTemplate* templ;
            while ((templ = (MatchableTemplate*)templateIter.next())) {
                String priorityAttr;

                if (templ->mTemplate->getNodeType() == Node::ELEMENT_NODE) {
                    Element* elem = (Element*)templ->mTemplate;
                    priorityAttr = elem->getAttribute(PRIORITY_ATTR);
                }

                double tmpPriority;
                if (!priorityAttr.isEmpty()) {
                    Double dbl(priorityAttr);
                    tmpPriority = dbl.doubleValue();
                }
                else {
                    tmpPriority = templ->mMatch->getDefaultPriority(aNode,
                                                                    aContext,
                                                                    this);
                }

                if (tmpPriority >= currentPriority &&
                    templ->mMatch->matches(aNode, aContext, this)) {

                    matchTemplate = templ->mTemplate;
                    currentPriority = tmpPriority;
                }
            }
        }
    }

    return matchTemplate;
} //-- findTemplate

/**
 * Returns the AttributeSet associated with the given name
 * or null if no AttributeSet is found
 */
NodeSet* ProcessorState::getAttributeSet(const String& aName)
{
    NodeSet* attset = new NodeSet;
    if (!attset)
        return attset;

    attset->setDuplicateChecking(MB_FALSE);

    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);
    frameIter.resetToEnd();

    while ((frame = (ImportFrame*)frameIter.previous())) {
        NodeSet* nodes = (NodeSet*)frame->mNamedAttributeSets.get(aName);
        if (nodes)
            nodes->copyInto(*attset);
    }
    return attset;
}

/**
 * Returns the source node currently being processed
**/
Node* ProcessorState::getCurrentNode() {
    return currentNodeStack.peek();
} //-- setCurrentNode

/**
 * Gets the default Namespace URI stack.
**/ 
Stack* ProcessorState::getDefaultNSURIStack() {
    return &defaultNameSpaceURIStack;
} //-- getDefaultNSURIStack

Expr* ProcessorState::getExpr(const String& pattern) {
//    NS_IMPL_LOG(XPATH)
//    PRINTF("Resolving XPath Expr %s",pattern.toCharArray());
//    FLUSH();
    Expr* expr = (Expr*)exprHash.get(pattern);
    if ( !expr ) {
        expr = exprParser.createExpr(pattern);
        if ( !expr ) {
            String err = "Error in parsing XPath expression: ";
            err.append(pattern);
            expr = new ErrorFunctionCall(err);
        }
        exprHash.put(pattern, expr);
    }
    return expr;
} //-- getExpr

/*
 * Returns the template associated with the given name, or
 * null if not template is found
 */
Element* ProcessorState::getNamedTemplate(String& aName)
{
    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);

    while ((frame = (ImportFrame*)frameIter.next())) {
        Element* templ = (Element*)frame->mNamedTemplates.get(aName);
        if (templ)
            return templ;
    }
    return 0;
}

/**
 * Returns the namespace URI for the given name, this method should only be
 * called for determining a namespace declared within the context (ie. the stylesheet)
**/
void ProcessorState::getNameSpaceURI(const String& name, String& nameSpaceURI) {
    String prefix;
    XMLUtils::getNameSpace(name, prefix);
    getNameSpaceURIFromPrefix(prefix, nameSpaceURI);

} //-- getNameSpaceURI

/**
 * Returns the namespace URI for the given namespace prefix, this method should
 * only be called for determining a namespace declared within the context
 * (ie. the stylesheet)
**/
void ProcessorState::getNameSpaceURIFromPrefix(const String& prefix, String& nameSpaceURI) {

    XSLTAction* action = currentAction;

    while (action) {
        Node* node = action->node;
        if (( node ) && (node->getNodeType() == Node::ELEMENT_NODE)) {
            if (XMLDOMUtils::getNameSpace(prefix, (Element*) node, nameSpaceURI))
                break;
        }
        action = action->prev;
    }

} //-- getNameSpaceURI

/**
 * Returns the NodeStack which keeps track of where we are in the
 * result tree
 * @return the NodeStack which keeps track of where we are in the
 * result tree
**/
NodeStack* ProcessorState::getNodeStack() {
    return resultNodeStack;
} //-- getNodeStack

/**
 * Returns the OutputFormat which contains information on how
 * to serialize the output. I will be removing this soon, when
 * change to an event based printer, so that I can serialize
 * as I go
**/
OutputFormat* ProcessorState::getOutputFormat() {
    return &format;
} //-- getOutputFormat

PatternExpr* ProcessorState::getPatternExpr(const String& pattern) {
    PatternExpr* pExpr = (PatternExpr*)patternExprHash.get(pattern);
    if ( !pExpr ) {
        pExpr = exprParser.createPatternExpr(pattern);
        patternExprHash.put(pattern, pExpr);
    }
    return pExpr;
} //-- getPatternExpr

Document* ProcessorState::getResultDocument() {
    return resultDocument;
} //-- getResultDocument

/**
 * Returns the namespace URI for the given name, this method should only be
 * called for returning a namespace declared within in the result document.
**/
void ProcessorState::getResultNameSpaceURI(const String& name, String& nameSpaceURI) {
    String prefix;
    XMLUtils::getNameSpace(name, prefix);
    if (prefix.isEmpty()) {
        nameSpaceURI.clear();
        nameSpaceURI.append(*(String*)defaultNameSpaceURIStack.peek());
    }
    else {
        String* result = (String*)nameSpaceMap.get(prefix);
        if (result) {
            nameSpaceURI.clear();
            nameSpaceURI.append(*result);
        }
    }

} //-- getResultNameSpaceURI

Stack* ProcessorState::getVariableSetStack() {
    return &variableSets;
} //-- getVariableSetStack

String& ProcessorState::getXSLNamespace() {
    return xsltNameSpace;
} //-- getXSLNamespace

/**
 * Determines if the given XSL node allows Whitespace stripping
**/
MBool ProcessorState::isXSLStripSpaceAllowed(Node* node) {

    if ( !node ) return MB_FALSE;
    return (MBool)(PRESERVE != getXMLSpaceMode(node));

} //--isXSLStripSpaceAllowed

/**
 * Returns the current XSLT action from the top of the stack.
 * @returns the XSLT action from the top of the stack
**/
Node* ProcessorState::peekAction() {
    NS_ASSERTION(currentAction, "currentAction is NULL, this is very bad");
    if (currentAction)
        return currentAction->node;
    return NULL;
}

/**
 * Removes the current XSLT action from the top of the stack.
 * @returns the XSLT action after removing from the top of the stack
**/
Node* ProcessorState::popAction() {
    Node* xsltAction = 0;
    if (currentAction) {
        xsltAction = currentAction->node;
        XSLTAction* item = currentAction;
        currentAction = currentAction->prev;
        item->node = 0;
        delete item;
    }
    return xsltAction;
} //-- popAction

/**
 * Removes and returns the current source node being processed, from the stack
 * @return the current source node
**/
Node* ProcessorState::popCurrentNode() {
   return currentNodeStack.pop();
} //-- popCurrentNode

void ProcessorState::processAttrValueTemplate(const String& aAttValue,
                                              Node* aContext,
                                              String& aResult)
{
    aResult.clear();
    AttributeValueTemplate* avt =
                    exprParser.createAttributeValueTemplate(aAttValue);
    if (!avt) {
        // XXX ErrorReport: out of memory
        return;
    }

    ExprResult* exprResult = avt->evaluate(aContext, this);
    delete avt;
    if (!exprResult) {
        // XXX ErrorReport: out of memory
        return;
    }

    exprResult->stringValue(aResult);
    delete exprResult;
}

/**
 * Adds the given XSLT action to the top of the action stack
**/
void ProcessorState::pushAction(Node* xsltAction) {
   if (currentAction) {
       XSLTAction* newAction = new XSLTAction;
       newAction->prev = currentAction;
       currentAction = newAction;
   }
   else {
       currentAction = new XSLTAction;
       currentAction->prev = 0;
   }
   currentAction->node = xsltAction;
} //-- pushAction

/**
 * Sets the source node currently being processed
 * @param node the source node to set as the "current" node
**/
void ProcessorState::pushCurrentNode(Node* node) {
    currentNodeStack.push(node);
} //-- setCurrentNode

/**
 * Sets a new default Namespace URI.
**/ 
void ProcessorState::setDefaultNameSpaceURIForResult(const String& nsURI) {
    String* nsTempURIPointer = 0;
    String* nsURIPointer = 0;
    StringListIterator theIterator(&nameSpaceURIList);

    while (theIterator.hasNext()) {
        nsTempURIPointer = theIterator.next();
        if (nsTempURIPointer->isEqual(nsURI)) {
            nsURIPointer = nsTempURIPointer;
            break;
        }
    }
    if ( ! nsURIPointer ) {
        nsURIPointer = new String(nsURI);
        nameSpaceURIList.add(nsURIPointer);
    }
    defaultNameSpaceURIStack.push(nsURIPointer);
} //-- setDefaultNameSpaceURI

/**
 * Sets the output method. Valid output method options are,
 * "xml", "html", or "text".
**/ 
void ProcessorState::setOutputMethod(const String& method) {
    format.setMethod(method);
    if ( method.indexOf(HTML) == 0 ) {
        setDefaultNameSpaceURIForResult(HTML_NS);
    }
}

/*
 * Adds the set of names to the Whitespace handling list.
 * xsl:strip-space calls this with MB_TRUE, xsl:preserve-space 
 * with MB_FALSE
 */
void ProcessorState::shouldStripSpace(String& aNames,
                                      MBool aShouldStrip,
                                      ImportFrame* aImportFrame)
{
    //-- split names on whitespace
    Tokenizer tokenizer(aNames);
    String name;
    while (tokenizer.hasMoreTokens()) {
        tokenizer.nextToken(name);
        txNameTestItem* nti = new txNameTestItem(name, aShouldStrip);
        if (!nti) {
            // XXX error report, parsing error or out of mem
            break;
        }
        double priority = nti->getDefaultPriority();
        txListIterator iter(&aImportFrame->mWhiteNameTests);
        while (iter.hasNext()) {
            txNameTestItem* iNameTest = (txNameTestItem*)iter.next();
            if (iNameTest->getDefaultPriority() <= priority) {
                break;
            }
        }
        iter.addBefore(nti);
    }

} //-- stripSpace

/**
 * Adds the supplied xsl:key to the set of keys
**/
MBool ProcessorState::addKey(Element* keyElem) {
    String keyName = keyElem->getAttribute(NAME_ATTR);
    if(!XMLUtils::isValidQName(keyName))
        return MB_FALSE;
    txXSLKey* xslKey = (txXSLKey*)xslKeys.get(keyName);
    if (!xslKey) {
        xslKey = new txXSLKey(this);
        if (!xslKey)
            return MB_FALSE;
        xslKeys.put(keyName, xslKey);
    }

    return xslKey->addKey(keyElem->getAttribute(MATCH_ATTR), keyElem->getAttribute(USE_ATTR));
}

/**
 * Adds the supplied xsl:key to the set of keys
 * returns NULL if no such key exists
**/
txXSLKey* ProcessorState::getKey(String& keyName) {
    return (txXSLKey*)xslKeys.get(keyName);
}

/*
 * Adds a decimal format. Returns false if the format already exists
 * but dosn't contain the exact same parametervalues
 */
MBool ProcessorState::addDecimalFormat(Element* element)
{
    // build new DecimalFormat structure
    MBool success = MB_TRUE;
    txDecimalFormat* format = new txDecimalFormat;
    if (!format)
        return MB_FALSE;

    String attValue = element->getAttribute(NAME_ATTR);
    String formatName = attValue;

    attValue = element->getAttribute(DECIMAL_SEPARATOR_ATTR);
    if (attValue.length() == 1)
        format->mDecimalSeparator = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(GROUPING_SEPARATOR_ATTR);
    if (attValue.length() == 1)
        format->mGroupingSeparator = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(INFINITY_ATTR);
    if (!attValue.isEmpty())
        format->mInfinity=attValue;

    attValue = element->getAttribute(MINUS_SIGN_ATTR);
    if (attValue.length() == 1)
        format->mMinusSign = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(NAN_ATTR);
    if (!attValue.isEmpty())
        format->mNaN=attValue;
        
    attValue = element->getAttribute(PERCENT_ATTR);
    if (attValue.length() == 1)
        format->mPercent = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(PER_MILLE_ATTR);
    if (attValue.length() == 1)
        format->mPerMille = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(ZERO_DIGIT_ATTR);
    if (attValue.length() == 1)
        format->mZeroDigit = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(DIGIT_ATTR);
    if (attValue.length() == 1)
        format->mDigit = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    attValue = element->getAttribute(PATTERN_SEPARATOR_ATTR);
    if (attValue.length() == 1)
        format->mPatternSeparator = attValue.charAt(0);
    else if (!attValue.isEmpty())
        success = MB_FALSE;

    if (!success) {
        delete format;
        return MB_FALSE;
    }

    // Does an existing format with that name exist?
    // (name="" means default format)
    
    txDecimalFormat* existing = NULL;

    if (defaultDecimalFormatSet || !formatName.isEmpty()) {
        existing = (txDecimalFormat*)decimalFormats.get(formatName);
    }
    else {
        // We are overriding the predefined default format which is always
        // allowed
        delete decimalFormats.remove(formatName);
        defaultDecimalFormatSet = MB_TRUE;
    }

    if (existing) {
        success = existing->isEqual(format);
        delete format;
    }
    else {
        decimalFormats.put(formatName, format);
    }
    
    return success;
}

/*
 * Returns a decimal format or NULL if no such format exists.
 */
txDecimalFormat* ProcessorState::getDecimalFormat(String& name)
{
    return (txDecimalFormat*)decimalFormats.get(name);
}

  //--------------------------------------------------/
 //- Virtual Methods from derived from ContextState -/
//--------------------------------------------------/


/**
 * Returns the Stack of context NodeSets
 * @return the Stack of context NodeSets
**/
Stack* ProcessorState::getNodeSetStack() {
    return &nodeSetStack;
} //-- getNodeSetStack

/**
 * Returns the value of a given variable binding within the current scope
 * @param the name to which the desired variable value has been bound
 * @return the ExprResult which has been bound to the variable with the given
 * name
**/
ExprResult* ProcessorState::getVariable(String& name) {

    StackIterator* iter = variableSets.iterator();
    ExprResult* exprResult = 0;
    while ( iter->hasNext() ) {
        NamedMap* map = (NamedMap*) iter->next();
        if ( map->get(name)) {
            exprResult = ((VariableBinding*)map->get(name))->getValue();
            break;
        }
    }
    delete iter;
    return exprResult;
} //-- getVariable

/**
 * Determines if the given XML node allows Whitespace stripping
**/
MBool ProcessorState::isStripSpaceAllowed(Node* node) {

    if ( !node ) return MB_FALSE;

    switch ( node->getNodeType() ) {

        case Node::ELEMENT_NODE :
        {
            // check Whitespace stipping handling list against given Node
            ImportFrame* frame;
            txListIterator frameIter(&mImportFrames);

            String name = node->getNodeName();
            while ((frame = (ImportFrame*)frameIter.next())) {
                txListIterator iter(&frame->mWhiteNameTests);
                while (iter.hasNext()) {
                    txNameTestItem* iNameTest = (txNameTestItem*)iter.next();
                    if (iNameTest->matches(node, this))
                        return iNameTest->stripsSpace();
                }
            }
            String method;
            if (format.getMethod(method).isEqual("html")) {
                String ucName = name;
                ucName.toUpperCase();
                if (ucName.isEqual("SCRIPT")) return MB_FALSE;
            }
            break;
        }
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
            if (!XMLUtils::shouldStripTextnode(node->getNodeValue()))
                return MB_FALSE;
            return isStripSpaceAllowed(node->getParentNode());
        case Node::DOCUMENT_NODE:
            return MB_TRUE;
        default:
            break;
    }
    XMLSpaceMode mode = getXMLSpaceMode(node);
    if (mode == DEFAULT) return (MBool)(defaultSpace == STRIP);
    return (MBool)(STRIP == mode);

} //--isStripSpaceAllowed


/**
 *  Notifies this Error observer of a new error, with default
 *  level of NORMAL
**/
void ProcessorState::recieveError(String& errorMessage) {
    recieveError(errorMessage, ErrorObserver::NORMAL);
} //-- recieveError

/**
 *  Notifies this Error observer of a new error using the given error level
**/
void ProcessorState::recieveError(String& errorMessage, ErrorLevel level) {
    ListIterator* iter = errorObservers.iterator();
    while ( iter->hasNext()) {
        ErrorObserver* observer = (ErrorObserver*)iter->next();
        observer->recieveError(errorMessage, level);
    }
    delete iter;
} //-- recieveError

/**
 * Returns a call to the function that has the given name.
 * This method is used for XPath Extension Functions.
 * @return the FunctionCall for the function with the given name.
**/
FunctionCall* ProcessorState::resolveFunctionCall(const String& name) {
   String err;

   if (DOCUMENT_FN.isEqual(name)) {
       return new DocumentFunctionCall(this);
   }
   else if (KEY_FN.isEqual(name)) {
       return new txKeyFunctionCall(this);
   }
   else if (FORMAT_NUMBER_FN.isEqual(name)) {
       return new txFormatNumberFunctionCall(this);
   }
   else if (CURRENT_FN.isEqual(name)) {
       return new CurrentFunctionCall(this);
   }
   else if (UNPARSED_ENTITY_URI_FN.isEqual(name)) {
       err = "function not yet implemented: ";
       err.append(name);
   }
   else if (GENERATE_ID_FN.isEqual(name)) {
       return new GenerateIdFunctionCall();
   }
   else if (SYSTEM_PROPERTY_FN.isEqual(name)) {
       return new SystemPropertyFunctionCall();
   }
   else if (ELEMENT_AVAILABLE_FN.isEqual(name)) {
       return new ElementAvailableFunctionCall();
   }
   else if (FUNCTION_AVAILABLE_FN.isEqual(name)) {
       return new FunctionAvailableFunctionCall();
   }
   else {
       err = "invalid function call: ";
       err.append(name);
   }

   return new ErrorFunctionCall(err);

} //-- resolveFunctionCall


/**
 * Sorts the given NodeSet by DocumentOrder.
 * @param nodes the NodeSet to sort
 *
 * Note: I will be moving this functionality elsewhere soon
**/
void ProcessorState::sortByDocumentOrder(NodeSet* nodes) {
    if (!nodes || (nodes->size() < 2))
        return;

    NodeSet sorted(nodes->size());
    sorted.setDuplicateChecking(MB_FALSE);
    sorted.add(nodes->get(0));

    int i, k;
    for (i = 1; i < nodes->size(); i++) {
        Node* node = nodes->get(i);
        for (k = i - 1; k >= 0; k--) {
            Node* tmpNode = sorted.get(k);
            if (node->compareDocumentPosition(tmpNode) > 0) {
                sorted.add(k + 1, node);
                break;
            }
            else if (k == 0) {
                sorted.add(0, node);
                break;
            }
        }
    }

    //-- save current state of duplicates checking
    MBool checkDuplicates = nodes->getDuplicateChecking();
    nodes->setDuplicateChecking(MB_FALSE);
    nodes->clear();
    for (i = 0; i < sorted.size(); i++) {
        nodes->add(sorted.get(i));
    }
    nodes->setDuplicateChecking(checkDuplicates);
    sorted.clear();

} //-- sortByDocumentOrder

  //-------------------/
 //- Private Methods -/
//-------------------/

/**
 * Returns the closest xml:space value for the given Text node
**/
ProcessorState::XMLSpaceMode ProcessorState::getXMLSpaceMode(Node* node) {

    if (!node) return DEFAULT; //-- we should never see this

    Node* parent = node;
    while ( parent ) {
        switch ( parent->getNodeType() ) {
            case Node::ELEMENT_NODE:
            {
                String value = ((Element*)parent)->getAttribute(XML_SPACE);
                if ( value.isEqual(PRESERVE_VALUE)) {
                    return PRESERVE;
                }
                break;
            }
            case Node::TEXT_NODE:
            case Node::CDATA_SECTION_NODE:
                //-- we will only see this the first time through the loop
                //-- if the argument node is a text node
                break;
            default:
                return DEFAULT;
        }
        parent = parent->getParentNode();
    }
    return DEFAULT;

} //-- getXMLSpaceMode

/**
 * Initializes this ProcessorState
**/
void ProcessorState::initialize() {
    //-- initialize default-space
    defaultSpace = PRESERVE;

    //-- add global variable set
    NamedMap* globalVars = new NamedMap();
    globalVars->setObjectDeletion(MB_TRUE);
    variableSets.push(globalVars);

    /* turn object deletion on for some of the Maps (NamedMap) */
    exprHash.setObjectDeletion(MB_TRUE);
    patternExprHash.setObjectDeletion(MB_TRUE);
    nameSpaceMap.setObjectDeletion(MB_TRUE);

    //-- create NodeStack
    resultNodeStack = new NodeStack();
    resultNodeStack->push(this->resultDocument);

    setDefaultNameSpaceURIForResult("");

    //-- determine xsl properties
    Element* element = NULL;
    if (mSourceDocument) {
        loadedDocuments.put(mSourceDocument->getBaseURI(), mSourceDocument);
    }
    if (xslDocument) {
        element = xslDocument->getDocumentElement();
        loadedDocuments.put(xslDocument->getBaseURI(), xslDocument);
    }
    if ( element ) {

        pushAction(element);

	    //-- process namespace nodes
	    NamedNodeMap* atts = element->getAttributes();
	    if ( atts ) {
	        for (PRUint32 i = 0; i < atts->getLength(); i++) {
	            Attr* attr = (Attr*)atts->item(i);
	            String attName = attr->getName();
	            String attValue = attr->getValue();
	            if ( attName.indexOf(XMLUtils::XMLNS) == 0) {
	                String ns;
	                XMLUtils::getLocalPart(attName, ns);
	                // default namespace
	                if ( attName.isEqual(XMLUtils::XMLNS) ) {
	                    //-- Is this correct?
	                    setDefaultNameSpaceURIForResult(attValue);
	                }
	                // namespace declaration
	                else {
	                    String ns;
	                    XMLUtils::getLocalPart(attName, ns);
	                    nameSpaceMap.put(ns, new String(attValue));
	                }
	                // check for XSL namespace
	                if ( attValue.indexOf(XSLT_NS) == 0) {
	                    xsltNameSpace = ns;
	                }
	            }
	            else if ( attName.isEqual(DEFAULT_SPACE_ATTR) ) {
	                if ( attValue.isEqual(STRIP_VALUE) ) {
	                    defaultSpace = STRIP;
	                }
	            }
	            else if ( attName.isEqual(RESULT_NS_ATTR) ) {
	                if (!attValue.isEmpty()) {
	                    if ( attValue.indexOf(HTML_NS) == 0 ) {
	                        setOutputMethod("html");
	                    }
	                    else setOutputMethod(attValue);
	                }
	            }
	            else if ( attName.isEqual(INDENT_RESULT_ATTR) ) {
	                if (!attValue.isEmpty()) {
	                    format.setIndent(attValue.isEqual(YES_VALUE));
	                }
	            }

	        } //-- end for each att
	    } //-- end if atts are not null
	}
    
    //-- make sure all keys are deleted
    xslKeys.setObjectDeletion(MB_TRUE);
    
    //-- Make sure all loaded documents get deleted
    loadedDocuments.setObjectDeletion(MB_TRUE);

    //-- add predefined default decimal format
    defaultDecimalFormatSet = MB_FALSE;
    decimalFormats.put("", new txDecimalFormat);
    decimalFormats.setObjectDeletion(MB_TRUE);
}

ProcessorState::ImportFrame::ImportFrame()
{
    mNamedAttributeSets.setObjectDeletion(MB_TRUE);
}

ProcessorState::ImportFrame::~ImportFrame()
{
    // Delete all txNameTestItems
    txListIterator whiteIter(&mWhiteNameTests);
    while (whiteIter.hasNext())
        delete (txNameTestItem*)whiteIter.next();

    // Delete templates in mMatchableTemplates
    StringList* templKeys = mMatchableTemplates.keys();
    if (templKeys) {
        StringListIterator keysIter(templKeys);
        String* key;
        while ((key = keysIter.next())) {
            txList* templList = (txList*)mMatchableTemplates.get(*key);
            txListIterator templIter(templList);
            MatchableTemplate* templ;
            while ((templ = (MatchableTemplate*)templIter.next())) {
                delete templ->mMatch;
                delete templ;
            }
            delete templList;
        }
    }
    delete templKeys;
}
