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
#include "txAtoms.h"

/**
 * Creates a new ProcessorState
**/
ProcessorState::ProcessorState() : mXPathParseContext(0),
                                   mSourceDocument(0),
                                   xslDocument(0),
                                   resultDocument(0)
{
    initialize();
} //-- ProcessorState

/**
 * Creates a new ProcessorState for the given XSL document
 * and resultDocument
**/
ProcessorState::ProcessorState(Document* aSourceDocument,
                               Document* aXslDocument,
                               Document* aResultDocument)
    : mXPathParseContext(0),
      mSourceDocument(aSourceDocument),
      xslDocument(aXslDocument),
      resultDocument(aResultDocument)
{
    initialize();
} //-- ProcessorState

/**
 * Destroys this ProcessorState
**/
ProcessorState::~ProcessorState()
{
  while (! variableSets.empty()) {
      delete (NamedMap*) variableSets.pop();
  }

  // Delete all ImportFrames
  txListIterator iter(&mImportFrames);
  while (iter.hasNext())
      delete (ImportFrame*)iter.next();

  // Make sure that xslDocument and mSourceDocument aren't deleted along with
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

    String name;
    if (!aAttributeSet->getAttr(txXSLTAtoms::name,
                                kNameSpaceID_None, name)) {
        String err("missing required name attribute for xsl:attribute-set");
        recieveError(err);
        return;
    }
    // Get attribute set, if already exists, then merge
    NodeSet* attSet = (NodeSet*)aImportFrame->mNamedAttributeSets.get(name);
    if (!attSet) {
        attSet = new NodeSet();
        aImportFrame->mNamedAttributeSets.put(name, attSet);
    }

    // Add xsl:attribute elements to attSet
    Node* node = aAttributeSet->getFirstChild();
    while (node) {
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            PRInt32 nsID = node->getNamespaceID();
            if (nsID != kNameSpaceID_XSLT)
                continue;
            txAtom* nodeName;
            if (!node->getLocalName(&nodeName) || !nodeName)
                continue;
            if (nodeName == txXSLTAtoms::attribute)
                attSet->append(node);
            TX_RELEASE_ATOM(nodeName);
        }
        node = node->getNextSibling();
    }

}
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
    
    String name;
    if (aXslTemplate->getAttr(txXSLTAtoms::name,
                              kNameSpaceID_None, name)) {
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

    String match;
    if (aXslTemplate->getAttr(txXSLTAtoms::match,
                              kNameSpaceID_None, match)) {
        // get the txList for the right mode
        String mode;
        aXslTemplate->getAttr(txXSLTAtoms::mode, kNameSpaceID_None, mode);
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
        Element* oldContext = mXPathParseContext;
        mXPathParseContext = aXslTemplate;
        templ->mMatch = exprParser.createPattern(match);
        mXPathParseContext = oldContext;
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
    templ->mMatch = exprParser.createPattern(match);
    if (templ->mMatch)
        templates->add(templ);
    else
        delete templ;
}

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

        xmlDoc = xmlParser.getDocumentFromURI(docUrl, xslDocument, errMsg);

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
 * Find template in specified mode matching the supplied node
 * @param aNode        node to find matching template for
 * @param aMode        mode of the template
 * @param aImportFrame out-param, is set to the ImportFrame containing
 *                     the found template
 * @return             root-node of found template, null if none is found
 */
Node* ProcessorState::findTemplate(Node* aNode,
                                   const String& aMode,
                                   ImportFrame** aImportFrame)
{
    return findTemplate(aNode, aMode, 0, aImportFrame);
}

/*
 * Find template in specified mode matching the supplied node. Only search
 * templates imported by a specific ImportFrame
 * @param aNode        node to find matching template for
 * @param aMode        mode of the template
 * @param aImportedBy  seach only templates imported by this ImportFrame,
 *                     or null to search all templates
 * @param aImportFrame out-param, is set to the ImportFrame containing
 *                     the found template
 * @return             root-node of found template, null if none is found
 */
Node* ProcessorState::findTemplate(Node* aNode,
                                   const String& aMode,
                                   ImportFrame* aImportedBy,
                                   ImportFrame** aImportFrame)
{
    NS_ASSERTION(aImportFrame, "missing ImportFrame pointer");
    NS_ASSERTION(aNode, "missing node");

    if (!aNode)
        return 0;

    Node* matchTemplate = 0;
    double currentPriority = Double::NEGATIVE_INFINITY;
    ImportFrame* endFrame = 0;
    txListIterator frameIter(&mImportFrames);

    if (aImportedBy) {
        ImportFrame* curr = (ImportFrame*)frameIter.next();
        while (curr != aImportedBy)
               curr = (ImportFrame*)frameIter.next();

        endFrame = aImportedBy->mFirstNotImported;
    }

    ImportFrame* frame;
    while (!matchTemplate &&
           (frame = (ImportFrame*)frameIter.next()) &&
           frame != endFrame) {

        // get templatelist for this mode
        txList* templates;
        templates = (txList*)frame->mMatchableTemplates.get(aMode);

        if (templates) {
            txListIterator templateIter(templates);

            // Find template with highest priority
            MatchableTemplate* templ;
            while ((templ = (MatchableTemplate*)templateIter.next())) {
                String priorityAttr;

                if (templ->mTemplate->getNodeType() == Node::ELEMENT_NODE) {
                    Element* elem = (Element*)templ->mTemplate;
                    elem->getAttr(txXSLTAtoms::priority, kNameSpaceID_None,
                                  priorityAttr);
                }

                double tmpPriority;
                if (!priorityAttr.isEmpty()) {
                    tmpPriority = Double::toDouble(priorityAttr);
                }
                else {
                    tmpPriority = templ->mMatch->getDefaultPriority(aNode,
                                                                    0,
                                                                    this);
                }

                if (tmpPriority >= currentPriority &&
                    templ->mMatch->matches(aNode, 0, this)) {

                    matchTemplate = templ->mTemplate;
                    *aImportFrame = frame;
                    currentPriority = tmpPriority;
                }
            }
        }
    }

    return matchTemplate;
}

/*
 * Gets current template rule
 */
ProcessorState::TemplateRule* ProcessorState::getCurrentTemplateRule()
{
    return mCurrentTemplateRule;
}

/*
 * Sets current template rule
 */
void ProcessorState::setCurrentTemplateRule(TemplateRule* aTemplateRule)
{
    mCurrentTemplateRule = aTemplateRule;
}

/**
 * Returns the AttributeSet associated with the given name
 * or null if no AttributeSet is found
 */
NodeSet* ProcessorState::getAttributeSet(const String& aName)
{
    NodeSet* attset = new NodeSet;
    if (!attset)
        return attset;

    ImportFrame* frame;
    txListIterator frameIter(&mImportFrames);
    frameIter.resetToEnd();

    while ((frame = (ImportFrame*)frameIter.previous())) {
        NodeSet* nodes = (NodeSet*)frame->mNamedAttributeSets.get(aName);
        if (nodes)
            attset->append(nodes);
    }
    return attset;
}

/**
 * Returns the source node currently being processed
**/
Node* ProcessorState::getCurrentNode() {
    return currentNodeStack.peek();
} //-- setCurrentNode

Expr* ProcessorState::getExpr(Element* aElem, ExprAttr aAttr)
{
    NS_ASSERTION(aElem, "missing element while getting expression");

    // This is how we'll have to do it for now
    mXPathParseContext = aElem;

    Expr* expr = (Expr*)mExprHashes[aAttr].get(aElem);
    if (!expr) {
        String attr;
        switch (aAttr) {
            case SelectAttr:
                aElem->getAttr(txXSLTAtoms::select, kNameSpaceID_None,
                               attr);
                break;
            case TestAttr:
                aElem->getAttr(txXSLTAtoms::test, kNameSpaceID_None,
                               attr);
                break;
            case ValueAttr:
                aElem->getAttr(txXSLTAtoms::value, kNameSpaceID_None,
                               attr);
                break;
        }

        // This is how we should do it once we namespaceresolve during parsing
        //Element* oldContext = mXPathParseContext;
        //mXPathParseContext = aElem;
        expr = exprParser.createExpr(attr);
        //mXPathParseContext = oldContext;

        if (!expr) {
            String err = "Error in parsing XPath expression: ";
            err.append(attr);
            recieveError(err);
        }
        else {
            mExprHashes[aAttr].put(aElem, expr);
        }
    }
    return expr;
}

PatternExpr* ProcessorState::getPattern(Element* aElem, PatternAttr aAttr)
{
    NS_ASSERTION(aElem, "missing element while getting pattern");

    // This is how we'll have to do it for now
    mXPathParseContext = aElem;

    Pattern* pattern = (Pattern*)mExprHashes[aAttr].get(aElem);
    if (!pattern) {
        String attr;
        switch (aAttr) {
            case CountAttr:
                aElem->getAttr(txXSLTAtoms::count, kNameSpaceID_None,
                               attr);
                break;
            case FromAttr:
                aElem->getAttr(txXSLTAtoms::from, kNameSpaceID_None,
                               attr);
                break;
        }

        // This is how we should do it once we namespaceresolve during parsing
        //Element* oldContext = mXPathParseContext;
        //mXPathParseContext = aElem;
        pattern = exprParser.createPattern(attr);
        //mXPathParseContext = oldContext;

        if (!pattern) {
            String err = "Error in parsing pattern: ";
            err.append(attr);
            recieveError(err);
        }
        else {
            mPatternHashes[aAttr].put(aElem, pattern);
        }
    }
    return pattern;
}

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

void ProcessorState::getNameSpaceURIFromPrefix(const String& aPrefix,
                                               String& aNamespaceURI)
{
    if (mXPathParseContext)
        XMLDOMUtils::getNamespaceURI(aPrefix, mXPathParseContext,
                                     aNamespaceURI);
}

txOutputFormat* ProcessorState::getOutputFormat()
{
    return &mOutputFormat;
}

Document* ProcessorState::getResultDocument()
{
    return resultDocument;
}

Stack* ProcessorState::getVariableSetStack()
{
    return &variableSets;
}

/*
 * Determines if the given XSL node allows Whitespace stripping
 */
MBool ProcessorState::isXSLStripSpaceAllowed(Node* node) {

    if (!node)
        return MB_FALSE;
    return (MBool)(getXMLSpaceMode(node) != PRESERVE);

}

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
 * Sets the source node currently being processed
 * @param node the source node to set as the "current" node
**/
void ProcessorState::pushCurrentNode(Node* node) {
    currentNodeStack.push(node);
} //-- setCurrentNode

/**
 * Adds the set of names to the Whitespace handling list.
 * xsl:strip-space calls this with MB_TRUE, xsl:preserve-space 
 * with MB_FALSE
 */
void ProcessorState::shouldStripSpace(String& aNames,
                                      MBool aShouldStrip,
                                      ImportFrame* aImportFrame)
{
    //-- split names on whitespace
    txTokenizer tokenizer(aNames);
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
MBool ProcessorState::addKey(Element* aKeyElem)
{
    String keyName;
    aKeyElem->getAttr(txXSLTAtoms::name, kNameSpaceID_None, keyName);
    if (!XMLUtils::isValidQName(keyName))
        return MB_FALSE;
    txXSLKey* xslKey = (txXSLKey*)xslKeys.get(keyName);
    if (!xslKey) {
        xslKey = new txXSLKey(this);
        if (!xslKey)
            return MB_FALSE;
        xslKeys.put(keyName, xslKey);
    }
    Element* oldContext = mXPathParseContext;
    mXPathParseContext = aKeyElem;
    Pattern* match;
    String matchAttr, useAttr;
    aKeyElem->getAttr(txXSLTAtoms::match, kNameSpaceID_None, matchAttr);
    aKeyElem->getAttr(txXSLTAtoms::use, kNameSpaceID_None, useAttr);
    match = exprParser.createPattern(matchAttr);
    Expr* use = exprParser.createExpr(useAttr);
    mXPathParseContext = oldContext;
    if (!match || !use || !xslKey->addKey(match, use)) {
        delete match;
        delete use;
        return MB_FALSE;
    }
    return MB_TRUE;
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

    String formatName, attValue;
    element->getAttr(txXSLTAtoms::name, kNameSpaceID_None, formatName);

    if (element->getAttr(txXSLTAtoms::decimalSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mDecimalSeparator = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::groupingSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mGroupingSeparator = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::infinity,
                         kNameSpaceID_None, attValue))
        format->mInfinity=attValue;

    if (element->getAttr(txXSLTAtoms::minusSign,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mMinusSign = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::NaN, kNameSpaceID_None,
                         attValue))
        format->mNaN=attValue;
        
    if (element->getAttr(txXSLTAtoms::percent, kNameSpaceID_None,
                         attValue)) {
        if (attValue.length() == 1)
            format->mPercent = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::perMille,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mPerMille = attValue.charAt(0);
        else if (!attValue.isEmpty())
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::zeroDigit,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mZeroDigit = attValue.charAt(0);
        else if (!attValue.isEmpty())
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::digit, kNameSpaceID_None,
                         attValue)) {
        if (attValue.length() == 1)
            format->mDigit = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

    if (element->getAttr(txXSLTAtoms::patternSeparator,
                         kNameSpaceID_None, attValue)) {
        if (attValue.length() == 1)
            format->mPatternSeparator = attValue.charAt(0);
        else
            success = MB_FALSE;
    }

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
    while (iter->hasNext()) {
        NamedMap* map = (NamedMap*) iter->next();
        if (map->get(name)) {
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
MBool ProcessorState::isStripSpaceAllowed(Node* node)
{
    if (!node)
        return MB_FALSE;

    switch (node->getNodeType()) {
        case Node::ELEMENT_NODE:
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
            if (mOutputFormat.mMethod == eHTMLOutput) {
                String ucName = name;
                ucName.toUpperCase();
                if (ucName.isEqual("SCRIPT"))
                    return MB_FALSE;
            }
            break;
        }
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        {
            if (!XMLUtils::shouldStripTextnode(node->getNodeValue()))
                return MB_FALSE;
            return isStripSpaceAllowed(node->getParentNode());
        }
        case Node::DOCUMENT_NODE:
        {
            return MB_TRUE;
        }
    }
    XMLSpaceMode mode = getXMLSpaceMode(node);
    if (mode == DEFAULT)
        return MB_FALSE;
    return (MBool)(STRIP == mode);
}

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
    while (iter->hasNext()) {
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
       return new DocumentFunctionCall(this, mXPathParseContext);
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

  //-------------------/
 //- Private Methods -/
//-------------------/

/*
 * Returns the closest xml:space value for the given Text node
 */
ProcessorState::XMLSpaceMode ProcessorState::getXMLSpaceMode(Node* aNode)
{
    NS_ASSERTION(aNode, "Calling getXMLSpaceMode with NULL node!");

    Node* parent = aNode;
    while (parent) {
        switch (parent->getNodeType()) {
            case Node::ELEMENT_NODE:
            {
                String value;
                ((Element*)parent)->getAttr(txXMLAtoms::space,
                                            kNameSpaceID_XML, value);
                if (value.isEqual(PRESERVE_VALUE))
                    return PRESERVE;
                break;
            }
            case Node::TEXT_NODE:
            case Node::CDATA_SECTION_NODE:
            {
                // We will only see this the first time through the loop
                // if the argument node is a text node.
                break;
            }
            default:
            {
                return DEFAULT;
            }
        }
        parent = parent->getParentNode();
    }
    return DEFAULT;
}

/**
 * Initializes this ProcessorState
**/
void ProcessorState::initialize()
{
    // add global variable set
    NamedMap* globalVars = new NamedMap();
    globalVars->setObjectDeletion(MB_TRUE);
    variableSets.push(globalVars);

    /* turn object deletion on for some of the Maps (NamedMap) */
    mExprHashes[SelectAttr].setOwnership(Map::eOwnsItems);
    mExprHashes[TestAttr].setOwnership(Map::eOwnsItems);
    mExprHashes[ValueAttr].setOwnership(Map::eOwnsItems);
    mPatternHashes[CountAttr].setOwnership(Map::eOwnsItems);
    mPatternHashes[FromAttr].setOwnership(Map::eOwnsItems);

    // determine xslt properties
    if (mSourceDocument) {
        loadedDocuments.put(mSourceDocument->getBaseURI(), mSourceDocument);
    }
    if (xslDocument) {
        loadedDocuments.put(xslDocument->getBaseURI(), xslDocument);
        // XXX hackarond to get namespacehandling in a little better shape
        // we won't need to do this once we resolve namespaces during parsing
        mXPathParseContext = xslDocument->getDocumentElement();
    }

    // make sure all keys are deleted
    xslKeys.setObjectDeletion(MB_TRUE);

    // Make sure all loaded documents get deleted
    loadedDocuments.setObjectDeletion(MB_TRUE);

    // add predefined default decimal format
    defaultDecimalFormatSet = MB_FALSE;
    decimalFormats.put("", new txDecimalFormat);
    decimalFormats.setObjectDeletion(MB_TRUE);
}

ProcessorState::ImportFrame::ImportFrame(ImportFrame* aFirstNotImported)
{
    mNamedAttributeSets.setObjectDeletion(MB_TRUE);
    mFirstNotImported = aFirstNotImported;
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
