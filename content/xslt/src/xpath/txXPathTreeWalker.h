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

class nsIAtom;

#ifndef TX_EXE
#include "nsVoidArray.h"

class txUint32Array : public nsVoidArray
{
public:
    PRBool AppendValue(PRUint32 aValue)
    {
        return InsertElementAt(NS_INT32_TO_PTR(aValue), Count());
    }
    PRBool RemoveValueAt(PRUint32 aIndex)
    {
        return RemoveElementsAt(aIndex, 1);
    }
    PRInt32 ValueAt(PRUint32 aIndex) const
    {
        return NS_PTR_TO_INT32(ElementAt(aIndex));
    }
};

class nsIDOMDocument;
#endif

class txXPathTreeWalker
{
public:
    txXPathTreeWalker(const txXPathTreeWalker& aOther);
    explicit txXPathTreeWalker(const txXPathNode& aNode);
    ~txXPathTreeWalker();

    PRBool getAttr(nsIAtom* aLocalName, PRInt32 aNSID, nsAString& aValue) const;
    PRInt32 getNamespaceID() const;
    PRUint16 getNodeType() const;
    void appendNodeValue(nsAString& aResult) const;
    void getNodeName(nsAString& aName) const;

    void moveTo(const txXPathTreeWalker& aWalker);

    PRBool moveToParent();
    PRBool moveToElementById(const nsAString& aID);
    PRBool moveToFirstAttribute();
    PRBool moveToNextAttribute();
    PRBool moveToFirstChild();
    PRBool moveToLastChild();
    PRBool moveToNextSibling();
    PRBool moveToPreviousSibling();

    PRBool isOnNode(const txXPathNode& aNode) const;

    const txXPathNode& getCurrentPosition() const;

private:
    txXPathNode mPosition;

#ifndef TX_EXE
    PRBool moveToValidAttribute(PRUint32 aStartIndex);
    PRBool moveToSibling(PRInt32 aDir);

    PRUint32 mCurrentIndex;
    txUint32Array mDescendants;
#endif
};

class txXPathNodeUtils
{
public:
    static PRBool getAttr(const txXPathNode& aNode, nsIAtom* aLocalName,
                          PRInt32 aNSID, nsAString& aValue);
    static already_AddRefed<nsIAtom> getLocalName(const txXPathNode& aNode);
    static void getLocalName(const txXPathNode& aNode,
                             nsAString& aLocalName);
    static void getNodeName(const txXPathNode& aNode,
                            nsAString& aName);
    static PRInt32 getNamespaceID(const txXPathNode& aNode);
    static void getNamespaceURI(const txXPathNode& aNode, nsAString& aURI);
    static PRUint16 getNodeType(const txXPathNode& aNode);
    static void appendNodeValue(const txXPathNode& aNode, nsAString& aResult);
    static PRBool isWhitespace(const txXPathNode& aNode);
    static txXPathNode* getDocument(const txXPathNode& aNode);
    static txXPathNode* getOwnerDocument(const txXPathNode& aNode);
    static PRInt32 getUniqueIdentifier(const txXPathNode& aNode);
    static nsresult getXSLTId(const txXPathNode& aNode, nsAString& aResult);
    static void release(txXPathNode* aNode);
    static void getBaseURI(const txXPathNode& aNode, nsAString& aURI);
    static PRIntn comparePosition(const txXPathNode& aNode,
                                  const txXPathNode& aOtherNode);

#ifdef TX_EXE
private:
    static void appendNodeValueHelper(NodeDefinition* aNode, nsAString& aResult);
#endif
};

#ifdef TX_EXE
class txXPathNativeNode
{
public:
    static txXPathNode* createXPathNode(Node* aNode);
    static nsresult getElement(const txXPathNode& aNode, Element** aResult);
    static nsresult getDocument(const txXPathNode& aNode, Document** aResult);
};
#else
class txXPathNativeNode
{
public:
    static txXPathNode* createXPathNode(nsIDOMNode* aNode);
    static txXPathNode* createXPathNode(nsIDOMDocument* aDocument);
    static nsresult getNode(const txXPathNode& aNode, nsIDOMNode** aResult);
    static nsIContent* getContent(const txXPathNode& aNode);
    static nsIDocument* getDocument(const txXPathNode& aNode);
};

#endif

inline const txXPathNode&
txXPathTreeWalker::getCurrentPosition() const
{
    return mPosition;
}

inline PRBool
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

inline PRUint16
txXPathTreeWalker::getNodeType() const
{
    return txXPathNodeUtils::getNodeType(mPosition);
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
#ifdef TX_EXE
    mPosition.mInner = aWalker.mPosition.mInner;
#else
    mPosition.mIndex = aWalker.mPosition.mIndex;
    // Hopefully it's ok to access mContent through mDocument.
    mPosition.mDocument = aWalker.mPosition.mDocument;
    mCurrentIndex = aWalker.mCurrentIndex;
    mDescendants.Clear();
#endif
}

inline PRBool
txXPathTreeWalker::isOnNode(const txXPathNode& aNode) const
{
    return (mPosition == aNode);
}

/* static */
inline PRInt32
txXPathNodeUtils::getUniqueIdentifier(const txXPathNode& aNode)
{
#ifdef TX_EXE
    return NS_PTR_TO_INT32(aNode.mInner);
#else
    NS_PRECONDITION(aNode.mIndex == txXPathNode::eDocument,
                    "Only implemented for documents.");
    return NS_PTR_TO_INT32(aNode.mDocument);
#endif
}

/* static */
inline void
txXPathNodeUtils::release(txXPathNode* aNode)
{
#ifdef TX_EXE
    delete aNode->mInner;
#else
    NS_RELEASE(aNode->mDocument);
#endif
}

#endif /* txXPathTreeWalker_h__ */
