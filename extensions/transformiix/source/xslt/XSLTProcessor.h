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
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */


#ifndef TRANSFRMX_XSLTPROCESSOR_H
#define TRANSFRMX_XSLTPROCESSOR_H

#ifndef __BORLANDC__
#ifdef TX_EXE
#include <iostream.h>
#include <fstream.h>
#endif
#endif


#ifndef TX_EXE
#include "nsICSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocumentTransformer.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsITransformObserver.h"
#include "nsWeakPtr.h"
#include "txMozillaTextOutput.h"
#include "txMozillaXMLOutput.h"
#endif

#include "dom.h"
#include "ExprParser.h"
#include "TxObject.h"
#include "NamedMap.h"
#include "ProcessorState.h"
#include "TxString.h"
#include "ErrorObserver.h"
#include "List.h"
#include "txTextHandler.h"

#ifndef TX_EXE
/* bacd8ad0-552f-11d3-a9f7-000064657374 */
#define TRANSFORMIIX_XSLT_PROCESSOR_CID   \
{ 0xbacd8ad0, 0x552f, 0x11d3, { 0xa9, 0xf7, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74 } }

#define TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID \
"@mozilla.org/document-transformer;1?type=text/xsl"

#else

/*
 * Initialisation and shutdown routines for standalone
 * Allocate and free static atoms.
 */
MBool txInit();
MBool txShutdown();

#endif

/**
 * A class for Processing XSL Stylesheets
**/
class XSLTProcessor
#ifndef TX_EXE
: public nsIDocumentTransformer,
  public nsIScriptLoaderObserver
