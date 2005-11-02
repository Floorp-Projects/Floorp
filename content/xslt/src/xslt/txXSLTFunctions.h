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
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999, 2000 Keith Visco.
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Olivier Gerardin,
 *    -- added document() function definition
 *
 * Jonas Sicking,
 *    -- added txXSLKey class
 *
 */

#ifndef TRANSFRMX_XSLT_FUNCTIONS_H
#define TRANSFRMX_XSLT_FUNCTIONS_H

#include "Expr.h"
#include "Map.h"
#include "NodeSet.h"

class ProcessorState;

/**
 * The definition for the XSLT document() function
**/
class DocumentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new document() function call
    **/
    DocumentFunctionCall(ProcessorState* aPs, Node* aDefResolveNode);

    TX_DECL_FUNCTION;

private:
    ProcessorState* mProcessorState;
    Node* mDefResolveNode;
};

/*
 * The definition for the XSLT key() function
 */
class txKeyFunctionCall : public FunctionCall {

public:

    /*
     * Creates a new key() function call
     */
    txKeyFunctionCall(ProcessorState* aPs, Node* aQNameResolveNode);

    TX_DECL_FUNCTION;

private:
    ProcessorState* mProcessorState;
    Node* mQNameResolveNode;
};

/**
 * The definition for the XSLT format-number() function
**/
class txFormatNumberFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new format-number() function call
    **/
    txFormatNumberFunctionCall(ProcessorState* aPs, Node* aQNameResolveNode);

    TX_DECL_FUNCTION;

private:
    static const PRUnichar FORMAT_QUOTE;

    enum FormatParseState {
        Prefix,
        IntDigit,
        IntZero,
        FracZero,
        FracDigit,
        Suffix,
        Finished
    };
    
    ProcessorState* mPs;
    Node* mQNameResolveNode;
};

/**
 * DecimalFormat
 * A representation of the XSLT element <xsl:decimal-format>
 */
class txDecimalFormat : public TxObject {

public:
    /*
     * Creates a new decimal format and initilizes all properties with
     * default values
     */
    txDecimalFormat();
    MBool isEqual(txDecimalFormat* other);
    
    PRUnichar       mDecimalSeparator;
    PRUnichar       mGroupingSeparator;
    nsString        mInfinity;
    PRUnichar       mMinusSign;
    nsString        mNaN;
    PRUnichar       mPercent;
    PRUnichar       mPerMille;
    PRUnichar       mZeroDigit;
    PRUnichar       mDigit;
    PRUnichar       mPatternSeparator;
};

/**
 * The definition for the XSLT current() function
**/
class CurrentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new current() function call
    **/
    CurrentFunctionCall(ProcessorState* ps);

    TX_DECL_FUNCTION;

private:
    ProcessorState* mPs;
};

/**
 * The definition for the XSLT unparsed-entity-uri() function
**/
class UnparsedEntityUriFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new unparsed-entity-uri() function call
    **/
    UnparsedEntityUriFunctionCall();

    TX_DECL_FUNCTION;

private:
};

/**
 * The definition for the XSLT generate-id() function
**/
class GenerateIdFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new generate-id() function call
    **/
    GenerateIdFunctionCall();

    TX_DECL_FUNCTION;

private:
    static const char printfFmt[];
};

/**
 * The definition for the XSLT system-property() function
**/
class SystemPropertyFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new system-property() function call
     * aNode is the Element in the stylesheet containing the 
     * Expr and is used for namespaceID resolution
    **/
    SystemPropertyFunctionCall(Node* aQNameResolveNode);

    TX_DECL_FUNCTION;

private:
    /*
     * resolve namespaceIDs with this node
     */
    Node* mQNameResolveNode;
};

/**
 * The definition for the XSLT element-available() function
**/
class ElementAvailableFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new element-available() function call
     * aNode is the Element in the stylesheet containing the 
     * Expr and is used for namespaceID resolution
    **/
    ElementAvailableFunctionCall(Node* aQNameResolveNode);

    TX_DECL_FUNCTION;

private:
    /*
     * resolve namespaceIDs with this node
     */
    Node* mQNameResolveNode;
};

/**
 * The definition for the XSLT function-available() function
**/
class FunctionAvailableFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new function-available() function call
    **/
    FunctionAvailableFunctionCall(Node* aQNameResolveNode);

    TX_DECL_FUNCTION;

private:
    /*
     * resolve namespaceIDs with this node
     */
    Node* mQNameResolveNode;
};

#endif
