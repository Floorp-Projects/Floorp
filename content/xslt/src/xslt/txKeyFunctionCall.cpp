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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
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

#include "txExecutionState.h"
#include "nsGkAtoms.h"
#include "txSingleNodeContext.h"
#include "txXSLTFunctions.h"
#include "nsReadableUtils.h"
#include "txKey.h"
#include "txXSLTPatterns.h"
#include "txNamespaceMap.h"

/*
 * txKeyFunctionCall
 * A representation of the XSLT additional function: key()
 */

/*
 * Creates a new key function call
 */
txKeyFunctionCall::txKeyFunctionCall(txNamespaceMap* aMappings)
    : mMappings(aMappings)
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
nsresult
txKeyFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    if (!aContext || !requireParams(2, 2, aContext))
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

    txExecutionState* es =
        static_cast<txExecutionState*>(aContext->getPrivateContext());

    nsAutoString keyQName;
    nsresult rv = mParams[0]->evaluateToString(aContext, keyQName);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName keyName;
    rv = keyName.init(keyQName, mMappings, false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<txAExprResult> exprResult;
    rv = mParams[1]->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    txXPathTreeWalker walker(aContext->getContextNode());
    walker.moveToRoot();

    nsRefPtr<txNodeSet> res;
    txNodeSet* nodeSet;
    if (exprResult->getResultType() == txAExprResult::NODESET &&
        (nodeSet = static_cast<txNodeSet*>
                              (static_cast<txAExprResult*>
                                          (exprResult)))->size() > 1) {
        rv = aContext->recycler()->getNodeSet(getter_AddRefs(res));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 i;
        for (i = 0; i < nodeSet->size(); ++i) {
            nsAutoString val;
            txXPathNodeUtils::appendNodeValue(nodeSet->get(i), val);

            nsRefPtr<txNodeSet> nodes;
            rv = es->getKeyNodes(keyName, walker.getCurrentPosition(), val,
                                 i == 0, getter_AddRefs(nodes));
            NS_ENSURE_SUCCESS(rv, rv);

            res->add(*nodes);
        }
    }
    else {
        nsAutoString val;
        exprResult->stringValue(val);
        rv = es->getKeyNodes(keyName, walker.getCurrentPosition(), val,
                             true, getter_AddRefs(res));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aResult = res;
    NS_ADDREF(*aResult);

    return NS_OK;
}

Expr::ResultType
txKeyFunctionCall::getReturnType()
{
    return NODESET_RESULT;
}

bool
txKeyFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return (aContext & NODE_CONTEXT) || argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
txKeyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = nsGkAtoms::key;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif

/**
 * Hash functions
 */

bool
txKeyValueHashEntry::KeyEquals(KeyTypePointer aKey) const
{
    return mKey.mKeyName == aKey->mKeyName &&
           mKey.mRootIdentifier == aKey->mRootIdentifier &&
           mKey.mKeyValue.Equals(aKey->mKeyValue);
}

PLDHashNumber
txKeyValueHashEntry::HashKey(KeyTypePointer aKey)
{
    return aKey->mKeyName.mNamespaceID ^
           NS_PTR_TO_INT32(aKey->mKeyName.mLocalName.get()) ^
           aKey->mRootIdentifier ^
           HashString(aKey->mKeyValue);
}

bool
txIndexedKeyHashEntry::KeyEquals(KeyTypePointer aKey) const
{
    return mKey.mKeyName == aKey->mKeyName &&
           mKey.mRootIdentifier == aKey->mRootIdentifier;
}

PLDHashNumber
txIndexedKeyHashEntry::HashKey(KeyTypePointer aKey)
{
    return aKey->mKeyName.mNamespaceID ^
           NS_PTR_TO_INT32(aKey->mKeyName.mLocalName.get()) ^
           aKey->mRootIdentifier;
}

/*
 * Class managing XSLT-keys
 */

nsresult
txKeyHash::getKeyNodes(const txExpandedName& aKeyName,
                       const txXPathNode& aRoot,
                       const nsAString& aKeyValue,
                       bool aIndexIfNotFound,
                       txExecutionState& aEs,
                       txNodeSet** aResult)
{
    *aResult = nsnull;

    PRInt32 identifier = txXPathNodeUtils::getUniqueIdentifier(aRoot);

    txKeyValueHashKey valueKey(aKeyName, identifier, aKeyValue);
    txKeyValueHashEntry* valueEntry = mKeyValues.GetEntry(valueKey);
    if (valueEntry) {
        *aResult = valueEntry->mNodeSet;
        NS_ADDREF(*aResult);

        return NS_OK;
    }

    // We didn't find a value. This could either mean that that key has no
    // nodes with that value or that the key hasn't been indexed using this
    // document.

    if (!aIndexIfNotFound) {
        // If aIndexIfNotFound is set then the caller knows this key is
        // indexed, so don't bother investigating.
        *aResult = mEmptyNodeSet;
        NS_ADDREF(*aResult);

        return NS_OK;
    }

    txIndexedKeyHashKey indexKey(aKeyName, identifier);
    txIndexedKeyHashEntry* indexEntry = mIndexedKeys.PutEntry(indexKey);
    NS_ENSURE_TRUE(indexEntry, NS_ERROR_OUT_OF_MEMORY);

    if (indexEntry->mIndexed) {
        // The key was indexed and apparently didn't contain this value so
        // return the empty nodeset.
        *aResult = mEmptyNodeSet;
        NS_ADDREF(*aResult);

        return NS_OK;
    }

    // The key needs to be indexed.
    txXSLKey* xslKey = mKeys.get(aKeyName);
    if (!xslKey) {
        // The key didn't exist, so bail.
        return NS_ERROR_INVALID_ARG;
    }

    nsresult rv = xslKey->indexSubtreeRoot(aRoot, mKeyValues, aEs);
    NS_ENSURE_SUCCESS(rv, rv);
    
    indexEntry->mIndexed = true;

    // Now that the key is indexed we can get its value.
    valueEntry = mKeyValues.GetEntry(valueKey);
    if (valueEntry) {
        *aResult = valueEntry->mNodeSet;
        NS_ADDREF(*aResult);
    }
    else {
        *aResult = mEmptyNodeSet;
        NS_ADDREF(*aResult);
    }

    return NS_OK;
}

nsresult
txKeyHash::init()
{
    nsresult rv = mKeyValues.Init(8);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mIndexedKeys.Init(1);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mEmptyNodeSet = new txNodeSet(nsnull);
    NS_ENSURE_TRUE(mEmptyNodeSet, NS_ERROR_OUT_OF_MEMORY);
    
    return NS_OK;
}


/**
 * Adds a match/use pair.
 * @param aMatch  match-pattern
 * @param aUse    use-expression
 * @return false if an error occurred, true otherwise
 */
bool txXSLKey::addKey(nsAutoPtr<txPattern> aMatch, nsAutoPtr<Expr> aUse)
{
    if (!aMatch || !aUse)
        return false;

    Key* key = mKeys.AppendElement();
    if (!key)
        return false;

    key->matchPattern = aMatch;
    key->useExpr = aUse;

    return true;
}

/**
 * Indexes a document and adds it to the hash of key values
 * @param aRoot         Subtree root to index and add
 * @param aKeyValueHash Hash to add values to
 * @param aEs           txExecutionState to use for XPath evaluation
 */
nsresult txXSLKey::indexSubtreeRoot(const txXPathNode& aRoot,
                                    txKeyValueHash& aKeyValueHash,
                                    txExecutionState& aEs)
{
    txKeyValueHashKey key(mName,
                          txXPathNodeUtils::getUniqueIdentifier(aRoot),
                          EmptyString());
    return indexTree(aRoot, key, aKeyValueHash, aEs);
}

/**
 * Recursively searches a node, its attributes and its subtree for
 * nodes matching any of the keys match-patterns.
 * @param aNode         Node to search
 * @param aKey          Key to use when adding into the hash
 * @param aKeyValueHash Hash to add values to
 * @param aEs           txExecutionState to use for XPath evaluation
 */
nsresult txXSLKey::indexTree(const txXPathNode& aNode,
                             txKeyValueHashKey& aKey,
                             txKeyValueHash& aKeyValueHash,
                             txExecutionState& aEs)
{
    nsresult rv = testNode(aNode, aKey, aKeyValueHash, aEs);
    NS_ENSURE_SUCCESS(rv, rv);

    // check if the node's attributes match
    txXPathTreeWalker walker(aNode);
    if (walker.moveToFirstAttribute()) {
        do {
            rv = testNode(walker.getCurrentPosition(), aKey, aKeyValueHash,
                          aEs);
            NS_ENSURE_SUCCESS(rv, rv);
        } while (walker.moveToNextAttribute());
        walker.moveToParent();
    }

    // check if the node's descendants match
    if (walker.moveToFirstChild()) {
        do {
            rv = indexTree(walker.getCurrentPosition(), aKey, aKeyValueHash,
                           aEs);
            NS_ENSURE_SUCCESS(rv, rv);
        } while (walker.moveToNextSibling());
    }

    return NS_OK;
}

/**
 * Tests one node if it matches any of the keys match-patterns. If
 * the node matches its values are added to the index.
 * @param aNode         Node to test
 * @param aKey          Key to use when adding into the hash
 * @param aKeyValueHash Hash to add values to
 * @param aEs           txExecutionState to use for XPath evaluation
 */
nsresult txXSLKey::testNode(const txXPathNode& aNode,
                            txKeyValueHashKey& aKey,
                            txKeyValueHash& aKeyValueHash,
                            txExecutionState& aEs)
{
    nsAutoString val;
    PRUint32 currKey, numKeys = mKeys.Length();
    for (currKey = 0; currKey < numKeys; ++currKey) {
        if (mKeys[currKey].matchPattern->matches(aNode, &aEs)) {
            txSingleNodeContext *evalContext =
                new txSingleNodeContext(aNode, &aEs);
            NS_ENSURE_TRUE(evalContext, NS_ERROR_OUT_OF_MEMORY);

            nsresult rv = aEs.pushEvalContext(evalContext);
            NS_ENSURE_SUCCESS(rv, rv);

            nsRefPtr<txAExprResult> exprResult;
            rv = mKeys[currKey].useExpr->evaluate(evalContext,
                                                  getter_AddRefs(exprResult));

            delete aEs.popEvalContext();
            NS_ENSURE_SUCCESS(rv, rv);

            if (exprResult->getResultType() == txAExprResult::NODESET) {
                txNodeSet* res = static_cast<txNodeSet*>
                                            (static_cast<txAExprResult*>
                                                        (exprResult));
                PRInt32 i;
                for (i = 0; i < res->size(); ++i) {
                    val.Truncate();
                    txXPathNodeUtils::appendNodeValue(res->get(i), val);

                    aKey.mKeyValue.Assign(val);
                    txKeyValueHashEntry* entry = aKeyValueHash.PutEntry(aKey);
                    NS_ENSURE_TRUE(entry && entry->mNodeSet,
                                   NS_ERROR_OUT_OF_MEMORY);

                    if (entry->mNodeSet->isEmpty() ||
                        entry->mNodeSet->get(entry->mNodeSet->size() - 1) !=
                        aNode) {
                        entry->mNodeSet->append(aNode);
                    }
                }
            }
            else {
                exprResult->stringValue(val);

                aKey.mKeyValue.Assign(val);
                txKeyValueHashEntry* entry = aKeyValueHash.PutEntry(aKey);
                NS_ENSURE_TRUE(entry && entry->mNodeSet,
                               NS_ERROR_OUT_OF_MEMORY);

                if (entry->mNodeSet->isEmpty() ||
                    entry->mNodeSet->get(entry->mNodeSet->size() - 1) !=
                    aNode) {
                    entry->mNodeSet->append(aNode);
                }
            }
        }
    }
    
    return NS_OK;
}
