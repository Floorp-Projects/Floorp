/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * Implementation of ProcessorState
 * This code was ported from XSL:P
 * @author <a href="kvisco@mitre.org">Keith Visco</a>
**/

#include "ProcessorState.h"

  //-------------/
 //- Constants -/
//-------------/
const String ProcessorState::wrapperNSPrefix  = "transformiix";
const String ProcessorState::wrapperName      = "transformiix:result";
const String ProcessorState::wrapperNS        = "http://www.mitre.org/TransforMiix";

/**
 * Creates a new ProcessorState for the given XSL document
 * and resultDocument
**/
ProcessorState::ProcessorState(Document& xslDocument, Document& resultDocument) {
    this->xslDocument = &xslDocument;
    this->resultDocument = &resultDocument;
    initialize();
} //-- ProcessorState

/**
 * Destroys this ProcessorState
**/
ProcessorState::~ProcessorState() {
  delete dfWildCardTemplate;
  delete dfTextTemplate;
  delete nodeStack;
  while ( ! variableSets.empty() ) {
      delete (NamedMap*) variableSets.pop();
  }
} //-- ~ProcessorState


/**
 *  Adds the given attribute set to the list of available named attribute sets
 * @param attributeSet the Element to add as a named attribute set
**/
void ProcessorState::addAttributeSet(Element* attributeSet) {
    if ( !attributeSet ) return;
    String name = attributeSet->getAttribute(NAME_ATTR);
    if ( name.length() == 0 ) {
        cout << "missing required name attribute for xsl:" << ATTRIBUTE_SET <<endl;
        return;
    }
    //-- get attribute set, if already exists, then merge
    NodeSet* attSet = (NodeSet*)namedAttributeSets.get(name);
    if ( !attSet) {
        attSet = new NodeSet();
        namedAttributeSets.put(name, attSet);
    }

    //-- add xsl:attribute elements to attSet
    NodeList* nl = attributeSet->getChildNodes();
    for ( int i = 0; i < nl->getLength(); i++) {
        Node* node = nl->item(i);
        if ( node->getNodeType() == Node::ELEMENT_NODE) {
            String nodeName = node->getNodeName();
            String ns;
            XMLUtils::getNameSpace(nodeName, ns);
            if ( !xsltNameSpace.isEqual(ns)) continue;
            String localPart;
            XMLUtils::getLocalPart(nodeName, localPart);
            if ( ATTRIBUTE.isEqual(localPart) ) attSet->add(node);
        }
    }

} //-- addAttributeSet

/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void ProcessorState::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

/**
 *  Adds the given template to the list of templates to process
 * @param xslTemplate, the Element to add as a template
**/
void ProcessorState::addTemplate(Element* xslTemplate) {
    if ( !xslTemplate ) return;
    const String match = xslTemplate->getAttribute(MATCH_ATTR);
    String name = xslTemplate->getAttribute(NAME_ATTR);
    if ( name.length() > 0 ) {
        //-- check for duplicates
        MITREObjectWrapper* mObj = (MITREObjectWrapper*)namedTemplates.get(name);
        if ( mObj ) {
            String warn("error duplicate template name: ");
            warn.append(name);
            warn.append("\n -- using template closest to end of document");
            recieveError(warn,ErrorObserver::WARNING);
            delete mObj;
        }
        MITREObjectWrapper* oldObj = mObj;
        mObj= new MITREObjectWrapper();
        mObj->object = xslTemplate;
        namedTemplates.put(name,mObj);
        if ( oldObj ) delete oldObj;
    }
    patternExprHash.put(match, exprParser.createPatternExpr(match));
    templates.add(xslTemplate);
} //-- addTempalte

