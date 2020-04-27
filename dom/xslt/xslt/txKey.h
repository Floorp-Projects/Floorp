/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txKey_h__
#define txKey_h__

#include "nsTHashtable.h"
#include "txNodeSet.h"
#include "txList.h"
#include "txXSLTPatterns.h"
#include "txXMLUtils.h"

class txPattern;
class Expr;
class txExecutionState;

class txKeyValueHashKey {
 public:
  txKeyValueHashKey(const txExpandedName& aKeyName, int32_t aRootIdentifier,
                    const nsAString& aKeyValue)
      : mKeyName(aKeyName),
        mKeyValue(aKeyValue),
        mRootIdentifier(aRootIdentifier) {}

  txExpandedName mKeyName;
  nsString mKeyValue;
  int32_t mRootIdentifier;
};

struct txKeyValueHashEntry : public PLDHashEntryHdr {
 public:
  typedef const txKeyValueHashKey& KeyType;
  typedef const txKeyValueHashKey* KeyTypePointer;

  explicit txKeyValueHashEntry(KeyTypePointer aKey)
      : mKey(*aKey), mNodeSet(new txNodeSet(nullptr)) {}

  txKeyValueHashEntry(const txKeyValueHashEntry& entry)
      : mKey(entry.mKey), mNodeSet(entry.mNodeSet) {}

  bool KeyEquals(KeyTypePointer aKey) const;

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey);

  enum { ALLOW_MEMMOVE = true };

  txKeyValueHashKey mKey;
  RefPtr<txNodeSet> mNodeSet;
};

typedef nsTHashtable<txKeyValueHashEntry> txKeyValueHash;

class txIndexedKeyHashKey {
 public:
  txIndexedKeyHashKey(txExpandedName aKeyName, int32_t aRootIdentifier)
      : mKeyName(aKeyName), mRootIdentifier(aRootIdentifier) {}

  txExpandedName mKeyName;
  int32_t mRootIdentifier;
};

struct txIndexedKeyHashEntry : public PLDHashEntryHdr {
 public:
  typedef const txIndexedKeyHashKey& KeyType;
  typedef const txIndexedKeyHashKey* KeyTypePointer;

  explicit txIndexedKeyHashEntry(KeyTypePointer aKey)
      : mKey(*aKey), mIndexed(false) {}

  txIndexedKeyHashEntry(const txIndexedKeyHashEntry& entry)
      : mKey(entry.mKey), mIndexed(entry.mIndexed) {}

  bool KeyEquals(KeyTypePointer aKey) const;

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey);

  enum { ALLOW_MEMMOVE = true };

  txIndexedKeyHashKey mKey;
  bool mIndexed;
};

typedef nsTHashtable<txIndexedKeyHashEntry> txIndexedKeyHash;

/**
 * Class holding all <xsl:key>s of a particular expanded name in the
 * stylesheet.
 */
class txXSLKey {
 public:
  explicit txXSLKey(const txExpandedName& aName) : mName(aName) {}

  /**
   * Adds a match/use pair.
   * @param aMatch  match-pattern
   * @param aUse    use-expression
   * @return false if an error occurred, true otherwise
   */
  bool addKey(mozilla::UniquePtr<txPattern>&& aMatch,
              mozilla::UniquePtr<Expr>&& aUse);

  /**
   * Indexes a subtree and adds it to the hash of key values
   * @param aRoot         Subtree root to index and add
   * @param aKeyValueHash Hash to add values to
   * @param aEs           txExecutionState to use for XPath evaluation
   */
  nsresult indexSubtreeRoot(const txXPathNode& aRoot,
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
  nsresult indexTree(const txXPathNode& aNode, txKeyValueHashKey& aKey,
                     txKeyValueHash& aKeyValueHash, txExecutionState& aEs);

  /**
   * Tests one node if it matches any of the keys match-patterns. If
   * the node matches its values are added to the index.
   * @param aNode         Node to test
   * @param aKey          Key to use when adding into the hash
   * @param aKeyValueHash Hash to add values to
   * @param aEs           txExecutionState to use for XPath evaluation
   */
  nsresult testNode(const txXPathNode& aNode, txKeyValueHashKey& aKey,
                    txKeyValueHash& aKeyValueHash, txExecutionState& aEs);

  /**
   * represents one match/use pair
   */
  struct Key {
    mozilla::UniquePtr<txPattern> matchPattern;
    mozilla::UniquePtr<Expr> useExpr;
  };

  /**
   * List of all match/use pairs. The items as |Key|s
   */
  nsTArray<Key> mKeys;

  /**
   * Name of this key
   */
  txExpandedName mName;
};

class txKeyHash {
 public:
  explicit txKeyHash(const txOwningExpandedNameMap<txXSLKey>& aKeys)
      : mKeyValues(4), mIndexedKeys(1), mKeys(aKeys) {}

  nsresult init();

  nsresult getKeyNodes(const txExpandedName& aKeyName, const txXPathNode& aRoot,
                       const nsAString& aKeyValue, bool aIndexIfNotFound,
                       txExecutionState& aEs, txNodeSet** aResult);

 private:
  // Hash of all indexed key-values
  txKeyValueHash mKeyValues;

  // Hash showing which keys+roots has been indexed
  txIndexedKeyHash mIndexedKeys;

  // Map of txXSLKeys
  const txOwningExpandedNameMap<txXSLKey>& mKeys;

  // Empty nodeset returned if no key is found
  RefPtr<txNodeSet> mEmptyNodeSet;
};

#endif  // txKey_h__
