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
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */



#ifndef TRANSFRMX_PROCESSORSTATE_H
#define TRANSFRMX_PROCESSORSTATE_H

#include "DOMHelper.h"
#include "NodeSet.h"
#include "NodeStack.h"
#include "Stack.h"
#include "ErrorObserver.h"
#include "NamedMap.h"
#include "ExprParser.h"
#include "Expr.h"
#include "StringList.h"
#include "OutputFormat.h"

class txXSLKey;
class txDecimalFormat;

/**
 * Class used for keeping the current state of the XSL Processor
**/
class ProcessorState : public ContextState {

public:

    static const String wrapperNSPrefix;
    static const String wrapperName;
    static const String wrapperNS;

    /**
     * Creates a new ProcessorState
    **/
    ProcessorState();

    /**
     * Creates a new ProcessorState for the given XSL document
     * And result Document
    **/
    ProcessorState(Document& sourceDocument, Document& xslDocument, Document& resultDocument);

    /**
     * Destroys this ProcessorState
    **/
    ~ProcessorState();

    /*
     * Contain information that is import precedence dependant.
     */
    class ImportFrame {
    public:
        ImportFrame();
        ~ImportFrame();
    
        // Map of named templates
        NamedMap mNamedTemplates;

        // Map of template modes, each item in the map is a list
        // of templates
        NamedMap mMatchableTemplates;

        // List of whitespace preserving and stripping nametests
        txList mWhiteNameTests;

        // Map of named attribute sets
        NamedMap mNamedAttributeSets;


        // The following stuff is missing here:

        // ImportFrame(?) for xsl:apply-imports
        // Nametests for xsl:strip-space and xsl:preserve-space
        // Namespace aliases (xsl:namespace-alias)
        // Named attribute sets
        // Toplevel variables/parameters
        // Output specifier (xsl:output)
    };
    friend class ImportFrame;

    /*
     * Adds the given attribute set to the list of available named attribute
     * sets
     * @param aAttributeSet the Element to add as a named attribute set
     * @param aImportFrame  ImportFrame to add the attributeset to
     */
    void addAttributeSet(Element* aAttributeSet, ImportFrame* aImportFrame);

    /**
     * Registers the given ErrorObserver with this ProcessorState
    **/
    void addErrorObserver(ErrorObserver& errorObserver);

    /**
     * Adds the given template to the list of templates to process
     * @param aXslTemplate  The Element to add as a template
     * @param aImportFrame  ImportFrame to add the template to
    **/
    void addTemplate(Element* aXslTemplate, ImportFrame* aImportFrame);

    /**
     *  Adds the given Node to the Result Tree
     *
    **/
    MBool addToResultTree(Node* node);

    /**
     * Copies the node using the rules defined in the XSL specification
    **/
    Node* copyNode(Node* node);

    /**
     * Returns the AttributeSet associated with the given name
     * or null if no AttributeSet is found
    **/
    NodeSet* getAttributeSet(const String& aName);

    /**
     * Returns the source node currently being processed
    **/
    Node* getCurrentNode();

    /**
     * Gets the default Namespace URI stack.
    **/ 
    Stack* getDefaultNSURIStack();

    /*
     * Returns the template associated with the given name, or
     * null if not template is found
     */
    Element* getNamedTemplate(String& aName);

    /**
     * Returns the NodeStack which keeps track of where we are in the
     * result tree
     * @return the NodeStack which keeps track of where we are in the
     * result tree
    **/
    NodeStack* getNodeStack();

    /**
     * Returns the OutputFormat which contains information on how
     * to serialize the output. I will be removing this soon, when
     * change to an event based printer, so that I can serialize
     * as I go
    **/
    OutputFormat* getOutputFormat();

    
    Stack*     getVariableSetStack();

    Expr*        getExpr(const String& pattern);
    PatternExpr* getPatternExpr(const String& pattern);

    /**
     * Returns a pointer to the result document
    **/
    Document* getResultDocument();

    /**
     * Returns the namespace URI for the given name, this method should
     * only be called to get a namespace declared within the result
     * document.
    **/
    void getResultNameSpaceURI(const String& name, String& nameSpaceURI);

