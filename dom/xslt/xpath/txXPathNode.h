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
#include "nsContentUtils.h"  // For NameSpaceManager().

typedef nsINode txXPathNodeType;

class txXPathNode {
 public:
  bool operator==(const txXPathNode& aNode) const;
  bool operator!=(const txXPathNode& aNode) const { return !(*this == aNode); }
  ~txXPathNode();

 private:
  friend class txNodeSet;
  friend class txXPathNativeNode;
  friend class txXPathNodeUtils;
  friend class txXPathTreeWalker;

  txXPathNode(const txXPathNode& aNode);

  explicit txXPathNode(mozilla::dom::Document* aDocument)
      : mNode(aDocument), mRefCountRoot(0), mIndex(eDocument) {
    MOZ_COUNT_CTOR(txXPathNode);
  }
  txXPathNode(nsINode* aNode, uint32_t aIndex, nsINode* aRoot)
      : mNode(aNode), mRefCountRoot(aRoot ? 1 : 0), mIndex(aIndex) {
    MOZ_COUNT_CTOR(txXPathNode);
    if (aRoot) {
      NS_ADDREF(aRoot);
    }
  }

  static nsINode* RootOf(nsINode* aNode) { return aNode->SubtreeRoot(); }
  nsINode* Root() const { return RootOf(mNode); }
  nsINode* GetRootToAddRef() const { return mRefCountRoot ? Root() : nullptr; }

  bool isDocument() const { return mIndex == eDocument; }
  bool isContent() const { return mIndex == eContent; }
  bool isAttribute() const { return mIndex != eDocument && mIndex != eContent; }

  nsIContent* Content() const {
    NS_ASSERTION(isContent() || isAttribute(), "wrong type");
    return static_cast<nsIContent*>(mNode);
  }
  mozilla::dom::Document* Document() const {
    NS_ASSERTION(isDocument(), "wrong type");
    return static_cast<mozilla::dom::Document*>(mNode);
  }

  enum PositionType { eDocument = (1 << 30), eContent = eDocument - 1 };

  nsINode* mNode;
  uint32_t mRefCountRoot : 1;
  uint32_t mIndex : 31;
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
  nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                        namespaceID);
  return namespaceID;
}

/* static */
inline nsresult txNamespaceManager::getNamespaceURI(const int32_t aID,
                                                    nsAString& aResult) {
  return nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aID, aResult);
}

inline bool txXPathNode::operator==(const txXPathNode& aNode) const {
  return mIndex == aNode.mIndex && mNode == aNode.mNode;
}

#endif /* txXPathNode_h__ */
