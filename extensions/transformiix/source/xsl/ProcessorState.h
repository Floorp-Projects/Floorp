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



#ifndef MITREXSL_PROCESSORSTATE_H
#define MITREXSL_PROCESSORSTATE_H

#include "dom.h"
#include "XMLUtils.h"
#include "Names.h"
#include "NodeSet.h"
#include "NodeStack.h"
#include "Stack.h"
#include "ErrorObserver.h"
#include "List.h"
#include "NamedMap.h"
#include "ExprParser.h"
#include "Expr.h"
#include "StringList.h"
#include "Tokenizer.h"

/**
 * Class used for keeping the current state of the XSL Processor
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class ProcessorState : public ContextState
{

public:

    static const String wrapperNSPrefix;
    static const String wrapperName;
    static const String wrapperNS;

    /**
     * Creates a new ProcessorState for the given XSL document
     * And result Document
    **/
    ProcessorState(Document& xslDocument, Document& resultDocument);

    /**
     * Destroys this ProcessorState
    **/
    ~ProcessorState();

    /**
     *  Adds the given attribute set to the list of available named attribute sets
     * @param attributeSet the Element to add as a named attribute set
    **/
    void addAttributeSet(Element* attributeSet);

    /**
     * Registers the given ErrorObserver with this ProcessorState
    **/
    void addErrorObserver(ErrorObserver& errorObserver);

    /**
     *  Adds the given template to the list of templates to process
     * @param xslTemplate, the Element to add as a template
    **/
    void addTemplate(Element* xslTemplate);

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
    NodeSet* getAttributeSet(const String& name);


    /**
     * Returns the template associated with the given name, or
     * null if not template is found
    **/
    Element* getNamedTemplate(String& name);

     NodeStack* getNodeStack();
     Stack*     getVariableSetStack();

     Expr*        getExpr(const String& pattern);
     PatternExpr* getPatternExpr(const String& pattern);

     /**
      * Returns a pointer to the result document
     **/
     Document* getResultDocument();

     /**
      * Returns a pointer to a list of available templates
     **/
     NodeSet* getTemplates();

    String& getXSLNamespace();

     /**
      * Finds a template for the given Node. Only templates without
      * a mode attribute will be searched.
     **/
     Element* findTemplate(Node* node, Node* context);

     /**
      * Finds a template for the given Node. Only templates with
      * a mode attribute equal to the given mode will be searched.
     **/
     Element* findTemplate(Node* node, Node* context, String* mode);

    /**
     * Determines if the given XSL node allows Whitespace stripping
    **/
    MBool isXSLStripSpaceAllowed(Node* node);

    /**
     * Adds the set of names to the Whitespace preserving element set
    **/
    void preserveSpace(String& names);

    /**
     * Adds the set of names to the Whitespace stripping element set
    **/
    void stripSpace(String& names);


     //-------------------------------------/
     //- Virtual Methods from ContextState -/
     //-------------------------------------/

     /**
      * Returns the value of a given variable binding within the current scope
      * @param the name to which the desired variable value has been bound
      * @return the ExprResult which has been bound to the variable with the given
      * name
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

private:

    enum XMLSpaceMode {STRIP = 0, DEFAULT, PRESERVE};

    /**
     * The list of ErrorObservers registered with this ProcessorState
    **/
    List  errorObservers;

    /**
     * A map for named attribute sets
    **/
     NamedMap      namedAttributeSets;

    /**
     * A map for named templates
    **/
    NamedMap       namedTemplates;

    /**
     * Current stack of nodes, where we are in the result document tree
    **/
    NodeStack*     nodeStack;

    /**
     * The set of whitespace preserving elements
    **/
    StringList       wsPreserve;

    /**
     * The set of whitespace stripping elements
    **/
    StringList       wsStrip;

    /**
     * The set of whitespace stripping elements
    **/
    XMLSpaceMode       defaultSpace;

    /**
     * A set of all availabe templates
    **/
    NodeSet        templates;


    Stack          nodeSetStack;
    Document*      xslDocument;
    Document*      resultDocument;
    NamedMap       exprHash;
    NamedMap       patternExprHash;
    Stack          variableSets;
    ExprParser     exprParser;
    String         xsltNameSpace;
    NamedMap       nameSpaceMap;

    //-- default templates
    Element*      dfWildCardTemplate;
    Element*      dfTextTemplate;

    /**
     * Returns the closest xml:space value for the given node
    **/
    XMLSpaceMode getXMLSpaceMode(Node* node);

    /**
     * Initializes the ProcessorState
    **/
    void initialize();

}; //-- ProcessorState


#endif