/**
 * Adds the given node to the result tree
 * @param node the Node to add to the result tree
**/
MBool ProcessorState::addToResultTree(Node* node) {

    Node* current = nodeStack->peek();

    switch (node->getNodeType()) {

        case Node::ATTRIBUTE_NODE:
        {
            if (current->getNodeType() != Node::ELEMENT_NODE) return MB_FALSE;
            Element* element = (Element*)current;
            Attr* attr = (Attr*)node;
            element->setAttribute(attr->getName(),attr->getValue());
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
                    nodeStack->push(wrapper);
                    current->appendChild(wrapper);
                    current = wrapper;
                }
            }
            current->appendChild(node);
            break;
        case Node::TEXT_NODE :
            //-- if current node is the document, create wrapper element
            if ( current == resultDocument ) {
                String nodeName(wrapperName);
                Element* wrapper = resultDocument->createElement(nodeName);
                nodeStack->push(wrapper);
                current->appendChild(wrapper);
                current = wrapper;
            }
            current->appendChild(node);
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE :
            current->appendChild(node);
            break;
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
Node* copyNode(Node* node) {
    return 0;
} //-- copyNode

/**
 * Finds a template for the given Node. Only templates with
 * a mode attribute equal to the given mode will be searched.
**/
Element* ProcessorState::findTemplate(Node* node, Node* context) {
    return findTemplate(node, context, 0);
} //-- findTemplate

/**
 * Finds a template for the given Node. Only templates with
 * a mode attribute equal to the given mode will be searched.
**/
Element* ProcessorState::findTemplate(Node* node, Node* context, String* mode) {

    if (!node) return 0;
    Element* matchTemplate = 0;
    double currentPriority = 0.5;

    for (int i = 0; i < templates.size(); i++) {

        //cout << "looking at template: " << i << endl;
        Element* xslTemplate = (Element*) templates.get(i);

        //-- check mode attribute
        Attr* modeAttr = xslTemplate->getAttributeNode(MODE_ATTR);
        if (( mode ) && (!modeAttr)) continue;
        else if (( !mode ) && (modeAttr)) continue;
        else if ( mode ) {
            if ( ! mode->isEqual( modeAttr->getValue() )  ) continue;
        }
        //-- get templates match expr
        String match = xslTemplate->getAttribute(MATCH_ATTR);
        //cout << "match attr: " << match << endl;

        //-- get Expr from expression hash table
        PatternExpr* pExpr = getPatternExpr(match);
        if ( !pExpr ) continue;

        if (pExpr->matches(node, context, this)) {
            String priorityAttr = xslTemplate->getAttribute(PRIORITY_ATTR);
            double tmpPriority = 0;
            if ( priorityAttr.length() > 0 ) {
                Double dbl(priorityAttr);
                tmpPriority = dbl.doubleValue();
            }
            else tmpPriority = pExpr->getDefaultPriority(node,context,this);

            if (( !matchTemplate ) || ( tmpPriority >= currentPriority ))
                matchTemplate = xslTemplate;
            currentPriority = tmpPriority;
        }
    }
    //cout << "findTemplate:end"<<endl;

    return matchTemplate;
} //-- findTemplate

/**
 * Returns the AttributeSet associated with the given name
 * or null if no AttributeSet is found
**/
NodeSet* ProcessorState::getAttributeSet(const String& name) {
    return (NodeSet*)namedAttributeSets.get(name);
} //-- getAttributeSet

Expr* ProcessorState::getExpr(const String& pattern) {
    Expr* expr = (Expr*)exprHash.get(pattern);
    if ( !expr ) {
        expr = exprParser.createExpr(pattern);
        if ( !expr ) {
            String err = "invalid expression: ";
            err.append(pattern);
            expr = new ErrorFunctionCall(err);
        }
        exprHash.put(pattern, expr);
    }
    return expr;
} //-- getExpr

/**
 * Returns the template associated with the given name, or
 * null if not template is found
**/
Element* ProcessorState::getNamedTemplate(String& name) {
    MITREObjectWrapper* mObj = (MITREObjectWrapper*)namedTemplates.get(name);
    if ( mObj ) {
        return (Element*)mObj->object;
    }
    return 0;
} //-- getNamedTemplate


NodeStack* ProcessorState::getNodeStack() {
    return nodeStack;
} //-- getNodeStack

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

NodeSet* ProcessorState::getTemplates() {
   return &templates;
} //-- getTemplates


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
 * Adds the set of names to the Whitespace preserving element set
**/
void ProcessorState::preserveSpace(String& names) {

    //-- split names on whitespace
    Tokenizer tokenizer(names);
    String name;
    while ( tokenizer.hasMoreTokens() ) {
        tokenizer.nextToken(name);
        wsPreserve.add(new String(name));
        wsStrip.remove(name);
    }

} //-- preserveSpace

/**
 * Adds the set of names to the Whitespace stripping element set
**/
void ProcessorState::stripSpace(String& names) {
    //-- split names on whitespace
    Tokenizer tokenizer(names);
    String name;
    while ( tokenizer.hasMoreTokens() ) {
        tokenizer.nextToken(name);
        wsStrip.add(new String(name));
        wsPreserve.remove(name);
    }

} //-- stripSpace

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
            exprResult = (ExprResult*)map->get(name);
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
            //-- check Whitespace element names against given Node
            String name = node->getNodeName();
            if (wsPreserve.contains(name)) return MB_FALSE;
            if (wsStrip.contains(name)) return MB_TRUE;
            break;
        }
        case Node::TEXT_NODE:
            return isStripSpaceAllowed(node->getParentNode());
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
    namedAttributeSets.setObjectDeletion(MB_TRUE);

    //-- named templates uses deletion, to remove the ObjectWrappers
    namedTemplates.setObjectDeletion(MB_TRUE);
    //-- do not set ObjectDeletion for templates, since the Document
    //-- handles the cleanup

    //-- create NodeStack
    nodeStack = new NodeStack();
    nodeStack->push(this->resultDocument);

    //-- determine xsl properties
    Element* element = xslDocument->getDocumentElement();
    if ( element ) {
        //-- process namespace nodes
        NamedNodeMap* atts = element->getAttributes();
        if ( atts ) {
            for (int i = 0; i < atts->getLength(); i++) {
                Attr* attr = (Attr*)atts->item(i);
                String attName = attr->getName();
                String attValue = attr->getValue();
                if ( attName.indexOf(XMLUtils::XMLNS) == 0) {
                    String ns;
                    XMLUtils::getLocalPart(attName, ns);
                    //-- default namespace
                    if ( attName.isEqual(XMLUtils::XMLNS) ) {
                        //-- handle default
                        //-- do nothing for now
                    }
                    // namespace declaration
                    else {
                        String ns;
                        XMLUtils::getNameSpace(attName, ns);
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
            } //-- end for each att
        } //-- end if atts are not null
    } //-- end if document element exists

    /* Create default (built-in) templates */

    //-- create default template for elements


    String templateName = xsltNameSpace;
    if (templateName.length() > 0) templateName.append(':');
    templateName.append(TEMPLATE);

    String actionName = xsltNameSpace;
    if ( actionName.length()>0) actionName.append(':');
    actionName.append(APPLY_TEMPLATES);

    dfWildCardTemplate = xslDocument->createElement(templateName);
    dfWildCardTemplate->setAttribute(MATCH_ATTR, "* | /");
    dfWildCardTemplate->appendChild(xslDocument->createElement(actionName));
    templates.add(dfWildCardTemplate);

    //-- create default "built-in" templates for text nodes
    dfTextTemplate = xslDocument->createElement(templateName);
    dfTextTemplate->setAttribute(MATCH_ATTR, "text()|@*");
    actionName = xsltNameSpace;
    if ( actionName.length()>0) actionName.append(':');
    actionName.append(VALUE_OF);
    Element* value_of = xslDocument->createElement(actionName);
    value_of->setAttribute(SELECT_ATTR, IDENTITY_OP);
    dfTextTemplate->appendChild(value_of);
    templates.add(dfTextTemplate);

    //-- add PatternExpr hash for default templates
    patternExprHash.put("*",      new WildCardExpr());
    patternExprHash.put("/",      new RootExpr());
    patternExprHash.put("text()", new TextExpr());

    //cout << "XSLT namespace: " << xsltNameSpace << endl;
} //-- initialize


