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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
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

#include "txXSLTPatterns.h"
#include "txNodeSetContext.h"
#include "txForwardContext.h"
#include "XSLTFunctions.h"
#include "ProcessorState.h"
#ifndef TX_EXE
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsINodeInfo.h"
#include "XMLUtils.h"
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
MBool txUnionPattern::matches(Node* aNode, txIMatchContext* aContext)
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

/*
 * The String representation will be appended to any data in the
 * destination String, to allow cascading calls to other
 * toString() methods for mLocPathPatterns.
 */
void txUnionPattern::toString(nsAString& aDest)
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
} // toString


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

MBool txLocPathPattern::matches(Node* aNode, txIMatchContext* aContext)
{
    NS_ASSERTION(aNode && mSteps.getLength(), "Internal error");

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
    Node* node = aNode->getXPathParent();

    while (step->isChild) {
        step = (Step*)iter.previous();
        if (!step)
            return MB_TRUE; // all steps matched
        if (!node || !step->pattern->matches(node, aContext))
            return MB_FALSE; // no more ancestors or no match

        node = node->getXPathParent();
    }

    // We have at least one // path separator
    Node *blockStart = node;
    txListIterator blockIter(iter);

    while ((step = (Step*)iter.previous())) {
        if (!node)
            return MB_FALSE; // There are more steps in the current block 
                             // than ancestors of the tested node

        if (!step->pattern->matches(node, aContext)) {
            // Didn't match. We restart at beginning of block using a new
            // start node
            iter = blockIter;
            blockStart = blockStart->getXPathParent();
            node = blockStart;
        }
        else {
            node = node->getXPathParent();
            if (!step->isChild) {
                // We've matched an entire block. Set new start iter and start node
                blockIter = iter;
                blockStart = node;
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

void txLocPathPattern::toString(nsAString& aDest)
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
} // txLocPathPattern::toString

/*
 * txRootPattern
 *
 * a txPattern matching the document node, or '/'
 */

txRootPattern::~txRootPattern()
{
}

MBool txRootPattern::matches(Node* aNode, txIMatchContext* aContext)
{
    return aNode && (aNode->getNodeType() == Node::DOCUMENT_NODE);
}

double txRootPattern::getDefaultPriority()
{
    return 0.5;
}

void txRootPattern::toString(nsAString& aDest)
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
#ifdef TX_EXE
    mIds = aString;
#else
    nsAString::const_iterator pos, begin, end;
    aString.BeginReading(begin);
    aString.EndReading(end);
    pos = begin;
    while (pos != end) {
        while (pos != end && XMLUtils::isWhitespace(*pos))
            ++pos;
        begin = pos;
        if (!mIds.IsEmpty())
            mIds += PRUnichar(' ');
        while (pos != end && !XMLUtils::isWhitespace(*pos))
            ++pos;
        mIds += Substring(begin, pos);
    }
#endif
}

txIdPattern::~txIdPattern()
{
}

MBool txIdPattern::matches(Node* aNode, txIMatchContext* aContext)
{
#ifdef TX_EXE
    return MB_FALSE; // not implemented
#else
    if (aNode->getNodeType() != Node::ELEMENT_NODE) {
        return MB_FALSE;
    }

    // Get a ID attribute, if there is

    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode->getNSObj());
    NS_ASSERTION(content, "a Element without nsIContent");
    if (!content) {
        return MB_FALSE;
    }
    nsCOMPtr<nsINodeInfo> ni;
    content->GetNodeInfo(*getter_AddRefs(ni));
    if (!ni) {
        return MB_FALSE;
    }
    nsCOMPtr<nsIAtom> idAttr;
    ni->GetIDAttributeAtom(getter_AddRefs(idAttr));
    if (!idAttr) {
        return MB_FALSE; // no ID for this element defined, can't match
    }
    nsAutoString value;
    nsresult rv = content->GetAttr(kNameSpaceID_None, idAttr, value);
    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        return MB_FALSE; // no ID attribute given
    }
    nsAString::const_iterator pos, begin, end;
    mIds.BeginReading(begin);
    mIds.EndReading(end);
    pos = begin;
    const PRUnichar space = PRUnichar(' ');
    PRBool found = FindCharInReadable(space, pos, end);

    while (found) {
        if (value.Equals(Substring(begin, pos))) {
            return MB_TRUE;
        }
        ++pos; // skip ' '
        begin = pos;
        found = FindCharInReadable(PRUnichar(' '), pos, end);
    }
    if (value.Equals(Substring(begin, pos))) {
        return MB_TRUE;
    }
    return MB_FALSE;
#endif // TX_EXE
}

