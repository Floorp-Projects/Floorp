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
 * $Id: XSLTProcessor.h,v 1.11 2000/07/06 12:35:42 axel%pike.org Exp $
 */


#ifndef TRANSFRMX_XSLTPROCESSOR_H
#define TRANSFRMX_XSLTPROCESSOR_H

#ifndef __BORLANDC__
#ifndef MOZ_XSL
#include <iostream.h>
#include <fstream.h>
#endif
#endif


#ifdef MOZ_XSL
#include "nsIDocumentTransformer.h"
#else
#include "CommandLineUtils.h"
#include "printers.h"
#include "XMLDOMUtils.h"
#endif

#include "URIUtils.h"
#include "XMLParser.h"
#include "dom.h"
#include "ExprParser.h"
#include "MITREObject.h"
#include "NamedMap.h"
#include "Names.h"
#include "NodeSet.h"
#include "ProcessorState.h"
#include "TxString.h"
#include "Tokenizer.h"
#include "ErrorObserver.h"
#include "List.h"
#include "VariableBinding.h"
#include "Numbering.h"
#include "NodeSorter.h"

#ifdef MOZ_XSL
/* bacd8ad0-552f-11d3-a9f7-000064657374 */
#define TRANSFORMIIX_XSLT_PROCESSOR_CID   \
{ 0xbacd8ad0, 0x552f, 0x11d3, {0xa9, 0xf7, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

#define TRANSFORMIIX_XSLT_PROCESSOR_PROGID \
"component://netscape/document-transformer?type=text/xsl"

#endif


/**
 * A class for Processing XSL Stylesheets
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.11 $ $Date: 2000/07/06 12:35:42 $
**/
class XSLTProcessor
#ifdef MOZ_XSL
: public nsIDocumentTransformer
#endif
{

public:
#ifdef MOZ_XSL
    // nsISupports interface
    NS_DECL_ISUPPORTS
    // nsIDocumentTransformer interface
    NS_DECL_NSIDOCUMENTTRANSFORMER
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
#ifndef MOZ_XSL
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
     * @param documentBase the document base of the XML document, for
     * resolving relative URIs
     * @return the result tree.
    **/
    Document* process(Document& xmlDocument, String& documentBase);

#endif

    /**
     * Processes the given XML Document using the given XSL document
     * @return the result tree.
     * @param documentBase the document base for resolving relative URIs.
    **/
    Document* process
         (Document& xmlDocument, Document& xslDocument, String& documentBase);

    /**
     * Reads an XML Document from the given XML input stream, and
     * processes the document using the XSL document derived from
     * the given XSL input stream.
     * @param documentBase the document base for resolving relative URIs.
     * @return the result tree.
    **/
    Document* process(istream& xmlInput, istream& xslInput,
           String& documentBase);

#ifndef MOZ_XSL
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
    Document* process(istream& xmlInput, String& documentBase);

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
    void process(istream& xmlInput, ostream& out, String& documentBase);

    /**
     * Processes the given XML Document using the given XSL document.
     * The result tree is printed to the given ostream argument,
     * will not close the ostream argument
     * @param documentBase the document base for resolving relative URIs.
    **/
    void process
      (Document& xmlDocument, Document& xslDocument,
         ostream& out, String& documentBase);

    /**
     * Reads an XML Document from the given XML input stream, and
     * processes the document using the XSL document derived from
     * the given XSL input stream.
     * The result tree is printed to the given ostream argument,
     * will not close the ostream argument
     * @param documentBase the document base for resolving relative URIs.
    **/
    void process(istream& xmlInput, istream& xslInput,
           ostream& out, String& documentBase);

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
     * An expression parser for creating AttributeValueTemplates
    **/
    ExprParser exprParser;

    /**
     * The version of XSL which this Processes
    **/
    String xslVersion;

    /**
     * Named Map for quick reference to XSL Types
    **/
    NamedMap xslTypes;

    /**
     * Binds the given Variable
    **/
    void bindVariable(String& name,
                      ExprResult* value,
                      MBool allowShadowing,
                      ProcessorState* ps);

#ifndef MOZ_XSL

    /**
     * Prints the given XML document to the given ostream and uses
     * the properties specified in the OutputFormat.
     * This method is used to print the result tree
     * @param document the XML document to print
     * @param format the OutputFormat specifying formatting info
     * @param ostream the Stream to print to
    **/
    void print(Document& document, OutputFormat* format, ostream& out);
#endif


    /**
     * Processes the xsl:with-param elements of the given xsl action
    **/
    NamedMap* processParameters(Element* xslAction, Node* context, ProcessorState* ps);

    /**
     * Looks up the given XSLType with the given name
     * The ProcessorState is used to get the current XSLT namespace
    **/
    short getElementType(String& name, ProcessorState* ps);


    /**
     * Gets the Text value of the given DocumentFragment. The value is placed
     * into the given destination String. If a non text node element is
     * encountered and warningForNonTextNodes is turned on, the MB_FALSE
     * will be returned, otherwise true is always returned.
     * @param dfrag the document fragment to get the text from
     * @param dest the destination string to place the text into.
     * @param deep indicates to process non text nodes and recusively append
     * their value. If this value is true, the allowOnlyTextNodes flag is ignored.
    **/
    MBool getText
        (DocumentFragment* dfrag, String& dest, MBool deep, MBool allowOnlyTextNodes);

    /**
     * Notifies all registered ErrorObservers of the given error
    **/
    void notifyError(const char* errorMessage);

    /**
     * Notifies all registered ErrorObservers of the given error
    **/
    void notifyError(String& errorMessage);

    /**
     * Notifies all registered ErrorObservers of the given error
    **/
    void notifyError(String& errorMessage, ErrorObserver::ErrorLevel level);

#ifndef MOZ_XSL
    /**
     * Parses the contents of data, and returns the type and href psuedo attributes
    **/
    void parseStylesheetPI(String& data, String& type, String& href);
#endif

    void process(Node* node, Node* context, ProcessorState* ps);
    void process(Node* node, Node* context, String* mode, ProcessorState* ps);

    void processAction(Node* node, Node* xslAction, ProcessorState* ps);

    /**
     * Processes the attribute sets specified in the names argument
    **/
    void processAttributeSets(const String& names, Node* node, ProcessorState* ps);

    /**
     * Processes the given attribute value as an AttributeValueTemplate
     * @param attValue the attribute value to process
     * @param result, the String in which to store the result
     * @param context the current context node
     * @param ps the current ProcessorState
    **/
    void processAttrValueTemplate
        (const String& attValue, String& result, Node* context, ProcessorState* ps);

    void processTemplate(Node* node, Node* xslTemplate, ProcessorState* ps, NamedMap* actualParams = NULL);
    void processTemplateParams(Node* xslTemplate, Node* context, ProcessorState* ps, NamedMap* actualParams);

    void processTopLevel(Document* xslDocument, ProcessorState* ps);
    void processTopLevel(Element* stylesheet, ProcessorState* ps);

    ExprResult* processVariable(Node* node, Element* xslVariable, ProcessorState* ps);

    /**
     * Performs the xsl:copy action as specified in the XSL Working Draft
    **/
    void xslCopy(Node* node, Element* action, ProcessorState* ps);

    /**
     * Performs the xsl:copy-of action as specified in the XSLT Working Draft
    **/
    void xslCopyOf(ExprResult* exprResult, ProcessorState* ps);

}; //-- XSLTProcessor

class XSLType : public MITREObject {

public:
    enum types {
        APPLY_IMPORTS =  1,
        APPLY_TEMPLATES,
        ATTRIBUTE,
        ATTRIBUTE_SET,
        CALL_TEMPLATE,
        CHOOSE,
        COMMENT,
        COPY,
        COPY_OF,
        ELEMENT,
        IF,
        INCLUDE,
        FOR_EACH,
        LITERAL,
        NUMBER,
        OTHERWISE,
        OUTPUT,
        PARAM,
        PI,
        PRESERVE_SPACE,
        SORT,
        STRIP_SPACE,
        TEMPLATE,
        TEXT,
        VALUE_OF,
        VARIABLE,
        WHEN,
        WITH_PARAM,
        MESSAGE,
        EXPR_DEBUG  // temporary, used for debugging
    };

    XSLType(const XSLType& xslType);
    XSLType();
    XSLType(short type);
    short type;
};

#endif


