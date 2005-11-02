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
 * $Id: txXSLTFunctions.h,v 1.6 2005/11/02 07:33:59 peterv%netscape.com Exp $
 */

#ifndef TRANSFRMX_XSLT_FUNCTIONS_H
#define TRANSFRMX_XSLT_FUNCTIONS_H

#include "Expr.h"
#include "ExprResult.h"
#include "DOMHelper.h"
#include "TxString.h"
#include "ProcessorState.h"

/**
 * The definition for the XSLT document() function
**/
class DocumentFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new document() function call
    **/
    DocumentFunctionCall(ProcessorState* ps, Document* xslDocument);

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
    void retrieveDocument(const String& uri,const String& baseUri, NodeSet &resultNodeSet, ContextState* cs);
    Document* xslDocument;
    ProcessorState* processorState;
};

/**
 * The definition for the XSLT key() function
**/
class KeyFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new key() function call
    **/
    KeyFunctionCall();

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
 * The definition for the XSLT format-number() function
**/
class FormatNumberFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new format-number() function call
    **/
    FormatNumberFunctionCall();

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
    GenerateIdFunctionCall(DOMHelper* domHelper);

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
    DOMHelper* domHelper;
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
