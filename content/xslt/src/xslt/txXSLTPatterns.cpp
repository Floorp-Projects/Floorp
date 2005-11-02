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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsReadableUtils.h"
#include "txExecutionState.h"
#include "txXSLTPatterns.h"
#include "txNodeSetContext.h"
#include "txForwardContext.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"
#ifndef TX_EXE
#include "nsIContent.h"
#endif

/*
 * txPattern
 *
 * Base class of all patterns
 * Implements only a default getSimplePatterns
 */
nsresult txPattern::getSimplePatterns(txList& aList)
{
    aList.add(this);
    return NS_OK;
}

txPattern::~txPattern()
{
}


/*
 * txUnionPattern
 *
 * This pattern is returned by the parser for "foo | bar" constructs.
 * |xsl:template|s should use the simple patterns
 */

/*
 * Destructor, deletes all LocationPathPatterns
 */
txUnionPattern::~txUnionPattern()
{
    txListIterator iter(&mLocPathPatterns);
    while (iter.hasNext()) {
        delete (txPattern*)iter.next();
    }
}

nsresult txUnionPattern::addPattern(txPattern* aPattern)
{
    if (!aPattern)
        return NS_ERROR_NULL_POINTER;
    mLocPathPatterns.add(aPattern);
    return NS_OK;
}

/*
 * Returns the default priority of this Pattern.
 * UnionPatterns don't like this.
 * This should be called on the simple patterns.
 */
double txUnionPattern::getDefaultPriority()
{
    NS_ASSERTION(0, "Don't call getDefaultPriority on txUnionPattern");
    return Double::NaN;
}

/*
 * Determines whether this Pattern matches the given node within
 * the given context
 * This should be called on the simple patterns for xsl:template,
 * but is fine for xsl:key and xsl:number
 */
MBool txUnionPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    txListIterator iter(&mLocPathPatterns);
    while (iter.hasNext()) {
        txPattern* p = (txPattern*)iter.next();
        if (p->matches(aNode, aContext)) {
            return MB_TRUE;
        }
    }
    return MB_FALSE;
}

nsresult txUnionPattern::getSimplePatterns(txList& aList)
{
    txListIterator iter(&mLocPathPatterns);
    while (iter.hasNext()) {
        aList.add(iter.next());
        iter.remove();
    }
    return NS_OK;
}

#ifdef TX_TO_STRING
void
txUnionPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txUnionPattern{"));
#endif
    txListIterator iter(&mLocPathPatterns);
    if (iter.hasNext())
        ((txPattern*)iter.next())->toString(aDest);
    while (iter.hasNext()) {
        aDest.Append(NS_LITERAL_STRING(" | "));
        ((txPattern*)iter.next())->toString(aDest);
    }
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif


/*
 * LocationPathPattern
 *
 * a list of step patterns, can start with id or key
 * (dealt with by the parser)
 */

/*
 * Destructor, deletes all PathPatterns
 */
txLocPathPattern::~txLocPathPattern()
{
    txListIterator iter(&mSteps);
    while (iter.hasNext()) {
         delete (Step*)iter.next();
    }
}

nsresult txLocPathPattern::addStep(txPattern* aPattern, MBool isChild)
{
    if (!aPattern)
        return NS_ERROR_NULL_POINTER;
    Step* step = new Step(aPattern, isChild);
    if (!step)
        return NS_ERROR_OUT_OF_MEMORY;
    mSteps.add(step);
    return NS_OK;
}

MBool txLocPathPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    NS_ASSERTION(mSteps.getLength(), "Internal error");

    /*
     * The idea is to split up a path into blocks separated by descendant
     * operators. For example "foo/bar//baz/bop//ying/yang" is split up into
     * three blocks. The "ying/yang" block is handled by the first while-loop
     * and the "foo/bar" and "baz/bop" blocks are handled by the second
     * while-loop.
     * A block is considered matched when we find a list of ancestors that
     * match the block. If there are more than one list of ancestors that
     * match a block we only need to find the one furthermost down in the
     * tree.
     */

    txListIterator iter(&mSteps);
    iter.resetToEnd();

    Step* step;
    step = (Step*)iter.previous();
    if (!step->pattern->matches(aNode, aContext))
        return MB_FALSE;

    txXPathTreeWalker walker(aNode);
    PRBool hasParent = walker.moveToParent();

    while (step->isChild) {
        step = (Step*)iter.previous();
        if (!step)
            return MB_TRUE; // all steps matched
        if (!hasParent || !step->pattern->matches(walker.getCurrentPosition(), aContext))
            return MB_FALSE; // no more ancestors or no match

        hasParent = walker.moveToParent();
    }

    // We have at least one // path separator
    txXPathTreeWalker blockWalker(walker);
    txListIterator blockIter(iter);

    while ((step = (Step*)iter.previous())) {
        if (!hasParent)
            return MB_FALSE; // There are more steps in the current block 
                             // than ancestors of the tested node

        if (!step->pattern->matches(walker.getCurrentPosition(), aContext)) {
            // Didn't match. We restart at beginning of block using a new
            // start node
            iter = blockIter;
            hasParent = blockWalker.moveToParent();
            walker.moveTo(blockWalker);
        }
        else {
            hasParent = walker.moveToParent();
            if (!step->isChild) {
                // We've matched an entire block. Set new start iter and start node
                blockIter = iter;
                blockWalker.moveTo(walker);
            }
        }
    }

    return MB_TRUE;
} // txLocPathPattern::matches

