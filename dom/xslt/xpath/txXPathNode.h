/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txXPathNode_h__
#define txXPathNode_h__

#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsINode.h"
#include "nsNameSpaceManager.h"

using txXPathNodeType = nsINode;

/**
 * txXPathNode represents a node in XPath's data model (which is a bit different
 * from the DOM's). While XPath 1.0 has 7 node types, we essentially deal with 3
 * kinds: a document node, any other node type that is backed by an
 * implementation of nsIContent in Gecko, and attribute nodes. Because we try to
 * avoid creating actual node objects for attribute nodes in Gecko's DOM, we
 * store attribute nodes as a pointer to their owner element and an index into
 * that element's attribute node list. So to represent the 3 kinds of node we
 * need to store:
 *
 *  - a pointer to a document (a nsIDocument as a nsINode pointer)
 *  - a pointer to a nsIContent object (as a nsINode pointer)
 *  - a pointer to a nsIContent object (a nsIContent as a nsINode pointer) and
 *    an index
 *
 * So we make txXPathNode store a |nsCOMPtr<nsINode>| and a uint32_t for the
 * index. To be able to distinguish between the attribute nodes and other nodes
 * we store a flag value for the other nodes in the index. To be able to quickly
 * cast from the nsINode pointer to a nsIDocument or nsIContent pointer we
 * actually use 2 different flag values (see txXPathNode::PositionType).
 *
 * We provide 2 fast constructors for txXPathNode, one that takes a nsIContent
 * and one that takes a nsIDocument, since for those kinds of nodes we can
 * simply store the pointer and the relevant flag value. For attribute nodes, or
 * if you have just a nsINode pointer, then there is a helper function
 * (txXPathNativeNode::createXPathNode) that either picks the right flag value
 * or the correct index (for attribute nodes) when creating the txXPathNode.
 */
class txXPathNode {
 public:
  explicit txXPathNode(const txXPathNode& aNode)
      : mNode(aNode.mNode), mIndex(aNode.mIndex) {
    MOZ_COUNT_CTOR(txXPathNode);
  }
  txXPathNode(txXPathNode&& aNode)
      : mNode(std::move(aNode.mNode)), mIndex(aNode.mIndex) {
    MOZ_COUNT_CTOR(txXPathNode);
  }

  explicit txXPathNode(mozilla::dom::Document* aDocument)
      : mNode(aDocument), mIndex(eDocument) {
    MOZ_COUNT_CTOR(txXPathNode);
  }
  explicit txXPathNode(nsIContent* aContent)
      : mNode(aContent), mIndex(eContent) {
    MOZ_COUNT_CTOR(txXPathNode);
  }

  txXPathNode& operator=(txXPathNode&& aOther) = default;
  bool operator==(const txXPathNode& aNode) const;
  bool operator!=(const txXPathNode& aNode) const { return !(*this == aNode); }
  ~txXPathNode() { MOZ_COUNT_DTOR(txXPathNode); }

 private:
  friend class txXPathNativeNode;
  friend class txXPathNodeUtils;
  friend class txXPathTreeWalker;

  txXPathNode(nsINode* aNode, uint32_t aIndex) : mNode(aNode), mIndex(aIndex) {
    MOZ_COUNT_CTOR(txXPathNode);
  }

  static nsINode* RootOf(nsINode* aNode) { return aNode->SubtreeRoot(); }
  nsINode* Root() const { return RootOf(mNode); }

  bool isDocument() const { return mIndex == eDocument; }
  bool isContent() const { return mIndex == eContent; }
  bool isAttribute() const { return mIndex != eDocument && mIndex != eContent; }

  nsIContent* Content() const {
    NS_ASSERTION(isContent() || isAttribute(), "wrong type");
    return static_cast<nsIContent*>(mNode.get());
  }
  mozilla::dom::Document* Document() const {
    NS_ASSERTION(isDocument(), "wrong type");
    return static_cast<mozilla::dom::Document*>(mNode.get());
  }

  enum PositionType : uint32_t {
    eDocument = UINT32_MAX,
    eContent = eDocument - 1
  };

  nsCOMPtr<nsINode> mNode;
  uint32_t mIndex;
};

class txNamespaceManager {
 public:
  static int32_t getNamespaceID(const nsAString& aNamespaceURI);
  static nsresult getNamespaceURI(const int32_t aID, nsAString& aResult);
};

/* static */
inline int32_t txNamespaceManager::getNamespaceID(
    const nsAString& aNamespaceURI) {
  int32_t namespaceID = kNameSpaceID_Unknown;
  nsNameSpaceManager::GetInstance()->RegisterNameSpace(aNamespaceURI,
                                                       namespaceID);
  return namespaceID;
}

/* static */
inline nsresult txNamespaceManager::getNamespaceURI(const int32_t aID,
                                                    nsAString& aResult) {
  return nsNameSpaceManager::GetInstance()->GetNameSpaceURI(aID, aResult);
}

inline bool txXPathNode::operator==(const txXPathNode& aNode) const {
  return mIndex == aNode.mIndex && mNode == aNode.mNode;
}

#endif /* txXPathNode_h__ */
