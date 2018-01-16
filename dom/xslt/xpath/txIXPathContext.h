/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TX_I_XPATH_CONTEXT
#define __TX_I_XPATH_CONTEXT

#include "txCore.h"

class FunctionCall;
class nsAtom;
class txAExprResult;
class txResultRecycler;
class txXPathNode;

/*
 * txIParseContext
 *
 * This interface describes the context needed to create
 * XPath Expressions and XSLT Patters.
 * (not completely though. key() requires the ProcessorState, which is
 * not part of this interface.)
 */

class txIParseContext
{
public:
    virtual ~txIParseContext()
    {
    }

    /*
     * Return a namespaceID for a given prefix.
     */
    virtual nsresult resolveNamespacePrefix(nsAtom* aPrefix, int32_t& aID) = 0;

    /*
     * Create a FunctionCall, needed for extension function calls and
     * XSLT. XPath function calls are resolved by the Parser.
     */
    virtual nsresult resolveFunctionCall(nsAtom* aName, int32_t aID,
                                         FunctionCall** aFunction) = 0;

    /**
     * Should nametests parsed in this context be case-sensitive
     */
    virtual bool caseInsensitiveNameTests() = 0;

    /*
     * Callback to be used by the Parser if errors are detected.
     */
    virtual void SetErrorOffset(uint32_t aOffset) = 0;

    enum Allowed {
        KEY_FUNCTION = 1 << 0
    };
    virtual bool allowed(Allowed aAllowed)
    {
        return true;
    }
};

/*
 * txIMatchContext
 *
 * Interface used for matching XSLT Patters.
 * This is the part of txIEvalContext (see below), that is independent
 * of the context node when evaluating a XPath expression, too.
 * When evaluating a XPath expression, |txIMatchContext|s are used
 * to transport the information from Step to Step.
 */
class txIMatchContext
{
public:
    virtual ~txIMatchContext()
    {
    }

    /*
     * Return the ExprResult associated with the variable with the
     * given namespace and local name.
     */
    virtual nsresult getVariable(int32_t aNamespace, nsAtom* aLName,
                                 txAExprResult*& aResult) = 0;

    /*
     * Is whitespace stripping allowed for the given node?
     * See http://www.w3.org/TR/xslt#strip
     */
    virtual nsresult isStripSpaceAllowed(const txXPathNode& aNode,
                                         bool& aAllowed) = 0;

    /**
     * Returns a pointer to the private context
     */
    virtual void* getPrivateContext() = 0;

    virtual txResultRecycler* recycler() = 0;

    /*
     * Callback to be used by the expression/pattern if errors are detected.
     */
    virtual void receiveError(const nsAString& aMsg, nsresult aRes) = 0;
};

#define TX_DECL_MATCH_CONTEXT \
    nsresult getVariable(int32_t aNamespace, nsAtom* aLName, \
                         txAExprResult*& aResult) override; \
    nsresult isStripSpaceAllowed(const txXPathNode& aNode, \
                                 bool& aAllowed) override; \
    void* getPrivateContext() override; \
    txResultRecycler* recycler() override; \
    void receiveError(const nsAString& aMsg, nsresult aRes) override

class txIEvalContext : public txIMatchContext
{
public:
    /*
     * Get the context node.
     */
    virtual const txXPathNode& getContextNode() = 0;

    /*
     * Get the size of the context node set.
     */
    virtual uint32_t size() = 0;

    /*
     * Get the position of the context node in the context node set,
     * starting with 1.
     */
    virtual uint32_t position() = 0;
};

#define TX_DECL_EVAL_CONTEXT \
    TX_DECL_MATCH_CONTEXT; \
    const txXPathNode& getContextNode() override; \
    uint32_t size() override; \
    uint32_t position() override

#endif // __TX_I_XPATH_CONTEXT
