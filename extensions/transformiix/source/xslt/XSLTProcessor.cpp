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
#include "VariableBinding.h"
#include "XMLUtils.h"
#include "XMLDOMUtils.h"
#include "txNodeSorter.h"
#include "Numbering.h"
#include "Tokenizer.h"
#include "URIUtils.h"
#include "txAtoms.h"
#include "txRtfHandler.h"
#ifndef TX_EXE
#include "nsIObserverService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsNetCID.h"
#include "nsIDOMClassInfo.h"
#include "nsIConsoleService.h"
#include "nsIScriptLoader.h"
#else
#include "TxLog.h"
#include "txHTMLOutput.h"
#include "txTextOutput.h"
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
const String XSLTProcessor::NON_TEXT_TEMPLATE_WARNING =
"templates for the following element are not allowed to generate non character data: ";

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


    //-- create XSL element types
    xslTypes.setObjectDeletion(MB_TRUE);
    xslTypes.put(APPLY_IMPORTS,   new XSLType(XSLType::APPLY_IMPORTS));
    xslTypes.put(APPLY_TEMPLATES, new XSLType(XSLType::APPLY_TEMPLATES));
    xslTypes.put(ATTRIBUTE,       new XSLType(XSLType::ATTRIBUTE));
    xslTypes.put(ATTRIBUTE_SET,   new XSLType(XSLType::ATTRIBUTE_SET));
    xslTypes.put(CALL_TEMPLATE,   new XSLType(XSLType::CALL_TEMPLATE));
    xslTypes.put(CHOOSE,          new XSLType(XSLType::CHOOSE));
    xslTypes.put(COMMENT,         new XSLType(XSLType::COMMENT));
    xslTypes.put(COPY,            new XSLType(XSLType::COPY));
    xslTypes.put(COPY_OF,         new XSLType(XSLType::COPY_OF));
    xslTypes.put(DECIMAL_FORMAT,  new XSLType(XSLType::DECIMAL_FORMAT));
    xslTypes.put(ELEMENT,         new XSLType(XSLType::ELEMENT));
    xslTypes.put(FOR_EACH,        new XSLType(XSLType::FOR_EACH));
    xslTypes.put(IF,              new XSLType(XSLType::IF));
    xslTypes.put(IMPORT,          new XSLType(XSLType::IMPORT));
    xslTypes.put(INCLUDE,         new XSLType(XSLType::INCLUDE));
    xslTypes.put(KEY,             new XSLType(XSLType::KEY));
    xslTypes.put(MESSAGE,         new XSLType(XSLType::MESSAGE));
    xslTypes.put(NUMBER,          new XSLType(XSLType::NUMBER));
    xslTypes.put(OTHERWISE,       new XSLType(XSLType::OTHERWISE));
    xslTypes.put(OUTPUT,          new XSLType(XSLType::OUTPUT));
    xslTypes.put(PARAM,           new XSLType(XSLType::PARAM));
    xslTypes.put(PROC_INST,       new XSLType(XSLType::PROC_INST));
    xslTypes.put(PRESERVE_SPACE,  new XSLType(XSLType::PRESERVE_SPACE));
    xslTypes.put(STRIP_SPACE,     new XSLType(XSLType::STRIP_SPACE));
    xslTypes.put(SORT,            new XSLType(XSLType::SORT));
    xslTypes.put(TEMPLATE,        new XSLType(XSLType::TEMPLATE));
    xslTypes.put(TEXT,            new XSLType(XSLType::TEXT));
    xslTypes.put(VALUE_OF,        new XSLType(XSLType::VALUE_OF));
    xslTypes.put(VARIABLE,        new XSLType(XSLType::VARIABLE));
    xslTypes.put(WHEN,            new XSLType(XSLType::WHEN));
    xslTypes.put(WITH_PARAM,      new XSLType(XSLType::WITH_PARAM));

    // Create default expressions
    ExprParser parser;
    String node("node()");
    mNodeExpr = parser.createExpr(node);

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

    PRInt32 size = data.length();
    NamedMap bufferMap;
    bufferMap.put("type", &type);
    bufferMap.put("href", &href);
    int ccount = 0;
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
        notifyError(err, ErrorObserver::FATAL);
        return 0;
    }
    //-- Read in XSL document
    Document* xslDoc = xmlParser.parse(xslInput, xslFilename);
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
Document* XSLTProcessor::process(istream& xmlInput, String& xmlFilename) {
    //-- read in XML Document
    XMLParser xmlParser;
    Document* xmlDoc = xmlParser.parse(xmlInput, xmlFilename);
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
    istream* xslInput = URIUtils::getInputStream(href,errMsg);
    Document* xslDoc = 0;
    if ( xslInput ) {
        xslDoc = xmlParser.parse(*xslInput, href);
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

// XXX
// XXX Standalone only code. This should move to txStandaloneXSLTProcessor
// XXX END

#endif

/**
 * Processes the Top level elements for an XSL stylesheet
**/
void XSLTProcessor::processStylesheet(Document* aSource,
                                      Document* aStylesheet,
                                      ListIterator* aImportFrame,
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
        processTopLevel(aSource, elem, aImportFrame, aPs);
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

/**
 * Processes the Top level elements for an XSL stylesheet
**/
void XSLTProcessor::processTopLevel(Document* aSource,
                                    Element* aStylesheet,
                                    ListIterator* importFrame,
                                    ProcessorState* aPs)
{
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

    NS_ASSERTION(aSource, "processTopLevel called without source document");

    MBool importsDone = MB_FALSE;
    Node* node = aStylesheet->getFirstChild();
    while (node && !importsDone) {
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            Element* element = (Element*)node;
            switch (getElementType(element, aPs)) {
                case XSLType::IMPORT :
                {
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
                    processInclude(href, aSource, importFrame, aPs);

                    // Restore iterator to initial position
                    importFrame->previous();

                    break;
                }
                default:
                    importsDone = MB_TRUE;
                    break;
            }
        }
        if (!importsDone)
            node = node->getNextSibling();
    }

    while (node) {
        if (node->getNodeType() == Node::ELEMENT_NODE) {
            Element* element = (Element*)node;
            switch (getElementType(element, aPs)) {
                case XSLType::ATTRIBUTE_SET:
                    aPs->addAttributeSet(element, currentFrame);
                    break;
                case XSLType::DECIMAL_FORMAT :
                {
                    if (!aPs->addDecimalFormat(element)) {
                        // Add error to ErrorObserver
                        String fName;
                        element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                         fName);
                        String err("unable to add ");
                        if (fName.isEmpty())
                            err.append("default");
                        else {
                            err.append("\"");
                            err.append(fName);
                            err.append("\"");
                        }
                        err.append(" decimal format for xsl:decimal-format");
                        notifyError(err);
                    }
                    break;
                }
                case XSLType::PARAM :
                {
                    String name;
                    element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                     name);
                    if (name.isEmpty()) {
                        notifyError("missing required name attribute for xsl:param");
                        break;
                    }

                    ExprResult* exprResult
                        = processVariable(aSource, element, aPs);
                    bindVariable(name, exprResult, MB_TRUE, aPs);
                    break;
                }
                case XSLType::IMPORT :
                {
                    notifyError("xsl:import only allowed at top of stylesheet");
                    break;
                }
                case XSLType::INCLUDE :
                {
                    String hrefAttr, href;
                    element->getAttr(txXSLTAtoms::href, kNameSpaceID_None,
                                     hrefAttr);
                    URIUtils::resolveHref(hrefAttr, element->getBaseURI(),
                                          href);

                    processInclude(href, aSource, importFrame, aPs);
                    break;
                }
                case XSLType::KEY :
                {
                    if (!aPs->addKey(element)) {
                        String name;
                        element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                         name);
                        String err("error adding key '");
                        err.append(name);
                        err.append("'");
                        notifyError(err);
                    }
                    break;
                    
                }
                case XSLType::OUTPUT :
                {
                    txOutputFormat& format = currentFrame->mOutputFormat;
                    String attValue;

                    if (element->getAttr(txXSLTAtoms::method, kNameSpaceID_None,
                                         attValue)) {
                        if (attValue.isEqual("html"))
                            format.mMethod = eHTMLOutput;
                        else if (attValue.isEqual("text"))
                            format.mMethod = eTextOutput;
                        else
                            format.mMethod = eXMLOutput;
                    }

                    if (element->getAttr(txXSLTAtoms::version, kNameSpaceID_None,
                                         attValue))
                        format.mVersion = attValue;

                    if (element->getAttr(txXSLTAtoms::encoding, kNameSpaceID_None,
                                         attValue))
                        format.mEncoding = attValue;

                    if (element->getAttr(txXSLTAtoms::omitXmlDeclaration,
                                         kNameSpaceID_None, attValue))
                        format.mOmitXMLDeclaration = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;

                    if (element->getAttr(txXSLTAtoms::standalone, kNameSpaceID_None,
                                         attValue))
                        format.mStandalone = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;

                    if (element->getAttr(txXSLTAtoms::doctypePublic,
                                         kNameSpaceID_None, attValue))
                        format.mPublicId = attValue;

                    if (element->getAttr(txXSLTAtoms::doctypeSystem,
                                         kNameSpaceID_None, attValue))
                        format.mSystemId = attValue;

                    if (element->getAttr(txXSLTAtoms::cdataSectionElements,
                                         kNameSpaceID_None, attValue)) {
                        txTokenizer tokens(attValue);
                        String token;
                        while (tokens.hasMoreTokens()) {
                            tokens.nextToken(token);
                            if (!XMLUtils::isValidQName(token))
                                break;

                            String namePart;
                            XMLUtils::getPrefix(token, namePart);
                            txAtom* nameAtom = TX_GET_ATOM(namePart);
                            PRInt32 nsID = element->lookupNamespaceID(nameAtom);
                            TX_IF_RELEASE_ATOM(nameAtom);
                            if (nsID == kNameSpaceID_Unknown)
                                // XXX ErrorReport: unknown prefix
                                break;
                            XMLUtils::getLocalPart(token, namePart);
                            nameAtom = TX_GET_ATOM(namePart);
                            if (!nameAtom)
                                // XXX ErrorReport: out of memory
                                break;
                            txExpandedName* qname = new txExpandedName(nsID, nameAtom);
                            TX_RELEASE_ATOM(nameAtom);
                            if (!qname)
                                // XXX ErrorReport: out of memory
                                break;
                            format.mCDATASectionElements.add(qname);
                        }
                    }

                    if (element->getAttr(txXSLTAtoms::indent, kNameSpaceID_None,
                                         attValue))
                        format.mIndent = attValue.isEqual(YES_VALUE) ? eTrue : eFalse;

                    if (element->getAttr(txXSLTAtoms::mediaType, kNameSpaceID_None,
                                         attValue))
                        format.mMediaType = attValue;
                    break;
                }
                case XSLType::TEMPLATE :
                    aPs->addTemplate(element, currentFrame);
                    break;
                case XSLType::VARIABLE :
                {
                    String name;
                    element->getAttr(txXSLTAtoms::name, kNameSpaceID_None,
                                     name);
                    if (name.isEmpty()) {
                        notifyError("missing required name attribute for xsl:variable");
                        break;
                    }
                    ExprResult* exprResult = processVariable(aSource, element, aPs);
                    bindVariable(name, exprResult, MB_FALSE, aPs);
                    break;
                }
                case XSLType::PRESERVE_SPACE :
                {
                    String elements;
                    if (!element->getAttr(txXSLTAtoms::elements,
                                          kNameSpaceID_None, elements)) {
                        //-- add error to ErrorObserver
                        String err("missing required 'elements' attribute for ");
                        err.append("xsl:preserve-space");
                        notifyError(err);
                    }
                    else {
                        aPs->shouldStripSpace(elements,
                                              MB_FALSE,
                                              currentFrame);
                    }
                    break;
                }
                case XSLType::STRIP_SPACE :
                {
                    String elements;
                    if (!element->getAttr(txXSLTAtoms::elements,
                                          kNameSpaceID_None, elements)) {
                        //-- add error to ErrorObserver
                        String err("missing required 'elements' attribute for ");
                        err.append("xsl:strip-space");
                        notifyError(err);
                    }
                    else {
                        aPs->shouldStripSpace(elements,
                                              MB_TRUE,
                                              currentFrame);
                    }
                    break;
                }
                default:
                {
                    // unknown
                    break;
                }
            }
        }
        node = node->getNextSibling();
    }
}

