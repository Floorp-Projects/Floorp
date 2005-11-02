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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * Jonas Sicking. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
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

#ifndef txKey_h__
#define txKey_h__

#include "nsDoubleHashtable.h"
#include "XMLUtils.h"
#include "NodeSet.h"
#include "List.h"
#include "txXSLTPatterns.h"

class txPattern;
class Expr;
class txExecutionState;

class txKeyValueHashKey
{
public:
    txKeyValueHashKey(const txExpandedName& aKeyName,
                      Document* aDocument,
                      const nsAString& aKeyValue)
        : mKeyName(aKeyName),
          mKeyValue(aKeyValue),
          mDocument(aDocument)
    {
    }

    txExpandedName mKeyName;
    nsString mKeyValue;
    Document* mDocument;
};

struct txKeyValueHashEntry : public PLDHashEntryHdr
{
    txKeyValueHashEntry(const void* aKey)
        : mKey(*NS_STATIC_CAST(const txKeyValueHashKey*, aKey))
    {
    }

    // @see nsDoubleHashtable.h
    const void* GetKey();
    PRBool MatchEntry(const void* aKey) const;
    static PLDHashNumber HashKey(const void* aKey);
    
    txKeyValueHashKey mKey;
    NodeSet mNodeSet;
};

DECL_DHASH_WRAPPER(txKeyValueHash, txKeyValueHashEntry, txKeyValueHashKey&);

class txIndexedKeyHashKey
{
public:
    txIndexedKeyHashKey(txExpandedName aKeyName,
                        Document* aDocument)
        : mKeyName(aKeyName),
          mDocument(aDocument)
    {
    }

    txExpandedName mKeyName;
    Document* mDocument;
};

struct txIndexedKeyHashEntry : public PLDHashEntryHdr
{
    txIndexedKeyHashEntry(const void* aKey)
        : mKey(*NS_STATIC_CAST(const txIndexedKeyHashKey*, aKey)),
          mIndexed(PR_FALSE)
    {
    }

    // @see nsDoubleHashtable.h
    const void* GetKey();
    PRBool MatchEntry(const void* aKey) const;
    static PLDHashNumber HashKey(const void* aKey);

    txIndexedKeyHashKey mKey;
    PRBool mIndexed;
};

DECL_DHASH_WRAPPER(txIndexedKeyHash, txIndexedKeyHashEntry,
                   txIndexedKeyHashKey&);

/**
 * Class holding all <xsl:key>s of a particular expanded name in the
 * stylesheet.
 */
class txXSLKey : public TxObject {
    
public:
    txXSLKey(const txExpandedName& aName) : mName(aName)
    {
    }
    ~txXSLKey();
    
    /**
     * Adds a match/use pair.
     * @param aMatch  match-pattern
     * @param aUse    use-expression
     * @return PR_FALSE if an error occured, PR_TRUE otherwise
     */
    PRBool addKey(nsAutoPtr<txPattern> aMatch, nsAutoPtr<Expr> aUse);

    /**
     * Indexes a document and adds it to the hash of key values
     * @param aDocument     Document to index and add
     * @param aKeyValueHash Hash to add values to
     * @param aEs           txExecutionState to use for XPath evaluation
     */
    nsresult indexDocument(Document* aDocument,
                           txKeyValueHash& aKeyValueHash,
                           txExecutionState& aEs);

private:
    /**
     * Recursively searches a node, its attributes and its subtree for
     * nodes matching any of the keys match-patterns.
     * @param aNode         Node to search
     * @param aKey          Key to use when adding into the hash
     * @param aKeyValueHash Hash to add values to
     * @param aEs           txExecutionState to use for XPath evaluation
     */
    nsresult indexTree(Node* aNode, txKeyValueHashKey& aKey,
                       txKeyValueHash& aKeyValueHash, txExecutionState& aEs);

    /**
     * Tests one node if it matches any of the keys match-patterns. If
     * the node matches its values are added to the index.
     * @param aNode         Node to test
     * @param aKey          Key to use when adding into the hash
     * @param aKeyValueHash Hash to add values to
     * @param aEs           txExecutionState to use for XPath evaluation
     */
    nsresult testNode(Node* aNode, txKeyValueHashKey& aKey,
                      txKeyValueHash& aKeyValueHash, txExecutionState& aEs);

    /**
     * represents one match/use pair
     */
    struct Key {
        nsAutoPtr<txPattern> matchPattern;
        nsAutoPtr<Expr> useExpr;
    };

    /**
     * List of all match/use pairs. The items as |Key|s
     */
    List mKeys;
    
    /**
     * Name of this key
     */
    txExpandedName mName;
};


class txKeyHash
{
public:
    txKeyHash(const txExpandedNameMap& aKeys)
        : mKeys(aKeys)
    {
    }
    
    nsresult init();

    nsresult getKeyNodes(const txExpandedName& aKeyName,
                         Document* aDocument,
                         const nsAString& aKeyValue,
                         PRBool aIndexIfNotFound,
                         txExecutionState& aEs,
                         const NodeSet** aResult);

private:
    // Hash of all indexed key-values
    txKeyValueHash mKeyValues;

    // Hash showing which keys+documents has been indexed
    txIndexedKeyHash mIndexedKeys;
    
    // Map of txXSLKeys
    const txExpandedNameMap& mKeys;
};


#endif //txKey_h__
