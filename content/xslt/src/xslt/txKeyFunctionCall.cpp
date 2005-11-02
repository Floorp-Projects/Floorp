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
#include "NamedMap.h"
#include "txAtoms.h"
#include "txSingleNodeContext.h"
#include "XMLDOMUtils.h"
#include "XSLTFunctions.h"
#include "nsReadableUtils.h"
#include "txKey.h"

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

    txExpandedName keyName;
    nsresult rv = keyName.init(keyQName, mQNameResolveNode, PR_FALSE);
    if (NS_FAILED(rv)) {
        delete res;
        return new StringResult(NS_LITERAL_STRING("error"));
    }

    ExprResult* exprResult = ((Expr*)iter.next())->evaluate(aContext);
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
        int i;
        for (i = 0; i < nodeSet->size(); ++i) {
            nsAutoString val;
            XMLDOMUtils::getNodeValue(nodeSet->get(i), val);
            const NodeSet* nodes = 0;
            rv = mProcessorState->getKeyNodes(keyName, contextDoc, val,
                                              i == 0, &nodes);
            if (NS_FAILED(rv)) {
                delete res;
                delete exprResult;
                return new StringResult(NS_LITERAL_STRING("error"));
            }
            if (nodes) {
                res->add(nodes);
            }
        }
    }
    else {
        nsAutoString val;
        exprResult->stringValue(val);
        const NodeSet* nodes = 0;
        rv = mProcessorState->getKeyNodes(keyName, contextDoc, val,
                                          PR_TRUE, &nodes);
        if (NS_FAILED(rv)) {
            delete res;
            delete exprResult;
            return new StringResult(NS_LITERAL_STRING("error"));
        }
        if (nodes) {
            res->append(nodes);
        }
    }
    delete exprResult;
    return res;
}

nsresult txKeyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::key;
    NS_ADDREF(*aAtom);
    return NS_OK;
}

/**
 * Hash functions
 */

DHASH_WRAPPER(txKeyValueHash, txKeyValueHashEntry, txKeyValueHashKey&);
DHASH_WRAPPER(txIndexedKeyHash, txIndexedKeyHashEntry, txIndexedKeyHashKey&);

const void*
txKeyValueHashEntry::GetKey()
{
    return &mKey;
}

PRBool
txKeyValueHashEntry::MatchEntry(const void* aKey) const
{
    const txKeyValueHashKey* key =
        NS_STATIC_CAST(const txKeyValueHashKey*, aKey);

    return mKey.mKeyName == key->mKeyName &&
           mKey.mDocument == key->mDocument &&
           mKey.mKeyValue.Equals(key->mKeyValue);
}

PLDHashNumber
txKeyValueHashEntry::HashKey(const void* aKey)
{
    const txKeyValueHashKey* key =
        NS_STATIC_CAST(const txKeyValueHashKey*, aKey);

    return key->mKeyName.mNamespaceID ^
           NS_PTR_TO_INT32(key->mKeyName.mLocalName.get()) ^
           NS_PTR_TO_INT32(key->mDocument) ^
           HashString(key->mKeyValue);
}

const void*
txIndexedKeyHashEntry::GetKey()
{
    return &mKey;
}

PRBool
txIndexedKeyHashEntry::MatchEntry(const void* aKey) const
{
    const txIndexedKeyHashKey* key =
        NS_STATIC_CAST(const txIndexedKeyHashKey*, aKey);

    return mKey.mKeyName == key->mKeyName &&
           mKey.mDocument == key->mDocument;
}

PLDHashNumber
txIndexedKeyHashEntry::HashKey(const void* aKey)
{
    const txIndexedKeyHashKey* key =
        NS_STATIC_CAST(const txIndexedKeyHashKey*, aKey);

    return key->mKeyName.mNamespaceID ^
           NS_PTR_TO_INT32(key->mKeyName.mLocalName.get()) ^
           NS_PTR_TO_INT32(key->mDocument);
}

/*
 * Class managing XSLT-keys
 */

nsresult
txKeyHash::getKeyNodes(const txExpandedName& aKeyName,
                       Document* aDocument,
                       const nsAString& aKeyValue,
                       PRBool aIndexIfNotFound,
                       ProcessorState* aPs,
                       const NodeSet** aResult)
{
    NS_ENSURE_TRUE(mKeyValues.mHashTable.ops && mIndexedKeys.mHashTable.ops,
                   NS_ERROR_OUT_OF_MEMORY);

    *aResult = nsnull;
    txKeyValueHashKey valueKey(aKeyName, aDocument, aKeyValue);
    txKeyValueHashEntry* valueEntry = mKeyValues.GetEntry(valueKey);
    if (valueEntry) {
        *aResult = &valueEntry->mNodeSet;
        return NS_OK;
    }

    // We didn't find a value. This could either mean that that key has no
    // nodes with that value or that the key hasn't been indexed using this
    // document.

    if (!aIndexIfNotFound) {
        // If aIndexIfNotFound is set then the caller knows this key is
        // indexed, so don't bother investigating.
        return NS_OK;
    }

    txIndexedKeyHashKey indexKey(aKeyName, aDocument);
    txIndexedKeyHashEntry* indexEntry = mIndexedKeys.AddEntry(indexKey);
    NS_ENSURE_TRUE(indexEntry, NS_ERROR_OUT_OF_MEMORY);

    if (indexEntry->mIndexed) {
        // The key was indexed and apparently didn't contain this value so
        // return null.

        return NS_OK;
    }

    // The key needs to be indexed.
    txXSLKey* xslKey = (txXSLKey*)mKeys.get(aKeyName);
    if (!xslKey) {
        // The key didn't exist, so bail.
        return NS_ERROR_INVALID_ARG;
    }

    nsresult rv = xslKey->indexDocument(aDocument, mKeyValues, aPs);
    NS_ENSURE_SUCCESS(rv, rv);
    
    indexEntry->mIndexed = PR_TRUE;

    // Now that the key is indexed we can get its value.
    valueEntry = mKeyValues.GetEntry(valueKey);
    if (valueEntry) {
        *aResult = &valueEntry->mNodeSet;
    }

    return NS_OK;
}