/*
 * Processes an include or import stylesheet
 * @param aHref    URI of stylesheet to process
 * @param aSource  source document
 * @param aImportFrame current importFrame iterator
 * @param aPs      current ProcessorState
 */
void XSLTProcessor::processInclude(String& aHref,
                                   Document* aSource,
                                   ListIterator* aImportFrame,
                                   ProcessorState* aPs)
{
    // make sure the include isn't included yet
    StackIterator* iter = aPs->getEnteredStylesheets()->iterator();
    if (!iter) {
        // XXX report out of memory
        return;
    }
    
    while (iter->hasNext()) {
        if (((String*)iter->next())->isEqual(aHref)) {
            String err("Stylesheet includes itself. URI: ");
            err.append(aHref);
            notifyError(err);
            delete iter;
            return;
        }
    }
    aPs->getEnteredStylesheets()->push(&aHref);
    delete iter;

    // Load XSL document
    Node* stylesheet = aPs->retrieveDocument(aHref, NULL_STRING);
    if (!stylesheet) {
        String err("Unable to load included stylesheet ");
        err.append(aHref);
        notifyError(err);
        aPs->getEnteredStylesheets()->pop();
        return;
    }

    switch(stylesheet->getNodeType()) {
        case Node::DOCUMENT_NODE :
            processStylesheet(aSource,
                              (Document*)stylesheet,
                              aImportFrame,
                              aPs);
            break;
        case Node::ELEMENT_NODE :
            processTopLevel(aSource, (Element*)stylesheet, aImportFrame, aPs);
            break;
        default:
            // This should never happen
            String err("Unsupported fragment identifier");
            notifyError(err);
            break;
    }

    aPs->getEnteredStylesheets()->pop();
}

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
    
        NodeSet nodeSet(&aXMLDocument);
        ps.pushCurrentNode(&aXMLDocument);
        ps.getNodeSetStack()->push(&nodeSet);
    
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

        NodeSet nodeSet(&aXMLDocument);
        ps.pushCurrentNode(&aXMLDocument);
        ps.getNodeSetStack()->push(&nodeSet);

        // Index templates and process top level xsl elements
        txListIterator importFrame(ps.getImportFrames());
        importFrame.addAfter(new ProcessorState::ImportFrame(0));
        if (!importFrame.next())
            // XXX ErrorReport: out of memory
            return;
        
        if (stylesheetElem)
            processTopLevel(&aXMLDocument, stylesheetElem, &importFrame, &ps);
        else
            processStylesheet(&aXMLDocument, stylesheetDoc, &importFrame, &ps);

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
        notifyError(err, ErrorObserver::FATAL);
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
        notifyError(err, ErrorObserver::FATAL);
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
            notifyError(err, ErrorObserver::FATAL);
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
        notifyError(err, ErrorObserver::FATAL);
        return;
    }
    //-- read in XSL Document
    Document* xslDoc = xmlParser.parse(xslInput, xslFilename);
    if (!xslDoc) {
        String err("error reading XSL stylesheet document: ");
        err.append(xmlParser.getErrorString());
        notifyError(err, ErrorObserver::FATAL);
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
short XSLTProcessor::getElementType(Element* aElement, ProcessorState* aPs)
{
    PRInt32 nsID = aElement->getNamespaceID();
    if (nsID != kNameSpaceID_XSLT)
        return XSLType::LITERAL;

    txAtom* nodeName;
    if (!aElement->getLocalName(&nodeName) || !nodeName)
        return 0;

    String name;
    TX_GET_ATOM_STRING(nodeName, name);
    TX_RELEASE_ATOM(nodeName);

    XSLType* xslType = (XSLType*)xslTypes.get(name);
    if (!xslType)
        // ErrorReport: unknown element in the XSLT namespace
        return 0;
    return xslType->type;
}

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

void XSLTProcessor::process(Node* node,
                            const String& mode,
                            ProcessorState* ps) {
    if (!node)
        return;

    ProcessorState::ImportFrame *frame;
    Node* xslTemplate = ps->findTemplate(node, mode, &frame);
    processMatchedTemplate(xslTemplate, node, 0, NULL_STRING, frame, ps);
} //-- process

void XSLTProcessor::processAction(Node* aNode,
                                  Node* aXSLTAction,
                                  ProcessorState* aPs)
{
    NS_ASSERTION(aXSLTAction, "We need an action to process.");
    if (!aXSLTAction)
        return;

    Document* resultDoc = aPs->getResultDocument();

    short nodeType = aXSLTAction->getNodeType();

    // Handle text nodes
    if (nodeType == Node::TEXT_NODE ||
        nodeType == Node::CDATA_SECTION_NODE) {
        const String& textValue = aXSLTAction->getNodeValue();
        if (!aPs->isXSLStripSpaceAllowed(aXSLTAction) ||
            !XMLUtils::shouldStripTextnode(textValue)) {
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->characters(textValue);
        }
        return;
    }
    // Handle element nodes
    if (nodeType == Node::ELEMENT_NODE) {
        Expr* expr = 0;
        Element* actionElement = (Element*)aXSLTAction;
        switch (getElementType(actionElement, aPs)) {
            // xsl:apply-imports
            case XSLType::APPLY_IMPORTS:
            {
                ProcessorState::TemplateRule* curr;
                Node* xslTemplate;
                ProcessorState::ImportFrame *frame;

                curr = aPs->getCurrentTemplateRule();
                if (!curr) {
                    String err("apply-imports not allowed here");
                    aPs->recieveError(err);
                    break;
                }

                xslTemplate = aPs->findTemplate(aNode, *curr->mMode,
                                                curr->mFrame, &frame);
                processMatchedTemplate(xslTemplate, aNode, curr->mParams,
                                       *curr->mMode, frame, aPs);

                break;
            }
            // xsl:apply-templates
            case XSLType::APPLY_TEMPLATES:
            {
                if (actionElement->hasAttr(txXSLTAtoms::select,
                                           kNameSpaceID_None))
                    expr = aPs->getExpr(actionElement,
                                        ProcessorState::SelectAttr);
                else
                    expr = mNodeExpr;

                if (!expr)
                    break;

                ExprResult* exprResult = expr->evaluate(aNode, aPs);
                if (!exprResult)
                    break;

                if (exprResult->getResultType() == ExprResult::NODESET) {
                    NodeSet* nodeSet = (NodeSet*)exprResult;

                    //-- push nodeSet onto context stack
                    aPs->getNodeSetStack()->push(nodeSet);

                    // Look for xsl:sort elements
                    txNodeSorter sorter(aPs);
                    Node* child = actionElement->getFirstChild();
                    while (child) {
                        if ((child->getNodeType() == Node::ELEMENT_NODE) &&
                            (getElementType((Element*)child, aPs) == XSLType::SORT)) {
                            sorter.addSortElement((Element*)child, aNode);
                        }
                        child = child->getNextSibling();
                    }
                    sorter.sortNodeSet(nodeSet);

                    // Process xsl:with-param elements
                    NamedMap* actualParams = processParameters(actionElement, aNode, aPs);

                    String mode;
                    actionElement->getAttr(txXSLTAtoms::mode,
                                           kNameSpaceID_None, mode);
                    for (int i = 0; i < nodeSet->size(); i++) {
                        ProcessorState::ImportFrame *frame;
                        Node* currNode = nodeSet->get(i);
                        Node* xslTemplate;
                        xslTemplate = aPs->findTemplate(currNode, mode, &frame);
                        processMatchedTemplate(xslTemplate, currNode,
                                               actualParams, mode, frame, aPs);
                    }

                    //-- remove nodeSet from context stack
                    aPs->getNodeSetStack()->pop();

                    delete actualParams;
                }
                else {
                    notifyError("error processing apply-templates");
                }
                //-- clean up
                delete exprResult;
                break;
            }
            // xsl:attribute
            case XSLType::ATTRIBUTE:
            {
                String nameAttr;
                if (!actionElement->getAttr(txXSLTAtoms::name,
                                            kNameSpaceID_None, nameAttr)) {
                    notifyError("missing required name attribute for xsl:attribute");
                    break;
                }

                // Process name as an AttributeValueTemplate
                String name;
                aPs->processAttrValueTemplate(nameAttr, aNode, name);

                // Check name validity (must be valid QName and not xmlns)
                if (!XMLUtils::isValidQName(name)) {
                    String err("error processing xsl:attribute, ");
                    err.append(name);
                    err.append(" is not a valid QName.");
                    notifyError(err);
                    break;
                }

                txAtom* nameAtom = TX_GET_ATOM(name);
                if (nameAtom == txXMLAtoms::xmlns) {
                    TX_RELEASE_ATOM(nameAtom);
                    String err("error processing xsl:attribute, name is xmlns.");
                    notifyError(err);
                    break;
                }
                TX_IF_RELEASE_ATOM(nameAtom);

                // Determine namespace URI from the namespace attribute or
                // from the prefix of the name (using the xslt action element).
                String resultNs;
                PRInt32 resultNsID = kNameSpaceID_None;
                if (actionElement->getAttr(txXSLTAtoms::_namespace, kNameSpaceID_None,
                                           resultNs)) {
                    String nsURI;
                    aPs->processAttrValueTemplate(resultNs, aNode, nsURI);
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
                if (resultNsID == kNameSpaceID_Unknown)
                    break;

                // Compute value
                String value;
                processChildrenAsValue(aNode, actionElement, aPs, MB_TRUE, value);

                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->attribute(name, resultNsID, value);
                break;
            }
            // call-template
            case XSLType::CALL_TEMPLATE:
            {
                String templateName;
                if (actionElement->getAttr(txXSLTAtoms::name,
                                           kNameSpaceID_None, templateName)) {
                    Element* xslTemplate = aPs->getNamedTemplate(templateName);
                    if ( xslTemplate ) {
                        NamedMap* actualParams = processParameters(actionElement, aNode, aPs);
                        processTemplate(aNode, xslTemplate, aPs, actualParams);
                        delete actualParams;
                    }
                }
                else {
                    notifyError("missing required name attribute for xsl:call-template");
                }
                break;
            }
            // xsl:choose
            case XSLType::CHOOSE:
            {
                Node* tmp = actionElement->getFirstChild();
                MBool caseFound = MB_FALSE;
                Element* xslTemplate;
                while (!caseFound && tmp) {
                    if (tmp->getNodeType() == Node::ELEMENT_NODE) {
                        xslTemplate = (Element*)tmp;
                        switch (getElementType(xslTemplate, aPs)) {
                            case XSLType::WHEN :
                            {
                                expr = aPs->getExpr(xslTemplate,
                                                    ProcessorState::TestAttr);
                                if (!expr)
                                    break;

                                ExprResult* result = expr->evaluate(aNode, aPs);
                                if (result && result->booleanValue()) {
                                    processChildren(aNode, xslTemplate, aPs);
                                    caseFound = MB_TRUE;
                                }
                                delete result;
                                break;
                            }
                            case XSLType::OTHERWISE:
                                processChildren(aNode, xslTemplate, aPs);
                                caseFound = MB_TRUE;
                                break;
                            default: //-- invalid xsl:choose child
                                break;
                        }
                    }
                    tmp = tmp->getNextSibling();
                } //-- end for-each child of xsl:choose
                break;
            }
            // xsl:comment
            case XSLType::COMMENT:
            {
                String value;
                processChildrenAsValue(aNode, actionElement, aPs, MB_TRUE, value);
                PRInt32 pos = 0;
                PRInt32 length = value.length();
                while ((pos = value.indexOf('-', pos)) != NOT_FOUND) {
                    ++pos;
                    if ((pos == length) || (value.charAt(pos) == '-'))
                        value.insert(pos++, ' ');
                }
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->comment(value);
                break;
            }
            // xsl:copy
            case XSLType::COPY:
            {
                xslCopy(aNode, actionElement, aPs);
                break;
            }
            // xsl:copy-of
            case XSLType::COPY_OF:
            {
                expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
                if (!expr)
                    break;

                ExprResult* exprResult = expr->evaluate(aNode, aPs);
                xslCopyOf(exprResult, aPs);
                delete exprResult;
                break;

            }
            // xsl:element
            case XSLType::ELEMENT:
            {
                String nameAttr;
                if (!actionElement->getAttr(txXSLTAtoms::name,
                                            kNameSpaceID_None, nameAttr)) {
                    notifyError("missing required name attribute for xsl:element");
                    break;
                }

                // Process name as an AttributeValueTemplate
                String name;
                aPs->processAttrValueTemplate(nameAttr, aNode, name);

                // Check name validity (must be valid QName and not xmlns)
                if (!XMLUtils::isValidQName(name)) {
                    String err("error processing xsl:element, '");
                    err.append(name);
                    err.append("' is not a valid QName.");
                    notifyError(err);
                    // XXX We should processChildren without creating attributes or
                    //     namespace nodes.
                    break;
                }

                // Determine namespace URI from the namespace attribute or
                // from the prefix of the name (using the xslt action element).
                String resultNs;
                PRInt32 resultNsID;
                if (actionElement->getAttr(txXSLTAtoms::_namespace, kNameSpaceID_None, resultNs)) {
                    String nsURI;
                    aPs->processAttrValueTemplate(resultNs, aNode, nsURI);
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
                    notifyError(err);
                    // XXX We should processChildren without creating attributes or
                    //     namespace nodes.
                    break;
                }

                startElement(aPs, name, resultNsID);
                processAttributeSets(actionElement, aNode, aPs);
                processChildren(aNode, actionElement, aPs);
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->endElement(name, resultNsID);
                break;
            }
            // xsl:for-each
            case XSLType::FOR_EACH:
            {
                expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
                if (!expr)
                    break;

                ExprResult* exprResult = expr->evaluate(aNode, aPs);
                if (!exprResult)
                    break;

                if (exprResult->getResultType() == ExprResult::NODESET) {
                    NodeSet* nodeSet = (NodeSet*)exprResult;

                    //-- push nodeSet onto context stack
                    aPs->getNodeSetStack()->push(nodeSet);

                    // Look for xsl:sort elements
                    txNodeSorter sorter(aPs);
                    Node* child = actionElement->getFirstChild();
                    while (child) {
                        int nodeType = child->getNodeType();
                        if (nodeType == Node::ELEMENT_NODE) {
                            if (getElementType((Element*)child, aPs) == XSLType::SORT) {
                                sorter.addSortElement((Element*)child, aNode);
                            }
                            else {
                                // xsl:sort must occur first
                                break;
                            }
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

                    for (int i = 0; i < nodeSet->size(); i++) {
                        Node* currNode = nodeSet->get(i);
                        aPs->pushCurrentNode(currNode);
                        processChildren(currNode, actionElement, aPs);
                        aPs->popCurrentNode();
                    }

                    aPs->setCurrentTemplateRule(oldTemplate);

                    // Remove nodeSet from context stack
                    aPs->getNodeSetStack()->pop();
                }
                else {
                    notifyError("error processing for-each");
                }
                //-- clean up exprResult
                delete exprResult;
                break;
            }
            // xsl:if
            case XSLType::IF:
            {
                expr = aPs->getExpr(actionElement, ProcessorState::TestAttr);
                if (!expr)
                    break;

                ExprResult* exprResult = expr->evaluate(aNode, aPs);
                if (!exprResult)
                    break;

                if ( exprResult->booleanValue() ) {
                    processChildren(aNode, actionElement, aPs);
                }
                delete exprResult;
                break;
            }
            // xsl:message
            case XSLType::MESSAGE:
            {
                String message;
                processChildrenAsValue(aNode, actionElement, aPs, MB_FALSE, message);
                // We should add a MessageObserver class
#ifdef TX_EXE
                cout << "xsl:message - "<< message << endl;
#else
                nsresult rv;
                nsCOMPtr<nsIConsoleService> consoleSvc = 
                  do_GetService("@mozilla.org/consoleservice;1", &rv);
                NS_ASSERTION(NS_SUCCEEDED(rv), "xsl:message couldn't get console service");
                if (consoleSvc) {
                    nsAutoString logString(NS_LITERAL_STRING("xsl:message - "));
                    logString.Append(message.getConstNSString());
                    rv = consoleSvc->LogStringMessage(logString.get());
                    NS_ASSERTION(NS_SUCCEEDED(rv), "xsl:message couldn't log");
                }
#endif
                break;
            }
            // xsl:number
            case XSLType::NUMBER:
            {
                String result;
                Numbering::doNumbering(actionElement, result, aNode, aPs);
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->characters(result);
                break;
            }
            // xsl:param
            case XSLType::PARAM:
            {
                // Ignore in this loop (already processed)
                break;
            }
            // xsl:processing-instruction
            case XSLType::PROC_INST:
            {
                String nameAttr;
                if (!actionElement->getAttr(txXSLTAtoms::name,
                                            kNameSpaceID_None, nameAttr)) {
                    String err("missing required name attribute for xsl:");
                    err.append(PROC_INST);
                    notifyError(err);
                    break;
                }

                // Process name as an AttributeValueTemplate
                String name;
                aPs->processAttrValueTemplate(nameAttr, aNode, name);

                // Check name validity (must be valid NCName and a PITarget)
                // XXX Need to check for NCName and PITarget
                if (!XMLUtils::isValidQName(name)) {
                    String err("error processing xsl:");
                    err.append(PROC_INST);
                    err.append(", '");
                    err.append(name);
                    err.append("' is not a valid QName.");
                    notifyError(err);
                }

                // Compute value
                String value;
                processChildrenAsValue(aNode, actionElement, aPs, MB_TRUE, value);
                XMLUtils::normalizePIValue(value);
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->processingInstruction(name, value);
                break;
            }
            // xsl:sort
            case XSLType::SORT:
            {
                // Ignore in this loop
                break;
            }
            // xsl:text
            case XSLType::TEXT:
            {
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
                break;
            }
            //-- xsl:value-of
            case XSLType::VALUE_OF :
            {
                expr = aPs->getExpr(actionElement, ProcessorState::SelectAttr);
                if (!expr)
                    break;

                ExprResult* exprResult = expr->evaluate(aNode, aPs);
                String value;
                if (!exprResult) {
                    notifyError("null ExprResult");
                    break;
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
                break;
            }
            // xsl:variable
            case XSLType::VARIABLE:
            {
                String name;
                if (!actionElement->getAttr(txXSLTAtoms::name,
                                            kNameSpaceID_None, name)) {
                    notifyError("missing required name attribute for xsl:variable");
                    break;
                }
                ExprResult* exprResult = processVariable(aNode, actionElement, aPs);
                bindVariable(name, exprResult, MB_FALSE, aPs);
                break;
            }
            // Literal result element
            default:
            {
                // XXX TODO Check for excluded namespaces and aliased namespaces (element and attributes) 
                PRInt32 nsID = aXSLTAction->getNamespaceID();
                const String& nodeName = aXSLTAction->getNodeName();
                startElement(aPs, nodeName, nsID);

                processAttributeSets(actionElement, aNode, aPs);

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
                        aPs->processAttrValueTemplate(attr->getValue(), aNode, value);
                        NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                        mResultHandler->attribute(attr->getName(), attr->getNamespaceID(), value);
                    }
                }

                // Process children
                processChildren(aNode, actionElement, aPs);
                NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
                mResultHandler->endElement(nodeName, nsID);
                break;
            }
        }
    }
}

/**
 * Processes the attribute sets specified in the use-attribute-sets attribute
 * of the element specified in aElement
**/
void XSLTProcessor::processAttributeSets(Element* aElement, Node* aNode, ProcessorState* aPs)
{
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
    String name;
    while (tokenizer.hasMoreTokens()) {
        tokenizer.nextToken(name);
        StackIterator *attributeSets = mAttributeSetStack.iterator();
        NS_ASSERTION(attributeSets, "Out of memory");
        if (!attributeSets)
            return;
        while (attributeSets->hasNext()) {
            String* test = (String*)attributeSets->next();
            if (test->isEqual(name))
                return;
        }
        delete attributeSets;

        NodeSet* attSet = aPs->getAttributeSet(name);
        if (attSet) {
            int i;
            //-- issue: we still need to handle the following fix cleaner, since
            //-- attribute sets are merged, a different parent could exist
            //-- for different xsl:attribute nodes. I will probably create
            //-- an AttributeSet object, which will handle this case better. - Keith V.
            if (attSet->size() > 0) {
                mAttributeSetStack.push(&name);
                Element* parent = (Element*) attSet->get(0)->getXPathParent();
                processAttributeSets(parent, aNode, aPs);
                mAttributeSetStack.pop();
            }
            for (i = 0; i < attSet->size(); i++)
                processAction(aNode, attSet->get(i), aPs);
            delete attSet;
        }
    }
} //-- processAttributeSets

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

    if (!xslAction || !params)
      return params;

    params->setObjectDeletion(MB_TRUE);

    //-- handle xsl:with-param elements
    Node* tmpNode = xslAction->getFirstChild();
    while (tmpNode) {
        int nodeType = tmpNode->getNodeType();
        if ( nodeType == Node::ELEMENT_NODE ) {
            Element* action = (Element*)tmpNode;
            short xslType = getElementType(action, ps);
            if ( xslType == XSLType::WITH_PARAM ) {
                String name;
                if (!action->getAttr(txXSLTAtoms::name,
                                     kNameSpaceID_None, name)) {
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
        tmpNode = tmpNode->getNextSibling();
    }
    return params;
} //-- processParameters

/**
 * Processes the children of the specified element using the given context node
 * and ProcessorState
 * @param node the context node
 * @param xslElement the template to be processed. Must be != NULL
 * @param ps the current ProcessorState
**/
void XSLTProcessor::processChildren(Node* node, Element* xslElement, ProcessorState* ps) {

    NS_ASSERTION(xslElement,"xslElement is NULL in call to XSLTProcessor::processChildren!");

    Stack* bindings = ps->getVariableSetStack();
    NamedMap localBindings;
    localBindings.setObjectDeletion(MB_TRUE);
    bindings->push(&localBindings);
    Node* child = xslElement->getFirstChild();
    while (child) {
        processAction(node, child, ps);
        child = child->getNextSibling();
    }
    bindings->pop();
} //-- processChildren

void
XSLTProcessor::processChildrenAsValue(Node* aNode,
                                      Element* aElement,
                                      ProcessorState* aPs,
                                      MBool aOnlyText,
                                      String& aValue)
{
    txXMLEventHandler* previousHandler = mResultHandler;
    txTextHandler valueHandler(aValue, aOnlyText);
    mResultHandler = &valueHandler;
    processChildren(aNode, aElement, aPs);
    mResultHandler = previousHandler;
}

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

    NS_ASSERTION(xslTemplate, "xslTemplate is NULL in call to XSLTProcessor::processTemplate!");

    Stack* bindings = ps->getVariableSetStack();
    NamedMap localBindings;
    localBindings.setObjectDeletion(MB_TRUE);
    bindings->push(&localBindings);
    processTemplateParams(xslTemplate, node, ps, params);
    Node* tmp = xslTemplate->getFirstChild();
    while (tmp) {
        processAction(node,tmp,ps);
        tmp = tmp->getNextSibling();
    }
    
    if (params) {
        StringList* keys = params->keys();
        if (keys) {
            StringListIterator keyIter(keys);
            String* key;
            while((key = keyIter.next())) {
                VariableBinding *var, *param;
                var = (VariableBinding*)localBindings.get(*key);
                param = (VariableBinding*)params->get(*key);
                if (var && var->getValue() == param->getValue()) {
                    // Don't delete the contained ExprResult since it's
                    // not ours
                    var->setValue(0);
                }
            }
        }
        else {
            // out of memory so we can't get the keys
            // don't delete any variables since it's better we leak then
            // crash
            localBindings.setObjectDeletion(MB_FALSE);
        }
        delete keys;
    }
    
    bindings->pop();
} //-- processTemplate

void XSLTProcessor::processMatchedTemplate(Node* aXslTemplate,
                                           Node* aNode,
                                           NamedMap* aParams,
                                           const String& aMode,
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

        aPs->pushCurrentNode(aNode);
        processTemplate(aNode, aXslTemplate, aPs, aParams);
        aPs->popCurrentNode();

        aPs->setCurrentTemplateRule(oldTemplate);
    }
    else {
        processDefaultTemplate(aNode, aPs, aMode);
    }
}

/**
 * Invokes the default template for the specified node
 * @param node  context node
 * @param ps    current ProcessorState
 * @param mode  template mode
**/
void XSLTProcessor::processDefaultTemplate(Node* node,
                                           ProcessorState* ps,
                                           const String& mode)
{
    NS_ASSERTION(node, "context node is NULL in call to XSLTProcessor::processTemplate!");

    switch(node->getNodeType())
    {
        case Node::ELEMENT_NODE :
        case Node::DOCUMENT_NODE :
        {
            if (!mNodeExpr)
                break;

            ExprResult* exprResult = mNodeExpr->evaluate(node, ps);
            if (!exprResult ||
                exprResult->getResultType() != ExprResult::NODESET) {
                notifyError("None-nodeset returned while processing default template");
                delete exprResult;
                return;
            }

            NodeSet* nodeSet = (NodeSet*)exprResult;

            //-- push nodeSet onto context stack
            ps->getNodeSetStack()->push(nodeSet);
            for (int i = 0; i < nodeSet->size(); i++) {
                Node* currNode = nodeSet->get(i);

                ProcessorState::ImportFrame *frame;
                Node* xslTemplate = ps->findTemplate(currNode, mode, &frame);
                processMatchedTemplate(xslTemplate, currNode, 0, mode, frame,
                                       ps);
            }
            //-- remove nodeSet from context stack
            ps->getNodeSetStack()->pop();
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
        Node* tmpNode = xslTemplate->getFirstChild();
        //-- handle params
        while (tmpNode) {
            int nodeType = tmpNode->getNodeType();
            if ( nodeType == Node::ELEMENT_NODE ) {
                Element* action = (Element*)tmpNode;
                short xslType = getElementType(action, ps);
                if ( xslType == XSLType::PARAM ) {
                    String name;
                    if (!action->getAttr(txXSLTAtoms::name,
                                         kNameSpaceID_None, name)) {
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
            else if (nodeType == Node::TEXT_NODE ||
                     nodeType == Node::CDATA_SECTION_NODE) {
                if (!XMLUtils::isWhitespace(tmpNode->getNodeValue()))
                    break;
            }
            tmpNode = tmpNode->getNextSibling();
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
    if (xslVariable->hasAttr(txXSLTAtoms::select, kNameSpaceID_None)) {
        Expr* expr = ps->getExpr(xslVariable, ProcessorState::SelectAttr);
        if (!expr)
            return new StringResult("unable to process variable");
        return expr->evaluate(node, ps);
    }
    else if (xslVariable->hasChildNodes()) {
        txResultTreeFragment* rtf = new txResultTreeFragment();
        if (!rtf)
            // XXX ErrorReport: Out of memory
            return 0;
        txXMLEventHandler* previousHandler = mResultHandler;
        txRtfHandler rtfHandler(ps->getResultDocument(), rtf);
        mResultHandler = &rtfHandler;
        processChildren(node, xslVariable, ps);
        //NS_ASSERTION(previousHandler, "Setting mResultHandler to NULL!");
        mResultHandler = previousHandler;
        return rtf;
    }
    else {
        return new StringResult("");
    }
} //-- processVariable

void XSLTProcessor::startTransform(Node* aNode, ProcessorState* aPs)
{
    mHaveDocumentElement = MB_FALSE;
    mOutputHandler->startDocument();
    process(aNode, NULL_STRING, aPs);
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
        case eMethodNotSet:
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

/**
 * Performs the xsl:copy action as specified in the XSLT specification
 */
void XSLTProcessor::xslCopy(Node* aNode, Element* aAction, ProcessorState* aPs)
{
    if (!aNode)
        return;

    switch (aNode->getNodeType()) {
        case Node::DOCUMENT_NODE:
        {
            // Just process children
            processChildren(aNode, aAction, aPs);
            break;
        }
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)aNode;
            String nodeName = element->getNodeName();
            PRInt32 nsID = element->getNamespaceID();

            startElement(aPs, nodeName, nsID);
            // XXX copy namespace attributes once we have them
            processAttributeSets(aAction, aNode, aPs);
            processChildren(aNode, aAction, aPs);
            NS_ASSERTION(mResultHandler, "mResultHandler must not be NULL!");
            mResultHandler->endElement(nodeName, nsID);
            break;
        }
        default:
        {
            copyNode(aNode, aPs);
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
            mResultHandler->comment(((Comment*)aNode)->getData());
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
                    mResultHandler->attribute(attr->getName(), attr->getNamespaceID(),
                                              attr->getValue());
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
            mResultHandler->characters(((CharacterData*)aNode)->getData());
            break;
        }
    }
}

void
XSLTProcessor::startElement(ProcessorState* aPs, const String& aName, const PRInt32 aNsID)
{
    if (!mHaveDocumentElement && (mResultHandler == mOutputHandler)) {
        txOutputFormat* format = aPs->getOutputFormat();
        // XXX Should check for whitespace-only sibling text nodes
        if ((format->mMethod == eMethodNotSet)
            && (aNsID == kNameSpaceID_None)
            && aName.isEqualIgnoreCase("html")) {
            // Switch to html output mode according to the XSLT spec.
            format->mMethod = eHTMLOutput;
#ifdef TX_EXE
            ostream* out;
            mOutputHandler->getOutputStream(&out);
            delete mOutputHandler;
            mOutputHandler = new txHTMLOutput();
            NS_ASSERTION(mOutputHandler, "Setting mResultHandler to NULL!");
            mOutputHandler->setOutputStream(out);
            mResultHandler = mOutputHandler;
#endif
            mOutputHandler->setOutputFormat(format);
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
                                 nsIObserver* aObserver)
{
    // We need source and result documents but no stylesheet.
    NS_ENSURE_ARG(aSourceDOM);
    NS_ENSURE_ARG(aOutputDoc);

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
    mDocument = do_QueryInterface(aOutputDoc);
    NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
    Document resultDocument(aOutputDoc);

    // Reset the output document.
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIDocument> inputDocument = do_QueryInterface(sourceDOMDocument);
    if (inputDocument) {
        inputDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
        nsCOMPtr<nsIIOService> serv(do_GetService(NS_IOSERVICE_CONTRACTID));
        if (serv) {
            // Create a temporary channel to get nsIDocument->Reset to
            // do the right thing. We want the output document to get
            // much of the input document's characteristics.
            nsCOMPtr<nsIURI> docURL;
            inputDocument->GetDocumentURL(getter_AddRefs(docURL));
            serv->NewChannelFromURI(docURL, getter_AddRefs(channel));
        }
    }
    mDocument->Reset(channel, loadGroup);

    // Start of block to ensure the destruction of the ProcessorState
    // before the destruction of the documents.
    {
        // Create a new ProcessorState
        ProcessorState ps(&sourceDocument, &xslDocument, &resultDocument);

        // XXX Need to add error observers

        // Set current node and nodeset.
        NodeSet nodeSet(&sourceDocument);
        ps.pushCurrentNode(&sourceDocument);
        ps.getNodeSetStack()->push(&nodeSet);

        // Index templates and process top level xsl elements
        ListIterator importFrame(ps.getImportFrames());
        importFrame.addAfter(new ProcessorState::ImportFrame(0));
        if (!importFrame.next())
            return NS_ERROR_OUT_OF_MEMORY;
        nsCOMPtr<nsIDOMDocument> styleDoc = do_QueryInterface(aStyleDOM);
        if (styleDoc) {
            processStylesheet(&sourceDocument, &xslDocument, &importFrame,
                              &ps);
        }
        else {
            nsCOMPtr<nsIDOMElement> styleElem = do_QueryInterface(aStyleDOM);
            NS_ENSURE_TRUE(styleElem, NS_ERROR_FAILURE);
            Element* element = xslDocument.createElement(styleElem);
            NS_ENSURE_TRUE(element, NS_ERROR_OUT_OF_MEMORY);
            processTopLevel(&sourceDocument, element, &importFrame, &ps);
        }

        initializeHandlers(&ps);

        if (mOutputHandler) {
            mOutputHandler->setOutputDocument(aOutputDoc);
            if (!aObserver)
                // Don't load stylesheets, we can't notify the caller when
                // they're loaded.
                mOutputHandler->disableStylesheetLoad();
        }

        // Get the script loader of the result document.
        nsCOMPtr<nsIScriptLoader> loader;
        mDocument->GetScriptLoader(getter_AddRefs(loader));
        if (loader) {
            if (aObserver)
                loader->AddObserver(this);
            else
                // Don't load scripts, we can't notify the caller when they're loaded.
                loader->Suspend();
        }

        // Process root of XML source document
        startTransform(sourceNode, &ps);
    }
    // End of block to ensure the destruction of the ProcessorState
    // before the destruction of the documents.

    mObserver = aObserver;
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
    if (!mObserver)
        return;

    if (!mOutputHandler || !mOutputHandler->isDone())
        return;

    nsCOMPtr<nsIScriptLoader> loader;
    mDocument->GetScriptLoader(getter_AddRefs(loader));
    if (loader)
        loader->RemoveObserver(this);
    mDocument = nsnull;

    nsresult rv;
    nsCOMPtr<nsIObserverService> anObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIContent> rootContent;
        mOutputHandler->getRootContent(getter_AddRefs(rootContent));
        anObserverService->AddObserver(mObserver, "xslt-done", PR_TRUE);
        anObserverService->NotifyObservers(rootContent, "xslt-done", nsnull);
    }
    mObserver = nsnull;
}

// XXX
// XXX Mozilla module only code. This should move to txMozillaXSLTProcessor
// XXX END

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
