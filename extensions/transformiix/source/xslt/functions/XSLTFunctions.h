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
#include "ExprResult.h"
#include "TxString.h"
#include "ProcessorState.h"
#include "Map.h"
#include "List.h"

/**
 * The definition for the XSLT document() function
**/
class DocumentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new document() function call
    **/
    DocumentFunctionCall(ProcessorState* aPs);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    ProcessorState* mProcessorState;
};

/*
 * The definition for the XSLT key() function
 */
class txKeyFunctionCall : public FunctionCall {

public:

    /*
     * Creates a new key() function call
     */
    txKeyFunctionCall(ProcessorState* aPs);

    /*
     * Evaluates a key() xslt-functioncall. First argument is name of key
     * to use, second argument is value to look up.
     * @param aContext the context node for evaluation of this Expr
     * @param aCs      the ContextState containing the stack information needed
     *                 for evaluation
     * @return the result of the evaluation
     */
    virtual ExprResult* evaluate(Node* aContext, ContextState* aCs);

private:
    ProcessorState* mProcessorState;
};

/*
 * Class representing an <xsl:key>. Or in the case where several <xsl:key>s
 * have the same name one object represents all <xsl:key>s with that name
 */
class txXSLKey : public TxObject {
    
public:
    txXSLKey(ProcessorState* aPs);
    ~txXSLKey();
    
    /*
     * Returns a NodeSet containing all nodes within the specified document
     * that have the value keyValue. The document is indexed in case it
     * hasn't been searched previously. The returned nodeset is owned by
     * the txXSLKey object
     * @param aKeyValue Value to search for
     * @param aDoc      Document to search in
     * @return a NodeSet* containing all nodes in doc matching with value
     *         keyValue
     */
    const NodeSet* getNodes(String& aKeyValue, Document* aDoc);
    
    /*
     * Adds a match/use pair. Returns MB_FALSE if matchString or useString
     * can't be parsed.
     * @param aMatchString String to be parsed as match-pattern
     * @param aUseString   String to be parsed as use-expression
     * @return MB_FALSE if matchString or useString can't be parsed
     *         MB_TRUE otherwise
     */
    MBool addKey(const String& aMatchString, const String& aUseString);
    
private:
    /*
     * Indexes a document and adds it to the set of indexed documents
     * @param aDoc Document to index and add
     * @returns a NamedMap* containing the index
     */
    NamedMap* addDocument(Document* aDoc);

    /*
     * Recursively searches a node, its attributes and its subtree for
     * nodes matching any of the keys match-patterns.
     * @param aNode node to search
     * @param aMap index to add search result in
     */
    void indexTree(Node* aNode, NamedMap* aMap);

    /*
     * Tests one node if it matches any of the keys match-patterns. If
     * the node matches its values are added to the index.
     * @param aNode node to test
     * @param aMap index to add values to
     */
    void testNode(Node* aNode, NamedMap* aMap);

    /*
     * represents one match/use pair
     */
    struct Key {
        Pattern* matchPattern;
        Expr* useExpr;
    };

    /*
     * List of all match/use pairs
     */
    List mKeys;

    /*
     * Map containing all indexes (keyed on document). Every index is a
     * NamedMap. Every NamedMap contains NodeLists with the nodes for
     * a certain value
     */
    Map mMaps;
    
    /*
     * ProcessorState used to parse the match-patterns and
     * use-expressions
     */
    ProcessorState* mProcessorState;
    
    /*
     * Used to return empty nodeset
     */
    NodeSet mEmptyNodeset;
};

/**
 * The definition for the XSLT format-number() function
**/
class txFormatNumberFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new format-number() function call
    **/
    txFormatNumberFunctionCall(ProcessorState* aPs);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param aContext the context node for evaluation of this Expr
     * @param aCs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* aContext, ContextState* aCs);

private:
    static const UNICODE_CHAR FORMAT_QUOTE;

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
    
    UNICODE_CHAR    mDecimalSeparator;
    UNICODE_CHAR    mGroupingSeparator;
    String          mInfinity;
    UNICODE_CHAR    mMinusSign;
    String          mNaN;
    UNICODE_CHAR    mPercent;
    UNICODE_CHAR    mPerMille;
    UNICODE_CHAR    mZeroDigit;
    UNICODE_CHAR    mDigit;
    UNICODE_CHAR    mPatternSeparator;
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

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
   ProcessorState* processorState;
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

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

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

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    static char* printfFmt;
};

/**
 * The definition for the XSLT system-property() function
**/
class SystemPropertyFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new system-property() function call
    **/
    SystemPropertyFunctionCall();

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
};

/**
 * The definition for the XSLT element-available() function
**/
class ElementAvailableFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new element-available() function call
    **/
    ElementAvailableFunctionCall();

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
};

/**
 * The definition for the XSLT function-available() function
**/
class FunctionAvailableFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new function-available() function call
    **/
    FunctionAvailableFunctionCall();

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param cs the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
};

#endif
