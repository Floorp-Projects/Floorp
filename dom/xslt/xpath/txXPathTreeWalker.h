/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txXPathTreeWalker_h__
#define txXPathTreeWalker_h__

#include "txCore.h"
#include "txXPathNode.h"
#include "nsIContentInlines.h"
#include "nsINodeInfo.h"
#include "nsTArray.h"

class nsIAtom;
class nsIDOMDocument;

class txUint32Array : public nsTArray<uint32_t>
{
public:
    bool AppendValue(uint32_t aValue)
    {
        return AppendElement(aValue) != nullptr;
    }
    bool RemoveValueAt(uint32_t aIndex)
    {
        if (aIndex < Length()) {
            RemoveElementAt(aIndex);
        }
        return true;
    }
    uint32_t ValueAt(uint32_t aIndex) const
    {
        return (aIndex < Length()) ? ElementAt(aIndex) : 0;
    }
};

class txXPathTreeWalker
{
public:
    txXPathTreeWalker(const txXPathTreeWalker& aOther);
    explicit txXPathTreeWalker(const txXPathNode& aNode);

    bool getAttr(nsIAtom* aLocalName, int32_t aNSID, nsAString& aValue) const;
    int32_t getNamespaceID() const;
    uint16_t getNodeType() const;
    void appendNodeValue(nsAString& aResult) const;
    void getNodeName(nsAString& aName) const;

    void moveTo(const txXPathTreeWalker& aWalker);

    void moveToRoot();
    bool moveToParent();
    bool moveToElementById(const nsAString& aID);
    bool moveToFirstAttribute();
    bool moveToNextAttribute();
    bool moveToNamedAttribute(nsIAtom* aLocalName, int32_t aNSID);
    bool moveToFirstChild();
    bool moveToLastChild();
    bool moveToNextSibling();
    bool moveToPreviousSibling();

    bool isOnNode(const txXPathNode& aNode) const;

    const txXPathNode& getCurrentPosition() const;

private:
    txXPathNode mPosition;

    bool moveToValidAttribute(uint32_t aStartIndex);
    bool moveToSibling(int32_t aDir);

    uint32_t mCurrentIndex;
    txUint32Array mDescendants;
};

class txXPathNodeUtils
{
public:
    static bool getAttr(const txXPathNode& aNode, nsIAtom* aLocalName,
                          int32_t aNSID, nsAString& aValue);
    static already_AddRefed<nsIAtom> getLocalName(const txXPathNode& aNode);
    static nsIAtom* getPrefix(const txXPathNode& aNode);
    static void getLocalName(const txXPathNode& aNode, nsAString& aLocalName);
    static void getNodeName(const txXPathNode& aNode,
                            nsAString& aName);
    static int32_t getNamespaceID(const txXPathNode& aNode);
    static void getNamespaceURI(const txXPathNode& aNode, nsAString& aURI);
    static uint16_t getNodeType(const txXPathNode& aNode);
    static void appendNodeValue(const txXPathNode& aNode, nsAString& aResult);
    static bool isWhitespace(const txXPathNode& aNode);
    static txXPathNode* getOwnerDocument(const txXPathNode& aNode);
    static int32_t getUniqueIdentifier(const txXPathNode& aNode);
    static nsresult getXSLTId(const txXPathNode& aNode,
                              const txXPathNode& aBase, nsAString& aResult);
    static void release(txXPathNode* aNode);
    static void getBaseURI(const txXPathNode& aNode, nsAString& aURI);
    static int comparePosition(const txXPathNode& aNode,
                               const txXPathNode& aOtherNode);
    static bool localNameEquals(const txXPathNode& aNode,
                                  nsIAtom* aLocalName);
    static bool isRoot(const txXPathNode& aNode);
    static bool isElement(const txXPathNode& aNode);
    static bool isAttribute(const txXPathNode& aNode);
    static bool isProcessingInstruction(const txXPathNode& aNode);
    static bool isComment(const txXPathNode& aNode);
    static bool isText(const txXPathNode& aNode);
    static inline bool isHTMLElementInHTMLDocument(const txXPathNode& aNode)
    {
      if (!aNode.isContent()) {
        return false;
      }
      nsIContent* content = aNode.Content();
      return content->IsHTML() && content->IsInHTMLDocument();
    }
};

