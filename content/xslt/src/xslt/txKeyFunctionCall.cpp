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
 * The Initial Developer of the Original Code is Jonas Sicking.
 * Portions created by Jonas Sicking are Copyright (C) 2001, Jonas Sicking.
 * All rights reserved.
 *
 * Contributor(s):
 * Jonas Sicking, sicking@bigfoot.com
 *   -- original author.
 */

#include "ProcessorState.h"
#include "txAtoms.h"
#include "txSingleNodeContext.h"
#include "XMLDOMUtils.h"
#include "XSLTFunctions.h"

/*
 * txKeyFunctionCall
 * A representation of the XSLT additional function: key()
 */

/*
 * Creates a new key function call
 */
txKeyFunctionCall::txKeyFunctionCall(ProcessorState* aPs,
                                     Node* aQNameResolveNode)
    : mProcessorState(aPs),
      mQNameResolveNode(aQNameResolveNode)
{
}

/*
 * Evaluates a key() xslt-function call. First argument is name of key
 * to use, second argument is value to look up.
 * @param aContext the context node for evaluation of this Expr
 * @param aCs      the ContextState containing the stack information needed
 *                 for evaluation
 * @return the result of the evaluation
 */
ExprResult* txKeyFunctionCall::evaluate(txIEvalContext* aContext)
{
    if (!aContext || !requireParams(2, 2, aContext))
        return new StringResult(NS_LITERAL_STRING("error"));

    NodeSet* res = new NodeSet;
    if (!res) {
        // ErrorReport: out of memory
        return 0;
    }

    txListIterator iter(&params);
    nsAutoString keyQName;
    evaluateToString((Expr*)iter.next(), aContext, keyQName);
    Expr* param = (Expr*) iter.next();

    txExpandedName keyName;
    txXSLKey* key = 0;
    nsresult rv = keyName.init(keyQName, mQNameResolveNode, MB_FALSE);
    if (NS_SUCCEEDED(rv)) {
        key = mProcessorState->getKey(keyName);
    }

    if (!key) {
        nsAutoString err(NS_LITERAL_STRING("No key with that name in: "));
        toString(err);
        aContext->receiveError(err, NS_ERROR_INVALID_ARG);
        return res;
    }

    ExprResult* exprResult = param->evaluate(aContext);
    if (!exprResult)
        return res;

    Document* contextDoc;
    Node* contextNode = aContext->getContextNode();
    if (contextNode->getNodeType() == Node::DOCUMENT_NODE)
        contextDoc = (Document*)contextNode;
    else
        contextDoc = contextNode->getOwnerDocument();

    if (exprResult->getResultType() == ExprResult::NODESET) {
        NodeSet* nodeSet = (NodeSet*) exprResult;
        for (int i=0; i<nodeSet->size(); i++) {
            nsAutoString val;
            XMLDOMUtils::getNodeValue(nodeSet->get(i), val);
            res->add(key->getNodes(val, contextDoc));
        }
    }
    else {
        nsAutoString val;
        exprResult->stringValue(val);
        res->append(key->getNodes(val, contextDoc));
    }
    delete exprResult;
    return res;

} // evaluate

nsresult txKeyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::key;
    NS_ADDREF(*aAtom);
    return NS_OK;
}

/*
 * Class representing an <xsl:key>. Or in the case where several <xsl:key>s
 * have the same name one object represents all <xsl:key>s with that name
 */

txXSLKey::txXSLKey(ProcessorState* aPs)
{
    mProcessorState = aPs;
    mMaps.setOwnership(Map::eOwnsItems);
} // txXSLKey

