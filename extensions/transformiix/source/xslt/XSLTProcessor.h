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

#include "ProcessorState.h"

class txIGlobalParameter : public TxObject
{
public:
    virtual nsresult getValue(ExprResult** aValue) = 0;
};

/**
 * A class for Processing XSLT Stylesheets
 */
class txXSLTProcessor
{
public:
    /**
     * Initialisation and shutdown routines
     * Allocate and free static atoms.
     */
    static MBool txInit();
    static MBool txShutdown();

    /**
     * Processes a stylesheet document.
     *
     * @param aStylesheet the stylesheet document to process
     * @param aPs the current ProcessorState
     */
    static nsresult processStylesheet(Document* aStylesheet,
                                      txExpandedNameMap* aGlobalParams,
                                      ProcessorState* aPs);

    /**
     * Processes a stylesheet element.
     *
     * @param aStylesheet the stylesheet element to process
     * @param aPs the current ProcessorState
     */
    static nsresult processTopLevel(Element* aStylesheet,
                                    txExpandedNameMap* aGlobalParams,
                                    ProcessorState* aPs);

    /**
     * Transforms a node.
     *
     * @param aPs the current ProcessorState with the node
     *            as current node and the desired templates
     */
    static void transform(ProcessorState* aPs);

private:
    /**
     * Copy a node. For document nodes, copy the children.
     *
     * @param aSourceNode node to copy
     * @param aPs the current ProcessorState
     */
    static void copyNode(Node* aSourceNode, ProcessorState* aPs);

    /**
     * Create the document we will use to create result tree fragments.
     *
     * @param aMethod the output method indicating which type of 
     *        document to create
     * @returns a document to create RTF nodes
     */
    static Document* createRTFDocument(txOutputMethod aMethod);

    /**
     * Log a message.
     *
     * @param aMessage the message to log
     */
    static void logMessage(const nsAString& aMessage);

    /**
     * Instantiate aAction in the result tree.
     * Either perform the action associated with the element in the
     * XSLT namespace, or copy the element as literal result element.
     *
     * @param aAction literal result element or XSLT element
     * @param aPs the current ProcessorState
     */
    static void processAction(Node* aAction,
                              ProcessorState* aPs);

    /**
     * Processes the attribute sets specified in the use-attribute-sets
     * attribute of the element specified in aElement.
     *
     * @param aElement Element specifying the attribute sets
     * @param aPs the current ProcessorState
     * @param aRecursionStack recursion stack to track attribute sets
     *                        including themselves, even indirectly
     */
    static void processAttributeSets(Element* aElement, ProcessorState* aPs,
                                     Stack* aRecursionStack = 0);

    /**
     * Processes the children of the specified element using the given
     * context node and ProcessorState.
     *
     * @param aElement the template to be processed. Must be != NULL
     * @param aPs the current ProcessorState
     */
    static void processChildren(Element* aElement,
                                ProcessorState* aPs);

    /**
     * Processes the children of the specified element using the given
     * context node and ProcessorState and append the text data to 
     * aValue.
     *
     * @param aElement the template to be processed. Must be != NULL
     * @param aPs the current ProcessorState
     * @param aOnlyText if true, don't allow for nested content
     * @param aValue String reference to append the result to
     */
    static void processChildrenAsValue(Element* aElement,
                                       ProcessorState* aPs,
                                       MBool aOnlyText,
                                       nsAString& aValue);

    /**
     * Invokes the default template for the specified node.
     *
     * @param aPs the current ProcessorState
     * @param aMode template mode
     */
    static void processDefaultTemplate(ProcessorState* aPs,
                                       const txExpandedName& aMode);

    /**
     * Processes an include or import stylesheet.
     *
     * @param aHref URI of stylesheet to process
     * @param aImportFrame current importFrame iterator
     * @param aPs the current ProcessorState
     */
    static void processInclude(const nsAString& aHref,
                               txListIterator* aImportFrame,
                               ProcessorState* aPs);

    /**
     * Instantiate the children of the template aTemplate with the
     * parameters aParams and the mode aMode.
     *
     * @param aTemplate xsl:template to instantiate
     * @param aParams txVariableMap holding the params, if given
     * @param aMode mode of this template
     * @param aFrame the current import frame, needed for apply-templates
     * @param aPs the current ProcessorState
     */
    static void processMatchedTemplate(Node* aTemplate,
                                       txVariableMap* aParams,
                                       const txExpandedName& aMode,
                                       ProcessorState::ImportFrame* aFrame,
                                       ProcessorState* aPs);

    /**
     * Processes the xsl:with-param child elements of the given xsl action.
     *
     * @param aAction  the action node that takes parameters (xsl:call-template
     *                 or xsl:apply-templates)
     * @param aMap     map to place parsed variables in
     * @param aPs      the current ProcessorState
     * @return         errorcode
     */
    static nsresult processParameters(Element* aAction, txVariableMap* aMap,
                                      ProcessorState* aPs);

    /**
     * Add a stylesheet to the processorstate. If the document element
     * is a LRE, just add one template with pattern "/". Otherwise,
     * call processTopLevel with the document element.
     *
     * @param aStylesheet the document to be added to the ps
     * @param aImportFrame the import frame to add the stylesheet to
     * @param aPs the current ProcessorState
     */
    static void processStylesheet(Document* aStylesheet,
                                  txExpandedNameMap* aGlobalParams,
                                  txListIterator* aImportFrame,
                                  ProcessorState* aPs);

    /**
     * Processes the specified template using the given context,
     * ProcessorState, and parameters. Iterates through the
     * |xsl:param|s and then calls processAction.
     *
     * @param aTemplate xsl:template to process
     * @param aParams variable map with xsl:with-param values
     * @param aPs the current ProcessorState
     */
    static void processTemplate(Node* aTemplate, txVariableMap* aParams,
                                ProcessorState* aPs);

    /**
     * Processes the top-level children of xsl:stylesheet.
     *
     * @param aStylesheet the stylesheet
     * @param aImportFrame the import frame of the stylesheet
     * @param aPs the current ProcessorState
     */
    static void processTopLevel(Element* aStylesheet,
                                txExpandedNameMap* aGlobalParams,
                                txListIterator* aImportFrame,
                                ProcessorState* aPs);

    /**
     * Processes the given xsl:variable.
     *
     * @param aVariable the variable element
     * @param aPs the current ProcessorState
     */
    static ExprResult* processVariable(Element* aVariable,
                                       ProcessorState* aPs);

    /**
     * Performs the xsl:copy action as specified in the XSLT specification.
     *
     * @param aAction element to copy
     * @param aPs the current ProcessorState
     */
    static void xslCopy(Element* aAction,
                        ProcessorState* aPs);

    /**
     * Performs the xsl:copy-of action as specified in the XSLT specification.
     *
     * @param aExprResult expression result to copy
     * @param aPs the current ProcessorState
     */
    static void xslCopyOf(ExprResult* aExprResult,
                          ProcessorState* aPs);

    /**
     * Used as default expression for some elements
     */
    static Expr* gNodeExpr;

    friend class ProcessorState;
};

#endif