class txXPathNativeNode
{
public:
    static txXPathNode* createXPathNode(nsIDOMNode* aNode,
                                        bool aKeepRootAlive = false);
    static txXPathNode* createXPathNode(nsIContent* aContent,
                                        bool aKeepRootAlive = false);
    static txXPathNode* createXPathNode(nsIDOMDocument* aDocument);
    static nsresult getNode(const txXPathNode& aNode, nsIDOMNode** aResult);
    static nsIContent* getContent(const txXPathNode& aNode);
    static nsIDocument* getDocument(const txXPathNode& aNode);
    static void addRef(const txXPathNode& aNode)
    {
        NS_ADDREF(aNode.mNode);
    }
    static void release(const txXPathNode& aNode)
    {
        nsINode *node = aNode.mNode;
        NS_RELEASE(node);
    }
};

inline const txXPathNode&
txXPathTreeWalker::getCurrentPosition() const
{
    return mPosition;
}

inline bool
txXPathTreeWalker::getAttr(nsIAtom* aLocalName, int32_t aNSID,
                           nsAString& aValue) const
{
    return txXPathNodeUtils::getAttr(mPosition, aLocalName, aNSID, aValue);
}

inline int32_t
txXPathTreeWalker::getNamespaceID() const
{
    return txXPathNodeUtils::getNamespaceID(mPosition);
}

inline void
txXPathTreeWalker::appendNodeValue(nsAString& aResult) const
{
    txXPathNodeUtils::appendNodeValue(mPosition, aResult);
}

inline void
txXPathTreeWalker::getNodeName(nsAString& aName) const
{
    txXPathNodeUtils::getNodeName(mPosition, aName);
}

inline void
txXPathTreeWalker::moveTo(const txXPathTreeWalker& aWalker)
{
    nsINode *root = nullptr;
    if (mPosition.mRefCountRoot) {
        root = mPosition.Root();
    }
    mPosition.mIndex = aWalker.mPosition.mIndex;
    mPosition.mRefCountRoot = aWalker.mPosition.mRefCountRoot;
    mPosition.mNode = aWalker.mPosition.mNode;
    nsINode *newRoot = nullptr;
    if (mPosition.mRefCountRoot) {
        newRoot = mPosition.Root();
    }
    if (root != newRoot) {
        NS_IF_ADDREF(newRoot);
        NS_IF_RELEASE(root);
    }

    mCurrentIndex = aWalker.mCurrentIndex;
    mDescendants.Clear();
}

inline bool
txXPathTreeWalker::isOnNode(const txXPathNode& aNode) const
{
    return (mPosition == aNode);
}

/* static */
inline int32_t
txXPathNodeUtils::getUniqueIdentifier(const txXPathNode& aNode)
{
    NS_PRECONDITION(!aNode.isAttribute(),
                    "Not implemented for attributes.");
    return NS_PTR_TO_INT32(aNode.mNode);
}

/* static */
inline void
txXPathNodeUtils::release(txXPathNode* aNode)
{
    NS_RELEASE(aNode->mNode);
}

/* static */
inline bool
txXPathNodeUtils::localNameEquals(const txXPathNode& aNode,
                                  nsIAtom* aLocalName)
{
    if (aNode.isContent() &&
        aNode.Content()->IsElement()) {
        return aNode.Content()->NodeInfo()->Equals(aLocalName);
    }

    nsCOMPtr<nsIAtom> localName = txXPathNodeUtils::getLocalName(aNode);

    return localName == aLocalName;
}

/* static */
inline bool
txXPathNodeUtils::isRoot(const txXPathNode& aNode)
{
    return !aNode.isAttribute() && !aNode.mNode->GetParentNode();
}

/* static */
inline bool
txXPathNodeUtils::isElement(const txXPathNode& aNode)
{
    return aNode.isContent() &&
           aNode.Content()->IsElement();
}


/* static */
inline bool
txXPathNodeUtils::isAttribute(const txXPathNode& aNode)
{
    return aNode.isAttribute();
}

/* static */
inline bool
txXPathNodeUtils::isProcessingInstruction(const txXPathNode& aNode)
{
    return aNode.isContent() &&
           aNode.Content()->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION);
}

/* static */
inline bool
txXPathNodeUtils::isComment(const txXPathNode& aNode)
{
    return aNode.isContent() &&
           aNode.Content()->IsNodeOfType(nsINode::eCOMMENT);
}

/* static */
inline bool
txXPathNodeUtils::isText(const txXPathNode& aNode)
{
    return aNode.isContent() &&
           aNode.Content()->IsNodeOfType(nsINode::eTEXT);
}

#endif /* txXPathTreeWalker_h__ */