double txIdPattern::getDefaultPriority()
{
    return 0.5;
}

void txIdPattern::toString(nsAString& aDest)
{
#ifdef DEBUG
    aDest.Append(NS_LITERAL_STRING("txIdPattern{"));
#endif
    aDest.Append(NS_LITERAL_STRING("id('"));
    aDest.Append(mIds);
    aDest.Append(NS_LITERAL_STRING("')"));
#ifdef DEBUG
    aDest.Append(PRUnichar('}'));
#endif
}

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

MBool txKeyPattern::matches(Node* aNode, txIMatchContext* aContext)
{
    Document* contextDoc;
    if (aNode->getNodeType() == Node::DOCUMENT_NODE)
        contextDoc = (Document*)aNode;
    else
        contextDoc = aNode->getOwnerDocument();
    txXSLKey* key = mProcessorState->getKey(mName);
    const NodeSet* nodes = key->getNodes(mValue, contextDoc);
    if (!nodes || nodes->isEmpty())
        return MB_FALSE;
    MBool isTrue = nodes->contains(aNode);
    return isTrue;
}

double txKeyPattern::getDefaultPriority()
{
    return 0.5;
}

void txKeyPattern::toString(nsAString& aDest)
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

/*
 * txStepPattern
 *
 * a txPattern to hold the NodeTest and the Predicates of a StepPattern
 */

txStepPattern::~txStepPattern()
{
    delete mNodeTest;
}

MBool txStepPattern::matches(Node* aNode, txIMatchContext* aContext)
{
    NS_ASSERTION(mNodeTest && aNode, "Internal error");
    if (!aNode)
        return MB_FALSE;

    if (!mNodeTest->matches(aNode, aContext))
        return MB_FALSE;

    if (!mIsAttr && !aNode->getParentNode())
        return MB_FALSE;
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
    NodeSet nodes;
    Node* parent = aNode->getXPathParent();
    if (mIsAttr) {
        NamedNodeMap* atts = parent->getAttributes();
        if (atts) {
            PRUint32 i;
            for (i = 0; i < atts->getLength(); i++) {
                Node* attr = atts->item(i);
                if (mNodeTest->matches(attr, aContext))
                    nodes.append(attr);
            }
        }
    }
    else {
        Node* tmpNode = parent->getFirstChild();
        while (tmpNode) {
            if (mNodeTest->matches(tmpNode, aContext))
                nodes.append(tmpNode);
            tmpNode = tmpNode->getNextSibling();
        }
    }

    txListIterator iter(&predicates);
    Expr* predicate = (Expr*)iter.next();
    NodeSet newNodes;

    while (iter.hasNext()) {
        newNodes.clear();
        MBool contextIsInPredicate = MB_FALSE;
        txNodeSetContext predContext(&nodes, aContext);
        while (predContext.hasNext()) {
            predContext.next();
            ExprResult* exprResult = predicate->evaluate(&predContext);
            if (!exprResult)
                break;
            switch(exprResult->getResultType()) {
                case ExprResult::NUMBER :
                    // handle default, [position() == numberValue()]
                    if ((double)predContext.position() ==
                        exprResult->numberValue()) {
                        Node* tmp = predContext.getContextNode();
                        if (tmp == aNode)
                            contextIsInPredicate = MB_TRUE;
                        newNodes.append(tmp);
                    }
                    break;
                default:
                    if (exprResult->booleanValue()) {
                        Node* tmp = predContext.getContextNode();
                        if (tmp == aNode)
                            contextIsInPredicate = MB_TRUE;
                        newNodes.append(tmp);
                    }
                    break;
            }
            delete exprResult;
        }
        // Move new NodeSet to the current one
        nodes.clear();
        nodes.append(&newNodes);
        if (!contextIsInPredicate) {
            return MB_FALSE;
        }
        predicate = (Expr*)iter.next();
    }
    txForwardContext evalContext(aContext, aNode, &nodes);
    ExprResult* exprResult = predicate->evaluate(&evalContext);
    if (!exprResult)
        return MB_FALSE;
    if (exprResult->getResultType() == ExprResult::NUMBER)
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

void txStepPattern::toString(nsAString& aDest)
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
