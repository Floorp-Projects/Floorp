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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#ifndef txXPathTreeWalker_h__
#define txXPathTreeWalker_h__

#include "txCore.h"
#include "txXPathNode.h"
#include "nsINodeInfo.h"
#include "nsTArray.h"

class nsIAtom;
class nsIDOMDocument;

class txUint32Array : public nsTArray<PRUint32>
{
public:
    bool AppendValue(PRUint32 aValue)
    {
        return AppendElement(aValue) != nsnull;
    }
    bool RemoveValueAt(PRUint32 aIndex)
    {
        if (aIndex < Length()) {
            RemoveElementAt(aIndex);
        }
        return PR_TRUE;
    }
    PRUint32 ValueAt(PRUint32 aIndex) const
    {
        return (aIndex < Length()) ? ElementAt(aIndex) : 0;
    }
};

class txXPathTreeWalker
{
public:
    txXPathTreeWalker(const txXPathTreeWalker& aOther);
    explicit txXPathTreeWalker(const txXPathNode& aNode);

    bool getAttr(nsIAtom* aLocalName, PRInt32 aNSID, nsAString& aValue) const;
    PRInt32 getNamespaceID() const;
    PRUint16 getNodeType() const;
    void appendNodeValue(nsAString& aResult) const;
    void getNodeName(nsAString& aName) const;

    void moveTo(const txXPathTreeWalker& aWalker);

    void moveToRoot();
    bool moveToParent();
    bool moveToElementById(const nsAString& aID);
    bool moveToFirstAttribute();
    bool moveToNextAttribute();
    bool moveToNamedAttribute(nsIAtom* aLocalName, PRInt32 aNSID);
    bool moveToFirstChild();
    bool moveToLastChild();
    bool moveToNextSibling();
    bool moveToPreviousSibling();

    bool isOnNode(const txXPathNode& aNode) const;

    const txXPathNode& getCurrentPosition() const;

private:
    txXPathNode mPosition;

    bool moveToValidAttribute(PRUint32 aStartIndex);
    bool moveToSibling(PRInt32 aDir);

    PRUint32 mCurrentIndex;
    txUint32Array mDescendants;
};

class txXPathNodeUtils
{
public:
    static bool getAttr(const txXPathNode& aNode, nsIAtom* aLocalName,
                          PRInt32 aNSID, nsAString& aValue);
    static already_AddRefed<nsIAtom> getLocalName(const txXPathNode& aNode);
    static nsIAtom* getPrefix(const txXPathNode& aNode);
    static void getLocalName(const txXPathNode& aNode, nsAString& aLocalName);
    static void getNodeName(const txXPathNode& aNode,
                            nsAString& aName);
    static PRInt32 getNamespaceID(const txXPathNode& aNode);
    static void getNamespaceURI(const txXPathNode& aNode, nsAString& aURI);
    static PRUint16 getNodeType(const txXPathNode& aNode);
    static void appendNodeValue(const txXPathNode& aNode, nsAString& aResult);
    static bool isWhitespace(const txXPathNode& aNode);
    static txXPathNode* getDocument(const txXPathNode& aNode);
    static txXPathNode* getOwnerDocument(const txXPathNode& aNode);
    static PRInt32 getUniqueIdentifier(const txXPathNode& aNode);
    static nsresult getXSLTId(const txXPathNode& aNode,
                              const txXPathNode& aBase, nsAString& aResult);
    static void release(txXPathNode* aNode);
    static void getBaseURI(const txXPathNode& aNode, nsAString& aURI);
    static PRIntn comparePosition(const txXPathNode& aNode,
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
        return PR_FALSE;
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
txXPathTreeWalker::getAttr(nsIAtom* aLocalName, PRInt32 aNSID,
                           nsAString& aValue) const
{
    return txXPathNodeUtils::getAttr(mPosition, aLocalName, aNSID, aValue);
}

inline PRInt32
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
    nsINode *root = nsnull;
    if (mPosition.mRefCountRoot) {
        root = mPosition.Root();
    }
    mPosition.mIndex = aWalker.mPosition.mIndex;
    mPosition.mRefCountRoot = aWalker.mPosition.mRefCountRoot;
    mPosition.mNode = aWalker.mPosition.mNode;
    nsINode *newRoot = nsnull;
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
inline PRInt32
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
    return !aNode.isAttribute() && !aNode.mNode->GetNodeParent();
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
