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

#include "XSLProcessor.h"


  //----------------------------------/
 //- Implementation of XSLProcessor -/
//----------------------------------/

/**
 * XSLProcessor is a class for Processing XSL styelsheets
 * @author Keith Visco (kvisco@mitre.org)
**/

/**
 * A warning message used by all templates that do not allow non character
 * data to be generated
**/
const String XSLProcessor::NON_TEXT_TEMPLATE_WARNING =
"templates for the following element are not allowed to generate non character data: ";

/**
 * Creates a new XSLProcessor
**/
XSLProcessor::XSLProcessor() {

#ifdef MOZILLA
    NS_INIT_ISUPPORTS();
#endif

    xslVersion.append("1.0");
    appName.append("TransforMiiX");
    appVersion.append("1.0 [beta v19990813]");

    //-- create XSL element types
    xslTypes.setObjectDeletion(MB_TRUE);
    xslTypes.put(APPLY_TEMPLATES, new XSLType(XSLType::APPLY_TEMPLATES));
    xslTypes.put(ATTRIBUTE,       new XSLType(XSLType::ATTRIBUTE));
    xslTypes.put(ATTRIBUTE_SET,   new XSLType(XSLType::ATTRIBUTE_SET));
    xslTypes.put(CALL_TEMPLATE,   new XSLType(XSLType::CALL_TEMPLATE));
    xslTypes.put(CHOOSE,          new XSLType(XSLType::CHOOSE));
    xslTypes.put(COMMENT,         new XSLType(XSLType::COMMENT));
    xslTypes.put(COPY,            new XSLType(XSLType::COPY));
    xslTypes.put(COPY_OF,         new XSLType(XSLType::COPY_OF));
    xslTypes.put(ELEMENT,         new XSLType(XSLType::ELEMENT));
    xslTypes.put(FOR_EACH,        new XSLType(XSLType::FOR_EACH));
    xslTypes.put(IF,              new XSLType(XSLType::IF));
    xslTypes.put(MESSAGE,         new XSLType(XSLType::MESSAGE));
    xslTypes.put(NUMBER,          new XSLType(XSLType::NUMBER));
    xslTypes.put(OTHERWISE,       new XSLType(XSLType::OTHERWISE));
    xslTypes.put(PI,              new XSLType(XSLType::PI));
    xslTypes.put(PRESERVE_SPACE,  new XSLType(XSLType::PRESERVE_SPACE));
    xslTypes.put(STRIP_SPACE,     new XSLType(XSLType::STRIP_SPACE));
    xslTypes.put(TEMPLATE,        new XSLType(XSLType::TEMPLATE));
    xslTypes.put(TEXT,            new XSLType(XSLType::TEXT));
    xslTypes.put(VALUE_OF,        new XSLType(XSLType::VALUE_OF));
    xslTypes.put(VARIABLE,        new XSLType(XSLType::VARIABLE));
    xslTypes.put(WHEN,            new XSLType(XSLType::WHEN));

    //-- proprietary debug elements
    xslTypes.put("expr-debug", new XSLType(XSLType::EXPR_DEBUG));
} //-- XSLProcessor

/**
 * Default destructor
**/
XSLProcessor::~XSLProcessor() {
    //-- currently does nothing, but added for future use
} //-- ~XSLProcessor

#ifdef MOZILLA
// Provide a Create method that can be called by a factory constructor:
NS_METHOD
XSLProcessor::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
  
    XSLProcessor* xslp = new XSLProcessor();
    if (xslp == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    // Note that Create doesn't initialize the instance -- that has to
    // be done by the caller since the initialization args aren't passed
    // in here.

    // AddRef before calling QI -- this makes it easier to handle the QI
    // failure case because we'll always just Release and return
    NS_ADDREF(xslp);
    nsresult rv = xslp->QueryInterface(aIID, aResult);

    // This will free it if QI failed:
    NS_RELEASE(xslp);
    return rv;
}

NS_IMPL_ISUPPORTS(XSLProcessor, nsIDocumentTransformer::GetIID());
#endif

/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void XSLProcessor::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

#ifndef MOZILLA
XMLPrinter* XSLProcessor::createPrinter(Document& xslDocument, ostream& out) {

    //-- check result-ns of stylesheet element
    Element* stylesheet = xslDocument.getDocumentElement();
    XMLPrinter* xmlPrinter = 0;

    ostream* target = 0;
    if ( !out ) target = &cout;
    else target = &out;

    if ( stylesheet ) {
        String result_ns     = stylesheet->getAttribute(RESULT_NS_ATTR);
        Attr* indentResult   = stylesheet->getAttributeNode(INDENT_RESULT_ATTR);

        //-- create appropriate printer
        if ( result_ns.indexOf(HTML_NS) == 0) {
            xmlPrinter = new HTMLPrinter(*target);
        }
        else xmlPrinter = new XMLPrinter(*target);

        //-- set use formatting
        if ( indentResult ) {
            MBool useFormat = (MBool) YES_VALUE.isEqual(indentResult->getValue());
            xmlPrinter->setUseFormat(useFormat);
        }

    }
    else xmlPrinter = new XMLPrinter(*target);
    return xmlPrinter;
} //-- createPrinter
#endif