    String& getXSLNamespace();

    /**
     * Retrieve the document designated by the URI uri, using baseUri as base URI.
     * Parses it as an XML document, and returns it. If a fragment identifier is
     * supplied, the element with seleced id is returned.
     * The returned document is owned by the ProcessorState
     *
     * @param uri the URI of the document to retrieve
     * @param baseUri the base URI used to resolve the URI if uri is relative
     * @return loaded document or element pointed to by fragment identifier. If
     *         loading or parsing fails NULL will be returned.
    **/
    Node* retrieveDocument(const String& uri, const String& baseUri);

    /*
     * Return stack of urls of currently entered stylesheets
     */
    Stack* getEnteredStylesheets();

    /**
     * Return list of import containers
    **/
    List* getImportFrames();

    /**
     * Finds a template for the given Node. Only templates without
     * a mode attribute will be searched.
    **/
    Element* findTemplate(Node* node, Node* context);

    /*
     * Finds a template for the given Node. Only templates with
     * a mode attribute equal to the given mode will be searched.
     */
    Element* findTemplate(Node* aNode, Node* aContext, const String& aMode);

    /**
     * Determines if the given XSL node allows Whitespace stripping
    **/
    MBool isXSLStripSpaceAllowed(Node* node);

    /**
     * Returns the current XSLT action from the top of the stack.
     * @returns the XSLT action from the top of the stack
    **/
    Node* peekAction();

    /**
     * Adds the set of names to the Whitespace preserving element set
    **/
    void preserveSpace(String& names);

    /**
     * Removes the current XSLT action from the top of the stack.
     * @returns the XSLT action after removing from the top of the stack
    **/
    Node* popAction();

    /**
     * Removes and returns the current node being processed from the stack
     * @return the current node
    **/
    Node* popCurrentNode();

    void processAttrValueTemplate(const String& aAttValue,
                                  Node* aContext,
                                  String& aResult);

    /**
     * Adds the given XSLT action to the top of the action stack
    **/
    void pushAction(Node* xsltAction);

    /**
     * Sets the given source node as the "current node" being processed
     * @param node the source node currently being processed
    **/
    void pushCurrentNode(Node* node);


    /**
     * Sets a new default Namespace URI. This is used for the Result Tree
    **/ 
    void setDefaultNameSpaceURIForResult(const String& nsURI);

    /**
     * Sets the output method. Valid output method options are,
     * "xml", "html", or "text".
    **/ 
    void setOutputMethod(const String& method);

    /*
     * Adds the set of names to the Whitespace stripping handling list.
     * xsl:strip-space calls this with MB_TRUE, xsl:preserve-space 
     * with MB_FALSE
     */
    void shouldStripSpace(String& aNames,
                          MBool aShouldStrip,
                          ImportFrame* aImportFrame);

    /**
     * Adds the supplied xsl:key to the set of keys
    **/
    MBool addKey(Element* xslKey);

    /**
     * Returns the key with the supplied name
     * returns NULL if no such key exists
    **/
    txXSLKey* getKey(String& keyName);

    /*
     * Adds a decimal format. Returns false if the format already exists
     * but dosn't contain the exact same parametervalues
     */
    MBool addDecimalFormat(Element* element);

    /**
     * Returns a decimal format or NULL if no such format exists.
    **/
    txDecimalFormat* getDecimalFormat(String& name);

    //-------------------------------------/
    //- Virtual Methods from ContextState -/
    //-------------------------------------/

    /**
     * Returns the value of a given variable binding within the current scope
     * @param the name to which the desired variable value has been bound
     * @return the ExprResult which has been bound to the variable with
     *  the given name
    **/
    virtual ExprResult* getVariable(String& name);

    /**
     * Returns the Stack of context NodeSets
     * @return the Stack of context NodeSets
    **/
    virtual Stack* getNodeSetStack();

    /**
     * Determines if the given XML node allows Whitespace stripping
    **/
    virtual MBool isStripSpaceAllowed(Node* node);

    /**
     *  Notifies this Error observer of a new error, with default
     *  level of NORMAL
    **/
    virtual void recieveError(String& errorMessage);

