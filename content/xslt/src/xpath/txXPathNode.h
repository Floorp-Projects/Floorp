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

#ifndef txXPathNode_h__
#define txXPathNode_h__

#ifdef TX_EXE
#include "txDOM.h"
#else
#include "nsAutoPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"
#include "nsContentUtils.h"
#endif

#ifdef TX_EXE
typedef Node txXPathNodeType;
#else
typedef nsIDOMNode txXPathNodeType;
#endif

class txXPathNode
{
public:
    PRBool operator==(const txXPathNode& aNode) const;
    PRBool operator!=(const txXPathNode& aNode) const
    {
        return !(*this == aNode);
    }
    ~txXPathNode();

private:
    friend class txNodeSet;
    friend class txXPathNativeNode;
    friend class txXPathNodeUtils;
    friend class txXPathTreeWalker;

    txXPathNode(const txXPathNode& aNode);

#ifdef TX_EXE
    txXPathNode(NodeDefinition* aNode) : mInner(aNode)
    {
    }

    NodeDefinition* mInner;
#else
    txXPathNode(nsIDocument* aDocument) : mNode(aDocument),
                                          mRefCountRoot(0),
                                          mIndex(eDocument)
    {
        MOZ_COUNT_CTOR(txXPathNode);
    }
    txXPathNode(nsINode *aNode, PRUint32 aIndex, nsINode *aRoot)
        : mNode(aNode),
          mRefCountRoot(aRoot ? 1 : 0),
          mIndex(aIndex)
    {
        MOZ_COUNT_CTOR(txXPathNode);
        if (aRoot) {
            NS_ADDREF(aRoot);
        }
    }

    static nsINode *RootOf(nsINode *aNode)
    {
        nsINode *ancestor, *root = aNode;
        while ((ancestor = root->GetNodeParent())) {
            root = ancestor;
        }
        return root;
    }
    nsINode *Root() const
    {
        return RootOf(mNode);
    }
    nsINode *GetRootToAddRef() const
    {
        return mRefCountRoot ? Root() : nsnull;
    }

    PRBool isDocument() const
    {
        return mIndex == eDocument;
    }
    PRBool isContent() const
    {
        return mIndex == eContent;
    }
    PRBool isAttribute() const
    {
        return mIndex != eDocument && mIndex != eContent;
    }

    nsIContent* Content() const
    {
        NS_ASSERTION(isContent() || isAttribute(), "wrong type");
        return NS_STATIC_CAST(nsIContent*, mNode);
    }
    nsIDocument* Document() const
    {
        NS_ASSERTION(isDocument(), "wrong type");
        return NS_STATIC_CAST(nsIDocument*, mNode);
    }

    enum PositionType
    {
        eDocument = (1 << 30),
        eContent = eDocument - 1
    };

    nsINode* mNode;
    PRUint32 mRefCountRoot : 1;
    PRUint32 mIndex : 31;
#endif
};

class txNamespaceManager
{
public:
    static PRInt32 getNamespaceID(const nsAString& aNamespaceURI);
    static nsresult getNamespaceURI(const PRInt32 aID, nsAString& aResult);
};

/* static */
inline PRInt32
txNamespaceManager::getNamespaceID(const nsAString& aNamespaceURI)
{
#ifdef TX_EXE
    return txStandaloneNamespaceManager::getNamespaceID(aNamespaceURI);
#else
    PRInt32 namespaceID = kNameSpaceID_Unknown;
    nsContentUtils::NameSpaceManager()->
        RegisterNameSpace(aNamespaceURI, namespaceID);
    return namespaceID;
#endif
}

/* static */
inline nsresult
txNamespaceManager::getNamespaceURI(const PRInt32 aID, nsAString& aResult)
{
#ifdef TX_EXE
    return txStandaloneNamespaceManager::getNamespaceURI(aID, aResult);
#else
    return nsContentUtils::NameSpaceManager()->
        GetNameSpaceURI(aID, aResult);
#endif
}

inline PRBool
txXPathNode::operator==(const txXPathNode& aNode) const
{
#ifdef TX_EXE
    return (mInner == aNode.mInner);
#else
    return mIndex == aNode.mIndex && mNode == aNode.mNode;
#endif
}

#endif /* txXPathNode_h__ */