String& XSLProcessor::getAppName() {
    return appName;
} //-- getAppName

String& XSLProcessor::getAppVersion() {
    return appVersion;
} //-- getAppVersion


#ifndef MOZILLA
/**
 * Parses all XML Stylesheet PIs associated with the
 * given XML document. If any stylesheet PIs are found with
 * type="text/xsl" the href psuedo attribute value will be
 * added to the given href argument. If multiple text/xsl stylesheet PIs
 * are found, the one closest to the end of the document is used.
**/
void XSLProcessor::getHrefFromStylesheetPI(Document& xmlDocument, String& href) {

    NodeList* nl = xmlDocument.getChildNodes();
    String type;
    String tmpHref;
    for ( int i = 0; i < nl->getLength(); i++ ) {
        Node* node = nl->item(i);
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
                    href.append(tmpHref);
                }
            }
        }
    }

} //-- getHrefFromStylesheetPI

/**
 * Parses the contents of data, and returns the type and href psuedo attributes
**/
void XSLProcessor::parseStylesheetPI(String& data, String& type, String& href) {

    Int32 size = data.length();
    NamedMap bufferMap;
    bufferMap.put("type", &type);
    bufferMap.put("href", &href);
    int ccount = 0;
    MBool inLiteral = MB_FALSE;
    char matchQuote = '"';
    String sink;
    String* buffer = &sink;

    for (ccount = 0; ccount < size; ccount++) {
        char ch = data.charAt(ccount);
        switch ( ch ) {
            case ' ' :
                if ( inLiteral ) {
                    buffer->append(ch);
                }
                break;
            case '=':
                if ( inLiteral ) buffer->append(ch);
                else if ( buffer->length() > 0 ) {
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
Document* XSLProcessor::process(Document& xmlDocument, String& documentBase) {
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
Document* XSLProcessor::process(istream& xmlInput, istream& xslInput) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput);
    if (!xmlDoc) {
            String err("error reading XML document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            return 0;
    }
    //-- Read in XSL document
    Document* xslDoc = xmlParser.parse(xslInput);
    if (!xslDoc) {
            String err("error reading XSL stylesheet document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
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
Document* XSLProcessor::process(istream& xmlInput, String& documentBase) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput);
    if (!xmlDoc) {
            String err("error reading XML document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            return 0;
    }
    //-- Read in XSL document
    String href;
    String errMsg;
    getHrefFromStylesheetPI(*xmlDoc, href);
    istream* xslInput = URIUtils::getInputStream(href,documentBase,errMsg);
    Document* xslDoc = 0;
    if ( xslInput ) {
        xslDoc = xmlParser.parse(*xslInput);
        delete xslInput;
    }
    if (!xslDoc) {
            String err("error reading XSL stylesheet document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            delete xmlDoc;
            return 0;
    }
    Document* result = process(*xmlDoc, *xslDoc);
    delete xmlDoc;
    delete xslDoc;
    return result;
} //-- process
#endif

/**
 * Processes the given XML Document using the given XSL document
 * and returns the result tree
**/
Document* XSLProcessor::process(Document& xmlDocument, Document& xslDocument) {

    Document* result = new Document();

    //-- create a new ProcessorState
    ProcessorState ps(xslDocument, *result);

    //-- add error observers
    ListIterator* iter = errorObservers.iterator();
    while ( iter->hasNext()) {
        ps.addErrorObserver(*((ErrorObserver*)iter->next()));
    }
    delete iter;
      //-------------------------------------------------------/
     //- index templates and process root level xsl elements -/
    //-------------------------------------------------------/
    Element* stylesheet = xslDocument.getDocumentElement();
    NodeList* nl = stylesheet->getChildNodes();
    for (int i = 0; i < nl->getLength(); i++) {
        Node* node = nl->item(i);
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            Element* element = (Element*)node;
            DOMString name = element->getNodeName();
            switch (getElementType(name, &ps)) {
                case XSLType::ATTRIBUTE_SET:
                    ps.addAttributeSet(element);
                    break;
                case XSLType::TEMPLATE :
                    ps.addTemplate(element);
                    break;
                case XSLType::VARIABLE :
                {
                    String name = element->getAttribute(NAME_ATTR);
                    if ( name.length() == 0 ) {
                        notifyError("missing required name attribute for xsl:variable");
                        break;
                    }
                    ExprResult* exprResult = processVariable(node, element, &ps);
                    bindVariable(name, exprResult, &ps);
                    break;
                }
                case XSLType::PRESERVE_SPACE :
                {
                    String elements = element->getAttribute(ELEMENTS_ATTR);
                    if ( elements.length() == 0 ) {
                        //-- add error to ErrorObserver
                        String err("missing required 'elements' attribute for ");
                        err.append("xsl:preserve-space");
                        notifyError(err);
                    }
                    else ps.preserveSpace(elements);
                    break;
                }
                case XSLType::STRIP_SPACE :
                {
                    String elements = element->getAttribute(ELEMENTS_ATTR);
                    if ( elements.length() == 0 ) {
                        //-- add error to ErrorObserver
                        String err("missing required 'elements' attribute for ");
                        err.append("xsl:strip-space");
                        notifyError(err);
                    }
                    else ps.stripSpace(elements);
                    break;
                }
                default:
                    //-- unknown
                    break;
            }
        }
    }
      //----------------------------------------/
     //- Process root of XML source document -/
    //--------------------------------------/
    process(&xmlDocument, &xmlDocument, &ps);

    //-- return result Document
    return result;
} //-- process

#ifndef MOZILLA
/**
 * Processes the given XML Document using the given XSL document
 * and prints the results to the given ostream argument
**/
void XSLProcessor::process(Document& xmlDocument, Document& xslDocument, ostream& out) {
    Document* resultDoc = process(xmlDocument, xslDocument);
    XMLPrinter* xmlPrinter = createPrinter(xslDocument, out);
    xmlPrinter->print(resultDoc);
    delete xmlPrinter;
    delete resultDoc;
} //-- process


/**
 * Reads an XML Document from the given XML input stream.
 * The XSL Stylesheet is obtained from the XML Documents stylesheet PI.
 * If no Stylesheet is found, an empty document will be the result;
 * otherwise the XML Document is processed using the stylesheet.
 * The result tree is printed to the given ostream argument,
 * will not close the ostream argument
**/
void XSLProcessor::process(istream& xmlInput, String& documentBase, ostream& out) {

    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput);
    if (!xmlDoc) {
            String err("error reading XML document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            return;
    }
    //-- Read in XSL document
    String href;
    String errMsg;
    getHrefFromStylesheetPI(*xmlDoc, href);
    istream* xslInput = URIUtils::getInputStream(href,documentBase,errMsg);
    Document* xslDoc = 0;
    if ( xslInput ) {
        xslDoc = xmlParser.parse(*xslInput);
        delete xslInput;
    }
    if (!xslDoc) {
            String err("error reading XSL stylesheet document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            delete xmlDoc;
            return;
    }
    Document* result = process(*xmlDoc, *xslDoc);
    XMLPrinter* xmlPrinter = createPrinter(*xslDoc, out);
    xmlPrinter->print(result);
    delete xmlPrinter;
    delete xmlDoc;
    delete xslDoc;
    delete result;
} //-- process

/**
 * Reads an XML Document from the given XML input stream, and
 * processes the document using the XSL document derived from
 * the given XSL input stream.
 * The result tree is printed to the given ostream argument,
 * will not close the ostream argument
**/
void XSLProcessor::process(istream& xmlInput, istream& xslInput, ostream& out) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput);
    if (!xmlDoc) {
            String err("error reading XML document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            return;
    }
    //-- read in XSL Document
    Document* xslDoc = xmlParser.parse(xslInput);
    if (!xslDoc) {
            String err("error reading XSL stylesheet document: ");
            err.append(xmlParser.getErrorString());
            notifyError(err, ErrorObserver::FATAL);
            delete xmlDoc;
            return;
    }
    Document* result = process(*xmlDoc, *xslDoc);
    XMLPrinter* xmlPrinter = createPrinter(*xslDoc, out);
    xmlPrinter->print(result);
    delete xmlPrinter;
    delete xmlDoc;
    delete xslDoc;
    delete result;
} //-- process
#endif
  //-------------------/
 //- Private Methods -/
//-------------------/

/**
 * Returns the type of Element represented by the given name
 * @return the XSLType represented by the given element name
**/
short XSLProcessor::getElementType(String& name, ProcessorState* ps) {


    String namePart;
    XMLUtils::getNameSpace(name, namePart);
    XSLType* xslType = 0;

    if ( ps->getXSLNamespace().isEqual(namePart) ) {
        namePart.clear();
        XMLUtils::getLocalPart(name, namePart);
        xslType = (XSLType*) xslTypes.get(namePart);
    }

    if ( !xslType ) {
        return XSLType::LITERAL;
    }
    else return xslType->type;

} //-- getElementType

void XSLProcessor::bindVariable
    (String& name, ExprResult* exprResult, ProcessorState* ps)
{
    NamedMap* varSet = (NamedMap*)ps->getVariableSetStack()->peek();
    //-- check for duplicate variable names
    ExprResult* current = (ExprResult*) varSet->get(name);
    if ( current ) {
        //-- error cannot rebind variables
        String err("error cannot rebind variables: ");
        err.append(name);
        err.append(" already exists in this scope.");
        notifyError(err);
    }
    else {
        varSet->put((const String&)name, exprResult);
    }

} //-- bindVariable

/**
 * Gets the Text value of the given DocumentFragment. The value is placed
 * into the given destination String. If a non text node element is
 * encountered and warningForNonTextNodes is turned on, the MB_FALSE
 * will be returned, otherwise true is always returned.
 * @param dfrag the document fragment to get the text from
 * @param dest the destination string to place the text into.
 * @param deep indicates to process non text nodes and recusively append
 * their value. If this value is true, the allowOnlyTextNodes flag is ignored.
 * @param allowOnlyTextNodes
**/
MBool XSLProcessor::getText
    (DocumentFragment* dfrag, String& dest, MBool deep, MBool allowOnlyTextNodes)
{
    if ( !dfrag ) return MB_TRUE;

    MBool flag = MB_TRUE;
    if ( deep ) XMLDOMUtils::getNodeValue(dfrag, &dest);
    else {
        NodeList* nl = dfrag->getChildNodes();
        for ( int i = 0; i < nl->getLength(); i++ ) {
            Node* node = nl->item(i);
            switch(node->getNodeType()) {
                case Node::CDATA_SECTION_NODE:
                case Node::TEXT_NODE :
                    dest.append( ((CharacterData*)node)->getData() );
                    break;
                default:
                    if (allowOnlyTextNodes) flag = MB_FALSE;
                    break;
            }
        }
    }
    return flag;
} //-- getText

/**
 * Notifies all registered ErrorObservers of the given error
**/
void XSLProcessor::notifyError(const char* errorMessage) {
    String err(errorMessage);
    notifyError(err, ErrorObserver::NORMAL);
} //-- notifyError

/**
 * Notifies all registered ErrorObservers of the given error
**/
void XSLProcessor::notifyError(String& errorMessage) {
    notifyError(errorMessage, ErrorObserver::NORMAL);
} //-- notifyError

/**
 * Notifies all registered ErrorObservers of the given error
**/
void XSLProcessor::notifyError(String& errorMessage, ErrorObserver::ErrorLevel level) {
    ListIterator* iter = errorObservers.iterator();

    //-- send fatal errors to default observer if no error obersvers
    //-- have been registered
    if ((!iter->hasNext()) && (level == ErrorObserver::FATAL)) {
        fatalObserver.recieveError(errorMessage, level);
    }
    while ( iter->hasNext() ) {
        ErrorObserver* observer = (ErrorObserver*)iter->next();
        observer->recieveError(errorMessage, level);
    }
    delete iter;
} //-- notifyError

void XSLProcessor::process(Node* node, Node* context, ProcessorState* ps) {
    process(node, context, 0, ps);
} //-- process

void XSLProcessor::process(Node* node, Node* context, String* mode, ProcessorState* ps) {
    if ( !node ) return;
    Element* xslTemplate = ps->findTemplate(node, context, mode);
    if (!xslTemplate) return;
    processTemplate(node, xslTemplate, ps);
} //-- process

void XSLProcessor::processAction
    (Node* node, Node* xslAction, ProcessorState* ps)
{
    if (!xslAction) return;
    Document* resultDoc = ps->getResultDocument();

    short nodeType = xslAction->getNodeType();

    //-- handle text nodes
    if (nodeType == Node::TEXT_NODE) {
        String textValue;
        if ( ps->isXSLStripSpaceAllowed(xslAction) ) {
            //-- strip whitespace
            //-- Note: we might want to save results of whitespace stripping.
            //-- I was thinking about removing whitespace while reading in the
            //-- XSL document, but this won't handle the case of Dynamic XSL
            //-- documents
            const String curValue = ((Text*)xslAction)->getData();

            //-- set leading + trailing whitespace stripping flags
            MBool stripLWS = (MBool) (xslAction->getPreviousSibling());
            MBool stripTWS = (MBool) (xslAction->getNextSibling());
            XMLUtils::stripSpace(curValue,textValue, stripLWS, stripTWS);
            //-- create new text node and add it to the result tree
            //-- if necessary
        }
        else {
            textValue = ((Text*)xslAction)->getData();
        }
        if ( textValue.length() > 0)
            ps->addToResultTree(resultDoc->createTextNode(textValue));
        return;
    }
    //-- handle element nodes
    else if (nodeType == Node::ELEMENT_NODE) {

        String nodeName = xslAction->getNodeName();
        PatternExpr* pExpr = 0;
        Expr* expr = 0;

        Element* actionElement = (Element*)xslAction;
        switch ( getElementType(nodeName, ps) ) {

            //-- xsl:apply-templates
            case XSLType::APPLY_TEMPLATES :
            {

                String* mode = 0;
                Attr* modeAttr = actionElement->getAttributeNode(MODE_ATTR);
                if ( modeAttr ) mode = new String(modeAttr->getValue());
                String selectAtt  = actionElement->getAttribute(SELECT_ATTR);
                if ( selectAtt.length() == 0 ) selectAtt = "*";
                pExpr = ps->getPatternExpr(selectAtt);
                ExprResult* exprResult = pExpr->evaluate(node, ps);
                NodeSet* nodeSet = 0;
                if ( exprResult->getResultType() == ExprResult::NODESET ) {
                    nodeSet = (NodeSet*)exprResult;
                    //-- push nodeSet onto context stack
                    ps->getNodeSetStack()->push(nodeSet);
                    for (int i = 0; i < nodeSet->size(); i++) {
                        process(nodeSet->get(i), node, mode, ps);
                    }
                    //-- remove nodeSet from context stack
                    ps->getNodeSetStack()->pop();
                }
                else {
                    notifyError("error processing apply-templates");
                }
                //-- clean up
                delete mode;
                delete exprResult;
                break;
            }
            //-- attribute
            case XSLType::ATTRIBUTE:
            {
                Attr* attr = actionElement->getAttributeNode(NAME_ATTR);
                if ( !attr) {
                    notifyError("missing required name attribute for xsl:attribute");
                }
                else {
                    String ns = actionElement->getAttribute(NAMESPACE_ATTR);
                    //-- process name as an AttributeValueTemplate
                    String name;
                    processAttrValueTemplate(attr->getValue(),name,node,ps);
                    Attr* newAttr = 0;
                    //-- check name validity
                    if ( XMLUtils::isValidQName(name)) {
                        newAttr = resultDoc->createAttribute(name);
                    }
                    else {
                        String err("error processing xsl:attribute, ");
                        err.append(name);
                        err.append(" is not a valid QName.");
                        notifyError(err);
                    }
                    if ( newAttr ) {
                        DocumentFragment* dfrag = resultDoc->createDocumentFragment();
                        ps->getNodeStack()->push(dfrag);
                        processTemplate(node, actionElement, ps);
                        ps->getNodeStack()->pop();
                        String value;
                        XMLDOMUtils::getNodeValue(dfrag, &value);
                        XMLUtils::normalizeAttributeValue(value);
                        newAttr->setValue(value);
                        if ( ! ps->addToResultTree(newAttr) )
                            delete newAttr;
                    }
                }
                break;
            }
            // call-template
            case XSLType::CALL_TEMPLATE :
            {
                String templateName = actionElement->getAttribute(NAME_ATTR);
                if ( templateName.length() > 0 ) {
                    Element* xslTemplate = ps->getNamedTemplate(templateName);
                    if ( xslTemplate ) {
                        processTemplate(node, xslTemplate, ps);
                    }
                }
                else {
                    notifyError("missing required name attribute for xsl:call-template");
                }
            }
            // xsl:if
            case XSLType::CHOOSE :
            {
                NodeList* nl = actionElement->getChildNodes();
                Element* xslTemplate = 0;
                for (int i = 0; i < nl->getLength(); i++ ) {
                    Node* tmp = nl->item(i);
                    if ( tmp->getNodeType() != Node::ELEMENT_NODE ) continue;
                    xslTemplate = (Element*)tmp;
                    DOMString nodeName = xslTemplate->getNodeName();
                    switch ( getElementType(nodeName, ps) ) {
                        case XSLType::WHEN :
                        {
                            expr = ps->getExpr(xslTemplate->getAttribute(TEST_ATTR));
                            ExprResult* result = expr->evaluate(node, ps);
                            if ( result->booleanValue() ) {
                                processTemplate(node, xslTemplate, ps);
                                return;
                            }
                            break;
                        }
                        case XSLType::OTHERWISE:
                            processTemplate(node, xslTemplate, ps);
                            return; //-- important to break out of everything
                        default: //-- invalid xsl:choose child
                            break;
                    }
                } //-- end for-each child of xsl:choose
                break;
            }
            case XSLType::COMMENT:
            {
                DocumentFragment* dfrag = resultDoc->createDocumentFragment();
                ps->getNodeStack()->push(dfrag);
                processTemplate(node, actionElement, ps);
                ps->getNodeStack()->pop();
                String value;
                if (!getText(dfrag, value, MB_FALSE,MB_TRUE)) {
                    String warning(NON_TEXT_TEMPLATE_WARNING);
                    warning.append(COMMENT);
                    notifyError(warning, ErrorObserver::WARNING);
                }
                //XMLUtils::normalizePIValue(value);
                Comment* comment = resultDoc->createComment(value);
                if ( ! ps->addToResultTree(comment) ) delete comment;
                break;
            }
            case XSLType::COPY:
                xslCopy(node, actionElement, ps);
                break;
            case XSLType::ELEMENT:
            {
                Attr* attr = actionElement->getAttributeNode(NAME_ATTR);
                if ( !attr) {
                    notifyError("missing required name attribute for xsl:element");
                }
                else {
                    String ns = actionElement->getAttribute(NAMESPACE_ATTR);
                    //-- process name as an AttributeValueTemplate
                    String name;
                    processAttrValueTemplate(attr->getValue(),name,node,ps);
                    Element* element = 0;
                    //-- check name validity
                    if ( XMLUtils::isValidQName(name)) {
                        element = resultDoc->createElement(name);
                    }
                    else {
                        String err("error processing xsl:element, '");
                        err.append(name);
                        err.append("' is not a valid QName.");
                        notifyError(err);
                    }
                    if ( element ) {
                        ps->addToResultTree(element);
                        ps->getNodeStack()->push(element);
                        //-- processAttributeSets
                        processAttributeSets(actionElement->getAttribute(USE_ATTRIBUTE_SETS_ATTR),
                            node, ps);
                    }
                    //-- process template
                    processTemplate(node, actionElement, ps);
                    if( element ) ps->getNodeStack()->pop();
                }
                break;
            }
            //-- xsl:for-each
            case XSLType::FOR_EACH :
            {
                String selectAtt  = actionElement->getAttribute(SELECT_ATTR);

                if ( selectAtt.length() == 0 ) selectAtt = "*"; //-- default

                pExpr = ps->getPatternExpr(selectAtt);

                ExprResult* exprResult = pExpr->evaluate(node, ps);
                NodeSet* nodeSet = 0;
                if ( exprResult->getResultType() == ExprResult::NODESET ) {
                    nodeSet = (NodeSet*)exprResult;

                    //-- push nodeSet onto context stack
                    ps->getNodeSetStack()->push(nodeSet);
                    for (int i = 0; i < nodeSet->size(); i++) {
                        processTemplate(nodeSet->get(i), xslAction, ps);
                    }
                    //-- remove nodeSet from context stack
                    ps->getNodeSetStack()->pop();
                }
                else {
                    notifyError("error processing for-each");
                }
                //-- clean up exprResult
                delete exprResult;
                break;
            }
            // xsl:if
            case XSLType::IF :
            {
                DOMString selectAtt = actionElement->getAttribute(TEST_ATTR);
                expr = ps->getExpr(selectAtt);
                //-- check for Error
                    /* add later when we create ErrResult */

                ExprResult* exprResult = expr->evaluate(node, ps);
                if ( exprResult->booleanValue() ) {
                    processTemplate(node, actionElement, ps);
                }
                delete exprResult;

                break;
            }
            case XSLType::MESSAGE :
            {
                String message;
                XMLDOMUtils::getNodeValue(actionElement, &message);
                //-- we should add a MessageObserver class
                cout << "xsl:message - "<< message << endl;
                break;
            }
            //xsl:number
            case XSLType::NUMBER :
            {
                break;
            }
            //-- xsl:processing-instruction
            case XSLType::PI:
            {
                Attr* attr = actionElement->getAttributeNode(NAME_ATTR);
                if ( !attr) {
                    String err("missing required name attribute for xsl:");
                    err.append(PI);
                    notifyError(err);
                }
                else {
                    String ns = actionElement->getAttribute(NAMESPACE_ATTR);
                    //-- process name as an AttributeValueTemplate
                    String name;
                    processAttrValueTemplate(attr->getValue(),name,node,ps);
                    //-- check name validity
                    if ( !XMLUtils::isValidQName(name)) {
                        String err("error processing xsl:");
                        err.append(PI);
                        err.append(", '");
                        err.append(name);
                        err.append("' is not a valid QName.");
                        notifyError(err);
                    }
                    DocumentFragment* dfrag = resultDoc->createDocumentFragment();
                    ps->getNodeStack()->push(dfrag);
                    processTemplate(node, actionElement, ps);
                    ps->getNodeStack()->pop();
                    String value;
                    if (!getText(dfrag, value, MB_FALSE,MB_TRUE)) {
                        String warning(NON_TEXT_TEMPLATE_WARNING);
                        warning.append(PI);
                        notifyError(warning, ErrorObserver::WARNING);
                    }
                    XMLUtils::normalizePIValue(value);
                    ProcessingInstruction* pi
                            = resultDoc->createProcessingInstruction(name, value);
                    if ( ! ps->addToResultTree(pi) ) delete pi;
                }
                break;
            }
            //-- xsl:text
            case XSLType::TEXT :
            {
                String data;
                //-- get Node value, and do not perform whitespace stripping
                XMLDOMUtils::getNodeValue(actionElement, &data);
                Text* text = resultDoc->createTextNode(data);
                ps->addToResultTree(text);
                break;
            }
            case XSLType::EXPR_DEBUG: //-- proprietary debug element
            {
                DOMString exprAtt = actionElement->getAttribute(EXPR_ATTR);
                Expr* expr = ps->getExpr(exprAtt);
                ExprResult* exprResult = expr->evaluate(node, ps);
                String data("expr debug: ");
                expr->toString(data);
                cout << data << endl;
                data.clear();
                cout << "result: ";
                if ( exprResult ) {
                    switch ( exprResult->getResultType() ) {
                        case  ExprResult::NODESET:
                            cout << "#NodeSet - ";
                        default:
                            exprResult->stringValue(data);
                            cout << data;
                            break;
                    }
                }
                cout << endl;

                delete exprResult;
                break;
            }
            //-- xsl:value-of
            case XSLType::VALUE_OF :
            {
                DOMString selectAtt = actionElement->getAttribute(SELECT_ATTR);

                Expr* expr = ps->getExpr(selectAtt);
                ExprResult* exprResult = expr->evaluate(node, ps);
                String value;
                if ( !exprResult ) {
                    notifyError("null ExprResult");
                    break;
                }
                exprResult->stringValue(value);
                //-- handle whitespace stripping
                if ( exprResult->getResultType() == ExprResult::NODESET) {
                    NodeSet* nodes = (NodeSet*)exprResult;
                    if ( nodes->size() > 0) {
                        Node* node = nodes->get(0);
                        if ( ps->isStripSpaceAllowed(node) ) {
                            const String temp = value;
                            value.clear();
                            XMLUtils::stripSpace(temp, value);
                        }
                    }
                }
                ps->addToResultTree(resultDoc->createTextNode(value));
                delete exprResult;
                break;
            }
            case XSLType::VARIABLE :
            {
                String name = actionElement->getAttribute(NAME_ATTR);
                if ( name.length() == 0 ) {
                    notifyError("missing required name attribute for xsl:variable");
                    break;
                }
                ExprResult* exprResult = processVariable(node, actionElement, ps);
                bindVariable(name, exprResult, ps);
                break;
            }
            //-- literal
            default:
                Element* element = resultDoc->createElement(nodeName);
                ps->addToResultTree(element);
                ps->getNodeStack()->push(element);
                //-- handle attributes
                NamedNodeMap* atts = actionElement->getAttributes();
                if ( atts ) {
                    String xsltNameSpace = ps->getXSLNamespace();
                    NodeSet nonXSLAtts(atts->getLength());
                    int i;
                    //-- process special XSL attributes first
                    for ( i = 0; i < atts->getLength(); i++ ) {
                        Attr* attr = (Attr*) atts->item(i);
                        //-- filter attributes in the XSLT namespace
                        String attrNameSpace;
                        XMLUtils::getNameSpace(attr->getName(), attrNameSpace);
                        if ( attrNameSpace.isEqual(xsltNameSpace) ) {
                            //-- check for useAttributeSet
                            String localPart;
                            XMLUtils::getLocalPart(attr->getName(), localPart);
                            if ( USE_ATTRIBUTE_SETS_ATTR.isEqual(localPart) ) {
                                processAttributeSets(attr->getValue(), node, ps);
                            }
                            continue;
                        }
                        else nonXSLAtts.add(attr);
                    }
                    //-- process all non XSL attributes
                    for ( i = 0; i < nonXSLAtts.size(); i++ ) {
                        Attr* attr = (Attr*) nonXSLAtts.get(i);
                        Attr* newAttr = resultDoc->createAttribute(attr->getName());
                        //-- process Attribute Value Templates
                        String value;
                        processAttrValueTemplate(attr->getValue(), value, node, ps);
                        newAttr->setValue(value);
                        ps->addToResultTree(newAttr);
                    }
                }
                //-- process children
                NodeList* nl = xslAction->getChildNodes();
                for (int i = 0; i < nl->getLength(); i++) {
                    processAction(node, nl->item(i),ps);
                }
                ps->getNodeStack()->pop();
                break;
        }
    }

    //cout << "XSLProcessor#processAction [exit]\n";
} //-- processAction

/**
 * Processes the attribute sets specified in the names argument
**/
void XSLProcessor::processAttributeSets
    (const String& names, Node* node, ProcessorState* ps)
{
    //-- split names
    Tokenizer tokenizer(names);
    String name;
    while ( tokenizer.hasMoreTokens() ) {
        tokenizer.nextToken(name);
        NodeSet* attSet = ps->getAttributeSet(name);
        if ( attSet ) {
            for ( int i = 0; i < attSet->size(); i++) {
                processAction(node, attSet->get(i), ps);
            }
        }
    }
} //-- processAttributeSets

/**
 * Processes the given attribute value as an AttributeValueTemplate
 * @param attValue the attribute value to process
 * @param result, the String in which to store the result
 * @param context the current context node
 * @param ps the current ProcessorState
**/
void XSLProcessor::processAttrValueTemplate
    (const String& attValue, String& result, Node* context, ProcessorState* ps)
{
    AttributeValueTemplate* avt = 0;
    avt = exprParser.createAttributeValueTemplate(attValue);
    ExprResult* exprResult = avt->evaluate(context,ps);
    exprResult->stringValue(result);
    delete exprResult;
    delete avt;

} //-- processAttributeValueTemplate


/**
 *  Processes the set of nodes using the given context, and ProcessorState
**/
void XSLProcessor::processTemplate(Node* node, Node* xslTemplate, ProcessorState* ps) {

    if ( !xslTemplate ) {
        //-- do default?
    }
    else {
        NodeList* nl = xslTemplate->getChildNodes();
        for (int i = 0; i < nl->getLength(); i++) {
            processAction(node, nl->item(i), ps);
        }
    }
} //-- processTemplate

/**
 *  processes the xslVariable parameter as an xsl:variable using the given context,
 *  and ProcessorState.
 *  If the xslTemplate contains an "expr" attribute, the attribute is evaluated
 *  as an Expression and the ExprResult is returned. Otherwise the xslVariable is
 *  is processed as a template, and it's result is converted into an ExprResult
 *  @return an ExprResult
**/
ExprResult* XSLProcessor::processVariable
        (Node* node, Element* xslVariable, ProcessorState* ps)
{

    if ( !xslVariable ) {
        return new StringResult("unable to process variable");
    }

    //-- check for EXPR_ATTR
    Attr* attr = xslVariable->getAttributeNode(EXPR_ATTR);
    if ( attr ) {
        Expr* expr = ps->getExpr(attr->getValue());
        return expr->evaluate(node, ps);
    }
    else {
        NodeList* nl = xslVariable->getChildNodes();
        Document* resultTree = ps->getResultDocument();
        NodeStack* nodeStack = ps->getNodeStack();
        nodeStack->push(resultTree->createDocumentFragment());
        for (int i = 0; i < nl->getLength(); i++) {
            processAction(node, nl->item(i), ps);
        }
        Node* node = nodeStack->pop();
        //-- add clean up for This new NodeSet;
        NodeSet* nodeSet = new NodeSet();
        nodeSet->add(node);
        return nodeSet;
    }
} //-- processTemplate

/**
 * Performs the xsl:copy action as specified in the XSL Working Draft
**/
void XSLProcessor::xslCopy(Node* node, Element* action, ProcessorState* ps) {
    if ( !node ) return;

    Document* resultDoc = ps->getResultDocument();
    Node* copy = 0;
    switch ( node->getNodeType() ) {
        case Node::DOCUMENT_NODE:
            //-- just process children
            processTemplate(node, action, ps);
            break;
        case Node::ATTRIBUTE_NODE:
        {
            Attr* attr = (Attr*) node;
            Attr* copyAtt = resultDoc->createAttribute(attr->getName());
            copyAtt->setValue(attr->getValue());
            copy = copyAtt;
            break;
        }
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)node;
            String nodeName = element->getNodeName();
            copy = resultDoc->createElement(nodeName);
            ps->addToResultTree(copy);
            ps->getNodeStack()->push(copy);
            //-- copy namespace attributes
            // * add later *
            processTemplate(node, action, ps);
            ps->getNodeStack()->pop();
            return;
        }
        case Node::CDATA_SECTION_NODE:
        {
            CDATASection* cdata = (CDATASection*)node;
            copy = resultDoc->createCDATASection(cdata->getData());
            break;
        }
        case Node::TEXT_NODE:
        {
            Text* text = (Text*)node;
            copy = resultDoc->createTextNode(text->getData());
            break;
        }
        case Node::PROCESSING_INSTRUCTION_NODE:
        {
            ProcessingInstruction* pi = (ProcessingInstruction*)node;
            copy = resultDoc->createProcessingInstruction(pi->getTarget(), pi->getData());
            break;
        }
        case Node::COMMENT_NODE:
        {
            Comment* comment = (Comment*)node;
            copy = resultDoc->createComment(comment->getData());
            break;
        }
        default:
            break;
    }
    if ( copy ) ps->addToResultTree(copy);
} //-- xslCopy


#ifdef MOZILLA
NS_IMETHODIMP
XSLProcessor::TransformDocument(nsIDOMElement* aSourceDOM, 
                               nsIDOMElement* aStyleDOM,
                               nsIDOMDocument* aOutputDoc,
                               nsIObserver* aObserver)
{
  return NS_OK;
}
#endif

XSLType::XSLType() {
    this->type = LITERAL;
} //-- XSLType

XSLType::XSLType(const XSLType& xslType) {
    this->type = xslType.type;
} //-- XSLType

XSLType::XSLType(short type) {
    this->type = type;
} //-- XSLType