double txLocPathPattern::getDefaultPriority()
{
    if (mSteps.getLength() > 1) {
        return 0.5;
    }

    return ((Step*)mSteps.get(0))->pattern->getDefaultPriority();
}

#ifdef TX_TO_STRING
void
txLocPathPattern::toString(nsAString& aDest)
{
    txListIterator iter(&mSteps);
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txLocPathPattern{"));
#endif
    Step* step;
    step = (Step*)iter.next();
    if (step) {
        step->pattern->toString(aDest);
    }
    while ((step = (Step*)iter.next())) {
        if (step->isChild)
            aDest.Append(PRUnichar('/'));
        else
            aDest.Append(NS_LITERAL_STRING("//"));
        step->pattern->toString(aDest);
    }
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif

/*
 * txRootPattern
 *
 * a txPattern matching the document node, or '/'
 */

txRootPattern::~txRootPattern()
{
}

MBool txRootPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    return (txXPathNodeUtils::getNodeType(aNode) == txXPathNodeType::DOCUMENT_NODE);
}

double txRootPattern::getDefaultPriority()
{
    return 0.5;
}

#ifdef TX_TO_STRING
void
txRootPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txRootPattern{"));
#endif
    if (mSerialize)
        aDest.Append(PRUnichar('/'));
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif

/*
 * txIdPattern
 *
 * txIdPattern matches if the given node has a ID attribute with one
 * of the space delimited values.
 * This looks like the id() function, but may only have LITERALs as
 * argument.
 */
txIdPattern::txIdPattern(const nsAString& aString)
{
    nsAString::const_iterator pos, begin, end;
    aString.BeginReading(begin);
    aString.EndReading(end);
    pos = begin;
    while (pos != end) {
        while (pos != end && XMLUtils::isWhitespace(*pos))
            ++pos;
        begin = pos;
        while (pos != end && !XMLUtils::isWhitespace(*pos))
            ++pos;
        // this can fail, XXX move to a Init(aString) method
        mIds.AppendString(Substring(begin, pos));
    }
}

txIdPattern::~txIdPattern()
{
}

MBool txIdPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    if (txXPathNodeUtils::getNodeType(aNode) != txXPathNodeType::ELEMENT_NODE) {
        return MB_FALSE;
    }

    // Get a ID attribute, if there is
    nsAutoString value;
#ifdef TX_EXE
    Element* elem;
    nsresult rv = txXPathNativeNode::getElement(aNode, &elem);
    NS_ASSERTION(NS_SUCCEEDED(rv), "So why claim it's an element above?");
    if (!elem->getIDValue(value)) {
        return PR_FALSE;
    }
#else
    nsIContent* content = txXPathNativeNode::getContent(aNode);
    NS_ASSERTION(content, "a Element without nsIContent");
    if (!content) {
        return MB_FALSE;
    }

    nsIAtom* idAttr = content->GetIDAttributeName();
    if (!idAttr) {
        return MB_FALSE; // no ID for this element defined, can't match
    }
    nsresult rv = content->GetAttr(kNameSpaceID_None, idAttr, value);
    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        return MB_FALSE; // no ID attribute given
    }
#endif // TX_EXE
    return mIds.IndexOf(value) > -1;
}

double txIdPattern::getDefaultPriority()
{
    return 0.5;
}

