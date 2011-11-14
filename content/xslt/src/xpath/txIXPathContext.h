/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __TX_I_XPATH_CONTEXT
#define __TX_I_XPATH_CONTEXT

#include "txCore.h"

class FunctionCall;
class nsIAtom;
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
    virtual nsresult resolveNamespacePrefix(nsIAtom* aPrefix, PRInt32& aID) = 0;

    /*
     * Create a FunctionCall, needed for extension function calls and
     * XSLT. XPath function calls are resolved by the Parser.
     */
    virtual nsresult resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                         FunctionCall** aFunction) = 0;

    /**
     * Should nametests parsed in this context be case-sensitive
     */
    virtual bool caseInsensitiveNameTests() = 0;

    /*
     * Callback to be used by the Parser if errors are detected.
     */
    virtual void SetErrorOffset(PRUint32 aOffset) = 0;
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
    virtual nsresult getVariable(PRInt32 aNamespace, nsIAtom* aLName,
                                 txAExprResult*& aResult) = 0;

    /*
     * Is whitespace stripping allowed for the given node?
     * See http://www.w3.org/TR/xslt#strip
     */
    virtual bool isStripSpaceAllowed(const txXPathNode& aNode) = 0;

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
    nsresult getVariable(PRInt32 aNamespace, nsIAtom* aLName, \
                         txAExprResult*& aResult); \
    bool isStripSpaceAllowed(const txXPathNode& aNode); \
    void* getPrivateContext(); \
    txResultRecycler* recycler(); \
    void receiveError(const nsAString& aMsg, nsresult aRes)

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
    virtual PRUint32 size() = 0;

    /*
     * Get the position of the context node in the context node set,
     * starting with 1.
     */
    virtual PRUint32 position() = 0;
};

#define TX_DECL_EVAL_CONTEXT \
    TX_DECL_MATCH_CONTEXT; \
    const txXPathNode& getContextNode(); \
    PRUint32 size(); \
    PRUint32 position()

#endif // __TX_I_XPATH_CONTEXT
