/*
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
 * (C) 1999 Keith Visco. All Rights Reserved.
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
 * $Id: XSLTProcessor.cpp,v 1.18 2000/07/06 12:35:42 axel%pike.org Exp $
 */

#include "XSLTProcessor.h"
#ifdef MOZ_XSL
#include "nsIObserverService.h"
#endif

  //-----------------------------------/
 //- Implementation of XSLTProcessor -/
//-----------------------------------/

/**
 * XSLTProcessor is a class for Processing XSL styelsheets
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.18 $ $Date: 2000/07/06 12:35:42 $
**/

/**
 * A warning message used by all templates that do not allow non character
 * data to be generated
**/
const String XSLTProcessor::NON_TEXT_TEMPLATE_WARNING =
"templates for the following element are not allowed to generate non character data: ";

/**
 * Creates a new XSLTProcessor
**/
XSLTProcessor::XSLTProcessor() {

#ifdef MOZ_XSL
    NS_INIT_ISUPPORTS();
#endif

    xslVersion.append("1.0");
    appName.append("TransforMiiX");
    appVersion.append("1.0 [beta v20000523]");


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
    xslTypes.put(INCLUDE,         new XSLType(XSLType::INCLUDE));
    xslTypes.put(MESSAGE,         new XSLType(XSLType::MESSAGE));
    xslTypes.put(NUMBER,          new XSLType(XSLType::NUMBER));
    xslTypes.put(OTHERWISE,       new XSLType(XSLType::OTHERWISE));
    xslTypes.put(OUTPUT,          new XSLType(XSLType::OUTPUT));
    xslTypes.put(PARAM,           new XSLType(XSLType::PARAM));
    xslTypes.put(PI,              new XSLType(XSLType::PI));
    xslTypes.put(PRESERVE_SPACE,  new XSLType(XSLType::PRESERVE_SPACE));
    xslTypes.put(STRIP_SPACE,     new XSLType(XSLType::STRIP_SPACE));
    xslTypes.put(SORT,            new XSLType(XSLType::SORT));
    xslTypes.put(TEMPLATE,        new XSLType(XSLType::TEMPLATE));
    xslTypes.put(TEXT,            new XSLType(XSLType::TEXT));
    xslTypes.put(VALUE_OF,        new XSLType(XSLType::VALUE_OF));
    xslTypes.put(VARIABLE,        new XSLType(XSLType::VARIABLE));
    xslTypes.put(WHEN,            new XSLType(XSLType::WHEN));
    xslTypes.put(WITH_PARAM,      new XSLType(XSLType::WITH_PARAM));

    //-- proprietary debug elements
    xslTypes.put("expr-debug", new XSLType(XSLType::EXPR_DEBUG));
} //-- XSLTProcessor

/**
 * Default destructor
**/
XSLTProcessor::~XSLTProcessor() {
    //-- currently does nothing, but added for future use
} //-- ~XSLTProcessor

#ifdef MOZ_XSL
NS_IMPL_ISUPPORTS1(XSLTProcessor, nsIDocumentTransformer)
#endif

/**
 * Registers the given ErrorObserver with this ProcessorState
**/
void XSLTProcessor::addErrorObserver(ErrorObserver& errorObserver) {
    errorObservers.add(&errorObserver);
} //-- addErrorObserver

#ifndef MOZ_XSL
void XSLTProcessor::print
    (Document& document, OutputFormat* format, ostream& out)
{

    XMLPrinter* xmlPrinter = 0;
    ostream* target = 0;
    if ( !out ) target = &cout;
    else target = &out;

    MBool indent = MB_FALSE;
    if (format->isMethodExplicit()) {
        if (format->isHTMLOutput()) xmlPrinter = new HTMLPrinter(*target);
        else xmlPrinter = new XMLPrinter(*target);
        indent = format->getIndent();
    }
    else {
        //-- try to determine output method
        Element* element = document.getDocumentElement();
        String name;
        if (element) name = element->getNodeName();
        name.toUpperCase();
        if (name.isEqual("HTML")) {
            xmlPrinter = new HTMLPrinter(*target);
            if (format->isIndentExplicit()) indent = format->getIndent();
            else indent = MB_TRUE;
        }
        else {
            xmlPrinter = new XMLPrinter(*target);
            indent = format->getIndent();
        }
    }

    xmlPrinter->setUseFormat(indent);
    xmlPrinter->print(&document);
    delete xmlPrinter;

} //-- print
#endif