    /**
     *  Notifies this Error observer of a new error using the given error level
    **/
    virtual void recieveError(String& errorMessage, ErrorLevel level);

    /**
     * Returns a call to the function that has the given name.
     * This method is used for XPath Extension Functions.
     * @return the FunctionCall for the function with the given name.
    **/
    virtual FunctionCall* resolveFunctionCall(const String& name);

    /**
     * Sorts the given NodeSet by DocumentOrder. 
     * @param nodes the NodeSet to sort
     *
     * Note: I will be moving this functionality elsewhere soon
    **/
    virtual void sortByDocumentOrder(NodeSet* nodes);

    //------------------------------------------/
    //- Virtual Methods from NamespaceResolver -/
    //------------------------------------------/

    /**
     * Returns the namespace URI for the given name, this method should
     * only be called to get a namespace declared within the context (ie.
     * the stylesheet).
    **/ 
    void getNameSpaceURI(const String& name, String& nameSpaceURI);

    /**
     * Returns the namespace URI for the given namespace prefix. This method
     * should only be called to get a namespace declared within the
     * context (ie. the stylesheet).
    **/
    void getNameSpaceURIFromPrefix(const String& prefix, String& nameSpaceURI);

private:

    enum XMLSpaceMode {STRIP = 0, DEFAULT, PRESERVE};

    struct XSLTAction {
        Node* node;
        XSLTAction* prev;
    };
    
    struct MatchableTemplate {
        Element* mTemplate;
        Pattern* mMatch;
    };

    NodeStack currentNodeStack;

    /**
     * Allows us to overcome some DOM deficiencies
    **/
    DOMHelper domHelper;

    /**
     * The list of ErrorObservers registered with this ProcessorState
    **/
    List  errorObservers;

    /**
     * Stack of URIs for currently entered stylesheets
    **/
    Stack          enteredStylesheets;

    /**
     * List of import containers. Sorted by ascending import precedence
    **/
    List           importFrames;

    /**
     * A map for named attribute sets
    **/
    List           mImportFrames;

    /**
     * Current stack of nodes, where we are in the result document tree
    **/
    NodeStack*     resultNodeStack;


    /**
     * The output format used when serializing the result
    **/
    OutputFormat format;

    /**
     * Default whitespace stripping mode
    **/
    XMLSpaceMode       defaultSpace;

    /**
     * The set of loaded documents. This includes both document() loaded
     * documents and xsl:include/xsl:import'ed documents.
    **/
    NamedMap       loadedDocuments;
    
    /**
     * The set of all available keys
    **/
    NamedMap       xslKeys;

    /*
     * A list of all avalible decimalformats
     */
    NamedMap       decimalFormats;
    
    /*
     * bool indicating if the default decimal format has been explicitly set
     * by the stylesheet
     */
    MBool          defaultDecimalFormatSet;


    XSLTAction*    currentAction;
    Stack          nodeSetStack;
    Document*      mSourceDocument;
    Document*      xslDocument;
    Document*      resultDocument;
    NamedMap       exprHash;
    NamedMap       patternExprHash;
    Stack          variableSets;
    ExprParser     exprParser;
    String         xsltNameSpace;
    NamedMap       nameSpaceMap;
    StringList     nameSpaceURIList;
    Stack          defaultNameSpaceURIStack;
    Stack          xsltNameSpaces;

    /**
     * Returns the closest xml:space value for the given node
    **/
    XMLSpaceMode getXMLSpaceMode(Node* node);

    /**
     * Initializes the ProcessorState
    **/
    void initialize();

}; //-- ProcessorState

/**
 * txNameTestItem holds both an ElementExpr and a bool for use in
 * whitespace stripping.
**/
class txNameTestItem {
public:
    txNameTestItem(String& aName, MBool stripSpace):
        mNameTest(aName),mStrips(stripSpace) {}

    MBool matches(Node* aNode, ContextState* aCS) {
        return mNameTest.matches(aNode, 0, aCS);
    }

    MBool stripsSpace() {
        return mStrips;
    }

    double getDefaultPriority() {
        return mNameTest.getDefaultPriority(0, 0, 0);
    }

protected:
    ElementExpr mNameTest;
    MBool mStrips;
};

#endif