nsresult
txKeyHash::init()
{
    nsresult rv = mKeyValues.Init(8);
    NS_ENSURE_SUCCESS(rv, rv);

    return mIndexedKeys.Init(1);
}

/**
 * Class holding all <xsl:key>s of a particular expanded name in the
 * stylesheet.
 */
txXSLKey::~txXSLKey()
{
    txListIterator iter(&mKeys);
    Key* key;
    while ((key = (Key*)iter.next())) {
        delete key->matchPattern;
        delete key->useExpr;
        delete key;
    }
}

/**
 * Adds a match/use pair.
 * @param aMatch  match-pattern
 * @param aUse    use-expression
 * @return PR_FALSE if an error occured, PR_TRUE otherwise
 */
PRBool txXSLKey::addKey(txPattern* aMatch, Expr* aUse)
{
    if (!aMatch || !aUse)
        return PR_FALSE;

    Key* key = new Key;
    if (!key)
        return PR_FALSE;

    key->matchPattern = aMatch;
    key->useExpr = aUse;
    mKeys.add(key);
    return PR_TRUE;
}

/**
 * Indexes a document and adds it to the hash of key values
 * @param aDocument     Document to index and add
 * @param aKeyValueHash Hash to add values to
 * @param aPs           ProcessorState to use for XPath evaluation
 */
nsresult txXSLKey::indexDocument(Document* aDocument,
                                 txKeyValueHash& aKeyValueHash,
                                 ProcessorState* aPs)
{
    txKeyValueHashKey key(mName, aDocument, NS_LITERAL_STRING(""));
    return indexTree(aDocument, key, aKeyValueHash, aPs);
}

/**
 * Recursively searches a node, its attributes and its subtree for
 * nodes matching any of the keys match-patterns.
 * @param aNode         Node to search
 * @param aKey          Key to use when adding into the hash
 * @param aKeyValueHash Hash to add values to
 * @param aPs           ProcessorState to use for XPath evaluation
 */
nsresult txXSLKey::indexTree(Node* aNode, txKeyValueHashKey& aKey,
                             txKeyValueHash& aKeyValueHash,
                             ProcessorState* aPs)
{
    nsresult rv = testNode(aNode, aKey, aKeyValueHash, aPs);
    NS_ENSURE_SUCCESS(rv, rv);

    // check if the nodes attributes matches
    NamedNodeMap* attrs = aNode->getAttributes();
    if (attrs) {
        for (PRUint32 i=0; i<attrs->getLength(); i++) {
            rv = testNode(attrs->item(i), aKey, aKeyValueHash, aPs);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    Node* child = aNode->getFirstChild();
    while (child) {
        rv = indexTree(child, aKey, aKeyValueHash, aPs);
        NS_ENSURE_SUCCESS(rv, rv);

        child = child->getNextSibling();
    }
    
    return NS_OK;
}

/**
 * Tests one node if it matches any of the keys match-patterns. If
 * the node matches its values are added to the index.
 * @param aNode         Node to test
 * @param aKey          Key to use when adding into the hash
 * @param aKeyValueHash Hash to add values to
 * @param aPs           ProcessorState to use for XPath evaluation
 */
nsresult txXSLKey::testNode(Node* aNode, txKeyValueHashKey& aKey,
                            txKeyValueHash& aKeyValueHash, ProcessorState* aPs)
{
    nsAutoString val;
    txListIterator iter(&mKeys);
    while (iter.hasNext())
    {
        Key* key=(Key*)iter.next();
        if (key->matchPattern->matches(aNode, aPs)) {
            txSingleNodeContext evalContext(aNode, aPs);
            txIEvalContext* prevCon =
                aPs->setEvalContext(&evalContext);
            ExprResult* exprResult = key->useExpr->evaluate(&evalContext);
            aPs->setEvalContext(prevCon);
            if (exprResult->getResultType() == ExprResult::NODESET) {
                NodeSet* res = (NodeSet*)exprResult;
                for (int i=0; i<res->size(); i++) {
                    val.Truncate();
                    XMLDOMUtils::getNodeValue(res->get(i), val);

                    aKey.mKeyValue.Assign(val);
                    txKeyValueHashEntry* entry = aKeyValueHash.AddEntry(aKey);
                    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

                    if (entry->mNodeSet.isEmpty() ||
                        entry->mNodeSet.get(entry->mNodeSet.size() - 1) !=
                        aNode) {
                        entry->mNodeSet.append(aNode);
                    }
                }
            }
            else {
                exprResult->stringValue(val);

                aKey.mKeyValue.Assign(val);
                txKeyValueHashEntry* entry = aKeyValueHash.AddEntry(aKey);
                NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

                if (entry->mNodeSet.isEmpty() ||
                    entry->mNodeSet.get(entry->mNodeSet.size()-1) !=
                    aNode) {
                    entry->mNodeSet.append(aNode);
                }
            }
            delete exprResult;
        }
    }
    
    return NS_OK;
}