String& XSLTProcessor::getAppName() {
    return appName;
} //-- getAppName

String& XSLTProcessor::getAppVersion() {
    return appVersion;
} //-- getAppVersion

#ifndef MOZ_XSL
/**
 * Parses all XML Stylesheet PIs associated with the
 * given XML document. If any stylesheet PIs are found with
 * type="text/xsl" the href psuedo attribute value will be
 * added to the given href argument. If multiple text/xsl stylesheet PIs
 * are found, the one closest to the end of the document is used.
**/
void XSLTProcessor::getHrefFromStylesheetPI(Document& xmlDocument, String& href) {

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
void XSLTProcessor::parseStylesheetPI(String& data, String& type, String& href) {

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
Document* XSLTProcessor::process(Document& xmlDocument, String& documentBase) {
    //-- look for Stylesheet PI
    Document xslDocument; //-- empty for now
    return process(xmlDocument, xslDocument, documentBase);
} //-- process

/**
 * Reads an XML Document from the given XML input stream, and
 * processes the document using the XSL document derived from
 * the given XSL input stream.
 * @return the result tree.
**/
Document* XSLTProcessor::process
(istream& xmlInput, istream& xslInput, String& documentBase) {
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
    Document* result = process(*xmlDoc, *xslDoc, documentBase);
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
Document* XSLTProcessor::process(istream& xmlInput, String& documentBase) {
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
    Document* result = process(*xmlDoc, *xslDoc, documentBase);
    delete xmlDoc;
    delete xslDoc;
    return result;
} //-- process
#endif

/**
 * Processes the Top level elements for an XSL stylesheet
**/
void XSLTProcessor::processTopLevel
   (Document* xslDocument, ProcessorState* ps)
{

    if (!xslDocument) return;

      //-------------------------------------------------------/
     //- index templates and process top level xsl elements -/
    //-------------------------------------------------------/

    Element* stylesheet = xslDocument->getDocumentElement();
    processTopLevel(stylesheet, ps);

}

void XSLTProcessor::processTopLevel
   (Element* stylesheet, ProcessorState* ps)
{
    if (!stylesheet) return;

    NodeList* nl = stylesheet->getChildNodes();
    for (int i = 0; i < nl->getLength(); i++) {
        Node* node = nl->item(i);
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            Element* element = (Element*)node;
            String name = element->getNodeName();
            switch (getElementType(name, ps)) {
                case XSLType::ATTRIBUTE_SET:
                    ps->addAttributeSet(element);
                    break;
                case XSLType::PARAM :
                {
                    String name = element->getAttribute(NAME_ATTR);
                    if ( name.length() == 0 ) {
                        notifyError("missing required name attribute for xsl:param");
                        break;
                    }

                    ExprResult* exprResult
                        = processVariable(node, element, ps);

                    bindVariable(name, exprResult, MB_TRUE, ps);
                    break;
                }
                case XSLType::INCLUDE :
                {

                    String href = element->getAttribute(HREF_ATTR);
                    //-- Read in XSL document

                    if (ps->getInclude(href)) {
                        String err("stylesheet already included: ");
                        err.append(href);
                        notifyError(err, ErrorObserver::WARNING);
                        break;
                    }

                    //-- get document base
                    String documentBase;
                    String currentHref;
                    //ps->getDocumentHref(element->getOwnerDocument(),
                    //        currentHref);
                    if (currentHref.length() == 0) {
                        documentBase.append(ps->getDocumentBase());
                    }
                    else {
                        //-- Fix for relative URIs (npride)
                        documentBase.append(ps->getDocumentBase());
                        documentBase.append('/');
                        //-- End fix
                        URIUtils::getDocumentBase(currentHref, documentBase);
                    }

                    String errMsg;
                    istream* xslInput = URIUtils::getInputStream(href,documentBase,errMsg);
                    Document* xslDoc = 0;
                    XMLParser xmlParser;
                    if ( xslInput ) {
                        xslDoc = xmlParser.parse(*xslInput);
                        delete xslInput;
                    }
                    if (!xslDoc) {
                        String err("error including XSL stylesheet: ");
                        err.append(href);
                        err.append("; ");
                        err.append(xmlParser.getErrorString());
                        notifyError(err);
                    }
                    else {
                        //-- add stylesheet to list of includes
                        ps->addInclude(href, xslDoc);
                        processTopLevel(xslDoc, ps);
                    }
                    break;

                }
                case XSLType::OUTPUT :
                {
                    OutputFormat* format = ps->getOutputFormat();

                    String attValue = element->getAttribute(METHOD_ATTR);
                    if (attValue.length() > 0) ps->setOutputMethod(attValue);

                    attValue = element->getAttribute(VERSION_ATTR);
                    if (attValue.length() > 0) format->setVersion(attValue);

                    attValue = element->getAttribute(ENCODING_ATTR);
                    if (attValue.length() > 0) format->setEncoding(attValue);

                    attValue = element->getAttribute(INDENT_ATTR);
                    if (attValue.length() > 0) {
                        MBool allowIndent = attValue.isEqual(YES_VALUE);
                       format->setIndent(allowIndent);
                    }

                    attValue = element->getAttribute(DOCTYPE_PUBLIC_ATTR);
                    if (attValue.length() > 0)
                        format->setDoctypePublic(attValue);

                    attValue = element->getAttribute(DOCTYPE_SYSTEM_ATTR);
                    if (attValue.length() > 0)
                        format->setDoctypeSystem(attValue);

                    break;
                }
                case XSLType::TEMPLATE :
                    ps->addTemplate(element);
                    break;
                case XSLType::VARIABLE :
                {
                    String name = element->getAttribute(NAME_ATTR);
                    if ( name.length() == 0 ) {
                        notifyError("missing required name attribute for xsl:variable");
                        break;
                    }
                    ExprResult* exprResult = processVariable(node, element, ps);
                    bindVariable(name, exprResult, MB_FALSE, ps);
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
                    else ps->preserveSpace(elements);
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
                    else ps->stripSpace(elements);
                    break;
                }
                default:
                    //-- unknown
                    break;
            }
        }
    }

} //-- process(Document, ProcessorState)

/**
 * Processes the given XML Document using the given XSL document
 * and returns the result tree
**/
Document* XSLTProcessor::process
   (Document& xmlDocument, Document& xslDocument, String& documentBase)
{

    Document* result = new Document();

    //-- create a new ProcessorState
    ProcessorState ps(xslDocument, *result);
    ps.setDocumentBase(documentBase);

    //-- add error observers
    ListIterator* iter = errorObservers.iterator();
    while ( iter->hasNext()) {
        ps.addErrorObserver(*((ErrorObserver*)iter->next()));
    }
    delete iter;

      //-------------------------------------------------------/
     //- index templates and process top level xsl elements -/
    //-------------------------------------------------------/

    processTopLevel(&xslDocument, &ps);

      //----------------------------------------/
     //- Process root of XML source document -/
    //--------------------------------------/
    process(&xmlDocument, &xmlDocument, &ps);

    //-- return result Document
    return result;
} //-- process

#ifndef MOZ_XSL
/**
 * Processes the given XML Document using the given XSL document
 * and prints the results to the given ostream argument
**/
void XSLTProcessor::process
   (  Document& xmlDocument,
      Document& xslDocument,
      ostream& out,
      String& documentBase  )
{


    Document* result = new Document();

    //-- create a new ProcessorState
    ProcessorState ps(xslDocument, *result);
    ps.setDocumentBase(documentBase);

    //-- add error observers
    ListIterator* iter = errorObservers.iterator();
    while ( iter->hasNext()) {
        ps.addErrorObserver(*((ErrorObserver*)iter->next()));
    }
    delete iter;

      //-------------------------------------------------------/
     //- index templates and process top level xsl elements -/
    //-------------------------------------------------------/

    processTopLevel(&xslDocument, &ps);

      //----------------------------------------/
     //- Process root of XML source document -/
    //--------------------------------------/
    process(&xmlDocument, &xmlDocument, &ps);

    print(*result, ps.getOutputFormat(), out);

    delete result;
} //-- process


/**
 * Reads an XML Document from the given XML input stream.
 * The XSL Stylesheet is obtained from the XML Documents stylesheet PI.
 * If no Stylesheet is found, an empty document will be the result;
 * otherwise the XML Document is processed using the stylesheet.
 * The result tree is printed to the given ostream argument,
 * will not close the ostream argument
**/
void XSLTProcessor::process
   (istream& xmlInput, ostream& out, String& documentBase)
{

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
    process(*xmlDoc, *xslDoc, out, documentBase);
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
   (istream& xmlInput, istream& xslInput, ostream& out, String& documentBase)
{
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
    process(*xmlDoc, *xslDoc, out, documentBase);
    delete xmlDoc;
    delete xslDoc;
} //-- process

#endif // ifndef MOZ_XSL

  //-------------------/
 //- Private Methods -/
//-------------------/

void XSLTProcessor::bindVariable
    (String& name, ExprResult* value, MBool allowShadowing, ProcessorState* ps)
{
    NamedMap* varSet = (NamedMap*)ps->getVariableSetStack()->peek();
    //-- check for duplicate variable names
    VariableBinding* current = (VariableBinding*) varSet->get(name);
    VariableBinding* binding = 0;
    if (current) {
        binding = current;
        if (current->isShadowingAllowed() ) {
            current->setShadowValue(value);
        }
        else {
            //-- error cannot rebind variables
            String err("cannot rebind variables: ");
            err.append(name);
            err.append(" already exists in this scope.");
            notifyError(err);
        }
    }
    else {
        binding = new VariableBinding(name, value);
        varSet->put((const String&)name, binding);
    }
    if ( allowShadowing ) binding->allowShadowing();
    else binding->disallowShadowing();

} //-- bindVariable

/**
 * Returns the type of Element represented by the given name
 * @return the XSLType represented by the given element name
**/
short XSLTProcessor::getElementType(String& name, ProcessorState* ps) {


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
MBool XSLTProcessor::getText
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
void XSLTProcessor::notifyError(const char* errorMessage) {
    String err(errorMessage);
    notifyError(err, ErrorObserver::NORMAL);
} //-- notifyError

/**
 * Notifies all registered ErrorObservers of the given error
**/
void XSLTProcessor::notifyError(String& errorMessage) {
    notifyError(errorMessage, ErrorObserver::NORMAL);
} //-- notifyError

/**
 * Notifies all registered ErrorObservers of the given error
**/
void XSLTProcessor::notifyError(String& errorMessage, ErrorObserver::ErrorLevel level) {
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

void XSLTProcessor::process(Node* node, Node* context, ProcessorState* ps) {
    process(node, context, 0, ps);
} //-- process

void XSLTProcessor::process(Node* node, Node* context, String* mode, ProcessorState* ps) {
    if ( !node ) return;
    Element* xslTemplate = ps->findTemplate(node, context, mode);
    if (!xslTemplate) return;
    processTemplate(node, xslTemplate, ps);
} //-- process

void XSLTProcessor::processAction
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
                if ( selectAtt.length() == 0 ) selectAtt = "* | text()";
                pExpr = ps->getPatternExpr(selectAtt);
                ExprResult* exprResult = pExpr->evaluate(node, ps);
                NodeSet* nodeSet = 0;
                if ( exprResult->getResultType() == ExprResult::NODESET ) {
                    nodeSet = (NodeSet*)exprResult;

                    //-- make sure nodes are in DocumentOrder
                    ps->sortByDocumentOrder(nodeSet);

                    //-- look for xsl:sort elements
                    Node* child = actionElement->getFirstChild();
                    while (child) {
                        if (child->getNodeType() == Node::ELEMENT_NODE) {
                            String nodeName = child->getNodeName();
                            if (getElementType(nodeName, ps) == XSLType::SORT) {
                                NodeSorter::sort(nodeSet, (Element*)child, node, ps);
                                break;
                            }
                        }
                        child = child->getNextSibling();
                    }

                    //-- push nodeSet onto context stack
                    ps->getNodeSetStack()->push(nodeSet);
                    for (int i = 0; i < nodeSet->size(); i++) {
                        Element* xslTemplate = ps->findTemplate(nodeSet->get(i), node, mode);
                        if ( xslTemplate ) {
                            NamedMap* actualParams = processParameters(actionElement, node, ps);
                            processTemplate(nodeSet->get(i), xslTemplate, ps, actualParams);
                            delete actualParams;
                        }
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
                        delete dfrag;
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
                        //-- new code from OG
                        NamedMap* actualParams = processParameters(actionElement, node, ps);
                        processTemplate(node, xslTemplate, ps, actualParams);
                        delete actualParams;
                        //-- end new code OG
                        /*
                          //-- original code
                        NamedMap params;
                        params.setObjectDeletion(MB_TRUE);
                        Stack* bindings = ps->getVariableSetStack();
                        bindings->push(&params);
                        processTemplateParams(xslTemplate, node, ps);
                        processParameters(actionElement, node, ps);
                        processTemplate(node, xslTemplate, ps);
                        bindings->pop();
                        */
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
                    String nodeName = xslTemplate->getNodeName();
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
                delete dfrag;
                break;
            }
            //-- xsl:copy
            case XSLType::COPY:
                xslCopy(node, actionElement, ps);
                break;
            //-- xsl:copy-of
            case XSLType::COPY_OF:
            {
                String selectAtt = actionElement->getAttribute(SELECT_ATTR);
                Expr* expr = ps->getExpr(selectAtt);
                ExprResult* exprResult = expr->evaluate(node, ps);
                xslCopyOf(exprResult, ps);
                delete exprResult;
                break;

            }
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
#ifdef MOZ_XSL
                        // XXX (pvdb) Check if we need to set a new default namespace?
                        String nameSpaceURI;
                        ps->getNameSpaceURI(name, nameSpaceURI);
                        element = resultDoc->createElementNS(nameSpaceURI, name);
#else
                        element = resultDoc->createElement(name);
#endif
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

                    //-- make sure nodes are in DocumentOrder
                    ps->sortByDocumentOrder(nodeSet);

                    //-- look for xsl:sort elements
                    Node* child = actionElement->getFirstChild();
                    while (child) {
                        if (child->getNodeType() == Node::ELEMENT_NODE) {
                            String nodeName = child->getNodeName();
                            if (getElementType(nodeName, ps) == XSLType::SORT) {
                                NodeSorter::sort(nodeSet, (Element*)child, node, ps);
                                break;
                            }
                        }
                        child = child->getNextSibling();
                    }

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
                String selectAtt = actionElement->getAttribute(TEST_ATTR);
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
            //-- xsl:message
            case XSLType::MESSAGE :
            {
                String message;

                DocumentFragment* dfrag = resultDoc->createDocumentFragment();
                ps->getNodeStack()->push(dfrag);
                processTemplate(node, actionElement, ps);
                ps->getNodeStack()->pop();
                XMLDOMUtils::getNodeValue(dfrag, &message);
                delete dfrag;

                //-- we should add a MessageObserver class
                cout << "xsl:message - "<< message << endl;
                break;
            }
            //-- xsl:number
            case XSLType::NUMBER :
            {
                String result;
                Numbering::doNumbering(actionElement, result, node, ps);
                ps->addToResultTree(resultDoc->createTextNode(result));
                break;
            }
            //-- xsl:param
            case XSLType::PARAM:
                //-- ignore in this loop already processed
                break;
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
                    delete dfrag;
                }
                break;
            }
            case XSLType::SORT:
                //-- ignore in this loop
                break;
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
                String exprAtt = actionElement->getAttribute(EXPR_ATTR);
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
                String selectAtt = actionElement->getAttribute(SELECT_ATTR);

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
                bindVariable(name, exprResult, MB_FALSE, ps);
                break;
            }
            //-- literal
            default:
#ifdef MOZ_XSL
                // Find out if we have a new default namespace
                MBool newDefaultNS = MB_FALSE;
                String nsURI = actionElement->getAttribute(XMLUtils::XMLNS);
                if ( nsURI.length() != 0 ) {
                    // Set the default namespace
                    ps->setDefaultNameSpaceURI(nsURI);
                    newDefaultNS = MB_TRUE;
                }

                String nameSpaceURI;
                ps->getNameSpaceURI(nodeName, nameSpaceURI);
                Element* element = resultDoc->createElementNS(nameSpaceURI, nodeName);
#else
                Element* element = resultDoc->createElement(nodeName);
#endif

                ps->addToResultTree(element);
                ps->getNodeStack()->push(element);
                //-- handle attributes
                NamedNodeMap* atts = actionElement->getAttributes();
                if ( atts ) {
                    String xsltNameSpace = ps->getXSLNamespace();
                    NodeSet nonXSLAtts(atts->getLength());
                    //-- process special XSL attributes first
                    int i;
                    for (i = 0; i < atts->getLength(); i++ ) {
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
                int i;
                for ( i = 0; i < nl->getLength(); i++) {
                    processAction(node, nl->item(i),ps);
                }
                ps->getNodeStack()->pop();
#ifdef MOZ_XSL
                if ( newDefaultNS ) {
                    ps->getDefaultNSURIStack()->pop();
                }
#endif
                break;
        }
    }

    //cout << "XSLTProcessor#processAction [exit]\n";
} //-- processAction

/**
 * Processes the attribute sets specified in the names argument
**/
void XSLTProcessor::processAttributeSets
    (const String& names, Node* node, ProcessorState* ps)
{
    if (names.length() == 0) return;

    //-- split names
    Tokenizer tokenizer(names);
    String name;
    while ( tokenizer.hasMoreTokens() ) {
        tokenizer.nextToken(name);
        NodeSet* attSet = ps->getAttributeSet(name);
        if ( attSet ) {

            //-- issue: we still need to handle the following fix cleaner, since
            //-- attribute sets are merged, a different parent could exist
            //-- for different xsl:attribute nodes. I will probably create
            //-- an AttributeSet object, which will handle this case better. - Keith V.
            //-- Fix: handle use-attribute-set recursion - Marina M.
            if (attSet->size() > 0) {
                Element* parent = (Element*) ps->getParentNode(attSet->get(0));
                processAttributeSets(parent->getAttribute(USE_ATTRIBUTE_SETS_ATTR),node, ps);
            }
            //-- End Fix
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
void XSLTProcessor::processAttrValueTemplate
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
 * Processes the xsl:with-param child elements of the given xsl action.
 * A VariableBinding is created for each actual parameter, and
 * added to the result NamedMap. At this point, we do not care
 * whether the actual parameter matches a formal parameter of the template
 * or not.
 * @param xslAction the action node that takes parameters (xsl:call-template
 *   or xsl:apply-templates
 * @param context the current context node
 * @ps the current ProcessorState
 * @return a NamedMap of variable bindings
**/
NamedMap* XSLTProcessor::processParameters(Element* xslAction, Node* context, ProcessorState* ps)
{
    NamedMap* params = new NamedMap();

    if ( !xslAction ) {
      return params;
    }

    //-- handle xsl:with-param elements
    NodeList* nl = xslAction->getChildNodes();
    for (int i = 0; i < nl->getLength(); i++) {
        Node* tmpNode = nl->item(i);
        int nodeType = tmpNode->getNodeType();
        if ( nodeType == Node::ELEMENT_NODE ) {
            Element* action = (Element*)tmpNode;
            String actionName = action->getNodeName();
            short xslType = getElementType(actionName, ps);
            if ( xslType == XSLType::WITH_PARAM ) {
                String name = action->getAttribute(NAME_ATTR);
                if ( name.length() == 0 ) {
                    notifyError("missing required name attribute for xsl:with-param");
                }
                else {
                    ExprResult* exprResult = processVariable(context, action, ps);
                    if (params->get(name)) {
                        //-- error cannot rebind parameters
                        String err("value for parameter '");
                        err.append(name);
                        err.append("' specified more than once.");
                        notifyError(err);
                    }
                    else {
                        VariableBinding* binding = new VariableBinding(name, exprResult);
                        params->put((const String&)name, binding);
                    }
				}
            }
        }
    }
    return params;
} //-- processParameters

/**
 * Processes the specified template using the given context, ProcessorState, and actual
 * parameters.
 * @param xslTemplate the template to be processed
 * @ps the current ProcessorState
 * @param params a NamedMap of variable bindings that contain the actual parameters for
 *   the template. Parameters that do not match a formal parameter of the template (i.e.
 *   there is no corresponding xsl:param in the template definition) will be discarded.
**/
void XSLTProcessor::processTemplate(Node* node, Node* xslTemplate, ProcessorState* ps, NamedMap* params) {

    if ( !xslTemplate ) {
        //-- do default?
    }
    else {
        Stack* bindings = ps->getVariableSetStack();
        NamedMap localBindings;
        localBindings.setObjectDeletion(MB_TRUE);
        bindings->push(&localBindings);
        processTemplateParams(xslTemplate, node, ps, params);
        NodeList* nl = xslTemplate->getChildNodes();
        for (int i = 0; i < nl->getLength(); i++)
            processAction(node, nl->item(i), ps);
        bindings->pop();
    }
} //-- processTemplate

/**
 * Builds the initial bindings for the template. Formal parameters (xsl:param) that
 *   have a corresponding binding in actualParams are bound to the actual parameter value,
 *   otherwise to their default value. Actual parameters that do not match any formal
 *   parameter are discarded.
 * @param xslTemplate the template node
 * @param context the current context node
 * @param ps the current ProcessorState
 * @param actualParams a NamedMap of variable bindings that contains the actual parameters
**/
void XSLTProcessor::processTemplateParams
    (Node* xslTemplate, Node* context, ProcessorState* ps, NamedMap* actualParams)
{

    if ( xslTemplate ) {
        NodeList* nl = xslTemplate->getChildNodes();
        int i = 0;
        //-- handle params
        for (i = 0; i < nl->getLength(); i++) {
            Node* tmpNode = nl->item(i);
            int nodeType = tmpNode->getNodeType();
            if ( nodeType == Node::ELEMENT_NODE ) {
                Element* action = (Element*)tmpNode;
                String actionName = action->getNodeName();
                short xslType = getElementType(actionName, ps);
                if ( xslType == XSLType::PARAM ) {
                    String name = action->getAttribute(NAME_ATTR);
                    if ( name.length() == 0 ) {
                        notifyError("missing required name attribute for xsl:param");
                    }
                    else {
                        VariableBinding* binding = 0;
                        if (actualParams) {
                            binding = (VariableBinding*) actualParams->get((const String&)name);
                        }
                        if (binding) {
                            // the formal parameter has a corresponding actual parameter, use it
                            ExprResult* exprResult = binding->getValue();
                            bindVariable(name, exprResult, MB_FALSE, ps);
                        }
                        else {
                            // no actual param, use default
                            ExprResult* exprResult = processVariable(context, action, ps);
                            bindVariable(name, exprResult, MB_FALSE, ps);
                        }
                    }
                }
                else break;
            }
            else if (nodeType == Node::TEXT_NODE) {
                if (!XMLUtils::isWhitespace(((Text*)tmpNode)->getData())) break;
            }
            else break;
        }
    }
} //-- processTemplateParams


/**
 *  processes the xslVariable parameter as an xsl:variable using the given context,
 *  and ProcessorState.
 *  If the xslTemplate contains an "expr" attribute, the attribute is evaluated
 *  as an Expression and the ExprResult is returned. Otherwise the xslVariable is
 *  is processed as a template, and it's result is converted into an ExprResult
 *  @return an ExprResult
**/
ExprResult* XSLTProcessor::processVariable
        (Node* node, Element* xslVariable, ProcessorState* ps)
{

    if ( !xslVariable ) {
        return new StringResult("unable to process variable");
    }

    //-- check for select attribute
    Attr* attr = xslVariable->getAttributeNode(SELECT_ATTR);
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
} //-- processVariable

/**
 * Performs the xsl:copy action as specified in the XSL Working Draft
**/
void XSLTProcessor::xslCopy(Node* node, Element* action, ProcessorState* ps) {
    if ( !node ) return;

    Document* resultDoc = ps->getResultDocument();
    Node* copy = 0;
    switch ( node->getNodeType() ) {
        case Node::DOCUMENT_NODE:
            //-- just process children
            processTemplate(node, action, ps);
            break;
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)node;
            String nodeName = element->getNodeName();
#ifdef MOZ_XSL
            // Find out if we have a new default namespace
            MBool newDefaultNS = MB_FALSE;
            String nsURI = element->getAttribute(XMLUtils::XMLNS);
            if ( nsURI.length() != 0 ) {
                // Set the default namespace
                ps->setDefaultNameSpaceURI(nsURI);
                newDefaultNS = MB_TRUE;
            }

            String nameSpaceURI;
            ps->getNameSpaceURI(nodeName, nameSpaceURI);
            copy = resultDoc->createElementNS(nameSpaceURI, nodeName);
#else
            copy = resultDoc->createElement(nodeName);
#endif
            ps->addToResultTree(copy);
            ps->getNodeStack()->push(copy);

            //-- copy namespace attributes
            // * add later * - kv

            // fix: handle use-attribute-sets - Marina M.
            processAttributeSets(action->getAttribute(USE_ATTRIBUTE_SETS_ATTR), copy, ps);

            //-- process template
            processTemplate(node, action, ps);
            ps->getNodeStack()->pop();
#ifdef MOZ_XSL
            if ( newDefaultNS ) {
                ps->getDefaultNSURIStack()->pop();
            }
#endif
            return;
        }
        //-- just copy node, xsl:copy template does not get processed
        default:
            copy = XMLDOMUtils::copyNode(node, resultDoc);
            break;
    }
    if ( copy ) ps->addToResultTree(copy);
} //-- xslCopy

/**
 * Performs the xsl:copy-of action as specified in the XSL Working Draft
**/
void XSLTProcessor::xslCopyOf(ExprResult* exprResult, ProcessorState* ps) {

    if ( !exprResult ) return;

    Document* resultDoc = ps->getResultDocument();

    switch ( exprResult->getResultType() ) {
        case  ExprResult::NODESET:
        {
            NodeSet* nodes = (NodeSet*)exprResult;
            for (int i = 0; i < nodes->size();i++) {
                Node* node = nodes->get(i);
                //-- handle special case of copying another document into
                //-- the result tree
                if (node->getNodeType() == Node::DOCUMENT_NODE) {
                    Node* child = node->getFirstChild();
                    while (child) {
                        ps->addToResultTree(XMLDOMUtils::copyNode(child, resultDoc));
                        child = child->getNextSibling();
                    }
                }
                //-- otherwise just copy node
                else ps->addToResultTree(XMLDOMUtils::copyNode(node, resultDoc));
            }
            break;
        }
        default:
        {
            String value;
            exprResult->stringValue(value);
            ps->addToResultTree(resultDoc->createTextNode(value));
            break;
        }

    }
} //-- xslCopyOf

#ifdef MOZ_XSL
NS_IMETHODIMP
XSLTProcessor::TransformDocument(nsIDOMElement* aSourceDOM,
                               nsIDOMElement* aStyleDOM,
                               nsIDOMDocument* aOutputDoc,
                               nsIObserver* aObserver)
{
    nsCOMPtr<nsIDOMNode> sourceDOMNode;
    nsCOMPtr<nsIDOMDocument> sourceDOMDocument;
    nsCOMPtr<nsIDOMDocument> styleDOMDocument;

    aSourceDOM->GetOwnerDocument(getter_AddRefs(sourceDOMDocument));
    Document* sourceDocument = new Document(sourceDOMDocument);
    Node sourceNode(aSourceDOM, sourceDocument);

    aStyleDOM->GetOwnerDocument(getter_AddRefs(styleDOMDocument));
    Document* xslDocument = new Document(styleDOMDocument);
    Element styleElement(aStyleDOM, xslDocument);

    Document* resultDocument = new Document(aOutputDoc);

    //-- create a new ProcessorState
    ProcessorState* ps = new ProcessorState(*xslDocument, *resultDocument);
    ps->initialize(&styleElement);

    nsCOMPtr<nsIDocument> sourceNsDocument = do_QueryInterface(sourceDOMDocument);
    nsCOMPtr<nsIURI> docURL;

    sourceNsDocument->GetBaseURL(*getter_AddRefs(docURL));
    if (docURL) {
        char* urlString;

        docURL->GetSpec(&urlString);
        String documentBase(urlString);
//cout << "documentbase: " << documentBase << endl;
        ps->setDocumentBase(documentBase);
        nsCRT::free(urlString);
    }
    else
        ps->setDocumentBase("");

    //-- add error observers

      //------------------------------------------------------/
     //- index templates and process top level xsl elements -/
    //------------------------------------------------------/

    processTopLevel(&styleElement, ps);

      //---------------------------------------/
     //- Process root of XML source document -/
    //---------------------------------------/
    process(&sourceNode, &sourceNode, ps);

    Element* theResultElement = resultDocument->getDocumentElement();

    nsresult res = NS_OK;
    nsAutoString XSLTransformer; XSLTransformer.AssignWithConversion("XSLTransformer");
    nsCOMPtr<nsIObserverService> anObserverService = do_GetService(NS_OBSERVERSERVICE_PROGID, &res);
    if (NS_SUCCEEDED(res)) {
        anObserverService->AddObserver(aObserver, XSLTransformer.GetUnicode());
        anObserverService->Notify(theResultElement->getNSObj(), XSLTransformer.GetUnicode(), nsnull);
    }

    /* XXX HACK (pvdb)
       This is leaking! But otherwise we crash on Mac (and others?).
       This workaround can be removed once we figure out what's going on.
    */
    delete ps;
//    delete resultDocument;
//    delete xslDocument;
    delete sourceDocument;
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