#endif
{
    friend class ProcessorState;

public:
#ifndef TX_EXE
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDocumentTransformer interface
    NS_DECL_NSIDOCUMENTTRANSFORMER

    NS_DECL_NSISCRIPTLOADEROBSERVER
#endif

    /**
     * A warning message used by all templates that do not allow non character
     * data to be generated
    **/
    static const String NON_TEXT_TEMPLATE_WARNING;

    /**
     * Creates a new XSLTProcessor
    **/
    XSLTProcessor();

    /**
     * Default destructor for XSLTProcessor
    **/
    virtual ~XSLTProcessor();

    /**
     * Registers the given ErrorObserver with this XSLTProcessor
    **/
    void addErrorObserver(ErrorObserver& errorObserver);

    /**
     * Returns the name of this XSLT processor
    **/
    String& getAppName();

    /**
     * Returns the version of this XSLT processor
    **/
    String& getAppVersion();

      //--------------------------------------------/
     //-- Methods that return the Result Document -/
    //--------------------------------------------/
#ifdef TX_EXE
    /**
     * Parses all XML Stylesheet PIs associated with the
     * given XML document. If any stylesheet PIs are found with
     * type="text/xsl" the href psuedo attribute value will be
     * added to the given href argument. If multiple text/xsl stylesheet PIs
     * are found, the one closest to the end of the document is used.
    **/
    void getHrefFromStylesheetPI(Document& xmlDocument, String& href);

    /**
     * Processes the given XML Document, the XSL stylesheet
     * will be retrieved from the XML Stylesheet Processing instruction,
     * otherwise an empty document will be returned.
     * @param xmlDocument the XML document to process
     * @return the result tree.
    **/
    Document* process(Document& xmlDocument);

#endif

    /**
     * Processes the given XML Document using the given XSL document
     * @return the result tree.
     * @param documentBase the document base for resolving relative URIs.
    **/
    Document* process
         (Document& xmlDocument, Document& xslDocument);

    /**
     * Reads an XML Document from the given XML input stream, and
     * processes the document using the XSL document derived from
     * the given XSL input stream.
     * @param documentBase the document base for resolving relative URIs.
     * @return the result tree.
    **/
    Document* process(istream& xmlInput, String& xmlFilename,
                      istream& xslInput, String& xslFilename);

#ifdef TX_EXE
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
    Document* process(istream& xmlInput, String& xmlFilename);

       //----------------------------------------------/
      //-- Methods that print the result to a stream -/
     //----------------------------------------------/

    /**
     * Reads an XML Document from the given XML input stream.
     * The XSL Stylesheet is obtained from the XML Documents stylesheet PI.
     * If no Stylesheet is found, an empty document will be the result;
     * otherwise the XML Document is processed using the stylesheet.
     * The result tree is printed to the given ostream argument,
     * will not close the ostream argument
    **/
    void process(istream& xmlInput, String& xmlFilename, ostream& out);

    /**
     * Processes the given XML Document using the given XSL document.
     * The result tree is printed to the given ostream argument,
     * will not close the ostream argument
     * @param documentBase the document base for resolving relative URIs.
    **/
    void process(Document& aXMLDocument, Node& aStylesheet,
                 ostream& aOut);

    /**
     * Reads an XML Document from the given XML input stream, and
     * processes the document using the XSL document derived from
     * the given XSL input stream.
     * The result tree is printed to the given ostream argument,
     * will not close the ostream argument
     * @param documentBase the document base for resolving relative URIs.
    **/
    void process(istream& xmlInput, String& xmlFilename,
                 istream& xslInput, String& xslFilename,
                 ostream& out);

#endif

private:


    /**
     * Application Name and version
    **/
    String appName;
    String appVersion;

    /**
     * The list of ErrorObservers
    **/
    List   errorObservers;

    /**
     * Fatal ErrorObserver
    **/
    SimpleErrorObserver fatalObserver;

    /**
     * The version of XSL which this Processes
    **/
    String xslVersion;

    /*
     * Used as default expression for some elements
     */
    Expr* mNodeExpr;

#ifdef TX_EXE
    /**
     * Parses the contents of data, and returns the type and href psuedo attributes
    **/
    void parseStylesheetPI(String& data, String& type, String& href);
#endif

    void process(Node* node,
                 const String& mode,
                 ProcessorState* ps);

    /*
     * Copy a node. For document nodes, copy the children.
     */
    void copyNode(Node* aNode, ProcessorState* aPs);

    void processAction(Node* aAction, ProcessorState* aPs);

    /**
     * Processes the attribute sets specified in the use-attribute-sets attribute
     * of the element specified in aElement
    **/
    void processAttributeSets(Element* aElement, ProcessorState* aPs,
                              Stack* aRecursionStack = 0);

    /**
     * Processes the children of the specified element using the given context node
     * and ProcessorState
     * @param aElement    template to be processed. Must be != NULL
     * @param aPs         current ProcessorState
    **/
    void processChildren(Element* aElement,
                         ProcessorState* aPs);

    void processChildrenAsValue(Element* aElement,
                                ProcessorState* aPs,
                                MBool aOnlyText,
                                String& aValue);

    /**
     * Invokes the default template for the specified node
     * @param aPs    current ProcessorState
     * @param aMode  template mode
    **/
    void processDefaultTemplate(ProcessorState* aPs,
                                const txExpandedName& aMode);

    /*
     * Processes an include or import stylesheet
     * @param aHref    URI of stylesheet to process
     * @param aImportFrame current importFrame iterator
     * @param aPs      current ProcessorState
     */
    void processInclude(String& aHref,
                        txListIterator* aImportFrame,
                        ProcessorState* aPs);

    void processMatchedTemplate(Node* aTemplate,
                                txVariableMap* aParams,
                                const txExpandedName& aMode,
                                ProcessorState::ImportFrame* aFrame,
                                ProcessorState* aPs);

    /*
     * Processes the xsl:with-param child elements of the given xsl action.
     * @param aAction  the action node that takes parameters (xsl:call-template
     *                 or xsl:apply-templates
     * @param aMap     map to place parsed variables in
     * @param aPs      the current ProcessorState
     * @return         errorcode
     */
    nsresult processParameters(Element* aAction, txVariableMap* aMap,
                               ProcessorState* aPs);

    void processStylesheet(Document* aStylesheet,
                           txListIterator* aImportFrame,
                           ProcessorState* aPs);

    /*
     * Processes the specified template using the given context,
     * ProcessorState, and parameters.
     * @param aTemplate the node in the xslt document that contains the
     *                  template
     * @param aParams   map with parameters to the template
     * @param aPs       the current ProcessorState
     */
    void processTemplate(Node* aTemplate, txVariableMap* aParams,
                         ProcessorState* aPs);

    void processTopLevel(Element* aStylesheet,
                         txListIterator* importFrame,
                         ProcessorState* aPs);

    ExprResult* processVariable(Element* aVariable, ProcessorState* aPs);

    void startTransform(Node* aNode, ProcessorState* aPs);

    MBool initializeHandlers(ProcessorState* aPs);

    /*
     * Performs the xsl:copy action as specified in the XSLT specification
     */
    void xslCopy(Element* aAction, ProcessorState* aPs);

    /*
     * Performs the xsl:copy-of action as specified in the XSLT specification
     */
    void xslCopyOf(ExprResult* aExprResult, ProcessorState* aPs);

    void startElement(const String& aName, const PRInt32 aNsID, ProcessorState* aPs);

#ifdef TX_EXE
    txStreamXMLEventHandler* mOutputHandler;
#else
    txMozillaXMLEventHandler* mOutputHandler;
#endif
    txXMLEventHandler* mResultHandler;
    MBool mHaveDocumentElement;
#ifndef TX_EXE
    void SignalTransformEnd();

    nsCOMPtr<nsIScriptLoader> mScriptLoader;
    nsWeakPtr mObserver;
#endif
}; //-- XSLTProcessor

#endif