#ifdef TX_TO_STRING
void
txIdPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txIdPattern{"));
#endif
    aDest.Append(NS_LITERAL_STRING("id('"));
    PRUint32 k, count = mIds.Count() - 1;
    for (k = 0; k < count; ++k) {
        aDest.Append(*mIds[k]);
        aDest.Append(PRUnichar(' '));
    }
    aDest.Append(*mIds[count]);
    aDest.Append(NS_LITERAL_STRING("')"));
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif

/*
 * txKeyPattern
 *
 * txKeyPattern matches if the given node is in the evalation of 
 * the key() function
 * This resembles the key() function, but may only have LITERALs as
 * argument.
 */

txKeyPattern::~txKeyPattern()
{
}

MBool txKeyPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    txExecutionState* es = (txExecutionState*)aContext->getPrivateContext();
    nsAutoPtr<txXPathNode> contextDoc(txXPathNodeUtils::getOwnerDocument(aNode));
    NS_ENSURE_TRUE(contextDoc, PR_FALSE);

    nsRefPtr<txNodeSet> nodes;
    nsresult rv = es->getKeyNodes(mName, *contextDoc, mValue, PR_TRUE,
                                  getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    return nodes->contains(aNode);
}

double txKeyPattern::getDefaultPriority()
{
    return 0.5;
}

#ifdef TX_TO_STRING
void
txKeyPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txKeyPattern{"));
#endif
    aDest.Append(NS_LITERAL_STRING("key('"));
    nsAutoString tmp;
    if (mPrefix) {
        mPrefix->ToString(tmp);
        aDest.Append(tmp);
        aDest.Append(PRUnichar(':'));
    }
    mName.mLocalName->ToString(tmp);
    aDest.Append(tmp);
    aDest.Append(NS_LITERAL_STRING(", "));
    aDest.Append(mValue);
    aDest.Append(NS_LITERAL_STRING("')"));
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif

/*
 * txStepPattern
 *
 * a txPattern to hold the NodeTest and the Predicates of a StepPattern
 */

txStepPattern::~txStepPattern()
{
    delete mNodeTest;
}

MBool txStepPattern::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    NS_ASSERTION(mNodeTest, "Internal error");

    if (!mNodeTest->matches(aNode, aContext))
        return MB_FALSE;

    txXPathTreeWalker walker(aNode);
    if ((!mIsAttr && walker.getNodeType() == txXPathNodeType::ATTRIBUTE_NODE) ||
        !walker.moveToParent()) {
        return MB_FALSE;
    }
    if (isEmpty()) {
        return MB_TRUE;
    }

    /*
     * Evaluate Predicates
     *
     * Copy all siblings/attributes matching mNodeTest to nodes
     * Up to the last Predicate do
     *  Foreach node in nodes
     *   evaluate Predicate with node as context node
     *   if the result is a number, check the context position,
     *    otherwise convert to bool
     *   if result is true, copy node to newNodes
     *  if aNode is not member of newNodes, return MB_FALSE
     *  nodes = newNodes
     *
     * For the last Predicate, evaluate Predicate with aNode as
     *  context node, if the result is a number, check the position,
     *  otherwise return the result converted to boolean
     */

    // Create the context node set for evaluating the predicates
    nsRefPtr<txNodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasNext = mIsAttr ? walker.moveToFirstAttribute() :
                               walker.moveToFirstChild();
    while (hasNext) {
        if (mNodeTest->matches(walker.getCurrentPosition(), aContext)) {
            nodes->append(walker.getCurrentPosition());
        }
        hasNext = mIsAttr ? walker.moveToNextAttribute() :
                            walker.moveToNextSibling();
    }

    txListIterator iter(&predicates);
    Expr* predicate = (Expr*)iter.next();
    nsRefPtr<txNodeSet> newNodes;
    rv = aContext->recycler()->getNodeSet(getter_AddRefs(newNodes));
    NS_ENSURE_SUCCESS(rv, rv);

    while (iter.hasNext()) {
        newNodes->clear();
        MBool contextIsInPredicate = MB_FALSE;
        txNodeSetContext predContext(nodes, aContext);
        while (predContext.hasNext()) {
            predContext.next();
            nsRefPtr<txAExprResult> exprResult;
            rv = predicate->evaluate(&predContext, getter_AddRefs(exprResult));
            NS_ENSURE_SUCCESS(rv, PR_FALSE);

            switch(exprResult->getResultType()) {
                case txAExprResult::NUMBER:
                    // handle default, [position() == numberValue()]
                    if ((double)predContext.position() ==
                        exprResult->numberValue()) {
                        const txXPathNode& tmp = predContext.getContextNode();
                        if (tmp == aNode)
                            contextIsInPredicate = MB_TRUE;
                        newNodes->append(tmp);
                    }
                    break;
                default:
                    if (exprResult->booleanValue()) {
                        const txXPathNode& tmp = predContext.getContextNode();
                        if (tmp == aNode)
                            contextIsInPredicate = MB_TRUE;
                        newNodes->append(tmp);
                    }
                    break;
            }
        }
        // Move new NodeSet to the current one
        nodes->clear();
        nodes->append(*newNodes);
        if (!contextIsInPredicate) {
            return MB_FALSE;
        }
        predicate = (Expr*)iter.next();
    }
    txForwardContext evalContext(aContext, aNode, nodes);
    nsRefPtr<txAExprResult> exprResult;
    rv = predicate->evaluate(&evalContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (exprResult->getResultType() == txAExprResult::NUMBER)
        // handle default, [position() == numberValue()]
        return ((double)evalContext.position() == exprResult->numberValue());

    return exprResult->booleanValue();
} // matches

double txStepPattern::getDefaultPriority()
{
    if (isEmpty())
        return mNodeTest->getDefaultPriority();
    return 0.5;
}

#ifdef TX_TO_STRING
void
txStepPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txStepPattern{"));
#endif
    if (mIsAttr)
        aDest.Append(PRUnichar('@'));
    if (mNodeTest)
        mNodeTest->toString(aDest);

    PredicateList::toString(aDest);
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}
#endif