txXSLKey::~txXSLKey()
{
    txListIterator iter(&mKeys);
    Key* key;
    while ((key = (Key*)iter.next())) {
        delete key->matchPattern;
        delete key->useExpr;
        delete key;
    }
} // ~txXSLKey

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
const NodeSet* txXSLKey::getNodes(const nsAString& aKeyValue, Document* aDoc)
{
    NS_ASSERTION(aDoc, "missing document");
    if (!aDoc)
        return &mEmptyNodeset;

    NamedMap* map = (NamedMap*)mMaps.get(aDoc);
    if (!map) {
        map = addDocument(aDoc);
        if (!map)
            return &mEmptyNodeset;
    }

    NodeSet* nodes = (NodeSet*)map->get(aKeyValue);
    if (!nodes)
        return &mEmptyNodeset;

    return nodes;
} // getNodes

/*
 * Adds a match/use pair. Returns MB_FALSE if matchString or useString
 * can't be parsed.
 * @param aMatch  match-pattern
 * @param aUse    use-expression
 * @return MB_FALSE if an error occured, MB_TRUE otherwise
 */
MBool txXSLKey::addKey(txPattern* aMatch, Expr* aUse)
{
    if (!aMatch || !aUse)
        return MB_FALSE;

    Key* key = new Key;
    if (!key)
        return MB_FALSE;

    key->matchPattern = aMatch;
    key->useExpr = aUse;
    mKeys.add(key);
    return MB_TRUE;
} // addKey

/*
 * Indexes a document and adds it to the set of indexed documents
 * @param aDoc Document to index and add
 * @returns a NamedMap* containing the index
 */
NamedMap* txXSLKey::addDocument(Document* aDoc)
{
    NamedMap* map = new NamedMap;
    if (!map)
        return nsnull;
    map->setObjectDeletion(MB_TRUE);
    mMaps.put(aDoc, map);
    indexTree(aDoc, map);
    return map;
} // addDocument

/*
 * Recursively searches a node, its attributes and its subtree for
 * nodes matching any of the keys match-patterns.
 * @param aNode node to search
 * @param aMap index to add search result in
 */
void txXSLKey::indexTree(Node* aNode, NamedMap* aMap)
{
    testNode(aNode, aMap);

    // check if the nodes attributes matches
    NamedNodeMap* attrs = aNode->getAttributes();
    if (attrs) {
        for (PRUint32 i=0; i<attrs->getLength(); i++) {
            testNode(attrs->item(i), aMap);
        }
    }

    Node* child = aNode->getFirstChild();
    while (child) {
        indexTree(child, aMap);
        child = child->getNextSibling();
    }
} // indexTree

/*
 * Tests one node if it matches any of the keys match-patterns. If
 * the node matches its values are added to the index.
 * @param aNode node to test
 * @param aMap index to add values to
 */
void txXSLKey::testNode(Node* aNode, NamedMap* aMap)
{
    nsAutoString val;
    NodeSet *nodeSet;

    txListIterator iter(&mKeys);
    while (iter.hasNext())
    {
        Key* key=(Key*)iter.next();
        if (key->matchPattern->matches(aNode, mProcessorState)) {
            txSingleNodeContext evalContext(aNode, mProcessorState);
            txIEvalContext* prevCon =
                mProcessorState->setEvalContext(&evalContext);
            ExprResult* exprResult = key->useExpr->evaluate(&evalContext);
            mProcessorState->setEvalContext(prevCon);
            if (exprResult->getResultType() == ExprResult::NODESET) {
                NodeSet* res = (NodeSet*)exprResult;
                for (int i=0; i<res->size(); i++) {
                    val.Truncate();
                    XMLDOMUtils::getNodeValue(res->get(i), val);

                    nodeSet = (NodeSet*)aMap->get(val);
                    if (!nodeSet) {
                        nodeSet = new NodeSet;
                        if (!nodeSet)
                            return;
                        aMap->put(val, nodeSet);
                    }
                    nodeSet->append(aNode);
                }
            }
            else {
                exprResult->stringValue(val);
                nodeSet = (NodeSet*)aMap->get(val);
                if (!nodeSet) {
                    nodeSet = new NodeSet;
                    if (!nodeSet)
                        return;
                    aMap->put(val, nodeSet);
                }
                nodeSet->append(aNode);
            }
            delete exprResult;
        }
    }
} // testNode
